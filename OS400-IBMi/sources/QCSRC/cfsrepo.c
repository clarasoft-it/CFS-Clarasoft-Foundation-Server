/* ===========================================================================
  Clarasoft Foundation Server Repository - OS/400
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

EXEC SQL SET OPTION CLOSQLCSR=*ENDMOD;

#include<sys/stat.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "qcsrc/cslib.h"
#include "qcsrc/csjson.h"

EXEC SQL INCLUDE SQLCA;

#define CFSRPS_STORAGE_NULL           0
#define CFSRPS_STORAGE_FILE           1
#define CFSRPS_STORAGE_DB             2

#define CFSCFG_PARAM                  1
#define CFSCFG_ENUM                   2

#define CFSRPS_SECMODE_NONE          "*NONE"
#define CFSRPS_SECMODE_TLS           "*TLS"
#define CFSRPS_SECMODE_SSL           "*TLS"

#define TLSCFG_LVL_ENVIRON            1
#define TLSCFG_LVL_SESSION            2

#define TLSCFG_PARAMTYPE_STRING       1
#define TLSCFG_PARAMTYPE_NUMERIC      2
#define TLSCFG_PARAMTYPE_ENUM         3

#define TLSCFG_PARAMINFO_FMT_100      100

typedef struct tagCFSRPS {

  char* szFileBuffer;
  long size;
  struct stat fileInfo;
  FILE* pFileRepo;
  CSJSON pJson;
  char szStatement[512];
  int storage;

} CFSRPS;

typedef struct tagCFSCFG {

  char szPath[256];

  long iterIndex;

  CSJSON pJson;
  CSJSON_DIRENTRY dire;
  CSJSON_LSENTRY lse;
  CSJSON_LSENTRY* plse;
  CSLIST pListing;

} CFSCFG;

typedef struct tagTLSCFG_CONFIGINFO {

  char szName[65];
  char szDesc[129];

} TLSCFG_CONFIGINFO;

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
  CFSRPS_Open
    (char* szFilePath) {

  CFSRPS* pRepo;

  pRepo = (CFSRPS*)malloc(sizeof(CFSRPS));

  if (szFilePath != NULL) {

    // The repository is in a file or a different database name
    // is specified

    pRepo->storage = CFSRPS_STORAGE_FILE;

    pRepo->pJson = CSJSON_Constructor();

    stat(szFilePath, &(pRepo->fileInfo));

    pRepo->szFileBuffer =
           (char*)malloc ((pRepo->fileInfo.st_size + 1) * sizeof(char));

    pRepo->pFileRepo = fopen(szFilePath, "rb");

    if (!(pRepo->pFileRepo)) {
      pRepo->storage = CFSRPS_STORAGE_NULL;
      CSJSON_Destructor(&(pRepo->pJson));
      free(pRepo->szFileBuffer);
      free(pRepo);
      return NULL;
    }

    pRepo->size = fread(pRepo->szFileBuffer, sizeof(char),
                               pRepo->fileInfo.st_size, pRepo->pFileRepo);

    pRepo->szFileBuffer[pRepo->size] = 0;

    fclose(pRepo->pFileRepo);

    if (CS_FAIL(CSJSON_Parse(pRepo->pJson, pRepo->szFileBuffer, 0))) {
      CSJSON_Destructor(&(pRepo->pJson));
      free(pRepo->szFileBuffer);
      free(pRepo);
      return NULL;
    }

    free(pRepo->szFileBuffer);

    return pRepo;
  }
  else {

    pRepo->storage = CFSRPS_STORAGE_DB;
    pRepo->pJson = NULL;
    return pRepo;
  }

  return NULL;
}

CFSCFG*
  CFSRPS_OpenConfig
    (CFSRPS* pRepo,
     char* szConfig) {

EXEC SQL BEGIN DECLARE SECTION;

  char CFSRPS_OpenConfig_szCfgDomain[33];
  char CFSRPS_OpenConfig_szCfgSubDomain[33];
  char CFSRPS_OpenConfig_szCfgName[33];
  char CFSRPS_OpenConfig_szCfgPath[100];
  char CFSRPS_OpenConfig_szDesc[129];
  char CFSRPS_OpenConfig_szParam[33];
  char CFSRPS_OpenConfig_szStorage[33];
  char CFSRPS_OpenConfig_szFormat[33];
  char CFSRPS_OpenConfig_szAttr[256];
  char CFSRPS_OpenConfig_szValue[256];

EXEC SQL END DECLARE SECTION;

  int numRows;
  int numRows2;
  int i, j;

  CFSCFG* pConfig;

  if (szConfig == NULL) {
    return NULL;
  }
  else {

    pConfig = (CFSCFG*)malloc(sizeof(CFSCFG));
    pConfig->pJson = CSJSON_Constructor();
    pConfig->pListing = CSLIST_Constructor();
    pConfig->iterIndex = 0;

    CSJSON_Init(pConfig->pJson, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pConfig->pJson, "/", "param", JSON_TYPE_OBJECT);
    CSJSON_MkDir(pConfig->pJson, "/", "enum", JSON_TYPE_OBJECT);

    strcpy(CFSRPS_OpenConfig_szCfgPath, szConfig);

    if (pRepo->storage == CFSRPS_STORAGE_FILE) {

    }
    else {

      EXEC SQL DECLARE CFSREPO_1 CURSOR FOR
        SELECT STORAGE, FORMAT, ATTR FROM RPSCFM
          WHERE PATH = :CFSRPS_OpenConfig_szCfgPath;

      EXEC SQL OPEN CFSREPO_1;

      if (SQLCODE == 0) {

        EXEC SQL FETCH CFSREPO_1 Into
          :CFSRPS_OpenConfig_szStorage,
          :CFSRPS_OpenConfig_szFormat,
          :CFSRPS_OpenConfig_szAttr;

        EXEC SQL CLOSE CFSREPO_1;

        if (!strcmp("*DATABASE", CFSRPS_OpenConfig_szStorage)) {

          EXEC SQL DECLARE CFSREPO_2 CURSOR FOR
            SELECT PARAM, VALUE
              FROM RPSCFP
                WHERE PATH = :CFSRPS_OpenConfig_szCfgPath;

          EXEC SQL OPEN CFSREPO_2;

          if (SQLCODE == 0) {

            EXEC SQL FETCH CFSREPO_2 Into
               :CFSRPS_OpenConfig_szParam,
               :CFSRPS_OpenConfig_szValue;

            while (SQLCODE == 0) {

              CSJSON_InsertString(pConfig->pJson,
                                  "/param",
                                  CFSRPS_OpenConfig_szParam,
                                  CFSRPS_OpenConfig_szValue);

              EXEC SQL FETCH CFSREPO_2 Into
                 :CFSRPS_OpenConfig_szParam,
                 :CFSRPS_OpenConfig_szValue;
            }

            EXEC SQL CLOSE CFSREPO_2;

            EXEC SQL DECLARE CFSREPO_3 CURSOR FOR
              SELECT DISTINCT PARAM
                FROM RPSENM
                  WHERE PATH = :CFSRPS_OpenConfig_szCfgPath;

            EXEC SQL OPEN CFSREPO_3;

            if (SQLCODE == 0) {

              EXEC SQL FETCH CFSREPO_3 Into
                 :CFSRPS_OpenConfig_szParam;

              while (SQLCODE == 0) {

                CSJSON_MkDir(pConfig->pJson, "/enum",
                           CFSRPS_OpenConfig_szParam, JSON_TYPE_ARRAY);

                EXEC SQL DECLARE CFSREPO_4 CURSOR FOR
                  SELECT VALUE
                    FROM RPSENM
                      WHERE PATH = :CFSRPS_OpenConfig_szCfgPath
                        AND PARAM = :CFSRPS_OpenConfig_szParam;

                EXEC SQL OPEN CFSREPO_4;

                if (SQLCODE == 0) {

                  sprintf(pConfig->szPath, "/enum/%s",
                                       CFSRPS_OpenConfig_szParam);

                  EXEC SQL FETCH CFSREPO_4 Into
                    :CFSRPS_OpenConfig_szValue;

                  while (SQLCODE == 0) {

                    CSJSON_InsertString(pConfig->pJson,
                                        pConfig->szPath,
                                        0,
                                        CFSRPS_OpenConfig_szValue);

                    EXEC SQL FETCH CFSREPO_4 Into
                      :CFSRPS_OpenConfig_szValue;
                  }

                  EXEC SQL CLOSE CFSREPO_4;
                }
                else {
                  EXEC SQL CLOSE CFSREPO_3;
                  //EXEC SQL ROLLBACK;
                  CSJSON_Destructor(&(pConfig->pJson));
                  CSLIST_Destructor(&(pConfig->pListing));
                  free(pConfig);
                  return NULL;
                }

                EXEC SQL FETCH CFSREPO_3 Into
                     :CFSRPS_OpenConfig_szParam;
              }

              EXEC SQL CLOSE CFSREPO_3;
            }
            else {
              //EXEC SQL ROLLBACK;
              CSJSON_Destructor(&(pConfig->pJson));
              CSLIST_Destructor(&(pConfig->pListing));
              free(pConfig);
              return NULL;
            }
          }
          else {
            //EXEC SQL ROLLBACK;
            CSJSON_Destructor(&(pConfig->pJson));
            CSLIST_Destructor(&(pConfig->pListing));
            free(pConfig);
            return NULL;
          }

          return pConfig;
        }
        else {

          // Repository is in database but config is in a file


        }
      }
      else {
        //EXEC SQL ROLLBACK;
        CSJSON_Destructor(&(pConfig->pJson));
        CSLIST_Destructor(&(pConfig->pListing));
        free(pConfig);
        return NULL;
      }
    }
  }

  return NULL;
}

CSRESULT
  CFSRPS_Close
    (CFSRPS** This) {

  if ((This != NULL) && (*This != NULL)) {

    CSJSON_Destructor(&((*This)->pJson));
    free(*This);
    *This= NULL;
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

CSRESULT
  CFSRPS_CloseConfig
    (CFSRPS* This,
     CFSCFG** pConfig) {

  if ((pConfig != NULL) && (*pConfig != NULL)) {

    CSJSON_Destructor(&((*pConfig)->pJson));
    CSLIST_Destructor(&((*pConfig)->pListing));

    free(*pConfig);
    *pConfig = NULL;
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

char*
  CFSCFG_LookupParam
    (CFSCFG* This,
     char* szParam) {

  if (CS_SUCCEED(CSJSON_LookupKey(This->pJson,
                                  "/param", szParam, &(This->lse)))) {

    return This->lse.szValue;
  }

  return NULL;
}

CSRESULT
  CFSCFG_IterStart
    (CFSCFG* This,
     char* szEnum) {

  This->iterIndex = 0;
  sprintf(This->szPath, "/enum/%s", szEnum);

  if (CS_SUCCEED(CSJSON_LookupDir(This->pJson,
                                  This->szPath, &(This->dire)))) {
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

char*
  CFSCFG_IterNext
    (CFSCFG* This) {

  if (This->iterIndex < This->dire.numItems) {
    CSJSON_LookupIndex(This->pJson, This->szPath, This->iterIndex, &(This->lse));
    (This->iterIndex)++;
    return This->lse.szValue;
  }

  return NULL;
}

CSRESULT
  CFSCFG_IterParamStart
    (CFSCFG* This) {

  This->iterIndex = 0;
  CSJSON_Ls(This->pJson, "/param", This->pListing);
  return CS_SUCCESS;
}

char*
  CFSCFG_IterParamNext
    (CFSCFG* This) {

  if (CS_SUCCEED(CSLIST_Get(This->pListing,
                            (void*)&(This->plse), This->iterIndex))) {
    (This->iterIndex)++;
    return This->plse->szKey;
  }

  return NULL;
}

CSRESULT
  CFSCFG_IterEnumStart
    (CFSCFG* This) {

  This->iterIndex = 0;
  CSJSON_Ls(This->pJson, "/enum", This->pListing);
  return CS_SUCCESS;
}

char*
  CFSCFG_IterEnumNext
    (CFSCFG* This) {

  if (CS_SUCCEED(CSLIST_Get(This->pListing,
                            (void*)&(This->plse), This->iterIndex))) {
    (This->iterIndex)++;
    return This->plse->szKey;
  }

  return NULL;
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

  EXEC SQL DECLARE C_1 CURSOR FOR STMT_1;

  if (SQLCODE == 0) {

    EXEC SQL OPEN C_1;

    if (SQLCODE == 0) {

      EXEC SQL FETCH C_1 Into
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

        EXEC SQL FETCH C_1 Into
            :TLSCFG_LsParam_param,
            :TLSCFG_LsParam_type,
            :TLSCFG_LsParam_level,
            :TLSCFG_LsParam_szValue;
      }

      EXEC SQL CLOSE C_1;
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

