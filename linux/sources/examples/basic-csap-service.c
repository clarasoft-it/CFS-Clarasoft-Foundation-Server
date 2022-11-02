/* ==========================================================================

  Clarasoft Foundation Server

  basic csap service
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BUILD:
//
// gcc -g -c basic-csap-service.c -o basic-csap-service.o
// gcc -shared -fPIC -Wl,-soname,libbasic-csap-service.so -o libbasic-csap-service.so basic-csap-service.o -lc -lcfsapi
// sudo cp libbasic-csap-service.so /usr/lib/clarasoft
// sudo ldconfig -n /usr/lib/clarasoft
// sudo ln -sf /usr/lib/clarasoft/libbasic-csap-service.so /usr/lib/libbasic-csap-service.so
//
// Running the service:
//
// ./clarad CSAP/DAEMONS/BASIC-CSAP
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <clarasoft/cfs.h>

void BasicCSAPEcho(CSAP pSession, char* szUser) {

  CSAPCTL CtlFrame;

  if (CS_SUCCEED(CSAP_Receive(pSession, &CtlFrame))) {

    CSAP_Stream(pSession, 
                CtlFrame.szSessionID, 
                CSAP_GetDataRef(pSession), 
                CtlFrame.DataSize, 
                CtlFrame.fmt);
  }

  return;
}

