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

#define _GNU_SOURCE     /* for basename( ) in <string.h> */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE   /* Get crypt() declaration from <unistd.h> */
#endif

#define __LOG 1

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <shadow.h>
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

typedef struct tagCSAPSTAT {

  char szVersion[11];
  char szStatus[4];
  char szReason[11];
  char szSessionID[37];

} CSAPSTAT;

typedef struct tagCSAPCTL {

  uint64_t UsrCtlSize;
  uint64_t DataSize;

} CSAPCTL;

typedef struct tagCSAP {

  CSWSCK pSession;

  CSJSON pJsonIn;
  CSJSON pJsonOut;

  CSJSON_LSENTRY lse;

  char szSessionID[37];

  long inDataSize;
  long outDataSize;
  long outDataSlabSize;
  long UsrCtlSize;
  long UsrCtlSlabSize;
  long fmt;

  char* pUsrCtlSlab;
  char* pInDataSlab;
  char* pOutDataSlab;

  CSLIST OutDataParts;

  CFSRPS pRepo;
  CFSRPS pConfig;

  CSAPCTL ctl;

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
     CSAPCTL* pCtlFrame,
     long toSlices);

CSRESULT
  CSAP_Send
    (CSAP* This,
     char* szSessionID,
     char* pUsrCtl,
     long Size,
     long toSlices,
     long Fmt);

void signalCatcher(int signal);

typedef void
  (*CSAP_SERVICEHANDLERPROC)
    (CSAP* pCSAP,
     char* szUser);

CSAP_SERVICEHANDLERPROC
  CSAP_GetServiceExport
    (CSMAP inprocServerMap, 
     char* pServiceConfig);

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

typedef struct tagSERVICEINFOSTRUCT {

  char *szService;
  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

} SERVICEINFOSTRUCT;

int conn_fd;
int stream_fd;

char szLibraryName[256];
char szSrvPgmName[256];
char szInProcHandler[256];

char* pHandshakeIn;

long g_CurHandsakeSlab;
long iServiceMode;

CSAP*  pCSAP;
CSMAP inprocServerMap;
CFSENV pEnv;

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

  pEnv = NULL;
  pRepo = NULL;


  #if __LOG
  openlog(basename(argv[0]), LOG_PID, LOG_LOCAL3);
  #endif

  sa.sa_handler = signalCatcher;
  sa.sa_flags = 0; // or SA_RESTART
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = 0;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  inprocServerMap = CSMAP_Constructor();

  // Get broker repository configuration (making sure no more than 
  // CFSRPS_PATH_MAXBUFF chars are copied from the outside)

  size = strlen(argv[3]);

  if (size > 255) {
    size = 255;
  }

  memcpy(szConfig, argv[3], size);
  szConfig[size] = 0; // make sure we have a null

  #if __LOG
  syslog(LOG_INFO, "CSAP Broker starting - config: %s", szConfig);
  #endif

  pCSAP     = CSAP_Constructor();
  pServices = CSMAP_Constructor();

  g_CurHandsakeSlab = 1024;
  pHandshakeIn = (char*)malloc(1025 * sizeof(char)); // one more for NULL

  pRepo = CFSRPS_Open(0);

  if (pRepo == NULL) {
    #if __LOG
    syslog(LOG_ERR, "Failed to open repository");
    #endif
    goto CSAPBRKR_END;
  }

  if ((pConfig = CFSRPS_OpenConfig(pRepo, szConfig)) == NULL) {
    CFSRPS_Close(&pRepo);
     #if __LOG
    syslog(LOG_ERR, "Failed to load configuration");
    #endif
    goto CSAPBRKR_END;
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "SECURE_CONFIG")) != NULL) {

    #if __LOG
    syslog(LOG_INFO, "Session mode: SECURE");
    #endif

    pEnv = CFS_OpenEnv(pszParam);

    if (pEnv == NULL) {
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      #if __LOG
      syslog(LOG_ERR, "Failed to initialize secure environment");
      #endif
      goto CSAPBRKR_END;
    }
  }
  else {
    #if __LOG
    syslog(LOG_INFO, "Session mode: NON-SECURE");
    #endif
    pEnv = CFS_OpenEnv(szConfig);

    if (pEnv == NULL) {
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      #if __LOG
      syslog(LOG_ERR, "Failed to initialize environment");
      #endif
      goto CSAPBRKR_END;
    }
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

  #if __LOG
  syslog(LOG_INFO, "Login mode: %d", login);
  #endif

  if ((pszParam = CFSCFG_LookupParam(pConfig, "SERVICE_MODE")) == NULL) {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    #if __LOG
    syslog(LOG_ERR, "Broker service mode not found");
    #endif
    goto CSAPBRKR_END;
  }

  #if __LOG
  syslog(LOG_INFO, "Service mode: %s", pszParam);
  #endif

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
        #if __LOG
        syslog(LOG_ERR, "Service handler not loaded: %s", pszParam);
        #endif
        goto CSAPBRKR_END;
      }

      strcpy(szService, pszParam);
    }
    else {
      CFSRPS_CloseConfig(pRepo, &pConfig);
      CFSRPS_Close(&pRepo);
      #if __LOG
      syslog(LOG_ERR, "Dedicated service not found");
      #endif
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

  stream_fd = atoi(argv[1]);

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

      pCSAP->inDataSize = 0;
      pCSAP->outDataSize = 0;
      pCSAP->UsrCtlSize = 0;

      if (CS_SUCCEED(hResult = CSWSCK_OpenChannel
                                        (pCSAP->pSession, 
                                         pEnv,
                                         conn_fd))) {

        if (CS_DIAG(hResult) == CSWSCK_DIAG_WEBSOCKET) {

          switch(iServiceMode) {
            case CSAP_SERVICEMODE_SINGLE_LOAD:
              #if __LOG
              syslog(LOG_INFO, "Connection received - single broker mode");
              #endif
              CSAP_Broker_Single(pCSAP, 
                                 szService, login, CSAP_ServiceHandler);
              #if __LOG
              syslog(LOG_INFO, "Connection terminated - single broker mode");
              #endif
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
          #if __LOG
          syslog(LOG_ERR, "Invalid Broker service mode");
          #endif
        }

        CSWSCK_CloseChannel(pCSAP->pSession, 0, 0);
      }
      else {
        #if __LOG
        syslog(LOG_ERR, "Failed open websocket channel");
        #endif
      }
    }
    else {
      #if __LOG
      syslog(LOG_ERR, "Failed to receive descriptor from mnain daemon");
      #endif
    }

    send(stream_fd, &buffer, 1, 0);
  }

  close(stream_fd);

  // Close shared libraries
  CSMAP_IterStart(inprocServerMap, CSMAP_ASCENDING);

  while (CS_SUCCEED(CSMAP_IterNext(inprocServerMap, &pszKey,
                                    (void **)(&pInprocServer), &valueSize)))
  {
    dlclose(pInprocServer);
  }

  CSAPBRKR_END:

  CSAP_Destructor(&pCSAP);
  CSMAP_Destructor(&pServices);
  CSMAP_Destructor(inprocServerMap);
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
    (CSMAP inprocServerMap,
     char* pServiceConfig) {

  char* pszParam;

  void *inprocServer;

  CFSRPS pRepo;
  CFSCFG pConfig;

  // Pointer to CSAP service handler function exported by service program.

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  pRepo = CFSRPS_Open(0);

  if ((pConfig = CFSRPS_OpenConfig(pRepo, pServiceConfig)) == NULL) {
    syslog(LOG_ERR, "Handler configuration not found");
    CFSRPS_Close(&pRepo);
    return NULL;
  }

  if (( pszParam = CFSCFG_LookupParam(pConfig, "INPROCSERVER")) == NULL) {
    syslog(LOG_ERR, "Failed to read config parameter: INPROCHANDLER_SRV");    
    CSAP_ServiceHandler = NULL;
    goto CSAP_SERVICEHANDLERPROC_END;
  }

  inprocServer = dlopen(pszParam, RTLD_NOW);
  
  if (inprocServer == NULL)
  {
    syslog(LOG_ERR, "Failed to load shared library - %s", dlerror());
    CSAP_ServiceHandler = NULL;
    goto CSAP_SERVICEHANDLERPROC_END;
  }

  CSMAP_Insert(inprocServerMap, pszParam, 
                    &inprocServer, sizeof(inprocServer));

  if ((pszParam = CFSCFG_LookupParam(pConfig, "INPROCHANDLER")) == NULL) {
    syslog(LOG_ERR, "Failed to read config parameter: INPROCHANDLER_EXPORT");
    dlclose(inprocServer);
    CSAP_ServiceHandler = NULL;
    goto CSAP_SERVICEHANDLERPROC_END;
  }

  CSAP_ServiceHandler = dlsym(inprocServer, pszParam);

  if (CSAP_ServiceHandler == NULL)
  {
    syslog(LOG_ERR, "Failed to get service proc address - %s", dlerror());
    dlclose(inprocServer);
  }

  CSAP_SERVICEHANDLERPROC_END:

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

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

  uint64_t size;

  if (CS_SUCCEED(CSWSCK_ReceiveAll(pCSAP->pSession, 
                                &size, 
                                1))) {
    
    CSJSON_Init(pCSAP->pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pCSAP->pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    //if (CS_SUCCEED(CSJSON_Parse(pCSAP->pJsonIn, pHandshakeIn, 0))) {
    if (CS_SUCCEED(CSJSON_Parse(pCSAP->pJsonIn, CSWSCK_GetDataRef(pCSAP->pSession), 0))) {

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
                      CSWSCK_OP_TEXT,
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

          syslog(LOG_ERR, 
                 "Requested Service %s not supported", pCSAP->lse.szValue);

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
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        syslog(LOG_ERR, "No specified service in request");

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
                    CSWSCK_OP_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error
      syslog(LOG_ERR, 
             "Handshake segment format error: %s", pCSAP->pInDataSlab);

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
                  CSWSCK_OP_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;
    }
  }
  else {

    // Error reading client
    syslog(LOG_ERR, "Reading client service request failed");
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

  long bytes;

  uint64_t size;

  SERVICEINFOSTRUCT* ServiceInfo;

  if (CS_SUCCEED(CSWSCK_ReceiveAll(pCSAP->pSession, 
                                &size, 
                                1))) {

    CSJSON_Init(pCSAP->pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pCSAP->pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    if (CS_SUCCEED(CSJSON_Parse(pCSAP->pJsonIn, CSWSCK_GetDataRef(pCSAP->pSession), 0))) {

      if (login == 1) {
        // authenticate user
        // get user id
        if (CS_FAIL(ValidateUserProfile(pCSAP, szUser))) {
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
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          CSAP_Clear(pCSAP);

          /////////////////////////////////////////////
          // Run the requested service.
          /////////////////////////////////////////////

          ServiceInfo->CSAP_ServiceHandler(pCSAP, szUser);

        }
        else {

          syslog(LOG_ERR, 
                 "Requested Service %s not supported", pCSAP->lse.szValue);

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
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        syslog(LOG_ERR, "No specified service in request");

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
                    CSWSCK_OP_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error

      // Handshake parse error
      syslog(LOG_ERR, "Handshake segment format error");

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
                  CSWSCK_OP_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client
    syslog(LOG_ERR, "Failed reading client service request");
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

  uint64_t size;

  CSAP_SERVICEHANDLERPROC CSAP_ServiceHandler;

  if (CS_SUCCEED(CSWSCK_ReceiveAll(pCSAP->pSession, 
                                &size,
                                1))) {

    CSJSON_Init(pCSAP->pJsonOut, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pCSAP->pJsonOut, "/", "handshake", JSON_TYPE_OBJECT);

    if (CS_SUCCEED(CSJSON_Parse(pCSAP->pJsonIn, CSWSCK_GetDataRef(pCSAP->pSession), 0))) {

      if (login == 1) {
        // authenticate user
        // get user id
        if (CS_FAIL(ValidateUserProfile(pCSAP, szUser))) {
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
                  CSAP_GetServiceExport(inprocServerMap, pCSAP->lse.szValue);

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
                      CSWSCK_OP_TEXT,
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

          syslog(LOG_ERR, 
                 "Requested Service %s not supported", pCSAP->lse.szValue);

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
                      CSWSCK_OP_TEXT,
                      pHandshake,
                      size,
                      CSWSCK_FIN_ON);

          return CS_FAILURE;
        }
      }
      else {

        syslog(LOG_ERR, "No specified service in request");

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
                    CSWSCK_OP_TEXT,
                    pHandshake,
                    size,
                    CSWSCK_FIN_ON);

        return CS_FAILURE;
      }
    }
    else {

      // Handshake parse error
      syslog(LOG_ERR, "Handshake segment format error");

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
                  CSWSCK_OP_TEXT,
                  pHandshake,
                  size,
                  CSWSCK_FIN_ON);

      return CS_FAILURE;

    }
  }
  else {

    // Error reading client
    syslog(LOG_ERR, "Reading client service request failed");
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

CSRESULT
  PRV_ValidateUserProfile
    (char* username, 
     char* password) {

  char *encrypted, *p;
  struct passwd *pwd;
  struct spwd *spwd;

  /* Look up password and shadow password records for username */

  pwd = getpwnam(username);

  if (pwd == NULL)
    return CS_FAILURE;

  spwd = getspnam(username);
  if (spwd == NULL && errno == EACCES)
    return CS_FAILURE;

  if (spwd != NULL)           /* If there is a shadow password record */
    pwd->pw_passwd = spwd->sp_pwdp;     /* Use the shadow password */

  password = getpass("Password: ");

  /* Encrypt password and erase cleartext version immediately */

  encrypted = crypt(password, pwd->pw_passwd);
  for (p = password; *p != '\0'; )
    *p++ = '\0';

  if (encrypted == NULL)
    return CS_FAILURE;

  if (strcmp(encrypted, pwd->pw_passwd) != 0) {
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}

CSRESULT
  ValidateUserProfile
    (CSAP* pCSAP,
     char* szUser) {

  char* pHandshake;

  char szPassword[256];

  uint64_t size;

  // authenticate user
  // get user id

  if (CS_FAIL(CSJSON_LookupKey(pCSAP->pJsonIn,
                                "/", "u",
                                &(pCSAP->lse)))) {

    syslog(LOG_ERR, "Invalid User Credential");

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
                CSWSCK_OP_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  strncpy(szUser, pCSAP->lse.szValue, 255);
  szUser[255]= 0;

  // get password
  if (CS_FAIL(CSJSON_LookupKey(pCSAP->pJsonIn,
                                "/", "p",
                                &(pCSAP->lse)))) {

    syslog(LOG_ERR, "Invalid User Credential");

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
                CSWSCK_OP_TEXT,
                pHandshake,
                size,
                CSWSCK_FIN_ON);

    return CS_FAILURE;
  }

  strncpy(szPassword, pCSAP->lse.szValue, 255);
  szPassword[255]= 0;

/*
  authenticate user here
*/
  if (PRV_ValidateUserProfile(szUser, szPassword)) {
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

/* --------------------------------------------------------------------------
  signalCatcher
-------------------------------------------------------------------------- */

void signalCatcher(int signal)
{
  long valueSize;

  char* pszKey;

  void* pInprocServer;

  switch (signal)
  {
    case SIGTERM:

      if (pCSAP->pSession != NULL) {
        CSWSCK_CloseChannel(pCSAP->pSession, 0, 0);
      }

      CFS_CloseEnv(&pEnv);
      close(stream_fd);
      syslog(LOG_INFO, "SIGTERM received - Handler existing");
      closelog();

      // Close shared libraries
      CSMAP_IterStart(inprocServerMap, CSMAP_ASCENDING);

      while (CS_SUCCEED(CSMAP_IterNext(inprocServerMap, &pszKey,
                                    (void **)(&pInprocServer), &valueSize)))
      {
        dlclose(pInprocServer);
      }

      break;
  }

  return;
}
