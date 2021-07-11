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

#ifndef __CLARASOFT_CFSAPI_CSAP_H__
#define __CLARASOFT_CFSAPI_CSAP_H__

#include "qcsrc/cswsck.h"

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
#define CSAP_NULLBUFFER     0x00000009

#define CSAP_FORMAT_DEFAULT  '1'
#define CSAP_FORMAT_TEXT     '1'
#define CSAP_FORMAT_BINARY   '2'

typedef struct CSAP* CSAP;

typedef struct tagCSAPSTAT {

  char szStatus[11];
  char szSessionID[37];
  char szVersion[11];

} CSAPSTAT;

typedef struct tagCSAPCTL {

  uint64_t UsrCtlSize;
  uint64_t DataSize;
  uint64_t NumFragments;
  uint64_t MaxFragmentSize;

  char fmt;
  char fin;

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
  CSAP_CloseChannel
    (CSAP This);

CSRESULT
  CSAP_CloseService
    (CSAP This);

CSRESULT
  CSAP_Get
    (CSAP This,
     char* pData,
     long* Size);

CSRESULT
  CSAP_GetDataRef
    (CSAP This,
     char** pData,
     long* Size);

CSRESULT
  CSAP_GetUserCtl
    (CSAP  This,
     char* pData);

CSRESULT
  CSAP_GetUserCtlRef
    (CSAP  This,
     char** pData);

CSRESULT
  CSAP_OpenChannel
    (CSAP This,
     CSWSCK pSession,
     char* szSessionID);

CSRESULT
  CSAP_OpenService
    (CSAP This,
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
     CSAPCTL* pCtlFrame);

CSRESULT
  CSAP_Send
    (CSAP This,
     char* pUsrCtl,
     long Size,
     char Fmt,
     char finState);

CSRESULT
  CSAP_Stream
    (CSAP This,
     char* pData,
     long Size,
     long Fmt);

#endif


 
