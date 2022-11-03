/* ==========================================================================

  Clarasoft Foundation Server - Linux

  client program for websocket echo service
  Version 1.0.0

  Build with:

    gcc -g basic-websocket-client.c -o basic-websocket-client -lcfsapi

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

  CSWSCK pSession;

  if (argc < 2) {
    printf("Missing program argument: usage ./echo-test \"string-to-echo\"\n");
    return 1;
  }

  pSession = CSWSCK_Constructor();

  if (CS_SUCCEED(CSWSCK_OpenSession(pSession, NULL, NULL, "localhost", "11010"))) {

    printf("Connected\n");
    strcpy(szOutBuffer, argv[1]);
    size = (uint64_t)strlen(szOutBuffer);
    printf("Sending string: %s (%ld bytes)\n", argv[1], size);

    CSWSCK_Send(pSession, 
                CSWSCK_OPER_TEXT, 
                szOutBuffer, 
                size, 
                CSWSCK_FIN_ON);

    if (CS_SUCCEED(CSWSCK_Receive(pSession, &size))) {              

      memcpy(szInBuffer, CSWSCK_GetDataRef(pSession), size);
      szInBuffer[size] = 0;
      printf("Received %ld bytes: %s\n", size, szInBuffer);
    }
    else {
      printf("Failed to receive data from service\n");
    }
    
    CSWSCK_CloseSession(pSession, 0, 0);
  }
  else {
    printf("Connection failure\n");
  }

  CSWSCK_Destructor(&pSession);
  
  return 0;
}

