/* ==========================================================================

  Clarasoft Foundation Server

  Example program:
    csap client program for 
    basic csap service program.

  Version 1.0.0

  Build with:

    gcc -g basic-csap-client.c -o basic-csap-client -lcfsapi

  Distributed under the MIT license

  Copyright (c) 2013 Clarasoft I.T. Solutions Inc.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sub-license, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
  THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <clarasoft/cslib.h>
#include <clarasoft/cfs.h>

int main(int argc, char** argv) {

  uint64_t size;

  char szInBuffer[256];
  char szOutBuffer[262];

  CSAP pSession;
  CSAPSTAT status;
  CSAPCTL CtlFrame;

  if (argc < 2) {
    printf("Missing program argument: usage ./basic-csap-client \"string-to-echo\"\n");
    return 1;
  }

  pSession = CSAP_Constructor();

  if (CS_SUCCEED(CSAP_OpenService(pSession, NULL, "CSAP/SERVICES/BASIC-CSAP-LOCAL", &status))) {

    printf("Connected\n");
    strcpy(szOutBuffer, argv[1]);
    size = strlen(szOutBuffer);
    printf("Sending string: %s (%ld bytes)\n", argv[1], size);

    CSAP_Stream(pSession,  
                NULL, 
                szOutBuffer, 
                size, 
                CSAP_FMT_TEXT);

    if (CS_SUCCEED(CSAP_Receive(pSession, &CtlFrame))) {              

      memcpy(szInBuffer, CSAP_GetDataRef(pSession), CtlFrame.DataSize);
      szInBuffer[size] = 0;
      printf("Received %ld bytes: %s\n", size, szInBuffer);
    }
    else {
      printf("Failed to receive data from service\n");
    }

    // Disconnect from the service
    CSAP_CloseService(pSession);
  }
  else {
    printf("Connection failure\n");
  }
  
  CSAP_Destructor(&pSession);
   
  return 0;
}

