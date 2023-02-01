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

#ifndef __CLARASOFT_CFS_CSAP_H__
#define __CLARASOFT_CFS_CSAP_H__

#include <inttypes.h>
#include <clarasoft/cfs.h>


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

typedef struct CSAP* CSAP;

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

CSAP
  CSAP_Constructor
    (void);

CSRESULT
  CSAP_Destructor
    (CSAP* This);

CSRESULT
  CSAP_Clear
    (CSAP This);

CSRESULT
  CSAP_CloseService
    (CSAP This);

CSRESULT
  CSAP_Get
    (CSAP This,
     char* pData,
     long Offset,
     long Size);

void*
  CSAP_GetDataRef
    (CSAP This);

CSRESULT
  CSAP_GetUserCtl
    (CSAP This,
     char* pData);

char*
  CSAP_GetUserCtlRef
    (CSAP This);

CSRESULT
  CSAP_OpenService
    (CSAP This,
     CFSENV pEnv, 
     char* szService,
     CSAPSTAT* status);

CSRESULT
  CSAP_Put
    (CSAP This,
     char* pData,
     long Size);

CSRESULT
  CSAP_Receive
    (CSAP This,
     CSAPCTL* pCtlFrame,
     long toSlices);

CSRESULT
  CSAP_Send
    (CSAP This,
     char* szUsrCtlFrame,
     long iUsrCtlSize);

CSRESULT
  CSAP_Stream
    (CSAP This,
     char* pData,
     long Size);

#endif
 
