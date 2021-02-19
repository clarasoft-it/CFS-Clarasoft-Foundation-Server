/* ==========================================================================

  Clarasoft Foundation Server

  echo-service.c
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

#include <clarasoft/cfsapi.h>

#define BUFF_MAX 129

void ExecHandler(CFS_SESSION *pInstance, char *szConfig)
{

  char szMessage[BUFF_MAX];
  char szResponse[BUFF_MAX + 16];

  int rc;

  uint64_t size;

  CSRESULT hResult;

  do
  {
    size = BUFF_MAX;
    hResult = pInstance->lpVtbl->CFS_Read(pInstance,
                                          szMessage,
                                          &size,
                                          -1, &rc);

    if (CS_SUCCEED(hResult))
    {
      // null-terminate message...
      szMessage[size] = 0;

      ////////////////////////////////////////////////////////////
      // Just to give a way for the client to
      // disconnect from this handler, if the first
      // character is the letter q, then we leave
      // the loop.
      ////////////////////////////////////////////////////////////

      if (szMessage[0] == 'q')
      {
        break;
      }

      ////////////////////////////////////////////////////////////
      // echo back the message in a response:
      ////////////////////////////////////////////////////////////

      strcpy(szResponse, "ECHO HANDLER: ");
      strcat(szResponse, szMessage);

      size = strlen(szResponse);

      hResult = pInstance->lpVtbl->CFS_WriteRecord
                                     (pInstance,
                                      szResponse,
                                      &size,
                                      -1, &rc);
    }
  } while (CS_SUCCEED(hResult));

  strcpy(szResponse, "BYE!");

  size = strlen(szResponse);

  hResult = pInstance->lpVtbl->CFS_WriteRecord
                                 (pInstance,
                                  szResponse,
                                  &size,
                                  -1, &rc);

  return;
}
