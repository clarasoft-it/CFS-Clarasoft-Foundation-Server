/* ===========================================================================
  Clarasoft Foundation Server Repository
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

#ifndef __CLARASOFT_CFS_CFSRPS_H__
#define __CLARASOFT_CFS_CFSRPS_H__

//////////////////////////////////////////////////////////////////////////////
// Repository definitions
//////////////////////////////////////////////////////////////////////////////

#include <clarasoft/cslib.h>

#define CFSRPS_SECMODE_NONE          "*NONE"
#define CFSRPS_SECMODE_TLS           "*TLS"
#define CFSRPS_SECMODE_SSL           "*TLS"

#define CFSRPS_PATH_MAXBUFF           99
#define CFSRPS_PARAM_MAXBUFF          33
#define CFSRPS_VALUE_MAXBUFF          129

typedef void* CFSRPS;

typedef struct tagCFSRPS_CONFIGINFO {

  char szDomain[33];
  char szSubDomain[33];
  char szName[33];
  char szPath[100];
  char szDesc[129];

} CFSRPS_CONFIGINFO;

typedef struct tagCFSRPS_PARAMINFO {

  int  fmt;
  char szParam[33];
  int  type;
  char* szValue;

} CFSRPS_PARAMINFO;

CFSRPS
  CFSRPS_Constructor
    (void);

void
  CFSRPS_Destructor
    (CFSRPS* This);

CSRESULT
  CFSRPS_LoadConfig
    (CFSRPS This,
     CFSRPS_CONFIGINFO* configInfo);

CSRESULT
  CFSRPS_LookupParam
    (CFSRPS This,
     char* szParamName,
     CFSRPS_PARAMINFO* param);

CSRESULT
  CFSRPS_IterStart
    (CFSRPS This,
     char* szParamName);

CSRESULT
  CFSRPS_IterNext
    (CFSRPS This,
     CFSRPS_PARAMINFO* param);

#endif
