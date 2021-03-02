/* ==========================================================================

  Clarasoft Foundation Server

  generic web-service handler (cfs)
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

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <clarasoft/cshttp.h>

int conn_fd;
int stream_fd;

CFS_SESSION *pInstance;

CSHTTP pHTTP;

// Laodable service procedure
typedef void (*SERVICEPROC)(CFS_SESSION*, char*, CSHTTP);

void signalCatcher(int signal);

int main(int argc, char **argv)
{

  void *inprocServer;

  char buffer = 0; // dummy byte character
                   // to send to main daemon

  int rc;

  struct sigaction sa;

  CSRESULT hResult;
  SERVICEPROC pServiceProc;

  CFSRPS repo;
  CFSRPS_CONFIGINFO cfgi;
  CFSRPS_PARAMINFO cfgpi;

  sa.sa_handler = signalCatcher;
  sa.sa_flags = 0; // or SA_RESTART
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = 0;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  repo = CFSRPS_Constructor();

  strcpy(cfgi.szPath, argv[3]);

  ///////////////////////////////////////////////////////////////////////
  // dynamically load exported function handler; load the service
  // configuration to get information on the handler function. 
  ///////////////////////////////////////////////////////////////////////

  if (CS_FAIL(CFSRPS_LoadConfig(repo, &cfgi)))
  {
    exit(1);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "INPROCSERVER", &cfgpi)))
  {
    exit(1);
  }

  inprocServer = dlopen(cfgpi.szValue, RTLD_LAZY);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "SERVICEPROC", &cfgpi)))
  {
    dlclose(inprocServer);
    exit(1);
  }

  pServiceProc = dlsym(inprocServer, cfgpi.szValue);

  if (dlerror() != NULL)
  {
    dlclose(inprocServer);
    exit(2);
  }

  CFSRPS_Destructor(&repo);

  if (argv[2][0] == 'T')
  {
    // This handler is transcient and will end once
    // the connectin is ended.

    // Create HTTP instance
    pHTTP = CSHTTP_Constructor();

    conn_fd = atoi(argv[1]);
    pInstance = CFS_OpenChannel(0, 0, conn_fd, &rc);

    pServiceProc(pInstance, argv[3], pHTTP);

    CFS_CloseChannel(pInstance, &rc);

    CSHTTP_Destructor(&pHTTP);

    return 0;
  }
  else
  {
    // This handler is resident and will not exist until parent dameon
    // ends.

    CFSRPS_Destructor(&repo);

    pInstance = CFS_OpenChannel(0, 0, 0, &rc);

    stream_fd = atoi(argv[1]);

    // Create HTTP instance
    pHTTP = CSHTTP_Constructor();

    /////////////////////////////////////////////////////////////////////
    // Try to send parent a byte; this indicates we are ready
    // to handle a client...
    /////////////////////////////////////////////////////////////////////

    send(stream_fd, &buffer, 1, 0);

    for (;;)
    {
      /////////////////////////////////////////////////////////////////////
      // The main server will eventually hand over the connection socket
      // needed to communicate with a client via the IPC descriptor. we
      // set an abitrary timeout so that we can periodically return and
      // check if the parent (the main daemon that spawned this handler)
      // has terminated abnormally. If that is the case, we terminate
      // on our own.
      /////////////////////////////////////////////////////////////////////

      hResult = CFS_ReceiveDescriptor(stream_fd, &conn_fd, -1);

      if (CS_SUCCEED(hResult))
      {
        //////////////////////////////////////////////////////////////////
        // ECHO handler using CFSAPI non-secure functions
        //////////////////////////////////////////////////////////////////

        CFS_SetChannelDescriptor(pInstance, conn_fd, &rc);

        pServiceProc(pInstance, argv[3], pHTTP);

        CFS_CloseChannelDescriptor(pInstance, &rc);

        //////////////////////////////////////////////////////////////////
        // Tell main daemon we can handle another connection
        //////////////////////////////////////////////////////////////////

        send(stream_fd, &buffer, 1, 0);
      }
    }

    CSHTTP_Destructor(&pHTTP);
  }

  CFS_CloseChannel(pInstance, &rc);
  close(stream_fd);
  dlclose(inprocServer);

  return 0;
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

      CFS_CloseChannel(pInstance, &rc);
      close(stream_fd);
      exit(0);
      break;
  }

  return;
}
