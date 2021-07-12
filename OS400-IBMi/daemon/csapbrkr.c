/* ==========================================================================

  Clarasoft Foundation Server

  CSAP Broker
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


#include <except.h>
#include <miptrnam.h>
#include <QLEAWI.h>
#include <qmhchgem.h>
#include <QSYRUSRI.h>
#include <QUSEC.h>
#include <qusrgfa2.h>
#include <QUSRJOBI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "qcsrc/cswsck.h"

#define CFS_DEF_SERVICENAME_MAX  (98)
#define CFS_DEF_LIBNAME_MAX      (11)
#define CFS_DEF_OBJNAME_MAX      (11)
#define CSAP_NULL_SESSION         "000000000000000000000000000000000000"


/////////////////////////////////////////////////////////////////////////////
// The following is the CSAP service handler function procedure
// pointer type definition.
/////////////////////////////////////////////////////////////////////////////

typedef CSRESULT
          (*CSAP_SERVICEHANDLERPROC)
             (CSWSCK pSession,
              char* szSessionID);

CSRESULT
  CSAP_Broker
    (CSWSCK pSession,
     CFSRPS repo);

int conn_fd;
int stream_fd;

CFSENV g_pEnv;

char szLibraryName[CFSRPS_PARAM_MAXBUFF];
char szSrvPgmName[CFSRPS_PARAM_MAXBUFF];
char szInProcHandler[CFSRPS_PARAM_MAXBUFF];

CSWSCK pConn;

volatile unsigned return_code;

void
  Cleanup
    (_CNL_Hndlr_Parms_T* args);

int main(int argc, char **argv)
{

  void *inprocServer;

  char buffer = 0; // dummy byte character
                   // to send to main daemon

  int rc;
  int type;

  char szConfig[CFSRPS_PATH_MAXBUFF];

  uint64_t size;

  CSRESULT hResult;

  CFSRPS repo;
  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO pi;

  return_code = 0;
  #pragma cancel_handler( Cleanup, return_code )

  strncpy(szConfig, argv[2], CFSRPS_PATH_MAXBUFF);
  szConfig[CFSRPS_PATH_MAXBUFF-1] = 0; // make sure we have a null-terminated
                    // string no longer than the buffer

  repo = CFSRPS_Constructor();

/*

  ///////////////////////////////////////////////////////////////////////
  // dynamically load exported function handler; load the service
  // configuration to get information on the handler function.
  ///////////////////////////////////////////////////////////////////////

  strncpy(ci.szPath, szConfig, CFSRPS_PATH_MAXBUFF);

  if (CS_FAIL(CFSRPS_LoadConfig(repo, &ci)))
  {
    exit(1);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCSERVER", &pi)))
  {
    exit(1);
  }

  strncpy(szSrvPgmName, pi.szValue, CFSRPS_PARAM_MAXBUFF);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCSERVERLIB", &pi)))
  {
    exit(1);
  }

  strncpy(szLibraryName, pi.szValue, CFSRPS_PARAM_MAXBUFF);

  pSrvPgm = rslvsp(WLI_SRVPGM, szSrvPgmName, szLibraryName, _AUTH_NONE);
  QleActBndPgm(&pSrvPgm, NULL, NULL, NULL, NULL);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "SERVICEPROC", &cfgpi)))
  {
    exit(1);
  }

  strncpy(szInProcHandler, pi.szValue, CFSRPS_PARAM_MAXBUFF);

  pServiceProc = 0;
  type = 0;
  QleGetExp(NULL, NULL, NULL, szInProcHandler,
            (void **)&pServiceProc, &type, NULL);

  if (pServiceProc = 0)
  {
    exit(2);
  }

*/

  if (argv[1][0] == 'T')
  {
    pConn = CSWSCK_Constructor();

    // This handler is transcient and will end once
    // the connection is ended.

    conn_fd = 0;
    if (CS_SUCCEED(hResult = CSWSCK_OpenChannel(pConn, 0, 0, conn_fd, &rc)))     {

      if (CS_DIAG(hResult) == CSWSCK_DIAG_WEBSOCKET) {

        CSAP_Broker(pConn, repo);
      }
      else {


      }

      CSWSCK_CloseChannel(pConn, 0, 0, 30);
   }

    CSWSCK_Destructor(&pConn);
  }
  else
  {
    // This handler is resident and will not exit until parent dameon
    // ends.

    pConn = CSWSCK_Constructor();

    stream_fd = 0;

    /////////////////////////////////////////////////////////////////////
    // Try to send parent a byte; this indicates we are ready
    // to handle a client...
    /////////////////////////////////////////////////////////////////////

    send(stream_fd, &buffer, 1, 0);

    for (;;)
    {
      /////////////////////////////////////////////////////////////////////
      // The main server will eventually hand over the connection socket
      // needed to communicate with a client via the IPC descriptor.
      /////////////////////////////////////////////////////////////////////

      hResult = CFS_ReceiveDescriptor(stream_fd, &conn_fd, -1);

      if (CS_SUCCEED(hResult))
      {
        if (CS_SUCCEED(hResult = CSWSCK_OpenChannel(pConn, 0, 0, conn_fd, &rc))) {

          if (CS_DIAG(hResult) == CSWSCK_DIAG_WEBSOCKET) {

            CSAP_Broker(pConn, repo);
          }
          else {


          }

          CSWSCK_CloseChannel(pConn, 0, 0, 30);
        }

        //////////////////////////////////////////////////////////////////
        // Tell main daemon we can handle another connection
        //////////////////////////////////////////////////////////////////

        send(stream_fd, &buffer, 1, 0);
      }
    }

    CSWSCK_Destructor(&pConn);
  }

  close(stream_fd);

  CFSRPS_Destructor(&repo);

  #pragma disable_handler

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
// CSAP Broker over websocket
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Broker
    (CSWSCK pSession,
     CFSRPS repo) {

  int i;
  int type;
  int argSize;
  int iSSLResult;

  volatile unsigned return_code;
  volatile int rsl_ok;
  volatile _INTRPT_Hndlr_Parms_T excbuf;

  short dummySocket;

  char szHandshake[50];
  char szAuthType[11];
  char szStatus[11];
  char szConfig[CFSRPS_PATH_MAXBUFF];

  char szServiceName[CFSRPS_PARAM_MAXBUFF];
  char szLibraryName[11];
  char szSrvPgmName[11];
  char szInProcHandler[CFSRPS_PARAM_MAXBUFF];
  char szSessionID[37];
  char szSrvId[33];
  char szAuthorizationType[11];
  char szConnTimeout[11];

  char  szInstance[33];

  Qus_EC_t ErrorCode;
  Qsy_USRI0300_T pData300;

  uint64_t size;
  uint64_t offset;

  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO pi;

  CSRESULT hResult;

  _SYSPTR pSrvPgm;  // OS400 System pointer

  ///////////////////////////////////////////////////////////////////////////
  // Pointer to CSAP service handler function exported by service program.
  ///////////////////////////////////////////////////////////////////////////

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  ///////////////////////////////////////////////////////////////////////////
  // Get CSAP request (version 3.0)
  //
  // Format is:
  //
  //      SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS ... S
  //
  // Where S == service name (98 bytes)
  //
  ///////////////////////////////////////////////////////////////////////////

  if (CS_SUCCEED(CSWSCK_Receive(pSession, &size, -1))) {

    // As a rule, the CSAP handshake must arrive in one frame;
    // we should have the service in a single read (98 bytes).

    if (size != 98 ) {
      return CS_FAILURE;
    }

    // get service name
    CSWSCK_GetData(pSession, szServiceName, 0, 98);

    szServiceName[CFSRPS_PATH_MAXBUFF-1] = 0;

    // Log connection

    ////////////////////////////////////////////////
    // Get service program name and exported
    // sub-procedure that implements the
    // required service.
    ////////////////////////////////////////////////

    strncpy(ci.szPath, szServiceName, CFSRPS_PATH_MAXBUFF);

    if (CS_SUCCEED(CFSRPS_LoadConfig(repo, &ci))) {

      if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCHANDLER", &pi))) {

        // protocol implementation not found

        memcpy(szHandshake,     "101",              3);
        memcpy(szHandshake + 3, "1000010011",       10);
        memcpy(szHandshake + 13, CSAP_NULL_SESSION, 36);

        CSWSCK_Send(pSession,
                    CSWSCK_OPER_TEXT,
                    szHandshake,
                    49,
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }

      strncpy(szSrvPgmName, pi.szValue, 11);
      szSrvPgmName[10] = 0;

      if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCHANDLER_LIB", &pi))) {

        // protocol implementation not found

        memcpy(szHandshake,     "101",              3);
        memcpy(szHandshake + 3, "1000010012",       10);
        memcpy(szHandshake + 13, CSAP_NULL_SESSION, 36);

        CSWSCK_Send(pSession,
                    CSWSCK_OPER_TEXT,
                    szHandshake,
                    49,
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }

      strncpy(szLibraryName, pi.szValue, 11);
      szLibraryName[10] = 0;

      if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCHANDLER_EXPORT", &pi))) {

        // protocol implementation not found

        memcpy(szHandshake,     "101",              3);
        memcpy(szHandshake + 3, "1000010013",       10);
        memcpy(szHandshake + 13, CSAP_NULL_SESSION, 36);

        CSWSCK_Send(pSession,
                    CSWSCK_OPER_TEXT,
                    szHandshake,
                    49,
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }

      strncpy(szInProcHandler, pi.szValue, 256);

#pragma exception_handler(DLOPEN_ERR, excbuf, 0, _C2_MH_ESCAPE, _CTLA_HANDLE_NO_MSG)

      pSrvPgm = rslvsp(WLI_SRVPGM,
                       szSrvPgmName,
                       szLibraryName,
                       _AUTH_NONE);

#pragma disable_handler

//#pragma exception_handler(DLOPEN_ERR, excbuf, 0, _C2_MH_ESCAPE, _CTLA_HANDLE_NO_MSG)

      QleActBndPgm(&pSrvPgm,
                   NULL,
                   NULL,
                   NULL,
                   NULL);

//#pragma disable_handler

      type = 0;  // pointer to function

//#pragma exception_handler(DLOPEN_ERR, excbuf, 0, _C2_MH_ESCAPE, _CTLA_HANDLE_NO_MSG)

      QleGetExp(NULL,
                NULL,
                NULL,
                szInProcHandler,
                (void**)&CSAP_ServiceHandler,
                &type,
                NULL);

//#pragma disable_handler

      if (!CSAP_ServiceHandler) {

//////////////////////////////////////////////////////////////////////////////
// Branching label
DLOPEN_ERR:

        // protocol implementation not loaded

        memcpy(szHandshake,     "101",              3);
        memcpy(szHandshake + 3, "1000010014",       10);
        memcpy(szHandshake + 13, CSAP_NULL_SESSION, 36);

        CSWSCK_Send(pSession,
                    CSWSCK_OPER_TEXT,
                    szHandshake,
                    49,
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }

      // Create a session ID

      CSSYS_MakeUUID(szSessionID,
                     CSSYS_UUID_LOWERCASE | CSSYS_UUID_DASHES);

      memcpy(szHandshake,     "000",        3);
      memcpy(szHandshake + 3, "4000010202", 10);
      memcpy(szHandshake + 13, szSessionID, 36);

      // Send ACK

      CSWSCK_Send(pSession,
                  CSWSCK_OPER_TEXT,
                  szHandshake,
                  49,
                  CSWSCK_FIN_ON,
                  -1);

      /////////////////////////////////////////////
      // Run the requested service.
      /////////////////////////////////////////////

      CSAP_ServiceHandler(pSession, szSessionID);

    }
    else {

      // Service configuration not found

      memcpy(szHandshake,     "101",              3);
      memcpy(szHandshake + 3, "1000010015",       10);
      memcpy(szHandshake + 13, CSAP_NULL_SESSION, 36);

      CSWSCK_Send(pSession,
                  CSWSCK_OPER_TEXT,
                  szHandshake,
                  49,
                  CSWSCK_FIN_ON,
                  -1);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client

    memcpy(szHandshake,     "101",              3);
    memcpy(szHandshake + 3, "1000010061",       10);
    memcpy(szHandshake + 13, CSAP_NULL_SESSION, 36);

    CSWSCK_Send(pSession,
                CSWSCK_OPER_TEXT,
                szHandshake,
                49,
                CSWSCK_FIN_ON,
                -1);

    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
 * Cleanup
 * ----------------------------------------------------------------------- */

void Cleanup(_CNL_Hndlr_Parms_T* data) {

  CSWSCK_CloseChannel(pConn, 0, 0, 30);
  close(stream_fd);
}
 
