#include <stdafx.h>
#include <stdio.h>
#include <stdlib.h>

#include <winsock.h>

#include <SDL.h>

SDL_Surface *screen;
int xpos=0,ypos=0;
SOCKET sock;
unsigned char buffer[320 * 240 * 4];
int bufferWidth;
int bufferHeight;
int bufferSize;
unsigned char tempJPegBuffer[65536 * 4];  // 256k buffer


// ********** JPEG CODE STARTS *************************************************

#include "jpeglib.h"
#include "jmemsrc.h"
#include "jmemdst.h"
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
	// initialize the socket library
	WSADATA info;
  struct sockaddr_in sa;
  struct hostent     *hp;

	if(WSAStartup(MAKEWORD(1, 1), &info) == SOCKET_ERROR)
    return;

  hp = gethostbyname(hostname);
  if (hp == NULL) /* we don't know who this host is */
    return;

  memset(&sa,0,sizeof(sa));
  memcpy((char *)&sa.sin_addr, hp->h_addr, hp->h_length);   /* set address */
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons((u_short)portnum);

  sock = socket(hp->h_addrtype, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET)
    return;

  /* try to connect to the specified socket */
  if (connect(sock, (struct sockaddr *)&sa, sizeof sa) == SOCKET_ERROR) {
    closesocket(sock);
    return;
  }

  int flag = 1;
  int result = setsockopt(sock,            /* socket affected */
                          IPPROTO_TCP,     /* set option at TCP level */
                          TCP_NODELAY,     /* name of option */
                          (char *) &flag,  /* the cast is historical cruft */
                          sizeof(int));    /* length of option value */

}

// Guaranteed to receive a certain amount of bytes before returning.
void receiveAll(unsigned char *buf, int length)
{
  char ch[4096];
  int bytesReceived;

  while(length > 0) {

    // Read 4k at a time
    if(length >= 4096) {
      bytesReceived = recv(sock, ch, 4096, 0);
      if(bytesReceived == 0)
        exit(2);
      if(bytesReceived == SOCKET_ERROR)
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

/*
void receiveImage()
{
  unsigned char buf[12];
  int x, y;
  int Y1, Y2, Cb, Cr;

  // Get Image Properties
  receiveAll(buf, 12); 
  bufferWidth = *((int *)buf);
  bufferHeight = *(((int *)buf) + 1);
  bufferSize = *(((int *)buf) + 2);

  // Sanity Check
  if(bufferWidth != 640) {
    fprintf(stderr,"Error, DScaler width is not set to 320. %d\n", bufferWidth / 2);
    Sleep(1000);
    exit(1);
  }

  // Get Image Buffer data
  if(buffer == NULL) { // Allocate New Buffer
    buffer = new unsigned char[bufferSize];
  }
  receiveAll(buffer, bufferSize / 4);

  // Send confirmation.
  send(sock, "Y", 1, 0);

  // Update the screen.
  int r, g, b;
  unsigned short *ptr = (unsigned short *)buffer;
  for(y = 0; y < bufferHeight / 4; y ++) {
    for(x = 0; x < bufferWidth / 2 / 2; x ++) {
//      r = *ptr; ptr ++;
//      g = *ptr; ptr ++;
//      b = *ptr; ptr ++;
//      a = *ptr; ptr ++;
//      r = g = b = a = rand() % 255;
//      r = (*ptr) & 0x1F;
//      g = ((*ptr) >> 5) & 0x1F;
//      b = ((*ptr) >> 10) & 0x1F;
//      r <<= 3; g <<= 3; b <<= 3;

// Grayscale      
//      r = g = b = *((unsigned char *)ptr);
      // Read the next two pixels. Since YUV's format is (Y1,Cr,Y2,Cb), we need to read
      //   two pixels at a time to get the correct chrominance info for both pixels.
      Y1 = *((unsigned char *)ptr);
      Cb = *(((unsigned char *)ptr) + 1);
      Y2 = *(((unsigned char *)ptr) + 2);
      Cr = *(((unsigned char *)ptr) + 3);

      // First Pixel
      r = (int)(Y1 + 1.402 * (Cr - 128) );
      g = (int)(Y1 - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) );
      b = (int)(Y1 + 1.772 * (Cb - 128) );
      if(r < 0) r = 0; if(r > 255) r = 255;
      if(g < 0) g = 0; if(g > 255) g = 255;
      if(b < 0) b = 0; if(b > 255) b = 255;
      setpixel(screen, x * 2, y, r, g, b);

      // Second Pixel
      r = (int)(Y2 + 1.402 * (Cr - 128) );
      g = (int)(Y2 - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) );
      b = (int)(Y2 + 1.772 * (Cb - 128) );
      if(r < 0) r = 0; if(r > 255) r = 255;
      if(g < 0) g = 0; if(g > 255) g = 255;
      if(b < 0) b = 0; if(b > 255) b = 255;
      setpixel(screen, x * 2 + 1, y, r, g, b);

      // Advance two pixels
      ptr += 2;
    }
  }

}
*/

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
    Sleep(5000);
    exit(1);
  }

//  fprintf(stderr, "Width: %d, Height:%d, BufferSize:%d\n", bufferWidth, bufferHeight, bufferSize);
//  Sleep(50);

  // Get Image Buffer data
  receiveAll(tempJPegBuffer, bufferSize);

  // Send confirmation.
//  command = 'Y';
//  send(sock, &command, 1, 0);

  tempBuf[0] = 'Y';
  if(GetAsyncKeyState(VK_UP) & 32768)
    tempBuf[0] = 'U';
  if(GetAsyncKeyState(VK_DOWN) & 32768)
    tempBuf[0] = 'D';
  if(GetAsyncKeyState(VK_ESCAPE) & 32768)
    tempBuf[0] = 'Q';

  send(sock, tempBuf, 256, 0);

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
      *sn = b; sn ++; // B
      *sn = g; sn ++; // G
      *sn = r; sn ++; // R
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

int main(int argc, char *argv[])
{
  Uint8* keys;
  FILE *fp;
  char str[256];

  fp = fopen("TVSP.ini", "rt");
  if(fp == NULL) {
    printf("Error: Can't read from TVSP.ini\n");
    SDL_Delay(4000);
    exit(1);
  }
  else {
    fgets(str, 255, fp);
    fclose(fp);
  }

  initSocket(str, 8888); // Connect to the DScaler TV Server.

  if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 )
  {
    printf("Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);

  screen=SDL_SetVideoMode(320,240,32,SDL_HWSURFACE|SDL_DOUBLEBUF);
  if ( screen == NULL )
  {
    printf("Unable to set 320x240 video: %s\n", SDL_GetError());
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

  return 0;
}
