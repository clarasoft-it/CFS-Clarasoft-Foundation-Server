/* ==========================================================================

  Clarasoft Foundation Server

  generic echo server
  Version 1.0.0

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

// gcc -shared -fPIC -Wl,-soname,libclarah-echo.so -o libclarah-echo.so clarah-echo.o -lc -lcfsapi
//	sudo cp libclarah-echo.so /usr/lib/clarasoft
//	sudo ldconfig -n /usr/lib/clarasoft
//	sudo ln -sf /usr/lib/clarasoft/libclarah-echo.so /usr/lib/libclarah-echo.so



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <unistd.h>
#include <time.h>

#include <clarasoft/cfs.h>

void Echo(CFS_SESSION* pSession) {

  long size;

  char szInBuffer[256];
  char szOutBuffer[262];

  size = 255;
  if (CS_SUCCEED(pSession->lpVtbl->CFS_Receive(pSession, szInBuffer, &size))) {

    szInBuffer[size > 255 ? 255 : size] = 0;
    strcpy(szOutBuffer, "ECHO: ");
    strcat(szOutBuffer, szInBuffer);

    size = strlen(szOutBuffer);

    pSession->lpVtbl->CFS_SendRecord(pSession, szOutBuffer, &size);
  }

  return;
}
