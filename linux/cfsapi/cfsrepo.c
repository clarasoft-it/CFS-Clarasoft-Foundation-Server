/* ===========================================================================
  Clarasoft Foundation Server Repository
  cfsrepo.c

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
    
#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clarasoft/cslib.h>
#include <clarasoft/csjson.h>

typedef struct tagCFSRPS {

  CSJSON pJson;

  char szDomain[33];
  char szSubDomain[33];
  char szName[33];
  char szPath[99];
  char szJsonPath[513];

  long CurEnumIndex;
  long Type;
  long Format;

} CFSRPS;

typedef struct tagCFSRPS_CONFIGINFO {

  char szDomain[33];
  char szSubDomain[33];
  char szName[33];
  char szPath[100];
  char szDesc[129];

} CFSRPS_CONFIGINFO;

typedef struct tagCFSRPS_PARAMINFO {

  // TLSCFG_PARAMINFO_FMT_100 section
  int  fmt;
  char szParam[33];
  int  type;
  char* szValue;

} CFSRPS_PARAMINFO;

CFSRPS*
  CFSRPS_Constructor
    (void) {

  CFSRPS* Instance;

  Instance = (CFSRPS*)malloc(sizeof(CFSRPS));

  Instance->pJson = CSJSON_Constructor();
  Instance->CurEnumIndex = 0;

  return Instance;
}

void
  CFSRPS_Destructor
    (CFSRPS** This) {

  CSJSON_Destructor(&((*This)->pJson));
  free(*This);
}

CSRESULT
  CFSRPS_LoadConfig
    (CFSRPS* This,
     CFSRPS_CONFIGINFO* configInfo) {

  struct stat fileInfo;

  char* szFileBuffer;

  char szConfPath[PATH_MAX];
  char szBasePath[256];

  long size;

  FILE* stream;
  CSJSON_LSENTRY lse;

  CSJSON_Init(This->pJson, JSON_TYPE_OBJECT);

  strcpy(This->szPath, configInfo->szPath);

  // Get file size

  if (stat("/etc/clarasoft/cfs/cfs-repo.json", &fileInfo) == -1) {
    return CS_FAILURE;
  }

  szFileBuffer = (char*)malloc( (fileInfo.st_size + 1) * sizeof(char));

  stream = fopen("/etc/clarasoft/cfs/cfs-repo.json", "rb");

  if (!stream) {
    free(szFileBuffer);
    return CS_FAILURE;
  }

  size = fread(szFileBuffer, sizeof(char), fileInfo.st_size, stream);
  szFileBuffer[size] = 0;
  fclose(stream);

  if (CS_FAIL(CSJSON_Parse(This->pJson, szFileBuffer, 0))) {
    free(szFileBuffer);
    return CS_FAILURE;
  }

  free(szFileBuffer);

  strcpy(szBasePath, "/repo/");
  strcpy(szBasePath, configInfo->szPath);

  if (CS_FAIL(CSJSON_LookupKey(This->pJson, szBasePath, "conf", &lse))) {
    return CS_FAILURE;
  }

  strcat(szBasePath, lse.szValue);

  // get the configuration base directory path
  if (CS_FAIL(CSJSON_LookupKey(This->pJson, "/base", "confDir", &lse))) {
    return CS_FAILURE;
  }

  strcpy(szConfPath, lse.szValue);

  // add slash if none at the end

  if (szConfPath[strlen(szConfPath)-1] != '/') {
    strcat(szConfPath, "/");
  }

  // get the configuration file name
  if (CS_FAIL(CSJSON_LookupKey(This->pJson, "/base", "confDir", &lse))) {
    return CS_FAILURE;
  }

  // read in the configuration file 

  if (stat(lse.szValue, &fileInfo) == -1) {
    return CS_FAILURE;
  }

  szFileBuffer = (char*)malloc( (fileInfo.st_size + 1) * sizeof(char));

  stream = fopen(lse.szValue, "rb");

  if (!stream) {
    free(szFileBuffer);
    return CS_FAILURE;
  }

  size = fread(szFileBuffer, sizeof(char), fileInfo.st_size, stream);
  szFileBuffer[size] = 0;
  fclose(stream);

  if (CS_FAIL(CSJSON_Parse(This->pJson, szFileBuffer, 0))) {
    free(szFileBuffer);
    return CS_FAILURE;
  }

  free(szFileBuffer);
  return CS_SUCCESS;
}

CSRESULT
  CFSRPS_LookupParam
    (CFSRPS* This,
     char* szParamName,
     CFSRPS_PARAMINFO* param) {

  CSJSON_LSENTRY lse;

  if (CS_SUCCEED(CSJSON_LookupKey(This->pJson,
                                  "/param",
                                  szParamName, &lse))) {

    param->type = lse.type;
    param->szValue = lse.szValue;

    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

CSRESULT
  CFSRPS_IterStart
    (CFSRPS* This,
     char* szParamName) {

  CSJSON_DIRENTRY pdire;

  This->CurEnumIndex = 0;

  strcpy(This->szJsonPath, "/enum/");
  strcat(This->szJsonPath, szParamName);

  if (CS_SUCCEED(CSJSON_LookupDir(This->pJson,
                                  This->szJsonPath, &pdire))) {

    if (pdire.type == JSON_TYPE_ARRAY) {
      return CS_SUCCESS;
    }
  }

  return CS_FAILURE;
}

CSRESULT
  CFSRPS_IterNext
    (CFSRPS* This,
     CFSRPS_PARAMINFO* param) {

  CSJSON_LSENTRY lse;

  if (CS_SUCCEED(CSJSON_LookupIndex(This->pJson,
                                    This->szJsonPath,
                                    This->CurEnumIndex,
                                    &lse))) {
    param->type = lse.type;
    param->szValue = lse.szValue;

    (This->CurEnumIndex)++;
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}
