/* ==========================================================================

  Clarasoft Foundation Server - Linux

  CSAP APPLICATION service
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

#define _GNU_SOURCE     /* for basename( ) in <string.h> */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <clarasoft/cslib.h>
#include <clarasoft/cfs.h>
#include <clarasoft/csjson.h>
#include <clarasoft/cscpt.h>

#define CSAPAPP_OPER_EXECUTE   (0x01000000)

#define CSAPAPP_CMD            (0x00000001)
#define CSAPAPP_EXEC           (0x00000002)
#define CSAPAPP_COMMIT         (0x00001001)
#define CSAPAPP_ROLLBACK       (0x00001002)
#define CSAPAPP_EXIT           (0x0000FFFF)

#define CSAPAPP_APPEXIT        (0x0FFF0000)

#define ERR_CONFIG             (0x00001001)
#define ERR_QUERYINTERFACE     (0x00001002)
#define ERR_NOIMPLEMENTATION   (0x00001003)
#define ERR_RECEIVE            (0x00004001)
#define ERR_PARSE              (0x00004002)
#define ERR_PROTOCOL           (0x00004003) 

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Application method prototype
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

typedef CSRESULT
  (*CSAPAPP_SERVICEMETHOD)
    (CSJSON pIN,
     CSJSON pOUT);

typedef struct tagTRX {

  CSAPAPP_SERVICEMETHOD pTrxBegin;
  CSAPAPP_SERVICEMETHOD pTrxCommit;
  CSAPAPP_SERVICEMETHOD pTrxRollback;

} TRX;

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Application Service functions
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
    
CSRESULT
  CSAPAPP_LoadFramework
    (CSAP pSession,
     CSMAP pFrameworkMethods,
     CSJSON pJsonIn,
     CSJSON pJsonOut);
    
CSRESULT
  CSAPAPP_LoadFrameworkService
    (char* pServiceConfig,
     CSMAP pFrameworkMethods);
    
CSRESULT
  CSAPAPP_LoadService
    (char* pServiceConfig,
     CSMAP pServiceMethods,
     CSMAP pServiceTransactions);

CSRESULT
  CSAPAPP_Execute 
    (CSAP pSession,
     CSMAP pFrameworkMethods,
     CSJSON pJsonIn,
     CSJSON pJsonOut);

CSRESULT
  CSAPAPP_RunApplication
    (CSAP pSession,
     CSMAP pServiceMethods,
     CSMAP pServiceTransactions,
     CSJSON pJsonIn,
     CSJSON pJsonOut);

CSRESULT
  CSAPAPP_RunCommand 
    (CSAP pSession,
     CSMAP pFrameworkMethods,
     CSJSON pJsonIn,
     CSJSON pJsonOut);

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Service function
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

CSRESULT 
  CSAPAPP_RunService
    (CSAP pSession, 
     char* szUser) {

  CSRESULT hResult;
  CSJSON_LSENTRY lse;

  CSJSON pJsonIn;
  CSJSON pJsonOut;
  CSMAP pFrameworkMethods;

  CSAPCTL CtlFrame;

  pJsonIn = CSJSON_Constructor();
  pJsonOut = CSJSON_Constructor();
  pFrameworkMethods = CSMAP_Constructor();

  openlog(basename("SDLC-RunService"), LOG_PID, LOG_LOCAL3);

  // Load the application framework

  if (CS_FAIL(CSAP_Receive(pSession, &CtlFrame))) {
    syslog(LOG_ERR, "CSAPAPP - Initialization - RECEIVE: Failed to receive from broker");
    return CS_FAILURE;
  }

  if (CS_FAIL(CSJSON_Parse(pJsonIn, 
                           CSAP_GetDataRef(pSession), 0))) {
    syslog(LOG_ERR, "CSAPAPP - Initialization - PARSE: Invalid JSON from framework");
    return CS_FAILURE;
  }

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "op", &lse))) {
    syslog(LOG_ERR, "CSAPAPP - Initialization - missing ctl/op");
    return CS_FAILURE;
  }

  if (!strcmp("FRAMEWORK-INIT", lse.szValue)) {

    if (CS_FAIL(CSAPAPP_LoadFramework(pSession,
                                      pFrameworkMethods,
                                      pJsonIn,
                                      pJsonOut))) {

      syslog(LOG_ERR, "CSAPAPP - Initialization - PARSE: Invalid JSON from framework");
      return CS_FAILURE;
    }
  }
  else {
    syslog(LOG_ERR, "CSAPAPP - Initialization - invalid op");
    return CS_FAILURE;
  }

  do {

    if (CS_FAIL(CSAP_Receive(pSession, &CtlFrame))) {
      syslog(LOG_ERR, "CSAPAPP - RECEIVE: Failed to receive from broker");
      break;
    }

    if (CS_FAIL(CSJSON_Parse(pJsonIn, 
                             CSAP_GetDataRef(pSession), 0))) {
      syslog(LOG_ERR, "CSAPAPP - PARSE: Invalid JSON from framework");
      break;
    }

    if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "op", &lse))) {
      syslog(LOG_ERR, "CSAPAPP - missing ctl/op");
      break;
    }

    if (!strcmp("EXEC", lse.szValue)) {
      hResult = CSAPAPP_Execute(pSession, pFrameworkMethods, pJsonIn, pJsonOut);
    }
    else {
      if (!strcmp("CMD", lse.szValue)) {
        hResult = CSAPAPP_RunCommand(pSession, pFrameworkMethods, pJsonIn, pJsonOut);
      }
      else {
        // Invalid JSON string
        syslog(LOG_ERR, "CSAPAPP - invalid op");
        break;
      }
    }
  }                            
  while (CS_DIAG(hResult) != CSAPAPP_EXIT);

  CSJSON_Destructor(&pJsonIn);
  CSJSON_Destructor(&pJsonOut);
  CSMAP_Destructor(&pFrameworkMethods);

  closelog();

  return CS_SUCCESS;
}

CSRESULT
  CSAPAPP_LoadFramework
    (CSAP pSession,
     CSMAP pFrameworkMethods,
     CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pJsonStr;

  char szNumber[33];

  long Size;

  CSJSON_LSENTRY lse;
  CSAPAPP_SERVICEMETHOD* pMethod;
  CSRESULT hResult;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "config", &lse))) {

    syslog(LOG_ERR, "CSAPAPP - Initialization - LOAD-FRAMEWORK: missing config");

    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "1");
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "100");
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "1001");
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "FRAMEWORK-INIT");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }

  if (CS_FAIL(CSAPAPP_LoadFrameworkService(lse.szValue, pFrameworkMethods))) {

    syslog(LOG_ERR, "CSAPAPP - Initialization - LOAD-FRAMEWORK: failed to load framework methods");

    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "1");
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "100");
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "1002");
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "FRAMEWORK-INIT");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }

  // Call INIT method

  if (CS_SUCCEED(CSMAP_Lookup(pFrameworkMethods, "INIT", (void**)&pMethod, &Size))) {
      
    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);

    hResult = (*pMethod)(pJsonIn, pJsonOut);

    if (CS_SUCCEED(hResult)) {

      CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
      CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "0");
      CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "000");
      CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "0000");
      CSJSON_InsertString(pJsonOut, "/ctl", "op", "FRAMEWORK-INIT");
    }
    else {

      hResult = CS_FAILURE | CSAPAPP_APPEXIT | CSAPAPP_EXEC;
      CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
      sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
      CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
      sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
      CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
      sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
      CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
      CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
      CSJSON_InsertString(pJsonOut, "/ctl", "op", "CMD");

      Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

      CSAP_Stream(pSession, 
                  "", 
                  pJsonStr, 
                  Size, 
                  CSAP_FMT_DEFAULT);              

      return CS_FAILURE;
    }
  }
  else {

    hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_NOIMPLEMENTATION;
    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
    sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
    sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
    CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "CMD");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }
 
  Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

  CSAP_Stream(pSession, 
              "", 
              pJsonStr, 
              Size, 
              CSAP_FMT_DEFAULT);              

  return CS_SUCCESS;
}

CSRESULT
  CSAPAPP_Execute 
    (CSAP pSession,
     CSMAP pFrameworkMethods,
     CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pJsonStr;

  char szNumber[33];

  long Size;

  CSRESULT hResult;
  CSJSON_LSENTRY lse;
  CSMAP pServiceMethods;
  CSMAP pServiceTransactions;
  CSAPAPP_SERVICEMETHOD* pMethod;
  
  CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "config", &lse))) {

    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "1");
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "100");
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "1001");
    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }

  pServiceMethods = CSMAP_Constructor();
  pServiceTransactions = CSMAP_Constructor();

  if (CS_FAIL(CSAPAPP_LoadService(lse.szValue, pServiceMethods, 
                                               pServiceTransactions))) {
    CSMAP_Destructor(&pServiceMethods);
    CSMAP_Destructor(&pServiceTransactions);

    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "1");
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "100");
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "1002");
    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }

  // Execute INIT method, if there is one
  if (CS_SUCCEED(CSMAP_Lookup(pServiceMethods, "INIT", (void**)&pMethod, &Size))) {
                  
    hResult = (*pMethod)(pJsonIn, pJsonOut);

    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
    sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
    sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
    CSJSON_InsertString(pJsonOut, "/ctl", "method", "INIT");
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "EXEC");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    if (CS_FAIL(hResult)) {
      return CS_FAILURE;
    }
  }
  else {

    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "0");
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "000");
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "0000");
    CSJSON_InsertNull(pJsonOut, "/ctl", "method");
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "EXEC");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              
  }

  CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
  CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);

  hResult = CSAPAPP_RunApplication(pSession,
                                   pServiceMethods,
                                   pServiceTransactions,
                                   pJsonIn,
                                   pJsonOut);

  if (CS_DIAG(hResult) == CSAPAPP_CMD) {
    hResult = CSAPAPP_RunCommand(pSession, pFrameworkMethods, pJsonIn, pJsonOut); 
  }
  else {

    sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
    sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
    sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
    CSJSON_InsertString(pJsonOut, "/ctl", "config", lse.szValue);
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "EXEC");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);
            
    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              
  }

  CSMAP_Destructor(&pServiceMethods);
  CSMAP_Destructor(&pServiceTransactions);

  return hResult;
}

CSRESULT
  CSAPAPP_RunApplication
    (CSAP pSession,
     CSMAP pServiceMethods,
     CSMAP pServiceTransactions,
     CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pJsonStr;

  char szNumber[33];

  long Size;

  CSRESULT hResult;
  CSJSON_LSENTRY lse;
  CSAPCTL CtlFrame;
  CSAPAPP_SERVICEMETHOD* pMethod;
  TRX* pTrxVtbl;

  do {

    if (CS_FAIL(CSAP_Receive(pSession, &CtlFrame))) {
      hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_RECEIVE;
    }

    if (CS_FAIL(CSJSON_Parse(pJsonIn, 
                             CSAP_GetDataRef(pSession), 0))) {
      hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PARSE;
      break;
    }

    if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "op", &lse))) {
      hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
      break;
    }

    if (!strcmp("CALL", lse.szValue)) {

      if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "method", &lse))) {
        hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
        break;
      }

      if (CS_FAIL(CSMAP_Lookup(pServiceMethods, lse.szValue, (void**)&pMethod, &Size))) {
        hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_NOIMPLEMENTATION;
        break;
      }
                  
      CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);            
      hResult = (*pMethod)(pJsonIn, pJsonOut);

      CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
      sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
      CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
      sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
      CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
      sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
      CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
      CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
      CSJSON_InsertString(pJsonOut, "/ctl", "op", "CALL");

      Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);
            
      CSAP_Stream(pSession, 
                  "", 
                  pJsonStr, 
                  Size, 
                  CSAP_FMT_DEFAULT);       
    }
    else {

      if (!strcmp("TRX-BEGIN", lse.szValue)) {

        if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "method", &lse))) {
          syslog(LOG_ERR, "CSAPAPP - TRX-BEGIN: Failed to lookup method");
          hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
          break;
        }
        else {

          if (CS_FAIL(CSMAP_Lookup(pServiceTransactions,
                                   lse.szValue, 
                                   (void**)&pTrxVtbl, 
                                   &Size))) {

            syslog(LOG_ERR, "CSAPAPP - TRX-BEGIN: Failed to fetch implementation");
            hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_NOIMPLEMENTATION;
            break;
          }

          // Call preparation step of transaction

          CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);            
          hResult = (pTrxVtbl->pTrxBegin)(pJsonIn, pJsonOut);

          CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
          sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
          CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
          sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
          CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
          sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
          CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
          CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
          CSJSON_InsertString(pJsonOut, "/ctl", "op", "TRX-BEGIN");
          
          Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);
             
          CSAP_Stream(pSession, 
                      "", 
                      pJsonStr, 
                      Size, 
                      CSAP_FMT_DEFAULT);              

          if (CS_SUCCEED(hResult)) {
          
            // Wait for client response

            if (CS_FAIL(CSAP_Receive(pSession, &CtlFrame))) {
              hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_RECEIVE;
            }

            if (CS_FAIL(CSJSON_Parse(pJsonIn, 
                                   CSAP_GetDataRef(pSession), 0))) {
              hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PARSE;
              break;
            }

            if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "op", &lse))) {
              hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
              break;
            }

            if (!strcmp("TRX-COMMIT", lse.szValue)) {

              CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);            
              hResult = (pTrxVtbl->pTrxCommit)(pJsonIn, pJsonOut);

              CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
              sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
              CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
              sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
              CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
              sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
              CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
              CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
              CSJSON_InsertString(pJsonOut, "/ctl", "op", "TRX-COMMIT");

              Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);
            
              CSAP_Stream(pSession, 
                          "", 
                          pJsonStr, 
                          Size, 
                          CSAP_FMT_DEFAULT);              
            }
            else {

              if (!strcmp("TRX-ROLLBACK", lse.szValue)) {

                CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);            
                hResult = (pTrxVtbl->pTrxRollback)(pJsonIn, pJsonOut);

                CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
                sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
                CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
                sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
                CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
                sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
                CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
                CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
                CSJSON_InsertString(pJsonOut, "/ctl", "op", "TRX-ROLLBACK");

                Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);
            
                CSAP_Stream(pSession, 
                            "", 
                            pJsonStr, 
                            Size, 
                            CSAP_FMT_DEFAULT);              
              }
              else {

                if (!strcmp("CMD", lse.szValue)) {
                  hResult = CS_SUCCESS | CSAPAPP_APPEXIT | CSAPAPP_CMD;
                }
                else {
                  syslog(LOG_ERR, "CSAPAPP - Invalid operation instruction");
                  hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
                  break;
                }
              }
            }
          }
        }
      }
      else {

        if (!strcmp("CMD", lse.szValue)) {
          hResult = CS_SUCCESS | CSAPAPP_APPEXIT | CSAPAPP_CMD;
        }
        else {
          syslog(LOG_ERR, "CSAPAPP - Invalid operation instruction");
          hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
          break;
        }
      }
    }
  }
  while ((CS_OPER(hResult) != CSAPAPP_APPEXIT));

  // Call exit function  
  if (CS_SUCCEED(CSMAP_Lookup(pServiceMethods, "EXIT", (void**)&pMethod, &Size))) {
    (*pMethod)(pJsonIn, pJsonOut);
  }  

  return hResult;
}

CSRESULT
  CSAPAPP_RunCommand 
    (CSAP pSession,
     CSMAP pFrameworkMethods,
     CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pJsonStr;

  char szNumber[33];

  long Size;

  CSRESULT hResult;
  CSAPAPP_SERVICEMETHOD* pMethod;
  CSJSON_LSENTRY lse;
  //CSAPCTL CtlFrame;

  CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/ctl", "method", &lse))) {
    hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_PROTOCOL;
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
    sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
    sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
    CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "CMD");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }

  if (CS_FAIL(CSMAP_Lookup(pFrameworkMethods, lse.szValue, (void**)&pMethod, &Size))) {
    hResult = CS_FAILURE | CSAPAPP_APPEXIT | ERR_NOIMPLEMENTATION;
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
    sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
    sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
    CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "CMD");

    Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

    CSAP_Stream(pSession, 
                "", 
                pJsonStr, 
                Size, 
                CSAP_FMT_DEFAULT);              

    return CS_FAILURE;
  }
                  
  CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);            
  hResult = (*pMethod)(pJsonIn, pJsonOut);

  if (CS_SUCCEED(hResult)) {
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", "0");
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", "000");
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", "00000");
    CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "CMD");
  }
  else {
    CSJSON_MkDir(pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
    sprintf(szNumber, "%.1lX", (CS_FAILURE & hResult) >> 31);
    CSJSON_InsertString(pJsonOut, "/ctl", "HRESULT", szNumber);
    sprintf(szNumber, "%.3lX", CS_OPER(hResult) >> 16);
    CSJSON_InsertString(pJsonOut, "/ctl", "FACILITY", szNumber);
    sprintf(szNumber, "%.4lX", CS_DIAG(hResult));
    CSJSON_InsertString(pJsonOut, "/ctl", "REASON", szNumber);
    CSJSON_InsertString(pJsonOut, "/ctl", "method", lse.szValue);
    CSJSON_InsertString(pJsonOut, "/ctl", "op", "CMD");
  }

  Size = CSJSON_Serialize(pJsonOut, "/", &pJsonStr, 0);

  CSAP_Stream(pSession, 
              "", 
              pJsonStr, 
              Size, 
              CSAP_FMT_DEFAULT);              
      
  return CS_SUCCESS | CSAPAPP_CMD;
}

/////////////////////////////////////////////////////////////////////////////
//
//  This function activates a service program and
//  retrieves a pointer from an exported procedure
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAPAPP_LoadFrameworkService
    (char* pServiceConfig,
     CSMAP pServiceMethods) {

  char* pszValue;
  char* pszMethod;

  void *inprocServer;

  CFSRPS pRepo;
  CFSCFG pConfig;

  // Pointer to CSAP service handler function exported by service program.

  CSAPAPP_SERVICEMETHOD pExport;

  if ((pRepo = CFSRPS_Open(0)) == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Failed to open CFS Repository");
    return CS_FAILURE;
  }

  if ((pConfig = CFSRPS_OpenConfig(pRepo, pServiceConfig)) == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Handler configuration not found");
    CFSRPS_Close(&pRepo);
    return CS_FAILURE;
  }

  if ((pszValue = CFSCFG_LookupParam(pConfig, "INPROCSERVER")) == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Failed to read config parameter: INPROCHANDLER_SRV"); 
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    return CS_FAILURE;
  }

  inprocServer = dlopen(pszValue, RTLD_NOW);
  
  if (inprocServer == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Failed to load shared library - %s", dlerror());
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    return CS_FAILURE;
  }

  CSMAP_Clear(pServiceMethods);
  CFSCFG_IterStart(pConfig, "METHODS");
  while ((pszMethod = CFSCFG_IterNext(pConfig)) != NULL) {

    if ((pszValue = CFSCFG_LookupParam(pConfig, pszMethod)) == NULL) {
      syslog(LOG_ERR, "CSAPAPP - Failed to read method value"); 
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      return CS_FAILURE;
    }

    pExport = dlsym(inprocServer, pszValue);
    CSMAP_Insert(pServiceMethods, pszMethod, &pExport, sizeof(pExport));
  }

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

  return CS_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////
//
//  This function activates a service program and
//  retrieves a pointer from an exported procedure
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAPAPP_LoadService
    (char* pServiceConfig,
     CSMAP pServiceMethods,
     CSMAP pServiceTransactions) {

  char* pszValue;
  char* pszMethod;

  void *inprocServer;

  CFSRPS pRepo;
  CFSCFG pConfig;
  CFSCFG pTrxConfig;

  TRX TransactionVtbl;

  // Pointer to CSAP service handler function exported by service program.

  CSAPAPP_SERVICEMETHOD pExport;

  if ((pRepo = CFSRPS_Open(0)) == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Failed to open CFS Repository");
    return CS_FAILURE;
  }

  if ((pConfig = CFSRPS_OpenConfig(pRepo, pServiceConfig)) == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Handler configuration not found");
    CFSRPS_Close(&pRepo);
    return CS_FAILURE;
  }

  if ((pszValue = CFSCFG_LookupParam(pConfig, "INPROCSERVER")) == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Failed to read config parameter: INPROCHANDLER_SRV"); 
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    return CS_FAILURE;
  }

  inprocServer = dlopen(pszValue, RTLD_NOW);
  
  if (inprocServer == NULL) {
    syslog(LOG_ERR, "CSAPAPP - Failed to load shared library - %s", dlerror());
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    return CS_FAILURE;
  }

  CSMAP_Clear(pServiceMethods);
  CFSCFG_IterStart(pConfig, "METHODS");
  while ((pszMethod = CFSCFG_IterNext(pConfig)) != NULL) {

    if ((pszValue = CFSCFG_LookupParam(pConfig, pszMethod)) == NULL) {
      syslog(LOG_ERR, "CSAPAPP - Failed to read method value"); 
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      return CS_FAILURE;
    }

    pExport = dlsym(inprocServer, pszValue);
    CSMAP_Insert(pServiceMethods, pszMethod, &pExport, sizeof(pExport));
  }

  CSMAP_Clear(pServiceTransactions);
  if (CS_SUCCEED(CFSCFG_IterStart(pConfig, "TRANSACTIONS"))) {

    while ((pszValue = CFSCFG_IterNext(pConfig)) != NULL) {

      // Open transactrion configuration
      if ((pTrxConfig = CFSRPS_OpenConfig(pRepo, pszValue)) == NULL) {
        syslog(LOG_ERR, "CSAPAPP - Transactions configuration not found");
        CFSRPS_CloseConfig(pRepo, &pConfig);
        CFSRPS_Close(&pRepo);
        return CS_FAILURE;
      }

      if ((pszValue = CFSCFG_LookupParam(pTrxConfig, "TRX-BEGIN")) == NULL) {
        syslog(LOG_ERR, "CSAPAPP - Failed to read config parameter: TRX-BEGIN"); 
        CFSRPS_CloseConfig(pRepo, &pConfig);
        CFSRPS_CloseConfig(pRepo, &pTrxConfig);
        CFSRPS_Close(&pRepo);
        return CS_FAILURE;
      }

      TransactionVtbl.pTrxBegin = dlsym(inprocServer, pszValue);

      if ((pszValue = CFSCFG_LookupParam(pTrxConfig, "TRX-COMMIT")) == NULL) {
        syslog(LOG_ERR, "CSAPAPP - Failed to read config parameter: TRX-COMMIT"); 
        CFSRPS_CloseConfig(pRepo, &pConfig);
        CFSRPS_CloseConfig(pRepo, &pTrxConfig);
        CFSRPS_Close(&pRepo);
        return CS_FAILURE;
      }

      TransactionVtbl.pTrxCommit = dlsym(inprocServer, pszValue);

      if ((pszValue = CFSCFG_LookupParam(pTrxConfig, "TRX-ROLLBACK")) == NULL) {
        syslog(LOG_ERR, "CSAPAPP - Failed to read config parameter: TRX-ROLLBACK"); 
        CFSRPS_CloseConfig(pRepo, &pConfig);
        CFSRPS_CloseConfig(pRepo, &pTrxConfig);
        CFSRPS_Close(&pRepo);
        return CS_FAILURE;
      }

      TransactionVtbl.pTrxRollback = dlsym(inprocServer, pszValue);

      if ((pszValue = CFSCFG_LookupParam(pTrxConfig, "METHOD_NAME")) == NULL) {
        syslog(LOG_ERR, "CSAPAPP - Failed to read config parameter: METHOD_NAME"); 
        CFSRPS_CloseConfig(pRepo, &pConfig);
        CFSRPS_CloseConfig(pRepo, &pTrxConfig);
        CFSRPS_Close(&pRepo);
        return CS_FAILURE;
      }

      CSMAP_Insert(pServiceTransactions, 
                   pszValue, &TransactionVtbl, sizeof(TransactionVtbl));

      CFSRPS_CloseConfig(pRepo, &pTrxConfig);
    }
  }

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

  return CS_SUCCESS;
}


