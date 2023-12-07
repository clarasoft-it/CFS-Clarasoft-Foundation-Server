/* ==========================================================================

  Clarasoft Foundation Server - OS/400 
  CFS Service Handler

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
#include <qmhchgem.h>
#include <qusrgfa2.h>
#include <QLEAWI.h>
#include <QSYGETPH.h>
#include <QSYRUSRI.h>
#include <QUSEC.h>
#include <QUSRJOBI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include "qcsrc/cfs.h"

typedef CSRESULT
  (*SERVICEHANDLERPROC)
    (CFS_SESSION* pSession);

SERVICEHANDLERPROC
  GetServiceExport
    (char* pServiceConfig);

void
  Cleanup
    (_CNL_Hndlr_Parms_T* args);

int conn_fd;
int stream_fd;

char szLibraryName[11];
char szSrvPgmName[11];
char szInProcHandler[11];

volatile unsigned return_code;

int main(int argc, char** argv)
{
  char buffer = 0; // dummy byte character
                   // to send to main daemon

  char szConfig[99];

  SERVICEHANDLERPROC pServiceHandler;
  CSRESULT hResult;
  CFSENV pEnv;
  CFS_SESSION* pSession;

  return_code = 0;
  #pragma cancel_handler( Cleanup, return_code )

  // Get repository configuration
  strncpy(szConfig, argv[1], 99);
  szConfig[98] = 0; // make sure we have a null-terminated
                    // string no longer than the buffer

  fprintf(stderr, "Starting handler with config: %s", szConfig); fflush(stderr);
  pEnv = CFS_OpenEnv(szConfig);
  if (!pEnv) {
    fprintf(stderr, "Cannot open environment %s", szConfig); fflush(stderr);
    return 1;
  }

  fprintf(stderr, "Retrieveing service export"); fflush(stderr);
  pServiceHandler = GetServiceExport(szConfig);

  if (pServiceHandler == NULL) {
    fprintf(stderr, "Failed to get service export"); fflush(stderr);
    CFS_CloseEnv(&pEnv);
    return 2;
  }

  /////////////////////////////////////////////////////////////////////
  // Try to send parent a byte; this indicates we are ready
  // to handle a client...
  /////////////////////////////////////////////////////////////////////

  stream_fd = 0;

  for (;;)
  {
    send(stream_fd, &buffer, 1, 0);

    /////////////////////////////////////////////////////////////////////
    // The main daemon will eventually hand over the connection socket
    // needed to communicate with a client via the IPC descriptor.
    /////////////////////////////////////////////////////////////////////

    fprintf(stderr, "Waiting for client handle"); fflush(stderr);
    hResult = CFS_ReceiveDescriptor(stream_fd, &conn_fd, -1);

    if (CS_SUCCEED(hResult))
    {
      if ((pSession = CFS_OpenChannel(pEnv, conn_fd)) != NULL) {

        pServiceHandler(pSession);
        CFS_CloseChannel(&pSession);
      }
      else {
        // failed to open connection with client
      }
    }
  }

  close(stream_fd);
  CFS_CloseEnv(&pEnv);

  #pragma disable_handler

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
//  This function activates a service program and
//  retrieves a pointer from an exported procedure
//
/////////////////////////////////////////////////////////////////////////////

SERVICEHANDLERPROC
  GetServiceExport
    (char* pServiceConfig) {

  int type;

  char szLibraryName[11];
  char szSrvPgmName[11];
  char szInProcHandler[256];

  CFSRPS pRepo;
  CFSCFG pConfig;

  _SYSPTR pSrvPgm;  // OS400 System pointer

  // Pointer to service handler function exported by service program.

  SERVICEHANDLERPROC pServiceHandler;

  pRepo = CFSRPS_Open(0);

  if (pRepo == NULL) {
    fprintf(stderr, "Failed to open Repository");
    return NULL;
  }

  pConfig = CFSRPS_OpenConfig(pRepo, pServiceConfig);

  if (pConfig == NULL) {

    fprintf(stderr, "Failed to open configuration %s",
                    pServiceConfig);
    fflush(stderr);
    CFSRPS_Close(&pRepo);
    return NULL;
  }

  strncpy(szSrvPgmName, CFSCFG_LookupParam(pConfig, "INPROCSERVER"), 11);
  szSrvPgmName[10] = 0;
  strncpy(szLibraryName, CFSCFG_LookupParam(pConfig, "INPROCSERVER_LIB"), 11);
  szLibraryName[10] = 0;
  strncpy(szInProcHandler, CFSCFG_LookupParam(pConfig, "INPROCHANDLER"), 256);
  szInProcHandler[255] = 0;

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

#pragma exception_handler(DLOPEN_ERR, 0, 0, _C2_MH_ESCAPE, _CTLA_HANDLE_NO_MSG)

  fprintf(stderr, "Retrieving service program pointer for %s/%s",
                  szLibraryName, szSrvPgmName);
  fflush(stderr);

  pSrvPgm = rslvsp(WLI_SRVPGM,
                   szSrvPgmName,
                   szLibraryName,
                   _AUTH_NONE);

#pragma disable_handler

  fprintf(stderr, "Activating service program %s/%s",
                  szLibraryName, szSrvPgmName);
  fflush(stderr);

  QleActBndPgm(&pSrvPgm,
               NULL,
               NULL,
               NULL,
               NULL);

  type = 0;  // pointer to function

  fprintf(stderr, "Retrieveing export function pointer for %s",
                  szInProcHandler);
  fflush(stderr);

  QleGetExp(NULL,
            NULL,
            NULL,
            szInProcHandler,
            (void**)&pServiceHandler,
            &type,
            NULL);

  if (pServiceHandler) {
    return pServiceHandler;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Branching label
  DLOPEN_ERR:

  //fprintf(stderr, "\nERROR: Cannot retrieve export procedure pointer\n");
  //fflush(stderr);

  fprintf(stderr, "Failed to retrieve export (function) pointer: %s/%s:%s",
                  szLibraryName, szSrvPgmName, szInProcHandler);
  fflush(stderr);

  return NULL;
}

/* --------------------------------------------------------------------------
 * Cleanup
 * ----------------------------------------------------------------------- */

void Cleanup(_CNL_Hndlr_Parms_T* data) {

  close(stream_fd);
}

