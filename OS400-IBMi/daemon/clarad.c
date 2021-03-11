/* ==========================================================================

  Clarasoft Foundation Server 400
  CSAP test Daemon
  Version 1.0.0

  Command line arguments:

     - Configuration path (value of field PATH in RPSCFP table)

  Compile module with:
    CRTCMOD MODULE(CSAPTD) SRCFILE(QCSRCX) DBGVIEW(*ALL)

  Build program with:

    CRTPGM PGM(CSAPTD) MODULE(CSAPTD)
               BNDSRVPGM(CSLIB CFSAPI)


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

#include "qcsrc/cfsapi.h"
#include "qcsrc/cslib.h"

void
  Cleanup
    (_CNL_Hndlr_Parms_T* args);

unsigned long
  setJobServerType
    (void);

void signalCatcher(int signal);

pid_t
  spawnHandler
    (char *szHandler,
     char *szConfig,
     CSLIST *handlers,
     int *NumHandlers);

int
  spawnExtraHandler
    (char *szHandler,
     char *szConfig,
     int conn_fd,
     int *NumHandlers);

typedef struct tagJobChangeInfo
{
  Qus_Job_Change_Information_t jci;
  Qus_JOBC0100_t               format_JOBC0100;
  char*                        data;
} JobChangeInfo;

volatile unsigned return_code;

typedef struct tagHANDLERINFO
{

  pid_t pid;  // the handler's PID
  int stream; // stream pipe
  int state;  // child state (1 == executing, 0 == waiting, -1 terminated)

} HANDLERINFO;

/* --------------------------------------------------------------------------
  Globals
-------------------------------------------------------------------------- */

CSLIST handlers;

int listen_fd;
int szDaemonNameLength;

char *szDaemonName;

/////////////////////////////////////////////////////////////////////////////
// A descriptor set for the listener (a single instance) and
// a descriptor set for the handler stream pipes. The number of
// handlers can vary so the descriptor set will be allocated dynamically.
/////////////////////////////////////////////////////////////////////////////

struct pollfd listenerFdSet[1];
struct pollfd *handlerFdSet;

char *szProtocolHandler;

int g_NumHandlers;

/* --------------------------------------------------------------------------
  Main
-------------------------------------------------------------------------- */

int main(int argc, char** argv) {

  int conn_fd;
  int i;
  int initialNumHandlers;
  int maxNumHandlers;
  int numDescriptors;

  int on;
  int rc;
  int fd;
  int backlog;

  pid_t pid;

  char dummyData;
  char szPort[11];
  char szConfig[99];

  socklen_t socklen;

  struct sigaction sa;

  struct sockaddr_in6 client;
  struct sockaddr_in6 server;

  CSRESULT hResult;

  CFSRPS repo;
  CFSRPS_CONFIGINFO cfgi;
  CFSRPS_PARAMINFO cfgpi;

  ////////////////////////////////////////////////////////////////////////////
  // Minimally check program arguments
  ////////////////////////////////////////////////////////////////////////////

  if (argc < 2)
  {
    printf("\nmissing argument");
    exit(1);
  }

  ////////////////////////////////////////////////////////////////////////////
  // The list of handler information structures must be initialized first
  // because the cleanup handler will use it.
  ////////////////////////////////////////////////////////////////////////////

  handlers = CSLIST_Constructor();

  ////////////////////////////////////////////////////////////////////////////
  // The daemon name could be other than this file name
  // since several implementations of this server could be executing
  // under different names.
  ////////////////////////////////////////////////////////////////////////////

  szDaemonNameLength = strlen(argv[0]);
  szDaemonName = (char *)malloc(szDaemonNameLength * sizeof(char) + 1);
  memcpy(szDaemonName, argv[0], szDaemonNameLength);
  szDaemonName[szDaemonNameLength] = 0;

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
    exit(1);
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

  repo = CFSRPS_Constructor();

  strcpy(cfgi.szPath, argv[1]);

  if (CS_FAIL(CFSRPS_LoadConfig(repo, &cfgi)))
  {
    exit(1);
  }

  strcpy(szConfig, argv[1]);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "PORT", &cfgpi)))
  {
    exit(2);
  }

  strcpy(szPort, cfgpi.szValue);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "HANDLER", &cfgpi)))
  {
    exit(3);
  }

  szProtocolHandler =
      (char *)malloc(sizeof(char) * strlen(cfgpi.szValue) + 1);
  strcpy(szProtocolHandler, cfgpi.szValue);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "LISTEN_BACKLOG", &cfgpi)))
  {
    backlog = 1024;
  }
  else
  {
    backlog = atoi(cfgpi.szValue);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "RES_NUM_HANDLERS", &cfgpi)))
  {
    initialNumHandlers = 0; // this means all handlers will be transcient
  }
  else
  {
    initialNumHandlers = atoi(cfgpi.szValue);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "TRS_NUM_HANDLERS", &cfgpi)))
  {
    maxNumHandlers = initialNumHandlers;
  }
  else
  {
    maxNumHandlers = initialNumHandlers + atoi(cfgpi.szValue);
  }

  CFSRPS_Destructor(&repo);


  handlerFdSet = (struct pollfd *)
      malloc((initialNumHandlers) * sizeof(struct pollfd));

  memset(handlerFdSet, 0, (initialNumHandlers) * sizeof(struct pollfd));

  ////////////////////////////////////////////////////////////////////////////
  // Get listening socket
  ////////////////////////////////////////////////////////////////////////////

  server.sin6_family = AF_INET6;
  server.sin6_addr = in6addr_any;
  server.sin6_port = htons(atoi(szPort));

  listen_fd = socket(AF_INET6, SOCK_STREAM, 0);

  on = 1;
  hResult = setsockopt(listen_fd,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       (void *)&on,
                       sizeof(int));

  bind(listen_fd, (struct sockaddr *)&server, sizeof(server));

  listen(listen_fd, backlog);

  // Assign listening socket to first wait container slot

  listenerFdSet[0].fd = listen_fd;
  listenerFdSet[0].events = POLLIN;

  ////////////////////////////////////////////////////////////////////////////
  // pre-fork connection handlers.
  ////////////////////////////////////////////////////////////////////////////

  g_NumHandlers = 0;

  for (i = 0; i < initialNumHandlers; i++)
  {
    spawnHandler(szProtocolHandler, szConfig, handlers, &g_NumHandlers);
  }

  ////////////////////////////////////////////////////////////////////////////
  // add child stream pipe descriptors to socket container.
  // We also initialise how many handlers are inserted inside the
  // socket wait container. Note that we initialise this here
  // since it is possible that some handlers may not
  // have spawned (although very unlikely)
  ///////////////////////////////////////////////////////////////////////////

  if (g_NumHandlers < 1)
  {
    // We could not spawn handlers; our server is useless
    exit(4);
  }

  ///////////////////////////////////////////////////////////////////////////
  // This is the main listening loop...
  // wait for client connections and dispatch to child handler
  ///////////////////////////////////////////////////////////////////////////

  for (;;)
  {
    numDescriptors = poll(listenerFdSet, 1, -1);

    if (numDescriptors < 0)
    {
      if (errno == EINTR)
      {
        continue; // start at the top of loop
      }
      else
      {
        //////////////////////////////////////////////////////////////////
        // Some error occurred.
        //////////////////////////////////////////////////////////////////
      }
    }
    else
    {
      if (numDescriptors > 0)
     {
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

        socklen = sizeof(struct sockaddr_in6);
        memset(&client, 0, sizeof(struct sockaddr_in6));

        conn_fd = accept(listen_fd, (struct sockaddr *)&client, &socklen);

        if (conn_fd < 0)
        {
          if (errno == EINTR)
          {
            // At this point, we know there is a connection pending
            // so we must restart accept() again

            goto RESTART_ACCEPT; // accept was interrupted by a signal
          }
        }

        //////////////////////////////////////////////////////////////////
        // Find an available handler; we first wait on handler descriptors
        // assuming they have all been used at least once.
        //////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////
        // BRANCHING LABEL
        RESTART_WAIT:
        //////////////////////////////////////////////////////////////////

        numDescriptors = poll(handlerFdSet, initialNumHandlers, 0);

        if (numDescriptors < 0)
        {
          if (errno == EINTR)
          {
            goto RESTART_WAIT; // call poll() again
          }
          else
          {
            ////////////////////////////////////////////////////////////
            // Some error occurred.
            ////////////////////////////////////////////////////////////
          }
        }

        if (numDescriptors > 0)
        {
          ///////////////////////////////////////////////////////////////
          // A handler is available; let's pass it the client
          // socket descriptor.
          ///////////////////////////////////////////////////////////////

          for (i = 0; i < initialNumHandlers; i++)
          {
            if (handlerFdSet[i].revents == POLLIN)
            {
              /////////////////////////////////////////////////////////
              // Receive dummy byte so to free up blocking handler
              /////////////////////////////////////////////////////////

              /////////////////////////////////////////////////////////
              // BRANCHING LABEL
              RESTART_RECV:
              /////////////////////////////////////////////////////////

              rc = recv(handlerFdSet[i].fd, &dummyData, 1, 0);

              if (rc < 0)
              {
                if (errno == EINTR)
                {
                  goto RESTART_RECV; // call recv() again
                }
              }
              else
              {
                if (rc > 0)
                {
                  ///////////////////////////////////////////////////
                  // This handler is ready
                  // send the connection socket to the
                  // handler and close the descriptor
                  //////////////////////////////////////////////////

                  hResult = CFS_SendDescriptor(handlerFdSet[i].fd,
                                               conn_fd,
                                               10);

                  if (CS_SUCCEED(hResult))
                  {
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
                }
              }
            }
          } // for
        }
        else
        {
          ///////////////////////////////////////////////////////////////
          // No handler is available...
          ///////////////////////////////////////////////////////////////

          if (g_NumHandlers < maxNumHandlers)
          {
            ///////////////////////////////////////////////////////////////
            // Spawn a new one up to maximum; handler is transcient which
            // means it will end once client disconnects.
            ///////////////////////////////////////////////////////////////

            spawnExtraHandler(szProtocolHandler,
                              szConfig, conn_fd, &g_NumHandlers);
          }
        }

        close(conn_fd);
      }
      else
      {
        //////////////////////////////////////////////////////////////////
        // We timed out on the listening socket; we can do
        // some housekeeping here.
        //////////////////////////////////////////////////////////////////
      }
    }

  } // End Listening loop

  CSLIST_Destructor(&handlers);
  free(szDaemonName);
  ///////////////////////////////////////////////////////////////////////////
  //  If you have registered a cancel handler
  //  (see above at the beginning of the main() function).
  ///////////////////////////////////////////////////////////////////////////

  #pragma disable_handler

  return 0;
}


/* --------------------------------------------------------------------------
   spawnHandler
-------------------------------------------------------------------------- */

pid_t
  spawnHandler
    (char* szHandler,
     char* szConfig,
     CSLIST* handlers,
     int* NumHandlers) {

   char *spawn_argv[4];
   char *spawn_envp[1];

   char szMode[2];

   struct inheritance inherit;

   pid_t pid;

   HANDLERINFO hi;

   int spawn_fdmap[1];
   int streamfd[2];

   if (socketpair(AF_UNIX, SOCK_STREAM, 0, streamfd) < 0) {
      return -1;
   }

   memset(&inherit, 0, sizeof(inherit));

   spawn_argv[0] = szDaemonName; // This will be replaced by the spawn function
                                 // with the actual qualified program name; we
                                 // must set it however with some non-null
                                 // value in order that the handler may have
                                 // the next argument

   szMode[0] = 'R';
   szMode[1] = 0;

   spawn_argv[1] = szMode;
   spawn_argv[2] = szConfig;

   spawn_argv[3] = NULL;    // To indicate to handler that there are
                             // no more arguments

   spawn_envp[0] = NULL;

   spawn_fdmap[0] = streamfd[1]; // handler end of stream pipe

   pid = spawn(szHandler,
               1,
               spawn_fdmap,
               &inherit,
               spawn_argv,
               spawn_envp);

   if (pid >= 0) {

      // Add the handler instance to our list of
      // handler jobs

      hi.pid = pid;
      hi.state = 0;
      hi.stream = streamfd[0];

      handlerFdSet[*NumHandlers].fd = hi.stream;
      handlerFdSet[*NumHandlers].events = POLLIN;

      CSLIST_Insert(handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);

      // We don't need the handler side descriptor

      close(streamfd[1]); // close child half of stream pipe.
      (*NumHandlers)++;
   }

   return pid;
}


/* --------------------------------------------------------------------------
   spawnHandler
-------------------------------------------------------------------------- */

pid_t
  spawnExtraHandler
    (char* szHandler,
     char* szConfig,
     int conn_fd,
     int *NumHandlers) {

   char *spawn_argv[4];
   char *spawn_envp[1];

   char szMode[2];

   struct inheritance inherit;

   pid_t pid;

   HANDLERINFO hi;

   int spawn_fdmap[1];
   int streamfd[2];

   memset(&inherit, 0, sizeof(inherit));

   spawn_argv[0]  = szDaemonName; // This will be replaced by the spawn function
                                  // with the actual qualified program name; we
                                  // must set it however with some non-null
                                  // value in order that the handler may have
                                  // the next argument
   szMode[0] = 'T';
   szMode[1] = 0;

   spawn_argv[1] = szMode;
   spawn_argv[2] = szConfig;

   spawn_argv[3] = NULL;    // To indicate to handler that there are
                             // no more arguments

   spawn_envp[0] = NULL;

   spawn_fdmap[0] = conn_fd; // handler end of stream pipe

   pid = spawn(szHandler,
               1,
               spawn_fdmap,
               &inherit,
               spawn_argv,
               spawn_envp);

   if (pid >= 0) {

      // Add the handler instance to our list of
      // handler jobs

      hi.pid = pid;
      hi.state = 0;
      hi.stream = -1;

      (*NumHandlers)++;
      CSLIST_Insert(handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);
      close(conn_fd);
   }

   return pid;
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

        count = CSLIST_Count(handlers);

        for (i=0; i<count; i++) {

          CSLIST_GetDataRef(handlers, (void**)(&phi), i);

          if (phi->pid == pid) {

            // Note that the descriptor set index is
            // aligned with the handler list index. We
            // dont remove handlers from the list
            // once they are terminated. We just mark them
            // as inactive so that we don't send them
            // the SIGTERM signal when this daemon
            // is shut down.

            close(handlerFdSet[i].fd);

            // This removes the descriptor from
            // being examined by the poll function.

            handlerFdSet[i].fd = -1;
            phi->state = -1;
            phi->pid = -1;
            break;
         }
       }

       g_NumHandlers--;
     }

     break;

   case SIGTERM:

     // terminate every child handler

     close(listen_fd);

     count = CSLIST_Count(handlers);

     for (i=0; i<count; i++) {

       CSLIST_GetDataRef(handlers, (void**)(&phi), i);
       kill(phi->pid, SIGTERM);
       close(handlerFdSet[i].fd);
     }

     // wait for all children
     while (wait(NULL) > 0)
             ;

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

  close(listen_fd);

  // terminate every child handler

  count = CSLIST_Count(handlers);

  for (i=0; i<count; i++) {

    CSLIST_GetDataRef(handlers, (void**)(&phi), i);

    if (phi->pid != -1) {

      if (handlerFdSet[i].fd >= 0) {
        close(handlerFdSet[i].fd);
      }

      kill(phi->pid, SIGTERM);
    }
  }
  // wait for all children

  while (wait(NULL) > 0)
             ;

  free(szDaemonName);
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
                                               szDaemonNameLength;
  chgInfo.format_JOBC0100.Key_Field          = 1911;
  chgInfo.format_JOBC0100.Type_Of_Data       = 'C';
  chgInfo.format_JOBC0100.Reserved[0]        = 0x40;
  chgInfo.format_JOBC0100.Reserved[1]        = 0x40;
  chgInfo.format_JOBC0100.Reserved[2]        = 0x40;
  chgInfo.format_JOBC0100.Length_Data        = szDaemonNameLength;
  chgInfo.data = szDaemonName;

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
 
