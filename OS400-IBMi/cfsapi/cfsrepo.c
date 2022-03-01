/* ===========================================================================
  Clarasoft Foundation Server Repository
  cfsrepo.c

  CRTSQLCI OBJ(CFSREPO) SRCFILE(QCSRC_CFS)
      CLOSQLCSR(*ENDMOD) OUTPUT(*PRINT) DBGVIEW(*SOURCE)
           COMPILEOPT('SYSIFCOPT(*IFSIO)')

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

EXEC SQL SET OPTION CLOSQLCSR=*ENDMOD;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "qcsrc_cfs/cslib.h"
#include "qcsrc_cfs/csjson.h"

EXEC SQL INCLUDE SQLCA;

#define CFSRPS_SECMODE_NONE          "*NONE"
#define CFSRPS_SECMODE_TLS           "*TLS"
#define CFSRPS_SECMODE_SSL           "*TLS"

#define CFSREPO_TYPE_DB               0x0000001
#define CFSREPO_TYPE_IFS              0x0000002

#define CFSREPO_FMT_SQL               0x0000001
#define CFSREPO_FMT_JSON              0x0000002
#define CFSREPO_FMT_XML               0x0000003

#define CFSREPO_PRMFMT_STRING         0x0000001
#define CFSREPO_PRMFMT_NUMERIC        0x0000002
#define CFSREPO_PRMFMT_BOOL           0x0000003

#define TLSCFG_LVL_ENVIRON            1
#define TLSCFG_LVL_SESSION            2

#define TLSCFG_PARAMTYPE_STRING       1
#define TLSCFG_PARAMTYPE_NUMERIC      2
#define TLSCFG_PARAMTYPE_ENUM         3

#define TLSCFG_PARAMINFO_FMT_100      100

typedef struct tagCFSRPS {

  CSMAP config;
  CSLIST enumeration;
  CSJSON pJson;

  char szDomain[33];
  char szSubDomain[33];
  char szName[33];
  char szPath[99];

  long CurIndex;
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

typedef struct tagTLSCFG_CONFIGINFO {

  char szName[65];
  char szDesc[129];

} TLSCFG_CONFIGINFO;

typedef struct tagCFSRPS_PARAMINFO {

  int  fmt;
  char szParam[33];
  int  type;
  char szValue[256];

} CFSRPS_PARAMINFO;

typedef struct tagTLSCFG_PARAMINFO {

  int  fmt;
  char szName[65];
  int  param;
  int  type;
  int  availLevel;
  int  level;
  char szValue[256];

} TLSCFG_PARAMINFO;

CFSRPS*
  CFSRPS_Constructor
    (void) {

  CFSRPS* Instance;

  Instance = (CFSRPS*)malloc(sizeof(CFSRPS));

  Instance->config = CSMAP_Constructor();
  Instance->enumeration = CSLIST_Constructor();
  Instance->pJson = CSJSON_Constructor();
  Instance->CurIndex = 0;

  return Instance;
}

void
  CFSRPS_Destructor
    (CFSRPS** This) {

  CSMAP_Destructor(&((*This)->config));
  CSLIST_Destructor(&((*This)->enumeration));
  CSJSON_Destructor(&((*This)->pJson));

  free(*This);
}

CSRESULT
  CFSRPS_LoadConfig
    (CFSRPS* This,
     CFSRPS_CONFIGINFO* configInfo) {

EXEC SQL BEGIN DECLARE SECTION;

  char CFSRPS_LoadConfig_szCfgDomain[33];
  char CFSRPS_LoadConfig_szCfgSubDomain[33];
  char CFSRPS_LoadConfig_szCfgName[33];
  char CFSRPS_LoadConfig_szCfgPath[100];
  char CFSRPS_LoadConfig_szDesc[129];
  char CFSRPS_LoadConfig_szParam[33];
  char CFSRPS_LoadConfig_szValue[256];
  char CFSRPS_LoadConfig_szStorage[33];
  char CFSRPS_LoadConfig_szFormat[33];
  char CFSRPS_LoadConfig_szAttr[256];

EXEC SQL END DECLARE SECTION;

  struct stat fileInfo;
  char* pConfigBuffer;
  FILE* stream;
  CSJSON pJson;
  CSSTRCV pCVT;
  CSRESULT hResult;

  CFSRPS_PARAMINFO pi;

  strcpy(This->szPath, configInfo->szPath);
  strcpy(CFSRPS_LoadConfig_szCfgPath, configInfo->szPath);

  Exec SQL
    Select storage, format, attr
      Into :CFSRPS_LoadConfig_szStorage,
           :CFSRPS_LoadConfig_szFormat,
           :CFSRPS_LoadConfig_szAttr
    From RPSCFM
    Where path = :CFSRPS_LoadConfig_szCfgPath;


  if (SQLCODE != 0) {
    return CS_FAILURE;
  }

  if (!strcmp(CFSRPS_LoadConfig_szStorage, "*DATABASE")) {

    This->Type = CFSREPO_TYPE_DB;
    This->Format = CFSREPO_FMT_SQL;

    CSMAP_Clear(This->config);

    EXEC SQL DECLARE CFSREPO_3 CURSOR FOR
      SELECT
        param,
        value
      FROM RPSCFP
        Where path = :CFSRPS_LoadConfig_szCfgPath;

    if (SQLCODE == 0) {

      EXEC SQL OPEN CFSREPO_3;

      if (SQLCODE == 0) {

        EXEC SQL FETCH CFSREPO_3 Into
           :CFSRPS_LoadConfig_szParam,
           :CFSRPS_LoadConfig_szValue;

        while (SQLCODE == 0) {

          strcpy(pi.szParam, CFSRPS_LoadConfig_szParam);
          strcpy(pi.szValue, CFSRPS_LoadConfig_szValue);

          CSMAP_Insert(This->config,
                       CFSRPS_LoadConfig_szParam,
                       (void*)(&pi),
                       sizeof(CFSRPS_PARAMINFO));

          EXEC SQL FETCH CFSREPO_3 Into
             :CFSRPS_LoadConfig_szParam,
             :CFSRPS_LoadConfig_szValue;
        }

        EXEC SQL CLOSE CFSREPO_3;
        hResult = CS_SUCCESS;
      }
      else {
        //EXEC SQL ROLLBACK;
        return CS_FAILURE;
      }
    }
    else {
      //EXEC SQL ROLLBACK;
      return CS_FAILURE;
    }
  }
  else {

    if (!strcmp(CFSRPS_LoadConfig_szStorage, "*IFS")) {

      if (!strcmp(CFSRPS_LoadConfig_szFormat, "*JSON")) {


        This->Type = CFSREPO_TYPE_IFS;
        This->Format = CFSREPO_FMT_JSON;

        fileInfo.st_size = 0;
        if (stat(CFSRPS_LoadConfig_szAttr, &fileInfo) == 0) {

          if (fileInfo.st_size > 0) {

            pConfigBuffer = (char*)malloc(fileInfo.st_size * sizeof(char) + 1);

            stream = fopen(CFSRPS_LoadConfig_szAttr, "rb");

            if (stream) {

              fread(pConfigBuffer, fileInfo.st_size, 1, stream);
              fclose(stream);
              pConfigBuffer[fileInfo.st_size] = 0;

              pCVT = CSSTRCV_Constructor();

              CSSTRCV_SetConversion(pCVT, "01208", "00000");
              CSSTRCV_StrCpy(pCVT, pConfigBuffer, fileInfo.st_size);
              CSSTRCV_Get(pCVT, pConfigBuffer);
              CSSTRCV_Destructor(&pCVT);

              if (CS_SUCCEED(CSJSON_Parse(This->pJson, pConfigBuffer, 0))) {
                hResult = CS_SUCCESS;
              }
              else {
                hResult = CS_FAILURE;
              }
            }
            else {
              hResult = CS_FAILURE;
            }

            free(pConfigBuffer);
          }
          else {
            hResult = CS_FAILURE;
          }
        }
        else {
          hResult = CS_FAILURE;
        }
      }
      else {
        hResult = CS_FAILURE;
      }
    }
    else {
      hResult = CS_FAILURE;
    }
  }

  //EXEC SQL COMMIT;
  return hResult;
}

CSRESULT
  CFSRPS_LookupParam
    (CFSRPS* This,
     char* szParamName,
     CFSRPS_PARAMINFO* param) {

  CFSRPS_PARAMINFO* ppi;
  long size;
  CSJSON_LSENTRY plse;

  switch(This->Format) {

    case CFSREPO_FMT_SQL:

      size = sizeof(CFSRPS_PARAMINFO);
      if (CS_SUCCEED(CSMAP_Lookup(This->config,
                         szParamName, (void**)&ppi, &size))) {

        param->fmt = 0;
        param->type = CFSREPO_PRMFMT_STRING;
        strcpy(param->szParam, ppi->szParam);
        strcpy(param->szValue, ppi->szValue);

        return CS_SUCCESS;
      }

      break;

    case CFSREPO_FMT_JSON:

      if (CS_SUCCEED(CSJSON_LookupKey(This->pJson,
                                 "/", szParamName, &plse))) {

        param->fmt = 0;
        param->type = CFSREPO_PRMFMT_STRING;
        strcpy(param->szParam, szParamName);
        strcpy(param->szValue, plse.szValue);

        return CS_SUCCESS;
      }

      break;
  }

  return CS_FAILURE;
}

CSRESULT
  CFSRPS_IterStart
    (CFSRPS* This,
     char* szParamName) {

EXEC SQL BEGIN DECLARE SECTION;

  char CFSRPS_IterStart_szCfgDomain[33];
  char CFSRPS_IterStart_szCfgSubDomain[33];
  char CFSRPS_IterStart_szCfgName[33];
  char CFSRPS_IterStart_szCfgPath[100];
  char CFSRPS_IterStart_szDesc[129];
  char CFSRPS_IterStart_szParam[33];
  char CFSRPS_IterStart_szValue[256];

EXEC SQL END DECLARE SECTION;

  CFSRPS_PARAMINFO pi;
  CSJSON_DIRENTRY pdire;
  CSJSON_LSENTRY plse;

  char szPath[129];
  long i;

  CSLIST_Clear(This->enumeration);
  This->CurIndex = 0;

  switch(This->Format) {

    case CFSREPO_FMT_SQL:

      strcpy(CFSRPS_IterStart_szCfgPath, This->szPath);
      strcpy(CFSRPS_IterStart_szParam, szParamName);

      EXEC SQL DECLARE CFSREPO_4 CURSOR FOR
        SELECT
          value
        FROM RPSENM
          Where path = :CFSRPS_IterStart_szCfgPath
             And param = :CFSRPS_IterStart_szParam
          Order By seq;

      if (SQLCODE == 0) {

        EXEC SQL OPEN CFSREPO_4;

        if (SQLCODE == 0) {

          EXEC SQL FETCH CFSREPO_4 Into
               :CFSRPS_IterStart_szValue;

          while (SQLCODE == 0) {

            pi.fmt = 0;
            pi.type = CFSREPO_PRMFMT_STRING;
            strcpy(pi.szParam, szParamName);
            strcpy(pi.szValue, CFSRPS_IterStart_szValue);

            CSLIST_Insert(This->enumeration,
                       (void*)(&pi),
                       sizeof(CFSRPS_PARAMINFO), CSLIST_BOTTOM);

            EXEC SQL FETCH CFSREPO_4 Into
                 :CFSRPS_IterStart_szValue;
          }

          EXEC SQL CLOSE CFSREPO_4;
        }
        else {
          //EXEC SQL ROLLBACK;
          return CS_FAILURE;
        }
      }
      else {
        //EXEC SQL ROLLBACK;
        return CS_FAILURE;
      }

      break;

    case CFSREPO_FMT_JSON:

      strcpy(szPath, "/enum/");
      strcat(szPath, szParamName);

      if (CS_SUCCEED(CSJSON_LookupDir(This->pJson,
                     szPath, &pdire))) {

        if (pdire.type == JSON_TYPE_ARRAY) {
          for (i=0; i<pdire.numItems; i++) {
            if (CS_SUCCEED(CSJSON_LookupIndex(This->pJson,
                                              szPath,
                                              i,
                                              &plse))) {
              pi.fmt = 0;
              pi.type = CFSREPO_PRMFMT_STRING;
              strcpy(pi.szParam, szParamName);
              strcpy(pi.szValue, plse.szValue);

              CSLIST_Insert(This->enumeration,
                       (void*)(&pi),
                       sizeof(CFSRPS_PARAMINFO), CSLIST_BOTTOM);

            }
          }
        }
        else {
          return CS_FAILURE;
        }
      }
      else {
        return CS_FAILURE;
      }

      break;
  }

  //EXEC SQL COMMIT;
  return CS_SUCCESS;
}

CSRESULT
  CFSRPS_IterNext
    (CFSRPS* This,
     CFSRPS_PARAMINFO* param) {

  if (CS_SUCCEED(CSLIST_Get(This->enumeration,
                            (void*)param, This->CurIndex))) {
    (This->CurIndex)++;
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

CSRESULT
  TLSCFG_LsParam
    (char*  szName,
     long   filter,
     int    fmt,
     CSLIST Listing) {

EXEC SQL BEGIN DECLARE SECTION;

  char TLSCFG_LsParam_szCfgName[65];
  char TLSCFG_LsParam_szStatement[1025];
  char TLSCFG_LsParam_szParamName[129];
  char TLSCFG_LsParam_szValue[129];

  long TLSCFG_LsParam_param;
  long TLSCFG_LsParam_type;
  long TLSCFG_LsParam_level;
  long TLSCFG_LsParam_availLevel;

  TLSCFG_PARAMINFO paramInfo;

EXEC SQL END DECLARE SECTION;

  TLSCFG_PARAMINFO* ppi;

  strcpy(TLSCFG_LsParam_szCfgName, szName);

  CSLIST_Clear(Listing);

  switch(filter) {

    case TLSCFG_LVL_ENVIRON:
    case TLSCFG_LVL_SESSION:

      sprintf(TLSCFG_LsParam_szStatement,
              "Select "
              "a.param, a.type, a.level, a.value "
              "From TLSCFP a "
              "Where a.name = '%s' "
              "And a.level = %ld ",
              TLSCFG_LsParam_szCfgName,
              filter);

      break;

    default:

      sprintf(TLSCFG_LsParam_szStatement,
              "Select "
              "a.param, a.type, a.level, a.value "
              "From TLSCFP a "
              "Where a.name = '%s' ",
              TLSCFG_LsParam_szCfgName);

      break;
  }

  EXEC SQL PREPARE STMT_1 FROM :TLSCFG_LsParam_szStatement;

  EXEC SQL DECLARE CFSREPO_1 CURSOR FOR STMT_1;

  if (SQLCODE == 0) {

    EXEC SQL OPEN CFSREPO_1;

    if (SQLCODE == 0) {

      EXEC SQL FETCH CFSREPO_1 Into
          :TLSCFG_LsParam_param,
          :TLSCFG_LsParam_type,
          :TLSCFG_LsParam_level,
          :TLSCFG_LsParam_szValue;

      while (SQLCODE == 0) {

        switch(fmt) {

          default:

            paramInfo.fmt = TLSCFG_PARAMINFO_FMT_100;
            paramInfo.param = TLSCFG_LsParam_param;
            strcpy(paramInfo.szValue, TLSCFG_LsParam_szValue);
            paramInfo.type = TLSCFG_LsParam_type;
            paramInfo.level = TLSCFG_LsParam_level;

            break;
        }

        CSLIST_Insert(Listing,
                   (void*)(&paramInfo),
                   sizeof(TLSCFG_PARAMINFO),
                   CSLIST_BOTTOM);

        EXEC SQL FETCH CFSREPO_1 Into
            :TLSCFG_LsParam_param,
            :TLSCFG_LsParam_type,
            :TLSCFG_LsParam_level,
            :TLSCFG_LsParam_szValue;
      }

      EXEC SQL CLOSE CFSREPO_1;
    }
    else {
      //EXEC SQL ROLLBACK;
      return CS_FAILURE;
    }
  }
  else {
    //EXEC SQL ROLLBACK;
    return CS_FAILURE;
  }

  //EXEC SQL COMMIT;
  return CS_SUCCESS;
}
