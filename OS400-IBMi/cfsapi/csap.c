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

#include "qcsrc/cswsck.h"

#define CSAP_NULL_SESSION         "000000000000000000000000000000000000"

#define CSAP_FIN_OFF        ('0')
#define CSAP_FIN_ON         ('1')

#define CSAP_GETDATA        0x00010000
#define CSAP_GETDATAREF     0x00020000
#define CSAP_USRCTL         0x00030000
#define CSAP_SEND           0x00040000
#define CSAP_RECEIVE        0x00050000
#define CSAP_SETDATA        0x00060000
#define CSAP_OPEN           0x00070000
#define CSAP_HANDSHAKE      0x00080000

#define CSAP_OVERFLOW       0x00000001
#define CSAP_TRANSPORT      0x00000002
#define CSAP_SIZE           0x00000003
#define CSAP_STATUS         0x00000005
#define CSAP_CONFIG         0x00000006
#define CSAP_PROTOCOL       0x00000007
#define CSAP_USERCTL        0x00000008

#define CSAP_FORMAT_DEFAULT  '1'
#define CSAP_FORMAT_TEXT     '1'
#define CSAP_FORMAT_BINARY   '2'

#define CSAP_RCVMODE_CACHE    0x00000001
#define CSAP_RCVMODE_STREAM   0x00000002
#define CSAP_RCVMODE_DEFAULT  0x00000002

#define CSAP_USRCTLSLABSIZE   (1024LL)

typedef struct tagCSAP_FRAGMENTINFO {

  char* pData;
  long dataSize;

} CSAP_FRAGMENTINFO;

typedef struct tagCSAPSTAT {

  char szStatus[4];
  char szReason[11];
  char szSessionID[37];

} CSAPSTAT;

typedef struct tagCSAPSTAT_T {

  char szStatus[3];
  char szReason[10];
  char szSessionID[36];

} CSAPSTAT_T;

typedef struct tagCSAPCTL {

  uint64_t UsrCtlSize;
  uint64_t DataSize;
  uint64_t NumFragments;
  uint64_t MaxFragmentSize;
  char fmt;
  char fin;

} CSAPCTL;

typedef struct tagCSAPCTL_T {

  char szUsrCtlSize[20];
  char szDataSize[20];
  char szNumFragments[20];
  char szMaxFragmentSize[20];
  char fmt;
  char fin;

} CSAPCTL_T;

typedef struct tagCSAP {

  CSWSCK pSession;

  char szSessionID[37];

  uint64_t OutDataSize;
  uint64_t InDataSize;
  uint64_t UsrCtlSize;
  uint64_t NumFragments;
  uint64_t MaxInFragmentSize;
  uint64_t MaxOutFragmentSize;
  uint64_t Offset;
  uint64_t UsrCtlSlabSize;

  char* pUsrCtlSlab;

  CSLIST InData;
  CSLIST OutData;

  CSAP_FRAGMENTINFO fragmentInfo;

} CSAP;

CSAP*
  CSAP_Constructor
    (void) {

  CSAP* Instance;

  Instance = (CSAP*)malloc(sizeof(CSAP));

  Instance->pSession = 0;

  Instance->OutDataSize = 0;
  Instance->InDataSize = 0;
  Instance->UsrCtlSize = 0;
  Instance->NumFragments = 0;
  Instance->MaxInFragmentSize = 0;
  Instance->MaxOutFragmentSize = 0;
  Instance->Offset = 0;
  Instance->UsrCtlSlabSize = CSAP_USRCTLSLABSIZE;

  Instance->pUsrCtlSlab =
      (char*)malloc(Instance->UsrCtlSlabSize * sizeof(char));

  Instance->InData = CSLIST_Constructor();
  Instance->OutData = CSLIST_Constructor();

   return Instance;
}

CSRESULT
  CSAP_Destructor
    (CSAP** This) {

  CSLIST_Destructor(&((*This)->InData));
  CSLIST_Destructor(&((*This)->OutData));
  free((*This)->pUsrCtlSlab);
  free(*This);

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Clear
    (CSAP* This) {

  uint64_t Size;

  // Clear all pending fragments: a RECEIVE may be issued
  // before all incoming fragments from a previous RECEIVE
  // have been read. We need to discard them before reading
  // the next wave of fragments.

  while (This->Offset < This->NumFragments) {

    if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &Size, -1))) {
      This->Offset++;
    }
    else {
      return CS_FAILURE;
    }
  }

  This->OutDataSize = 0;
  This->InDataSize = 0;
  This->UsrCtlSize = 0;
  This->NumFragments = 0;
  This->MaxInFragmentSize = 0;
  This->MaxOutFragmentSize = 0;
  This->Offset = 0;

  CSLIST_Clear(This->InData);
  CSLIST_Clear(This->OutData);

  return CS_SUCCESS;
}

CSRESULT
  CSAP_CloseChannel
    (CSAP* This)
{

  This->pSession = 0;
  return CS_SUCCESS;
}

CSRESULT
  CSAP_CloseService
    (CSAP* This)
{
  CSWSCK_CloseSession(This->pSession, 0, 0, -1);

  CSWSCK_Destructor(&(This->pSession));

  This->pSession = 0;
  return CS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//
// Caller must provide a buffer large enough to receive data. The control
// frame contains the size of the largest fragment so the caller can
// insure proper size. THe fragment will be read and will no longer be
// available for reading.
//
////////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Get
    (CSAP* This,
     char* pData,
     long* Size) {

  uint64_t dataSize;

  if (This->Offset < This->NumFragments) {

    if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &dataSize, -1))) {

      *Size = (long)dataSize;
      This->Offset++;

      if ((uint64_t)(*Size) < dataSize) {
        return CS_FAILURE | CSAP_GETDATA | CSAP_SIZE;
      }

      CSWSCK_GetData(This->pSession, pData, 0, *Size);

      return CS_SUCCESS;
    }
    else {
      return CS_FAILURE | CSAP_GETDATA | CSAP_TRANSPORT;
    }
  }

  return CS_FAILURE | CSAP_GETDATA | CSAP_OVERFLOW;
}

CSRESULT
  CSAP_GetDataRef
    (CSAP* This,
     char** pData,
     long* Size) {

  uint64_t dataSize;

  if (This->Offset < This->NumFragments) {

    if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &dataSize, -1))) {
      *pData = CSWSCK_GetDataRef(This->pSession);
      This->Offset++;
      *Size = (long)dataSize;
      return CS_SUCCESS;
    }
    else {
      return CS_FAILURE | CSAP_GETDATA | CSAP_TRANSPORT;
    }
  }

  return CS_FAILURE | CSAP_GETDATA | CSAP_OVERFLOW;
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

CSRESULT
  CSAP_GetUserCtlRef
    (CSAP* This,
     char** pData) {

  if (This->UsrCtlSize > 0) {
    *pData = This->pUsrCtlSlab;
    return CS_SUCCESS;
  }

  *pData = 0;
  return CS_FAILURE;
}

CSRESULT
  CSAP_OpenChannel
    (CSAP* This,
     CSWSCK pSession,
     char* szSessionID) {

  This->pSession = pSession;
  This->OutDataSize = 0;
  This->InDataSize = 0;
  This->UsrCtlSize = 0;
  This->NumFragments = 0;
  This->MaxInFragmentSize = 0;
  This->MaxOutFragmentSize = 0;
  This->Offset = 0;

  CSLIST_Clear(This->InData);
  CSLIST_Clear(This->OutData);

  strncpy(This->szSessionID, szSessionID, 37);
  This->szSessionID[36] = 0;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_OpenService
    (CSAP* This,
     char* szService,
     CSAPSTAT* status) {

  int e;

  uint64_t size;

  char szServicePath[] =
        "                    "
        "                    "
        "                    "
        "                    "
        "                    ";

  char szBuffer[99];

  CSAPSTAT_T* pHandshake;

  This->pSession = CSWSCK_Constructor();

  if (CS_SUCCEED(CSWSCK_OpenSession(This->pSession,
                            0, szService, 0, 0, &e))) {

    // Send handshake information
    size = 99;

    strncpy(szBuffer, szService, 99);
    szBuffer[98] = 0;

    memcpy(szServicePath, szBuffer, strlen(szBuffer));

    if (CS_SUCCEED(CSWSCK_Send(This->pSession,
                               CSWSCK_OPER_TEXT,
                               szServicePath,
                               size, CSWSCK_FIN_ON, -1))) {
      // read response
      if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &size, -1))) {

        if (size == sizeof(CSAPSTAT)) {

          pHandshake = (CSAPSTAT_T*)CSWSCK_GetDataRef(This->pSession);

          memcpy(status->szStatus, pHandshake, 3);
          status->szStatus[3] = 0;
          memcpy(status->szReason, pHandshake + 3, 10);
          status->szReason[10] = 0;
          memcpy(status->szSessionID, pHandshake + 13, 36);
          status->szSessionID[36] = 0;

          strncpy(This->szSessionID, status->szSessionID, 37);

          if (!strcmp(status->szStatus, "000")) {

            This->OutDataSize = 0;
            This->InDataSize = 0;
            This->UsrCtlSize = 0;
            This->NumFragments = 0;
            This->MaxInFragmentSize = 0;
            This->MaxOutFragmentSize = 0;
            This->Offset = 0;

            CSLIST_Clear(This->InData);
            CSLIST_Clear(This->OutData);

            return CS_SUCCESS;
          }
          else {
            return CS_FAILURE | CSAP_HANDSHAKE | CSAP_STATUS;
          }
        }
        else {
          strcpy(status->szStatus, "999");
          strcpy(status->szReason, "0000000000");
          strcpy(status->szSessionID, CSAP_NULL_SESSION);

          return CS_FAILURE | CSAP_HANDSHAKE | CSAP_PROTOCOL;
        }
      }
      else {
        strcpy(status->szStatus, "999");
        strcpy(status->szReason, "0000000000");
        strcpy(status->szSessionID, CSAP_NULL_SESSION);

        return CS_FAILURE | CSAP_RECEIVE | CSAP_TRANSPORT;
      }
    }
    else {

      strcpy(status->szStatus, "999");
      strcpy(status->szReason, "0000000000");
      strcpy(status->szSessionID, CSAP_NULL_SESSION);

      return CS_FAILURE | CSAP_SEND | CSAP_TRANSPORT;
    }
  }

  strcpy(status->szStatus, "999");
  strcpy(status->szReason, "0000000000");
  strcpy(status->szSessionID, CSAP_NULL_SESSION);

  return CS_FAILURE | CSAP_OPEN | CSAP_CONFIG;
}

CSRESULT
  CSAP_Put
    (CSAP* This,
     char* pData,
     long Size) {

  CSLIST_Insert(This->OutData, (void*)pData, Size, CSLIST_BOTTOM);

  if (Size > This->MaxOutFragmentSize) {
    This->MaxOutFragmentSize = Size;
  }

  This->OutDataSize += Size;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Receive
    (CSAP* This,
     CSAPCTL* pCtlFrame) {

  uint64_t Size;

  char szNumBuffer[21];

  long Count;
  long i;

  CSAPCTL_T* pCtlFrame_t;

  // Clear all pending fragments: a RECEIVE may be issued
  // before all incoming fragments from a previous RECEIVE
  // have been read. We need to discard them before reading
  // the next wave of fragments.

  while(This->Offset < This->NumFragments) {

    if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &Size, -1))) {
      This->Offset++;
    }
    else {
      return CS_FAILURE;
    }
  }

  // Reset offset
  This->Offset = 0;

  // Read control frame

  if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &Size, -1))) {

    if (Size == sizeof(CSAPCTL_T)) {

      pCtlFrame_t = (CSAPCTL_T*)CSWSCK_GetDataRef(This->pSession);

      memcpy(szNumBuffer, pCtlFrame_t->szUsrCtlSize, 20);
      szNumBuffer[20] = 0;
      This->UsrCtlSize = pCtlFrame->UsrCtlSize
                       = strtoull(szNumBuffer, 0, 10);

      memcpy(szNumBuffer, pCtlFrame_t->szDataSize, 20);
      szNumBuffer[20] = 0;
      This->InDataSize = pCtlFrame->DataSize
                       = strtoull(szNumBuffer, 0, 10);

      memcpy(szNumBuffer, pCtlFrame_t->szNumFragments, 20);
      szNumBuffer[20] = 0;
      This->NumFragments = pCtlFrame->NumFragments
                         = strtoull(szNumBuffer, 0, 10);

      memcpy(szNumBuffer, pCtlFrame_t->szMaxFragmentSize, 20);
      szNumBuffer[20] = 0;
      This->MaxInFragmentSize = pCtlFrame->MaxFragmentSize
                            = strtoull(szNumBuffer, 0, 10);

      pCtlFrame->fin = pCtlFrame_t->fin;

      // Read User Control frame if any

      if (This->UsrCtlSize > 0) {

        if (This->UsrCtlSize > This->UsrCtlSlabSize) {
          free(This->pUsrCtlSlab);
          This->pUsrCtlSlab = (char*)malloc(This->UsrCtlSize * sizeof(char));
          This->UsrCtlSlabSize = This->UsrCtlSize;
        }

        if (CS_SUCCEED(CSWSCK_Receive(This->pSession, &Size, -1))) {
          if (Size != This->UsrCtlSize) {
            return CS_FAILURE | CSAP_RECEIVE | CSAP_USERCTL;
          }

          CSWSCK_GetData(This->pSession,
                         This->pUsrCtlSlab, 0, This->UsrCtlSize);
        }
        else {
          return CS_FAILURE | CSAP_RECEIVE | CSAP_USERCTL;
        }
      }

      return CS_SUCCESS;
    }
    else {
      This->UsrCtlSize = pCtlFrame->UsrCtlSize = 0;
      This->InDataSize = pCtlFrame->DataSize = 0;
      This->NumFragments = pCtlFrame->NumFragments = 0;
      This->MaxInFragmentSize = pCtlFrame->MaxFragmentSize = 0;
    }
  }
  else {
    This->UsrCtlSize = pCtlFrame->UsrCtlSize = 0;
    This->InDataSize = pCtlFrame->DataSize = 0;
    This->NumFragments = pCtlFrame->NumFragments = 0;
    This->MaxInFragmentSize = pCtlFrame->MaxFragmentSize = 0;
  }

  return CS_FAILURE;
}

CSRESULT
  CSAP_Send
    (CSAP* This,
     char* pUsrCtl,
     long Size,
     char Fmt,
     char finState) {

  long i;
  long Count;
  long oper;

  char szNumBuffer[21];

  char* pData;

  uint64_t DataSize;

  CSAPCTL_T ctlFrame_t;

  switch(Fmt) {

     case CSAP_FORMAT_TEXT:
       ctlFrame_t.fmt = CSAP_FORMAT_TEXT;
       oper = CSWSCK_OPER_TEXT;
       break;
     case CSAP_FORMAT_BINARY:
       ctlFrame_t.fmt = CSAP_FORMAT_BINARY;
       oper = CSWSCK_OPER_BINARY;
       break;
     default:
       ctlFrame_t.fmt = CSAP_FORMAT_BINARY;
       oper = CSWSCK_OPER_BINARY;
       break;
  }

  sprintf(szNumBuffer, "%020llu", This->OutDataSize);
  memcpy(ctlFrame_t.szDataSize, szNumBuffer, 20);

  sprintf(szNumBuffer, "%020llu", This->MaxOutFragmentSize);
  memcpy(ctlFrame_t.szMaxFragmentSize, szNumBuffer, 20);

  Count = CSLIST_Count(This->OutData);

  sprintf(szNumBuffer, "%020llu", (uint64_t)Count);
  memcpy(ctlFrame_t.szNumFragments, szNumBuffer, 20);

  if (pUsrCtl != 0 && Size > 0) {
    sprintf(szNumBuffer, "%020llu", (uint64_t)Size);
    memcpy(ctlFrame_t.szUsrCtlSize, szNumBuffer, 20);
  }
  else {
    memcpy(ctlFrame_t.szUsrCtlSize, "00000000000000000000", 20);
  }

  ctlFrame_t.fin = finState;

  CSWSCK_Send(This->pSession,
              CSWSCK_OPER_TEXT,
              (char*)(&ctlFrame_t),
              sizeof(CSAPCTL_T),
              CSWSCK_FIN_ON,
              -1);

  if (pUsrCtl != 0) {

    CSWSCK_Send(This->pSession,
                CSWSCK_OPER_TEXT,
                pUsrCtl,
                Size,
                CSWSCK_FIN_ON,
                -1);
  }

  for (i=0; i<Count; i++) {

    DataSize = CSLIST_ItemSize(This->OutData, i);

    CSLIST_GetDataRef(This->OutData, (void**)&pData, i);

    CSWSCK_Send(This->pSession,
                oper,
                pData,
                DataSize,
                CSWSCK_FIN_ON,
                -1);
  }

  CSLIST_Clear(This->OutData);
  This->OutDataSize = 0;
  This->MaxOutFragmentSize = 0;

  return CS_SUCCESS;
}

CSRESULT
  CSAP_Stream
    (CSAP This,
     char* pData,
     long Size,
     long Fmt) {

  return CS_SUCCESS;
}


 
