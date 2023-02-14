/* ==========================================================================

  Clarasoft Foundation Server OS/400
  Main listening daemon with pre-spawned handlers

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

========================================================================== */

#include <arpa/inet.h>
#include <errno.h>
#include <except.h>
#include <netinet/in.h>
#include <qusec.h>
#include <QWTCHGJB.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "qcsrc/cfs.h"
#include "qcsrc/cslib.h"

/* --------------------------------------------------------------------------
  Local definitions
-------------------------------------------------------------------------- */

void
  signalCatcher
    (int signal);

long
  spawnHandler
    (char *szHandler,
     char *szConfig,
     int conn_fd);

void
  Cleanup
    (_CNL_Hndlr_Parms_T* args);

unsigned long
  setJobServerType
    (void);

typedef struct tagJobChangeInfo {

  Qus_Job_Change_Information_t jci;
  Qus_JOBC0100_t               format_JOBC0100;
  char*                        data;

} JobChangeInfo;

typedef struct tagHANDLERINFO {

   pid_t pid;  // the handler's PID
   int stream; // stream pipe
   int state;  // child state (1 == executing, 0 == waiting, -1 terminated)

} HANDLERINFO;

typedef struct tagDAEMON {

  int initialNumHandlers;
  int maxNumHandlers;
  int numHandlers;
  int nextHandlerSlot;
  int listen_fd;
  int iDaemonNameLength;
  int iPeerNameSize;

  char* szDaemonName;
  char* szProtocolHandler;

  char szPeerName[256];
  char szLogDir[1024];
  char szLogName[256];
  char szLogFile[2049];

  struct pollfd listenerFdSet[1];
  struct pollfd *handlerFdSet;

  HANDLERINFO* handlerInfoVector;

} DAEMON;

/* --------------------------------------------------------------------------
  Globals
-------------------------------------------------------------------------- */

volatile unsigned return_code;

DAEMON d;

int main(int argc, char** argv) {

  int i;
  int numDescriptors;
  int on;
  int rc;
  int backlog;
  int handlerTimeout;
  int pid;
  int size;
  int conn_fd;
  int len;
  int fd;

  long handlerIndex;

  char dummyData;

  char* pszParam;

  char szPort[11];
  char szConfig[99];
  char szHandlerConfig[99];

  struct sigaction sa;
  char szInstance[65];
  char szHandler[129];
  char szBacklog[129];
  char szMinHandlers[129];
  char szMaxHandlers[129];

  struct sockaddr_in6 client;
  struct sockaddr_in6 server;

  CSRESULT hResult;

  CFSRPS pRepo;
  CFSCFG pConfig;

  HANDLERINFO hi;
  HANDLERINFO* phi;

  socklen_t socklen;

  ////////////////////////////////////////////////////////////////////////////
  // Minimally check program arguments
  ////////////////////////////////////////////////////////////////////////////

  if (argc < 2) {
    printf("\nmissing argument");
    exit(1);
  }

  ////////////////////////////////////////////////////////////////////////////
  // The daemon name could be other than this file name
  // since several implementations of this server could be executing
  // under different names.
  ////////////////////////////////////////////////////////////////////////////

  d.iDaemonNameLength = strlen(argv[0]);

  if (d.iDaemonNameLength > 255) {
    d.iDaemonNameLength = 255;
  }

  d.szDaemonName = (char *)malloc(d.iDaemonNameLength * sizeof(char) + 1);
  memcpy(d.szDaemonName, argv[0], d.iDaemonNameLength);
  d.szDaemonName[d.iDaemonNameLength] = 0;

  ////////////////////////////////////////////////////////////////////////////
  //
  // This code is to register a cleanup handler
  // for when the main server job is cancelled. This is not
  // necessary but is proper i5 OS practice for servers.
  //
  // The #pragma directive must be coupled with another at some later point
  // in the main() function; see code just before return statement in main().
  //
  ////////////////////////////////////////////////////////////////////////////

  return_code = 0;
  #pragma cancel_handler( Cleanup, return_code )

  ////////////////////////////////////////////////////////////////////////////
  // This is to register the program as a server job to
  // perform administrative tasks, such as stopping,
  // starting, and monitoring the server in the same way
  // as a server that is supplied on the System i platform.
  ////////////////////////////////////////////////////////////////////////////

  if (!setJobServerType()) {
    exit(6);
  }

  ////////////////////////////////////////////////////////////////////////////
  // Set signal handlers.
  // We will monitor for SIGCHLD and SIGTERM.
  ////////////////////////////////////////////////////////////////////////////

  // ignore SIGHUP
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0; // or SA_RESTART
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = (void *)signalCatcher;

  sigaction(SIGHUP, &sa, NULL);

  sa.sa_handler = signalCatcher;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGCHLD, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  //////////////////////////////////////////////////////////////////
  // Retrieve daemon configuration
  //////////////////////////////////////////////////////////////////

  pRepo = CFSRPS_Open(NULL);

  size = strlen(argv[1]);

  if (size > 98) {
    size = 98;
  }

  memcpy(szConfig, argv[1], size);
  szConfig[size] = 0;

  if ((pConfig = CFSRPS_OpenConfig(pRepo, szConfig)) == NULL)
  {
    CFSRPS_Close(&pRepo);
    exit(1);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "HANDLER")) == NULL)
  {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    exit(4);
  }

  d.szProtocolHandler =
      (char *)malloc(sizeof(char) * strlen(pszParam) + 1);

  strcpy(d.szProtocolHandler, pszParam);

  if ((pszParam = CFSCFG_LookupParam(pConfig, "PORT")) == NULL)
  {
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    exit(3);
  }

  strcpy(szPort, pszParam);

  if ((pszParam = CFSCFG_LookupParam(pConfig, "HANDLER_CONFIG"))== NULL)
  {
    strncpy(szHandlerConfig, argv[1], 99);
  }
  else {
    strncpy(szHandlerConfig, pszParam, 99);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "LISTEN_BACKLOG")) == NULL)
  {
    backlog = 1024;
  }
  else
  {
    backlog = atoi(pszParam);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "RES_NUM_HANDLERS")) == NULL)
  {
    d.initialNumHandlers = 1; // this means all handlers will be transcient
  }
  else
  {
    d.initialNumHandlers = atoi(pszParam);

    if (d.initialNumHandlers == 0) {
      d.initialNumHandlers = 1;
    }
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "MAX_NUM_HANDLERS")) == NULL)
  {
    d.maxNumHandlers = d.initialNumHandlers;
  }
  else
  {
    d.maxNumHandlers = atoi(pszParam);
    if (d.maxNumHandlers < d.initialNumHandlers) {
      d.maxNumHandlers = d.initialNumHandlers;
    }
  }

  CFSRPS_CloseConfig(pRepo, &pConfig);
  CFSRPS_Close(&pRepo);

  ////////////////////////////////////////////////////////////////////////////
  // Get listening socket
  ////////////////////////////////////////////////////////////////////////////

  server.sin6_family = AF_INET6;
  server.sin6_addr = in6addr_any;
  server.sin6_port = htons(atoi(szPort));

  d.listen_fd = socket(AF_INET6, SOCK_STREAM, 0);

  on = 1;
  hResult = setsockopt(d.listen_fd,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       (void *)&on,
                       sizeof(int));

  bind(d.listen_fd, (struct sockaddr *)&server, sizeof(server));

  listen(d.listen_fd, backlog);

  // Assign listening socket to first wait container slot

  d.listenerFdSet[0].fd = d.listen_fd;
  d.listenerFdSet[0].events = POLLIN;

  ////////////////////////////////////////////////////////////////////////////
  // pre-fork connection handlers.
  ////////////////////////////////////////////////////////////////////////////

  d.handlerFdSet = (struct pollfd *)
      malloc((d.maxNumHandlers) * sizeof(struct pollfd));

  d.handlerInfoVector = (HANDLERINFO*)
      malloc(d.maxNumHandlers * sizeof(HANDLERINFO));

  for (i=0; i<d.maxNumHandlers; i++) {
    d.handlerFdSet[i].fd = -1;
    d.handlerFdSet[i].events = 0;

    d.handlerInfoVector[i].pid = -1;
    d.handlerInfoVector[i].stream = -1;
    d.handlerInfoVector[i].state = 0;
  }

  d.numHandlers = 0;

  for (i=0; i<d.initialNumHandlers; i++)
  {
    spawnHandler(d.szProtocolHandler, szHandlerConfig, -1);
  }

  socklen = sizeof(struct sockaddr_in6);

  ///////////////////////////////////////////////////////////////////////////
  // This is the main listening loop...
  // wait for client connections and dispatch to child handler
  ///////////////////////////////////////////////////////////////////////////

  for (;;)
  {
    numDescriptors = poll(d.listenerFdSet, 1, -1);

    if (numDescriptors > 0) {

      //////////////////////////////////////////////////////////////////
      // A connection request has come in; we want to
      // get an available handler. For this, we specify
      // a zero timeout to immediately get the descriptors
      // that are ready.
      //////////////////////////////////////////////////////////////////

      //////////////////////////////////////////////////////////////////
      // BRANCHING LABEL
      RESTART_ACCEPT:
      //////////////////////////////////////////////////////////////////

      memset(&client, 0, sizeof(struct sockaddr_in6));

      conn_fd = accept(d.listen_fd, (struct sockaddr *)&client, &socklen);

      if (conn_fd < 0)
      {
        if (errno == EINTR)
        {
          // At this point, we know there is a connection pending
          // so we must restart accept() again
          goto RESTART_ACCEPT; // accept was interrupted by a signal
        }
        else {
          // report error
        }
      }
      else {

        handlerTimeout = 0; // return immediately for available handler

        //////////////////////////////////////////////////////////////////
        // Find an available handler; we first wait on handler descriptors
        // assuming they have all been used at least once.
        //////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////
        // BRANCHING LABEL
        RESTART_WAIT:
        //////////////////////////////////////////////////////////////////

        numDescriptors = poll(d.handlerFdSet, d.numHandlers, handlerTimeout);

        if (numDescriptors > 0)
        {
          ///////////////////////////////////////////////////////////////
          // A handler is available; let's pass it the client
          // socket descriptor.
          ///////////////////////////////////////////////////////////////

          for (i = 0; i < d.maxNumHandlers; i++)
          {
            if (d.handlerFdSet[i].revents == POLLIN)
            {
              /////////////////////////////////////////////////////////
              // Receive dummy byte so to free up blocking handler
              /////////////////////////////////////////////////////////

              /////////////////////////////////////////////////////////
              // BRANCHING LABEL
              RESTART_RECV:
              /////////////////////////////////////////////////////////

              rc = recv(d.handlerFdSet[i].fd, &dummyData, 1, 0);

              if (rc > 0)
              {
                ///////////////////////////////////////////////////
                // This handler is ready
                // send the connection socket to the
                // handler and close the descriptor
                //////////////////////////////////////////////////

                hResult = CFS_SendDescriptor(d.handlerFdSet[i].fd,
                                             conn_fd,
                                             10);

                if (CS_SUCCEED(hResult))
                {
                  ///////////////////////////////////////////////////////////////
                  // Log information on the peer
                  ///////////////////////////////////////////////////////////////

                  ////////////////////////////////////////////////
                  // IMPORTANT... we must break out of the loop
                  // because another handler may be sending its
                  // dummy byte and we would wind up sending
                  // it the client socket but we have already
                  // done so; there would be more than one
                  // handler for a single client!
                  ////////////////////////////////////////////////
                  break;
                }
                else {
                  // report error and leave inner for loop
                  break;
                }
              }
              else
              {
                if (rc < 0) {
                  if (errno == EINTR)
                  {
                    // recv was interrupted
                    goto RESTART_RECV; // call recv() again
                  }
                  else {
                    // report error and leave inner for loop
                    break;
                  }
                }
                else {
                  // handler closed connection, leave inner for loop
                  break;
                }
              }
            }
          } // for
        }
        else {

          if (numDescriptors == 0) {

            ///////////////////////////////////////////////////////////////
            // No handler is available... if we have not yet reached
            // max handlers, spawn a new one and retry the poll call.
            ///////////////////////////////////////////////////////////////

            if (d.numHandlers < d.maxNumHandlers) {

              if ((handlerIndex =
                    spawnHandler(d.szProtocolHandler,
                                 szHandlerConfig,
                                 conn_fd)) > 0) {

                /////////////////////////////////////////////////////////
                // Receive dummy byte so to free up blocking handler
                /////////////////////////////////////////////////////////

                /////////////////////////////////////////////////////////
                // BRANCHING LABEL
                RESTART_SPAWN_RECV:
                /////////////////////////////////////////////////////////

                rc = recv(d.handlerFdSet[handlerIndex].fd, &dummyData, 1, 0);

                if (rc > 0) {

                  ///////////////////////////////////////////////////
                  // This handler is ready
                  // send the connection socket to the
                  // handler and close the descriptor
                  //////////////////////////////////////////////////

                  hResult = CFS_SendDescriptor(d.handlerFdSet[handlerIndex].fd,
                                               conn_fd,
                                               10);

                  if (CS_FAIL(hResult))
                  {
                    // report error
                  }
                }
                else
                {
                  if (rc < 0) {
                    if (errno == EINTR)
                    {
                      // recv was interrupted
                      goto RESTART_SPAWN_RECV; // call recv() again
                    }
                    else {
                      // report error
                    }
                  }
                  else {
                    // handler closed connection
                  }
                }
              }
              else {
                // error spawning extra handler
              }
            }
            else {
              // maximumm number of handlers reached
            }
          }
          else {

            if (errno == EINTR)
            {
              // poll interrupted
              goto RESTART_WAIT; // call poll() again
            }
            else
            {
              // Some error occurred.
            }
          }
        }

        close(conn_fd);

      }
    }
    else {

      if (numDescriptors < 0)
      {
        if (errno != EINTR)
        {
          //////////////////////////////////////////////////////////////////
          // Some error occurred.
          //////////////////////////////////////////////////////////////////
        }
        else {
          // poll interrupted ... we go back to calling pool again anyway
        }
      }
      else {
        // poll timed out (if there is a timeout)
      }
    }
  } // End Listening loop

  ///////////////////////////////////////////////////////////////////////////
  //  If you have registered a cancel handler
  //  (see above at the beginning of the main() function).
  ///////////////////////////////////////////////////////////////////////////

  #pragma disable_handler

  free(d.szDaemonName);

  return 0;
}

/* --------------------------------------------------------------------------
   spawnHandler
-------------------------------------------------------------------------- */

long
  spawnHandler
    (char *szHandler,
     char *szConfig,
     int conn_fd) {

   char *spawn_argv[4];
   char *spawn_envp[1];

   char szMode[2];

   int i;
   int e;

   long handlerIndex;

   struct inheritance inherit;

   pid_t pid;

   int spawn_fdmap[1];
   int streamfd[2];

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, streamfd) < 0) {
      return -1;
   }

   memset(&inherit, 0, sizeof(inherit));

   spawn_argv[0]  = d.szDaemonName; // This will be replaced by the spawn function
                                  // with the actual qualified program name; we
                                  // must set it however with some non-null
                                  // value in order that the handler may have
                                  // the next argument

   spawn_argv[1]  = szConfig;

   spawn_argv[2]  = NULL;    // To indicate to handler that there are
                             // no more arguments

   spawn_envp[0]  = NULL;

   spawn_fdmap[0] = streamfd[1]; // handler end of stream pipe

   pid = spawn(szHandler,
               1,
               spawn_fdmap,
               &inherit,
               spawn_argv,
               spawn_envp);

   if (pid >= 0) {

      // find next available handler slot

      for (handlerIndex=-1, i=0; i<d.maxNumHandlers; i++) {

        if (d.handlerFdSet[i].fd == -1) {

          d.handlerFdSet[i].fd = streamfd[0];
          d.handlerFdSet[i].events = POLLIN;

          d.handlerInfoVector[i].pid = pid;
          d.handlerInfoVector[i].stream = streamfd[0];
          d.handlerInfoVector[i].state = 1;
          handlerIndex = i;
          break;
        }
      }

      (d.numHandlers)++;
      close(streamfd[1]); // close child half of stream pipe.

      return handlerIndex;
   }
   else {
     e = errno;
   }

   return -1;
}

/* --------------------------------------------------------------------------
  signalCatcher
-------------------------------------------------------------------------- */

void signalCatcher(int signal)
{
  pid_t pid;
  int stat;
  long i;
  long count;
  long size;
  HANDLERINFO* phi;

  switch(signal)
  {
    case SIGCHLD:

      // wait for the available children ...
      // this is to avoid accumulation of zombies
      // when child handlers are terminated
      // (because the parent (daemon) keeps executing).

      while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {

        for (i=0; i<d.maxNumHandlers; i++) {

          if (d.handlerInfoVector[i].pid == pid) {

            if (d.handlerInfoVector[i].stream > -1)
            {
              close(d.handlerInfoVector[i].stream);
            }

            d.handlerInfoVector[i].pid = -1;
            d.handlerInfoVector[i].stream = -1;
            d.handlerInfoVector[i].state = 0;

            // mark this stream pipe as invalid
            d.handlerFdSet[i].fd = -1;
            d.handlerFdSet[i].events = 0;

            // Decrement number of executing handlers
            (d.numHandlers)--;

            break;
         }
       }
     }

     break;

   case SIGTERM:

     // terminate every child handler

     close(d.listen_fd);

     for (i=0; i<d.maxNumHandlers; i++) {

       if (d.handlerInfoVector[i].pid != -1) {
         kill(d.handlerInfoVector[i].pid, SIGTERM);
         close(d.handlerFdSet[i].fd);
       }
     }

     // wait for all children
     while (wait(NULL) > 0)
             ;

     exit(0);
     break;
  }

  return;
}

/* --------------------------------------------------------------------------
  Cleanup
-------------------------------------------------------------------------- */

void Cleanup(_CNL_Hndlr_Parms_T* data) {

  long i;
  long count;
  long size;
  HANDLERINFO* phi;

  close(d.listen_fd);

  for (i=0; i<d.maxNumHandlers; i++) {

    if (d.handlerInfoVector[i].pid != -1) {
      kill(d.handlerInfoVector[i].pid, SIGTERM);
      close(d.handlerFdSet[i].fd);
    }
  }

  // wait for all children
  while (wait(NULL) > 0)
             ;

/*
  close(d.listen_fd);

  // terminate every child handler

  close(d.listen_fd);

  for (i=0; i<d.maxNumHandlers; i++) {

    if (d.handlerInfoVector[i].pid != -1) {
      kill(d.handlerInfoVector[i].pid, SIGTERM);
      close(d.handlerFdSet[i].fd);
    }
  }

  // wait for all children
  while (wait(NULL) > 0)
             ;

  free(d.szDaemonName);
*/
}

/* --------------------------------------------------------------------------
 setJobServerType
-------------------------------------------------------------------------- */

unsigned long setJobServerType(void) {

  Qus_EC_t     EcStruc;

  JobChangeInfo chgInfo;

  /*
    The fields are:
      Number of variable length records
      Length of this structure
      Key - Server Type.
      Type of Data - Character
      Reserved (3 blank characters)
      Length of data with server name
      Server name
  */

  chgInfo.jci.Number_Fields_Enterd           = 1;
  chgInfo.format_JOBC0100.Length_Field_Info_ = sizeof(chgInfo) +
                                               d.iDaemonNameLength;
  chgInfo.format_JOBC0100.Key_Field          = 1911;
  chgInfo.format_JOBC0100.Type_Of_Data       = 'C';
  chgInfo.format_JOBC0100.Reserved[0]        = 0x40;
  chgInfo.format_JOBC0100.Reserved[1]        = 0x40;
  chgInfo.format_JOBC0100.Reserved[2]        = 0x40;
  chgInfo.format_JOBC0100.Length_Data        = d.iDaemonNameLength;
  chgInfo.data = d.szDaemonName;

  EcStruc.Bytes_Provided  = 16;
  EcStruc.Bytes_Available = 0;

  QWTCHGJB("*                         ",
           "                ",
           "JOBC0200",
           &chgInfo,
           &EcStruc);

   if ( EcStruc.Bytes_Available != 0 ) {
     return 0;
   }

   return 1;
}
