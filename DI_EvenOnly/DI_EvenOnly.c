/////////////////////////////////////////////////////////////////////////////
// $Id: DI_EvenOnly.c,v 1.7 2002/06/13 12:10:24 adcockj Exp $
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.  If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 04 Jan 2001   John Adcock           Split into separate module
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log: DI_EvenOnly.c,v $
// Revision 1.7  2002/06/13 12:10:24  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.6  2001/11/22 13:32:03  adcockj
// Finished changes caused by changes to TDeinterlaceInfo - Compiles
//
// Revision 1.5  2001/11/21 15:21:40  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.4  2001/07/13 16:13:33  adcockj
// Added CVS tags and removed tabs
//
/////////////////////////////////////////////////////////////////////////////

#include "windows.h"
#include "DS_Deinterlace.h"
#include "..\help\helpids.h"

#include "process.h"
#include "winsock.h"
#include <stdio.h>

// ********** JPEG CODE STARTS *************************************************

#include "jpeglib.h"
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


void write_JPEG_memory (unsigned char *inputBuf, int inputWidth, int inputHeight, 
                        unsigned char *outputBuf, int quality, int *outputBufSize)
{
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

//  jpeg_stdio_dest(&cinfo, outfile);
  jpeg_memory_dest(&cinfo, outputBuf, outputBufSize);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = inputWidth; 	/* image width and height, in pixels */
  cinfo.image_height = inputHeight;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = inputWidth * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
//    row_pointer[0] = & image_buffer[cinfo.next_scanline * row_stride];
    row_pointer[0] = &inputBuf[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
}


#define NETWORK_ERROR -1
#define NETWORK_OK     0


// Global Variables
static HANDLE threadHandle = 0;
static BOOL bufferUsed = TRUE;
static BYTE *buffer = NULL;
static int bufferWidth = 0;
static int bufferHeight = 0;
static unsigned char tempRGBBuffer[320 * 240 * 3];  // 320x240 rgb buffer.
static unsigned char tempJPegBuffer[65536 * 4];     // 256k jpeg buffer
static int tempJPegBufferSize;     


void ReportError(int errorCode, const char *whichFunc) {
   char errorMsg[92];					// Declare a buffer to hold
							// the generated error message
   
   ZeroMemory(errorMsg, 92);				// Automatically NULL-terminate the string

   // The following line copies the phrase, whichFunc string, and integer errorCode into the buffer
   sprintf(errorMsg, "Call to %s returned error %d!", (char *)whichFunc, errorCode);

   MessageBox(NULL, errorMsg, "socketIndication", MB_OK);
}

UINT socketThread(LPVOID lpvoid)
{
	WORD sockVersion;
	WSADATA wsaData;
	int nret;
  char tempBuf[256];
  int iMode = 1;
  int bufferSize;
  int index;
  int flag = 1, result;
  int x, y; // indexes
  int Y1, Y2, Cb, Cr; // YUV
  int r, g, b;        // RGB

	// Use a SOCKADDR_IN struct to fill in address information
	SOCKADDR_IN serverInfo;

	// Next, create the listening socket
	SOCKET listeningSocket;

	// Wait for a client
	SOCKET theClient;


  sockVersion = MAKEWORD(1, 1);			// We'd like Winsock version 1.1

	// We begin by initializing Winsock
	WSAStartup(sockVersion, &wsaData);

	listeningSocket = socket(AF_INET,		// Go over TCP/IP
			         SOCK_STREAM,   	// This is a stream-oriented socket
				 IPPROTO_TCP);		// Use TCP rather than UDP

	if (listeningSocket == INVALID_SOCKET) {
		nret = WSAGetLastError();		// Get a more detailed error
		ReportError(nret, "socket()");		// Report the error with our custom function

		WSACleanup();				// Shutdown Winsock
		return NETWORK_ERROR;			// Return an error value
	}

	serverInfo.sin_family = AF_INET;
	serverInfo.sin_addr.s_addr = INADDR_ANY;	// Since this socket is listening for connections,
							// any local address will do
	serverInfo.sin_port = htons(8888);		// Convert integer 8888 to network-byte order
							// and insert into the port field


	// Bind the socket to our local server address
	nret = bind(listeningSocket, (LPSOCKADDR)&serverInfo, sizeof(struct sockaddr));

	if (nret == SOCKET_ERROR) {
		nret = WSAGetLastError();
		ReportError(nret, "bind()");

		WSACleanup();
		return NETWORK_ERROR;
	}


	// Make the socket listen
	nret = listen(listeningSocket, 1);		// Up to 10 connections may wait at any
							// one time to be accept()'ed

	if (nret == SOCKET_ERROR) {
		nret = WSAGetLastError();
		ReportError(nret, "listen()");

		WSACleanup();
		return NETWORK_ERROR;
	}


	theClient = accept(listeningSocket,
			   NULL,			// Address of a sockaddr structure (see explanation below)
			   NULL);			// Address of a variable containing size of sockaddr struct

	if (theClient == INVALID_SOCKET) {
  	nret = WSAGetLastError();
	 	ReportError(nret, "accept()");

	  WSACleanup();
	  return 0;
  }

  flag = 1;
  result = setsockopt(theClient,            /* socket affected */
                          IPPROTO_TCP,     /* set option at TCP level */
                          TCP_NODELAY,     /* name of option */
                         (char *) &flag,  /* the cast is historical cruft */
                         sizeof(int));    /* length of option value */

  while(1) {

//    sprintf(tempStr, "Width: %d   Height: %d\r\n", bufferWidth, bufferHeight);
//    nret = send(theClient, tempStr, strlen(tempStr), 0);
    
    // Send the image buffer only if it is filled (available).
    if(bufferUsed == FALSE) {

      // Convert image from YUV (8:4:8:4) to RGB
      unsigned char *yuvPtr = buffer;
      unsigned char *rgbPtr = tempRGBBuffer;
      for(y = 0; y < 240; y ++) {
        for(x = 0; x < 320; x += 2) {

          // Read YUV Data. Read two pixels at a time. 
          Y1 = *yuvPtr; yuvPtr ++;
          Cb = *yuvPtr; yuvPtr ++;
          Y2 = *yuvPtr; yuvPtr ++;
          Cr = *yuvPtr; yuvPtr ++;

          // First Pixel
          r = (int)(Y1 + 1.402 * (Cr - 128) );
          g = (int)(Y1 - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) );
          b = (int)(Y1 + 1.772 * (Cb - 128) );
          if(r < 0) r = 0; if(r > 255) r = 255;
          if(g < 0) g = 0; if(g > 255) g = 255;
          if(b < 0) b = 0; if(b > 255) b = 255;
          *rgbPtr = r; rgbPtr ++;
          *rgbPtr = g; rgbPtr ++;
          *rgbPtr = b; rgbPtr ++;

          // Second Pixel
          r = (int)(Y2 + 1.402 * (Cr - 128) );
          g = (int)(Y2 - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128) );
          b = (int)(Y2 + 1.772 * (Cb - 128) );
          if(r < 0) r = 0; if(r > 255) r = 255;
          if(g < 0) g = 0; if(g > 255) g = 255;
          if(b < 0) b = 0; if(b > 255) b = 255;
          *rgbPtr = r; rgbPtr ++;
          *rgbPtr = g; rgbPtr ++;
          *rgbPtr = b; rgbPtr ++;
        }
      }

      // Compress RGB to JPeg
      write_JPEG_memory(tempRGBBuffer, 320, 240, tempJPegBuffer, 30, &tempJPegBufferSize);
      bufferSize = tempJPegBufferSize;

      // Send the size of the buffer.
      nret = send(theClient, (char *)&bufferWidth, 4, 0);
      nret = send(theClient, (char *)&bufferHeight, 4, 0);
      nret = send(theClient, (char *)&bufferSize, 4, 0);

      // Send 64k at a time.
      for(index = 0; index < bufferSize; index += 65536) {
        if(bufferSize - index < 65536)
          nret = send(theClient, (char *)(tempJPegBuffer + index), bufferSize - index, 0);
        else
          nret = send(theClient, (char *)(tempJPegBuffer + index), 65536, 0);
      }

      bufferUsed = TRUE;

      if(nret == SOCKET_ERROR)
        break;

      if(nret == 0 || WSAGetLastError() == WSAECONNRESET)
        break;

      // Wait for confirmation.
      Sleep(50);
      recv(theClient, tempBuf, 256, 0);

      if(tempBuf[0] == 'Q')
        break;
      else if(tempBuf[0] == 'U') {
			  keybd_event(VK_PRIOR, MapVirtualKey(VK_PRIOR, NULL), NULL, NULL);
				Sleep(5);
			  keybd_event(VK_PRIOR, MapVirtualKey(VK_PRIOR, NULL), KEYEVENTF_KEYUP, NULL);
				Sleep(5);
      }
      else if(tempBuf[0] == 'D') {
			  keybd_event(VK_NEXT, MapVirtualKey(VK_NEXT, NULL), NULL, NULL);
				Sleep(5);
			  keybd_event(VK_NEXT, MapVirtualKey(VK_NEXT, NULL), KEYEVENTF_KEYUP, NULL);
				Sleep(5);
      }

    }

//    nret = recv(theClient, tempBuf, 255, 0);
//    if(nret > 0)
//      nret = recv(theClient, tempBuf, 255, 0);

//    send(theClient, "Hi, Test Test\n", 15, 0);    

//    Sleep(2000);
  }


	// Send and receive from the client, and finally,
	closesocket(theClient);
	closesocket(listeningSocket);


	// Shutdown Winsock
	WSACleanup();
  
  // Kill this thread.
  CloseHandle(threadHandle);
  threadHandle = 0;

  return 0;
}



BOOL DeinterlaceEvenOnly(TDeinterlaceInfo* pInfo)
{
    int nLineTarget;
    BYTE* CurrentLine = pInfo->PictureHistory[0]->pData;    

    // Create a new thread to listen to connections if it doesn't exist already.
    if(threadHandle == 0) {
			 threadHandle = CreateThread(NULL,  // no security attributes 
							          0,                // use default stack size 
							          (LPTHREAD_START_ROUTINE) socketThread, 
							          (LPVOID)0, // param to thread func 
							          CREATE_SUSPENDED, // creation flag 
						            NULL);       // 
			 ResumeThread(threadHandle); 

//       SetThreadPriority(threadHandle, THREAD_PRIORITY_LOWEST);
       SetThreadPriority(threadHandle, THREAD_PRIORITY_NORMAL);
//			 SetThreadPriority(threadHandle, THREAD_PRIORITY_HIGHEST);
//			 SetThreadPriority(threadHandle, THREAD_PRIORITY_TIME_CRITICAL);
    }

    if (pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_EVEN)
    {
/*
        for (nLineTarget = 0; nLineTarget < pInfo->FieldHeight; nLineTarget++)
        {
            // copy latest field's rows to overlay, resulting in a half-height image.
            pInfo->pMemcpy(pInfo->Overlay + nLineTarget * pInfo->OverlayPitch,
                        CurrentLine,
                        pInfo->LineLength);
            CurrentLine += pInfo->InputPitch;
        }
*/

        // Allocate memory for the buffer if it doesn't exist
        if(buffer == NULL) {
          bufferWidth = pInfo->LineLength / 2;
          bufferHeight = pInfo->FieldHeight;
          buffer = (BYTE *)malloc(bufferWidth * bufferHeight * 2);
        }

        if(bufferWidth != pInfo->LineLength / 2 || bufferHeight != pInfo->FieldHeight) {
          Beep(100, 100);
          return FALSE;
        }

        if(bufferUsed == FALSE) {
          for (nLineTarget = 0; nLineTarget < pInfo->FieldHeight; nLineTarget++)
          {
              // copy latest field's rows to overlay, resulting in a half-height image.
              pInfo->pMemcpy(pInfo->Overlay + nLineTarget * pInfo->OverlayPitch,
                          CurrentLine,
                          pInfo->LineLength);  
              CurrentLine += pInfo->InputPitch;
          }
        }
        else {
          for (nLineTarget = 0; nLineTarget < pInfo->FieldHeight; nLineTarget++)
          {
              // copy latest field's rows to overlay, resulting in a half-height image.
              pInfo->pMemcpy(pInfo->Overlay + nLineTarget * pInfo->OverlayPitch,
                          CurrentLine,
                          pInfo->LineLength);  

              // intercept the YUV (4:2:2 16bits) and store it into a buffer.
              memcpy(buffer + nLineTarget * bufferWidth * 2, CurrentLine, pInfo->LineLength);

              CurrentLine += pInfo->InputPitch;
          }

          bufferUsed = FALSE;
        }

        // need to clear up MMX registers
        _asm
        {
            emms
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


DEINTERLACE_METHOD EvenOnlyMethod =
{
    sizeof(DEINTERLACE_METHOD),
    DEINTERLACE_CURRENT_VERSION,
    "Even Scanlines Only", 
    "Even",
    TRUE, 
    FALSE, 
    DeinterlaceEvenOnly, 
    25, 
    30,
    0,
    NULL,
    INDEX_EVEN_ONLY,
    NULL,
    NULL,
    NULL,
    NULL,
    1,
    0,
    0,
    -1,
    NULL,
    0,
    FALSE,
    FALSE,
    IDH_EVEN,
};


__declspec(dllexport) DEINTERLACE_METHOD* GetDeinterlacePluginInfo(long CpuFeatureFlags)
{
    return &EvenOnlyMethod;
}

BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}


