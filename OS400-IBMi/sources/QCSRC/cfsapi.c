/* ===========================================================================

  Clarasoft Foundation Server - OS/400
  cfsapi.c

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

=========================================================================== */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <gskssl.h>
#include <limits.h>
#include <netdb.h>

#include <QUSEC.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/un.h>

#include "qcsrc/cslib.h"
#include "qcsrc/cfsrepo.h"

#define CFS_NTOP_ADDR_MAX             (1025)
#define CFS_NTOP_PORT_MAX             (9)

#define CFS_SSL_MAXRECORDSIZE         (16384)

// Operation codes

#define CFS_OPER_WAIT                 (0x00010000)
#define CFS_OPER_READ                 (0x01010000)
#define CFS_OPER_WRITE                (0x01020000)
#define CFS_OPER_CONFIG               (0x01030000)

// Diagnostic codes

#define CFS_DIAG_CONNCLOSE            (0x0000F001)
#define CFS_DIAG_WOULDBLOCK           (0x0000F002)
#define CFS_DIAG_READNOBLOCK          (0x0000F003)
#define CFS_DIAG_WRITENOBLOCK         (0x0000F004)
#define CFS_DIAG_TIMEDOUT             (0x0000F005)
#define CFS_DIAG_ALLDATA              (0x0000F006)
#define CFS_DIAG_PARTIALDATA          (0x0000F007)
#define CFS_DIAG_NODATA               (0x0000F008)
#define CFS_DIAG_INVALIDSIZE          (0x0000F009)
#define CFS_DIAG_ENVOPEN              (0x0000F00A)
#define CFS_DIAG_APPID                (0x0000F00B)
#define CFS_DIAG_SESSIONTYPE          (0x0000F00C)
#define CFS_DIAG_ENVINIT              (0x0000F00D)
#define CFS_DIAG_SOCOPEN              (0x0000F00E)
#define CFS_DIAG_SETFD                (0x0000F00F)
#define CFS_DIAG_SOCINIT              (0x0000F010)
#define CFS_DIAG_NOTFOUND             (0x0000F011)
#define CFS_DIAG_SESSIONINIT          (0x0000F012)
#define CFS_DIAG_SEQNUM_EXHAUSTED     (0x0000F013)
#define CFS_DIAG_LIBNOTFOUND          (0x0000F014)
#define CFS_DIAG_SRVCNOTFOUND         (0x0000F015)
#define CFS_DIAG_PROCNOTFOUND         (0x0000F016)
#define CFS_DIAG_SECMODE              (0x00000017)
#define CFS_DIAG_SYSTEM               (0x0000FFFE)
#define CFS_DIAG_UNKNOWN              (0x0000FFFF)

typedef struct tagCFS_SESSION CFS_SESSION;

//////////////////////////////////////////////////////////////////////////////
//  Virtual function table interface
//////////////////////////////////////////////////////////////////////////////

typedef struct tagCFSVTBL {

  CSRESULT
    (*CFS_Receive)
      (CFS_SESSION*,
       char*,
       long*,
       long);

  CSRESULT
    (*CFS_ReceiveRecord)
      (CFS_SESSION*,
       char*,
       long*,
       long);

  CSRESULT
    (*CFS_Send)
      (CFS_SESSION*,
       char*,
       long*,
       long);

  CSRESULT
    (*CFS_SendRecord)
      (CFS_SESSION*,
       char*,
       long*,
       long);

} CFSVTBL;

typedef CFSVTBL* LPCFSVTBL;

typedef struct tagCFSERRINFOSTRUCT {

  CSRESULT csresult;
  int m_errno;
  int secrc;

} CFSERRINFOSTRUCT;

typedef struct tagCFSENV {

  gsk_handle ssl_henv;

  CSLIST Config_SecureEnv;
  CSLIST Config_SecureSession;

  int connectTimeout;
  int readTimeout;
  int writeTimeout;
  int secMode;

} CFSENV;

typedef struct tagSESSIONINFO {

  char* szHost;
  char* szPort;
  char* szLocalPort;
  char* szRemotePort;

} SESSIONINFO;

typedef struct tagCFS_SESSION {

  LPCFSVTBL lpVtbl;

  char  szPort[11];
  char  szConfigName[65];
  char  szHostName[256];

  gsk_handle ssl_hsession;

  int secMode;
  int connfd;
  int connectTimeout;
  int readTimeout;
  int writeTimeout;

  int32_t size;

  CFSENV*  pEnv;

  SESSIONINFO info;
  CFSERRINFOSTRUCT errInfo;

} CFS_SESSION;

//////////////////////////////////////////////////////////////////////////////
//
// CFS_PRV_SetBlocking
//
// This function sets a socket descriptor to either blocking
// or non-blocking mode.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_PRV_SetBlocking
    (int connfd,
     int blocking) {

    /* Save the current flags */

    int flags = fcntl(connfd, F_GETFL, 0);
    if (flags == -1)
        return 0;

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    if (fcntl(connfd, F_SETFL, flags) == -1)
      return CS_FAILURE;

    return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Read
//
// This function reads a non-secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_Read
    (CFS_SESSION* This,
     char* buffer,
     long* maxSize,
     long toSlices) {

   int rc;
   int readSize;
   int to;

   struct pollfd fdset[1];

   to = This->readTimeout * toSlices;

   readSize =
     (int)(*maxSize > INT_MAX ?
                      INT_MAX :
                      *maxSize);

   fdset[0].fd = This->connfd;
   fdset[0].events = POLLIN;

   ////////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted
   // poll call. An interrupted system call may result from
   // a caught signal and will have errno set to EINTR. We
   // must call poll again.

   CFS_READ_POLL:

   //
   ////////////////////////////////////////////////////////////

   rc = poll(fdset, 1, to >= 0 ? to: -1);

   if (rc == 1) {

     if (!(fdset[0].revents & POLLIN)) {

       /////////////////////////////////////////////////////////
       // If we get anything other than POLLIN
       // this means an error occurred.
       /////////////////////////////////////////////////////////

       *maxSize = 0;
       This->errInfo.m_errno = 0;
       This->errInfo.secrc = 0;

       This->errInfo.csresult =   CS_FAILURE
                                | CFS_OPER_WAIT
                                | CFS_DIAG_SYSTEM;

       return This->errInfo.csresult;
     }
   }
   else {

     if (rc == 0) {

       *maxSize = 0;
       This->errInfo.m_errno = 0;
       This->errInfo.secrc = 0;

       This->errInfo.csresult =   CS_FAILURE
                                | CFS_OPER_WAIT
                                | CFS_DIAG_TIMEDOUT;

       return This->errInfo.csresult;
     }
     else {

       if (errno == EINTR) {

         ///////////////////////////////////////////////////
         // poll() was interrupted by a signal
         // or the kernel could not allocate an
         // internal data structure. We will call
         // poll() again.
         ///////////////////////////////////////////////////

         goto CFS_READ_POLL;
       }
       else {

         *maxSize = 0;
         This->errInfo.m_errno = errno;
         This->errInfo.secrc = 0;

         This->errInfo.csresult =   CS_FAILURE
                                  | CFS_OPER_WAIT
                                  | CFS_DIAG_SYSTEM;

         return This->errInfo.csresult;
       }
     }
   }

   /////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted
   // recv() call. An interrupted system call may result
   // from a signal and will have errno set to EINTR.
   // We must call recv() again.

   CFS_READ:

   //
   /////////////////////////////////////////////////////////

   rc = recv(This->connfd, buffer, readSize, 0);

   if (rc < 0) {

     if (errno == EINTR) {

       ///////////////////////////////////////////////////
       // recv() was interrupted by a signal
       // or the kernel could not allocate an
       // internal data structure. We will call
       // recv() again.
       ///////////////////////////////////////////////////

       goto CFS_READ;
     }
     else {

       *maxSize = 0;
       This->errInfo.m_errno = errno;
       This->errInfo.secrc = 0;

       This->errInfo.csresult =   CS_FAILURE
                                | CFS_OPER_READ
                                | CFS_DIAG_SYSTEM;

       return This->errInfo.csresult;
     }
   }
   else {

     if (rc == 0) {

       /////////////////////////////////////////////////////////////////
       // This indicates a connection close; we are done.
       /////////////////////////////////////////////////////////////////

       *maxSize = 0;
       This->errInfo.m_errno = 0;
       This->errInfo.secrc = 0;

       This->errInfo.csresult =   CS_FAILURE
                                | CFS_OPER_READ
                                | CFS_DIAG_CONNCLOSE;

       return This->errInfo.csresult;
     }
   }

   *maxSize = rc;
   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_ReadRecord
//
// This function reads a specific number of bytes from a non secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_ReadRecord
    (CFS_SESSION* This,
     char* buffer,
     long* size,
     long toSlices) {

   int rc;
   int readSize;
   int to;

   struct pollfd fdset[1];

   long leftToRead;
   long offset;

   to = toSlices * This->readTimeout;

   leftToRead = *size;

   *size = 0;
   offset = 0;

   //////////////////////////////////////////////////////////////////////////
   // The total record size exceeds the maximum int value; we will
   // read up to int size at a time until we send the entire buffer.
   //////////////////////////////////////////////////////////////////////////

   readSize =
     (int)(leftToRead > INT_MAX ?
                        INT_MAX :
                        leftToRead);

   do {

     ////////////////////////////////////////////////////////////
     // This branching label for restarting an interrupted
     // poll call. An interrupted system call may result from
     // a caught signal and will have errno set to EINTR. We
     // must call poll again.

     CFS_READRECORD_POLL:

     //
     ////////////////////////////////////////////////////////////

     fdset[0].fd = This->connfd;
     fdset[0].events = POLLIN;

     rc = poll(fdset, 1, to >= 0 ? to: -1);

     if (rc == 1) {

       /////////////////////////////////////////////////////////
       // If we get anything other than POLLIN
       // this means we got an error.
       /////////////////////////////////////////////////////////

       if (!(fdset[0].revents & POLLIN)) {

         This->errInfo.m_errno = 0;
         This->errInfo.secrc = 0;

         This->errInfo.csresult =   CS_FAILURE
                                  | CFS_OPER_WAIT
                                  | CFS_DIAG_SYSTEM;

         return This->errInfo.csresult;
       }
     }
     else {

       if (rc == 0) {

         This->errInfo.m_errno = 0;
         This->errInfo.secrc = 0;

         This->errInfo.csresult =   CS_FAILURE
                                  | CFS_OPER_WAIT
                                  | CFS_DIAG_TIMEDOUT;

         return This->errInfo.csresult;
       }
       else {

         if (errno == EINTR) {

           ///////////////////////////////////////////////////
           // poll() was interrupted by a signal
           // or the kernel could not allocate an
           // internal data structure. We will call
           // poll() again.
           ///////////////////////////////////////////////////

           goto CFS_READRECORD_POLL;
         }
         else {

           This->errInfo.m_errno = errno;
           This->errInfo.secrc = 0;

           This->errInfo.csresult =   CS_FAILURE
                                    | CFS_OPER_WAIT
                                    | CFS_DIAG_SYSTEM;

           return This->errInfo.csresult;
         }
       }
     }

     /////////////////////////////////////////////////////////
     // This branching label for restarting an interrupted
     // recv() call. An interrupted system call may result
     // from a signal and will have errno set to EINTR.
     // We must call recv() again.

     CFS_READRECORD:

     //
     /////////////////////////////////////////////////////////

     rc = recv(This->connfd, buffer + offset, readSize, 0);

     if (rc < 0) {

       if (errno == EINTR) {

         ///////////////////////////////////////////////////
         // recv() was interrupted by a signal
         // or the kernel could not allocate an
         // internal data structure. We will call
         // recv() again.
         ///////////////////////////////////////////////////

         goto CFS_READRECORD;
       }
       else {

         This->errInfo.m_errno = errno;
         This->errInfo.secrc = 0;

         This->errInfo.csresult =   CS_FAILURE
                                  | CFS_OPER_READ
                                  | CFS_DIAG_SYSTEM;

         return This->errInfo.csresult;
       }
     }
     else {

       if (rc == 0) {

         /////////////////////////////////////////////////////////////////
         // This indicates a connection close; we are done.
         /////////////////////////////////////////////////////////////////
         This->errInfo.m_errno = 0;
         This->errInfo.secrc = 0;

         This->errInfo.csresult =   CS_FAILURE
                                  | CFS_OPER_READ
                                  | CFS_DIAG_CONNCLOSE;

         return This->errInfo.csresult;
       }
       else {

         /////////////////////////////////////////////////////////////////
         // We read some data (maybe as much as required) ...
         // if not, we will continue reading for as long
         // as we don't block before receiving data.
         /////////////////////////////////////////////////////////////////

         offset += rc;
         *size += rc;
         leftToRead -= rc;

         readSize =
           (int)(leftToRead > INT_MAX ?
                              INT_MAX :
                              leftToRead);
       }
     }
   }
   while (leftToRead > 0);

   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SecureRead
//
// This function reads a secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureRead
    (CFS_SESSION* This,
     char* buffer,
     long* maxSize,
     long toSlices) {

  int readSize;
  int to;
  int iSSLResult;

  struct pollfd fdset[1];

  // Set read timeout
  to = toSlices * This->writeTimeout;

  ////////////////////////////////////////////////////////////
  // This branching label for restarting an interrupted
  // gsk_attribute_set_numeric_value call.

  CFS_SECURE_READ_SET_TIMEOUT:

  //
  ////////////////////////////////////////////////////////////

  iSSLResult =
    gsk_attribute_set_numeric_value
                       (This->pEnv->ssl_henv,
                        GSK_IBMI_READ_TIMEOUT,
                        to);

  if (iSSLResult != GSK_OK) {
    if (iSSLResult == GSK_ERROR_IO) {
      if (errno == EINTR) {
        goto CFS_SECURE_READ_SET_TIMEOUT;
      }
      This->errInfo.m_errno = errno;
    }
    else {
      This->errInfo.m_errno = 0;
    }

    This->errInfo.secrc    = iSSLResult;

    This->errInfo.csresult =   CS_FAILURE
                             | CFS_OPER_READ
                             | CFS_DIAG_SYSTEM;
    *maxSize = 0;

    return This->errInfo.csresult;
  }

  ////////////////////////////////////////////////////////////
  // The maximum number of bytes returned from a single
  // GSKKit read function is 16k
  ////////////////////////////////////////////////////////////

  readSize =
     (int)(*maxSize) > CFS_SSL_MAXRECORDSIZE ?
                       CFS_SSL_MAXRECORDSIZE :
                       (int)(*maxSize);

  ////////////////////////////////////////////////////////////
  // This branching label for restarting an interrupted
  // gsk_secure_soc_read call.

  CFS_SECURE_READ:

  //
  ////////////////////////////////////////////////////////////

  iSSLResult = gsk_secure_soc_read(This->ssl_hsession,
                                   buffer,
                                   readSize,
                                   &readSize);

  if (iSSLResult != GSK_OK) {

    if (iSSLResult == GSK_ERROR_IO) {

      if (errno == EINTR) {
        goto CFS_SECURE_READ;
      }

      This->errInfo.m_errno    = errno;
      This->errInfo.secrc    = iSSLResult;

      This->errInfo.csresult =   CS_FAILURE
                               | CFS_OPER_READ
                               | CFS_DIAG_SYSTEM;

      return This->errInfo.csresult;
    }
    else {

      *maxSize = 0;
      This->errInfo.m_errno = 0;
      This->errInfo.secrc = iSSLResult;

      switch(iSSLResult) {

        case GSK_WOULD_BLOCK:

          This->errInfo.csresult =   CS_FAILURE
                                   | CFS_OPER_READ
                                   | CFS_DIAG_WOULDBLOCK;
          break;

        case GSK_IBMI_ERROR_TIMED_OUT:

          This->errInfo.csresult =   CS_FAILURE
                                   | CFS_OPER_READ
                                   | CFS_DIAG_TIMEDOUT;
          break;

        case GSK_OS400_ERROR_CLOSED:
        case GSK_ERROR_SOCKET_CLOSED:

          This->errInfo.csresult =   CS_FAILURE
                                   | CFS_OPER_READ
                                   | CFS_DIAG_CONNCLOSE;
          break;

        case GSK_ERROR_SEQNUM_EXHAUSTED:

          This->errInfo.csresult =   CS_FAILURE
                                   | CFS_OPER_READ
                                   | CFS_DIAG_SEQNUM_EXHAUSTED;
          break;

        default:

          This->errInfo.csresult =   CS_FAILURE
                                   | CFS_OPER_READ
                                   | CFS_DIAG_SYSTEM;
          break;
      }

      return This->errInfo.csresult;
    }
  }
  else {

    if (readSize == 0) {

      /////////////////////////////////////////////////////////////////
      // This indicates a connection close
      /////////////////////////////////////////////////////////////////

      *maxSize = 0;
      This->errInfo.m_errno = 0;
      This->errInfo.secrc = iSSLResult;

      This->errInfo.csresult =   CS_FAILURE
                               | CFS_OPER_READ
                               | CFS_DIAG_CONNCLOSE;

      return This->errInfo.csresult;
    }
  }

  *maxSize = (long)readSize;
  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SecureReadRecord
//
// This function reads a specific number of bytes on a secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureReadRecord
    (CFS_SESSION* This,
     char* buffer,
     long* size,
     long toSlices) {

  int readSize;
  int to;
  int iSSLResult;

  long leftToRead;
  long offset;

  struct pollfd fdset[1];

  leftToRead = *size;
  *size = 0;
  offset = 0;

  //////////////////////////////////////////////////////////////////////////
  // If the total record size exceeds the maximum GSK SSL data size value,
  // we will read up to 16k at a time until we reach the desired size.
  //////////////////////////////////////////////////////////////////////////

  readSize =
       (int)(leftToRead > CFS_SSL_MAXRECORDSIZE ?
                          CFS_SSL_MAXRECORDSIZE :
                          leftToRead);

  fdset[0].fd = This->connfd;
  fdset[0].events = POLLIN;

  // Set read timeout
  to = toSlices * This->readTimeout;

  ////////////////////////////////////////////////////////////
  // This branching label for restarting an interrupted
  // gsk_attribute_set_numeric_value call.

  CFS_SECURE_READRECORD_SET_TIMEOUT:

  //
  ////////////////////////////////////////////////////////////

  iSSLResult =
    gsk_attribute_set_numeric_value
                  (This->pEnv->ssl_henv,
                   GSK_IBMI_READ_TIMEOUT,
                   to);

  if (iSSLResult != GSK_OK) {
    if (iSSLResult == GSK_ERROR_IO) {
      if (errno == EINTR) {
        goto CFS_SECURE_READRECORD_SET_TIMEOUT;
      }
      This->errInfo.m_errno = errno;
    }
    else {
      This->errInfo.m_errno = 0;
    }

    This->errInfo.secrc    = iSSLResult;

    This->errInfo.csresult =   CS_FAILURE
                             | CFS_OPER_READ
                             | CFS_DIAG_SYSTEM;

    return This->errInfo.csresult;
  }

  do {

    ////////////////////////////////////////////////////////////
    // This branching label for restarting an interrupted
    // gsk_secure_soc_read call.

    CFS_SECURE_READRECORD:

    //
    ////////////////////////////////////////////////////////////

    iSSLResult = gsk_secure_soc_read(This->ssl_hsession,
                                     buffer + offset,
                                     readSize,
                                     &readSize);

    if (iSSLResult != GSK_OK) {

      if (iSSLResult == GSK_ERROR_IO) {

        if (errno == EINTR) {
          // system call interruption; recall function
          goto CFS_SECURE_READRECORD;
        }

        This->errInfo.m_errno    = errno;
        This->errInfo.secrc    = iSSLResult;

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_READ
                                 | CFS_DIAG_SYSTEM;

        return This->errInfo.csresult;
      }
      else {

        This->errInfo.m_errno = 0;
        This->errInfo.secrc = iSSLResult;

        switch(iSSLResult) {

          case GSK_WOULD_BLOCK:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_READ
                                     | CFS_DIAG_WOULDBLOCK;
            break;

          case GSK_IBMI_ERROR_TIMED_OUT:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_READ
                                     | CFS_DIAG_TIMEDOUT;
            break;

          case GSK_OS400_ERROR_CLOSED:
          case GSK_ERROR_SOCKET_CLOSED:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_READ
                                     | CFS_DIAG_CONNCLOSE;
            break;

          case GSK_ERROR_SEQNUM_EXHAUSTED:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_READ
                                     | CFS_DIAG_SEQNUM_EXHAUSTED;
            break;

          default:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_READ
                                     | CFS_DIAG_SYSTEM;
            break;
        }

        return This->errInfo.csresult;
      }
    }
    else {

      if (readSize == 0) {

        /////////////////////////////////////////////////////////////////
        // This indicates a connection close
        /////////////////////////////////////////////////////////////////

        *size = 0;
        This->errInfo.m_errno = 0;
        This->errInfo.secrc = iSSLResult;

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_READ
                                 | CFS_DIAG_CONNCLOSE;

        return This->errInfo.csresult;
      }
      else {

        offset += readSize;
        *size +=  readSize;
        leftToRead -= readSize;

        readSize =
            (int)(leftToRead > CFS_SSL_MAXRECORDSIZE ?
                               CFS_SSL_MAXRECORDSIZE :
                               leftToRead);
      }
    }
  }
  while (leftToRead > 0);

  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SecureWrite
//
// This function writes to a secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureWrite
    (CFS_SESSION* This,
     char* buffer,
     long* maxSize,
     long toSlices) {

  int writeSize;
  int to;
  int iSSLResult;

  struct pollfd fdset[1];

  // no need for this: to = toSlices * This->writeTimeout;

  //////////////////////////////////////////////////////////////////////////
  // The total record size exceeds the maximum int value; we will
  // write up to int size at a time.
  //////////////////////////////////////////////////////////////////////////

  writeSize =
        (int)(*maxSize > INT_MAX ?
                         INT_MAX :
                         *maxSize);

  //////////////////////////////////////////////////////////////////////////
  // We first try to write on the socket.
  //////////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////
  // This branching label for restarting an interrupted
  // gsk_secure_soc_write call.

  CFS_SECURE_WRITE:

  //
  ////////////////////////////////////////////////////////////

  iSSLResult = gsk_secure_soc_write(This->ssl_hsession,
                                    buffer,
                                    writeSize,
                                    &writeSize);

  if (errno == EINTR) {

    //////////////////////////////////////////////////////
    // gsk_secure_soc_write was interrupted by a signal.
    // we must restart gsk_secure_soc_write.
    //////////////////////////////////////////////////////

    goto CFS_SECURE_WRITE;
  }

  if (iSSLResult == GSK_OK) {

    if (writeSize == 0) {

      /////////////////////////////////////////////////////////////////
      // This indicates a connection close; we are done.
      /////////////////////////////////////////////////////////////////

      *maxSize = 0;
      This->errInfo.m_errno = errno;
      This->errInfo.secrc = iSSLResult;

      This->errInfo.csresult =   CS_FAILURE
                               | CFS_OPER_WRITE
                               | CFS_DIAG_CONNCLOSE;
    }
    else {
      *maxSize = writeSize;
      This->errInfo.csresult = CS_SUCCESS;
    }
  }
  else {

    *maxSize = 0;
    This->errInfo.m_errno = errno;
    This->errInfo.secrc = iSSLResult;

    switch(iSSLResult) {

      case GSK_WOULD_BLOCK:


        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_WRITE
                                 | CFS_DIAG_WOULDBLOCK;
        break;

      case GSK_ERROR_SOCKET_CLOSED:
      case GSK_IBMI_ERROR_CLOSED:

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_WRITE
                                 | CFS_DIAG_CONNCLOSE;
        break;

      case GSK_ERROR_SEQNUM_EXHAUSTED:

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_WRITE
                                 | CFS_DIAG_SEQNUM_EXHAUSTED;
        break;

      default:

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_WRITE
                                 | CFS_DIAG_SYSTEM;
        break;
    }
  }

  return This->errInfo.csresult;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SecureWriteRecord
//
// This function writes a specific number of bytes to a secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureWriteRecord
    (CFS_SESSION* This,
     char* buffer,
     long* size,
     long toSlices) {

  int rc;
  int writeSize;
  int bytes;
  int to;
  int iSSLResult;

  long leftToWrite;
  long offset;

  struct pollfd fdset[1];

  to = toSlices * This->writeTimeout;

  leftToWrite = *size;
  *size       = 0;
  offset      = 0;

  do {

    //////////////////////////////////////////////////////////////////////////
    // The total record size exceeds the maximum int value; we will
    // write up to int size at a time.
    //////////////////////////////////////////////////////////////////////////

    writeSize =
         (int)(leftToWrite > INT_MAX ?
                             INT_MAX :
                             leftToWrite);

    ////////////////////////////////////////////////////////////
    // This branching label for restarting an interrupted
    // gsk_secure_soc_write call.

    CFS_SECURE_WRITERECORD:

    //
    ////////////////////////////////////////////////////////////

    iSSLResult = gsk_secure_soc_write(This->ssl_hsession,
                                      buffer,
                                      writeSize,
                                      &writeSize);

    if (iSSLResult != GSK_OK) {

      if (iSSLResult == GSK_ERROR_IO) {

        if (errno == EINTR) {
          // system call interruption; recall function
          goto CFS_SECURE_WRITERECORD;
        }

        This->errInfo.m_errno = errno;
        This->errInfo.secrc = iSSLResult;

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_READ
                                 | CFS_DIAG_SYSTEM;

        return This->errInfo.csresult;
      }
      else {

        This->errInfo.m_errno = errno;
        This->errInfo.secrc = iSSLResult;

        switch(iSSLResult) {

          case GSK_WOULD_BLOCK:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_WRITE
                                     | CFS_DIAG_WOULDBLOCK;

            break;

          case GSK_ERROR_SOCKET_CLOSED:
          case GSK_IBMI_ERROR_CLOSED:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_WRITE
                                     | CFS_DIAG_CONNCLOSE;

            break;

          case GSK_ERROR_SEQNUM_EXHAUSTED:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_WRITE
                                     | CFS_DIAG_SEQNUM_EXHAUSTED;

            break;

          default:

            This->errInfo.csresult =   CS_FAILURE
                                     | CFS_OPER_WRITE
                                     | CFS_DIAG_SYSTEM;

            break;
        }

        return This->errInfo.csresult;
      }
    }
    else {

      if (writeSize == 0) {

        /////////////////////////////////////////////////////////////////
        // This indicates a connection close; we are done.
        /////////////////////////////////////////////////////////////////
        This->errInfo.m_errno = errno;
        This->errInfo.secrc = iSSLResult;

        This->errInfo.csresult =   CS_FAILURE
                                 | CFS_OPER_WRITE
                                 | CFS_DIAG_CONNCLOSE;

        return This->errInfo.csresult;
      }
      else {

        offset += writeSize;
        *size += writeSize;
        leftToWrite -= writeSize;

        writeSize =
               (int)(leftToWrite > INT_MAX ?
                                   INT_MAX :
                                   leftToWrite);
      }
    }
  }
  while (leftToWrite > 0);

  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Write
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_Write
    (CFS_SESSION* This,
     char* buffer,
     long* maxSize,
     long toSlices) {

   int rc;
   int writeSize;
   int to;

   struct pollfd fdset[1];

   to = toSlices * This->writeTimeout;
   writeSize = (int)(*maxSize);

   fdset[0].fd = This->connfd;
   fdset[0].events = POLLOUT;

   ////////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted
   // poll call. An interrupted system call may result from
   // a caught signal and will have errno set to EINTR. We
   // must call poll again.

   CFS_WRITE_POLL:

   //
   ////////////////////////////////////////////////////////////

   rc = poll(fdset, 1, to >= 0 ? to: -1);

   if (rc == 1) {

      if (!(fdset[0].revents & POLLOUT)) {

        /////////////////////////////////////////////////////////
        // If we get anything other than POLLOUT
        // this means an error occurred.
        /////////////////////////////////////////////////////////

        *maxSize = 0;

        return   CS_FAILURE
               | CFS_OPER_WAIT
               | CFS_DIAG_SYSTEM;
      }
   }
   else {

      if (rc == 0) {

         *maxSize = 0;

         return   CS_FAILURE
                | CFS_OPER_WAIT
                | CFS_DIAG_TIMEDOUT;
      }
      else {

         if (errno == EINTR) {

            ///////////////////////////////////////////////////
            // poll() was interrupted by a signal
            // or the kernel could not allocate an
            // internal data structure. We will call
            // poll() again.
            ///////////////////////////////////////////////////

            goto CFS_WRITE_POLL;
         }
         else {

            *maxSize = 0;

            return   CS_FAILURE
                   | CFS_OPER_WAIT
                   | CFS_DIAG_SYSTEM;
         }
      }
   }

   /////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted
   // recv() call. An interrupted system call may result
   // from a signal and will have errno set to EINTR.
   // We must call recv() again.

   CFS_WRITE:

   //
   /////////////////////////////////////////////////////////

   *maxSize = send(This->connfd, buffer, writeSize, 0);

   if (*maxSize < 0) {

      if (errno == EINTR) {

         ///////////////////////////////////////////////////
         // send() was interrupted by a signal
         // or the kernel could not allocate an
         // internal data structure. We will call
         // send() again.
         ///////////////////////////////////////////////////

         goto CFS_WRITE;
      }
      else {

         *maxSize = 0;

         return   CS_FAILURE
                | CFS_OPER_WRITE
                | CFS_DIAG_SYSTEM;
      }
   }
   else {

      if (*maxSize == 0) {

         /////////////////////////////////////////////////////////////////
         // This indicates a connection close; we are done.
         /////////////////////////////////////////////////////////////////

         return   CS_FAILURE
                | CFS_OPER_WRITE
                | CFS_DIAG_CONNCLOSE;
      }
   }

   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_WriteRecord
//
// This function writes a specific number of bytes to a non secure socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_WriteRecord
    (CFS_SESSION* This,
     char* buffer,
     long* size,
     long toSlices) {

   int rc;
   int writeSize;
   int to;

   struct pollfd fdset[1];

   uint64_t initialSize;
   uint64_t leftToWrite;
   uint64_t offset;

   to = toSlices * This->writeTimeout;

   initialSize = *size;
   *size = 0;
   offset = 0;

   leftToWrite = initialSize;

   //////////////////////////////////////////////////////////////////////////
   // The total record size exceeds the maximum int value; we will
   // write up to int size at a time until we send the entire buffer.
   //////////////////////////////////////////////////////////////////////////

   writeSize =
     leftToWrite > INT_MAX ?
                   INT_MAX :
                   leftToWrite;

   do {

      ////////////////////////////////////////////////////////////
      // This branching label for restarting an interrupted
      // poll call. An interrupted system call may result from
      // a caught signal and will have errno set to EINTR. We
      // must call poll again.

      CFS_WRITERECORD_POLL:

      //
      ////////////////////////////////////////////////////////////

      fdset[0].fd = This->connfd;
      fdset[0].events = POLLOUT;

      rc = poll(fdset, 1, to >= 0 ? to: -1);

      if (rc == 1) {

         /////////////////////////////////////////////////////////
         // If we get anything other than POLLOUT
         // this means we got an error.
         /////////////////////////////////////////////////////////

         if (!(fdset[0].revents & POLLOUT)) {

            *size = 0;

            return   CS_FAILURE
                   | CFS_OPER_WAIT
                   | CFS_DIAG_SYSTEM;
         }
      }
      else {

         if (rc == 0) {

            *size = 0;

            return   CS_FAILURE
                   | CFS_OPER_WAIT
                   | CFS_DIAG_TIMEDOUT;
         }
         else {

            if (errno == EINTR) {

               ///////////////////////////////////////////////////
               // poll() was interrupted by a signal
               // or the kernel could not allocate an
               // internal data structure. We will call
               // poll() again.
               ///////////////////////////////////////////////////

               goto CFS_WRITERECORD_POLL;
            }
            else {

              *size = 0;

               return   CS_FAILURE
                      | CFS_OPER_WAIT
                      | CFS_DIAG_SYSTEM;
            }
         }
      }

      /////////////////////////////////////////////////////////
      // This branching label for restarting an interrupted
      // send() call. An interrupted system call may result
      // from a signal and will have errno set to EINTR.
      // We must call send() again.

      CFS_WRITERECORD:

      //
      /////////////////////////////////////////////////////////

      rc = send(This->connfd, buffer + offset, writeSize, 0);

      if (rc < 0) {

         if (errno == EINTR) {

            ///////////////////////////////////////////////////
            // send() was interrupted by a signal
            // or the kernel could not allocate an
            // internal data structure. We will call
            // send() again.
            ///////////////////////////////////////////////////

            goto CFS_WRITERECORD;
         }
         else {

           return   CS_FAILURE
                  | CFS_OPER_WAIT
                  | CFS_DIAG_SYSTEM;
         }
      }
      else {

         if (rc == 0) {

            /////////////////////////////////////////////////////////////////
            // This indicates a connection close; we are done.
            /////////////////////////////////////////////////////////////////

            return   CS_FAILURE
                   | CFS_OPER_WRITE
                   | CFS_DIAG_CONNCLOSE;
         }
         else {

            /////////////////////////////////////////////////////////////////
            // We wrote some data (maybe as much as required) ...
            // if not, we will continue writing for as long
            // as we don't block before sending out the supplied buffer.
            /////////////////////////////////////////////////////////////////

            offset += rc;
            *size += rc;
            leftToWrite -= rc;

            writeSize =
               leftToWrite > INT_MAX ?
                             INT_MAX :
                             leftToWrite;
         }
      }
   }
   while (leftToWrite > 0);

   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// Declare and initialize VTABLES
//
// Assign function pointers to vTable members. Functions must already
// be defined before assignment; this is why those global variables
// are declared here.
//
//////////////////////////////////////////////////////////////////////////////

CFSVTBL dftVtbl = {
  CFS_Read,
  CFS_ReadRecord,
  CFS_Write,
  CFS_WriteRecord,
};

CFSVTBL secureVtbl = {
  CFS_SecureRead,
  CFS_SecureReadRecord,
  CFS_SecureWrite,
  CFS_SecureWriteRecord,
};

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Constructor
//
// This function creates a network session object
//
//////////////////////////////////////////////////////////////////////////////

CFS_SESSION*
  CFS_Constructor
    (void) {

  CFS_SESSION* Instance;

  Instance = (CFS_SESSION*)malloc(sizeof(CFS_SESSION));

  Instance->pEnv = 0;

  return Instance;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Destructor
//
// This function releases the resources allocated by a network session object
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_Destructor
    (CFS_SESSION** This) {

  if (This == NULL) {
    return CS_FAILURE;
  }

  if (*This == NULL) {
    return CS_FAILURE;
  }

  free(*This);
  *This = NULL;
  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_CloseChannel
//
// This function closes a server session.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_CloseChannel
    (CFS_SESSION** This) {

  int iSSLResult;
  CSRESULT hResult;

  if (This == NULL) {
    return CS_FAILURE;
  }

  if (*This == NULL) {
    return CS_FAILURE;
  }

  if ((*This)->secMode == 1) {

    ///////////////////////////////////////////////////////
    // Branching label for interrupted system call
    CFS_CLOSECHANNEL:
    ///////////////////////////////////////////////////////

    iSSLResult = gsk_secure_soc_close(&((*This)->ssl_hsession));

    if (iSSLResult != GSK_OK) {
      if (iSSLResult == GSK_ERROR_IO) {
        if (errno == EINTR) {
          goto CFS_CLOSECHANNEL;
        }
      }

      hResult = CS_FAILURE;
    }
    else {
      hResult = CS_SUCCESS;
    }
  }
  else {
    hResult = CS_SUCCESS;
  }

  close((*This)->connfd);
  CFS_Destructor(This);
  return hResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_CloseChannelDescriptor
//
// This function closes a server session.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_CloseChannelDescriptor
    (CFS_SESSION* This) {


   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_CloseEnv
//
// This function releases a global environment
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_CloseEnv
    (CFSENV** pEnv) {

  int iSSLResult;
  CSRESULT hResult;

  if (pEnv == NULL) {
    return CS_FAILURE;
  }

  if (*pEnv == NULL) {
    return CS_FAILURE;
  }

  if ((*pEnv)->secMode == 1) {

    ///////////////////////////////////////////////////////
    // Branching label for interrupted system call
    CFS_CLOSEENV:
    ///////////////////////////////////////////////////////

    iSSLResult = gsk_environment_close(&((*pEnv)->ssl_henv));

    if (iSSLResult != GSK_OK) {
      if (iSSLResult == GSK_ERROR_IO) {
        if (errno == EINTR) {
          goto CFS_CLOSEENV;
        }
      }

      hResult = CS_FAILURE;
    }
    else {
      hResult = CS_SUCCESS;
    }
  }
  else {
    hResult = CS_SUCCESS;
  }

  CSLIST_Destructor(&((*pEnv)->Config_SecureEnv));
  CSLIST_Destructor(&((*pEnv)->Config_SecureSession));

  free(*pEnv);
  *pEnv = NULL;

  return hResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_CloseSession
//
// This function closes a client session.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_CloseSession
    (CFS_SESSION** This) {

  int iSSLResult;
  CSRESULT hResult;

  if (This == NULL) {
    return CS_FAILURE;
  }

  if (*This == NULL) {
    return CS_FAILURE;
  }

  if ((*This)->secMode == 1) {

    ///////////////////////////////////////////////////////
    // Branching label for interrupted system call
    CFS_CLOSESESSION:
    ///////////////////////////////////////////////////////

    iSSLResult = gsk_secure_soc_close(&((*This)->ssl_hsession));

    if (iSSLResult != GSK_OK) {
      if (iSSLResult == GSK_ERROR_IO) {
        if (errno == EINTR) {
          goto CFS_CLOSESESSION;
        }
      }

      hResult = CS_FAILURE;
    }
    else {
      hResult = CS_SUCCESS;
    }
  }
  else {
    hResult = CS_SUCCESS;
  }

  close((*This)->connfd);
  CFS_Destructor(This);

  return hResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_OpenEnv
//
// This function creates a global environment
//
//////////////////////////////////////////////////////////////////////////////

CFSENV*
  CFS_OpenEnv
    (char* szConfigName) {

  CFSENV* pEnv;

  CFSRPS pRepo;
  CFSCFG Config_Session;

  char* pszValue;

  int iSSLResult;
  int iValue;

  long count;
  long i;

  TLSCFG_PARAMINFO* ppi;

  pEnv = (CFSENV*)malloc(sizeof(CFSENV));

  pEnv->Config_SecureEnv = CSLIST_Constructor();
  pEnv->Config_SecureSession = CSLIST_Constructor();

  if (szConfigName == 0) {
    // Indicate that we run in non-secure mode
    pEnv->secMode = 0;
    pEnv->readTimeout = 20000;
    pEnv->writeTimeout = 20000;
    pEnv->connectTimeout = 20000;
    return pEnv;
  }

  pRepo = CFSRPS_Open(0);

  if ((Config_Session = CFSRPS_OpenConfig(pRepo,
                                          szConfigName)) != NULL) {

    if ((pszValue = CFSCFG_LookupParam(Config_Session,
                                       "READ_TO")) == NULL) {
      pEnv->readTimeout = 20000;
    }
    else {
      pEnv->readTimeout = (int)strtol(pszValue, 0, 10);
    }

    if ((pszValue = CFSCFG_LookupParam(Config_Session,
                                       "WRITE_TO")) == NULL) {
      pEnv->writeTimeout = 20000;
    }
    else {
      pEnv->writeTimeout = (int)strtol(pszValue, 0, 10);
    }

    if ((pszValue = CFSCFG_LookupParam(Config_Session,
                                       "SECURE_CONFIG")) != NULL) {

      iSSLResult = gsk_environment_open(&(pEnv->ssl_henv));

      if (iSSLResult != GSK_OK) {
        CFSRPS_CloseConfig(pRepo, &Config_Session);
        CFSRPS_Close(&pRepo);
        goto CFS_OPENENV_ERROR;
      }

      // Retrieve TLS session configuration
      if (CS_FAIL(TLSCFG_LsParam(pszValue,
                       TLSCFG_LVL_SESSION,
                       TLSCFG_PARAMINFO_FMT_100,
                       pEnv->Config_SecureSession))) {

      }

      // Retrieve TLS environment configuration
      if (CS_FAIL(TLSCFG_LsParam(pszValue,
                       TLSCFG_LVL_ENVIRON,
                       TLSCFG_PARAMINFO_FMT_100,
                       pEnv->Config_SecureEnv))) {

      }

      count = CSLIST_Count(pEnv->Config_SecureEnv);

      // Process TLS environment settings only

      for (i=0; i<count; i++) {

        CSLIST_GetDataRef(pEnv->Config_SecureEnv,
                          (void**)&ppi, i);

        switch(ppi->type) {

          case TLSCFG_PARAMTYPE_STRING:

            ///////////////////////////////////////////////////////
            // Branching label for interrupted system call
            GSK_ATTRIBUTE_SET_BUFFER_START:
            ///////////////////////////////////////////////////////

            iSSLResult = gsk_attribute_set_buffer
                             (pEnv->ssl_henv,
                              ppi->param,
                              ppi->szValue,
                              strlen(ppi->szValue));

            if (iSSLResult != GSK_OK) {
              if (iSSLResult == GSK_ERROR_IO) {
                if (errno == EINTR) {
                  goto GSK_ATTRIBUTE_SET_BUFFER_START;
                }
              }
              else {
                CFSRPS_CloseConfig(pRepo, &Config_Session);
                CFSRPS_Close(&pRepo);
                goto CFS_OPENENV_ERROR;
              }
            }

            break;

          case TLSCFG_PARAMTYPE_NUMERIC:

            iValue = (int)strtol(ppi->szValue, 0, 10);

            ///////////////////////////////////////////////////////
            // Branching label for interrupted system call
            GSK_ATTRIBUTE_SET_NUMERIC_VALUE_START:
            ///////////////////////////////////////////////////////

            iSSLResult =
                gsk_attribute_set_numeric_value
                      (pEnv->ssl_henv,
                       ppi->param,
                       iValue);

            if (iSSLResult != GSK_OK) {
              if (iSSLResult == GSK_ERROR_IO) {
                if (errno == EINTR) {
                  goto GSK_ATTRIBUTE_SET_NUMERIC_VALUE_START;
                }
              }
              else {
                CFSRPS_CloseConfig(pRepo, &Config_Session);
                CFSRPS_Close(&pRepo);
                goto CFS_OPENENV_ERROR;
              }
            }

            break;

          case TLSCFG_PARAMTYPE_ENUM:

            iValue = (int)strtol(ppi->szValue, 0, 10);

            ///////////////////////////////////////////////////////
            // Branching label for interrupted system call
            GSK_ATTRIBUTE_SET_ENUM_START:
            ///////////////////////////////////////////////////////

            iSSLResult =
                  gsk_attribute_set_enum(pEnv->ssl_henv,
                               ppi->param,
                               iValue);

            if (iSSLResult != GSK_OK) {
              if (iSSLResult == GSK_ERROR_IO) {
                if (errno == EINTR) {
                  goto GSK_ATTRIBUTE_SET_ENUM_START;
                }
              }
              else {
                CFSRPS_CloseConfig(pRepo, &Config_Session);
                CFSRPS_Close(&pRepo);
                goto CFS_OPENENV_ERROR;
              }
            }

            break;
        }
      }

      ///////////////////////////////////////////////////////
      // Branching label for interrupted system call
      GSK_ENVIRONMENT_INIT:
      ///////////////////////////////////////////////////////

      iSSLResult = gsk_environment_init(pEnv->ssl_henv);

      if (iSSLResult != GSK_OK) {
        if (iSSLResult == GSK_ERROR_IO) {
          if (errno == EINTR) {
            goto GSK_ENVIRONMENT_INIT;
          }
        }
        else {
          gsk_environment_close(&(pEnv->ssl_henv));
          CFSRPS_CloseConfig(pRepo, &Config_Session);
          CFSRPS_Close(&pRepo);
          goto CFS_OPENENV_ERROR;
        }
      }

      // Indicate that we run in secure mode
      pEnv->secMode = 1;
      // return environement
      CFSRPS_CloseConfig(pRepo, &Config_Session);
      CFSRPS_Close(pRepo);
      return pEnv;
    }
    else {

      // Indicate that we run in non-secure mode
      pEnv->secMode = 0;
      // return environement
      CFSRPS_CloseConfig(pRepo, &Config_Session);
      CFSRPS_Close(pRepo);
      return pEnv;
    }
  }

  // If we get here, there was an error
  CFS_OPENENV_ERROR:

  CSLIST_Destructor(&(pEnv->Config_SecureSession));
  CSLIST_Destructor(&(pEnv->Config_SecureEnv));
  free(pEnv);

  return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_OpenChannel
//
// This function initializes a server session.
//
//////////////////////////////////////////////////////////////////////////////

CFS_SESSION*
  CFS_OpenChannel
    (CFSENV* pEnv,
     int connfd) {

  long count;
  long i;
  int iValue;
  int iSSLResult;

  CFS_SESSION* Session;

  TLSCFG_PARAMINFO* ppi;

  Session = CFS_Constructor();

  Session->connfd = connfd;

  if (pEnv != NULL) {
    Session->connectTimeout = pEnv->connectTimeout;
    Session->readTimeout    = pEnv->readTimeout;
    Session->writeTimeout   = pEnv->writeTimeout;
    Session->secMode        = pEnv->secMode;
    Session->pEnv           = pEnv;
  }
  else {
    Session->connectTimeout = 20000;
    Session->readTimeout    = 20000;
    Session->writeTimeout   = 20000;
    Session->secMode        = 0;
    Session->pEnv           = pEnv;
  }

  // Set socket to non-blocking mode
  CFS_PRV_SetBlocking(Session->connfd, 1);

  if (Session->secMode == 1) {

    Session->lpVtbl = &secureVtbl;

    ///////////////////////////////////////////////////////
    // Branching label for interrupted system call
    GSK_SECURE_SOC_OPEN_START:
    ///////////////////////////////////////////////////////

    iSSLResult = gsk_secure_soc_open
                    (Session->pEnv->ssl_henv,
                     &(Session->ssl_hsession));

    if (iSSLResult != GSK_OK) {
      if (iSSLResult == GSK_ERROR_IO) {
        if (errno == EINTR) {
          goto GSK_SECURE_SOC_OPEN_START;
        }
      }
      else {
        goto CFS_OPENSESSION_FREE;
      }
    }

    // Process TLS session settings

    count = CSLIST_Count(Session->pEnv->Config_SecureSession);

    for (i=0; i<count; i++) {

      CSLIST_GetDataRef(Session->pEnv->Config_SecureSession,
                        (void**)&ppi, i);

      switch(ppi->type) {

        case TLSCFG_PARAMTYPE_STRING:

          ///////////////////////////////////////////////////////
          // Branching label for interrupted system call
          GSK_ATTRIBUTE_SET_BUFFER_START:
          ///////////////////////////////////////////////////////

          iSSLResult = gsk_attribute_set_buffer
                          (Session->ssl_hsession,
                           ppi->param,
                           ppi->szValue,
                           strlen(ppi->szValue));

          if (iSSLResult != GSK_OK) {
            if (iSSLResult == GSK_ERROR_IO) {
              if (errno == EINTR) {
                goto GSK_ATTRIBUTE_SET_BUFFER_START;
              }
            }
            else {
              goto CFS_OPENSESSION_FREE;
            }
          }

          break;

        case TLSCFG_PARAMTYPE_NUMERIC:

          iValue = (int)strtol(ppi->szValue, 0, 10);

          ///////////////////////////////////////////////////////
          // Branching label for interrupted system call
          GSK_ATTRIBUTE_SET_NUMERIC_VALUE_START:
          ///////////////////////////////////////////////////////

          iSSLResult =
             gsk_attribute_set_numeric_value
                           (Session->ssl_hsession,
                            ppi->param,
                            iValue);

          if (iSSLResult != GSK_OK) {
            if (iSSLResult == GSK_ERROR_IO) {
              if (errno == EINTR) {
                goto GSK_ATTRIBUTE_SET_NUMERIC_VALUE_START;
              }
            }
            else {
              goto CFS_OPENSESSION_FREE;
            }
          }

          break;

        case TLSCFG_PARAMTYPE_ENUM:

          iValue = (int)strtol(ppi->szValue, 0, 10);

          ///////////////////////////////////////////////////////
          // Branching label for interrupted system call
          GSK_ATTRIBUTE_SET_ENUM_START:
          ///////////////////////////////////////////////////////

          iSSLResult =
             gsk_attribute_set_enum(Session->ssl_hsession,
                            ppi->param,
                            iValue);

          if (iSSLResult != GSK_OK) {
            if (iSSLResult == GSK_ERROR_IO) {
              if (errno == EINTR) {
                goto GSK_ATTRIBUTE_SET_ENUM_START;
              }
            }
            else {
              goto CFS_OPENSESSION_FREE;
            }
          }

          break;
      }
    }

    ///////////////////////////////////////////////////////
    // Branching label for interrupted system call
    GSK_ATTRIBUTE_SET_NUMERIC_VALUE_FD_START:
    ///////////////////////////////////////////////////////

    iSSLResult = gsk_attribute_set_numeric_value
                                (Session->ssl_hsession,
                                 GSK_FD,
                                 Session->connfd);

    if (iSSLResult != GSK_OK) {
      if (iSSLResult == GSK_ERROR_IO) {
        if (errno == EINTR) {
          goto GSK_ATTRIBUTE_SET_NUMERIC_VALUE_FD_START;
        }
      }
      else {
        gsk_secure_soc_close(&(Session->ssl_hsession));
        goto CFS_OPENSESSION_FREE;
      }
    }

    ///////////////////////////////////////////////////////
    // Branching label for interrupted system call
    GSK_SECURE_SOC_INIT_START:
    ///////////////////////////////////////////////////////

    iSSLResult = gsk_secure_soc_init(Session->ssl_hsession);

    if (iSSLResult != GSK_OK) {
      if (iSSLResult == GSK_ERROR_IO) {
        if (errno == EINTR) {
          goto GSK_SECURE_SOC_INIT_START;
        }
      }
      else {
        gsk_secure_soc_close(&(Session->ssl_hsession));
        goto CFS_OPENSESSION_FREE;
      }
    }

    return Session;
  }
  else {
    Session->lpVtbl = &dftVtbl;
    return Session;
  }

  // IF we get here, there was an error
  CFS_OPENSESSION_FREE:

  close(Session->connfd);
  Session->connfd = -1;
  CFS_Destructor(&Session);

  return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_OpenSession
//
// This function initialises a connection to a server.
//
//////////////////////////////////////////////////////////////////////////////

CFS_SESSION*
  CFS_OpenSession
    (CFSENV* pEnv,
     char* szConfig,
     char* szHost,
     char* szPort) {

  int rc;

  CSRESULT hResult;
  CFSRPS pRepo;
  CFSCFG pConfig;

  TLSCFG_PARAMINFO* ppi;

  char* pszHost;
  char* pszPort;
  char* pszParam;

  struct addrinfo* addrInfo;
  struct addrinfo* addrInfo_first;
  struct addrinfo  hints;

  struct pollfd fdset[1];

  int iValue;
  int iSSLResult;
  long count;
  long i;

  char* pszValue;

  CFS_SESSION* Session;

  Session = CFS_Constructor();

  if (pEnv == NULL) {

    Session->secMode        = 0;
    Session->connectTimeout = 20000;
    Session->readTimeout    = 20000;
    Session->writeTimeout   = 20000;
  }
  else {

    Session->secMode        = pEnv->secMode;
    Session->connectTimeout = pEnv->connectTimeout;
    Session->readTimeout    = pEnv->readTimeout;
    Session->writeTimeout   = pEnv->writeTimeout;
  }

  if (szConfig) {

    pRepo = CFSRPS_Open(0);

    if ((pConfig = CFSRPS_OpenConfig(pRepo, szConfig)) == NULL) {
      CFSRPS_Close(&pRepo);
      return NULL;
    }

    if ((pszHost = CFSCFG_LookupParam(pConfig, "HOST")) == NULL) {
      CFSRPS_CloseConfig(pRepo, &(pConfig));
      CFSRPS_Close(&pRepo);
      return NULL;
    }

    strncpy(Session->szHostName, pszHost, 256);
    Session->szHostName[255] = 0;

    if ((pszPort = CFSCFG_LookupParam(pConfig, "PORT")) == NULL) {
      CFSRPS_CloseConfig(pRepo, &(pConfig));
      CFSRPS_Close(&pConfig);
      return NULL;
    }

    strncpy(Session->szPort, pszPort, 11);
    Session->szPort[10] = 0;

    // Configuration overrides on the environment

    if ((pszValue = CFSCFG_LookupParam(pConfig, "CONN_TO")) != NULL) {
      Session->connectTimeout = (int)strtol(pszValue, 0, 10);
    }

    if ((pszValue = CFSCFG_LookupParam(pConfig, "READ_TO")) != NULL) {
      Session->readTimeout = (int)strtol(pszValue, 0, 10);
    }

    if ((pszValue = CFSCFG_LookupParam(pConfig, "WRITE_TO")) != NULL) {
      Session->writeTimeout = (int)strtol(pszValue, 0, 10);
    }

    CFSRPS_CloseConfig(pRepo, &(pConfig));
    CFSRPS_Close(&pRepo);
  }
  else {

    if (szHost == NULL) {
      return NULL;
    }
    else {
      strncpy(Session->szHostName, szHost, 256);
      Session->szHostName[255] = 0;
    }

    if (szPort == NULL) {
      return NULL;
    }
    else {
      strncpy(Session->szPort, szPort, 11);
      Session->szPort[10] = 0;
    }
  }

  Session->info.szHost = Session->szHostName;
  Session->info.szPort = Session->szPort;
  Session->info.szLocalPort = 0;
  Session->info.szRemotePort = 0;
  Session->pEnv = pEnv;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  hResult = CS_FAILURE;

  if (getaddrinfo(Session->szHostName,
                   Session->szPort,
                   &hints,
                   &addrInfo) == 0) {

    addrInfo_first = addrInfo;

    while (addrInfo != 0)
    {
      Session->connfd = socket(addrInfo->ai_family,
                               addrInfo->ai_socktype,
                               addrInfo->ai_protocol);

      if (Session->connfd >= 0) {

        // Set socket to non-blocking
        CFS_PRV_SetBlocking(Session->connfd, 0);

        rc = connect(Session->connfd,
                     addrInfo->ai_addr,
                     addrInfo->ai_addrlen);

        if (rc < 0) {

          if (errno != EINPROGRESS) {

            Session->errInfo.m_errno = errno;
            Session->errInfo.secrc = 0;

            Session->errInfo.csresult =   CS_FAILURE
                                        | CFS_OPER_READ
                                        | CFS_DIAG_CONNCLOSE;

            close(Session->connfd);
          }
          else {

            ////////////////////////////////////////////////////////
            // The connection is in progress, so we must wait
            // for the TCP handshgake to complete
            ////////////////////////////////////////////////////////

            fdset[0].fd = Session->connfd;
            fdset[0].events = POLLOUT;

            CFS_OPENSESSION_CONNECT_POLL_READ:

            rc = poll(fdset, 1,
                      Session->connectTimeout >= 0 ?
                      Session->connectTimeout: -1);

            if (rc == 1) {

              /////////////////////////////////////////////////////////
              // If we get anything other than POLLOUT
              // this means we got an error.
              /////////////////////////////////////////////////////////

              if (fdset[0].revents & POLLOUT) {
                // TCP handshake has completed so we are connected
                hResult = CS_SUCCESS;
                break;  // leave the connection loop
              }
              else {

                Session->errInfo.m_errno = errno;
                Session->errInfo.secrc = 0;

                Session->errInfo.csresult =   CS_FAILURE
                                            | CFS_OPER_READ
                                            | CFS_DIAG_CONNCLOSE;

                close(Session->connfd);
              }
            }
            else {

              if (rc == 0) {

                Session->errInfo.m_errno = errno;
                Session->errInfo.secrc = 0;

                Session->errInfo.csresult =   CS_FAILURE
                                            | CFS_OPER_READ
                                            | CFS_DIAG_CONNCLOSE;

                close(Session->connfd);
              }
              else {

                if (errno == EINTR) {
                  goto CFS_OPENSESSION_CONNECT_POLL_READ;
                }
                else {

                  Session->errInfo.m_errno = errno;
                  Session->errInfo.secrc = 0;

                  Session->errInfo.csresult =   CS_FAILURE
                                              | CFS_OPER_READ
                                              | CFS_DIAG_CONNCLOSE;

                  close(Session->connfd);
                }
              }
            }
          }
        }
        else {
          hResult = CS_SUCCESS;
          break;
        }
      }

      addrInfo = addrInfo->ai_next;
    }

    freeaddrinfo(addrInfo_first);
  }

  if (CS_SUCCEED(hResult)) {

    CFS_PRV_SetBlocking(Session->connfd, 1);

    if (Session->secMode == 1) {

      // Load secure network functions
      Session->lpVtbl = &secureVtbl;

      // Initialize TLS session

      ///////////////////////////////////////////////////////
      // Branching label for interrupted system call
      GSK_SECURE_SOC_OPEN_START:
      ///////////////////////////////////////////////////////

      iSSLResult = gsk_secure_soc_open(Session->pEnv->ssl_henv,
                                        &(Session->ssl_hsession));

      if (iSSLResult == GSK_OK) {

        count = CSLIST_Count(Session->pEnv->Config_SecureSession);

        for (i=0; i<count; i++) {

          CSLIST_GetDataRef(Session->pEnv->Config_SecureSession,
                            (void**)&ppi, i);

          switch(ppi->type) {

            case TLSCFG_PARAMTYPE_STRING:

              ///////////////////////////////////////////////////////
              // Branching label for interrupted system call
              GSK_ATTRIBUTE_SET_BUFFER_START:
              ///////////////////////////////////////////////////////

              iSSLResult = gsk_attribute_set_buffer
                                     (Session->ssl_hsession,
                                      ppi->param,
                                      ppi->szValue,
                                      strlen(ppi->szValue));

              if (iSSLResult != GSK_OK) {
                if (iSSLResult == GSK_ERROR_IO) {
                  if (errno == EINTR) {
                    goto GSK_ATTRIBUTE_SET_BUFFER_START;
                  }
                }
                else {
                  goto CFS_OPENSESSION_CLOSE;
                }
              }

              break;

            case TLSCFG_PARAMTYPE_NUMERIC:

              iValue = (int)strtol(ppi->szValue, 0, 10);

              ///////////////////////////////////////////////////////
              // Branching label for interrupted system call
              GSK_ATTRIBUTE_SET_NUMERIC_VALUE_START:
              ///////////////////////////////////////////////////////

              iSSLResult =
                  gsk_attribute_set_numeric_value
                                     (Session->ssl_hsession,
                                      ppi->param,
                                      iValue);

              if (iSSLResult != GSK_OK) {
                if (iSSLResult == GSK_ERROR_IO) {
                  if (errno == EINTR) {
                    goto GSK_ATTRIBUTE_SET_NUMERIC_VALUE_START;
                  }
                }
                else {
                  goto CFS_OPENSESSION_CLOSE;
                }
              }

              break;

            case TLSCFG_PARAMTYPE_ENUM:

              iValue = (int)strtol(ppi->szValue, 0, 10);

              ///////////////////////////////////////////////////////
              // Branching label for interrupted system call
              GSK_ATTRIBUTE_SET_ENUM_START:
              ///////////////////////////////////////////////////////

              iSSLResult =
                  gsk_attribute_set_enum(Session->ssl_hsession,
                                         ppi->param,
                                         iValue);

              if (iSSLResult != GSK_OK) {
                if (iSSLResult == GSK_ERROR_IO) {
                  if (errno == EINTR) {
                    goto GSK_ATTRIBUTE_SET_ENUM_START;
                  }
                }
                else {
                  goto CFS_OPENSESSION_CLOSE;
                }
              }

              break;
          }
        }

        ///////////////////////////////////////////////////////
        // Branching label for interrupted system call
        GSK_ATTRIBUTE_SET_NUMERIC_VALUE_FD_START:
        ///////////////////////////////////////////////////////

        iSSLResult = gsk_attribute_set_numeric_value
                                    (Session->ssl_hsession,
                                    GSK_FD,
                                    Session->connfd);

        if (iSSLResult == GSK_OK) {

          ///////////////////////////////////////////////////////
          // Branching label for interrupted system call
          GSK_SECURE_SOC_INIT_START:
          ///////////////////////////////////////////////////////

          iSSLResult = gsk_secure_soc_init(Session->ssl_hsession);

          if (iSSLResult != GSK_OK) {
            if (iSSLResult == GSK_ERROR_IO) {
              if (errno == EINTR) {
                goto GSK_SECURE_SOC_INIT_START;
              }
            }
            else {
              gsk_secure_soc_close(&(Session->ssl_hsession));
              goto CFS_OPENSESSION_CLOSE;
            }
          }
        }
        else {
          if (iSSLResult == GSK_ERROR_IO) {
            if (errno == EINTR) {
              goto GSK_ATTRIBUTE_SET_NUMERIC_VALUE_FD_START;
            }
          }
          else {
            gsk_secure_soc_close(&(Session->ssl_hsession));
            goto CFS_OPENSESSION_CLOSE;
          }
        }
      }
      else {

        if (iSSLResult == GSK_ERROR_IO) {
          if (errno == EINTR) {
            goto GSK_SECURE_SOC_OPEN_START;
          }
        }
        else {
          goto CFS_OPENSESSION_CLOSE;
        }
      }

      if (iSSLResult != GSK_OK) {
        goto CFS_OPENSESSION_CLOSE;
      }
    }
    else {

      // Use non-TLS network functions
      Session->lpVtbl = &dftVtbl;
    }

    return Session;
  }

  CFS_OPENSESSION_CLOSE:
  close(Session->connfd);

  CFS_OPENSESSION_FREE:
  Session->connfd = -1;
  CFS_Destructor(&Session);

  return NULL;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_QuerySessionInfo
//
// This function retrieves session information.
//
//////////////////////////////////////////////////////////////////////////////

SESSIONINFO*
  CFS_QuerySessionInfo
    (CFS_SESSION* This) {

  return &(This->info);
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_ReceiveDescriptor
//
// This function receives a file (socket) descriptor from another process.
// It is assumed that the caller has already established a connection to
// the other process via a local domain (UNIX) socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_ReceiveDescriptor
    (int fd,
     int* descriptor,
     int timeout) {

  // The peer needs to send some data
  // even though we will ignore it; this is required
  // by the sendmsg function.

  char dummyByte = 0;

  struct iovec iov[1];

  struct pollfd fdset[1];

  struct msghdr msgInstance;

  int rc;

  //////////////////////////////////////////////////////////////////////////
  // ancillary (control) data.
  // This is where the descriptor will be held.
  //////////////////////////////////////////////////////////////////////////

#ifdef MSGHDR_MSG_CONTROL

  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_un;

  struct cmsghdr* cmptr;

  msgInstance.msg_control = control_un.control;
  msgInstance.msg_controllen = sizeof(control_un.control);

#else

  int receivedFD;
  msgInstance.msg_accrights = (caddr_t)&receivedFD;
  msgInstance.msg_accrightslen = sizeof(int);

#endif

  msgInstance.msg_name = NULL;
  msgInstance.msg_namelen = 0;

  iov[0].iov_base = &dummyByte;
  iov[0].iov_len = 1;

  msgInstance.msg_iov = iov;
  msgInstance.msg_iovlen = 1;

  //////////////////////////////////////////////////////////////////////////
  // This branching label for restarting an interrupted poll call.
  // An interrupted system call may results from a caught signal
  // and will have errno set to EINTR. We must call poll again.

  CFS_WAIT_DESCRIPTOR:

  //
  //////////////////////////////////////////////////////////////////////////

  *descriptor = -1;

  fdset[0].fd = fd;
  fdset[0].events = POLLIN;

  rc = poll(fdset, 1, timeout >= 0 ? timeout * 1000: -1);

  switch(rc) {

    case 0:  // timed-out

      rc = CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
      break;

    case 1:  // descriptor is ready

      if (fdset[0].revents == POLLIN) {

        // get the descriptor

        rc = recvmsg(fd, &msgInstance, 0);

        if (rc >= 0) {

          // Assume the rest will fail
          rc = CS_FAILURE | CFS_OPER_READ | CFS_DIAG_SYSTEM;

#ifdef MSGHDR_MSG_CONTROL

          if ( (cmptr = CMSG_FIRSTHDR(&msgInstance)) != NULL) {

            if (cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {

              if (cmptr->cmsg_level == SOL_SOCKET &&
                  cmptr->cmsg_type  == SCM_RIGHTS) {

                *descriptor = *((int*)CMSG_DATA(cmptr));
                rc = CS_SUCCESS;

              }
            }
          }

#else

          if (msgInstance.msg_accrightslen == sizeof(receivedFD)) {

            *descriptor = receivedFD;
            rc = CS_SUCCESS;
          }

#endif

        }
        else {
          rc = CS_FAILURE | CFS_OPER_READ | CFS_DIAG_SYSTEM;
        }
      }

      break;

    default:

      if (errno == EINTR) {

        goto CFS_WAIT_DESCRIPTOR;
      }
      else {
        rc = CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
      }

      break;
   }

   return rc;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SendDescriptor
//
// This function sends a file (socket) descriptor to another process.
// The caller has already established a connection to the other process
// via a local domain (UNIX) socket.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SendDescriptor
    (int fd,
     int descriptor,
     int timeout) {

   struct iovec iov[1];

   struct pollfd fdset[1];

   struct msghdr msgInstance;

   int rc;

   // We need to send some data, even though it will be ignored
   // by the peer

   char dummyByte = 0;

   //////////////////////////////////////////////////////////////////////////
   // ancillary (control) data.
   // This is where the descriptor will be held.
   //////////////////////////////////////////////////////////////////////////

#ifdef MSGHDR_MSG_CONTROL

   //////////////////////////////////////////////////////////////////////////
   // We are using a cmsghdr to pass along the ancillary data ...
   // This union is to properly align the cmsghdr structure with the data
   // buffer that will hold the descriptor.
   //////////////////////////////////////////////////////////////////////////

   union {
      struct cmsghdr cm;
      char control[CMSG_SPACE(sizeof(int))];
   } control_un;

   struct cmsghdr* cmptr;

   //////////////////////////////////////////////////////////////////////////
   //  Initialise the msghdr with the ancillary data and
   //  ancillary data length.
   //////////////////////////////////////////////////////////////////////////

   msgInstance.msg_control    = control_un.control;
   msgInstance.msg_controllen = sizeof(control_un.control);

   //////////////////////////////////////////////////////////////////////////
   //  Initialise the ancillary data itself with the
   //  descriptor to pass along sendmsg().
   //////////////////////////////////////////////////////////////////////////

   // point to first (and only) ancillary data entry.
   cmptr = CMSG_FIRSTHDR(&msgInstance);

   // initialise ancillary data header.
   cmptr->cmsg_len   = CMSG_LEN(sizeof(int));  // size of descriptor
   cmptr->cmsg_level = SOL_SOCKET;
   cmptr->cmsg_type  = SCM_RIGHTS;

   //////////////////////////////////////////////////////////////////////////
   // Assign the descriptor to ancillary data
   //
   // CMSG_DATA will return the address of the first data byte,
   // which is located somewhere in the control_un.control array.
   // To assign the descriptor, which is an integer, we must cast
   // this address to int* and dereference the address to set
   // it to the descriptor value.
   //////////////////////////////////////////////////////////////////////////

   *((int*)CMSG_DATA(cmptr)) = descriptor;

#else

   msgInstance.msg_accrights = (caddr_t)&descriptor;
   msgInstance.msg_accrightslen = sizeof(descriptor);

#endif

   msgInstance.msg_name    = NULL;
   msgInstance.msg_namelen = 0;

   iov[0].iov_base = &dummyByte;
   iov[0].iov_len = 1;

   msgInstance.msg_iov = iov;
   msgInstance.msg_iovlen = 1;

   //////////////////////////////////////////////////////////////////////////
   // Send the descriptor via the stream pipe descriptor.
   //////////////////////////////////////////////////////////////////////////

   //////////////////////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted poll call.
   // An interrupted system call may results from a caught signal
   // and will have errno set to EINTR. We must call poll again.

   CFS_WAIT_DESCRIPTOR:

   //
   //////////////////////////////////////////////////////////////////////////

   fdset[0].fd = fd;
   fdset[0].events = POLLOUT;

   rc = poll(fdset, 1, timeout >= 0 ? timeout * 1000: -1);

   switch(rc) {

      case 0:  // timed-out

         rc = CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
         break;

      case 1:  // descriptor is ready

         if(fdset[0].revents == POLLOUT) {

            rc = sendmsg(fd, &msgInstance, 0);

            if (rc < 0) {
               rc = CS_FAILURE | CFS_OPER_WRITE  | CFS_DIAG_SYSTEM;
            }
            else {
               rc = CS_SUCCESS;
            }
         }
         else {

            rc = CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
         }

         break;

      default:

         if (errno == EINTR) {

            goto CFS_WAIT_DESCRIPTOR;
         }
         else {
            rc = CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
         }

         break;
   }

   return rc;
}

CSRESULT
  CFS_SetChannelDescriptor
    (CFS_SESSION* This,
     int connfd) {

  This->connfd = connfd;

  if (This->secMode == 1) {

  }

  // Set socket to blocking mode
  CFS_PRV_SetBlocking(This->connfd, 1);

  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_PRV_NetworkToPresentation
//
// This function returns a string representation of a network address.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_PRV_NetworkToPresentation
    (const struct sockaddr* sa,
     char* addrstr,
     char* portstr)
{
  CSRESULT rc;

  struct sockaddr_in*  sin;
  struct sockaddr_un*  unp;
  struct sockaddr_in6* sin6;

  strcpy(addrstr, "");
  strcpy(portstr, "");
  rc = CS_SUCCESS;

  switch(sa->sa_family)
  {
    case AF_INET:
    {
      sin = (struct sockaddr_in*)sa;

      if (inet_ntop(AF_INET, &sin->sin_addr, addrstr,
                    (socklen_t)CFS_NTOP_ADDR_MAX) != 0)
      {
         snprintf(portstr, CFS_NTOP_PORT_MAX, "%d", ntohs(sin->sin_port));
          rc = CS_SUCCESS;
      }

      break;
    }

    case AF_INET6:
    {
      sin6 = (struct sockaddr_in6 *)sa;
      addrstr[0] = '[';
      if (inet_ntop(AF_INET6, &sin6->sin6_addr,
              addrstr + 1,
              (socklen_t)(CFS_NTOP_ADDR_MAX - 1)) != (const char*)NULL)
      {
        snprintf(portstr, CFS_NTOP_PORT_MAX, "%d", ntohs(sin6->sin6_port));
        strcat(addrstr, "]");
        rc = CS_SUCCESS;
      }

      break;
    }

    case AF_UNIX:
    {
      unp = (struct sockaddr_un *)sa;

      /* OK to have no pathname bound to the socket: happens on
           every connect() unless client calls bind() first. */

      if (unp->sun_path[0] == 0)
      {
        strcpy(addrstr, "(no pathname bound)");
      }
      else
      {
        snprintf(addrstr, CFS_NTOP_ADDR_MAX, "%s", unp->sun_path);
        rc = CS_SUCCESS;
      }

      break;
    }

    default:
    {
       snprintf(addrstr, CFS_NTOP_ADDR_MAX,
                "sock_ntop: unknown AF_xxx: %d",
                sa->sa_family);

       rc = CS_FAILURE;
       break;
    }
  }

  return rc;
}
