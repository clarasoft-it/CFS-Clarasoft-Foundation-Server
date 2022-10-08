/* ==========================================================================

  Clarasoft Foundation Server

  generic echo server
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

#define _GNU_SOURCE 

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>

#include <clarasoft/cfs.h>

typedef void (*INPROCHANDLER)(CSWSCK pSession);

void* pInprocServer;

INPROCHANDLER pInprocHandler;


int conn_fd;
int stream_fd;
time_t current_time;
struct tm *local_time;
pid_t pid;
FILE* stream;
time_t rawtime;
struct tm * timeinfo;

CSWSCK pSession;

void signalCatcher(int signal);

int main(int argc, char **argv)
{
  char buffer = 0; // dummy byte character
                   // to send to main daemon

  char* pszParam;

  char szConfig[99];

  struct sigaction sa;

  CSRESULT hResult;

  CFSENV pEnv;
  CFSRPS pRepo;
  CFSCFG pConfig;

  sa.sa_handler = signalCatcher;
  sa.sa_flags = 0; // or SA_RESTART
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = 0;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  pid = getpid();

  pRepo = CFSRPS_Open(0);

  strncpy(szConfig, argv[3], 99);
  szConfig[98] = 0;

  if ((pConfig = CFSRPS_OpenConfig(pRepo, szConfig)) == NULL) {
    CFSRPS_Close(&pRepo);
    exit(2);
  }

  if ((pszParam = CFSRPS_LookupParam(pConfig, "ENV")) == NULL) {
    // use default environment
    pEnv = CFS_OpenEnv(0);
  }
  else {
    pEnv = CFS_OpenEnv(pszParam);
  }

  if ((pszParam = CFSRPS_LookupParam(pConfig, "INPROCSERVER")) == NULL) {
    CFSRPS_CloseConfig(&pConfig);
    CFSRPS_Close(&pRepo);
    exit(3);
  }

  pInprocServer = dlopen(pszParam, RTLD_LAZY);

  if (pInprocServer) {

    if ((pszParam = CFSRPS_LookupParam(pConfig, "INPROCHANDLER")) == NULL) {
      CFSRPS_CloseConfig(&pConfig);
      CFSRPS_Close(&pRepo);
      exit(4);
    }

    pInprocHandler = dlsym(pInprocServer, pszParam);

    if (dlerror() != NULL)
    {
      CFSRPS_CloseConfig(&pConfig);
      CFSRPS_Close(&pRepo);
      dlclose(pInprocServer);
      exit(5);
    }

    CFSRPS_CloseConfig(&pConfig);

  }
  else {
    CFSRPS_CloseConfig(&pConfig);
    CFSRPS_Close(&pRepo);
    exit(6);
  }

  CFSRPS_CloseConfig(&pConfig);
  CFSRPS_Close(&pRepo);

  pSession = CSWSCK_Constructor();

  if (argv[2][0] == 'T')
  {
    // This is a transceint execution; the program will exit 
    // once the client terminates the connection

    conn_fd = atoi(argv[1]);
    CSWSCK_OpenChannel(pSession, pEnv, conn_fd);

    pInprocHandler(pSession); 

    CSWSCK_CloseChannel(pSession, 0, 0);
  }
  else
  {
    /////////////////////////////////////////////////////////////////////
    // Try to send parent a byte; this indicates we are ready
    // to handle a client...
    /////////////////////////////////////////////////////////////////////

    openlog(basename(argv[0]), LOG_PID, LOG_LOCAL3);
    syslog(LOG_ERR, "Starting resident handler");

    stream_fd = atoi(argv[1]);

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
        openlog(basename(argv[0]), LOG_PID, LOG_LOCAL3);

        syslog(LOG_ERR, "Receiving connection");

        if (CS_SUCCEED(CSWSCK_OpenChannel(pSession, pEnv, conn_fd))) {

          pInprocHandler(pSession); 
          syslog(LOG_ERR, "Closing connection");
          CSWSCK_CloseChannel(pSession, 0, 0);
        }

        //////////////////////////////////////////////////////////////////
        // Tell main daemon we can handle another connection
        //////////////////////////////////////////////////////////////////

        send(stream_fd, &buffer, 1, 0);
      }
    }
  }

  dlclose(pInprocServer);

  CFS_CloseEnv(&pEnv);

  CSWSCK_Destructor(&pSession);

  close(stream_fd);

  return 0;
}

/* --------------------------------------------------------------------------
  signalCatcher
-------------------------------------------------------------------------- */

void signalCatcher(int signal)
{
  switch (signal)
  {
    case SIGTERM:

      if (pSession != NULL) {
        CSWSCK_CloseChannel(pSession, 0, 0);
      }
      close(stream_fd);
      exit(0);
      break;
  }

  return;
}