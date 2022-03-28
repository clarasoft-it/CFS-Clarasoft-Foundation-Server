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

#include <clarasoft/cslib.h>
#include <clarasoft/cfsapi.h>

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
     CSLIST *handlers,
     int *NumHandlers);

pid_t 
  spawnExtraHandler
    (char *szHandler,
     char *szConfig,
     int conn_fd,
     int *NumHandlers);

CSRESULT
  CleanupLogs
    (char* szPath,
     char* szPattern,
     int days);

typedef struct tagHANDLERINFO
{

  pid_t pid;  // the handler's PID
  int stream; // stream pipe
  int state;  // child state (1 == executing, 0 == waiting, -1 terminated)

} HANDLERINFO;

/* --------------------------------------------------------------------------
  Globals
-------------------------------------------------------------------------- */

FILE* daemonlog;

time_t now;

struct tm ts;

CSLIST handlers;

CSMAP extraHandlers;

int listen_fd;
int g_NumHandlers;
int iDaemonNameLength;
int iPeerNameSize;
int days;

char* szDaemonName;
char* szProtocolHandler;

char szPeerName[256];
char timestamp[80];
char szLogDir[1024];
char szLogName[256];
char szLogFile[2049];

/////////////////////////////////////////////////////////////////////////////
// A descriptor set for the listener (a single instance) and
// a descriptor set for the handler stream pipes. The number of
// handlers can vary so the descriptor set will be allocated dynamically.
/////////////////////////////////////////////////////////////////////////////

struct pollfd listenerFdSet[1];
struct pollfd *handlerFdSet;

/* --------------------------------------------------------------------------
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
  int len;

  pid_t pid;

  char dummyData;

  char szPort[11];
  char szConfig[99];
  char szHandlerConfig[99];

  socklen_t socklen;

  struct sigaction sa;

  struct sockaddr_in6 client;
  struct sockaddr_in6 server;

  CSRESULT hResult;

  CFSRPS repo;
  CFSRPS_CONFIGINFO cfgi;
  CFSRPS_PARAMINFO cfgpi;

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

  strncpy(szConfig, argv[1], 99);

  ////////////////////////////////////////////////////////////////////////////
  // The list of handler informaton structures must be initialized first
  // because the cleanup handler will use it.
  ////////////////////////////////////////////////////////////////////////////

  handlers = CSLIST_Constructor();

  ////////////////////////////////////////////////////////////////////////////
  // The daemon name could be other than this file name
  // since several implementations of this server could be executing
  // under different names.
  ////////////////////////////////////////////////////////////////////////////

  iDaemonNameLength = strlen(argv[0]);
  szDaemonName = (char *)malloc(iDaemonNameLength * sizeof(char) + 1);
  memcpy(szDaemonName, argv[0], iDaemonNameLength);
  szDaemonName[iDaemonNameLength] = 0;

  ////////////////////////////////////////////////////////////////////////////
  // We must now turn this process into a daemon
  ////////////////////////////////////////////////////////////////////////////

  // Let's duplicate and detach from the calling process

  if (argc >= 3) {
   
    if (argv[2][0] == 'd') {
      syslog (LOG_INFO, "Running in DEBUG mode");
      goto START_DAEMON_DEBUGMODE;
    }
  }

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

  repo = CFSRPS_Constructor();

  strcpy(cfgi.szPath, argv[1]);

  if (CS_FAIL(CFSRPS_LoadConfig(repo, &cfgi)))
  {
    syslog(LOG_ERR, "can't open configuration : %s", cfgi.szPath);
    closelog();
    exit(1);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "LOGDIR", &cfgpi)))
  {
     strcpy(szLogDir, "/var/log");
  }
  else {
     strcpy(szLogDir, cfgpi.szValue);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "LOGFILE_RETAIN", &cfgpi)))
  {
     days = atoi(cfgpi.szValue);
  }
  else {
     days = 7;
  }

  // Create log file
  // use configuration path to name the log file; for this, we 
  // clean up the configuration path

  i = 0;
  while (argv[1][i] != 0) {
    if (!isalpha(argv[1][i]) && argv[1][i] != '_') {
      szLogName[i] = '-';
    }
    else {
      szLogName[i] = argv[1][i];
    }
    i++;
  }

  CleanupLogs(szLogDir, szLogName, days);

  time(&now);
  ts = *localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M-%S", &ts);

  sprintf(szLogFile, 
          "%s/%s-%s-log.txt",
          szLogDir, szLogName, timestamp);    

  daemonlog = fopen(szLogFile, "w+");

  if (!daemonlog) {
    syslog(LOG_ERR, "can't open log file : %s", szLogFile);
    closelog();
    exit(2);
  }

  // No longer need syslog, we switch to daemon log file
  syslog(LOG_INFO, "running with configuration %s", cfgi.szPath);
  closelog( );

  if (CS_FAIL(CFSRPS_LookupParam(repo, "PORT", &cfgpi)))
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-ERR   PORT parameter not found()\n",
            timestamp);
    fflush(daemonlog);

    exit(3);
  }

  strcpy(szPort, cfgpi.szValue);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "HANDLER", &cfgpi)))
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-ERR   HANDLER parameter not found\n",
            timestamp);
    fflush(daemonlog);

    exit(4);
  }

  szProtocolHandler =
      (char *)malloc(sizeof(char) * strlen(cfgpi.szValue) + 1);

  strcpy(szProtocolHandler, cfgpi.szValue);

  if (CS_FAIL(CFSRPS_LookupParam(repo, "HANDLER_CONFIG", &cfgpi)))
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
    strncpy(szHandlerConfig, cfgpi.szValue, 99);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "LISTEN_BACKLOG", &cfgpi)))
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
    backlog = atoi(cfgpi.szValue);
  }

  //if (CS_FAIL(CFSRPS_LookupParam(repo, "RES_NUM_HANDLERS", &cfgpi)))
  if (CFSRPS_LookupParam(repo, "RES_NUM_HANDLERS", &cfgpi) == CS_FAILURE)
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-WARN  RES_NUM_HANDLERS parameter not found - using default value of 0\n",
            timestamp);
    fflush(daemonlog);

    initialNumHandlers = 0; // this means all handlers will be transcient
  }
  else
  {
    initialNumHandlers = atoi(cfgpi.szValue);
  }

  if (CS_FAIL(CFSRPS_LookupParam(repo, "TRS_NUM_HANDLERS", &cfgpi)))
  {
    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONF-WARN  TRS_NUM_HANDLERS parameter not found - using default value\n",
            timestamp);
    fflush(daemonlog);

    maxNumHandlers = initialNumHandlers == 0 ? 1 : initialNumHandlers;
  }
  else
  {
    maxNumHandlers = initialNumHandlers + atoi(cfgpi.szValue);
  }

  time(&now);
  ts = *localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

  fprintf(daemonlog, "%s - DAEMON-STR Starting daemon - %s "
                     "config: %s "
                     "port: %s "
                     "handler: %s\n", 
                     timestamp, szDaemonName, argv[1], szPort, szProtocolHandler);
  fflush(daemonlog);

  CFSRPS_Destructor(&repo);

  // allocate one more slote because number of handlers could be zero
  handlerFdSet = (struct pollfd *)
      malloc((initialNumHandlers + 1) * sizeof(struct pollfd));

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
    spawnHandler(szProtocolHandler, szHandlerConfig, handlers, &g_NumHandlers);
  }

  ///////////////////////////////////////////////////////////////////////////
  // This is the main listening loop...
  // wait for client connections and dispatch to child handler
  ///////////////////////////////////////////////////////////////////////////

  for (;;)
  {
    numDescriptors = poll(listenerFdSet, 1, -1);

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

      socklen = sizeof(struct sockaddr_in6);
      memset(&client, 0, sizeof(struct sockaddr_in6));

      conn_fd = accept(listen_fd, (struct sockaddr *)&client, &socklen);

      if (conn_fd < 0)
      {
        if (errno == EINTR)
        {
          // At this point, we know there is a connection pending
          // so we must restart accept() again

          time(&now);
          ts = *localtime(&now);
          strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

          fprintf(daemonlog, "%s - ACCP-INT   interrupted on accept()\n",
                  timestamp);
          fflush(daemonlog);

          goto RESTART_ACCEPT; // accept was interrupted by a signal
        }
        else {

          time(&now);
          ts = *localtime(&now);
          strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

          fprintf(daemonlog, "%s - ACCP-ERR   errno: %10d accept() returned an error\n",
                  timestamp, errno);
          fflush(daemonlog);
        }
      }
      else {

        ///////////////////////////////////////////////////////////////
        // Log information on the peer
        ///////////////////////////////////////////////////////////////

        time(&now);
        ts = *localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

        sprintf(szPeerName,"IPV6 %02x%02x:%02x%02x:%02x%02x:%02x%02x:"
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

        fprintf(daemonlog, "%s - CONN       HOST: %s Connection received\n", 
                timestamp, szPeerName);

        fflush(daemonlog);

        //////////////////////////////////////////////////////////////////
        // Find an available handler; we first wait on handler descriptors
        // assuming they have all been used at least once.
        //////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////
        // BRANCHING LABEL
        RESTART_WAIT:
        //////////////////////////////////////////////////////////////////

        numDescriptors = poll(handlerFdSet, initialNumHandlers, 0);

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

                  CSLIST_GetDataRef(handlers, (void**)&phi, i);

                  time(&now);
                  ts = *localtime(&now);
                  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

                  fprintf(daemonlog, "%s - CONN-HND   MODE: RES  PID: %10d         Resident handler starting\n", 
                          timestamp, phi->pid);

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

                  CSLIST_GetDataRef(handlers, (void**)&phi, i);

                  time(&now);
                  ts = *localtime(&now);
                  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

                  fprintf(daemonlog, "%s - CONN-ERR   errno: %10d       Failed to send socket descriptor to handler\n", 
                          timestamp, errno);

                  fflush(daemonlog);
                }
              }
              else
              {
                if (rc < 0) {
                  if (errno == EINTR)
                  {
                    time(&now);
                    ts = *localtime(&now);
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

                    fprintf(daemonlog, "%s - HND-RECV-H recv() interrupted while reading handler stream pipe\n",
                            timestamp);
                    fflush(daemonlog);

                    goto RESTART_RECV; // call recv() again
                  }
                }
                else {
                  
                  ///////////////////////////////////////////////////
                  // handler closed connection
                  ///////////////////////////////////////////////////
                  time(&now);
                  ts = *localtime(&now);
                  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

                  fprintf(daemonlog, "%s - HND-DISC-H handler closed connection on stream pipe\n",
                          timestamp);
                  fflush(daemonlog);
                }
              }
            }
          } // for
        }
        else {

          if (numDescriptors == 0) {

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
                                szHandlerConfig, conn_fd, &g_NumHandlers);
            }
            else {

              time(&now);
              ts = *localtime(&now);
              strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

              fprintf(daemonlog, "%s - EXEC-FAIL  Could not spawn extra handler: maximum limit reached\n",
                          timestamp);
              fflush(daemonlog);
            }
          }
          else {
        
            if (errno == EINTR)
            {
              time(&now);
              ts = *localtime(&now);
              strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

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

              time(&now);
              ts = *localtime(&now);
              strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);
              fprintf(daemonlog, "%s - WAIT-ERR-H errno: %10d        poll() returned an error waiting on available handler - \n",
                       timestamp, errno);
              fflush(daemonlog);
            }
          } 
        }
      }

      close(conn_fd);
    }
    else {

      if (numDescriptors < 0)
      {
        if (errno != EINTR)
        {
          //////////////////////////////////////////////////////////////////
          // Some error occurred.
          //////////////////////////////////////////////////////////////////

          time(&now);
          ts = *localtime(&now);
          strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

          fprintf(daemonlog, "%s - SYS-ERROR  errno: %10d         poll returned error\n",
                     timestamp, errno);
          fflush(daemonlog);
        }
        else {

          time(&now);
          ts = *localtime(&now);
          strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

          fprintf(daemonlog, "%s - WAIT-INT-P poll() interrupted\n",
                          timestamp);
          fflush(daemonlog);
        }
      }
      else {

        time(&now);
        ts = *localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);
        fprintf(daemonlog, "%s - POLL-WAIT  poll() timed-out\n",
                   timestamp);
        fflush(daemonlog);
      }
    }
  }

  CSLIST_Destructor(&handlers);
  free(szDaemonName);

  return 0;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */

pid_t spawnHandler
  (char *szHandler,
   char *szConfig,
   CSLIST *handlers,
   int *NumHandlers) {

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

    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - RHND-STR   PID: %10d         Starting resident handler\n", 
                timestamp, hi.pid);
    fflush(daemonlog);

    handlerFdSet[*NumHandlers].fd = hi.stream;
    handlerFdSet[*NumHandlers].events = POLLIN;

    CSLIST_Insert(handlers, &hi, sizeof(HANDLERINFO), CSLIST_BOTTOM);

    close(streamfd[1]); // close child half of stream pipe.

    (*NumHandlers)++;

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
      return -1;
    }
  }

  return pid;
}

/* --------------------------------------------------------------------------
-------------------------------------------------------------------------- */

pid_t 
  spawnExtraHandler
    (char *szHandler,
     char *szConfig,
     int conn_fd,
     int *NumHandlers) {

  pid_t pid;

  char *szArgs[5];

  char szDescriptor[8];
  char szRunMode[2];

  pid = fork();

  if (pid > 0)
  {

    time(&now);
    ts = *localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

    fprintf(daemonlog, "%s - CONN-HND   MODE: TRS  PID: %10d         Transcient handler starting\n", 
            timestamp, pid);

    fflush(daemonlog);
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

      // When parent exists, send SIGKILL to all children
      prctl(PR_SET_PDEATHSIG, SIGKILL);

      if (execv((const char *)szHandler, szArgs) < 0) {
        exit(6);
      }
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
-------------------------------------------------------------------------- */

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

              time(&now);
              ts = *localtime(&now);
              strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ts);

              fprintf(daemonlog, "%s - HND-KILL   PID: %d         Handler terminated\n", 
                                 timestamp, pid);
              fflush(daemonlog);
            }

            break;
          }
        }

        // Decrement number of executing handlers; note that
        // if a resident handler is terminated, this will
        // allow one more transcient handler since we are 
        // decrementing the number of active handlers. In essence,
        // the situation could evolve in such a way that only 
        // transcient handlers are spawned if all resident
        // handlers are terminated.

        g_NumHandlers--;
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



