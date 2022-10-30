/* ==========================================================================

  Clarasoft Foundation Server for Linux
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

#define _GNU_SOURCE 

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <clarasoft/cfs.h>

/* --------------------------------------------------------------------------
  Local definitions
-------------------------------------------------------------------------- */

void 
  signalCatcher
    (int signal);

pid_t 
  spawnHandler
    (char *szHandler,
     char *szConfig,
     int conn_fd);

CSRESULT
  CleanupLogs
    (char* szPath,
     char* szPattern,
     int days);

int
  RunAsDaemon
    (int argc, char **argv);

typedef struct tagHANDLERINFO
{

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

  CSLIST handlers;

} DAEMON;

/* --------------------------------------------------------------------------
  Globals
-------------------------------------------------------------------------- */

FILE* daemonlog;

time_t now;
struct tm ts;

int days;

DAEMON d;

char timestamp[80];

/////////////////////////////////////////////////////////////////////////////
// A descriptor set for the listener (a single instance) and
// a descriptor set for the handler stream pipes. The number of
// handlers can vary so the descriptor set will be allocated dynamically.
/////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{

  int i;
  int numDescriptors;
  int on;
  int rc;
  int backlog;
  int handlerTimeout;
  int pid;
  int size;

  //////////////////////////////////////////////////////////////
  // The following is the client connection socket; when a new
  // handler is spawned, this descriptor must be closed
  // by both the parent and the child; the parent closes it
  // after sending it to the child; the child has to close
  // it before calling the exec function.
  //////////////////////////////////////////////////////////////

  int conn_fd;

  char dummyData;

  char* pszParam;

  char szPort[11];
  char szConfig[99];
  char szHandlerConfig[99];

  socklen_t socklen;

  struct sigaction sa;

  struct sockaddr_in6 client;
  struct sockaddr_in6 server;

  CSRESULT hResult;

  CFSRPS pRepo;
  CFSCFG pConfig;

  HANDLERINFO hi;
  HANDLERINFO* phi;
 
  openlog(basename(argv[0]), LOG_PID, LOG_LOCAL3);

  ////////////////////////////////////////////////////////////////////////////
  // Minimally check program arguments
  ////////////////////////////////////////////////////////////////////////////

  if (argc < 2)
  {
    syslog(LOG_ERR, "missing argument");
    closelog();
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
  // If we are running this daemon in debug mode, then we skip converting
  // this process to a daemon process.
  ////////////////////////////////////////////////////////////////////////////

  if (argc >= 3) {
   
    if (argv[2][0] == 'd') {
      syslog (LOG_INFO, "Running in DEBUG mode");
      goto START_DAEMON_DEBUGMODE;
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // We must now turn this process into a daemon
  ////////////////////////////////////////////////////////////////////////////

  if (RunAsDaemon(argc, argv) > 0) {
    exit(1);
  }

  ////////////////////////////////////////////////////////////////////////////
  // branching label
  START_DAEMON_DEBUGMODE:

  // reset umask to prevent insecure file privileges
  // (this daemon could run as root)

  umask(027); // default file creation mode is now 750

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

  if ((pConfig = CFSRPS_OpenConfig(pRepo, argv[1])) == NULL)
  {
    CFSRPS_Close(&pRepo);
    syslog(LOG_ERR, "can't open configuration : %s", argv[1]);
    closelog();
    exit(1);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "LOGDIR")) == NULL)
  {
     strcpy(d.szLogDir, "");
  }
  else {
     strcpy(d.szLogDir, pszParam);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "LOGFILE")) == NULL)
  {
     strcpy(d.szLogName, argv[0]);
  }
  else {
     strcpy(d.szLogName, pszParam);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "LOGFILE_RETAIN")) == NULL)
  {
     days = 7;
  }
  else {
     days = atoi(pszParam);
  }

  CleanupLogs(d.szLogDir, d.szLogName, days);

  time(&now);
  ts = *localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", &ts);

  sprintf(d.szLogFile, 
          "%s/%s-%s-log.txt",
          d.szLogDir, d.szLogName, timestamp);    

  daemonlog = fopen(d.szLogFile, "w+");

  if (!daemonlog) {
    syslog(LOG_ERR, "can't open log file : %s", d.szLogFile);
    closelog();
    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    exit(2);
  }

  // No longer need syslog, we switch to daemon log file
  syslog(LOG_INFO, "running with configuration %s", argv[1]);
  closelog( );

  if ((pszParam = CFSCFG_LookupParam(pConfig, "HANDLER")) == NULL)
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-ERR   HANDLER parameter not found\n",
            timestamp);
    fflush(daemonlog);

    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    exit(4);
  }

  d.szProtocolHandler =
      (char *)malloc(sizeof(char) * strlen(pszParam) + 1);

  strcpy(d.szProtocolHandler, pszParam);

  time(&now);
  ts = *localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

  pid = getpid();

  fprintf(daemonlog, "%s - DAEMON-STR NAME: %s PID: %d CONFIG: %s "
                     "port: %s "
                     "handler: %s\n", 
                     timestamp, d.szDaemonName, pid, szConfig, szPort, d.szProtocolHandler);
  fflush(daemonlog);

  if ((pszParam = CFSCFG_LookupParam(pConfig, "PORT")) == NULL)
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-ERR   PORT parameter not found()\n",
            timestamp);
    fflush(daemonlog);

    CFSRPS_CloseConfig(pRepo, &pConfig);
    CFSRPS_Close(&pRepo);
    exit(3);
  }

  strcpy(szPort, pszParam);

  if ((pszParam = CFSCFG_LookupParam(pConfig, "HANDLER_CONFIG"))== NULL)
  {
    strncpy(szHandlerConfig, argv[1], 99);

    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-WARN  HANDLER_CONFIG parameter not found - using daemon configuration\n",
            timestamp);
    fflush(daemonlog);
  }
  else {
    strncpy(szHandlerConfig, pszParam, 99);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "LISTEN_BACKLOG")) == NULL)
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-WARN  LISTEN_BACKLOG parameter not found - using default value of 1024\n",
            timestamp);
    fflush(daemonlog);

    backlog = 1024;
  }
  else
  {
    backlog = atoi(pszParam);
  }

  if ((pszParam = CFSCFG_LookupParam(pConfig, "RES_NUM_HANDLERS")) == NULL)
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-WARN  RES_NUM_HANDLERS parameter not found - using default value of 0\n",
            timestamp);
    fflush(daemonlog);

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
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-WARN  MAX_NUM_HANDLERS parameter not found - using default value\n",
            timestamp);
    fflush(daemonlog);

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

  for (i=0; i<d.maxNumHandlers; i++) {
    d.handlerFdSet[i].fd = -1;
    d.handlerFdSet[i].events = 0;
  }

  hi.pid = -1;
  hi.state = 0;
  hi.stream = -1;
  d.numHandlers = 0;

  d.handlers = CSLIST_Constructor();

  for (i=0; i<d.initialNumHandlers; i++)
  {
    CSLIST_Insert(d.handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);
    if ((pid = spawnHandler(d.szProtocolHandler, szHandlerConfig, -1)) > 0) {

      fprintf(daemonlog, "%s - HNDL-STR   PID: %10d         Starting resident handler\n", 
              timestamp, pid);
    }
    else {

      fprintf(daemonlog, "%s - HNDL-ERROR Failed starting resident handler\n", 
              timestamp);
    }
  }

  fflush(daemonlog);

  for (; i < d.maxNumHandlers; i++)
  {
    CSLIST_Insert(d.handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);
  }

  socklen = sizeof(struct sockaddr_in6);

  ///////////////////////////////////////////////////////////////////////////
  // This is the main listening loop...
  // wait for client connections and dispatch to child handler
  ///////////////////////////////////////////////////////////////////////////

  for (;;)
  {
    numDescriptors = poll(d.listenerFdSet, 1, -1);

    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

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

          fprintf(daemonlog, "%s - ACCP-INT   interrupted on accept()\n",
                  timestamp);
          fflush(daemonlog);

          goto RESTART_ACCEPT; // accept was interrupted by a signal
        }
        else {

          fprintf(daemonlog, "%s - ACCP-ERR   errno: %10d accept() returned an error\n",
                  timestamp, errno);
          fflush(daemonlog);
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

                  CSLIST_GetDataRef(d.handlers, (void**)&phi, i);

                  ///////////////////////////////////////////////////////////////
                  // Log information on the peer
                  ///////////////////////////////////////////////////////////////

                  sprintf(d.szPeerName,"IPV6 %02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                                       "%02x%02x:%02x%02x:%02x%02x:%02x%02x - " 
                                       "IPV4 %03d:%03d:%03d:%03d",
                          (int)client.sin6_addr.s6_addr[0],  (int)client.sin6_addr.s6_addr[1],
                          (int)client.sin6_addr.s6_addr[2],  (int)client.sin6_addr.s6_addr[3],
                          (int)client.sin6_addr.s6_addr[4],  (int)client.sin6_addr.s6_addr[5],
                          (int)client.sin6_addr.s6_addr[6],  (int)client.sin6_addr.s6_addr[7],
                          (int)client.sin6_addr.s6_addr[8],  (int)client.sin6_addr.s6_addr[9],
                          (int)client.sin6_addr.s6_addr[10], (int)client.sin6_addr.s6_addr[11],
                          (int)client.sin6_addr.s6_addr[12], (int)client.sin6_addr.s6_addr[13],
                          (int)client.sin6_addr.s6_addr[14], (int)client.sin6_addr.s6_addr[15],
                          (int)client.sin6_addr.s6_addr[12], (int)client.sin6_addr.s6_addr[13],
                          (int)client.sin6_addr.s6_addr[14], (int)client.sin6_addr.s6_addr[15]); 

                  fprintf(daemonlog, "%s - CONN       HOST: %s PID:  %10d Connection received\n", 
                  timestamp, d.szPeerName, phi->pid);

                  fflush(daemonlog);

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

                  CSLIST_GetDataRef(d.handlers, (void**)&phi, i);

                  fprintf(daemonlog, "%s - CONN-ERR   errno: %10d       Failed to send socket descriptor to handler\n", 
                          timestamp, errno);

                  fflush(daemonlog);
                  break;
                }
              }
              else
              {
                if (rc < 0) {
                  if (errno == EINTR)
                  {
                    fprintf(daemonlog, "%s - HND-RECV-H recv() interrupted while reading handler stream pipe\n",
                            timestamp);
                    fflush(daemonlog);

                    goto RESTART_RECV; // call recv() again
                  }
                  else {
                    fprintf(daemonlog, "%s - HND-RECV-H errno: %d recv() error\n",
                            timestamp, errno);
                    fflush(daemonlog);
                    break;
                  }
                }
                else {
                  
                  ///////////////////////////////////////////////////
                  // handler closed connection
                  ///////////////////////////////////////////////////

                  fprintf(daemonlog, "%s - HND-DISC-H handler closed connection on stream pipe\n",
                          timestamp);
                  fflush(daemonlog);
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

            if (d.numHandlers < d.maxNumHandlers)
            {
              if ((pid = spawnHandler(d.szProtocolHandler, szHandlerConfig, conn_fd)) > 0) {

                fprintf(daemonlog, "%s - HNDL-STR   PID: %10d         Starting resident handler\n", 
                        timestamp, pid);
                fflush(daemonlog);

                // allow extra handler time to start
                handlerTimeout = 20000;
                // return back to poll on handler descriptors
                goto RESTART_WAIT;
              }
              else {

                sprintf(d.szPeerName,"IPV6 %02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                                     "%02x%02x:%02x%02x:%02x%02x:%02x%02x - " 
                                     "IPV4 %03d:%03d:%03d:%03d",
                          (int)client.sin6_addr.s6_addr[0],  (int)client.sin6_addr.s6_addr[1],
                          (int)client.sin6_addr.s6_addr[2],  (int)client.sin6_addr.s6_addr[3],
                          (int)client.sin6_addr.s6_addr[4],  (int)client.sin6_addr.s6_addr[5],
                          (int)client.sin6_addr.s6_addr[6],  (int)client.sin6_addr.s6_addr[7],
                          (int)client.sin6_addr.s6_addr[8],  (int)client.sin6_addr.s6_addr[9],
                          (int)client.sin6_addr.s6_addr[10], (int)client.sin6_addr.s6_addr[11],
                          (int)client.sin6_addr.s6_addr[12], (int)client.sin6_addr.s6_addr[13],
                          (int)client.sin6_addr.s6_addr[14], (int)client.sin6_addr.s6_addr[15],
                          (int)client.sin6_addr.s6_addr[12], (int)client.sin6_addr.s6_addr[13],
                          (int)client.sin6_addr.s6_addr[14], (int)client.sin6_addr.s6_addr[15]); 

                fprintf(daemonlog, "%s - CONN-FAIL  HOST: %s Could not spawn extra handler: spawn function returned invalid PID\n",
                        timestamp, d.szPeerName);
                fflush(daemonlog);
              }
            }
            else {

              sprintf(d.szPeerName,"IPV6 %02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                                   "%02x%02x:%02x%02x:%02x%02x:%02x%02x - " 
                                   "IPV4 %03d:%03d:%03d:%03d",
                          (int)client.sin6_addr.s6_addr[0],  (int)client.sin6_addr.s6_addr[1],
                          (int)client.sin6_addr.s6_addr[2],  (int)client.sin6_addr.s6_addr[3],
                          (int)client.sin6_addr.s6_addr[4],  (int)client.sin6_addr.s6_addr[5],
                          (int)client.sin6_addr.s6_addr[6],  (int)client.sin6_addr.s6_addr[7],
                          (int)client.sin6_addr.s6_addr[8],  (int)client.sin6_addr.s6_addr[9],
                          (int)client.sin6_addr.s6_addr[10], (int)client.sin6_addr.s6_addr[11],
                          (int)client.sin6_addr.s6_addr[12], (int)client.sin6_addr.s6_addr[13],
                          (int)client.sin6_addr.s6_addr[14], (int)client.sin6_addr.s6_addr[15],
                          (int)client.sin6_addr.s6_addr[12], (int)client.sin6_addr.s6_addr[13],
                          (int)client.sin6_addr.s6_addr[14], (int)client.sin6_addr.s6_addr[15]); 

              fprintf(daemonlog, "%s - CONN-FAIL  HOST: %s Could not spawn extra handler: maximum limit reached\n",
                      timestamp, d.szPeerName);
              fflush(daemonlog);
            }
          }
          else {
        
            if (errno == EINTR)
            {
              fprintf(daemonlog, "%s - WAIT-INT-H poll() interrupted on handler wait\n",
                          timestamp);
              fflush(daemonlog);
              goto RESTART_WAIT; // call poll() again
            }
            else
            {
              ////////////////////////////////////////////////////////////
              // Some error occurred.
              ////////////////////////////////////////////////////////////
              fprintf(daemonlog, "%s - WAIT-ERR-H errno: %10d        poll() returned an error waiting on available handler - \n",
                       timestamp, errno);
              fflush(daemonlog);
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

          fprintf(daemonlog, "%s - SYS-ERROR  errno: %10d         poll returned error\n",
                     timestamp, errno);
          fflush(daemonlog);
        }
        else {

          fprintf(daemonlog, "%s - WAIT-INT-P poll() interrupted\n",
                          timestamp);
          fflush(daemonlog);
        }
      }
      else {

        fprintf(daemonlog, "%s - POLL-WAIT  poll() timed-out\n",
                   timestamp);
        fflush(daemonlog);
      }
    }
  }

  CSLIST_Destructor(&(d.handlers));
  free(d.szDaemonName);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////

pid_t spawnHandler
  (char *szHandler,
   char *szConfig,
   int conn_fd) {

  char *szArgs[5];
  char szDescriptor[8];

  char szRunMode[2];

  pid_t pid;

  HANDLERINFO hi;

  int i;
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

    // find next available handler slot

    d.nextHandlerSlot = -1;
    for (i=0; i<d.maxNumHandlers; i++) {
      if (d.handlerFdSet[i].fd == -1) {
        d.nextHandlerSlot = i;
        break;
      }
    }

    d.handlerFdSet[d.nextHandlerSlot].fd = hi.stream;
    d.handlerFdSet[d.nextHandlerSlot].events = POLLIN;
    CSLIST_Set(d.handlers, &hi, sizeof(HANDLERINFO), (long)d.nextHandlerSlot);

    (d.numHandlers)++;
    close(streamfd[1]); // close child half of stream pipe.

    return pid;
  }
  else
  {
    if (pid == 0)
    {
      // we are the child
      
      fclose(daemonlog); // no need for dameon log file

      close(d.listen_fd);   // handler will not listen for connections

      // if we spawn as a result of having no available handler, then this means
      // we have a valid client socket and we must close it to avoid a 
      // duplicate; the parent will eventually send it to the child via
      // the stream pipe.

      if (conn_fd >= 0) {
        close(conn_fd);
      }

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

      syslog(LOG_INFO, "Spawning handler %s config: %s",szArgs[0], szArgs[3]);

      // When parent exists, send SIGKILL to all children
      prctl(PR_SET_PDEATHSIG, SIGKILL);

      if (execv((const char *)szHandler, szArgs) < 0) {
        syslog(LOG_ERR, "Failed to exec %s: errno: %d",szArgs[0], errno);
        exit(5);
      }
    }
    else
    {
      // some error occurred
      close(streamfd[0]);
      close(streamfd[1]);
      return -1;
    }
  }

  return pid;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////

void 
  signalCatcher
    (int signal) {

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
        count = CSLIST_Count(d.handlers);

        for (i = 0; i < count; i++)
        {
          CSLIST_GetDataRef(d.handlers, (void **)(&phi), i);

          if (phi->pid == pid)
          {

            if (phi->stream > -1)
            {
              close(phi->stream);
            }

            ////////////////////////////////////////////////////////////
            // The handler list is aligned to the stream pipe array;
            // We must set this handler as non-executing rather than
            // remove it from the list because the addition of handlers
            // later on would mis-align the handler pid with its stream
            // pipe. It could also overwrite a valid stream pipe
            // and would render its existing associated handler useless.
            ////////////////////////////////////////////////////////////

            phi->pid = -1;
            phi->state = 0;
            phi->stream = -1;

            d.nextHandlerSlot = i;

            // mark this stream pipe as invalid
            d.handlerFdSet[i].fd = -1;
            d.handlerFdSet[i].events = 0;

            // Decrement number of executing handlers
            (d.numHandlers)--;

            time(&now);
            ts = *localtime(&now);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

            fprintf(daemonlog, "%s - HND-KILL   PID: %d         Handler terminated\n", 
                                 timestamp, pid);
            fflush(daemonlog);

            break;
          }
        }
      }

      break;

    case SIGTERM:


      time(&now);
      ts = *localtime(&now);
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

      fprintf(daemonlog, "%s - DAEMON-END Daemon terminated by SIGTERM signal\n", 
                          timestamp);
      fflush(daemonlog);

      // terminate every child handler

      close(d.listen_fd);

      count = CSLIST_Count(d.handlers);

      for (i = 0; i < count; i++)
      {
        CSLIST_GetDataRef(d.handlers, (void **)(&phi), i);

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

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////

CSRESULT
  CleanupLogs
    (char* szPath,
     char* szPattern,
     int days) {

  int seconds;
  time_t now;
  struct dirent *dir;
  DIR* directory;
  char szFullFileName[1024];
  char* pPattern;
  struct stat fileInfo;

  directory = opendir(szPath);

  seconds = 86400 * days;  

  time(&now);

  now -= seconds;

  if (directory)
  {
    while ((dir = readdir(directory)) != NULL)
    {
      if(dir->d_type==DT_REG){
        strcpy(szFullFileName, szPath);
        strcat(szFullFileName, "/");
        strcat(szFullFileName, dir->d_name);
        
        // get file info
        if (stat(szFullFileName, &fileInfo) >= 0) {

          if (szPattern) {

            pPattern = strstr(dir->d_name, szPattern);

            if (pPattern != NULL) {
              if (pPattern == dir->d_name) {
                if (fileInfo.st_ctime < now) {
                  remove(szFullFileName);
                }
              }
            }
          }
        }
      }
    }
  }

  return CS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////

int
  RunAsDaemon
    (int argc, char **argv) {

  pid_t pid;

  int len;
  int i;
  int fd;

  // Let's duplicate and detach from the calling process

  pid = fork();

  if (pid < 0)
  {
    syslog(LOG_ERR, "fork failure : %m");
    closelog();
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
    syslog(LOG_ERR, "setsid failure : %m");
    closelog();
    return 2;
  }

  // Now, we duplicate again this time to make sure
  // the daemon cannot acquire a controlling terminal.

  pid = fork();

  if (pid < 0)
  {
    return 3;
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

  // change current directory: same as daemon image

  len = strlen(argv[0]);
  while (len > 0) {
    if (argv[0][len] == '/') {
      argv[0][len] = 0;
      break;
    }
    len--;
  }

  if (len == 0) {
    chdir("/");
  }
  else {
    chdir(argv[0]);

  }

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

  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////


