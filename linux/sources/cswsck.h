/* ==========================================================================

  Clarasoft Foundation Server - Linux
  cswsck.h

  Distributed under the MIT license

  Copyright (c) 2017 Clarasoft I.T. Solutions Inc.

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
========================================================================== */

#ifndef __CLARASOFT_CFS_CSWSCK_H__
#define __CLARASOFT_CFS_CSWSCK_H__

#include <clarasoft/cfsapi.h>

#define CSWSCK_MASK_OPERATION        (0x0FFF0000)

#define CSWSCK_OP_CONTINUATION       (0x00)
#define CSWSCK_OP_TEXT               (0x01)
#define CSWSCK_OP_BINARY             (0x02)
#define CSWSCK_OP_CLOSE              (0x08)
#define CSWSCK_OP_PING               (0x09)
#define CSWSCK_OP_PONG               (0x0A)

#define CSWSCK_OPER_CONTINUATION     (0x00000000)
#define CSWSCK_OPER_TEXT             (0x00010000)
#define CSWSCK_OPER_BINARY           (0x00020000)
#define CSWSCK_OPER_CLOSE            (0x00080000)
#define CSWSCK_OPER_PING             (0x00090000)
#define CSWSCK_OPER_PONG             (0x000A0000)
#define CSWSCK_OPER_CFSAPI           (0x0F010000)
#define CSWSCK_OPER_OPENCHANNEL      (0x0F020000)

#define CSWSCK_FIN_OFF               (0x00)
#define CSWSCK_FIN_ON                (0x01)

#define CSWSCK_E_NODATA              (0x00000001)
#define CSWSCK_E_PARTIALDATA         (0x00000002)
#define CSWSCK_E_ALLDATA             (0x00000003)

#define CSWSCK_MOREDATA              (0x00000002)
#define CSWSCK_ALLDATA               (0x00000003)

#define CSWSCK_SR_CACHE              (0x00000001)
#define CSWSCK_SR_STREAM             (0x00000002)

#define CSWSCK_DIAG_WEBSOCKET        (0x00000001)
#define CSWSCK_DIAG_HTTP             (0x00000002)
#define CSWSCK_DIAG_UNKNOWNPROTO     (0x00008001)
#define CSWSCK_DIAG_NOTSUPPORTED     (0x00008002)
#define CSWSCK_DIAG_NOPROTOCOL       (0x00008003)

#define CSWSCK_OPERATION(x)          ((x) & CSWSCK_MASK_OPERATION)

typedef void* CSWSCK;

CSWSCK
  CSWSCK_Constructor
    (void);

CSRESULT
  CSWSCK_Destructor
    (CSWSCK* This);

CSRESULT
  CSWSCK_CloseChannel
    (CSWSCK,
     char*,
     long);

CSRESULT
  CSWSCK_CloseSession
    (CSWSCK,
     char*,
     uint64_t);

CSRESULT
  CSWSCK_GetData
    (CSWSCK,
     char*,
     uint64_t,
     uint64_t);

void*
  CSWSCK_GetDataRef
    (CSWSCK);

CSRESULT
  CSWSCK_OpenChannel
    (CSWSCK This,
     CFSENV pEnv,
     int connfd);

CSRESULT
  CSWSCK_OpenSession
    (CSWSCK This,
     CFSENV pEnv,
     char* szConfig,
     char* szHost,
     char* szPort);

CSRESULT
  CSWSCK_Ping
    (CSWSCK,
     char*,
     uint64_t);

CSRESULT
  CSWSCK_Receive
    (CSWSCK* This,
     uint64_t* iDataSize,
     long toSlices);

CSRESULT
  CSWSCK_ReceiveAll
    (CSWSCK* This,
     uint64_t* iDataSize,
     long toSlices);

CSRESULT
  CSWSCK_Send
    (CSWSCK*  This,
     char     operation,
     char*    data,
     uint64_t iDataSize,
     char     fin);

#endif