/* ==========================================================================

  Clarasoft Foundation Server - OS/400 IBM i 
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

#include "qcsrc_cfs/cswsck.h"
#include "qcsrc_cfs/cfsrepo.h"
#include "qcsrc_cfs/csjson.h"

#define CFS_DEF_SERVICENAME_MAX  (98)
#define CFS_DEF_LIBNAME_MAX      (256)
#define CFS_DEF_OBJNAME_MAX      (256)
#define CSAP_NULL_SESSION         "000000000000000000000000000000000000"

#define CSAP_SERVICEMODE_SINGLE_LOAD       1
#define CSAP_SERVICEMODE_ENUMERATION_LOAD  2
#define CSAP_SERVICEMODE_DYNAMIC_LOAD      3

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

///////////////////////////////////////////////////////
// The following is to allow for friend access
// to CSAP instance
///////////////////////////////////////////////////////

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

  char* pUsrCtlSlab;
  char* pInData;
  char* pOutData;

  CSLIST OutDataParts;

  CFSRPS pRepo;

} CSAP;

CSAP*
  CSAP_Constructor
    (void);

CSRESULT
  CSAP_Destructor
    (CSAP** This);

CSRESULT
  CSAP_Clear
    (CSAP* This);

typedef void
  (*CSAP_SERVICEHANDLERPROC)
    (CSAP* pCSAP,
     char* szUser);

CSRESULT
  ValidateUSerProfile
    (CSAP* pCSAP,
     char* szUser);

typedef struct tagCSAPSTAT {

  char szVersion[11];
  char szStatus[4];
  char szReason[11];
  char szSessionID[37];

} CSAPSTAT;

typedef struct tagCSAPCTL {

  long UsrCtlSize;
  long DataSize;
  long fmt;

} CSAPCTL;

typedef struct tagSERVICEINFOSTRUCT {

  char *szService;
  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

} SERVICEINFOSTRUCT;

CSRESULT
  CSAP_Get
    (CSAP* This,
     char* pData,
     long Offset,
     long Size);

void*
  CSAP_GetDataRef
    (CSAP* This);

CSRESULT
  CSAP_GetUserCtl
    (CSAP* This,
     char* pData);

void*
  CSAP_GetUserCtlRef
    (CSAP* This);

CSRESULT
  CSAP_Put
    (CSAP* This,
     char* pData,
     long Size);

CSRESULT
  CSAP_Receive
    (CSAP* This,
     CSAPCTL* pCtlFrame);

CSRESULT
  CSAP_Send
    (CSAP* This,
     char* szSessionID,
     char* pUsrCtl,
     long Size,
     long Fmt);

void signalCatcher(int signal);

CSAP_SERVICEHANDLERPROC
  CSAP_GetServiceExport
    (char* pServiceConfig);

CSRESULT
  CSAP_Broker_Single
    (CSAP* pSession,
     char* szService,
     int login,
     CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler);

CSRESULT
  CSAP_Broker_Enum
    (CSAP* pCSAP,
     CSMAP* Services,
     int login);

CSRESULT
  CSAP_Broker
    (CSAP* pCSAP,
     int login);

CSRESULT
  ValidateUserProfile
    (CSAP* pCSAP,
     char* szUser);

void
  Cleanup
    (_CNL_Hndlr_Parms_T* args);

int conn_fd;
int stream_fd;

char szLibraryName[256];
char szSrvPgmName[256];
char szInProcHandler[256];

char* pHandshakeIn;

long g_CurHandsakeSlab;

volatile unsigned return_code;

CSAP*  pCSAP;
FILE* log;

int main(int argc, char** argv)
{
  char buffer = 0; // dummy byte character
                   // to send to main daemon

  int login;

  long iServiceMode;
  long size;

  char szConfig[99];
  char szService[99];

  char* pszParam;

  struct sigaction sa;

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  CSRESULT hResult;

  CFSRPS pRepo;
  CFSCFG pConfig;
  CSMAP  pServices;
  CFSENV pEnv;

  SERVICEINFOSTRUCT ServiceInfo;

  return_code = 0;

  // Get broker repository configuration (making sure no more than
  // CFSRPS_PATH_MAXBUFF chars are copied from the outside)
  size = strlen(argv[1]);
  if (size > 128) {
    size = 128;
  }

  memcpy(szConfig, argv[1], size);
  szConfig[size] = 0; // make sure we have a null
  fprintf(stderr,
          "Starting CSAP broker with config: %s",
          szConfig);
  fflush(stderr);

  pCSAP     = CSAP_Constructor();
  pServices = CSMAP_Constructor();

  g_CurHandsakeSlab = 1024;
  pHandshakeIn = (char*)malloc(1025 * sizeof(char)); // one more for NULL

  pRepo = CFSRPS_Open(0);

  if (pRepo == NULL) {
    fprintf(stderr,
            "Failed to open repository");
    fflush(stderr);
    goto CSAPBRKR_END;
  }

  if ((pConfig = CFSRPS_OpenConfig(pRepo, szConfig)) == NULL) {
    fprintf(stderr,
            "Failed to open config: %s", szConfig);
    fflush(stderr);
    CFSRPS_Close(&pRepo);
    goto CSAPBRKR_END;
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "SECURE_CONFIG")) != NULL) {

    pEnv = CFS_OpenEnv(pszParam);

    if (pEnv == NULL) {
      fprintf(stderr, "Failed to open secure environment: %s", szConfig);
      fflush(stderr);
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      goto CSAPBRKR_END;
    }
    fprintf(stderr, "Connection is secured: %s", pszParam);
    fflush(stderr);
  }
  else {
    pEnv = CFS_OpenEnv(szConfig);
    fprintf(stderr, "Connection is not secured");
    fflush(stderr);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "USER_LOGIN")) != NULL) {
    if (!strcmp(pszParam, "*NO")) {
      login = 0;
      fprintf(log, "Login mode == FALSE: %s", pszParam);
      fflush(stderr);
    }
    else {
      login = 1;
      fprintf(stderr, "Login mode == TRUE: %s", pszParam);
      fflush(stderr);
    }
  }
  else {
    login = 0;
    fprintf(stderr, "Login mode == FALSE: %s", pszParam);
    fflush(stderr);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "SERVICE_MODE")) == NULL) {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    fprintf(stderr, "Missing service mode");
    fflush(stderr);
    goto CSAPBRKR_END;
  }

  if (!strcmp(pszParam, "*SINGLE")) {

    fprintf(stderr, "Running in *SINGLE mode");
    fflush(stderr);

    iServiceMode = CSAP_SERVICEMODE_SINGLE_LOAD;

    if (CFSCFG_IterStart(pConfig, "SERVICES") == CS_SUCCESS) {

      // Fetch the first service in sequence from
      // the SERVICES enumeration

      while ((pszParam = CFSCFG_IterNext(pConfig)) != NULL) {
        fprintf(stderr, "Getting service export from %s", pszParam);
        fflush(stderr);
        CSAP_ServiceHandler = CSAP_GetServiceExport(pszParam);
        // Get first entry only
        break;
      }

      if (CSAP_ServiceHandler == NULL) {
        fprintf(stderr, "Failed to load service handler");
        fflush(stderr);
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

          ServiceInfo.CSAP_ServiceHandler = CSAP_GetServiceExport(pszParam);

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

  stream_fd = 0; //atoi(argv[1]);

  /////////////////////////////////////////////////////////////////////
  // Try to send parent a byte; this indicates we are ready
  // to handle a client...
  /////////////////////////////////////////////////////////////////////

  send(stream_fd, &buffer, 1, 0);

  for (;;) {

    /////////////////////////////////////////////////////////////////////
    // The main daemon will eventually hand over the connection socket
    // needed to communicate with a client via the IPC descriptor.
    /////////////////////////////////////////////////////////////////////

    hResult = CFS_ReceiveDescriptor(stream_fd, &conn_fd, -1);

    if (CS_SUCCEED(hResult))
    {
      // Note that we are accessing
      // the CSAP::pSession private member. We can do this
      // because actually, the CSAP Broker is part of the
      // CSAP implementation and as such, CSAPBROKER is
      // a friend of the CSAP class.

      if (CS_SUCCEED(hResult = CSWSCK_OpenChannel
                                        (pCSAP->pSession,
                                         pEnv,
                                         conn_fd))) {

        if (CS_DIAG(hResult) == CSWSCK_DIAG_WEBSOCKET) {

          switch(iServiceMode) {
            case CSAP_SERVICEMODE_SINGLE_LOAD:
              CSAP_Broker_Single(pCSAP, szService, login, CSAP_ServiceHandler);
              break;
            case CSAP_SERVICEMODE_ENUMERATION_LOAD:
              CSAP_Broker_Enum(pCSAP, pServices, login);
              break;
            case CSAP_SERVICEMODE_DYNAMIC_LOAD:
              CSAP_Broker(pCSAP, login);
              break;
          }
        }
        else {
        }

        CSWSCK_CloseChannel(pCSAP->pSession, 0, 0);
      }
      else {
      }
    }
    else {
    }

    send(stream_fd, &buffer, 1, 0);
  }

  close(stream_fd);

  CSAPBRKR_END:

  CSAP_Destructor(&pCSAP);
  CSMAP_Destructor(&pServices);

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
//  This function activates a service program and
//  retrieves a pointer from an exported procedure
//
/////////////////////////////////////////////////////////////////////////////

CSAP_SERVICEHANDLERPROC
  CSAP_GetServiceExport
    (char* pServiceConfig) {

  int type;

  char szLibraryName[11];
  char szSrvPgmName[11];
  char szInProcHandler[129];

  CFSRPS pRepo;
  CFSCFG pConfig;

  _SYSPTR pSrvPgm;  // OS400 System pointer

  // Pointer to CSAP service handler function exported by service program.

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  pRepo = CFSRPS_Open(0);
  pConfig = CFSRPS_OpenConfig(pRepo, pServiceConfig);

  if (pConfig != NULL) {

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
    (CSAP* pCSAP,
     char* szService,
     int login,
     CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler) {


  char* pHandshake;
  char szBuffer[1025];
  char szUser[10];
  char szPassword[513];
  char szProfileHandle[12];
  char cpf[8];
  char errInfo[16];

  int bytesProvided;
  int bytesAvail;
  int pwdSize;

  uint64_t size;

  if (CS_SUCCEED(CSWSCK_Receive(pCSAP->pSession, &size))) {

    if (size > g_CurHandsakeSlab) {
      free(pHandshakeIn);
      pHandshakeIn = (char*)malloc((size + 1) * sizeof(char));
      g_CurHandsakeSlab = size;
    }

    CSWSCK_GetData(pCSAP->pSession, pHandshakeIn, 0, size);

    pHandshakeIn[size] = 0;

    CSJSON_Init(pCSAP->pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pCSAP->pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    if (CS_SUCCEED(CSJSON_Parse(pCSAP->pJsonIn, pHandshakeIn, 0))) {

      if (login == 1) {
        // authenticate user
        // get user id
        if (CS_FAIL(ValidateUSerProfile(pCSAP, szUser))) {
          return CS_FAILURE;
        }
      }
      else {
        szUser[0] = 0;
      }

      // get service name
      if (CS_SUCCEED(CSJSON_LookupKey(pCSAP->pJsonIn,
                                      "/", "service",
                                      &(pCSAP->lse)))) {

        if (strcmp(pCSAP->lse.szValue, szService) == 0) {

          // Create a session ID

          CSSYS_MakeUUID(pCSAP->szSessionID,
                     CSSYS_UUID_LOWERCASE | CSSYS_UUID_DASHES);

          CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "version", "7.0");
          CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "status", "000");
          CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "reason", "0000000000");
          CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "sid", pCSAP->szSessionID);

          size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pCSAP->pSession,
                      CSWSCK_OPER_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          CSAP_ServiceHandler(pCSAP, szUser);

        }
        else {

          fprintf(stdout,
                  "\nERROR: Requested Service %s not supported\n",
                  pCSAP->lse.szValue);
          fflush(stdout);

          CSJSON_InsertString(pCSAP->pJsonOut,
                            "/handshake", "version", "7.0");
          CSJSON_InsertString(pCSAP->pJsonOut,
                            "/handshake", "status", "101");
          CSJSON_InsertString(pCSAP->pJsonOut,
                            "/handshake", "reason", "1000010011");
          CSJSON_InsertString(pCSAP->pJsonOut,
                            "/handshake", "sid", CSAP_NULL_SESSION);

          size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

          CSWSCK_Send(pCSAP->pSession,
                      CSWSCK_OPER_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        fprintf(stdout,
                "\nERROR: No specified service in request\n");
        fflush(stdout);

        CSJSON_InsertString(pCSAP->pJsonOut,
                          "/handshake", "version", "7.0");
        CSJSON_InsertString(pCSAP->pJsonOut,
                          "/handshake", "status", "101");
        CSJSON_InsertString(pCSAP->pJsonOut,
                          "/handshake", "reason", "1000010015");
        CSJSON_InsertString(pCSAP->pJsonOut,
                          "/handshake", "sid", CSAP_NULL_SESSION);

        size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

        CSWSCK_Send(pCSAP->pSession,
                    CSWSCK_OPER_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error

      fprintf(stdout,
            "\nERROR: Handshake segment format error\n");
      fflush(stdout);

      CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "version", "7.0");
      CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "status", "101");
      CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "reason", "1000010062");
      CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "sid", CSAP_NULL_SESSION);

      size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

      CSWSCK_Send(pCSAP->pSession,
                  CSWSCK_OPER_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client

    fprintf(stdout,
            "\nERROR: Reading client service request failed\n");
    fflush(stdout);

    CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "version", "7.0");
    CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "status", "101");
    CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "reason", "1000010061");
    CSJSON_InsertString(pCSAP->pJsonOut,
                        "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pCSAP->pSession,
                CSWSCK_OPER_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

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
    (CSAP* pCSAP,
     CSMAP* Services,
     int login) {

  return CS_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////
//
// CSAP Broker over websocket (*ANY)
//
/////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSAP_Broker
    (CSAP* pCSAP,
     int login) {

  return CS_FAILURE;
}

/* --------------------------------------------------------------------------
  signalCatcher
-------------------------------------------------------------------------- */

void signalCatcher(int signal)
{

  switch (signal)
  {
    case SIGTERM:

      CFS_CloseChannel(pCSAP->pSession);
      close(stream_fd);
      //dlclose(inprocServer);

      break;
  }

  return;
}

CSRESULT
  ValidateUSerProfile
    (CSAP* pCSAP,
     char* szUser) {

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

  if (CS_FAIL(CSJSON_LookupKey(pCSAP->pJsonIn,
                                "/", "u",
                                &(pCSAP->lse)))) {

    fprintf(stdout,
            "\nERROR: Invalid User Credentail 1000010111\n");
    fflush(stdout);

    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "version", "7.0");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "status", "701");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "reason", "1000010111");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pCSAP->pSession,
                CSWSCK_OPER_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  memset(szUser, ' ', 10);
  memcpy(szUser, pCSAP->lse.szValue, strlen(pCSAP->lse.szValue));

  // get password
  if (CS_FAIL(CSJSON_LookupKey(pCSAP->pJsonIn,
                                "/", "p",
                                &(pCSAP->lse)))) {

    sprintf(szBuffer,
        "\nERROR: Invalid User Credentail 1000010222: User: %s\n",
        pCSAP->lse.szValue);

    fprintf(stdout,
            szBuffer);
    fflush(stdout);

    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "version", "7.0");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "status", "701");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "reason", "1000010222");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pCSAP->pSession,
                CSWSCK_OPER_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  pwdSize = strlen(pCSAP->lse.szValue);

  if (pwdSize > 512) {
    pCSAP->lse.szValue[512] = 0;
    pwdSize = 512;
  }

  memset(errInfo, ' ', 16);
  memset(szProfileHandle, ' ', 12);

  bytesProvided = 16;
  memcpy(errInfo, &bytesProvided, sizeof(int));

  QSYGETPH(szUser,
            pCSAP->lse.szValue,
            (  void*)szProfileHandle,
            errInfo,
            pwdSize,
            0);

  memcpy(cpf, errInfo + 8, 7);
  cpf[7] = 0;

  if (strcmp("       ", cpf)) {

    sprintf(szBuffer,
        "\nERROR: User authentication failure 1000010333: Exception: %s\n",
        cpf);

    fprintf(stdout, szBuffer);
    fflush(stdout);

    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "version", "7.0");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "status", "701");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "reason", "1000010333");
    CSJSON_InsertString(pCSAP->pJsonOut,
                      "/handshake", "sid", CSAP_NULL_SESSION);

    size = CSJSON_Serialize(pCSAP->pJsonOut, "/", &pHandshake, 0);

    CSWSCK_Send(pCSAP->pSession,
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
  CSWSCK_CloseChannel(pCSAP->pSession, 0, 0);
  close(stream_fd);
}

