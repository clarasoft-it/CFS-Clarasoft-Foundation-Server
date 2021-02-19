/* ===========================================================================
  Clarasoft Foundation Server 400
  cshttp.h

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

---------------------------------------------------------------------------
  Change history
---------------------------------------------------------------------------

  2017-02-18
  Frederic Soucie
  Creation

  2020-09-21
  Frederic Soucie
  Added global/local configuration support

  2020-11-05
  Frederic Soucie
  Added automatic internal data conversion if Content-Type
  is text (taking into account chaset if any).

=========================================================================== */

#ifndef __CLARASOFT_CSHTTP_H__
#define __CLARASOFT_CSHTTP_H__

#include "qcsrc/cfsapi.h"

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

#define CSHTTP_SENDMODE_DEFAULT   (0)
#define CSHTTP_SENDMODE_PIPELINE  (1)

#define CSHTTP_DATAFMT            (0x0A010000)
#define CSHTTP_DATA_ENCODED       (0x0000A001)

#define CSHTTP_MAX_RESPONSE_HEADERS (103)

typedef void* CSHTTP;

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

/* --------------------------------------------------------

Use the following in your source if you want to print
the HTTP header names; each string aligns wiht the
CSHTTP_HEADERS_ID enumeration

char* CSHTTP_HeaderNamesTable[] =
{
  "A-IM",
  "Accept",
  "Accept-Charset",
  "Accept-Datetime",
  "Accept-Encoding",
  "Accept-Language",
  "Accept-Patch",
  "Accept-Ranges",
  "Access-Control-Allow-Credentials",
  "Access-Control-Allow-Headers",
  "Access-Control-Allow-Methods",
  "Access-Control-Allow-Origin",
  "Access-Control-Expose-Headers",
  "Access-Control-Max-Age",
  "Access-Control-Request-Method",
  "Access-Control-Request-Headers",
  "Age",
  "Allow",
  "Alt-Svc",
  "Authorization",
  "Cache-Control",
  "Connection",
  "Content-Disposition",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-MD5",
  "Content-Range",
  "Content-Security-Policy",
  "Content-Type",
  "Cookie",
  "Date",
  "Delta-Base",
  "DNT",
  "ETag",
  "Expect",
  "Expires",
  "Forwarded",
  "From",
  "Front-End-Https",
  "Host",
  "HTTP2-Settings",
  "If-Match",
  "If-Modified-Since",
  "If-None-Match",
  "If-Range",
  "If-Unmodified-Since",
  "IM",
  "Last-Modified",
  "Link",
  "Location",
  "Max-Forwards",
  "Origin",
  "P3P",
  "Pragma",
  "Proxy-Authenticate",
  "Proxy-Authorization",
  "Proxy-Connection",
  "Public-Key-Pins",
  "Range",
  "Referer",
  "Refresh",
  "Retry-After",
  "Save-Data",
  "Server",
  "Set-Cookie",
  "Status",
  "Strict-Transport-Security",
  "TE",
  "Timing-Allow-Origin",
  "Tk",
  "Trailer",
  "Transfer-Encoding",
  "User-Agent",
  "Upgrade",
  "Upgrade-Insecure-Requests",
  "Vary",
  "Via",
  "Warning",
  "WWW-Authenticate",
  "X-ATT-DeviceId",
  "X-Content-Duration",
  "X-Content-Security-Policy",
  "X-Content-Type-Options",
  "X-Correlation-ID",
  "X-Csrf-Token",
  "X-Forwarded-For",
  "X-Forwarded-Host",
  "X-Forwarded-Proto",
  "X-Frame-Options",
  "X-Http-Method-Override",
  "X-Powered-By",
  "X-Request-ID",
  "X-Requested-With",
  "X-UA-Compatible",
  "X-UIDH",
  "X-Wap-Profile",
  "X-WebKit-CSP",
  "X-XSS-Protection",
  "Sec-WebSocket-Key",
  "Sec-WebSocket-Protocol",
  "Sec-WebSocket-Version"
};

-------------------------------------------------------- */

typedef void* CSHTTP;

CSHTTP
  CSHTTP_Constructor
    (void);

void
  CSHTTP_Destructor
    (CSHTTP*);

CSRESULT
  CSHTTP_GetData
    (CSHTTP This,
     char* pBuffer);

CSRESULT
  CSHTTP_GetDataRef
    (CSHTTP This,
     char** pBuffer);

uint64_t
  CSHTTP_GetDataSize
    (CSHTTP);

CSRESULT
  CSHTTP_IBMI_GetCharSetCCSID
    (CSHTTP This,
     char* szCharset,
     char** szCCSID);

CSRESULT
  CSHTTP_GetMediaType
    (CSHTTP* This,
     char** pType,
     char** pSubType,
     char** pCharset);

char*
  CSHTTP_GetRequestMethod
    (CSHTTP This);

char*
  CSHTTP_GetRequestURI
    (CSHTTP This);

char*
  CSHTTP_GetRequestVersion
    (CSHTTP This);

char*
  CSHTTP_GetRespReason
    (CSHTTP This);

char*
  CSHTTP_GetRespStatus
    (CSHTTP This);

char*
  CSHTTP_GetStdHeader
    (CSHTTP This,
     int id);

CSRESULT
  CSHTTP_InsertData
    (CSHTTP This,
     void* pData,
     long size,
     long mode);

CSRESULT
  CSHTTP_RecvRequest
    (CSHTTP This,
     CFS_SESSION* Session);

CSRESULT
  CSHTTP_SendRequest
    (CSHTTP* This,
     CFS_SESSION* Session);

CSRESULT
  CSHTTP_SendResponse
    (CSHTTP This,
     CFS_SESSION* Session);

CSRESULT
  CSHTTP_SetAuthHeader
    (CSHTTP This,
     int Type,
     char* value);

CSRESULT
  CSHTTP_SetData
    (CSHTTP This,
     void* pData,
     long size);

CSRESULT
  CSHTTP_SetDataRef
     (CSHTTP This,
      void* pData,
      long size);

CSRESULT
  CSHTTP_SetExtHeader
    (CSHTTP This,
     char* szHeader,
     char* value);

CSRESULT
  CSHTTP_SetProxyAuthHeader
    (CSHTTP This,
     int type,
     char* value);

CSRESULT
  CSHTTP_SetSessionInterface
    (CSHTTP This,
     CFS_SESSION* Session);

CSRESULT
  CSHTTP_SetStdHeader
    (CSHTTP This,
     CSHTTP_HEADERS_ID header,
     char* value);

CSRESULT
  CSHTTP_StartRequest
    (CSHTTP This,
     int     method,
     int     httpVersion,
     char*   szURI);

CSRESULT
  CSHTTP_StartResponse
    (CSHTTP This,
      char*   szStatus,
      int     httpVersion,
      char*   httpReason);

#endif

