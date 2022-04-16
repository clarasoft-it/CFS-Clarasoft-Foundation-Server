/* ===========================================================================
  Clarasoft Foundation Server 400
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

#define MSGHDR_MSG_CONTROL

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <unistd.h>
#include <clarasoft/cslib.h>
#include <clarasoft/cfsrepo.h>

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

  CSRESULT (*CFS_Read)         (CFS_SESSION*, char*, uint64_t*, int, int*);
  CSRESULT (*CFS_ReadRecord)   (CFS_SESSION*, char*, uint64_t*, int, int*);
  CSRESULT (*CFS_Write)        (CFS_SESSION*, char*, uint64_t*, int, int*);
  CSRESULT (*CFS_WriteRecord)  (CFS_SESSION*, char*, uint64_t*, int, int*);

} CFSVTBL;

typedef CFSVTBL* LPCFSVTBL;

typedef struct tagCFSENV {

  LPCFSVTBL lpVtbl;

  int secMode;

  char  szConfigName[99];

  SSL_CTX *ctx;

  CFSRPS TlsCfg_Env;
  CFSRPS TlsCfg_Session;

} CFSENV;

typedef struct tagCFS_SESSION {

  LPCFSVTBL lpVtbl;

  int connInfoFmt;

  int connfd;
  int connectTimeout;
  int readTimeout;
  int writeTimeout;
  int gskHandleResetCipher;
  int secMode;

  char  szPort[11];
  char  szConfigName[99];
  char  szHostName[256];

  int32_t size;

  CFSRPS  Cfg;

  CFSENV*  pEnv;
  CFSENV*  pLocalEnv;

  const SSL_METHOD *method;
  SSL_CTX *ctx;
  SSL* ssl;

  CSLIST  TlsCfg_Session;
  CSLIST  TlsCfg_LocalSession;

  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO cfscpi;

} CFS_SESSION;

//////////////////////////////////////////////////////////////////////////////
//  Prototypes
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_PRV_SecureLoadTrustStore
    (CFSENV* pEnv);

int
  CFS_SSL_VerifyCallback
    (int preverify_ok, 
    X509_STORE_CTX *ctx);

CSRESULT
  CFS_CloseChannel
    (CFS_SESSION* This,
     int* e);

CSRESULT
  CFS_CloseEnv
    (CFSENV** pEnv);

CSRESULT
  CFS_CloseSession
    (CFS_SESSION* This,
     int* e);

CFSENV*
  CFS_OpenEnv
    (char* szConfig);

CFS_SESSION*
  CFS_OpenSession
    (CFSENV* pEnv,
     char* szSessionConfig,
     char* szHost,
     char* szPort,
     int* e);

CFS_SESSION*
  CFS_OpenChannel
    (CFSENV* pEnv,
     char* szSessionConfig,
     int connfd,
     int* e);

CSRESULT
  CFS_QueryConfig
    (CFS_SESSION* This,
     char* szParam,
     CFSRPS_PARAMINFO* cfscpi);

CSRESULT
  CFS_Read
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* e);

CSRESULT
  CFS_ReadRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* e);

CSRESULT
  CFS_ReceiveDescriptor
    (int fd,
     int* descriptor,
     int timeout);

CSRESULT
  CFS_SecureRead
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* iSSLResult);

CSRESULT
  CFS_SecureReadRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* iSSLResult);

CSRESULT
  CFS_SecureWrite
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* iSSLResult);

CSRESULT
  CFS_SecureWriteRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* iSSLResult);

CSRESULT
  CFS_SendDescriptor
    (int fd,
     int descriptor,
     int timeout);

CSRESULT
  CFS_Write
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* e);

CSRESULT
  CFS_WriteRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* e);

CFS_SESSION*
  CFS_Constructor
    (void);

void
  CFS_Destructor
    (CFS_SESSION** This);

CSRESULT
  CFS_PRV_NetworkToPresentation
    (const struct sockaddr* sa,
     char* addrstr,
     char* portstr);

CSRESULT
  CFS_PRV_SetBlocking
    (int connfd,
    int blocking);

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Read
//
// This function reads a non-secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it reads on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_Read
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* e) {

   int rc;
   int readSize;

   struct pollfd fdset[1];

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

   CFS_WAIT_POLL:

   //
   ////////////////////////////////////////////////////////////

   if (timeout != -1) {
     rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
   }
   else {
     rc = poll(fdset, 1,
               This->readTimeout >= 0 ? This->readTimeout * 1000: -1);
   }

   if (rc == 1) {

     if (!(fdset[0].revents & POLLIN)) {

       /////////////////////////////////////////////////////////
       // If we get anything other than POLLIN
       // this means an error occurred.
       /////////////////////////////////////////////////////////

       *e = errno;
       *maxSize = 0;
       return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
     }
   }
   else {

     if (rc == 0) {
       *maxSize = 0;
       return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
     }
     else {

       if (errno == EINTR) {

         ///////////////////////////////////////////////////
         // poll() was interrupted by a signal
         // or the kernel could not allocate an
         // internal data structure. We will call
         // poll() again.
         ///////////////////////////////////////////////////

         goto CFS_WAIT_POLL;
       }
       else {

         *e = errno;
         *maxSize = 0;
         return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
       }
     }
   }

   /////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted
   // recv() call. An interrupted system call may result
   // from a signal and will have errno set to EINTR.
   // We must call recv() again.

   CFS_WAIT_READ:

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

       goto CFS_WAIT_READ;
     }
     else {

       *e = errno;
       *maxSize = 0;
       return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_SYSTEM;
     }
   }
   else {

     if (rc == 0) {

       /////////////////////////////////////////////////////////////////
       // This indicates a connection close; we are done.
       /////////////////////////////////////////////////////////////////

       *e = errno;
       *maxSize = 0;
       return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_CONNCLOSE;
     }
   }

   *maxSize = (uint64_t)rc;
   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_ReadRecord
//
// This function reads a specific number of bytes from a non secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it reads on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_ReadRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* e) {

   int rc;
   int readSize;

   struct pollfd fdset[1];

   uint64_t leftToRead;
   uint64_t offset;

   leftToRead = *size;

   *size       = 0;
   offset      = 0;

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

     CFS_WAIT_POLL:

     //
     ////////////////////////////////////////////////////////////

     fdset[0].fd = This->connfd;
     fdset[0].events = POLLIN;

     if (timeout != -1) {
       rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
     }
     else {
       rc = poll(fdset, 1,
               This->readTimeout >= 0 ? This->readTimeout * 1000: -1);
     }

     if (rc == 1) {

       /////////////////////////////////////////////////////////
       // If we get anything other than POLLIN
       // this means we got an error.
       /////////////////////////////////////////////////////////

       if (!(fdset[0].revents & POLLIN)) {

        *e = errno;
         return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
       }
     }
     else {

       if (rc == 0) {
         *e = errno;
         return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
       }
       else {

         if (errno == EINTR) {

           ///////////////////////////////////////////////////
           // poll() was interrupted by a signal
           // or the kernel could not allocate an
           // internal data structure. We will call
           // poll() again.
           ///////////////////////////////////////////////////

           goto CFS_WAIT_POLL;
         }
         else {

           *e = errno;
           return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
         }
       }
     }

     /////////////////////////////////////////////////////////
     // This branching label for restarting an interrupted
     // recv() call. An interrupted system call may result
     // from a signal and will have errno set to EINTR.
     // We must call recv() again.

     CFS_WAIT_READ:

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

         goto CFS_WAIT_READ;
       }
       else {
         *e = errno;
         return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_SYSTEM;
       }
     }
     else {

       if (rc == 0) {

         /////////////////////////////////////////////////////////////////
         // This indicates a connection close; we are done.
         /////////////////////////////////////////////////////////////////

         *e = errno;
         return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_CONNCLOSE;
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
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it reads on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureRead
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* iSSLResult) {

  int rc;
  int readSize;
  int bytes;

  struct pollfd fdset[1];

  readSize = (int)(*maxSize > INT_MAX ?
                              INT_MAX :
                              *maxSize);

  *maxSize = 0;

  CFS_SECUREREAD_AGAIN:  

  bytes = SSL_read(This->ssl, buffer, readSize);

  switch(SSL_get_error(This->ssl, bytes)) {
                  
    case SSL_ERROR_NONE:
      
      // we have read something
      break;

    case SSL_ERROR_ZERO_RETURN:

      /////////////////////////////////////////////////////////////////
      // This indicates a connection close; we are done.
      /////////////////////////////////////////////////////////////////

      *iSSLResult = errno;
      return CS_SUCCESS | CFS_OPER_READ | CFS_DIAG_CONNCLOSE;

    case SSL_ERROR_WANT_READ:

      /////////////////////////////////////////////////////////////////
      // This indicates that we need for the socket descriptor
      // to become readable, possibly for session renegociation, after
      // which, we must re-issue SSL_read exactly as before.
      /////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////
      // This branching label for restarting an interrupted
      // poll call. An interrupted system call may result from
      // a caught signal and will have errno set to EINTR. We
      // must call poll again.

      CFS_SECUREREAD_POLL_READ:
  
      //
      ////////////////////////////////////////////////////////////

      fdset[0].fd = This->connfd;
      fdset[0].events = POLLIN;

      if (timeout != -1) {
        rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
      }
      else {
        rc = poll(fdset, 1,
               This->readTimeout >= 0 ? This->readTimeout * 1000: -1);
      }

      if (rc == 1) {

        /////////////////////////////////////////////////////////
        // If we get anything other than POLLIN
        // this means we got an error.
        /////////////////////////////////////////////////////////
 
        if (!(fdset[0].revents & POLLIN)) {

          *iSSLResult = errno;
           return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }

        goto CFS_SECUREREAD_AGAIN;
      }
      else {

        if (rc == 0) {
          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }
        else {

          if (errno == EINTR) {

            ///////////////////////////////////////////////////
            // poll() was interrupted by a signal
            // or the kernel could not allocate an
            // internal data structure. We will call
            // poll() again.
            ///////////////////////////////////////////////////

            goto CFS_SECUREREAD_POLL_READ;
          }
          else {

            *iSSLResult = errno;
             return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
          }
        }
      }

      break;

    case SSL_ERROR_WANT_WRITE:

      /////////////////////////////////////////////////////////////////
      // This indicates that we need for the socket descriptor
      // to become writeable, possibly for session renegociation, after
      // which, we must re-issue SSL_read exactly as before.
      // We don't call SSL_write despite the name of this exception.
      // So we wait until descriptor is writeable and we jump back 
      // to SSL_read
      /////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////
      // This branching label for restarting an interrupted
      // poll call. An interrupted system call may result from
      // a caught signal and will have errno set to EINTR. We
      // must call poll again.

      CFS_SECUREREAD_POLL_WRITE:
  
      //
      ////////////////////////////////////////////////////////////

      fdset[0].fd = This->connfd;
      fdset[0].events = POLLOUT;

      if (timeout != -1) {
        rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
      }
      else {
        rc = poll(fdset, 1,
               This->writeTimeout >= 0 ? This->writeTimeout * 1000: -1);
      }

      if (rc == 1) {

        /////////////////////////////////////////////////////////
        // If we get anything other than POLLOUT
        // this means we got an error.
        /////////////////////////////////////////////////////////
 
        if (!(fdset[0].revents & POLLOUT)) {

          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }

        goto CFS_SECUREREAD_AGAIN;
      }
      else {

        if (rc == 0) {
          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }
        else {

          if (errno == EINTR) {

            ///////////////////////////////////////////////////
            // poll() was interrupted by a signal
            // or the kernel could not allocate an
            // internal data structure. We will call
            // poll() again.
            ///////////////////////////////////////////////////

            goto CFS_SECUREREAD_POLL_WRITE;
          }
          else {

            *iSSLResult = errno;
             return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
          }
        }
      }

      break;

    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_SSL:
    default:

      *iSSLResult = errno;
      return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_SYSTEM;
  }

  *maxSize = bytes;

  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SecureReadRecord
//
// This function reads a specific number of bytes on a secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it reads on the descriptor
//             or until a session close or until an error occurs.
// 
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureReadRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* iSSLResult) {

  int rc;
  int readSize;
  int bytes;

  uint64_t leftToRead;
  uint64_t offset;

  struct pollfd fdset[1];

  readSize = (int)(*size > INT_MAX ?
                           INT_MAX :
                           *size);

  leftToRead = *size;
  *size = 0;
  offset = 0;

  do {

    CFS_SECUREREADRECORD_AGAIN:  

    bytes = SSL_read(This->ssl, buffer + offset, readSize);

    switch(SSL_get_error(This->ssl, bytes)) {
                  
      case SSL_ERROR_NONE:

        /////////////////////////////////////////////////////////////////
        // We got some data.
        /////////////////////////////////////////////////////////////////

        offset += bytes;
        *size += bytes;
        leftToRead -= bytes;

        readSize = (int)(leftToRead > INT_MAX ?
                                      INT_MAX :
                                      leftToRead);
                              
        break;
                              
      case SSL_ERROR_ZERO_RETURN:

        /////////////////////////////////////////////////////////////////
        // This indicates a connection close; we are done.
        /////////////////////////////////////////////////////////////////

        *iSSLResult = errno;
        return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_CONNCLOSE;

      case SSL_ERROR_WANT_READ:

        /////////////////////////////////////////////////////////////////
        // This indicates that we need for the socket descriptor
        // to become readable, possibly for session renegociation, after
        // which, we must re-issue SSL_read exactly as before.
        /////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        // This branching label for restarting an interrupted
        // poll call. An interrupted system call may result from
        // a caught signal and will have errno set to EINTR. We
        // must call poll again.

        CFS_SECUREREADRECORD_POLL_READ:
  
        //
        ////////////////////////////////////////////////////////////

        fdset[0].fd = This->connfd;
        fdset[0].events = POLLIN;

        if (timeout != -1) {
          rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
        }
        else {
          rc = poll(fdset, 1,
               This->readTimeout >= 0 ? This->readTimeout * 1000: -1);
        }

        if (rc == 1) {

          /////////////////////////////////////////////////////////
          // If we get anything other than POLLIN
          // this means we got an error.
          /////////////////////////////////////////////////////////
 
          if (!(fdset[0].revents & POLLIN)) {

            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }

          goto CFS_SECUREREADRECORD_AGAIN;
        }
        else {

          if (rc == 0) {
            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }
          else {

            if (errno == EINTR) {

              ///////////////////////////////////////////////////
              // poll() was interrupted by a signal
              // or the kernel could not allocate an
              // internal data structure. We will call
              // poll() again.
              ///////////////////////////////////////////////////

              goto CFS_SECUREREADRECORD_POLL_READ;
            }
            else {

              *iSSLResult = errno;
              return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
            }
          }
        }

        break;

      case SSL_ERROR_WANT_WRITE:

        /////////////////////////////////////////////////////////////////
        // This indicates that we need for the socket descriptor
        // to become writeable, possibly for session renegociation, after
        // which, we must re-issue SSL_read exactly as before.
        // We don't call SSL_write despite the name of this exception.
        // So we wait until descriptor is writeable and we jump back 
        // to SSL_read
        /////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        // This branching label for restarting an interrupted
        // poll call. An interrupted system call may result from
        // a caught signal and will have errno set to EINTR. We
        // must call poll again.

        CFS_SECUREREADRECORD_POLL_WRITE:
  
        //
        ////////////////////////////////////////////////////////////

        fdset[0].fd = This->connfd;
        fdset[0].events = POLLOUT;

        if (timeout != -1) {
          rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
        }
        else {
          rc = poll(fdset, 1,
               This->writeTimeout >= 0 ? This->writeTimeout * 1000: -1);
        }

        if (rc == 1) {

          /////////////////////////////////////////////////////////
          // If we get anything other than POLLOUT
          // this means we got an error.
          /////////////////////////////////////////////////////////
 
          if (!(fdset[0].revents & POLLOUT)) {

            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }

          goto CFS_SECUREREADRECORD_AGAIN;
        }
        else {

          if (rc == 0) {
            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }
          else {

            if (errno == EINTR) {

              ///////////////////////////////////////////////////
              // poll() was interrupted by a signal
              // or the kernel could not allocate an
              // internal data structure. We will call
              // poll() again.
              ///////////////////////////////////////////////////

              goto CFS_SECUREREADRECORD_POLL_WRITE;
            }
            else {

              *iSSLResult = errno;
              return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
            }
          }
        }

        break;

      case SSL_ERROR_SYSCALL:
      case SSL_ERROR_SSL:
      default:
        *iSSLResult = errno;
        return CS_FAILURE | CFS_OPER_READ | CFS_DIAG_SYSTEM;
    }
  }
  while (leftToRead > 0);

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
// CFS_CloseChannel
//
// This function closes a server session.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_CloseChannel
    (CFS_SESSION* This,
     int* iSSLResult) {

   if (This->secMode == 1) {
     SSL_shutdown(This->ssl);
     SSL_free(This->ssl);
   }

   close(This->connfd);
   This->connfd = -1;

   // Close the environement if it is local
   if (This->pLocalEnv != 0) {
     CFS_CloseEnv(&(This->pLocalEnv));
   }

   CFS_Destructor(&This);

   return CS_SUCCESS;
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
    (CFS_SESSION* This,
     int* iSSLResult) {

   if (This->secMode == 1) {
     SSL_shutdown(This->ssl);
     SSL_free(This->ssl);
   }

   close(This->connfd);
   This->connfd = -1;

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

  if (*pEnv != 0) {

    if ((*pEnv)->secMode == 1) {
      SSL_CTX_free((*pEnv)->ctx);
    }

    CFSRPS_Destructor(&((*pEnv)->TlsCfg_Session));
    CFSRPS_Destructor(&((*pEnv)->TlsCfg_Env));

    free(*pEnv);
    *pEnv = 0;
   }

   return CS_SUCCESS;
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
    (CFS_SESSION* This,
     int* iSSLResult) {

   if (This->secMode == 1) {
     if (This->connfd >= 0) {
       SSL_shutdown(This->ssl);
       SSL_free(This->ssl);
       close(This->connfd);
     }
   }
   else {
     if (This->connfd >= 0) {
       close(This->connfd);
     }
   }

   This->connfd = -1;

   // Close the environement if it is local

   if (This->pLocalEnv != 0) {
     CFS_CloseEnv(&(This->pLocalEnv));
   }

   CFS_Destructor(&This);

   return CS_SUCCESS;
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

  CFSENV* Instance;

  CFSRPS_CONFIGINFO ci;
  CFSRPS_PARAMINFO cfscpi;

  Instance = (CFSENV*)malloc(sizeof(CFSENV));

  Instance->TlsCfg_Env = CFSRPS_Constructor();
  Instance->TlsCfg_Session = CFSRPS_Constructor();

  CSSTR_Trim(szConfigName, Instance->szConfigName);

  if (Instance->szConfigName[0] == 0) {

    Instance->lpVtbl = &dftVtbl;

    // Indicate that we run in non-secure mode
    Instance->secMode = 0;
  }
  else {

    // Seed random number generator
    //RAND_load_file("/dev/random", 4097);

    // Load secure configuration
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    strcpy(ci.szPath, Instance->szConfigName);  
    if (CS_FAIL(CFSRPS_LoadConfig(Instance->TlsCfg_Env, &ci))) {

      CFSRPS_Destructor(&(Instance->TlsCfg_Env));
      CFSRPS_Destructor(&(Instance->TlsCfg_Session));
      free(Instance);
      return 0;
    }  

    if (CS_FAIL(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                "TLS_SESSION_TYPE", &cfscpi))) {
      CFSRPS_Destructor(&(Instance->TlsCfg_Env));
      CFSRPS_Destructor(&(Instance->TlsCfg_Session));
      free(Instance);
      return 0;
    }

    if (!strcmp("*CLIENT", cfscpi.szValue)) {

      Instance->ctx = SSL_CTX_new(TLS_client_method());

      // By default, we make a client validate peer certificates; OpenSSL
      // does not validate certificatesd by default. Unfortunately, we 
      // must duplicate certificate validation code if the configuration
      // verify key value is missing for client sessions.

      if (CS_FAIL(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                  "TLS_VALIDATE_PEER", &(cfscpi)))) {

        SSL_CTX_set_verify(Instance->ctx, SSL_VERIFY_PEER, CFS_SSL_VerifyCallback);
        CFS_PRV_SecureLoadTrustStore(Instance);
      }
      else {
        if (!strcmp(cfscpi.szValue, "*YES")) {

          SSL_CTX_set_verify(Instance->ctx, SSL_VERIFY_PEER, CFS_SSL_VerifyCallback);
          CFS_PRV_SecureLoadTrustStore(Instance);
        }
        else {
          SSL_CTX_set_verify(Instance->ctx, SSL_VERIFY_NONE, CFS_SSL_VerifyCallback);
        }
      }
    }
    else {

      Instance->ctx = SSL_CTX_new(TLS_server_method());

      // Server certificate
      if (CS_SUCCEED(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                  "TLS_CERT_FILE", &(cfscpi)))) {

        if (SSL_CTX_use_certificate_file(Instance->ctx, 
                               cfscpi.szValue, SSL_FILETYPE_PEM) <= 0) {
          CFSRPS_Destructor(&(Instance->TlsCfg_Env));
          CFSRPS_Destructor(&(Instance->TlsCfg_Session));
          free(Instance);
          return 0;
        }

        if (CS_SUCCEED(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                  "TLS_CERT_KEY_FILE", &(cfscpi)))) {

          if (SSL_CTX_use_PrivateKey_file(Instance->ctx, 
                                 cfscpi.szValue, SSL_FILETYPE_PEM) <= 0 ) {
            CFSRPS_Destructor(&(Instance->TlsCfg_Env));
            CFSRPS_Destructor(&(Instance->TlsCfg_Session));
            free(Instance);
            return 0;
          }
        }
        else {
          CFSRPS_Destructor(&(Instance->TlsCfg_Env));
          CFSRPS_Destructor(&(Instance->TlsCfg_Session));
          free(Instance);
          return 0;
        }
      }
      else {
        CFSRPS_Destructor(&(Instance->TlsCfg_Env));
        CFSRPS_Destructor(&(Instance->TlsCfg_Session));
        free(Instance);
        return 0;
      }

      // For server sessions, we do not validate client certificates 
      // by default, unless the configuration states otherwise.

      if (CS_SUCCEED(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                  "TLS_VALIDATE_PEER", &(cfscpi)))) {

      	STACK_OF(X509_NAME) *certs;
 
        if (CS_SUCCEED(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                  "TLS_CA_LIST_FILE", &(cfscpi)))) {

        	certs = SSL_load_client_CA_file(cfscpi.szValue);

	        if (certs != NULL) {

            // Note that the following function will have 
            // the context take ownership
            // of the stack and free it when the context is freed.
	          SSL_CTX_set_client_CA_list(Instance->ctx, certs);

            if (CS_SUCCEED(CFSRPS_LookupParam(Instance->TlsCfg_Env,
                                        "TLS_CA_LIST_DIR", &(cfscpi)))) { 
              SSL_add_dir_cert_subjects_to_stack(certs, cfscpi.szValue);
            }  
          }
        }
      }
    }

    ///////////////////////////////////////////////////////////
    // Retrieve peer validation parameters
    ///////////////////////////////////////////////////////////

    // Virtual function table for secure read/write functions
    Instance->lpVtbl = &secureVtbl;

    Instance->secMode = 1;
  }

  return Instance;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_OpenChannel
//
// This function initializes a server session.
//
//
//  Cases:
//
//     pEnv != 0, szSessionConfig == 0
//        Use global environment and global session configuration
//        caller must provide host and port.
//
//     pEnv != 0, szSessionConfig != 0
//        Use global environment and local TLS session configuration
//        if host/port is provided, they will override session
//        configuration host/port if any.
//
//     pEnv == 0, szSessionConfig == 0
//        This is a non-secure session and host/port must be provided.
//
//     pEnv == 0, szSessionConfig != 0
//        Use local environment and local session configuration
//        if host/port is provided, they will override session
//        configuration host/port if any.
//
//////////////////////////////////////////////////////////////////////////////

CFS_SESSION*
  CFS_OpenChannel
    (CFSENV* pEnv,
     char* szSessionConfig,
     int connfd,
     int* iSSLResult) {

  int rc;

  CFS_SESSION* This;

  struct pollfd fdset[1];

  This = CFS_Constructor();

  if (szSessionConfig != 0) {

    strcpy(This->ci.szPath, szSessionConfig);

    if (CS_SUCCEED(CFSRPS_LoadConfig(This->Cfg, &(This->ci)))) {

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg,
                                     "READ_TO", &(This->cfscpi)))) {
        This->readTimeout = 20;  // forever
      }
      else {
        This->readTimeout = (int)strtol(This->cfscpi.szValue, 0, 10);
      }

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg,
                                     "WRITE_TO", &(This->cfscpi)))) {
        This->writeTimeout = 20; // forever
      }
      else {
        This->writeTimeout = (int)strtol(This->cfscpi.szValue, 0, 10);
      }

      ////////////////////////////////////////////////////////////
      // Get secure mode
      ////////////////////////////////////////////////////////////

      if (CS_SUCCEED(CFSRPS_LookupParam(This->Cfg,
                                   "SECURE_CONFIG", &(This->cfscpi)))) {

        if (pEnv == 0) {

          // Seed random number generator
          //RAND_load_file("/dev/random", 4097);

          This->pEnv = This->pLocalEnv = CFS_OpenEnv(This->cfscpi.szValue);

          if (This->pEnv == 0) {
            CFS_Destructor(&This);
            return 0;
          }

          This->TlsCfg_Session = This->pEnv->TlsCfg_Session;

          This->secMode = This->pEnv->secMode;

          // Use environment vTable
          This->lpVtbl = This->pEnv->lpVtbl;
        }
        else {

          ///////////////////////////////////////////////////////
          // we want the session TLS configuration only
          ///////////////////////////////////////////////////////
          /*

          List SSL parameters here

          // Alias TLS session config for later use
          This->TlsCfg_Session = This->TlsCfg_LocalSession;
          */

          // adopt global environment
          This->pEnv = pEnv;
          This->secMode = This->pEnv->secMode;

          // Use environment vTable
          This->lpVtbl = This->pEnv->lpVtbl;
        }
      }
      else {

        if (pEnv == 0) {

          // This means it is a non-secure session.
          // Use non-secure default vTable

          This->secMode = 0;
          This->lpVtbl = &dftVtbl;
        }
        else {

          // adopt global environment
          This->pEnv = pEnv;

          // Use environment vTable
          This->lpVtbl = This->pEnv->lpVtbl;
        }
      }
    }
    else {
      CFS_Destructor(&This);
      return 0;
    }
  }
  else {

    This->readTimeout  = -1; // forever
    This->writeTimeout = -1; // forever

    if (pEnv == 0) {

      // This means it is a non-secure session.
      This->secMode = 0;
      // Use non-secure default vTable
      This->lpVtbl = &dftVtbl;
    }
    else {

      // use global environment TLS session parameters
      // Alias TLS session config for later use

      This->TlsCfg_Session = pEnv->TlsCfg_Session;

      // adopt global environment
      This->pEnv = pEnv;
      This->secMode = This->pEnv->secMode;

      // Use environment vTable
      This->lpVtbl = This->pEnv->lpVtbl;
    }
  }

  This->connfd = connfd;

  if (This->secMode == 1) {

    // perform handshake

    This->ssl = SSL_new(This->pEnv->ctx);
    
    // Set socket to NON-blocking mode
    CFS_PRV_SetBlocking(This->connfd, 0);

    SSL_set_fd(This->ssl, This->connfd);

    while (1) 
    {

      rc = SSL_accept(This->ssl); 

      switch (SSL_get_error(This->ssl, rc))
      {
        case SSL_ERROR_NONE:

          return This;

        case SSL_ERROR_ZERO_RETURN:

          goto CFS_OPENCHANNEL_CLOSE_SSL;

        case SSL_ERROR_WANT_READ:

          fdset[0].fd = This->connfd;
          fdset[0].events = POLLIN;

          CFS_OPENCHANNEL_POLL_READ:

          rc = poll(fdset, 1,
                  This->connectTimeout >= 0 ? 
                  This->connectTimeout * 1000: -1);

          if (rc == 1) {

            /////////////////////////////////////////////////////////
            // If we get anything other than POLLIN
            // this means we got an error.
            /////////////////////////////////////////////////////////

            if (!(fdset[0].revents & POLLIN)) {
              *iSSLResult = errno;
              goto CFS_OPENCHANNEL_CLOSE_SSL;
            }
          }
          else {

            if (rc == 0) { // timed out
              *iSSLResult = errno;
              goto CFS_OPENCHANNEL_CLOSE_SSL;
            }
            else {

              if (errno == EINTR) { // poll() interrupted
                goto CFS_OPENCHANNEL_POLL_READ;
              }
              else { // system error
                *iSSLResult = errno;
                goto CFS_OPENCHANNEL_CLOSE_SSL;
              }
            }
          }

          break;

        case SSL_ERROR_WANT_WRITE:

          fdset[0].fd = This->connfd;
          fdset[0].events = POLLOUT;

          CFS_OPENCHANNEL_POLL_WRITE:

          rc = poll(fdset, 1,
                  This->connectTimeout >= 0 ? 
                  This->connectTimeout * 1000: -1);

          if (rc == 1) {

            /////////////////////////////////////////////////////////
            // If we get anything other than POLLOUT
            // this means we got an error.
            /////////////////////////////////////////////////////////

            if (!(fdset[0].revents & POLLOUT)) {
              *iSSLResult = errno;
              goto CFS_OPENCHANNEL_CLOSE_SSL;
            }
          }
          else {

            if (rc == 0) { // timed out
              *iSSLResult = errno;
              goto CFS_OPENCHANNEL_CLOSE_SSL;
            }
            else {

              if (errno == EINTR) { // poll() interrupted
                goto CFS_OPENCHANNEL_POLL_WRITE;
              }
              else { // system error
                *iSSLResult = errno;
                goto CFS_OPENCHANNEL_CLOSE_SSL;
              }
            }
          }

          break;

        default:

          goto CFS_OPENCHANNEL_CLOSE_SSL;
      }
    }

    CFS_OPENCHANNEL_CLOSE_SSL:

    SSL_shutdown(This->ssl);
    SSL_free(This->ssl);
    close(This->connfd);
    This->connfd = -1;

    if (This->pLocalEnv != 0) {
      CFS_CloseEnv(&(This->pLocalEnv));
    }
  }
  else {
    // Set socket to blocking mode
    CFS_PRV_SetBlocking(This->connfd, 1);
    return This;
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_OpenSession
//
// This function initialises a connection to a server.
//
//
//  Cases:
//
//     pEnv != 0, szSessionConfig == 0
//        Use global environment and global session configuration
//        caller must provide host and port.
//
//     pEnv != 0, szSessionConfig != 0
//        Use global environment and local TLS session configuration
//        if host/port is provided, they will override session
//        configuration host/port if any.
//
//     pEnv == 0, szSessionConfig == 0
//        This is a non-secure session and host/port must be provided.
//
//     pEnv == 0, szSessionConfig != 0
//        Use local environment and local session configuration
//        if host/port is provided, they will override session
//        configuration host/port if any.
//
//////////////////////////////////////////////////////////////////////////////

CFS_SESSION*
  CFS_OpenSession
    (CFSENV* pEnv,
     char* szSessionConfig,
     char* szHost,
     char* szPort,
     int* iSSLResult) {

  int rc;
  
  CSRESULT hResult;

  struct addrinfo* addrInfo;
  struct addrinfo* addrInfo_first;
  struct addrinfo  hints;

  struct pollfd fdset[1];

  CFS_SESSION* This;

  This = CFS_Constructor();

  if (szSessionConfig != 0) {

    strcpy(This->ci.szPath, szSessionConfig);

    if (CS_SUCCEED(CFSRPS_LoadConfig(This->Cfg, &(This->ci)))) {

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg, "HOST", &(This->cfscpi)))) {

        if (szHost != 0) {
          strcpy(This->szHostName, szHost);
        }
        else {
          strcpy(This->szHostName, "");
        }
      }
      else {
        strcpy(This->szHostName, This->cfscpi.szValue);
      }

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg, "PORT", &(This->cfscpi)))) {

        if (szPort != 0) {
          strcpy(This->szPort, szPort);
        }
        else {
          strcpy(This->szPort, "-1");
        }
      }
      else {
        strcpy(This->szPort, This->cfscpi.szValue);
      }

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg,
                                     "CONN_TO", &(This->cfscpi)))) {
        This->connectTimeout = 20;
      }
      else {
        This->connectTimeout = (int)strtol(This->cfscpi.szValue, 0, 10);
      }

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg,
                                     "READ_TO", &(This->cfscpi)))) {
        This->readTimeout = 20;
      }
      else {
        This->readTimeout = (int)strtol(This->cfscpi.szValue, 0, 10);
      }

      if (CS_FAIL(CFSRPS_LookupParam(This->Cfg,
                                     "WRITE_TO", &(This->cfscpi)))) {
        This->writeTimeout = 20;
      }
      else {
        This->writeTimeout = (int)strtol(This->cfscpi.szValue, 0, 10);
      }

      ////////////////////////////////////////////////////////////
      // Get secure mode
      ////////////////////////////////////////////////////////////

      if (CS_SUCCEED(CFSRPS_LookupParam(This->Cfg,
                                        "SECURE_CONFIG", &(This->cfscpi)))) {

        if (pEnv == 0) {

          This->pEnv = This->pLocalEnv = CFS_OpenEnv(This->cfscpi.szValue);

          if (This->pEnv == 0) {
            CFS_Destructor(&This);
            return 0;
          }

          This->TlsCfg_Session = This->pEnv->TlsCfg_Env;

          This->secMode = This->pEnv->secMode;

          // Use environment vTable
          This->lpVtbl = This->pEnv->lpVtbl;
        }
        else {

          ///////////////////////////////////////////////////////
          // we want the session TLS configuration only
          ///////////////////////////////////////////////////////

          // Alias TLS session config for later use
          This->TlsCfg_Session = This->pEnv->TlsCfg_Env;

          // adopt global environment
          This->pEnv = pEnv;
          This->secMode = This->pEnv->secMode;

          // Use environment vTable
          This->lpVtbl = This->pEnv->lpVtbl;
        }
      }
      else {

        if (pEnv == 0) {

          // This means it is a non-secure session.
          // Use non-secure default vTable

          This->secMode = 0;
          This->lpVtbl = &dftVtbl;
        }
        else {

          // adopt global environment
          This->pEnv = pEnv;
          This->secMode = This->pEnv->secMode;

          // Use environment vTable
          This->lpVtbl = This->pEnv->lpVtbl;
        }
      }
    }
    else {
      CFS_Destructor(&This);
      return 0;
    }
  }
  else {

    if (szPort == 0 || szHost == 0) {
      CFS_Destructor(&This);
      return 0;
    }

    strcpy(This->szHostName, szHost);
    strcpy(This->szPort, szPort);

    This->connectTimeout = 20;
    This->readTimeout = 20;
    This->writeTimeout = 20;

    if (pEnv == 0) {

      // This means it is a non-secure session.
      This->secMode = 0;
      // Use non-secure default vTable
      This->lpVtbl = &dftVtbl;
    }
    else {

      // use global environment TLS session parameters
      // Alias TLS session config for later use

      This->TlsCfg_Session = pEnv->TlsCfg_Env;

      // adopt global environment
      This->pEnv = pEnv;
      This->secMode = This->pEnv->secMode;

      // Use environment vTable
      This->lpVtbl = This->pEnv->lpVtbl;
    }
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  hResult = CS_FAILURE;

  if (getaddrinfo(This->szHostName,
                   This->szPort,
                   &hints,
                   &addrInfo) == 0) {

    addrInfo_first = addrInfo;

    while (addrInfo != 0)
    {
      This->connfd = socket(addrInfo->ai_family,
                            addrInfo->ai_socktype,
                            addrInfo->ai_protocol);

      if (This->connfd >= 0) {

        // Set socket to non-blocking
        CFS_PRV_SetBlocking(This->connfd, 0);

        rc = connect(This->connfd,
                     addrInfo->ai_addr,
                     addrInfo->ai_addrlen);

        if (rc < 0) {

          if (errno != EINPROGRESS) {
            *iSSLResult = errno;
            close(This->connfd);
          }
          else {

            fdset[0].fd = This->connfd;
            fdset[0].events = POLLOUT;

            CFS_OPENSESSION_CONNECT_POLL_READ:

            rc = poll(fdset, 1,
                   This->connectTimeout >= 0 ? 
                   This->connectTimeout * 1000: -1);

            if (rc == 1) {

              /////////////////////////////////////////////////////////
              // If we get anything other than POLLOUT
              // this means we got an error.
              /////////////////////////////////////////////////////////
 
              if (fdset[0].revents & POLLOUT) {
                hResult = CS_SUCCESS;
                break;  // leave the connection loop
              } 
              else {
                *iSSLResult = errno;
                close(This->connfd);
              }
            }
            else {

              if (rc == 0) {
                *iSSLResult = errno;
                close(This->connfd);
              }
              else {

                if (errno == EINTR) {
                  goto CFS_OPENSESSION_CONNECT_POLL_READ;
                }
                else {
                  *iSSLResult = errno;
                  close(This->connfd);
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

    if (This->secMode == 1) {

      This->ssl = SSL_new(This->pEnv->ctx);

      SSL_set_tlsext_host_name(This->ssl, This->szHostName);

      SSL_set_fd(This->ssl, This->connfd);

      while (1) 
      {
        rc = SSL_connect(This->ssl);

        switch (SSL_get_error(This->ssl, rc))
        {
          case SSL_ERROR_NONE:

            // SSL handshake succeeded, we return the session handle

            /* ---------------------------------------------------------------------
              The following shows how to retrieve information from the 
              peer certificate
            {
                X509 *cert;
                char *line;
                cert = SSL_get_peer_certificate(This->ssl); // get the server's certificate
                if ( cert != NULL )
                {
                    printf("Server certificates:\n");
                    line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
                    printf("Subject: %s\n", line);
                    free(line);       // free the malloc'ed string
                    line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
                    printf("Issuer: %s\n", line);
                    free(line);       // free the malloc'ed string 
                    X509_free(cert);     // free the malloc'ed certificate copy 
                }
                else
                    printf("Info: No client certificates configured.\n");
            }
            ----------------------------------------------------------------------- */

            return This;

          case SSL_ERROR_ZERO_RETURN:

            goto CFS_OPENSESSION_CLOSE_SSL;

          case SSL_ERROR_WANT_READ:

            fdset[0].fd = This->connfd;
            fdset[0].events = POLLIN;

            CFS_OPENSESSION_POLL_READ:

            rc = poll(fdset, 1,
                   This->connectTimeout >= 0 ? 
                   This->connectTimeout * 1000: -1);

            if (rc == 1) {

              /////////////////////////////////////////////////////////
              // If we get anything other than POLLIN
              // this means we got an error.
              /////////////////////////////////////////////////////////
 
              if (!(fdset[0].revents & POLLIN)) {
                *iSSLResult = errno;
                goto CFS_OPENSESSION_CLOSE_SSL;
              }
            }
            else {

              if (rc == 0) { // timed out
                *iSSLResult = errno;
                goto CFS_OPENSESSION_CLOSE_SSL;
              }
              else {

                if (errno == EINTR) { // poll() interrupted
                  goto CFS_OPENSESSION_POLL_READ;
                }
                else { // system error
                  *iSSLResult = errno;
                  goto CFS_OPENSESSION_CLOSE_SSL;
                }
              }
            }

            break;

          case SSL_ERROR_WANT_WRITE:

            fdset[0].fd = This->connfd;
            fdset[0].events = POLLOUT;

            CFS_OPENSESSION_POLL_WRITE:

            rc = poll(fdset, 1,
                   This->connectTimeout >= 0 ? 
                   This->connectTimeout * 1000: -1);

            if (rc == 1) {

              /////////////////////////////////////////////////////////
              // If we get anything other than POLLOUT
              // this means we got an error.
              /////////////////////////////////////////////////////////
 
              if (!(fdset[0].revents & POLLOUT)) {
                *iSSLResult = errno;
                goto CFS_OPENSESSION_CLOSE_SSL;
              }
            }
            else {

              if (rc == 0) { // timed out
                *iSSLResult = errno;
                goto CFS_OPENSESSION_CLOSE_SSL;
              }
              else {

                if (errno == EINTR) { // poll() interrupted
                  goto CFS_OPENSESSION_POLL_WRITE;
                }
                else { // system error
                  *iSSLResult = errno;
                  goto CFS_OPENSESSION_CLOSE_SSL;
                }
              }
            }

            break;

          default:

            goto CFS_OPENSESSION_CLOSE_SSL;
        }
      }

      CFS_OPENSESSION_CLOSE_SSL:

      SSL_shutdown(This->ssl);
      SSL_free(This->ssl);
      close(This->connfd);
      This->connfd = -1;

      if (This->pLocalEnv != 0) {
        CFS_CloseEnv(&(This->pLocalEnv));
      }
    }
    else {
      // Set socket back to blocking
      CFS_PRV_SetBlocking(This->connfd, 1);
      return This;
    }
  }
  else {
    close(This->connfd);
    This->connfd = -1;
  }

  CFS_Destructor(&This);

  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_QueryConfig
//
// This function retrieves a value from the CFS configuration.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_QueryConfig
    (CFS_SESSION* This,
     char* szParam,
     CFSRPS_PARAMINFO* cfscpi) {

  return CFSRPS_LookupParam(This->Cfg, szParam, cfscpi);
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
// CFS_SecureWrite
//
// This function writes to a secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it writes on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureWrite
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* iSSLResult) {

  int rc;
  int writeSize;
  int bytes;

  struct pollfd fdset[1];

  writeSize = *maxSize > INT_MAX ?            
                         INT_MAX :
                         *maxSize;

  *maxSize = 0;

  CFS_SECUREWRITE_AGAIN:  

  bytes = SSL_write(This->ssl, buffer, writeSize);

  switch(SSL_get_error(This->ssl, bytes)) {
                  
    case SSL_ERROR_NONE:
      
      // we have read something
      break;

    case SSL_ERROR_ZERO_RETURN:

      /////////////////////////////////////////////////////////////////
      // This indicates a connection close; we are done.
      /////////////////////////////////////////////////////////////////

      *iSSLResult = errno;
      return CS_SUCCESS | CFS_OPER_WRITE | CFS_DIAG_CONNCLOSE;

    case SSL_ERROR_WANT_READ:

      /////////////////////////////////////////////////////////////////
      // This indicates that we need for the socket descriptor
      // to become readable, possibly for session renegociation, after
      // which, we must re-issue SSL_write exactly as before.
      // We don't call SSL_read despite the name of this exception.
      // So we wait until descriptor is readable and we jump back 
      // to SSL_write
      /////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////
      // This branching label for restarting an interrupted
      // poll call. An interrupted system call may result from
      // a caught signal and will have errno set to EINTR. We
      // must call poll again.

      CFS_SECUREWRITE_POLL_READ:
  
      //
      ////////////////////////////////////////////////////////////

      fdset[0].fd = This->connfd;
      fdset[0].events = POLLIN;

      if (timeout != -1) {
        rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
      }
      else {
        rc = poll(fdset, 1,
             This->readTimeout >= 0 ? This->readTimeout * 1000: -1);
      }

      if (rc == 1) {

        /////////////////////////////////////////////////////////
        // If we get anything other than POLLIN
        // this means we got an error.
        /////////////////////////////////////////////////////////
 
        if (!(fdset[0].revents & POLLIN)) {

          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }

        goto CFS_SECUREWRITE_AGAIN;
      }
      else {

        if (rc == 0) {
          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }
        else {

          if (errno == EINTR) {

            ///////////////////////////////////////////////////
            // poll() was interrupted by a signal
            // or the kernel could not allocate an
            // internal data structure. We will call
            // poll() again.
            ///////////////////////////////////////////////////

            goto CFS_SECUREWRITE_POLL_READ;
          }
          else {

            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
          }
        }
      }

      break;

    case SSL_ERROR_WANT_WRITE:

      /////////////////////////////////////////////////////////////////
      // This indicates that we need for the socket descriptor
      // to become writable, possibly for session renegociation, after
      // which, we must re-issue SSL_write exactly as before.
      // We wait until descriptor is writable and we jump back 
      // to SSL_write.
      /////////////////////////////////////////////////////////////////

      ////////////////////////////////////////////////////////////
      // This branching label for restarting an interrupted
      // poll call. An interrupted system call may result from
      // a caught signal and will have errno set to EINTR. We
      // must call poll again.

      CFS_SECUREWRITE_POLL_WRITE:
  
      //
      ////////////////////////////////////////////////////////////

      fdset[0].fd = This->connfd;
      fdset[0].events = POLLOUT;

      if (timeout != -1) {
        rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
      }
      else {
        rc = poll(fdset, 1,
             This->writeTimeout >= 0 ? This->writeTimeout * 1000: -1);
      }

      if (rc == 1) {

        /////////////////////////////////////////////////////////
        // If we get anything other than POLLOUT
        // this means we got an error.
        /////////////////////////////////////////////////////////
 
        if (!(fdset[0].revents & POLLOUT)) {

          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }

        goto CFS_SECUREWRITE_AGAIN;
      }
      else {

        if (rc == 0) {
          *iSSLResult = errno;
          return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
        }
        else {

          if (errno == EINTR) {

            ///////////////////////////////////////////////////
            // poll() was interrupted by a signal
            // or the kernel could not allocate an
            // internal data structure. We will call
            // poll() again.
            ///////////////////////////////////////////////////

            goto CFS_SECUREWRITE_POLL_WRITE;
          }
          else {

            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
          }
        }
      }

      break;

    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_SSL:
    default:
                  
      *iSSLResult = errno;
      return CS_FAILURE | CFS_OPER_WRITE | CFS_DIAG_SYSTEM;
  }

  *maxSize = bytes;

  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_SecureWriteRecord
//
// This function writes a specific number of bytes to a secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it writes on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_SecureWriteRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* iSSLResult) {

  int rc;
  int writeSize;
  int bytes;

  uint64_t leftToWrite;
  uint64_t offset;

  struct pollfd fdset[1];

  writeSize = (int)(*size > INT_MAX ? 
                            INT_MAX : 
                            *size);

  leftToWrite = *size;
  *size = 0;
  offset = 0;

  do {

    CFS_SECUREWRITERECORD_AGAIN:  

    bytes = SSL_write(This->ssl, buffer + offset, writeSize);

    switch(SSL_get_error(This->ssl, bytes)) {
                  
      case SSL_ERROR_NONE:

        /////////////////////////////////////////////////////////////////
        // We got some data.
        /////////////////////////////////////////////////////////////////

        offset += bytes;
        *size += bytes;
        leftToWrite -= bytes;

        writeSize = (int)(leftToWrite > INT_MAX ? 
                                        INT_MAX : 
                                        leftToWrite);

        break;

      case SSL_ERROR_ZERO_RETURN:

        /////////////////////////////////////////////////////////////////
        // This indicates a connection close; we are done.
        /////////////////////////////////////////////////////////////////

        *iSSLResult = errno;
        return CS_FAILURE | CFS_OPER_WRITE | CFS_DIAG_CONNCLOSE;

      case SSL_ERROR_WANT_READ:

        /////////////////////////////////////////////////////////////////
        // This indicates that we need for the socket descriptor
        // to become readable, possibly for session renegociation, after
        // which, we must re-issue SSL_write exactly as before.
        // We don't call SSL_read despite the name of this exception.
        // So we wait until descriptor is readable and we jump back 
        // to SSL_write
        /////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        // This branching label for restarting an interrupted
        // poll call. An interrupted system call may result from
        // a caught signal and will have errno set to EINTR. We
        // must call poll again.

        CFS_SECUREWRITERECORD_POLL_READ:
  
        //
        ////////////////////////////////////////////////////////////

        fdset[0].fd = This->connfd;
        fdset[0].events = POLLIN;

        if (timeout != -1) {
          rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
        }
        else {
          rc = poll(fdset, 1,
             This->readTimeout >= 0 ? This->readTimeout * 1000: -1);
        }

        if (rc == 1) {

          /////////////////////////////////////////////////////////
          // If we get anything other than POLLIN
          // this means we got an error.
          /////////////////////////////////////////////////////////
 
          if (!(fdset[0].revents & POLLIN)) {

            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }

          goto CFS_SECUREWRITERECORD_AGAIN;
        }
        else {

          if (rc == 0) {
            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }
          else {

            if (errno == EINTR) {

              ///////////////////////////////////////////////////
              // poll() was interrupted by a signal
              // or the kernel could not allocate an
              // internal data structure. We will call
              // poll() again.
              ///////////////////////////////////////////////////

              goto CFS_SECUREWRITERECORD_POLL_READ;
            }
            else {

              *iSSLResult = errno;
              return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
            }
          }
        }

        break;

      case SSL_ERROR_WANT_WRITE:

        /////////////////////////////////////////////////////////////////
        // This indicates that we need for the socket descriptor
        // to become writable, possibly for session renegociation, after
        // which, we must re-issue SSL_write exactly as before.
        // We wait until descriptor is writable and we jump back 
        // to SSL_write.
        /////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////
        // This branching label for restarting an interrupted
        // poll call. An interrupted system call may result from
        // a caught signal and will have errno set to EINTR. We
        // must call poll again.

        CFS_SECUREWRITERECORD_POLL_WRITE:
  
        //
        ////////////////////////////////////////////////////////////

        fdset[0].fd = This->connfd;
        fdset[0].events = POLLOUT;

        if (timeout != -1) {
          rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
        }
        else {
          rc = poll(fdset, 1,
             This->writeTimeout >= 0 ? This->writeTimeout * 1000: -1);
        }

        if (rc == 1) {

          /////////////////////////////////////////////////////////
          // If we get anything other than POLLOUT
          // this means we got an error.
          /////////////////////////////////////////////////////////
 
          if (!(fdset[0].revents & POLLOUT)) {

            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }

          goto CFS_SECUREWRITERECORD_AGAIN;
        }
        else {

          if (rc == 0) {
            *iSSLResult = errno;
            return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_TIMEDOUT;
          }
          else {

            if (errno == EINTR) {

              ///////////////////////////////////////////////////
              // poll() was interrupted by a signal
              // or the kernel could not allocate an
              // internal data structure. We will call
              // poll() again.
              ///////////////////////////////////////////////////

              goto CFS_SECUREWRITERECORD_POLL_WRITE;
            }
            else {

              *iSSLResult = errno;
              return CS_FAILURE | CFS_OPER_WAIT | CFS_DIAG_SYSTEM;
            }
          }
        }

        break;

      case SSL_ERROR_SYSCALL:
      case SSL_ERROR_SSL:
      default:
        *iSSLResult = errno;
        return CS_FAILURE | CFS_OPER_WRITE | CFS_DIAG_SYSTEM;
    }
  }
  while (leftToWrite > 0);

  return CS_SUCCESS;
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

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Write
//
// This function writes to a non-secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it writes on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_Write
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* maxSize,
     int timeout,
     int* e) {

   int rc;
   int writeSize;

   struct pollfd fdset[1];

   writeSize = (int)(*maxSize);

   fdset[0].fd = This->connfd;
   fdset[0].events = POLLOUT;

   ////////////////////////////////////////////////////////////
   // This branching label for restarting an interrupted
   // poll call. An interrupted system call may result from
   // a caught signal and will have errno set to EINTR. We
   // must call poll again.

   CFS_WAIT_POLL:

   //
   ////////////////////////////////////////////////////////////

   if (timeout != -1) {
     rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
   }
   else {
     rc = poll(fdset, 1,
             This->writeTimeout >= 0 ? This->writeTimeout * 1000: -1);
   }

   if (rc == 1) {

      if (!(fdset[0].revents & POLLOUT)) {

        /////////////////////////////////////////////////////////
        // If we get anything other than POLLOUT
        // this means an error occurred.
        /////////////////////////////////////////////////////////

        *e = errno;
        return   CS_FAILURE
               | CFS_OPER_WAIT
               | CFS_DIAG_SYSTEM;
      }
   }
   else {

      if (rc == 0) {

         return   CS_SUCCESS
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

            goto CFS_WAIT_POLL;
         }
         else {

            *e = errno;
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

   CFS_WAIT_WRITE:

   //
   /////////////////////////////////////////////////////////

   rc = send(This->connfd, buffer, writeSize, 0);

   if (rc < 0) {

      if (errno == EINTR) {

         ///////////////////////////////////////////////////
         // send() was interrupted by a signal
         // or the kernel could not allocate an
         // internal data structure. We will call
         // send() again.
         ///////////////////////////////////////////////////

         goto CFS_WAIT_WRITE;
      }
      else {

         *e = errno;
         return   CS_FAILURE
                  | CFS_OPER_WRITE
                  | CFS_DIAG_SYSTEM;
      }
   }
   else {

      if (rc == 0) {

         /////////////////////////////////////////////////////////////////
         // This indicates a connection close; we are done.
         /////////////////////////////////////////////////////////////////

         return CS_SUCCESS | CFS_OPER_WRITE | CFS_DIAG_CONNCLOSE;
      }
   }

   *maxSize = (uint64_t)rc;
   return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_WriteRecord
//
// This function writes a specific number of bytes to a non secure socket.
//
// The timeout parameter will NOT be taken into account if it is equal
// to -1, else, the timeout value in the configuration will be used. 
// Here are the possible values and the actual timeout that will be used:
//
//    >   0  : the function will wait the specified number of seconds
//    ==  0  : the function will not wait on the descriptor
//    == -1  : the function will use the timeout value in the configuration
//    <  -1  : the function will wait until either it writes on the descriptor
//             or until a session close or until an error occurs.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CFS_WriteRecord
    (CFS_SESSION* This,
     char* buffer,
     uint64_t* size,
     int timeout,
     int* e) {

   int rc;
   int writeSize;

   struct pollfd fdset[1];

   uint64_t initialSize;
   uint64_t leftToWrite;
   uint64_t offset;

   initialSize = *size;
   *size       = 0;
   offset      = 0;

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

      CFS_WAIT_POLL:

      //
      ////////////////////////////////////////////////////////////

      fdset[0].fd = This->connfd;
      fdset[0].events = POLLOUT;

      if (timeout != -1) {
        rc = poll(fdset, 1,
               timeout >= 0 ? timeout * 1000: -1);
      }
      else {
        rc = poll(fdset, 1,
             This->writeTimeout >= 0 ? This->writeTimeout * 1000: -1);
      }

      if (rc == 1) {

         /////////////////////////////////////////////////////////
         // If we get anything other than POLLOUT
         // this means we got an error.
         /////////////////////////////////////////////////////////

         if (!(fdset[0].revents & POLLOUT)) {

            *e = errno;
            return   CS_FAILURE
                     | CFS_OPER_WAIT
                     | CFS_DIAG_SYSTEM;
         }
      }
      else {

         if (rc == 0) {

            *e = errno;
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

               goto CFS_WAIT_POLL;
            }
            else {

               *e = errno;
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

      CFS_WAIT_SEND:

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

            goto CFS_WAIT_SEND;
         }
         else {

           *e = errno;
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

            *e = errno;
            return CS_FAILURE | CFS_OPER_WRITE | CFS_DIAG_CONNCLOSE;
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

CSRESULT
  CFS_SetChannelDescriptor
    (CFS_SESSION* This,
     int connfd,
     int* iSSLResult) {

  This->connfd = connfd;

  if (This->secMode == 1) {

  }

  // Set socket to blocking mode
  CFS_PRV_SetBlocking(This->connfd, 1);

  return CS_SUCCESS;
}

/* ---------------------------------------------------------------------------
   private functions
--------------------------------------------------------------------------- */

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

  Instance->Cfg = CFSRPS_Constructor();
  Instance->TlsCfg_LocalSession = CSLIST_Constructor();

  Instance->pEnv = 0;
  Instance->pLocalEnv = 0;

  return Instance;
}

//////////////////////////////////////////////////////////////////////////////
//
// CFS_Destructor
//
// This function releases the resources allocated by a network session object
//
//////////////////////////////////////////////////////////////////////////////

void
  CFS_Destructor
    (CFS_SESSION** This) {

  CFSRPS_Destructor(&((*This)->Cfg));
  CSLIST_Destructor(&((*This)->TlsCfg_LocalSession));

  free(*This);
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

int 
  CFS_SSL_VerifyCallback
    (int status, 
     X509_STORE_CTX *store) {

  char    szBuffer[256];
  X509*   cert;
  int     error; 
  int     depth;

  if (status == 0) {

    cert = X509_STORE_CTX_get_current_cert(store);
    depth = X509_STORE_CTX_get_error_depth(store);
    error = X509_STORE_CTX_get_error(store);

    printf("CFS - ERROR - TLS Certificate validation error: depth: %d error: %d - %s",
          depth,
          error,
          X509_verify_cert_error_string(error));

    X509_NAME_oneline(X509_get_issuer_name(cert), szBuffer, 255);
    printf("\n\tIssuer: %s",
           szBuffer);
    X509_NAME_oneline(X509_get_subject_name(cert), szBuffer, 255);
    printf("\n\tSubject: %s",
           szBuffer);
  }

  return status;
}

CSRESULT
  CFS_PRV_SecureLoadTrustStore
    (CFSENV* pEnv) {

  char szFile[129];
  char szDir[129];

  int useConf;

  CFSRPS_PARAMINFO cfscpi;

  useConf = 0;
  if (CS_SUCCEED(CFSRPS_LookupParam(pEnv->TlsCfg_Env,
                            "TLS_VALIDATE_PEER_DEPTH", &(cfscpi)))) {
    useConf = 1;
    SSL_CTX_set_verify_depth(pEnv->ctx, atoi(cfscpi.szValue));
  }
  
  if (CS_SUCCEED(CFSRPS_LookupParam(pEnv->TlsCfg_Env,
                              "TLS_CERT_STORE_FILE", &(cfscpi)))) {
    useConf = 1;
    strcpy(szFile, cfscpi.szValue); 
  }
  else {
    szFile[0] = 0;
  }

  if (CS_SUCCEED(CFSRPS_LookupParam(pEnv->TlsCfg_Env,
                              "TLS_CERT_STORE_DIR", &(cfscpi)))) {
    strcpy(szDir, cfscpi.szValue); 
  }
  else {
    szDir[0] = 0;
  }

  if (useConf) {
    SSL_CTX_load_verify_locations(pEnv->ctx,
                                  szFile[0] == 0 ? NULL: szFile,
                                  szDir[0] == 0 ? NULL: szDir);
  }
  else {
    SSL_CTX_set_default_verify_paths(pEnv->ctx);
  }

  return CS_SUCCESS;
}