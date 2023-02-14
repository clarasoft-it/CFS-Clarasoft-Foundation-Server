/* ===========================================================================

  Clarasoft Foundation Server Repository - OS/400
  cfsrepo.h

  Distributed under the MIT license

  Copyright (c) 2020 Clarasoft I.T. Solutions Inc.

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

#ifndef __CLARASOFT_CFS_CFSREPO_H__
#define __CLARASOFT_CFS_CFSREPO_H__

//////////////////////////////////////////////////////////////////////////////
// Repository definitions
//////////////////////////////////////////////////////////////////////////////

#include "qcsrc/cslib.h"

#define CFSRPS_SECMODE_NONE          "*NONE"
#define CFSRPS_SECMODE_TLS           "*TLS"
#define CFSRPS_SECMODE_SSL           "*TLS"

#define CFSRPS_PATH_MAXBUFF           99

#define TLSCFG_LVL_ENVIRON            1
#define TLSCFG_LVL_SESSION            2

#define TLSCFG_PARAMTYPE_STRING       1
#define TLSCFG_PARAMTYPE_NUMERIC      2
#define TLSCFG_PARAMTYPE_ENUM         3

#define TLSCFG_PARAMINFO_FMT_100      100

typedef struct tagTLSCFG_PARAMINFO {

  int  fmt;
  char szName[65];
  int  param;
  int  type;
  int  availLevel;
  int  level;
  char szValue[129];

} TLSCFG_PARAMINFO;

typedef void* CFSRPS;
typedef void* CFSCFG;

CFSRPS
  CFSRPS_Open
    (char* fileName);

CFSCFG
  CFSRPS_OpenConfig
    (CFSRPS This,
     char* szConfig);

char*
  CFSCFG_LookupParam
    (CFSCFG This,
     char* szParam);

CSRESULT
  CFSCFG_IterStart
    (CFSCFG This,
     char* szEnum);

char*
  CFSCFG_IterNext
    (CFSCFG This);

CSRESULT
  CFSRPS_CloseConfig
    (CFSRPS pRepo,
     CFSCFG* This);

CSRESULT
  CFSRPS_Close
    (CFSRPS* This);

CSRESULT
  TLSCFG_LsParam
    (char*  szName,
     long   filter,
     int    fmt,
     CSLIST Listing);

#endif

