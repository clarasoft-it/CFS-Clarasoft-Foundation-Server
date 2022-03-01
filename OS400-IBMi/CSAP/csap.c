/* ==========================================================================

  Clarasoft Foundation Server

  Common Service Access Protocol
  Version 7.0.0

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

#include "qcsrc_cfs/cfsrepo.h"
#include "qcsrc_cfs/cswsck.h"
#include "qcsrc_cfs/cslib.h"
#include "qcsrc_cfs/csjson.h"

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

#define CSAP_FMT_DEFAULT     (0x00010000)
#define CSAP_FMT_TEXT        (0x00010000)
#define CSAP_FMT_BINARY      (0x00020000)

#define CSAP_RCVMODE_CACHE   (0x00000001)
#define CSAP_RCVMODE_STREAM  (0x00000002)
#define CSAP_RCVMODE_DEFAULT (0x00000002)

#define CSAP_USRCTLSLABSIZE  (1024LL)
#define CSAP_MAX_SEGMENTSIZE (2097152L)

typedef struct tagCSAPSTAT {

  char szVersion[11];
  char szStatus[4];
  char szReason[11];
  char szSessionID[37];

} CSAPSTAT;

typedef union tagCSAP_CTL {

    char CTLFRAMEBUFFER[80];

    struct {
      char padding_1[4];
      char version[10];
      char sid[36];
      char format[10];
      char userCtlSize[10];
      char dataSize[10];

    } CTLFRAME;

} CSAP_CTL;

typedef struct tagCSAPCTL {

  long UsrCtlSize;
  long DataSize;
  long fmt;

} CSAPCTL;

typedef struct tagCSAP {

  CSWSCK pSession;

  CSJSON pJsonIn;
  CSJSON pJsonOut;

  CSJSON_LSENTRY lse;

  char szSessionID[37];

  long OutDataSize;
  long InDataSize;
  long UsrCtlSize;
  long UsrCtlSlabSize;

  long fmt;
  long OutDataSlabSize;

  char* pUsrCtlSlab;
  char* pInData;
  char* pUsrCtl;
  char* pOutData;

  CSLIST OutDataParts;

  CFSRPS repo;
  CSAP_CTL* pCtl;

} CSAP;

CSAP*
  CSAP_Constructor
    (void) {

  CSAP* Instance;

  Instance = (CSAP*)malloc(sizeof(CSAP));

  Instance->pSession = CSWSCK_Constructor();

  Instance->OutDataSize = 0;
  Instance->InDataSize = 0;
  Instance->UsrCtlSize = 0;
  Instance->UsrCtlSlabSize = CSAP_USRCTLSLABSIZE;

  Instance->pUsrCtlSlab =
      (char*)malloc((Instance->UsrCtlSlabSize + 1) * sizeof(char));

  Instance->OutDataParts = CSLIST_Constructor();

  Instance->repo = CFSRPS_Constructor();

  Instance->pJsonIn = CSJSON_Constructor();
  Instance->pJsonOut = CSJSON_Constructor();

  Instance->OutDataSlabSize = 65535;
  Instance->pOutData = (char*)malloc
                ((Instance->OutDataSlabSize + 1) * sizeof(char));

  return Instance;
}

CSRESULT
  CSAP_Destructor
    (CSAP** This) {

  CSLIST_Destructor(&((*This)->OutDataParts));
  CFSRPS_Destructor(&((*This)->repo));
  CSWSCK_Destructor(&((*This)->pSession));
  CSJSON_Destructor(&((*This)->pJsonIn));
  CSJSON_Destructor(&((*This)->pJsonOut));

  free((*This)->pUsrCtlSlab);
  free((*This)->pOutData);

  free(*This);

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Clear
    (CSAP* This) {

  This->OutDataSize = 0;
  This->InDataSize = 0;
  This->UsrCtlSize = 0;

  CSLIST_Clear(This->OutDataParts);

  return CS_SUCCESS;
}

CSRESULT
  CSAP_CloseService
    (CSAP* This)
{
  CSWSCK_CloseSession(This->pSession, 0, 0, -1);
  return CS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//
// Caller must provide a buffer large enough to receive data.
//
////////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Get
    (CSAP* This,
     char* pData,
     long Offset,
     long Size) {

  if (This->pInData == NULL) {
    return CS_FAILURE | CSAP_GETDATA | CSAP_NULLBUFFER;
  }

  if (Offset >= This->InDataSize) {
    return CS_FAILURE | CSAP_GETDATA | CSAP_OVERFLOW;
  }

  if (Size > (This->InDataSize - Offset)) {
    memcpy(pData, This->pInData + Offset,
                  This->InDataSize - Offset);
    return CS_SUCCESS | CSAP_GETDATA | CSAP_OVERFLOW;
  }

  memcpy(pData, This->pInData + Offset, Size);
  return CS_SUCCESS;
}

void*
  CSAP_GetDataRef
    (CSAP* This) {

  return This->pInData;
}

CSRESULT
  CSAP_GetUserCtl
    (CSAP* This,
     char* pData) {

  if (This->UsrCtlSize > 0) {
    memcpy(pData, This->pUsrCtlSlab, This->UsrCtlSize);
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

void*
  CSAP_GetUserCtlRef
    (CSAP* This) {

  return (void*)This->pUsrCtlSlab;
}

CSRESULT
  CSAP_OpenService
    (CSAP* This,
     char* szService,
     CSAPSTAT* status) {

  int e;

  uint64_t size;

  char szBuffer[1049];

  char szTemplate[] =
         "{\"op\":\"open\",\"service\":\"%s\"}";

  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO pi;

  char* pHandshake;

  // Reset outbound data, in case a send is
  // performed immidiately after the open. A receive
  // will properly reset the CSAP instance.

  This->OutDataSize = 0;

  CSLIST_Clear(This->OutDataParts);

  strncpy(ci.szPath, szService, CFSRPS_PATH_MAXBUFF);

  if (CS_SUCCEED(CFSRPS_LoadConfig(This->repo, &ci))) {

    if (CS_SUCCEED(CFSRPS_LookupParam(This->repo, "SERVICE", &pi))) {
      sprintf(szBuffer, szTemplate, pi.szValue);
    }
    else {
      return CS_FAILURE | CSAP_OPEN | CSAP_CONFIG;
    }
  }
  else {
    return CS_FAILURE | CSAP_OPEN | CSAP_CONFIG;
  }

  if (CS_SUCCEED(CSWSCK_OpenSession(This->pSession,
                            0, szService, 0, 0, &e))) {

    if (CS_SUCCEED(CSWSCK_Send(This->pSession,
                               CSWSCK_OPER_TEXT,
                               szBuffer,
                               strlen(szBuffer), CSWSCK_FIN_ON, -1))) {
      // read response
      if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &size, -1))) {

        pHandshake = (char*)CSWSCK_GetDataRef(This->pSession);

        if (CS_FAIL(CSJSON_Parse(This->pJsonIn, pHandshake, 0))) {

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

  long numSegments;
  long segmentSize;
  long offset;
  long blockSize;
  long i;

  if (This->OutDataSize + Size > CSAP_MAX_SEGMENTSIZE) {
    return CS_FAILURE;
  }

  CSLIST_Insert(This->OutDataParts, (void*)pData, Size, CSLIST_BOTTOM);

  This->OutDataSize += Size;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Receive
    (CSAP* This,
     CSAPCTL* pCtlFrame) {

  uint64_t Size;

  char szNumBuffer[21];
  char* pCtl;

  char szUsrCtlSize[11];
  char szDataSize[11];
  char szFormat[11];

  long Count;
  long i;

  uint64_t dataSize;

  // Read control frame

  if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &Size, -1))) {

    // we must at least have the CTL frame
    if (Size >= 80) {

      This->pCtl = (CSAP_CTL*)CSWSCK_GetDataRef(This->pSession);

      //
      //  CSAP (4)
      //  Version (10)
      //  Session ID (36)
      //  Format (10)
      //  User Ctl Size (10)
      //  Data Size (10)
      //

      memcpy(szUsrCtlSize,
             This->pCtl->CTLFRAME.userCtlSize, 10);

      szUsrCtlSize[10] = 0;

      This->UsrCtlSize = pCtlFrame->UsrCtlSize
                         = strtol(szUsrCtlSize, 0, 10);

      memcpy(szDataSize,
             This->pCtl->CTLFRAME.dataSize, 10);

      szDataSize[10] = 0;

      This->InDataSize = pCtlFrame->DataSize
                       = strtol(szDataSize, 0, 10);

      memcpy(szFormat,
             This->pCtl->CTLFRAME.format, 10);

      szFormat[10] = 0;

      if (!strcmp("TEXT      ", szFormat)) {
        This->fmt = pCtlFrame->fmt = CSAP_FMT_TEXT;
      }
      else {
        This->fmt = pCtlFrame->fmt = CSAP_FMT_BINARY;
      }

      // Read User Control frame if any

      if (This->UsrCtlSize > 0) {

        if (This->UsrCtlSize > This->UsrCtlSlabSize) {
          free(This->pUsrCtlSlab);
          This->pUsrCtlSlab = (char*)malloc((This->UsrCtlSize + 1) * sizeof(char));
          This->UsrCtlSlabSize = This->UsrCtlSize;
        }

        memcpy(This->pUsrCtlSlab, (char*)This->pCtl + 80, This->UsrCtlSize);
        This->pUsrCtlSlab[This->UsrCtlSize] = 0;
      }

      if (This->InDataSize > 0) {
        This->pInData = (char*)((char*)(This->pCtl) + 80 + This->UsrCtlSize);

      }

      return CS_SUCCESS;
    }
    else {
      This->pInData = NULL;
      This->UsrCtlSize = pCtlFrame->UsrCtlSize = 0;
      This->InDataSize = pCtlFrame->DataSize = 0;
    }
  }
  else {
    This->pInData = NULL;
    This->UsrCtlSize = pCtlFrame->UsrCtlSize = 0;
    This->InDataSize = pCtlFrame->DataSize = 0;
  }

  return CS_FAILURE;
}

CSRESULT
  CSAP_Send
    (CSAP* This,
     char* szSessionID,
     char* pUsrCtl,
     long Size,
     char Fmt) {

  long i;
  long Count;
  long fmtIndex;
  long Offset;
  long OutSize;
  long DataSize;
  long len;

  char szCtl[512];
  char szUsrCtlSize[21];
  char szDataSize[21];
  char szNumFragments[21];
  char szMaxFragmentSize[21];
  char szSessionIDBuffer[37];

  char* pData;

  OutSize = ((80 + Size + This->OutDataSize) * sizeof(char));

  if (This->OutDataSlabSize < OutSize) {
    free(This->pOutData);
    This->OutDataSlabSize = OutSize;
    This->pOutData = (char*)malloc(This->OutDataSlabSize);
  }

  memset(This->pOutData, ' ', 80);
  memcpy(This->pOutData, "CSAP", 4);
  memcpy(This->pOutData + 4, "0700000000", 10);

  if (szSessionID != 0) {
    len = strlen(szSessionID);
    if (len > 36) {
      memcpy(This->pOutData + 14, szSessionID, 36);
    }
    else {
      memcpy(This->pOutData + 14, szSessionID, len);
    }
  }
  else {
    memcpy(This->pOutData + 14, This->szSessionID, 36);
  }

  switch(Fmt) {

     case CSAP_FMT_BINARY:
       memcpy(This->pOutData + 50, "BINARY", 6);
       break;
     default:
       memcpy(This->pOutData + 50, "TEXT", 4);
       break;
  }

  if (pUsrCtl != 0 && Size > 0) {
    sprintf(This->pOutData + 60, "%010ld", Size);
  }
  else {
    memcpy(This->pOutData + 60, "0000000000", 10);
  }

  sprintf(This->pOutData + 70, "%010ld", This->OutDataSize);

  if (pUsrCtl != 0 && Size > 0) {
    memcpy(This->pOutData + 80, pUsrCtl, Size);
  }

  Count = CSLIST_Count(This->OutDataParts);

  for (Offset= (80 + Size), i=0; i<Count; i++) {

    DataSize = CSLIST_ItemSize(This->OutDataParts, i);

    CSLIST_GetDataRef(This->OutDataParts, (void**)&pData, i);
    memcpy(This->pOutData + Offset, pData, DataSize);
    Offset += DataSize;
  }

  CSWSCK_Send(This->pSession,
              CSWSCK_OPER_TEXT,
              This->pOutData,
              (uint64_t)OutSize,
              CSWSCK_FIN_ON,
              -1);

  CSLIST_Clear(This->OutDataParts);

  This->OutDataSize = 0;

  return CS_SUCCESS;
}

