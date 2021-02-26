/* ==========================================================================

  Clarasoft Foundation Server

  web-service-proc.c
  example loadable service implementation
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

#include <string.h>

#include <clarasoft/cshttp.h>

#define BUFF_MAX 129

void RunService(CFS_SESSION *pInstance, char *szConfig, CSHTTP pHTTP)
{

  char szResponse[BUFF_MAX + 16];

  if (CS_SUCCEED(CSHTTP_RecvRequest(pHTTP, pInstance))) {
    strcpy(szResponse, "Received request!");
  }
  else {
    strcpy(szResponse, "Error receiving request!");
  }

  CSHTTP_StartResponse(pHTTP, 200, CSHTTP_VER_1_1);
  CSHTTP_SetStdHeader(pHTTP, CSHTTP_Content_Type, "text/plain");

  CSHTTP_InsertData(pHTTP, "Fragment 1", 10, 0);
  CSHTTP_InsertData(pHTTP, "Fragment 2", 10, 0);
  CSHTTP_InsertData(pHTTP, "End Fragment", 12, 0);

  CSHTTP_SendResponse(pHTTP, pInstance);

  return;
}
