// Standard Headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SDL Headers
#include <SDL.h>

// PSP Specific Headers
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psputility.h>
#include <psppower.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pspnet.h>
#include <pspnet_resolver.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <pspctrl.h>


// PSP Specific flags
PSP_MODULE_INFO("TVPSP", 0x1000, 1, 1);
PSP_MAIN_THREAD_ATTR(0);
PSP_MAIN_THREAD_STACK_SIZE_KB(32); /* smaller stack for kernel thread */

static char resolverBuffer[1024];
static int resolverId;

SDL_Surface *screen;
int xpos=0,ypos=0;
int sock;
unsigned char buffer[320 * 240 * 4];
int bufferWidth;
int bufferHeight;
int bufferSize;
unsigned char tempJPegBuffer[65536 * 4];  // 256k buffer



/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
   // Disconnect from net.
   shutdown(sock, 0); shutdown(sock, 1); shutdown(sock, 2);
   sceNetInetShutdown(sock, 0);
   sceNetInetShutdown(sock, 1);
   sceNetInetShutdown(sock, 2);
   sceNetInetClose(0);
   sceNetInetClose(1);
   sceNetInetClose(2);
   sceNetApctlDisconnect();
   pspSdkInetTerm();

   sceKernelExitGame();
   return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
   int cbid;

   cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
   sceKernelRegisterExitCallback(cbid);

   sceKernelSleepThreadCB();

   return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
   int thid = 0;

   thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
   if(thid >= 0)
   {
      sceKernelStartThread(thid, 0, 0);
   }

   return thid;
}


void sceCtrlReadBufferPositiveNoRepeat(SceCtrlData *ctrl, int count)
{
	static unsigned int bufButtons = 0;
	unsigned int buttonBits, bufButtonBits;
	int index;
	count ++;	// Get rid of warning

    sceCtrlReadBufferPositive(ctrl, count);
	buttonBits = ctrl->Buttons;
	bufButtonBits = bufButtons;

	// Cycle through each button
	// Don't let held buttons be repeatedly reported as pressed.
	for(index = 0; index < 32; index ++) {
		if(buttonBits & 1) {
			if( !(bufButtonBits & 1) ) {
				bufButtons = bufButtons | (1 << index);
			}
			else {
				ctrl->Buttons = ctrl->Buttons & (0xFFFFFFFF ^ (1 << index));
			}
		}
		else {
			bufButtons = bufButtons & (0xFFFFFFFF ^ (1 << index));
		}

		buttonBits >>= 1;
		bufButtonBits >>= 1;
	}

}


// ********** JPEG CODE STARTS *************************************************

extern "C" {
#include "jpeglib.h"
}
#include "jmemsrc.h"

#include <setjmp.h>

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */

int read_JPEG_memory (unsigned char *inputBuf, int inputBufSize, 
                      unsigned char *outputBuf, int *outputWidth, int *outputHeight)
{
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo);
    return 0;
  }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */

//  jpeg_stdio_src(&cinfo, infile);
  jpeg_memory_src(&cinfo, inputBuf, inputBufSize);

  /* Step 3: read file parameters with jpeg_read_header() */

  (void) jpeg_read_header(&cinfo, TRUE);
  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  *outputWidth = cinfo.output_width;
  *outputHeight = cinfo.output_height;

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    /* Assume put_scanline_someplace wants a pointer and sample count. */
    //put_scanline_someplace(buffer[0], row_stride);
    memcpy(outputBuf + (cinfo.output_scanline - 1) * row_stride, buffer[0], row_stride);
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return 1;
}

// ********** JPEG CODE ENDS *************************************************



void initSocket(const char *hostname, unsigned short portnum)
{
  /*
	// initialize the socket library
	WSADATA info;
  struct sockaddr_in sa;
  struct hostent     *hp;

	if(WSAStartup(MAKEWORD(1, 1), &info) == SOCKET_ERROR)
    return;

  hp = gethostbyname(hostname);
  if (hp == NULL) // we don't know who this host is
    return;

  memset(&sa,0,sizeof(sa));
  memcpy((char *)&sa.sin_addr, hp->h_addr, hp->h_length);   // set address
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons((u_short)portnum);

  sock = socket(hp->h_addrtype, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET)
    return;

  // try to connect to the specified socket
  if (connect(sock, (struct sockaddr *)&sa, sizeof sa) == SOCKET_ERROR) {
    closesocket(sock);
    return;
  }

  int flag = 1;
  int result = setsockopt(sock,            // socket affected
                          IPPROTO_TCP,     // set option at TCP level
                          TCP_NODELAY,     // name of option
                          (char *) &flag,  // the cast is historical cruft
                          sizeof(int));    // length of option value
*/

   // init wlan
   int err = pspSdkInetInit();
   if (err != 0) {
      pspDebugScreenPrintf("pspSdkInetInit failed: %i", err);
      return;
   }

   // print available connections
   pspDebugScreenPrintf("available connections:\n");
   int i;
   for (i = 1; i < 100; i++) // skip the 0th connection
   {
      if (sceUtilityCheckNetParam(i) != 0) break;  // no more
      char name[64];
      sceUtilityGetNetParam(i, 0, (netData*) name);
      pspDebugScreenPrintf("%i: %s\n", i, name);
   }
   
   // use connection 1
   pspDebugScreenPrintf("using first connection\n");
   err = sceNetApctlConnect(1);
   if (err != 0) {
      pspDebugScreenPrintf("sceNetApctlConnect failed: %i", err);
      sceKernelDelayThread(5*1000000); // 5 sec to read error
      return;
   }

   // WLAN is initialized when PSP IP address is available
   pspDebugScreenPrintf("init WLAN and getting IP address");
   char pspIPAddr[32];
   while (1) {
      if (sceNetApctlGetInfo(8, pspIPAddr) == 0) break;
      pspDebugScreenPrintf(".");
      sceKernelDelayThread(1000 * 1000);  // wait a second
   }
   pspDebugScreenPrintf("\nPSP IP address: %s\n", pspIPAddr);

   // start DNS resolver
   err = sceNetResolverCreate(&resolverId, resolverBuffer, sizeof(resolverBuffer));
   if (err != 0) {
      pspDebugScreenPrintf("sceNetResolverCreate failed: %i", err);
      sceKernelDelayThread(5*1000000); // 5 sec to read error
      return;
   }

   // resolve host
   pspDebugScreenPrintf("resolving host...%s:%d\n", hostname, portnum);
   struct sockaddr_in addrTo;
   addrTo.sin_family = AF_INET;
   addrTo.sin_port = htons(portnum);
   err = sceNetInetInetAton(hostname, &addrTo.sin_addr);
   if (err == 0) {
      err = sceNetResolverStartNtoA(resolverId, hostname, &addrTo.sin_addr, 2, 3);
      if (err != 0) {
         pspDebugScreenPrintf("sceNetResolverStartNtoA failed: %i\n", err);
         sceKernelDelayThread(5*1000000); // 5 sec to read error
         return;
      }
   }


   ///////////// standard socket part ///////////////

   // create socket (in blocking mode)
   pspDebugScreenPrintf("creating socket...\n");
   sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0) {
      pspDebugScreenPrintf("socket failed, errno: %i\n", errno);
      sceKernelDelayThread(5*1000000); // 5 sec to read error
      return;
   }

   // connect (this may block some time)
   pspDebugScreenPrintf("connecting...\n");
   err = connect(sock, (struct sockaddr*) &addrTo, sizeof(addrTo));
   if (err != 0) {
      pspDebugScreenPrintf("connect failed\n");
      sceKernelDelayThread(5*1000000); // 5 sec to read error
      return;
   }

   sceKernelDelayThread(2*1000000); // 5 sec to read error  
}

// Guaranteed to receive a certain amount of bytes before returning.
#define SOCKETBUFSIZE 65536
void receiveAll(unsigned char *buf, int length)
{
  char ch[SOCKETBUFSIZE];
  int bytesReceived;

  while(length > 0) {

    // Read 4k at a time
    if(length >= SOCKETBUFSIZE) {
      bytesReceived = recv(sock, ch, SOCKETBUFSIZE, 0);
      if(bytesReceived == 0)
        exit(2);
      if(bytesReceived <= 0)
        exit(3);
      memcpy(buf, ch, bytesReceived);
      buf += bytesReceived;
      length -= bytesReceived;
    }
    else {
      bytesReceived = recv(sock, ch, length, 0);
      memcpy(buf, ch, bytesReceived);
      buf += bytesReceived;
      length -= bytesReceived;
    }

/*
    bytesReceived = recv(sock, ch, 1, 0);
    if(bytesReceived == 0)
      exit(2);
    if(bytesReceived == SOCKET_ERROR)
      exit(3);
    *buf = ch[0];
    buf += bytesReceived;
    length -= bytesReceived;
*/
    if(bytesReceived <= 0)
      exit(4);
  }

}



void receiveImage()
{
  unsigned char buf[12];
  char tempBuf[256];
  int x, y;

  receiveAll(buf, 12); 
  bufferWidth = *((int *)buf);
  bufferHeight = *(((int *)buf) + 1);
  bufferSize = *(((int *)buf) + 2);

  // Sanity Check
  if(bufferWidth != 320) {
    fprintf(stderr,"Error, DScaler width is not set to 320. %d\n", bufferWidth);
    SDL_Delay(5000);
    exit(1);
  }

//  fprintf(stderr, "Width: %d, Height:%d, BufferSize:%d\n", bufferWidth, bufferHeight, bufferSize);
//  Sleep(50);

  // Get Image Buffer data
  receiveAll(tempJPegBuffer, bufferSize);

  // Get Input
	SceCtrlData bufCtrl;
//  sceCtrlReadBufferPositiveNoRepeat(&bufCtrl, 1);
  sceCtrlReadBufferPositive(&bufCtrl, 1);

  tempBuf[0] = 'Y';
	if(bufCtrl.Buttons & PSP_CTRL_START) {
    tempBuf[0] = 'Q';
    send(sock, tempBuf, 256, 0);
    SDL_Delay(2000);    
    shutdown(sock, 0); shutdown(sock, 1); shutdown(sock, 2);
    sceNetInetShutdown(sock, 0);
    sceNetInetShutdown(sock, 1);
    sceNetInetShutdown(sock, 2);
    sceNetInetClose(0);
    sceNetInetClose(1);
    sceNetInetClose(2);
    sceNetApctlDisconnect();
    pspSdkInetTerm();
    sceKernelExitGame();
	}
  else if(bufCtrl.Buttons & PSP_CTRL_UP) {
    tempBuf[0] = 'U';
  }
  else if(bufCtrl.Buttons & PSP_CTRL_DOWN) {
    tempBuf[0] = 'D';
  }

  // Send confirmation.
  send(sock, tempBuf, 256, 0);
//  send(sock, "Y", 1, 0);


//  FILE *fp = fopen("output.jpg", "wb");
//  fwrite(tempJPegBuffer, 1, bufferSize, fp);
//  fclose(fp);

  // Uncompress the JPeg to RGB
  read_JPEG_memory(tempJPegBuffer, bufferSize, buffer, &bufferWidth, &bufferHeight);

  // Lock SDL Screen
  if(SDL_MUSTLOCK(screen)) {
    if(SDL_LockSurface(screen) < 0) 
      return;
  }

  // Update the screen.
  int r, g, b;
  unsigned char *ptr = (unsigned char *)buffer;
  unsigned char *sn;
  for(y = 0; y < 240; y ++) {
    sn = (unsigned char *)screen->pixels + y * screen->pitch;
    for(x = 0; x < 320; x ++) {

      // Read the RGB triples.
      r = *ptr; ptr ++;
      g = *ptr; ptr ++;
      b = *ptr; ptr ++;

      // Write to screen (32bit color)
      *sn = r; sn ++; // R
      *sn = g; sn ++; // G
      *sn = b; sn ++; // B
      sn ++;          // Alpha (Skip it)
    }
  }


  // Unlock SDL screen
  if(SDL_MUSTLOCK(screen)) {
    SDL_UnlockSurface(screen);
  }

  SDL_Delay(10);

}


void DrawScene()
{
  receiveImage();

  SDL_Flip(screen);
}

int user_main(SceSize argc, void* argv)
{
  Uint8* keys;
  FILE *fp;
  char str[256];

  SetupCallbacks();                   // Setup psp callbacks

  fp = fopen("TVSP.ini", "rt");
  if(fp == NULL) {
    pspDebugScreenPrintf("Error: Can't read from TVSP.ini\n");
    sceKernelDelayThread(2*1000000); // 5 sec to read error  
    sceKernelExitGame();
    exit(1);
  }
  else {
    fgets(str, 255, fp);
    fclose(fp);
  }


  pspDebugScreenPrintf("TVSP 0.1a    by: Lok Tai (Andy) Fung    2006\n");
  pspDebugScreenPrintf("\n");
  pspDebugScreenPrintf(":-) Watch TV in your Bathroom today :-)");
  pspDebugScreenPrintf("\n");
  pspDebugScreenPrintf("\n");
  sceKernelDelayThread(2*1000000); // 5 sec to read error  

  //  initSocket("200.100.50.41", 8888); // Connect to the DScaler TV Server.
  initSocket(str, 8888); // Connect to the DScaler TV Server.

  if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 )
  {
    pspDebugScreenPrintf("Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);

  screen=SDL_SetVideoMode(320,240,32,SDL_HWSURFACE|SDL_DOUBLEBUF);
  if ( screen == NULL )
  {
    pspDebugScreenPrintf("Unable to set 320x240 video: %s\n", SDL_GetError());
    exit(1);
  }

  int done=0;

  while(done == 0)
  {
    SDL_Event event;

    while ( SDL_PollEvent(&event) )
    {
      if ( event.type == SDL_QUIT )  {  done = 1;  }

      if ( event.type == SDL_KEYDOWN )
      {
        if ( event.key.keysym.sym == SDLK_ESCAPE ) { done = 1; }
      }
    }
    keys = SDL_GetKeyState(NULL);
    if ( keys[SDLK_UP] ) { ypos -= 1; }
    if ( keys[SDLK_DOWN] ) { ypos += 1; }
    if ( keys[SDLK_LEFT] ) { xpos -= 1; }
    if ( keys[SDLK_RIGHT] ) { xpos += 1; }

    DrawScene();
  }
   
   // wait until user ends the program
   sceKernelSleepThread();

   return 0;
}

//__attribute__((constructor)) void wlanInit()
//{
   // TODO: wlan initialization should work in a kernel mode constructor
//   pspSdkLoadInetModules();
//}

extern "C" int main(void)
{
//   scePowerSetClockFrequency(333, 333, 166);

   pspDebugScreenInit();
   int err = pspSdkLoadInetModules();

   if (err != 0) {
      pspDebugScreenPrintf("pspSdkLoadInetModules failed with %x\n", err);
      sceKernelDelayThread(5*1000000); // 5 sec to read error
      return 1;
   }

   // create user thread, tweek stack size here if necessary
   SceUID thid = sceKernelCreateThread("User Mode Thread", user_main,
      0x11, // default priority
      256 * 1024, // stack size (256KB is regular default)
      PSP_THREAD_ATTR_USER, NULL);
   
   // start user thread, then wait for it to do everything else
   sceKernelStartThread(thid, 0, NULL);
   sceKernelWaitThreadEnd(thid, NULL);
   
   sceKernelExitGame();

   return 0;

}
