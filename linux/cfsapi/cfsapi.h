/* ===========================================================================
  Clarasoft Foundation Server 400
  cfsapi.h

  Distributed under the MIT license

  Copyright (c) 2013 Clarasoft I.T. Solutions Inc.

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

#ifndef __CLARASOFT_CFS_CFSAPI_H__
#define __CLARASOFT_CFS_CFSAPI_H__

#include <inttypes.h>
#include <clarasoft/cslib.h>

//////////////////////////////////////////////////////////////////////////////
// Clarasoft Foundation Server Definitions
//////////////////////////////////////////////////////////////////////////////

#define CFS_NTOP_ADDR_MAX             (1025)
#define CFS_NTOP_PORT_MAX             (9)

// Operation codes

#define CFS_OPER_WAIT                 (0x00010000)
#define CFS_OPER_READ                 (0x01010000)
#define CFS_OPER_WRITE                (0x01020000)
#define CFS_OPER_CONFIG               (0x01030000)

// Diagnostic codes

#define CFS_DIAG_CONNCLOSE            (0x0000F001)
#define CFS_DIAG_WOULDBLOCK           (0x0000F002)
#define CFS_DIAG_READNOBLOCK          (0x0000F003)
#define CFS_DIAG_WRITENOBLOCK         (0x0000F004)
#define CFS_DIAG_TIMEDOUT             (0x0000F005)
#define CFS_DIAG_ALLDATA              (0x0000F006)
#define CFS_DIAG_PARTIALDATA          (0x0000F007)
#define CFS_DIAG_NODATA               (0x0000F008)
#define CFS_DIAG_INVALIDSIZE          (0x0000F009)
#define CFS_DIAG_ENVOPEN              (0x0000F00A)
#define CFS_DIAG_APPID                (0x0000F00B)
#define CFS_DIAG_SESSIONTYPE          (0x0000F00C)
#define CFS_DIAG_ENVINIT              (0x0000F00D)
#define CFS_DIAG_SOCOPEN              (0x0000F00E)
#define CFS_DIAG_SETFD                (0x0000F00F)
#define CFS_DIAG_SOCINIT              (0x0000F010)
#define CFS_DIAG_NOTFOUND             (0x0000F011)
#define CFS_DIAG_SESSIONINIT          (0x0000F012)
#define CFS_DIAG_SEQNUM_EXHAUSTED     (0x0000F013)
#define CFS_DIAG_LIBNOTFOUND          (0x0000F014)
#define CFS_DIAG_SRVCNOTFOUND         (0x0000F015)
#define CFS_DIAG_PROCNOTFOUND         (0x0000F016)
#define CFS_DIAG_SECMODE              (0x00000017)
#define CFS_DIAG_SYSTEM               (0x0000FFFE)
#define CFS_DIAG_UNKNOWN              (0x0000FFFF)

typedef void* CFSENV;
typedef void* CFSRPS;

typedef struct tagCFSERRINFO {

  int m_errno;
  int sslrc;
  char* szMessage;

} CFSERRINFO;

//////////////////////////////////////////////////////////////////////////////
//  Virtual function table interface
//////////////////////////////////////////////////////////////////////////////

typedef struct tagCFS_SESSION CFS_SESSION;

typedef struct tagCFSVTBL {

  CSRESULT (*CFS_Receive)         (CFS_SESSION*, char*, long*);
  CSRESULT (*CFS_ReceiveRecord)   (CFS_SESSION*, char*, long*);
  CSRESULT (*CFS_Send)            (CFS_SESSION*, char*, long*);
  CSRESULT (*CFS_SendRecord)      (CFS_SESSION*, char*, long*);

} CFSVTBL;

typedef CFSVTBL* LPCFSVTBL;

typedef struct tagCFS_SESSION {

  LPCFSVTBL lpVtbl;

} CFS_SESSION;

typedef struct tagSESSIONINFO {
  
  char* szHost;
  char* szPort;
  char* szLocalPort;
  char* szRemotePort;

} SESSIONINFO;

// ---------------------------------------------------------------------------
// Prototypes
// ---------------------------------------------------------------------------

CSRESULT
  CFS_CloseChannel
    (CFS_SESSION** Session);

CSRESULT
  CFS_CloseChannelDescriptor
    (CFS_SESSION* This);

CSRESULT
  CFS_CloseEnv
    (CFSENV* pEnv);

CSRESULT
  CFS_CloseSession
    (CFS_SESSION** Session);

CSRESULT
  CFS_GetLastError
    (CFSENV pEnv,
     CFSERRINFO* errInfo);

CFS_SESSION*
  CFS_OpenChannel
    (CFSENV pEnv,
     int connfd);

CFSENV
  CFS_OpenEnv
    (char* szConfig);

CFS_SESSION*
  CFS_OpenSession
    (CFSENV pEnv,
     char* szConfig,
     char* szHost,
     char* szPort);

SESSIONINFO*
  CFS_QuerySessionInfo
    (CFS_SESSION* This);

CSRESULT
  CFS_ReceiveDescriptor
    (int fd,
     int* descriptor,
     int timeout);

CSRESULT
  CFS_SendDescriptor
    (int fd,
     int descriptor,
     int timeout);

CSRESULT
  CFS_SetChannelDescriptor
    (CFS_SESSION* This,
     int connfd,
     int* iSSLResult);

CFSRPS
  CFSRPS_Open
    (char* fileName);

CSRESULT
  CFSRPS_LoadConfig
    (CFSRPS This,
     char* szConfig);

char* 
  CFSRPS_LookupParam
    (CFSRPS This,
     char* szParam);

CSRESULT
  CFSRPS_IterStart
    (CFSRPS This, 
     char* szEnum);

char*
  CFSRPS_IterNext
    (CFSRPS This);

CSRESULT
  CFSRPS_Close
    (CFSRPS* This);

#endif
