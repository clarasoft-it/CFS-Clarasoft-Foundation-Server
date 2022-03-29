/* ==========================================================================

  Clarasoft Foundation Server

  CSAP Broker
  Version 7.0.0

  Compile with

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

#define _GNU_SOURCE     /* for basename( ) in <string.h> */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include <clarasoft/cfsapi.h>
#include <clarasoft/cswsck.h>
#include <clarasoft/cfsrepo.h>
#include <clarasoft/csjson.h>

#define CFS_DEF_SERVICENAME_MAX  (98)
#define CFS_DEF_LIBNAME_MAX      (256)
#define CFS_DEF_OBJNAME_MAX      (256)
#define CSAP_NULL_SESSION         "000000000000000000000000000000000000"

#define CSAP_SERVICEMODE_SINGLE_LOAD       1
#define CSAP_SERVICEMODE_ENUMERATION_LOAD  2
#define CSAP_SERVICEMODE_DYNAMIC_LOAD      3

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

  uint64_t OutDataSize;
  uint64_t InDataSize;
  uint64_t UsrCtlSize;
  uint64_t UsrCtlSlabSize;

  long fmt;

  char* pUsrCtlSlab;
  char* pInData;
  char* pOutData;

  CSLIST OutDataParts;

  CFSRPS repo;

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

typedef struct tagCSAPCTL {

  uint64_t UsrCtlSize;
  uint64_t DataSize;
  long fmt;

} CSAPCTL;

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

typedef void
  (*CSAP_SERVICEHANDLERPROC)
    (CSAP* pCSAP,
     char* szUser);

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
  ValidateUSerProfile
    (CSAP* pCSAP,
     char* szUser);

typedef struct tagSERVICEINFOSTRUCT {

  char *szService;
  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

} SERVICEINFOSTRUCT;

int conn_fd;
int stream_fd;

char szLibraryName[CFSRPS_PARAM_MAXBUFF];
char szSrvPgmName[CFSRPS_PARAM_MAXBUFF];
char szInProcHandler[CFSRPS_PARAM_MAXBUFF];

char* pHandshakeIn;

long g_CurHandsakeSlab;

volatile unsigned return_code;

CSAP*  pCSAP;

int main(int argc, char** argv)
{
  char buffer = 0; // dummy byte character
                   // to send to main daemon

  int rc;
  int login;

  long iServiceMode;

  char szConfig[CFSRPS_PATH_MAXBUFF];
  char szService[CFSRPS_PATH_MAXBUFF];

  struct sigaction sa;

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  CSRESULT hResult;

  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO pi;

  CFSRPS repo;
  CSMAP  pServices;
  CFSENV pEnv;

  SERVICEINFOSTRUCT ServiceInfo;

  return_code = 0;

  openlog(basename(argv[0]), LOG_PID, LOG_LOCAL3);

  sa.sa_handler = signalCatcher;
  sa.sa_flags = 0; // or SA_RESTART
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = 0;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);
  
  // Get broker repository configuration

  strncpy(szConfig, argv[3], CFSRPS_PATH_MAXBUFF);
  szConfig[CFSRPS_PATH_MAXBUFF-1] = 0; // make sure we have a null-terminated
                                       // string no longer than the buffer

  syslog(LOG_INFO, "CSAP BROKER starting - config: %s", szConfig);

  pCSAP     = CSAP_Constructor();
  repo      = CFSRPS_Constructor();
  pServices = CSMAP_Constructor();

  strcpy(ci.szPath, szConfig);

  g_CurHandsakeSlab = 1024;
  pHandshakeIn = (char*)malloc(1025 * sizeof(char)); // one more for NULL

  if (CS_FAIL(CFSRPS_LoadConfig(repo, &ci))) {
    syslog(LOG_INFO, "ERROR: Cannot load configuration");
    goto CSAPBRKR_END;
  }

  syslog(LOG_INFO, "CSAP BROKER config opened");

  if (CS_SUCCEED(CFSRPS_LookupParam(repo, "SECURE_CONFIG", &pi))) {

    syslog(LOG_INFO, "CSAP BROKER secure session mode");
    pEnv = CFS_OpenEnv(pi.szValue);

    if (pEnv == NULL) {
      syslog(LOG_INFO, "ERROR: Secure environment initialization failure");
      goto CSAPBRKR_END;
    }
  }
  else {
    syslog(LOG_INFO, "CSAP BROKER non-secure session mode");
    pEnv = NULL;
  }

  if (CS_SUCCEED(CFSRPS_LookupParam(repo, "USER_LOGIN", &pi))) {
    if (!strcmp(pi.szValue, "*NO")) {
      login = 0;
    }
    else {
      login = 1;
    }
  }
  else {
    login = 1;
  }

  syslog(LOG_INFO, "CSAP BROKER login mode: %d", login);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "SERVICE_MODE", &pi))) {
    syslog(LOG_INFO, "ERROR: Broker service mode not found");
    goto CSAPBRKR_END;
  }

  syslog(LOG_INFO, "CSAP BROKER service mode: %s", pi.szValue);

  if (!strcmp(pi.szValue, "*SINGLE")) {

    iServiceMode = CSAP_SERVICEMODE_SINGLE_LOAD;

    if (CFSRPS_IterStart(repo, "SERVICES") == CS_SUCCESS) {

      // Fetch the first service in sequence from
      // the SERVICES enumeration

      while (CFSRPS_IterNext(repo, &pi) == CS_SUCCESS) {

        CSAP_ServiceHandler =
                CSAP_GetServiceExport(pi.szValue);
        // Get first entry only
        break;
      }

      if (CSAP_ServiceHandler == NULL) {

        syslog(LOG_INFO, "ERROR: Service handler not loaded: %s", pi.szValue);
        goto CSAPBRKR_END;
      }

      strcpy(szService, pi.szValue);
    }
    else {
      syslog(LOG_INFO, "ERROR: Dedicated service not found");
      goto CSAPBRKR_END;
    }
  }
  else {

    if (!strcmp(pi.szValue, "*ENUM")) {

      // limit the available services to a set (enumeration) of services

      iServiceMode = CSAP_SERVICEMODE_ENUMERATION_LOAD;

      if (CFSRPS_IterStart(repo, "SERVICES") == CS_SUCCESS) {

        // Fetch the service handlers from
        // the SERVICES enumeration and store them in
        // the service map

        while (CFSRPS_IterNext(repo, &pi) == CS_SUCCESS) {

          ServiceInfo.CSAP_ServiceHandler =
                         CSAP_GetServiceExport(pi.szValue);

          if (ServiceInfo.CSAP_ServiceHandler != 0) {

            CSMAP_Insert(pServices,
                         pi.szValue,
                         &ServiceInfo,
                         sizeof(SERVICEINFOSTRUCT));
          }
        }
      }
    }
    else {

      if (!strcmp(pi.szValue, "*ANY")) {

        iServiceMode = CSAP_SERVICEMODE_DYNAMIC_LOAD;
      }
    }
  }  

  if (argv[2][0] == 'T')
  {
    // This handler is transcient and will terminate once
    // the connection is ended. Note that we are accessing
    // the CSAP::pSession private member. We can do this
    // because actually, the CSAP Broker is part of the
    // CSAP implementation and as such, CSAPBROKER is
    // a friend of the CSAP class.

    conn_fd = atoi(argv[1]);

    if (CS_SUCCEED(hResult = CSWSCK_OpenChannel
                                 (pCSAP->pSession,
                                  pEnv,
                                  szConfig,
                                  conn_fd, &rc))) {

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
        syslog(LOG_INFO, "ERROR: Invalid Broker service mode");
      }

      CSWSCK_CloseChannel(pCSAP->pSession, 0, 0, 30);
    }
  }
  else {

    stream_fd = atoi(argv[1]);

    /////////////////////////////////////////////////////////////////////
    // Try to send parent a byte; this indicates we are ready
    // to handle a client...
    /////////////////////////////////////////////////////////////////////

    send(stream_fd, &buffer, 1, 0);

    for (;;)
    {
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

        syslog(LOG_INFO, "CSAP BROKER - %s - connection received", szConfig);
        
        if (CS_SUCCEED(hResult = CSWSCK_OpenChannel
                                          (pCSAP->pSession, 
                                          NULL,
                                          szConfig, 
                                          conn_fd, 
                                          &rc))) {

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

            CSWSCK_CloseChannel(pCSAP->pSession, 0, 0, 30);
          }
          else {
            syslog(LOG_INFO, "ERROR: Invalid Broker service mode");
          }
        }
        else {
          syslog(LOG_INFO, "ERROR: Could not open websocket channel");
        }
      }
      else {
        syslog(LOG_INFO, "ERROR: Could not receive descriptor from mnain daemon");
      }

      send(stream_fd, &buffer, 1, 0);
    }
  }

  close(stream_fd);

  CSAPBRKR_END:

  CSAP_Destructor(&pCSAP);
  CSMAP_Destructor(&pServices);
  CFSRPS_Destructor(&repo);

  closelog();

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

  void *inprocServer;

  CFSRPS repo;
  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO pi;

  // Pointer to CSAP service handler function exported by service program.

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  repo = CFSRPS_Constructor();

  strcpy(ci.szPath, pServiceConfig);

  if (CS_FAIL(CFSRPS_LoadConfig(repo, &ci))) {
    syslog(LOG_INFO, "ERROR: Handler configuration not found");
    CSAP_ServiceHandler = NULL;
    goto CSAP_GetServiceExport_END;
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCHANDLER_SRV", &pi))) {
    syslog(LOG_INFO, "ERROR: Cannot read config parameter: INPROCHANDLER_SRV");    return NULL;
    CSAP_ServiceHandler = NULL;
    goto CSAP_GetServiceExport_END;
  }

  inprocServer = dlopen(pi.szValue, RTLD_NOW);
  
  if (inprocServer == NULL)
  {
    syslog(LOG_INFO, "ERROR: Cannot load shared library - %s", dlerror());
    dlclose(inprocServer);
    goto CSAP_GetServiceExport_END;
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCHANDLER_EXPORT", &pi))) {
    syslog(LOG_INFO, "ERROR: Cannot read config parameter: INPROCHANDLER_EXPORT");
    CSAP_ServiceHandler = NULL;
    goto CSAP_GetServiceExport_END;
  }

  CSAP_ServiceHandler = dlsym(inprocServer, pi.szValue);

  if (CSAP_ServiceHandler == NULL)
  {
    syslog(LOG_INFO, "ERROR: Cannot get service proc address - %s", dlerror());
    goto CSAP_GetServiceExport_END;
  }

  CSAP_GetServiceExport_END:

  CFSRPS_Destructor(&repo);
  return CSAP_ServiceHandler;
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
  char szUser[256];
  //char szPassword[513];

  uint64_t size;

  if (CS_SUCCEED(CSWSCK_Receive(pCSAP->pSession, &size, -1))) {

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
                      CSWSCK_FIN_ON,
                      -1);

          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          CSAP_ServiceHandler(pCSAP, szUser);
        }
        else {

          syslog(LOG_INFO, "CSAP BROKER - ERROR: Requested Service %s not supported", pCSAP->lse.szValue);

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
                      CSWSCK_FIN_ON,
                      -1);

          return CS_FAILURE;
        }
      }
      else {

        syslog(LOG_INFO, "CSAP BROKER - ERROR: No specified service in request");

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
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error
      syslog(LOG_INFO, "CSAP BROKER - ERROR: Handshake segment format error");

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
                  CSWSCK_FIN_ON,
                  -1);

      return CS_FAILURE;
    }
  }
  else {

    // Error reading client
    syslog(LOG_INFO, "CSAP BROKER - ERROR: Reading client service request failed");
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

  char* pHandshake;
  char szUser[256];
  //char szPassword[513];

  long bytes;

  uint64_t size;

  SERVICEINFOSTRUCT* ServiceInfo;

  if (CS_SUCCEED(CSWSCK_Receive(pCSAP->pSession, &size, -1))) {

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

        // If requsted service is in the service map ...

        if (CS_SUCCEED(CSMAP_Lookup(Services,
                         pCSAP->lse.szValue,
                         (void**)&ServiceInfo, &bytes))) {

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
                      CSWSCK_FIN_ON,
                      -1);

          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          ServiceInfo->CSAP_ServiceHandler(pCSAP, szUser);

        }
        else {

          syslog(LOG_INFO, "CSAP BROKER - ERROR: Requested Service %s not supported", pCSAP->lse.szValue);

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
                      CSWSCK_FIN_ON,
                      -1);

          return CS_FAILURE;
        }
      }
      else {

        syslog(LOG_INFO, "CSAP BROKER - ERROR: No specified service in request");

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
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error

      // Handshake parse error
      syslog(LOG_INFO, "CSAP BROKER - ERROR: Handshake segment format error");

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
                  CSWSCK_FIN_ON,
                  -1);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client
    syslog(LOG_INFO, "CSAP BROKER - ERROR: Reading client service request failed");
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
    (CSAP* pCSAP,
     int login) {

  char* pHandshake;
  char szUser[256];
  //char szPassword[513];

  uint64_t size;

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  if (CS_SUCCEED(CSWSCK_Receive(pCSAP->pSession, &size, -1))) {

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

        CSAP_ServiceHandler =
                         CSAP_GetServiceExport(pCSAP->lse.szValue);

        if (CSAP_ServiceHandler != 0) {

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
                      CSWSCK_FIN_ON,
                      -1);

          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          CSAP_ServiceHandler(pCSAP, szUser);
        }
        else {

          syslog(LOG_INFO, "CSAP BROKER - ERROR: Requested Service %s not supported", pCSAP->lse.szValue);

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
                      CSWSCK_FIN_ON,
                      -1);

          return CS_FAILURE;
        }
      }
      else {

        syslog(LOG_INFO, "CSAP BROKER - ERROR: No specified service in request");

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
                    CSWSCK_FIN_ON,
                    -1);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error
      syslog(LOG_INFO, "CSAP BROKER - ERROR: Handshake segment format error");

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
                  CSWSCK_FIN_ON,
                  -1);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client
    syslog(LOG_INFO, "CSAP BROKER - ERROR: Reading client service request failed");
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

CSRESULT
  ValidateUSerProfile
    (CSAP* pCSAP,
     char* szUser) {

  char* pHandshake;

  //char szPassword[513];

  int pwdSize;

  uint64_t size;

  // authenticate user
  // get user id

  if (CS_FAIL(CSJSON_LookupKey(pCSAP->pJsonIn,
                                "/", "u",
                                &(pCSAP->lse)))) {

    syslog(LOG_INFO, "CSAP BROKER - ERROR: Invalid User Credential");

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
                CSWSCK_FIN_ON,
                -1);

    return CS_FAILURE;
  }

  memset(szUser, ' ', 10);
  memcpy(szUser, pCSAP->lse.szValue, strlen(pCSAP->lse.szValue));

  // get password
  if (CS_FAIL(CSJSON_LookupKey(pCSAP->pJsonIn,
                                "/", "p",
                                &(pCSAP->lse)))) {

    syslog(LOG_INFO, "CSAP BROKER - ERROR: Invalid User Credential");

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
                CSWSCK_FIN_ON,
                -1);

    return CS_FAILURE;
  }

  pwdSize = strlen(pCSAP->lse.szValue);

  if (pwdSize > 512) {
    pCSAP->lse.szValue[512] = 0;
    pwdSize = 512;
  }

/*
  authenticate user here
*/

  return CS_SUCCESS;
}

/* --------------------------------------------------------------------------
  signalCatcher
-------------------------------------------------------------------------- */

void signalCatcher(int signal)
{
  int rc;

  switch (signal)
  {
    case SIGTERM:

      CFS_CloseChannel(pCSAP->pSession, &rc);
      close(stream_fd);
      //dlclose(inprocServer);

      break;
  }

  return;
}

