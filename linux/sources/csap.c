/* ==========================================================================

  Clarasoft Foundation Server - Linux
  Common Service Access Protocol

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

#include <clarasoft/cfsrepo.h>
#include <clarasoft/cswsck.h>
#include <clarasoft/csjson.h>

#define CSAP_NULL_SESSION    "000000000000000000000000000000000000"

#define CSAP_GETDATA         (0x00010000)
#define CSAP_GETDATAREF      (0x00020000)
#define CSAP_USRCTL          (0x00030000)
#define CSAP_SEND            (0x00040000)
#define CSAP_RECEIVE         (0x00050000)
#define CSAP_SETDATA         (0x00060000)
#define CSAP_OPEN            (0x00070000)
#define CSAP_HANDSHAKE       (0x00080000)

#define CSAP_OVERFLOW        (0x00000001)
#define CSAP_TRANSPORT       (0x00000002)
#define CSAP_SIZE            (0x00000003)
#define CSAP_STATUS          (0x00000005)
#define CSAP_CONFIG          (0x00000006)
#define CSAP_PROTOCOL        (0x00000007)
#define CSAP_USERCTL         (0x00000008)
#define CSAP_NULLBUFFER      (0x00000009)
#define CSAP_FORMAT          (0x0000000A)
#define CSAP_CONNECT         (0x0000000B)
#define CSAP_DATA            (0x0000000C)

#define CSAP_FMT_DEFAULT     (0x00000010)
#define CSAP_FMT_TEXT        (0x00000010)
#define CSAP_FMT_BINARY      (0x00000020)

#define CSAP_RCVMODE_CACHE   (0x00000001)
#define CSAP_RCVMODE_STREAM  (0x00000002)
#define CSAP_RCVMODE_DEFAULT (0x00000002)

#define CSAP_USRCTLSLABSIZE  (1024LL)
#define CSAP_DATASLABSIZE    (65536)
#define CSAP_MAX_SEGMENTSIZE (2097152L)

typedef struct tagCSAPSTAT {

  char szVersion[11];
  char szStatus[4];
  char szReason[11];
  char szSessionID[37];

} CSAPSTAT;

typedef struct tagCSAPCTL {

  long UsrCtlSize;
  long DataSize;

} CSAPCTL;

typedef struct tagCSAP {

  CSWSCK pSession;

  CSJSON pJsonIn;
  CSJSON pJsonOut;

  CSJSON_LSENTRY lse;

  char szSessionID[37];

  long inDataSize;
  long outDataSize;
  long outDataSlabSize;
  long UsrCtlSize;
  long UsrCtlSlabSize;
  long fmt;

  char* pUsrCtlSlab;
  char* pInDataSlab;
  char* pOutDataSlab;

  CSLIST OutDataParts;

  CFSRPS pRepo;
  CFSRPS pConfig;

  CSAPCTL ctl;

} CSAP;

CSAP*
  CSAP_Constructor
    (void) {

  CSAP* Instance;

  Instance = (CSAP*)malloc(sizeof(CSAP));

  Instance->pSession = CSWSCK_Constructor();

  Instance->inDataSize = 0;
  Instance->outDataSize = 0;
  Instance->UsrCtlSize = 0;
  Instance->UsrCtlSlabSize = CSAP_USRCTLSLABSIZE;
  Instance->outDataSlabSize = CSAP_DATASLABSIZE;

  Instance->pUsrCtlSlab =
      (char*)malloc((Instance->UsrCtlSlabSize + 1) * sizeof(char));

  Instance->pOutDataSlab = (char*)malloc
                ((Instance->outDataSlabSize + 1) * sizeof(char));

  Instance->OutDataParts = CSLIST_Constructor();

  Instance->pJsonIn = CSJSON_Constructor();
  Instance->pJsonOut = CSJSON_Constructor();

  return Instance;
}

CSRESULT
  CSAP_Destructor
    (CSAP** This) {

  CSLIST_Destructor(&((*This)->OutDataParts));
  CSWSCK_Destructor(&((*This)->pSession));
  CSJSON_Destructor(&((*This)->pJsonIn));
  CSJSON_Destructor(&((*This)->pJsonOut));

  free((*This)->pUsrCtlSlab);
  free((*This)->pOutDataSlab);

  free(*This);

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Clear
    (CSAP* This) {

  This->inDataSize = 0;
  This->outDataSize = 0;
  This->UsrCtlSize = 0;

  CSLIST_Clear(This->OutDataParts);

  return CS_SUCCESS;
}

CSRESULT
  CSAP_CloseService
    (CSAP* This) {

  CSWSCK_CloseSession(This->pSession, 0, 0);
  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// Caller must provide a buffer large enough to receive data.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Get
    (CSAP* This,
     char* pData,
     long Offset,
     long Size) {

  if (Offset >= This->inDataSize) {
    return CS_FAILURE | CSAP_GETDATA | CSAP_OVERFLOW;
  }

  if (Size > (This->inDataSize - Offset)) {
    memcpy(pData, CSWSCK_GetDataRef(This->pSession) + Offset,
                  This->inDataSize - Offset);
    return CS_SUCCESS | CSAP_GETDATA | CSAP_OVERFLOW;
  }

  memcpy(pData, CSWSCK_GetDataRef(This->pSession) + Offset, Size);
  return CS_SUCCESS;
}

void*
  CSAP_GetDataRef
    (CSAP* This) {

  return CSWSCK_GetDataRef(This->pSession);
}

CSRESULT
  CSAP_GetUserCtl
    (CSAP* This,
     char* pData) {

  if (This->UsrCtlSize > 0) {
    memcpy(pData, This->pUsrCtlSlab, This->UsrCtlSize);
    pData[This->UsrCtlSize] = 0;
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

char*
  CSAP_GetUserCtlRef
    (CSAP* This) {

  return This->pUsrCtlSlab;
}

CSRESULT
  CSAP_OpenService
    (CSAP* This,
     CFSENV pEnv,
     char* szService,
     CSAPSTAT* status) {

  uint64_t size;
  
  char* pszParam;
  char* szBuffer;

  // Reset outbound data, in case a send is
  // performed immidiately after the open. A receive
  // will properly reset the CSAP instance.

  This->inDataSize = 0;
  This->outDataSize = 0;

  CSLIST_Clear(This->OutDataParts);

  CSJSON_Init(This->pJsonOut, JSON_TYPE_OBJECT);

  This->pRepo = CFSRPS_Open(0);

  if ((This->pConfig = CFSRPS_OpenConfig(This->pRepo, szService)) == NULL) {
    CFSRPS_Close(&This->pRepo);
    return CS_FAILURE | CSAP_OPEN | CSAP_CONFIG;
  }
  else {

    if ((pszParam = CFSCFG_LookupParam(This->pConfig, "SERVICE")) != NULL) {
      CSJSON_InsertString(This->pJsonOut, "/", "service", pszParam);
    }
    else {
      CFSRPS_CloseConfig(This->pRepo, &(This->pConfig));
      CFSRPS_Close(&(This->pRepo));
      return CS_FAILURE | CSAP_OPEN | CSAP_CONFIG;
    }
    if ((pszParam = CFSCFG_LookupParam(This->pConfig, "U")) != NULL) {
      CSJSON_InsertString(This->pJsonOut, "/", "u", pszParam);
    }
    else {
      CSJSON_InsertString(This->pJsonOut, "/", "u", "");
    }
    if ((pszParam = CFSCFG_LookupParam(This->pConfig, "P")) != NULL) {
      // decrypt password
      CSJSON_InsertString(This->pJsonOut, "/", "p", pszParam);
    }
    else {
      CSJSON_InsertString(This->pJsonOut, "/", "p", "");
    }
  }

  CFSRPS_CloseConfig(This->pRepo, &(This->pConfig));
  CFSRPS_Close(&(This->pRepo));

  if (CS_SUCCEED(CSWSCK_OpenSession
                           (This->pSession, 
                            pEnv, 
                            szService, 
                            0, 0))) {

    size = (uint64_t)CSJSON_Serialize(This->pJsonOut, "/", &szBuffer, 0);

    if (CS_SUCCEED(CSWSCK_Send(This->pSession,
                               CSWSCK_OP_TEXT,
                               szBuffer,
                               size, CSWSCK_FIN_ON))) {

      if (CS_SUCCEED(CSWSCK_ReceiveAll(This->pSession, 
                                    (void*)(This->pInDataSlab),
                                    1))) {

        if (CS_FAIL(CSJSON_Parse(This->pJsonIn, CSWSCK_GetDataRef(This->pSession), 0))) {

          return CS_FAILURE | CSAP_HANDSHAKE | CSAP_FORMAT;
        }

        if (CS_FAIL(CSJSON_LookupKey
                                 (This->pJsonIn,
                                  "/handshake", "status",
                                  &(This->lse)))) {

          return CS_FAILURE | CSAP_HANDSHAKE | CSAP_STATUS;
        }

        strcpy(status->szStatus, This->lse.szValue);

        CSJSON_LookupKey(This->pJsonIn,
                           "/handshake", "reason",
                           &(This->lse));

        strcpy(status->szReason, This->lse.szValue);

        CSJSON_LookupKey(This->pJsonIn,
                           "/handshake", "sid",
                           &(This->lse));

        strcpy(status->szSessionID, This->lse.szValue);
        strcpy(This->szSessionID, This->lse.szValue);

        if (!strcmp(status->szStatus, "000")) {

          return CS_SUCCESS;
        }
        else {

          strcpy(status->szStatus, "850");
          strcpy(status->szReason, "0000000000");
          strcpy(status->szSessionID, CSAP_NULL_SESSION);

          return CS_FAILURE | CSAP_HANDSHAKE | CSAP_PROTOCOL;
        }
      }
      else {
        strcpy(status->szStatus, "802");
        strcpy(status->szReason, "0000000000");
        strcpy(status->szSessionID, CSAP_NULL_SESSION);

        return CS_FAILURE | CSAP_RECEIVE | CSAP_TRANSPORT;
      }
    }
    else {

      strcpy(status->szStatus, "803");
      strcpy(status->szReason, "0000000000");
      strcpy(status->szSessionID, CSAP_NULL_SESSION);

      return CS_FAILURE | CSAP_SEND | CSAP_TRANSPORT;
    }
  }

  strcpy(status->szStatus, "804");
  strcpy(status->szReason, "0000000000");
  strcpy(status->szSessionID, CSAP_NULL_SESSION);

  return CS_FAILURE | CSAP_OPEN | CSAP_CONNECT;
}

CSRESULT
  CSAP_Put
    (CSAP* This,
     char* pData,
     long Size) {

  if (pData == NULL) {
    return CS_FAILURE;
  }

  if (Size < 1) {
    return CS_FAILURE;
  }

  CSLIST_Insert(This->OutDataParts, (void*)pData, Size, CSLIST_BOTTOM);

  This->outDataSize += Size;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Receive
    (CSAP* This,
     CSAPCTL* pCtlFrame,
     long toSlices) {

  uint64_t Size;

  if (CS_FAIL(CSWSCK_ReceiveAll(This->pSession, 
                                &Size, 
                                toSlices))) {

    pCtlFrame->UsrCtlSize = 0;
    pCtlFrame->DataSize = 0;
    This->pUsrCtlSlab[0] = 0;
    return CS_FAILURE;
  }

  if (CS_FAIL(CSJSON_Parse(This->pJsonIn, CSWSCK_GetDataRef(This->pSession), 0))) {

    pCtlFrame->UsrCtlSize = 0;
    pCtlFrame->DataSize = 0;
    This->pUsrCtlSlab[0] = 0;
    return CS_FAILURE;
  }

  if (CS_FAIL(CSJSON_LookupKey(This->pJsonIn, "/ctl", "usrCtlSize", &(This->lse)))) {

    pCtlFrame->UsrCtlSize = 0;
    pCtlFrame->DataSize = 0;
    This->pUsrCtlSlab[0] = 0;
    return CS_FAILURE;
  }

  pCtlFrame->UsrCtlSize = strtol(This->lse.szValue, 0, 10);

  if (CS_FAIL(CSJSON_LookupKey(This->pJsonIn, "/ctl", "dataSize", &(This->lse)))) {

    pCtlFrame->UsrCtlSize = 0;
    pCtlFrame->DataSize = 0;
    This->pUsrCtlSlab[0] = 0;
    return CS_FAILURE;
  }

  pCtlFrame->DataSize = strtol(This->lse.szValue, 0, 10);

  if (pCtlFrame->UsrCtlSize > 0) {

    if (CS_FAIL(CSWSCK_ReceiveAll(This->pSession, 
                                  &Size, 
                                  toSlices))) {

      pCtlFrame->UsrCtlSize = 0;
      pCtlFrame->DataSize = 0;
      This->pUsrCtlSlab[0] = 0;
      return CS_FAILURE;
    }

    if (Size == pCtlFrame->UsrCtlSize) {

      if (This->UsrCtlSlabSize < pCtlFrame->UsrCtlSize) {
        free(This->pUsrCtlSlab);
        This->pUsrCtlSlab = (char*)malloc((pCtlFrame->UsrCtlSize + 1) * sizeof(char));       
        This->UsrCtlSlabSize = pCtlFrame->UsrCtlSize;  
      }

      CSWSCK_GetData(This->pSession, This->pUsrCtlSlab, 0, (uint64_t)(pCtlFrame->UsrCtlSize));
      This->pUsrCtlSlab[pCtlFrame->UsrCtlSize] = 0;
    }
    else {

      pCtlFrame->UsrCtlSize = 0;
      pCtlFrame->DataSize = 0;
      This->pUsrCtlSlab[0] = 0;
      return CS_FAILURE;
    }
  }
  else {
    This->pUsrCtlSlab[0] = 0;
  }

  if (pCtlFrame->DataSize > 0) {

    if (CS_FAIL(CSWSCK_ReceiveAll(This->pSession, 
                                  &Size, 
                                  toSlices))) {

      pCtlFrame->UsrCtlSize = 0;
      pCtlFrame->DataSize = 0;
      This->pUsrCtlSlab[0] = 0;
      return CS_FAILURE;
    }

    if (Size != pCtlFrame->DataSize) {

      pCtlFrame->UsrCtlSize = 0;
      pCtlFrame->DataSize = 0;
      This->pUsrCtlSlab[0] = 0;
      return CS_FAILURE;
    }
  } 

  This->ctl.UsrCtlSize = pCtlFrame->UsrCtlSize;
  This->ctl.DataSize = pCtlFrame->DataSize;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Send
    (CSAP* This,
     char* szUsrCtlFrame,
     long iUsrCtlSize) {

  char szSize[11];

  char* lpszFrame;

  long i;
  long Count;
  long Offset;
  long OutSize;
  long DataSize;

  // This resets the internal buffer, in case we call Send after this call

  if (This->outDataSlabSize < This->outDataSize) {
    free(This->pOutDataSlab);
    This->outDataSlabSize = This->outDataSize;
    This->pOutDataSlab = 
              (char*)malloc((This->outDataSlabSize+1) * sizeof(char));
  }

  Count = CSLIST_Count(This->OutDataParts);

  if (Count > 0) {

    for (Offset=0, i=0; i<Count; i++, Offset += DataSize) {

      DataSize = CSLIST_ItemSize(This->OutDataParts, i);
      CSLIST_Get(This->OutDataParts, 
                 (void*)(This->pOutDataSlab + Offset), i);
    }
  }

  /////////////////////////////////////////////////////////////
  // Send control frame
  /////////////////////////////////////////////////////////////

  CSJSON_Init(This->pJsonOut, JSON_TYPE_OBJECT);
  CSJSON_MkDir(This->pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
  sprintf(szSize, "%ld", This->outDataSize);
  CSJSON_InsertNumeric(This->pJsonOut, "/ctl", "dataSize", szSize);
  sprintf(szSize, "%ld", iUsrCtlSize);
  CSJSON_InsertNumeric(This->pJsonOut, "/ctl", "usrCtlSize", szSize);

  OutSize = CSJSON_Serialize(This->pJsonOut, "/", &lpszFrame, 0);

  CSWSCK_Send(This->pSession,
              CSWSCK_OP_TEXT,
              lpszFrame,
              (uint64_t)OutSize,
              CSWSCK_FIN_ON);

  /////////////////////////////////////////////////////////////
  // Send user control frame if any
  /////////////////////////////////////////////////////////////

  if (szUsrCtlFrame != NULL && iUsrCtlSize > 0) {

    CSWSCK_Send(This->pSession,
                CSWSCK_OP_TEXT,
                szUsrCtlFrame,
                (uint64_t)iUsrCtlSize,
                CSWSCK_FIN_ON);
  }

  /////////////////////////////////////////////////////////////
  // Send data
  /////////////////////////////////////////////////////////////

  CSWSCK_Send(This->pSession,
              CSWSCK_OP_TEXT,
              This->pOutDataSlab,
              (uint64_t)This->outDataSize,
              CSWSCK_FIN_ON);
  
  CSLIST_Clear(This->OutDataParts);

  This->outDataSize = 0;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Stream
    (CSAP* This,
     char* pData,
     long Size) {

  char szDataSize[11];

  char* lpszFrame;

  long OutSize;
   
  if (pData == NULL) {
    return CS_FAILURE;
  }

  if (Size < 0) {
    return CS_FAILURE;
  }

  /////////////////////////////////////////////////////////////
  // Send control frame
  /////////////////////////////////////////////////////////////

  CSJSON_Init(This->pJsonOut, JSON_TYPE_OBJECT);
  CSJSON_MkDir(This->pJsonOut, "/", "ctl", JSON_TYPE_OBJECT);
  sprintf(szDataSize, "%ld", Size);
  CSJSON_InsertNumeric(This->pJsonOut, "/ctl", "dataSize", szDataSize);
  CSJSON_InsertNumeric(This->pJsonOut, "/ctl", "usrCtlSize", "0");

  OutSize = CSJSON_Serialize(This->pJsonOut, "/", &lpszFrame, 0);

  CSWSCK_Send(This->pSession,
              CSWSCK_OP_TEXT,
              lpszFrame,
              (uint64_t)OutSize,
              CSWSCK_FIN_ON);

  /////////////////////////////////////////////////////////////
  // Send data
  /////////////////////////////////////////////////////////////

  CSWSCK_Send(This->pSession,
              CSWSCK_OP_TEXT,
              pData,
              (uint64_t)Size,
              CSWSCK_FIN_ON);

  return CS_SUCCESS;
}
