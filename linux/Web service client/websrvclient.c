/* ===========================================================================
  Clarasoft Foundation Server 400
  websrvclient.c

  Example web service client using CSHTTP and TLS

  Usage:

    websrvclient "hostname" "uri" "port"

  For example, to get a JSON response from a web service called bored, which
  returns a suggestion for an activity to fight boredom, issue the following
  command:

    ./websrvclient "www.boredapi.com" "/api/activity" "443"

  This assumes you have a TLS configuration named WEBSERVICE/TLS/DEFAULT_CONFIG
  with the following parameters:

     TLS_VALIDATE_PEER : *YES
     TLS_SESSION_TYPE  : *CLIENT


  Distributed under the MIT license

  Copyright (c) 2022 Clarasoft I.T. Solutions Inc.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sublicense, and/or sell
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

=========================================================================== */

#include<stdio.h>
#include <clarasoft/cfs.h>

int main(int argc, char** argv) {

  int rc;
  char* pResponse;
  long size;

  CSRESULT hResult;
  CSHTTP pHTTP;

  CFS_SESSION* pSession;
  CFSENV pENV;
  
  pHTTP = CSHTTP_Constructor();

  CSHTTP_StartRequest(pHTTP, 
                      CSHTTP_METHOD_GET, 
                      CSHTTP_VER_1_1, argv[2]);

  CSHTTP_SetStdHeader(pHTTP, CSHTTP_Host, argv[1]);

  pENV = CFS_OpenEnv("WEBSERVICE/TLS/DEFAULT_CONFIG");

  pSession = CFS_OpenSession(pENV, NULL, argv[1], argv[3], &rc); 

  if (pSession != 0) {

    CSHTTP_SendRequest(pHTTP, pSession);

    size = CSHTTP_GetDataSize(pHTTP);
    CSHTTP_GetDataRef(pHTTP, &pResponse);

    printf("\nReceived %ld bytes\n\n%s", size, pResponse);

    CFS_CloseSession(pSession, &rc);
  }
  else {
    printf("\nRequest failure");
  }
  CFS_CloseEnv(&pENV);

  CSHTTP_Destructor(&pHTTP);

  printf("\nBye!\n");
  return 0;
}

