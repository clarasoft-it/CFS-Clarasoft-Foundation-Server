/* ==========================================================================

  Clarasoft Foundation Server - Linux
  CSAP Broker

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

#include "qcsrc/cswsck.h"
#include "qcsrc/cfsrepo.h"
#include "qcsrc/csjson.h"
#include "qcsrc/csap.h"

#define CFS_DEF_SERVICENAME_MAX  (98)
#define CFS_DEF_LIBNAME_MAX      (256)
#define CFS_DEF_OBJNAME_MAX      (256)
#define CSAP_NULL_SESSION         "000000000000000000000000000000000000"

#define CSAP_SERVICEMODE_SINGLE_LOAD       1
#define CSAP_SERVICEMODE_ENUMERATION_LOAD  2
#define CSAP_SERVICEMODE_DYNAMIC_LOAD      3

void signalCatcher(int signal);

typedef void
  (*CSAP_SERVICEHANDLERPROC)
    (CSAP pCSAP,
     char* szUser);

CSAP_SERVICEHANDLERPROC
  CSAP_GetServiceExport
    (CSMAP inprocServerMap,
     char* pServiceConfig);

CSRESULT
  CSAP_Broker_Single
    (char* szService,
     int login,
     CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler);

CSRESULT
  CSAP_Broker_Enum
    (CSMAP* Services,
     int login);

CSRESULT
  CSAP_Broker
    (int login);

CSRESULT
  ValidateUserProfile
    (char* szUser);

typedef struct tagSERVICEINFOSTRUCT {

  char *szService;
  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

} SERVICEINFOSTRUCT;

int conn_fd;
int stream_fd;

long iServiceMode;

CSWSCK pSession;
CSAP  pCSAP;
CSMAP inprocServerMap;
CFSENV pEnv;
CSJSON pJsonIn;
CSJSON pJsonOut;
CSJSON_LSENTRY lse;

volatile unsigned return_code;

int main(int argc, char** argv)
{
  char buffer = 0; // dummy byte character
                   // to send to main daemon

  int login;

  long size;
  long valueSize;

  char szConfig[99];
  char szService[99];

  char* pszParam;
  char* pszKey;

  void* pInprocServer;

  struct sigaction sa;

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  CSRESULT hResult;

  CFSRPS pRepo;
  CFSCFG pConfig;
  CSMAP  pServices;

  SERVICEINFOSTRUCT ServiceInfo;

  return_code = 0;
  #pragma cancel_handler( Cleanup, return_code )

  pEnv = NULL;
  pRepo = NULL;

  inprocServerMap = CSMAP_Constructor();

  // Get broker repository configuration (making sure no more than
  // CFSRPS_PATH_MAXBUFF chars are copied from the outside)

  size = strlen(argv[1]);

  if (size > 255) {
    size = 255;
  }

  memcpy(szConfig, argv[1], size);
  szConfig[size] = 0; // make sure we have a null

  pCSAP     = CSAP_Constructor();
  pServices = CSMAP_Constructor();
  pJsonIn   = CSJSON_Constructor();
  pJsonOut  = CSJSON_Constructor();
  pSession  = CSWSCK_Constructor();

  pRepo = CFSRPS_Open(0);

  if (pRepo == NULL) {
    goto CSAPBRKR_END;
  }

  if ((pConfig = CFSRPS_OpenConfig(pRepo, szConfig)) == NULL) {
    CFSRPS_Close(&pRepo);
    goto CSAPBRKR_END;
  }

  pEnv = CFS_OpenEnv(szConfig);

  if (pEnv == NULL) {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    goto CSAPBRKR_END;
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "USER_LOGIN")) != NULL) {
    if (!strcmp(pszParam, "*NO")) {
      login = 0;
    }
    else {
      login = 1;
    }
  }
  else {
    login = 0;
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "SERVICE_MODE")) == NULL) {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    goto CSAPBRKR_END;
  }

  if (!strcmp(pszParam, "*SINGLE")) {

    iServiceMode = CSAP_SERVICEMODE_SINGLE_LOAD;

    if (CFSCFG_IterStart(pConfig, "SERVICES") == CS_SUCCESS) {

      // Fetch the first service in sequence from
      // the SERVICES enumeration

      while ((pszParam = CFSCFG_IterNext(pConfig)) != NULL) {

        CSAP_ServiceHandler =
                  CSAP_GetServiceExport(inprocServerMap, pszParam);

        // Get first entry only
        break;
      }

      if (CSAP_ServiceHandler == NULL) {
        CFSRPS_CloseConfig(pRepo, &pConfig);
        CFSRPS_Close(&pRepo);
        goto CSAPBRKR_END;
      }

      strcpy(szService, pszParam);
    }
    else {
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      goto CSAPBRKR_END;
    }
  }
  else {

    if (!strcmp(pszParam, "*ENUM")) {

      // limit the available services to a set (enumeration) of services

      iServiceMode = CSAP_SERVICEMODE_ENUMERATION_LOAD;

      if (CFSCFG_IterStart(pRepo, "SERVICES") == CS_SUCCESS) {

        // Fetch the service handlers from
        // the SERVICES enumeration and store them in
        // the service map

        while ((pszParam = CFSCFG_IterNext(pConfig)) != NULL) {

          ServiceInfo.CSAP_ServiceHandler =
                  CSAP_GetServiceExport(inprocServerMap, pszParam);

          if (ServiceInfo.CSAP_ServiceHandler != 0) {

            CSMAP_Insert(pServices,
                         pszParam,
                         &ServiceInfo,
                         sizeof(SERVICEINFOSTRUCT));
          }
        }
      }
    }
    else {

      if (!strcmp(pszParam, "*ANY")) {

        iServiceMode = CSAP_SERVICEMODE_DYNAMIC_LOAD;
      }
    }
  }

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

  stream_fd = 0;

  for (;;) {

    /////////////////////////////////////////////////////////////////////
    // Try to send parent a byte; this indicates we are ready
    // to handle a client...
    /////////////////////////////////////////////////////////////////////

    send(stream_fd, &buffer, 1, 0);

    /////////////////////////////////////////////////////////////////////
    // The main daemon will eventually hand over the connection socket
    // needed to communicate with a client via the IPC descriptor.
    /////////////////////////////////////////////////////////////////////

    hResult = CFS_ReceiveDescriptor(stream_fd, &conn_fd, -1);

    if (CS_SUCCEED(hResult))
    {

      hResult = CSWSCK_OpenChannel(pSession, CFS_OpenChannel(pEnv, conn_fd));

      if (CS_DIAG(hResult) == CSWSCK_DIAG_WEBSOCKET) {

        switch(iServiceMode) {
          case CSAP_SERVICEMODE_SINGLE_LOAD:
            CSAP_Broker_Single(szService, login, CSAP_ServiceHandler);
            break;
          case CSAP_SERVICEMODE_ENUMERATION_LOAD:
            CSAP_Broker_Enum(pServices, login);
            break;
          case CSAP_SERVICEMODE_DYNAMIC_LOAD:
            CSAP_Broker(login);
            break;
        }
      }

      CSAP_CloseChannel(pCSAP);
    }
  }

  close(stream_fd);

  CSAPBRKR_END:

  CSAP_Destructor(&pCSAP);
  CSMAP_Destructor(&pServices);
  CSMAP_Destructor(inprocServerMap);
  CSJSON_Destructor(&pJsonIn);
  CSJSON_Destructor(&pJsonOut);

  #pragma disable_handler

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
//  This function activates a shared library and
//  retrieves a pointer from an exported procedure
//
/////////////////////////////////////////////////////////////////////////////

CSAP_SERVICEHANDLERPROC
  CSAP_GetServiceExport
    (CSMAP inprocServerMap,
     char* pServiceConfig) {

  int type;

  char* pszParam;

  void *inprocServer;

  CFSRPS pRepo;
  CFSCFG pConfig;

  char szSrvPgmName[11];
  char szLibraryName[11];
  char szInProcHandler[129];

  _SYSPTR pSrvPgm;  // OS400 System pointer

  // Pointer to CSAP service handler function exported by shared library.

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  pRepo = CFSRPS_Open(0);

  if ((pConfig = CFSRPS_OpenConfig(pRepo, pServiceConfig)) == NULL) {
    CFSRPS_Close(&pRepo);
    return NULL;
  }

  strncpy(szSrvPgmName, CFSCFG_LookupParam(pConfig,
                                     "INPROCSERVER"), 11);
  szSrvPgmName[10] = 0;
  fprintf(stderr, "Service program: %s", szSrvPgmName);
  fflush(stderr);

  strncpy(szLibraryName, CFSCFG_LookupParam(pConfig,
                                     "INPROCSERVER_LIB"), 11);
  szLibraryName[10] = 0;
  fprintf(stderr, "Library: %s", szLibraryName);
  fflush(stderr);

  strncpy(szInProcHandler, CFSCFG_LookupParam(pConfig,
                                     "INPROCHANDLER"), 129);
  szInProcHandler[128] = 0;
  fprintf(stderr, "Export: %s", szInProcHandler);
  fflush(stderr);

#pragma exception_handler(DLOPEN_ERR, 0, 0,_C2_MH_ESCAPE, _CTLA_HANDLE_NO_MSG)

  pSrvPgm = rslvsp(WLI_SRVPGM,
                   szSrvPgmName,
                   szLibraryName,
                   _AUTH_NONE);

#pragma disable_handler

  QleActBndPgm(&pSrvPgm,
               NULL,
               NULL,
               NULL,
               NULL);

  type = 0;  // pointer to function

  QleGetExp(NULL,
            NULL,
            NULL,
            szInProcHandler,
            (void**)&CSAP_ServiceHandler,
            &type,
            NULL);

  if (CSAP_ServiceHandler) {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    return CSAP_ServiceHandler;
  }

//////////////////////////////////////////////////////////////////////////////
// Branching label
DLOPEN_ERR:

  fprintf(stderr, "\nERROR: Cannot retrieve export procedure pointer");
  fflush(stderr);

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

  return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
// CSAP Broker over websocket: (*SINGLE)
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Broker_Single
    (char* szService,
     int login,
     CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler) {

  char* pHandshake;
  char szUser[256];
  char szSessionID[37];

  uint64_t size;

  if (CS_SUCCEED(CSWSCK_ReceiveAll(pSession, &size, 1))) {

    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    if (CS_SUCCEED(CSJSON_Parse(pJsonIn, CSWSCK_GetDataRef(pSession), 0))) {

      if (login == 1) {

        /*
        // authenticate user
        // get user id
        if (CS_SUCCEED(CSJSON_LookupKey(pCSAP->pJsonIn,
                                        "/", "user",
                                        &(pCSAP->lse)))) {

          if (CS_FAIL(ValidateUserProfile(pCSAP, szUser))) {
            return CS_FAILURE;
          }
        }
        else {
          return CS_FAILURE;
        }
        */
        szUser[0] = 0;
      }
      else {
        szUser[0] = 0;
      }

      // get service name
      if (CS_SUCCEED(CSJSON_LookupKey(pJsonIn,
                                      "/", "service",
                                      &lse))) {

        if (strcmp(lse.szValue, szService) == 0) {

          // Create a session ID

          CSSYS_MakeUUID(szSessionID,
                     CSSYS_UUID_LOWERCASE | CSSYS_UUID_DASHES);

          CSJSON_InsertString(pJsonOut,
                        "/handshake", "version", "7.0");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "status", "000");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "reason", "0000000000");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "sid", szSessionID);

          size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pSession,
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          CSAP_OpenChannel(pCSAP, pSession, szSessionID);
          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          CSAP_ServiceHandler(pCSAP, szUser);
        }
        else {

          CSJSON_InsertString(pJsonOut,
                            "/handshake", "version", "7.0");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "status", "101");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "reason", "1000010011");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "sid", CSAP_NULL_SESSION);

          size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pSession,
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        CSJSON_InsertString(pJsonOut,
                          "/handshake", "version", "7.0");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "status", "101");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "reason", "1000010015");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "sid", CSAP_NULL_SESSION);

        size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

        CSWSCK_Send(pSession,
                    CSWSCK_OP_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error

      CSJSON_InsertString(pJsonOut,
                        "/handshake", "version", "7.0");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "status", "101");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "reason", "1000010062");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "sid", CSAP_NULL_SESSION);

      size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

      CSWSCK_Send(pSession,
                  CSWSCK_OP_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;
    }
  }
  else {

    // Error reading client
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
//
// CSAP Broker over websocket (*ENUM)
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Broker_Enum
    (CSMAP* Services,
     int login) {

  char* pHandshake;
  char szUser[256];
  char szSessionID[37];

  long bytes;

  uint64_t size;

  SERVICEINFOSTRUCT* ServiceInfo;

  if (CS_SUCCEED(CSWSCK_ReceiveAll(pSession,
                                &size,
                                1))) {

    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    if (CS_SUCCEED(CSJSON_Parse(pJsonIn, CSWSCK_GetDataRef(pSession), 0))) {

      if (login == 1) {
        // authenticate user
        // get user id
        if (CS_FAIL(ValidateUserProfile(szUser))) {
          return CS_FAILURE;
        }
      }
      else {
        szUser[0] = 0;
      }

      // get service name
      if (CS_SUCCEED(CSJSON_LookupKey(pJsonIn,
                                      "/", "service",
                                      &lse))) {

        // If requsted service is in the service map ...

        if (CS_SUCCEED(CSMAP_Lookup(Services,
                                    lse.szValue,
                                   (void**)&ServiceInfo, &bytes))) {

          // Create a session ID

          CSSYS_MakeUUID(szSessionID,
                     CSSYS_UUID_LOWERCASE | CSSYS_UUID_DASHES);

          CSJSON_InsertString(pJsonOut,
                        "/handshake", "version", "7.0");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "status", "000");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "reason", "0000000000");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "sid", szSessionID);

          size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pSession,
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          CSAP_OpenChannel(pCSAP, pSession, szSessionID);
          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          ServiceInfo->CSAP_ServiceHandler(pCSAP, szUser);

        }
        else {

          CSJSON_InsertString(pJsonOut,
                            "/handshake", "version", "7.0");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "status", "101");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "reason", "1000010011");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "sid", CSAP_NULL_SESSION);

          size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pSession,
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        CSJSON_InsertString(pJsonOut,
                          "/handshake", "version", "7.0");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "status", "101");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "reason", "1000010015");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "sid", CSAP_NULL_SESSION);

        size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

        CSWSCK_Send(pSession,
                    CSWSCK_OP_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error

      CSJSON_InsertString(pJsonOut,
                        "/handshake", "version", "7.0");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "status", "101");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "reason", "1000010062");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "sid", CSAP_NULL_SESSION);

      size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

      CSWSCK_Send(pSession,
                  CSWSCK_OP_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
//
// CSAP Broker over websocket (*ANY)
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Broker
    (int login) {

  char* pHandshake;
  char szUser[256];
  char szSessionID[37];

  uint64_t size;

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  if (CS_SUCCEED(CSWSCK_ReceiveAll(pSession,
                                   &size,
                                   1))) {

    CSJSON_Init(pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    if (CS_SUCCEED(CSJSON_Parse(pJsonIn, CSWSCK_GetDataRef(pSession), 0))) {

      if (login == 1) {
        // authenticate user
        // get user id
        if (CS_FAIL(ValidateUserProfile(szUser))) {
          return CS_FAILURE;
        }
      }
      else {
        szUser[0] = 0;
      }

      // get service name
      if (CS_SUCCEED(CSJSON_LookupKey(pJsonIn,
                                      "/", "service",
                                      &lse))) {

        CSAP_ServiceHandler =
                  CSAP_GetServiceExport(inprocServerMap, lse.szValue);

        if (CSAP_ServiceHandler != 0) {

          // Create a session ID

          CSSYS_MakeUUID(szSessionID,
                     CSSYS_UUID_LOWERCASE | CSSYS_UUID_DASHES);

          CSJSON_InsertString(pJsonOut,
                        "/handshake", "version", "7.0");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "status", "000");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "reason", "0000000000");
          CSJSON_InsertString(pJsonOut,
                        "/handshake", "sid", szSessionID);

          size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pSession,
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          CSAP_OpenChannel(pCSAP, pSession, szSessionID);
          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          CSAP_ServiceHandler(pCSAP, szUser);
        }
        else {

          CSJSON_InsertString(pJsonOut,
                            "/handshake", "version", "7.0");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "status", "101");
          CSJSON_InsertString(pJsonOut,
                              "/handshake", "reason", "1000010011");
          CSJSON_InsertString(pJsonOut,
                            "/handshake", "sid", CSAP_NULL_SESSION);

          size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pSession,
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        CSJSON_InsertString(pJsonOut,
                          "/handshake", "version", "7.0");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "status", "101");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "reason", "1000010015");
        CSJSON_InsertString(pJsonOut,
                          "/handshake", "sid", CSAP_NULL_SESSION);

        size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

        CSWSCK_Send(pSession,
                    CSWSCK_OP_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error

      CSJSON_InsertString(pJsonOut,
                        "/handshake", "version", "7.0");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "status", "101");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "reason", "1000010062");
      CSJSON_InsertString(pJsonOut,
                        "/handshake", "sid", CSAP_NULL_SESSION);

      size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

      CSWSCK_Send(pSession,
                  CSWSCK_OP_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

void signalCatcher(int signal)
{

  switch (signal)
  {
    case SIGTERM:

      CFS_CloseChannel(pSession);
      close(stream_fd);
      //dlclose(inprocServer);

      break;
  }

  return;
}

CSRESULT
  ValidateUserProfile
    (char* szUser) {

  char* pHandshake;

  char szBuffer[1025];
  char szPassword[513];
  char szProfileHandle[12];
  char cpf[8];
  char errInfo[16];

  int bytesProvided;
  int bytesAvail;
  int pwdSize;

  long size;

  // authenticate user
  // get user id

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn,
                               "/", "u",
                               &lse))) {

    CSJSON_InsertString(pJsonOut,
                      "/handshake", "version", "7.0");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "status", "701");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "reason", "1000010111");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pSession,
                CSWSCK_OPER_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  memset(szUser, ' ', 10);
  memcpy(szUser, lse.szValue, strlen(lse.szValue));

  // get password
  if (CS_FAIL(CSJSON_LookupKey(pJsonIn,
                               "/", "p",
                               &lse))) {

    CSJSON_InsertString(pJsonOut,
                      "/handshake", "version", "7.0");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "status", "701");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "reason", "1000010222");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pSession,
                CSWSCK_OPER_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  pwdSize = strlen(lse.szValue);

  if (pwdSize > 512) {
    lse.szValue[512] = 0;
    pwdSize = 512;
  }

  memset(errInfo, ' ', 16);
  memset(szProfileHandle, ' ', 12);

  bytesProvided = 16;
  memcpy(errInfo, &bytesProvided, sizeof(int));

  QSYGETPH(szUser,
            lse.szValue,
            (void*)szProfileHandle,
            errInfo,
            pwdSize,
            0);

  memcpy(cpf, errInfo + 8, 7);
  cpf[7] = 0;

  if (strcmp("       ", cpf)) {

    CSJSON_InsertString(pJsonOut,
                      "/handshake", "version", "7.0");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "status", "701");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "reason", "1000010333");
    CSJSON_InsertString(pJsonOut,
                      "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pSession,
                CSWSCK_OPER_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
 * Cleanup
 * ----------------------------------------------------------------------- */

void Cleanup(_CNL_Hndlr_Parms_T* data) {
  CSWSCK_CloseChannel(pSession, 0, 0);
  close(stream_fd);
}

