/* ===========================================================================
  Clarasoft Foundation Server 400
  cshttp.c

  Distributed under the MIT license

  Copyright (c) 2017 Clarasoft I.T. Solutions Inc.

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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <clarasoft/cfsapi.h>

#define CSHTTP_VER_1_0            (10)
#define CSHTTP_VER_1_1            (11)
#define CSHTTP_VER_2_0            (20)

#define CSHTTP_METHOD_GET         (1)
#define CSHTTP_METHOD_POST        (2)
#define CSHTTP_METHOD_PUT         (3)
#define CSHTTP_METHOD_HEAD        (4)
#define CSHTTP_METHOD_DELETE      (5)
#define CSHTTP_METHOD_PATCH       (6)
#define CSHTTP_METHOD_OPTIONS     (7)

#define CSHTTP_SEC_NONE           (0)
#define CSHTTP_SEC_TLS            (1)

#define CSHTTP_SENDMODE_DEFAULT   (0)
#define CSHTTP_SENDMODE_PIPELINE  (1)

#define CSHTTP_MORE_HEADERS       (-1)
#define CSHTTP_INVALID_HEADERS    (-2)
#define CSHTTP_INVALID_LINEBREAK  (-3)
#define CSHTTP_INVALID_SEQUENCE   (-4)

#define CSHTTP_DATAFMT            (0x0A010000)
#define CSHTTP_DATA_ENCODED       (0x0000A001)

#define HTTP_MAX_RESPONSE_HEADERS (103)

#define CSHTTP_DATA_LIST          (1)
#define CSHTTP_DATA_REF           (2)
#define CSHTTP_DATA_BUFFER        (3)

#define CSHTTP_DATASLAB_SIZE      (65535)
#define CSHTTP_HEADERSLAB_SIZE    (65535)
#define CSHTTP_URI_SIZE           (4096)

#define CSHTTP_CHUNKSTATE_SIZEREC 10
#define CSHTTP_CHUNKSTATE_DATAREC 20

#define CSHTTP_CHUNK_SIZEBYTES    11


char* httpStatusReasons[] = {
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "Continue", 
  "Switching Protocols", 
  "Processing", 
  "Early Hints", 
  "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  //////////////////////////////////////////////
  "OK", 
  "Created", 
  "Accepted", 
  "Non-Authoritative Information", 
  "No Content",
  "Reset Content", 
  "Partial Content", 
  "Multi-Status", 
  "Already Reported", 
  "", 
  "", "", "", "", "", "", "", "", "", "", 
  "", "", "", "", "", "",
  "IM Used", 
  "", "", "", 
  "", "", "", "", "", "", "", "", "", "", 
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  ///////////////////////////////////////////
  "Multiple Choices",
  "Moved Permanently",
  "Found",
  "See Other",
  "Not Modified",
  "Use Proxy",
  "Switch Proxy",
  "Temporary Redirect",
  "Permanent Redirect",
  "", 
  "", "", "", "", "", "", "", "", "", "", 
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
////////////////////////////////////////////
  "Bad Request",
  "Unauthorized",
  "Payment Required",
  "Forbidden",
  "Not Found",
  "Method Not Allowed",
  "Not Acceptable",
  "Proxy Authentication Required",
  "Request Timeout",
  "Conflict",
  "Gone",
  "Length Required",
  "Precondition Failed",
  "Payload Too Large",
  "RI Too Long",
  "Unsupported Media Type",
  "Range Not Satisfiable",
  "Expectation Failed",
  "I'm a teapot ",
  "Misdirected Request",
  "Unprocessable Entity",
  "Failed Dependency",
  "Too Early",
  "Upgrade Require",
  "Precondition Require",
  "Too Many Requests",
  "Request Header Fields Too Large",
  "Unavailable For Legal Reasons"
  "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "",
////////////////////////////////////////////
  "Internal Server Error",
  "Not Implemented",
  "Bad Gateway",
  "Service Unavailable",
  "Gateway Timeout",
  "HTTP Version Not Supported",
  "Variant Also Negotiates",
  "Insufficient Storage",
  "Loop Detected",
  "Not Extended",
  "Network Authentication Required"
};


typedef enum {
  CSHTTP_A_IM,
  CSHTTP_Accept,
  CSHTTP_Accept_Charset,
  CSHTTP_Accept_Datetime,
  CSHTTP_Accept_Encoding,
  CSHTTP_Accept_Language,
  CSHTTP_Accept_Patch,
  CSHTTP_Accept_Ranges,
  CSHTTP_Access_Control_Allow_Credentials,
  CSHTTP_Access_Control_Allow_Headers,
  CSHTTP_Access_Control_Allow_Methods,
  CSHTTP_Access_Control_Allow_Origin,
  CSHTTP_Access_Control_Expose_Headers,
  CSHTTP_Access_Control_Max_Age,
  CSHTTP_Access_Control_Request_Method,
  CSHTTP_Access_Control_Request_Headers,
  CSHTTP_Age,
  CSHTTP_Allow,
  CSHTTP_Alt_Svc,
  CSHTTP_Authorization,
  CSHTTP_Cache_Control,
  CSHTTP_Connection,
  CSHTTP_Content_Disposition,
  CSHTTP_Content_Encoding,
  CSHTTP_Content_Language,
  CSHTTP_Content_Length,
  CSHTTP_Content_Location,
  CSHTTP_Content_MD5,
  CSHTTP_Content_Range,
  CSHTTP_Content_Security_Policy,
  CSHTTP_Content_Type,
  CSHTTP_Cookie,
  CSHTTP_Date,
  CSHTTP_Delta_Base,
  CSHTTP_DNT,
  CSHTTP_ETag,
  CSHTTP_Expect,
  CSHTTP_Expires,
  CSHTTP_Forwarded,
  CSHTTP_From,
  CSHTTP_Front_End_Https,
  CSHTTP_Host,
  CSHTTP_HTTP2_Settings,
  CSHTTP_If_Match,
  CSHTTP_If_Modified_Since,
  CSHTTP_If_None_Match,
  CSHTTP_If_Range,
  CSHTTP_If_Unmodified_Since,
  CSHTTP_IM,
  CSHTTP_Last_Modified,
  CSHTTP_Link,
  CSHTTP_Location,
  CSHTTP_Max_Forwards,
  CSHTTP_Origin,
  CSHTTP_P3P,
  CSHTTP_Pragma,
  CSHTTP_Proxy_Authenticate,
  CSHTTP_Proxy_Authorization,
  CSHTTP_Proxy_Connection,
  CSHTTP_Public_Key_Pins,
  CSHTTP_Range,
  CSHTTP_Referer,
  CSHTTP_Refresh,
  CSHTTP_Retry_After,
  CSHTTP_Save_Data,
  CSHTTP_Server,
  CSHTTP_Set_Cookie,
  CSHTTP_Status,
  CSHTTP_Strict_Transport_Security,
  CSHTTP_TE,
  CSHTTP_Timing_Allow_Origin,
  CSHTTP_Tk,
  CSHTTP_Trailer,
  CSHTTP_Transfer_Encoding,
  CSHTTP_User_Agent,
  CSHTTP_Upgrade,
  CSHTTP_Upgrade_Insecure_Requests,
  CSHTTP_Vary,
  CSHTTP_Via,
  CSHTTP_Warning,
  CSHTTP_WWW_Authenticate,
  CSHTTP_X_ATT_DeviceId,
  CSHTTP_X_Content_Duration,
  CSHTTP_X_Content_Security_Policy,
  CSHTTP_X_Content_Type_Options,
  CSHTTP_X_Correlation_ID,
  CSHTTP_X_Csrf_Token,
  CSHTTP_X_Forwarded_For,
  CSHTTP_X_Forwarded_Host,
  CSHTTP_X_Forwarded_Proto,
  CSHTTP_X_Frame_Options,
  CSHTTP_X_Http_Method_Override,
  CSHTTP_X_Powered_By,
  CSHTTP_X_Request_ID,
  CSHTTP_X_Requested_With,
  CSHTTP_X_UA_Compatible,
  CSHTTP_X_UIDH,
  CSHTTP_X_Wap_Profile,
  CSHTTP_X_WebKit_CSP,
  CSHTTP_X_XSS_Protection,
  CSHTTP_Sec_WebSocket_Key,
  CSHTTP_Sec_WebSocket_Protocol,
  CSHTTP_Sec_WebSocket_Version

} CSHTTP_HEADERS_ID;

int CSHTTP_HeaderCaptionLen[HTTP_MAX_RESPONSE_HEADERS] = {
    6,  8, 16, 17, 17, 17, 14, 15, 34, 30,
   30, 29, 31, 24, 31, 32,  5,  7,  9, 15,
   15, 12, 21, 18, 18, 16, 18, 13, 15, 25,
   14,  8,  6, 12,  5,  6,  8,  9, 11,  6,
   17,  6, 16, 10, 19, 15, 10, 21,  4, 15,
    6, 10, 14,  9,  5,  8, 20, 21, 18, 17,
    7,  9,  9, 13, 11,  8, 12,  8, 27,  4,
   21,  4,  9, 19, 12,  9, 27,  6,  5,  9,
   18, 16, 20, 27, 24, 18, 14, 17, 18, 19,
   17, 24, 14, 14, 18, 17,  8, 15, 14, 18,
   17, 22, 21
};

char* CSHTTP_Methods[] = {
  "GET ",   
  "POST ",  
  "PUT ",   
  "HEAD ",  
  "DELETE ",
  "PATCH ",
  "OPTIONS "
};

long CSHTTP_MethodSizes[] = {
  4,
  5,
  4,
  5,
  7,
  6,
  8
};

char* CSHTTP_HeaderCaption[HTTP_MAX_RESPONSE_HEADERS] = {
  "A-IM: ",
  "Accept: ",
  "Accept-Charset: ",
  "Accept-Datetime: ",
  "Accept-Encoding: ",
  "Accept-Language: ",
  "Accept-Patch: ",
  "Accept-Ranges: ",
  "Access-Control-Allow-Credentials: ",
  "Access-Control-Allow-Headers: ",  //10
  "Access-Control-Allow-Methods: ",
  "Access-Control-Allow-Origin: ",
  "Access-Control-Expose-Headers: ",
  "Access-Control-Max-Age: ",
  "Access-Control-Request-Method: ",
  "Access-Control-Request-Headers: ",
  "Age: ",
  "Allow: ",
  "Alt-Svc: ",
  "Authorization: ", //20
  "Cache-Control: ",
  "Connection: ",
  "Content-Disposition: ",
  "Content-Encoding: ",
  "Content-Language: ",
  "Content-Length: ",
  "Content-Location: ",
  "Content-MD5: ",
  "Content-Range: ",
  "Content-Security-Policy: ", //30
  "Content-Type: ",
  "Cookie: ",
  "Date: ",
  "Delta-Base: ",
  "DNT: ", //35
  "ETag: ",
  "Expect: ",
  "Expires: ",
  "Forwarded: ",
  "From: ", //40
  "Front-End-Https: ",
  "Host: ",
  "HTTP2-Settings: ",
  "If-Match: ",
  "If-Modified-Since: ", //45
  "If-None-Match: ",
  "If-Range: ",
  "If-Unmodified-Since: ",
  "IM: ",
  "Last-Modified: ", //50
  "Link: ",
  "Location: ",
  "Max-Forwards: ",
  "Origin: ",
  "P3P: ", //55
  "Pragma: ",
  "Proxy-Authenticate: ",
  "Proxy-Authorization: ",
  "Proxy-Connection: ",
  "Public-Key-Pins: ", //60
  "Range: ",
  "Referer: ",
  "Refresh: ",
  "Retry-After: ",
  "Save-Data: ", //65
  "Server: ",
  "Set-Cookie: ",
  "Status: ",
  "Strict-Transport-Security: ",
  "TE: ", //70
  "Timing-Allow-Origin: ",
  "Tk: ",
  "Trailer: ",
  "Transfer-Encoding: ",
  "User-Agent: ", //75
  "Upgrade: ",
  "Upgrade-Insecure-Requests: ",
  "Vary: ",
  "Via: ",
  "Warning: ", //80
  "WWW-Authenticate: ",
  "X-ATT-DeviceId: ",
  "X-Content-Duration: ",
  "X-Content-Security-Policy: ",
  "X-Content-Type-Options: ", //85
  "X-Correlation-ID: ",
  "X-Csrf-Token: ",
  "X-Forwarded-For: ",
  "X-Forwarded-Host: ",
  "X-Forwarded-Proto: ", //90
  "X-Frame-Options: ",
  "X-Http-Method-Override: ",
  "X-Powered-By: ",
  "X-Request-ID: ",
  "X-Requested-With: ", //95
  "X-UA-Compatible: ",
  "X-UIDH: ",
  "X-Wap-Profile: ",
  "X-WebKit-CSP: ",
  "X-XSS-Protection: ",
  "Sec-WebSocket-Key: ",
  "Sec-WebSocket-Protocol: ",
  "Sec-WebSocket-Version: "
};

char* CSHTTP_HeaderCaptionRef[HTTP_MAX_RESPONSE_HEADERS] = {
  "A-IM",
  "Accept",
  "Accept-Charset",
  "Accept-Datetime",
  "Accept-Encoding",
  "Accept-Language",
  "Accept-Patch",
  "Accept-Ranges",
  "Access-Control-Allow-Credentials",
  "Access-Control-Allow-Headers",  //10
  "Access-Control-Allow-Methods",
  "Access-Control-Allow-Origin",
  "Access-Control-Expose-Headers",
  "Access-Control-Max-Age",
  "Access-Control-Request-Method",
  "Access-Control-Request-Headers",
  "Age",
  "Allow",
  "Alt-Svc",
  "Authorization", //20
  "Cache-Control",
  "Connection",
  "Content-Disposition",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-MD5",
  "Content-Range",
  "Content-Security-Policy", //30
  "Content-Type",
  "Cookie",
  "Date",
  "Delta-Base",
  "DNT", //35
  "ETag",
  "Expect",
  "Expires",
  "Forwarded",
  "From", //40
  "Front-End-Https",
  "Host",
  "HTTP2-Settings",
  "If-Match",
  "If-Modified-Since", //45
  "If-None-Match",
  "If-Range",
  "If-Unmodified-Since",
  "IM",
  "Last-Modified", //50
  "Link",
  "Location",
  "Max-Forwards",
  "Origin",
  "P3P", //55
  "Pragma",
  "Proxy-Authenticate",
  "Proxy-Authorization",
  "Proxy-Connection",
  "Public-Key-Pins", //60
  "Range",
  "Referer",
  "Refresh",
  "Retry-After",
  "Save-Data", //65
  "Server",
  "Set-Cookie",
  "Status",
  "Strict-Transport-Security",
  "TE", //70
  "Timing-Allow-Origin",
  "Tk",
  "Trailer",
  "Transfer-Encoding",
  "User-Agent", //75
  "Upgrade",
  "Upgrade-Insecure-Requests",
  "Vary",
  "Via",
  "Warning", //80
  "WWW-Authenticate",
  "X-ATT-DeviceId",
  "X-Content-Duration",
  "X-Content-Security-Policy",
  "X-Content-Type-Options", //85
  "X-Correlation-ID",
  "X-Csrf-Token",
  "X-Forwarded-For",
  "X-Forwarded-Host",
  "X-Forwarded-Proto", //90
  "X-Frame-Options",
  "X-Http-Method-Override",
  "X-Powered-By",
  "X-Request-ID",
  "X-Requested-With", //95
  "X-UA-Compatible",
  "X-UIDH",
  "X-Wap-Profile",
  "X-WebKit-CSP",
  "X-XSS-Protection",
  "Sec-WebSocket-Key",
  "Sec-WebSocket-Protocol",
  "Sec-WebSocket-Version"
};

typedef struct tagCSHTTP_FRAGMENT {
  char* data;
  long size;

} CSHTTP_FRAGMENT;

typedef struct tagCSHTTP {

  long headerIndices[HTTP_MAX_RESPONSE_HEADERS];
  long headerValuesIndices[HTTP_MAX_RESPONSE_HEADERS][2];

  int  Version;
  int  Status;
  int  Method;

  long headerSlabDataOffset;
  long headerSlabDataSize;
  long headerSlabCurOffset;
  long headerSlabSize;
  long URISize;
  long dataSlabSize;
  long dataSlabCurOffset;
  long InDataSize;
  long OutDataSize;

  char szStatus[4];

  char* headerSlab;
  char* dataSlab;
  char* pOutData;
  char* szURI;
  char* szHTTPVersion;
  char* szHTTPStatus;
  char* szHTTPReason;
  char* pMimeType;
  char* pMimeSubType;
  char* pCharsetType;

  CFS_SESSION* Session;
  CSLIST DataFragments;

} CSHTTP;

CSRESULT
  CSHTTP_PRV_processHeaders
    (CSHTTP* This);

long
  CSHTTP_PRV_parseHeaders
    (CSHTTP* This,
     long size,
     int* reset);

CSHTTP*
  CSHTTP_Constructor
    (void) {

  CSHTTP* Instance;

  Instance = (CSHTTP*)malloc(sizeof(CSHTTP));

  ////////////////////////////////////////////////////////////////////////////
  // Slabs and buffers
  ////////////////////////////////////////////////////////////////////////////

  Instance->headerSlabCurOffset = 0;
  Instance->headerSlabSize = CSHTTP_HEADERSLAB_SIZE;
  Instance->headerSlab =
             (char*)malloc(CSHTTP_HEADERSLAB_SIZE * sizeof(char));
  Instance->dataSlabSize = CSHTTP_DATASLAB_SIZE;

  // we allocate an extra byte for null-termination
  Instance->dataSlab =
             (char*)malloc(CSHTTP_DATASLAB_SIZE * sizeof(char) + 1);

  Instance->dataSlab[0] = 0;

  Instance->DataFragments = CSLIST_Constructor();

  return Instance;
}

CSRESULT
  CSHTTP_Destructor
    (CSHTTP** This) {

  if (*This != 0) {

    if ((*This)->dataSlab != 0) {
      free((*This)->dataSlab);
    }

    free((*This)->headerSlab);

    CSLIST_Destructor(&((*This)->DataFragments));

    free(*This);
    *This = 0;
  }

  return CS_SUCCESS;
}

CSRESULT
  CSHTTP_GetData
    (CSHTTP* This,
     char* pBuffer) {

  if (This->headerValuesIndices[CSHTTP_Content_Encoding][0] == 1) {
    // Return data as is with an indication that it must
    // be processed by the caller
    memcpy(pBuffer, This->dataSlab, This->InDataSize);
    return CS_SUCCESS | CSHTTP_DATAFMT | CSHTTP_DATA_ENCODED;
  }

  memcpy(pBuffer, This->dataSlab, This->InDataSize);
  return CS_SUCCESS;
}

char*
  CSHTTP_GetDataRef
    (CSHTTP* This) {

  return This->dataSlab;
}

long
  CSHTTP_GetDataSize
    (CSHTTP* This) {

  return This->InDataSize;
}

CSRESULT
  CSHTTP_GetMediaType
    (CSHTTP* This,
     char** pType,
     char** pSubType,
     char** pCharset) {

  *pType    = This->pMimeType;
  *pSubType = This->pMimeSubType;
  *pCharset = This->pCharsetType;

  return CS_SUCCESS;
}

char*
  CSHTTP_GetRequestMethod
    (CSHTTP* This) {

  return This->headerSlab;
}

char*
  CSHTTP_GetRequestVersion
    (CSHTTP* This) {

  return This->szHTTPVersion;
}

char*
  CSHTTP_GetRequestURI
    (CSHTTP* This) {

  return This->szURI;
}

char*
  CSHTTP_GetRespReason
    (CSHTTP* This) {

  return This->szHTTPReason;
}

char*
  CSHTTP_GetRespStatus
    (CSHTTP* This) {

  return This->szHTTPStatus;
}

char*
  CSHTTP_GetRespVersion
    (CSHTTP* This) {

  return This->headerSlab;
}

char*
  CSHTTP_GetStdHeader
    (CSHTTP* This, int id) {

  int i;

  if (This->headerValuesIndices[id][0] == 1) {
    i=0;
    while(This->headerSlab[This->headerValuesIndices[id][1] + i] == ' ') {
      i++;
    }

    return &(This->headerSlab[This->headerValuesIndices[id][1] + i]);
  }

  return 0;
}

CSRESULT
  CSHTTP_RecvRequest
    (CSHTTP* This,
     CFS_SESSION* Session) {

  CSRESULT hResult;

  long size;

  int reset;

  long i;
  long offset;
  long TotalReadSize;
  long partialDataSize;

  memset(This->headerIndices, 0, sizeof(This->headerIndices));
  memset(This->headerValuesIndices, 0, sizeof(This->headerValuesIndices));

  This->Method = 0;
  This->Status = -1;
  This->headerSlabDataOffset = -1;
  This->Version = 0;
  This->InDataSize = 0;
  This->OutDataSize = 0;
  This->headerSlabCurOffset = 0;
  This->dataSlabCurOffset=0;
  This->pOutData = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Read Server response until we have all headers
  /////////////////////////////////////////////////////////////////////////////

  TotalReadSize = 0;
  offset = 0;
  size = This->headerSlabSize;
  reset = 1;

  do {

    hResult = Session->lpVtbl->CFS_Receive(Session,
                          This->headerSlab + offset,
                          &size);

    if (CS_SUCCEED(hResult)) {

      TotalReadSize += (long)size;
      This->headerSlabDataOffset =
                CSHTTP_PRV_parseHeaders(This, (long)size, &reset);

      if (This->headerSlabDataOffset == CSHTTP_MORE_HEADERS) {
        offset += (long)size;
        size = This->headerSlabSize - offset;

        if (size <= 0) {
          // This means the response is larger than the response slab
          This->InDataSize    = 0;
          This->szURI = 0;
          This->szHTTPVersion = 0;
          This->headerSlab[0] = 0;
          This->dataSlab[0] = 0;
          return CS_FAILURE;
        }
      }
      else {
        break;
      }
    }
    else {
      This->InDataSize    = 0;
      This->szURI = 0;
      This->szHTTPVersion = 0;
      This->headerSlab[0] = 0;
      This->dataSlab[0] = 0;
      return CS_FAILURE;
    }

  } while(1);

  // At this point, we either have an error
  // or we are positioned in the data segment

  if (This->headerSlabDataOffset > 0) {

    // Parse the request line

    // Extract HTTP request method
    i=0;
    while (This->headerSlab[i] != ' ' && This->headerSlab[i] != 0) {
      i++;
    }

    if (This->headerSlab[i] != 0) {

      This->headerSlab[i] = 0;

      // Scan URI
      i++;
      This->szURI = &(This->headerSlab[i]);
      while (This->headerSlab[i] != ' ' && This->headerSlab[i] != 0) {
        i++;
      }

      if (This->headerSlab[i] != 0) {
        This->headerSlab[i] = 0;

        // HTTP Version follows
        i++;
        This->szHTTPVersion = &(This->headerSlab[i]);
      }
    }
    else {
      This->InDataSize    = 0;
      This->szURI = 0;
      This->szHTTPVersion = 0;
      This->headerSlab[0] = 0;
      This->dataSlab[0] = 0;
      return CS_FAILURE;
    }

    //////////////////////////////////////////////////////////////////////////
    //
    // Read the rest of the data: for this, we need to examine two headers:
    //
    // 1) Content-Length
    //
    //     If this header is found, then we know how many bytes to read from
    //     the data block.
    //
    // 2) Transfer-Encoding
    //
    //     If this header is present, then we need to process reading
    //     the data in successive reads.
    //
    //////////////////////////////////////////////////////////////////////////

    partialDataSize = TotalReadSize - This->headerSlabDataOffset;

    if (This->headerValuesIndices[CSHTTP_Content_Length][0] == 1) {

      // Get size of input data from the server
      This->InDataSize =
          atoll(&(This->headerSlab[This->headerValuesIndices[
                                  CSHTTP_Content_Length][1]]));

      // we will use the data slab to hold the server data

      if (This->InDataSize > This->dataSlabSize) {

        // we need a larger data slab
        free(This->dataSlab);
        This->dataSlabSize = This->InDataSize;
        This->dataSlab =
              (char*)malloc((This->dataSlabSize * sizeof(char)) + 1);
      }

      // copy data portion from header slab to data slab ...
      // data must be taken from the original input slab.

      if (partialDataSize > 0) {
        memcpy(This->dataSlab,
               &(This->headerSlab[This->headerSlabDataOffset]),
               TotalReadSize - This->headerSlabDataOffset);
      }

      // We now read the rest of the data (if any)
      size = This->InDataSize - partialDataSize;

      if (size > 0) {
        if (CS_FAIL(Session->lpVtbl->CFS_ReceiveRecord(Session,
                                      This->dataSlab + partialDataSize,
                                      &size))) {
          This->InDataSize    = 0;
          This->szURI = 0;
          This->szHTTPVersion = 0;
          This->headerSlab[0] = 0;
          This->dataSlab[0] = 0;
          return CS_FAILURE;
        }
      }
    }
    else {

      if (This->headerValuesIndices[CSHTTP_Transfer_Encoding][0] == 1) {
        //CSHTTP_PRV_ReadChunkedData(This);
      }
      else {

        This->InDataSize = 0;
        This->dataSlab[0] = 0;
      }
    }

    // Aplpy conversion if there is a content type with a charset value or
    // if content type is TEXT with no charset.
    // Check for content-encoding first; if it is present, then we will not
    // convert it; we leave it to the data consumer (caller).
    //
    // For example, if the Content-Type is application/json

    if (This->headerValuesIndices[CSHTTP_Content_Encoding][0] == 0) {

      // get content-type header

      if (This->headerValuesIndices[CSHTTP_Content_Type][0] == 1) {

        // check for charset;
        // if there is a charset, then apply it, else,
        // check if media type is text, then apply
        // US-ASCII translation (CCSID == 819)

        i=0;
        while(This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] == ' ') {
          i++;
        }

        // We are at the MIME type; next, we move to the slash
        This->pMimeType = &(This->headerSlab[This->headerValuesIndices[
                            CSHTTP_Content_Type][1] + i]);

        while(This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != '/' &&
              This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != 0) {
          // convert to lowercase
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i]
             = tolower(This->headerSlab[This->headerValuesIndices[
                                        CSHTTP_Content_Type][1] + i]);
          i++;
        }

        // We have the MIME type, null terminate it and get charset if any
        This->headerSlab[This->headerValuesIndices[
                         CSHTTP_Content_Type][1] + i] = 0;
        i++;

        // MIME subtype should begin here
        This->pMimeSubType = &(This->headerSlab[This->headerValuesIndices[
                                                CSHTTP_Content_Type][1] + i]);

        // try to reach the charset
        while(This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != ';' &&
              This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != 0) {
          // convert to lowercase
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i]
             = tolower(This->headerSlab[This->headerValuesIndices[
                                        CSHTTP_Content_Type][1] + i]);
          i++;
        }

        if (This->headerSlab[This->headerValuesIndices[
                             CSHTTP_Content_Type][1] + i] == ';' ) {

          // null-terminate MIME subtype
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i] = 0;
          i++;

          // we assume we have a charset, skip over whitespace
          while(This->headerSlab[This->headerValuesIndices[
                                 CSHTTP_Content_Type][1] + i] == ' ') {
            i++;
          }

          // reach charset value
          while(This->headerSlab[This->headerValuesIndices[
                                 CSHTTP_Content_Type][1] + i] != '=' &&
                This->headerSlab[This->headerValuesIndices[
                                 CSHTTP_Content_Type][1] + i] != 0) {
            i++;
          }

          if (This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] == '=' ) {

            i++;
            // we have reached the charset value; get to the end of it
            This->pCharsetType = &(This->headerSlab[
                                   This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i]);

            while(This->headerSlab[This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i] != ' ' &&
                  This->headerSlab[This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i] != ';' &&
                  This->headerSlab[This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i] != 0) {

              // convert to uppercase
              This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i]
                    = toupper(This->headerSlab[This->headerValuesIndices[
                                               CSHTTP_Content_Type][1] + i]);
              i++;
            }

            // we should have the charset, null-terminate it
            This->headerSlab[This->headerValuesIndices[
                             CSHTTP_Content_Type][1] + i] = 0;
          }
        }
        else {

          // null-terminate MIME subtype
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i] = 0;
          This->pMimeSubType = &(This->headerSlab[This->headerValuesIndices[
                                          CSHTTP_Content_Type][1] + i]);
          This->pCharsetType = 0;
        }
      }
    }
    else {

      This->InDataSize    = 0;
      This->pMimeType     = 0;
      This->pMimeSubType  = 0;
      This->pCharsetType  = 0;
    }

    // NULL-terminate data; slab holds an extra byte for NULL
    This->dataSlab[This->InDataSize] = 0;
  }

  return CS_SUCCESS;
}

CSRESULT
  CSHTTP_SendRequest
    (CSHTTP* This,
     CFS_SESSION* Session,
     int mode) {

  CSRESULT hResult;

  CSHTTP_FRAGMENT* pFragment;

  long size;

  int reset;

  long i;
  long offset;
  long count;
  long TotalReadSize;
  long partialDataSize;

  // Chunk stream variables
  int ChunkState;
  int Done;
  int StateChange;

  long Tail;
  long ChunkSize;
  long FragmentSize;
  long CurChunkPos;
  long CurChunkSizePos;
  long ChunkStartOffset;
  long CurFragmentPos;

  long DataSize;

  char* ChunkFragment;

  char szChunkSize[CSHTTP_CHUNK_SIZEBYTES];

  ///////////////////////////////////////////////////////////////////////
  // Reset data slab
  ///////////////////////////////////////////////////////////////////////

  This->dataSlab[0] = 0;

  ///////////////////////////////////////////////////////////////////////
  // Insert blank line
  ///////////////////////////////////////////////////////////////////////

  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), "\r\n", 2);

  ///////////////////////////////////////////////////////////////////////
  // Send Headers
  ///////////////////////////////////////////////////////////////////////

  size = This->headerSlabCurOffset+2; // plus empty line ...
                                      // don't include NULL

  if (CS_SUCCEED(Session->lpVtbl->CFS_SendRecord(Session,
                                This->headerSlab,
                                &size))) {

    ///////////////////////////////////////////////////////////////////////
    // Send Data
    ///////////////////////////////////////////////////////////////////////

    if (This->pOutData != 0) {

      size = This->OutDataSize;

      if (CS_FAIL(Session->lpVtbl->CFS_SendRecord(Session,
                                 This->pOutData,
                                 &size))) {
        This->InDataSize    = 0;
        This->szHTTPStatus  = 0;
        This->szHTTPReason  = 0;
        This->pMimeType     = 0;
        This->pMimeSubType  = 0;
        This->pCharsetType  = 0;
        This->headerSlab[0] = 0;
        This->dataSlab[0]   = 0;
        return CS_FAILURE;
      }
    }
    else {

      count = CSLIST_Count(This->DataFragments);

      if (count > 0) {

        for (i=0; i<count; i++) {

          CSLIST_GetDataRef(This->DataFragments, (void**)&pFragment, i);
          size = pFragment->size;

          if (CS_FAIL(Session->lpVtbl->CFS_SendRecord(Session,
                                     pFragment->data,
                                     &size))) {
            This->InDataSize    = 0;
            This->szHTTPStatus  = 0;
            This->szHTTPReason  = 0;
            This->pMimeType     = 0;
            This->pMimeSubType  = 0;
            This->pCharsetType  = 0;
            This->headerSlab[0] = 0;
            This->dataSlab[0]   = 0;
            return CS_FAILURE;
          }
        }
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Read Server response until we have all headers
    ///////////////////////////////////////////////////////////////////////////

    TotalReadSize = 0;
    offset = 0;
    size = This->headerSlabSize;
    reset = 1;

    do {

      hResult = Session->lpVtbl->CFS_Receive(Session,
                            This->headerSlab + offset,
                            &size);

      if (CS_SUCCEED(hResult)) {

        TotalReadSize += (long)size;
        This->headerSlabDataOffset =
                 CSHTTP_PRV_parseHeaders(This, (long)size, &reset);

        if (This->headerSlabDataOffset == CSHTTP_MORE_HEADERS) {
          offset += (long)size;
          size = This->headerSlabSize - offset;

          if (size <= 0) {
            // This means the response is larger than the response slab
            This->InDataSize    = 0;
            This->szHTTPStatus  = 0;
            This->szHTTPReason  = 0;
            This->pMimeType     = 0;
            This->pMimeSubType  = 0;
            This->pCharsetType  = 0;
            This->headerSlab[0] = 0;
            This->dataSlab[0]   = 0;
            return CS_FAILURE;
          }
        }
        else {
          break;
        }
      }
      else {
        This->InDataSize    = 0;
        This->szHTTPStatus  = 0;
        This->szHTTPReason  = 0;
        This->pMimeType     = 0;
        This->pMimeSubType  = 0;
        This->pCharsetType  = 0;
        This->headerSlab[0] = 0;
        This->dataSlab[0]   = 0;
        return CS_FAILURE;
      }

    } while(1);

  }
  else {
    This->InDataSize    = 0;
    This->szHTTPStatus  = 0;
    This->szHTTPReason  = 0;
    This->pMimeType     = 0;
    This->pMimeSubType  = 0;
    This->pCharsetType  = 0;
    This->headerSlab[0] = 0;
    This->dataSlab[0]   = 0;
    return CS_FAILURE;
  }

  // At this point, we either have an error
  // or we are positioned in the data segment

  if (This->headerSlabDataOffset > 0) {

    This->szHTTPVersion = This->headerSlab;

    // Parse response status; first skip over HTTP version
    i=0;
    while (This->headerSlab[i] != ' ' && This->headerSlab[i] != 0) {
      i++;
    }

    if (This->headerSlab[i] != 0) {

      // NULL-terminate HTTP version
      This->headerSlab[i] = 0;

      // HTTP status code
      i++;
      This->szHTTPStatus = &(This->headerSlab[i]);

      // Status code is 3 characters
      This->headerSlab[i+3] = 0;

      // Convert to numeric
      //This->Status = atoi(This->szHTTPStatus);

      This->szHTTPReason = &(This->headerSlab[i+4]);
    }
    else {
      This->InDataSize    = 0;
      This->szHTTPStatus  = 0;
      This->szHTTPReason  = 0;
      This->pMimeType     = 0;
      This->pMimeSubType  = 0;
      This->pCharsetType  = 0;
      This->headerSlab[0] = 0;
      This->dataSlab[0]   = 0;
      return CS_FAILURE;
    }

    ///////////////////////////////////////////////////////////////////////////
    //
    // Read the rest of the data: for this, we need to examine two headers:
    //
    // 1) Content-Length
    //
    //     If this header is found, then we know how many bytes to read from
    //     the data block.
    //
    // 2) Transfer-Encoding
    //
    //     If this header is present, then we need
    //     to process reading the data in successive reads.
    //
    ///////////////////////////////////////////////////////////////////////////

    partialDataSize = TotalReadSize - This->headerSlabDataOffset;

    if (This->headerValuesIndices[CSHTTP_Content_Length][0] == 1) {

      // Get size of input data from the server
      This->InDataSize =
          atoll(&(This->headerSlab[This->headerValuesIndices[
                                  CSHTTP_Content_Length][1]]));

      // we will use the data slab to hold the server data;

      if (This->InDataSize > This->dataSlabSize) {

        // we need a larger data slab
        free(This->dataSlab);
        This->dataSlabSize = This->InDataSize;
        // we allocate an extra byte for null-termination
        This->dataSlab =
            (char*)malloc((This->dataSlabSize * sizeof(char)) + 1);
      }

      // copy data portion from header slab to data slab ...
      // data must be taken from the original input slab.

      if (partialDataSize > 0) {
        memcpy(This->dataSlab,
               &(This->headerSlab[This->headerSlabDataOffset]),
               TotalReadSize - This->headerSlabDataOffset);
      }

      offset = partialDataSize;

      // We now read the rest of the data (if any)
      size = This->InDataSize - partialDataSize;

      if (size > 0) {

        if (CS_FAIL(hResult = Session->lpVtbl->CFS_ReceiveRecord
                                      (Session,
                                       This->dataSlab + partialDataSize,
                                       &size))) {

          This->InDataSize    = 0;
          This->szHTTPStatus  = 0;
          This->szHTTPReason  = 0;
          This->pMimeType     = 0;
          This->pMimeSubType  = 0;
          This->pCharsetType  = 0;
          This->headerSlab[0] = 0;
          This->dataSlab[0]   = 0;
          return CS_FAILURE;
        }
      }
    }
    else {

      if (This->headerValuesIndices[CSHTTP_Transfer_Encoding][0] == 1) {

        // We assume we have a chunked encoding...
        // We will work from the header slab and copy the data over
        // to the data Slab.

        ChunkState       = CSHTTP_CHUNKSTATE_SIZEREC;
        CurChunkPos      = This->headerSlabDataOffset;
        CurChunkSizePos  = 0;
        CurFragmentPos   = 0;
        ChunkStartOffset = 0;
        Done             = 0;
        This->InDataSize = 0;
        ChunkStartOffset = 0;

        CSLIST_Clear(This->DataFragments);

        // Here, we need to scan from the header slab data offset
        // to the end of the read data we already have in the header slab,
        // we set the Data size such that we will scan from the data
        // offset to the end of the data within the header slab. On
        // subsequent iterations, the Data size will actually be the
        // size read from the stream.

        DataSize = TotalReadSize;

        while (1) {

          while ((CurChunkPos < (long)DataSize) && !Done) {
          //while ((CurChunkPos<=(long)DataSize) && !Done) {

            StateChange = 0;

            switch(ChunkState) {

              case CSHTTP_CHUNKSTATE_SIZEREC:

                while ((CurChunkPos < (long)DataSize) && !StateChange) {

                  switch(This->headerSlab[CurChunkPos]) {

                    case '\r': // ASCII Carriage Return

                      // NULL-terminate HEX size
                      szChunkSize[CurChunkSizePos] = 0;

                      // Compute chunk size
                      ChunkSize = strtol(szChunkSize, 0, 16);

                      if (ChunkSize != 0) {

                        This->InDataSize += ChunkSize;

                        // reset chunk size buffer offset
                        CurChunkSizePos = 0;

                        // Allocate chunk and reset offset
                        FragmentSize = ChunkSize;
                        ChunkFragment =
                             (char*)malloc(FragmentSize * sizeof(char));
                        CurFragmentPos = 0;
                      }
                      else {

                        Done = 1;  // this means there is not more
                                   // data in the stream
                      }

                      StateChange = 1;
                      ChunkState = CSHTTP_CHUNKSTATE_DATAREC;

                      break;

                    default:

                      szChunkSize[CurChunkSizePos] =
                                 This->headerSlab[CurChunkPos];

                      CurChunkPos++;
                      CurChunkSizePos++;

                      break;
                  }
                }

                if (!Done) {

                  if (StateChange) {

                    // We are at the CR. We must determine if we can
                    // just skip over the LF or jump over to the next
                    // stream buffer.

                    Tail = (long)DataSize - (CurChunkPos + 1);

                    switch(Tail) {

                      case 0:
                        // we are at the last character and the data stream
                        // must be read. The next data stream buffer will
                        // start with a LF; we must skip it.
                        CurChunkPos = (long)DataSize; // break out of the loop
                        ChunkStartOffset = 1;
                        break;
                      case 1:
                        // we just have the LF and must read the data stream.
                        // The next stream data will start the data record.
                        CurChunkPos = (long)DataSize; // break out of the loop
                        ChunkStartOffset = 0;
                        break;
                      default:
                        // we have at least one character in the data record
                        // and we need only skip over the LF.
                        CurChunkPos += 2;
                        break;
                    }
                  }
                  else {
                    ChunkStartOffset = 0;
                  }
                }

                break;

              case CSHTTP_CHUNKSTATE_DATAREC:

                while ((CurChunkPos < (long)DataSize) && (ChunkSize > 0)) {

                  ChunkFragment[CurFragmentPos] =
                            This->headerSlab[CurChunkPos];

                  ChunkSize--;
                  CurChunkPos++;
                  CurFragmentPos++;
                }

                if (ChunkSize == 0) {

                  // We have read the whole chunk; insert it in the
                  // chunk list

                  CSLIST_Insert(This->DataFragments,
                                ChunkFragment, FragmentSize, CSLIST_BOTTOM);

                  // the next round will read a chunk size record
                  ChunkState = CSHTTP_CHUNKSTATE_SIZEREC;

                  // we must determine
                  // how far we are from the nex size record.
                  Tail = (long)DataSize - CurChunkPos;

                  switch(Tail) {
                    case 0:
                      // This means the stream data ends exactly at the end
                      // of the chunk; we need to read more from the stream
                      // and the next stream buffer will start with CRLF so
                      // we want to skip those reading the next stream buffer.
                      CurChunkPos = (long)DataSize;  // break out of the loop
                      ChunkStartOffset = 2;
                      // The next round will read a chunk size record
                      break;
                    case 1:
                      // This means the stream data ends exactly at the CR
                      // following the chunk; we need to read more from the
                      // stream and the next read will start with LF so we
                      // want to skip it reading the next stream buffer.
                      CurChunkPos = (long)DataSize;  //break out of the loop
                      ChunkStartOffset = 1;
                      break;
                    case 2:
                      // This means the stream data ends exactly with CRLF
                      // following the chunk; we need to read more from the
                      // stream and the next read will start at the beginning
                      // of the next stream buffer.
                      CurChunkPos = (long)DataSize;  // break out of the loop
                      ChunkStartOffset = 0;
                      break;
                    default:
                      // We are positioned at a CR and there is something
                      // beyond the following linefeed, so we just skip over.
                      // the CRLF.
                      CurChunkPos += 2;
                      break;
                  }
                }
                else {

                  // We have not read the whole data record and a
                  // new strem buffer will be read; we start copying
                  // at the first byte.
                  ChunkStartOffset = 0;
                }

                break;

            }
          }

          if (!Done) {

            // Read more data from the stream

            DataSize = This->headerSlabSize;

            if (CS_FAIL(Session->lpVtbl->CFS_Receive(Session,
                                    This->headerSlab,
                                    &DataSize))) {
              This->InDataSize    = 0;
              This->szHTTPStatus  = 0;
              This->szHTTPReason  = 0;
              This->pMimeType     = 0;
              This->pMimeSubType  = 0;
              This->pCharsetType  = 0;
              This->headerSlab[0] = 0;
              This->dataSlab[0]   = 0;
              return CS_FAILURE;
            }

            if (DataSize == 0) {
              // There should have been more data
              This->InDataSize    = 0;
              This->szHTTPStatus  = 0;
              This->szHTTPReason  = 0;
              This->pMimeType     = 0;
              This->pMimeSubType  = 0;
              This->pCharsetType  = 0;
              This->headerSlab[0] = 0;
              This->dataSlab[0]   = 0;
              return CS_FAILURE;
            }

            CurChunkPos = ChunkStartOffset;
          }
          else {

            // Allocate data slab and copy fragments;

            if (This->dataSlabSize <= This->InDataSize) {
              free(This->dataSlab);
              This->dataSlabSize = This->InDataSize;

              // Allocate an extra byte for NULL-termination
              This->dataSlab =
                   (char*)malloc(This->dataSlabSize * sizeof(char) + 1);
            }

            //Copy chunk fragments to data slab
            This->dataSlabCurOffset = 0;
            count = CSLIST_Count(This->DataFragments);
            for (i=0; i<count; i++) {
              size = CSLIST_ItemSize(This->DataFragments, i);
              CSLIST_GetDataRef(This->DataFragments, (void**)&pFragment, i);
              memcpy(This->dataSlab + This->dataSlabCurOffset,
                     pFragment, size);
              This->dataSlabCurOffset += (long)size;
            }

            break;
          }
        }
      }
      else {

        // Cannot know how much data to read. We stop and return what is read
        This->InDataSize = partialDataSize;

        // Get size of input data from the server
        This->InDataSize =
            atoi(&(This->headerSlab[This->headerValuesIndices[
                                    CSHTTP_Content_Length][1]]));

        // we will use the data slab to hold the server data;

        if (This->InDataSize > This->dataSlabSize) {

          // we need a larger data slab
          free(This->dataSlab);
          This->dataSlabSize = This->InDataSize;
          // we allocate an extra byte for null-termination
          This->dataSlab =
              (char*)malloc((This->dataSlabSize * sizeof(char)) + 1);
        }

        // copy data portion from header slab to data slab ...
        // data must be taken from the original input slab.

        if (partialDataSize > 0) {
          memcpy(This->dataSlab,
                 &(This->headerSlab[This->headerSlabDataOffset]),
                 TotalReadSize - This->headerSlabDataOffset);
        }
      }
    }

    if (This->headerValuesIndices[CSHTTP_Content_Encoding][0] == 0) {

      // get content-type header

      if (This->headerValuesIndices[CSHTTP_Content_Type][0] == 1) {

        // check for charset;
        // if there is a charset, then apply it, else,
        // check if media type is text, then apply
        // US-ASCII translation (CCSID == 819)

        i=0;
        while(This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] == ' ') {
          i++;
        }

        // We are at the MIME type; next, we move to the slash
        This->pMimeType = &(This->headerSlab[This->headerValuesIndices[
                                             CSHTTP_Content_Type][1] + i]);

        while(This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != '/' &&
              This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != 0) {
          // convert to lowercase
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i] =
             tolower(This->headerSlab[This->headerValuesIndices[
                                        CSHTTP_Content_Type][1] + i]);
          i++;
        }

        // We have the MIME type, null terminate it and get charset if any
        This->headerSlab[This->headerValuesIndices[
                         CSHTTP_Content_Type][1] + i] = 0;
        i++;

        // MIME subtype should begin here
        This->pMimeSubType = &(This->headerSlab[This->headerValuesIndices[
                                                CSHTTP_Content_Type][1] + i]);

        // try to reach the charset
        while(This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != ';' &&
              This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] != 0) {
          // convert to lowercase
          This->headerSlab[This->headerValuesIndices[
                             CSHTTP_Content_Type][1] + i] =
              tolower(This->headerSlab[This->headerValuesIndices[
                                        CSHTTP_Content_Type][1] + i]);
          i++;
        }

        if (This->headerSlab[This->headerValuesIndices[
                             CSHTTP_Content_Type][1] + i] == ';' ) {

          // null-terminate MIME subtype
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i] = 0;
          i++;

          // we assume we have a charset, skip over whitespace
          while(This->headerSlab[This->headerValuesIndices[
                                 CSHTTP_Content_Type][1] + i] == ' ') {
            i++;
          }

          // reach charset value
          while(This->headerSlab[This->headerValuesIndices[
                                 CSHTTP_Content_Type][1] + i] != '=' &&
                This->headerSlab[This->headerValuesIndices[
                                 CSHTTP_Content_Type][1] + i] != 0) {
            i++;
          }

          if (This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] == '=' ) {

            i++;
            // we have reached the charset value; get to the end of it
            This->pCharsetType =
                        &(This->headerSlab[This->headerValuesIndices[
                                                CSHTTP_Content_Type][1] + i]);

            while(This->headerSlab[This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i] != ' ' &&
                  This->headerSlab[This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i] != ';' &&
                  This->headerSlab[This->headerValuesIndices[
                                   CSHTTP_Content_Type][1] + i] != 0) {

              // convert to uppercase
              This->headerSlab[This->headerValuesIndices[
                               CSHTTP_Content_Type][1] + i] =
                    toupper(This->headerSlab[This->headerValuesIndices[
                                             CSHTTP_Content_Type][1] + i]);
              i++;
            }

            // we should have the charset, null-terminate it
            This->headerSlab[This->headerValuesIndices[
                             CSHTTP_Content_Type][1] + i] = 0;

            // apply conversion

          }
        }
        else {

          // null-terminate MIME subtype
          This->headerSlab[This->headerValuesIndices[
                           CSHTTP_Content_Type][1] + i] = 0;
          This->pCharsetType = 0;

        }
      }
    }

    // NULL-terminate data slab; slab holds an extra byte for NULL
    This->dataSlab[This->InDataSize] = 0;
  }
  else {

    This->InDataSize    = 0;
    This->pMimeType     = 0;
    This->pMimeSubType  = 0;
    This->pCharsetType  = 0;
  }

  return CS_SUCCESS;
}

CSRESULT
  CSHTTP_SetData
    (CSHTTP* This,
     void* pData,
     long size) {

  long len;
  char szHeader[2049];

  if (This->dataSlabSize < size) {
    free(This->dataSlab);
    This->dataSlabSize = size;
    This->dataSlab = (char*)malloc(This->dataSlabSize * sizeof(char));
  }

  memcpy(This->dataSlab, pData, This->dataSlabSize);

  // Create Content-Length header

  sprintf(szHeader, "Content-Length: %ld\r\n", This->OutDataSize);
  len = strlen(szHeader);
  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), szHeader, len);
  This->headerSlabCurOffset += len;

  return CS_SUCCESS;
}

CSRESULT
  CSHTTP_SetDataRef
    (CSHTTP* This,
     void* pData,
     long size) {

  long len;
  char szHeader[2049];

  This->pOutData = pData;
  This->OutDataSize = size;

  // Create Content-Length header

  sprintf(szHeader, "Content-Length: %ld\r\n", This->OutDataSize);
  len = strlen(szHeader);
  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), szHeader, len);
  This->headerSlabCurOffset += len;

  return CS_SUCCESS;
}

CSRESULT
  CSHTTP_SetExtHeader
    (CSHTTP* This,
     char* header) {

  long len;

  len = strlen(header);

  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), header, len);
  This->headerSlabCurOffset += len;

  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), "\r\n", 2);
  This->headerSlabCurOffset += 2;

  return CS_SUCCESS;
}

CSRESULT
  CSHTTP_SetStdHeader
    (CSHTTP* This,
     CSHTTP_HEADERS_ID header,
     char* value) {

  long len;

  len = strlen(value);

  memcpy(&(This->headerSlab[This->headerSlabCurOffset]),
         CSHTTP_HeaderCaption[header%HTTP_MAX_RESPONSE_HEADERS],
         CSHTTP_HeaderCaptionLen[header%HTTP_MAX_RESPONSE_HEADERS]);

  This->headerSlabCurOffset +=
            CSHTTP_HeaderCaptionLen[header%HTTP_MAX_RESPONSE_HEADERS];

  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), value, len);
  This->headerSlabCurOffset += len;

  memcpy(&(This->headerSlab[This->headerSlabCurOffset]), "\r\n", 2);
  This->headerSlabCurOffset += 2;

  return CS_SUCCESS;
}

CSRESULT CSHTTP_StartRequest
  (CSHTTP* This,
   int     method,
   int     httpVersion,
   char*   szURI) {

  memset(This->headerIndices, 0, sizeof(This->headerIndices));
  memset(This->headerValuesIndices, 0, sizeof(This->headerValuesIndices));

  This->Method = method;
  This->Status = -1;
  This->headerSlabDataOffset = -1;
  This->Version = httpVersion;
  This->InDataSize = 0;
  This->OutDataSize = 0;
  This->headerSlabCurOffset = 0;
  This->dataSlabCurOffset=0;
  This->pOutData = 0;

  CSLIST_Clear(This->DataFragments);

  switch(This->Method) {

    case CSHTTP_METHOD_GET:
      strcpy(This->headerSlab, "GET ");
      This->headerSlabCurOffset += 4;
      break;

    case CSHTTP_METHOD_POST:
      strcpy(This->headerSlab, "POST ");
      This->headerSlabCurOffset += 5;
      break;

    case CSHTTP_METHOD_PUT:
      strcpy(This->headerSlab, "PUT ");
      This->headerSlabCurOffset += 4;
      break;

    case CSHTTP_METHOD_HEAD:
      strcpy(This->headerSlab, "HEAD ");
      This->headerSlabCurOffset += 5;
      break;

    case CSHTTP_METHOD_DELETE:
      strcpy(This->headerSlab, "DELETE ");
      This->headerSlabCurOffset += 7;
      break;

    case CSHTTP_METHOD_PATCH:
      strcpy(This->headerSlab, "PATCH ");
      This->headerSlabCurOffset += 6;
      break;

    case CSHTTP_METHOD_OPTIONS:
      strcpy(This->headerSlab, "OPTIONS ");
      This->headerSlabCurOffset += 8;
      break;
  }

  strcat(This->headerSlab, szURI);
  This->headerSlabCurOffset += strlen(szURI);

  switch(This->Version) {

    case CSHTTP_VER_1_1:
      strcat(This->headerSlab, " HTTP/1.1\r\n");
      This->headerSlabCurOffset += 11;
      break;

    default:
      strcat(This->headerSlab, " HTTP/1.0\r\n");
      This->headerSlabCurOffset += 11;
      break;
  }


  return CS_SUCCESS;
}

/* ==========================================================================
  Private Methods
========================================================================== */

long
  CSHTTP_PRV_parseHeaders
    (CSHTTP* This,
     long size,
     int* reset) {

  static long curPos    = 0;
  static long CR_Offset = 0;
  static long LF_Offset = 0;
  static long nextIndex = 0;
  static long WantColon = 1;

  long i;

  if (*reset != 0) {
    curPos    = 0;
    CR_Offset = 0;
    LF_Offset = 0;
    nextIndex = 0;
    WantColon = 1;
    *reset = 0;
  }

  for (i=0; i<size; i++, curPos++) {

    switch(This->headerSlab[curPos]) {

      case ':':

        // Only NULL-terminate if this is the colon immediately
        // following a header;
        // this is because some header values may include a colon and we must
        // avoid overwriting a colon that is part of a value

        if (WantColon) {
          // This is the start of a header value
          This->headerSlab[curPos] = 0;

          /////////////////////////////////////////////////////////////////////
          // we have a header value starting at i+1
          /////////////////////////////////////////////////////////////////////

          This->headerIndices[nextIndex] = curPos+1;
          nextIndex++;
          // Indicate we already NULL-terminated a colon for the current header
          WantColon = 0;
        }

        break;

      case '\r': 

        This->headerSlab[curPos] = 0;
        // Indicate we expect the next colon to
        // be the start of the next header's value
        WantColon = 1;
        CR_Offset = curPos;

        break;

      case '\n': 

        if ((curPos-CR_Offset) == 1) {

          // Check where previous LF was; we might have
          // reached the end of the headers

          if (curPos-LF_Offset == 2) {

            ////////////////////////////////////////////////////////////////////
            // we are at the end of the headers; the previous header
            // is therefore the empty string and we can set its
            // index to zero.
            ////////////////////////////////////////////////////////////////////

            This->headerIndices[nextIndex-1] = 0;

            ////////////////////////////////////////////////////////////////////
            // We will now parse the headers to get their value...
            ////////////////////////////////////////////////////////////////////

            if (CS_SUCCEED(CSHTTP_PRV_processHeaders(This))) {

              // we return the start index of the data section

              return (curPos + 1);
            }
            else {
              return CSHTTP_INVALID_HEADERS;
            }
          }
          else {

            ////////////////////////////////////////////////////////////////////
            // we have a valid line break; this means that the next
            // header will start at i+1;
            ////////////////////////////////////////////////////////////////////

            This->headerIndices[nextIndex] = curPos+1;
            LF_Offset = curPos;
            nextIndex++;
          }
        }
        else {
          // HTTP request is invalid
          return CSHTTP_INVALID_LINEBREAK;
        }

        break;

      default:

        break;
    }
  }

  // This indicates we have not reached the data section of the response
  return CSHTTP_MORE_HEADERS;
}

CSRESULT
  CSHTTP_PRV_processHeaders
    (CSHTTP* This) {

  char** pHeaders;
  pHeaders = &CSHTTP_HeaderCaptionRef[0];

  int i, j;

  i=0;
  while (This->headerIndices[i] > 0) {

    for (j=0; j<HTTP_MAX_RESPONSE_HEADERS; j++) {

      if (This->headerValuesIndices[j][0] == 0) {
        if (!strcmp(&(This->headerSlab)[This->headerIndices[i]], pHeaders[j])) {
          This->headerValuesIndices[j][0] = 1;
          This->headerValuesIndices[j][1] = This->headerIndices[i+1];
          break;
        }
      }
    }

    i += 2;  // Skip header value index
  }

  return CS_SUCCESS;
}

long
  CSCGI_ReadStdInput
    (char* pBuffer,
     long Size) {

  return fread(pBuffer, 1, Size, stdin);
}

long
  CSCGI_WriteStdOutput
    (char* pBuffer,
     long Size) {

  long cb;

  cb = fwrite(pBuffer, 1, Size, stdout);
  fflush(stdout);
  return cb;
}
