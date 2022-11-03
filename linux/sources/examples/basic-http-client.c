/* ==========================================================================

  Clarasoft Foundation Server - Linux

  http client program
  Version 1.0.0

  Build with:

    gcc -g basic-http-client.c -o basic-http-client -lcfsapi

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

#include <clarasoft/cfs.h>

// gcc -g basic-http-client.c -o driver -lcfsapi

int main() {

  CSHTTP pWeb;
  CFS_SESSION* pSession;

  pWeb = CSHTTP_Constructor();

  CSHTTP_StartRequest(pWeb, 
                      CSHTTP_METHOD_GET, 
                      CSHTTP_VER_1_0, 
                      "/");

  pSession = CFS_OpenSession(NULL, NULL, "localhost", "80");

  if (pSession != NULL) {
    CSHTTP_SendRequest(pWeb, pSession);

    printf("\n\n%s\n", CSHTTP_GetDataRef(pWeb));
  }
    
  CSHTTP_Destructor(&pWeb);
  CFS_CloseSession(&pSession);

  return 0;
}

