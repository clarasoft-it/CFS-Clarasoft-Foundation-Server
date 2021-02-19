
/* ==========================================================================

  Clarasoft Foundation Server for Linux

  clarad.c
  Main listening daemon with pre-spawned handlers
  Version 1.0.0

  Command line arguments:

   Configuration path from the CFS Repository

 
  Distributed under the MIT license

  Copyright (c) 2013 Clarasoft I.T. Solutions Inc.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sub license, and/or sell
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

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <clarasoft/cslib.h>
#include <clarasoft/cfsapi.h>

void signalCatcher(int signal);

pid_t spawnHandler(char *szHandler,
                   char *szConfig,
                   CSLIST *handlers);

int spawnExtraHandler(char *szHandler,
                      char *szConfig,
                      int conn_fd,
                      int *NumHandlers);

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

int main(int argc, char **argv)
{

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

  HANDLERINFO *phi;

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

  ////////////////////////////////////////////////////////////////////////////
  // We must now turn this process into a daemon
  ////////////////////////////////////////////////////////////////////////////

  // Let's duplicate and detach from the calling process

  pid = fork();

  if (pid < 0)
  {
    return 1;
  }
  else
  {

    if (pid > 0)
    {
      // we are the parent and we are done: if
      // we were executed from a command line, this
      // returns control to the shell.
      exit(0);
    }
  }

  if (setsid() < 0)
  {
    return 2;
  }

  // Now, we duplicate again this time to make sure
  // the daemon cannot acquire a controlling terminal.

  pid = fork();

  if (pid < 0)
  {
    return 1;
  }
  else
  {

    if (pid > 0)
    {
      // we are the parent and we are done: if
      // we were executed from a command line, this
      // returns control to the shell.
      exit(0);
    }
  }

  // change current directory

  chdir("/");

  // close all file descriptors (should only be the first 3)

  for (i = getdtablesize(); i >= 0; --i)
  {
    close(i);
  }

  // redirect STDIN, STDOUT and STDERR

  close(0);
  close(1);
  close(2);

  // redirect STDIN, STDOUT and STDERR to /dev/null (the file descriptor)

  fd = open("/dev/null", O_RDWR, 0);

  if (fd != -1)
  {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    if (fd > 2)
      close(fd);
  }

  // reset umask to prevent insecure file privileges
  // (this daemon could run as root)

  umask(027); // default file creation mode is now 750

  // Check for valid argument values;
  // if the max number of handlers is lower than the initial number, then
  // this is interpreted as meaning there is no maximum.

  maxNumHandlers = maxNumHandlers < initialNumHandlers ? initialNumHandlers : maxNumHandlers;

  handlerFdSet = (struct pollfd *)
      malloc((initialNumHandlers) * sizeof(struct pollfd));

  memset(handlerFdSet, 0, (maxNumHandlers) * sizeof(struct pollfd));

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

  for (i = 0; i < initialNumHandlers; i++)
  {

    spawnHandler(szProtocolHandler, szConfig, handlers);
  }

  ////////////////////////////////////////////////////////////////////////////
  // add child stream pipe descriptors to socket container.
  // We also initialise how many handlers are inserted inside the
  // socket wait container. Note that we initialise this here
  // since it is possible that some handlers may not
  // have spawned (although very unlikely)
  ///////////////////////////////////////////////////////////////////////////

  g_NumHandlers = CSLIST_Count(handlers);

  if (g_NumHandlers < 1)
  {
    // We could not spawn handlers; our server is useless
    exit(4);
  }

  for (i = 0; i < initialNumHandlers; i++)
  {

    CSLIST_GetDataRef(handlers, (void **)&phi, i);

    handlerFdSet[i].fd = phi->stream;
    handlerFdSet[i].events = POLLIN;
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

  return 0;
}

/* --------------------------------------------------------------------------
   spawnHandler
-------------------------------------------------------------------------- */

pid_t spawnHandler(char *szHandler,
                   char *szConfig,
                   CSLIST *handlers)
{

  char *szArgs[5];
  char szDescriptor[8];

  char szRunMode[2];

  pid_t pid;

  HANDLERINFO hi;

  int streamfd[2];

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, streamfd) < 0)
  {
    return -1;
  }

  pid = fork();

  if (pid > 0)
  {

    hi.pid = pid;
    hi.state = 0;
    hi.stream = streamfd[0];

    CSLIST_Insert(handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);

    close(streamfd[1]); // close child half of stream pipe.
    return pid;
  }
  else
  {

    if (pid == 0)
    {

      // we are the child

      close(listen_fd);   // handler will not listen for connections
      close(streamfd[0]); // close parent half of stream pipe.

      // Execute handler; stream pipe descriptor is handed over in argv[1]

      szArgs[0] = szHandler;
      sprintf(szDescriptor, "%d", streamfd[1]);
      szArgs[1] = szDescriptor;

      szRunMode[0] = 'R'; // stay resident, keeps executing after client closes
      szRunMode[1] = 0;   // NULL-terminate

      szArgs[2] = szRunMode;
      szArgs[3] = szConfig;
      szArgs[4] = 0;

      execvp((const char *)szHandler, szArgs);
    }
    else
    {
      // some error occurred
      return -1;
    }
  }

  return pid;
}

int spawnExtraHandler(char *szHandler,
                      char *szConfig,
                      int conn_fd,
                      int *NumHandlers)
{

  pid_t pid;

  char *szArgs[4];

  char szDescriptor[8];
  char szRunMode[2];

  HANDLERINFO hi;

  pid = fork();

  if (pid > 0)
  {

    hi.pid = pid;
    hi.state = 0;
    hi.stream = -1; // we do not have a stream pipe for this child

    CSLIST_Insert(handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);

    (*NumHandlers)++;
    return pid;
  }
  else
  {

    if (pid == 0)
    {

      // we are the child

      close(listen_fd); // handler will not listen for connections

      szArgs[0] = szHandler;
      sprintf(szDescriptor, "%d", conn_fd);
      szArgs[1] = szDescriptor;

      szRunMode[0] = 'T'; // Transcient, stop executing after client closes
      szRunMode[1] = 0;   // NULL-terminate

      szArgs[2] = szRunMode;
      szArgs[3] = szConfig;
      szArgs[4] = 0;

      execvp((const char *)szHandler, szArgs);
    }
    else
    {
      // some error occurred
      return -1;
    }
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

  HANDLERINFO *phi;

  switch (signal)
  {
  case SIGCHLD:

    ///////////////////////////////////////////////////////////////////
    // wait for the available child ...
    // this is to avoid accumulation of zombies
    ///////////////////////////////////////////////////////////////////

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {

      // The child may be resident (R) or transcient(T). A transcient child
      // does not share a stream pipe with this daemon but a resident handler
      // does and we must close our end of this stream pipe and no longer
      // wait on it. Resident children are in the handlers list so we
      // scan it to loacte the stream pipe descriptor.

      count = CSLIST_Count(handlers);

      for (i = 0; i < count; i++)
      {

        CSLIST_GetDataRef(handlers, (void **)(&phi), i);

        if (phi->pid == pid)
        {

          if (phi->stream > -1)
          {

            close(phi->stream);

            ////////////////////////////////////////////////////////////
            // The handler list is aligned to the stream pipe array;
            // We must set this handler as non-executing rather than
            // remove it from the list because the addition of handlers
            // later on would mis-align the handler pid with its stream
            // pipe. It could also overwrite a valid stream pipe
            // and would render its existing associated handler useless.
            ////////////////////////////////////////////////////////////

            phi->pid = -1;

            // mark this stream pipe as invalid
            handlerFdSet[i].fd = -1;
            handlerFdSet[i].events = 0;
          }

          // Decrement number of executing handlers
          g_NumHandlers--;

          break;
        }
      }
    }

    break;

  case SIGTERM:

    // terminate every child handler

    close(listen_fd);

    count = CSLIST_Count(handlers);

    for (i = 0; i < count; i++)
    {

      CSLIST_GetDataRef(handlers, (void **)(&phi), i);

      if (phi->pid != -1)
      {

        kill(phi->pid, SIGTERM);

        if (phi->stream > -1)
        {

          close(phi->stream);
        }
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
