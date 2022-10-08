/* ==========================================================================

  Clarasoft Foundation Server

  generic-service-proc.c
  example CSAP service handler
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

#include <clarasoft/cslib.h>
#include <clarasoft/csap.h>

void RunService(CSAP pCSAP, char* szUser)
{

  char szBuffer[276]; 
  char* pData;
  char* pUsrCtl;

  CSAPCTL ctl;

  while (CS_SUCCEED(CSAP_Receive(pCSAP, &ctl))) {

    if (ctl.DataSize >= 0 && ctl.DataSize < 256) {

      if (ctl.DataSize == 0) {

        // no data to echo
        // includes prefix and NULL
        strcpy(szBuffer, "CSAP ECHO SERVICE: ");
        CSAP_Put(pCSAP, szBuffer, (19) * sizeof(char));

        if ((pUsrCtl = CSAP_GetUserCtlRef(pCSAP)) != NULL) {
          CSAP_SendEx(pCSAP, ctl.szSessionID, pUsrCtl, strlen(pUsrCtl), CSAP_FMT_DEFAULT);
        }
        else {
          CSAP_SendEx(pCSAP, ctl.szSessionID, 0, 0, CSAP_FMT_DEFAULT);
        }
      }
      else {

        pData = CSAP_GetDataRef(pCSAP);

        if (!strcmp("QUIT", pData)) {

          strcpy(szBuffer, "CSAP ECHO SERVICE: BYE!");

          CSAP_Put(pCSAP, szBuffer, 23);
          CSAP_SendEx(pCSAP, ctl.szSessionID, 0, 0, CSAP_FMT_DEFAULT);
          break;
        }

        // includes prefix and NULL
        strcpy(szBuffer, "CSAP ECHO SERVICE: ");
        strcat(szBuffer, pData);
        CSAP_Put(pCSAP, szBuffer, (ctl.DataSize + 19) * sizeof(char));

        if ((pUsrCtl = CSAP_GetUserCtlRef(pCSAP)) != NULL) {
          CSAP_SendEx(pCSAP, ctl.szSessionID, pUsrCtl, strlen(pUsrCtl), CSAP_FMT_DEFAULT);
        }
        else {
          CSAP_SendEx(pCSAP, ctl.szSessionID, 0, 0, CSAP_FMT_DEFAULT);
        }
      }
    }
    else {

      strcpy(szBuffer, "CSAP ECHO SERVICE: ERROR - Data size too large - 255 characters max");

      CSAP_Put(pCSAP, szBuffer, 67);
      CSAP_SendEx(pCSAP, ctl.szSessionID, 0, 0, CSAP_FMT_DEFAULT);
    }
  }

  return;
}