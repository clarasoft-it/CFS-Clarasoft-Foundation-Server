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

/* ---------------------------------------------------------------------------

  This implementation may use PostgreSQL.

  To avoid having to provide a user/password everytime one wants to access
  the CFS Repository, the database (named cfsrepo) will specify trust 
  authentication mode; the host will also only be restricted to localhost.

  The database may be created as follow:

  create database cfsrepo LOCALE 'en_CA.UTF-8';

  Then, edit the pg_hba.conf file to specify trust authentication; the 
  location of this file may be retrieved by issuing the following
  command in psql:

    psql -t -P format=unaligned -c 'SHOW hba_file;'

  The output will be something like:

    /etc/postgresql/14/main/pg_hba.conf

  The pg_hba.conf file has entries like the following:

      # TYPE  DATABASE        USER            ADDRESS                 METHOD

      # "local" is for Unix domain socket connections only
      local   all             all                                     peer
      # IPv4 local connections:
      host    all             all             127.0.0.1/32            scram-sha-256
      # IPv6 local connections:
      host    all             all             ::1/128                 scram-sha-256


  It is IMPORTANT that the authentication rules be placed in the proper order.
  PostgreSQL will examine each line in sequence until it finds one that matches 
  the conditions under which the connection is requested. It is therefore
  recommended to put the CFS Repository database connection configuration(s)
  before lines that specify "all" under the DATABSE column:

      # TYPE  DATABASE        USER            ADDRESS                 METHOD

      #CFS Repository --------------------------------------------------------------
      local   cfsrepo         all                                     trust
      local   cfsrepo         all             127.0.0.1/32            trust
      local   cfsrepo         all             ::1/128                 trust
      #-----------------------------------------------------------------------------

      # "local" is for Unix domain socket connections only
      local   all             all                                     peer
      # IPv4 local connections:
      host    all             all             127.0.0.1/32            scram-sha-256
      # IPv6 local connections:
      host    all             all             ::1/128                 scram-sha-256


  The aboce example only allows connections from the system hosting the 
  CFS Repository, either using local sockets or TCP sockets. 


  To test if the CFS Repository is accessible in trusted mode, from
  the command line, do the following:

  
  Open and Close operations are usually matched. The Open operation returns 
  either a valid handle or NULL; the Close operation can handle safely a 
  NULL handle such that:

     Handle = CFSRPS_Open(0);

     CFSRPS_CLose(&Handle);


  will execute ok. You must check for a valid handle however if you use
  any other CFSRPS methods; if you pass a NULL handle, the functions
  will crash.

--------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <clarasoft/cslib.h>
#include <clarasoft/csjson.h>

#ifdef __CLARASOFT_CFS_POSTGRESQL_SUPPORT

#include <postgresql/libpq-fe.h>

typedef struct tagCFSRPS {

  FILE* pFileRepo;
  PGconn* conn;
  char szStatement[512];

} CFSRPS;

#else

typedef struct tagCFSRPS {

  FILE* pFileRepo;
  char szStatement[512];

} CFSRPS;

#endif

typedef struct tagCFSCFG {

  char szPath[256];
  CSJSON pJson;
  CSJSON_DIRENTRY dire;
  long enumIndex;
  CSJSON_LSENTRY lse;

} CFSCFG;

#ifdef __CLARASOFT_CFS_POSTGRESQL_SUPPORT

CFSRPS*
  CFSRPS_Open
    (char* szConnString) {

  CFSRPS* pRepo;

  pRepo = (CFSRPS*)malloc(sizeof(CFSRPS));

  if (szConnString != NULL) {
    // The repository is in a file or a different database name
    // is specified

    pRepo->pFileRepo = fopen(szConnString, "r");

    if (pRepo->pFileRepo == NULL) {
      free(pRepo);
      return NULL;
    }

    pRepo->conn = NULL;
  }
  else {

    // At present, only the cfsrepo database name is supported
    pRepo->conn = PQconnectdb("dbname=cfsrepo");

    if (PQstatus(pRepo->conn) == CONNECTION_BAD) {
    
      PQfinish(pRepo->conn);
      free(pRepo);
      return NULL;
    }

    pRepo->pFileRepo = NULL;
  }

  return pRepo;
}

CFSCFG*
  CFSRPS_OpenConfig
    (CFSRPS* pRepo,
     char* szConfig) {

  int numRows;
  int numRows2;
  int i, j;

  long size;

  char* szParam;
  char* szConfigFile;
  char* szFileBuffer;

  struct stat fileInfo;

  FILE* stream;

  PGresult *result;
  PGresult *result2;

  CFSCFG* pConfig;

  if (szConfig != NULL) {

    if (pRepo->conn != NULL) {

      pConfig = (CFSCFG*)malloc(sizeof(CFSCFG));

      pConfig->pJson = CSJSON_Constructor();
      pConfig->enumIndex = 0;

      sprintf(pRepo->szStatement,
            "SELECT STORAGE, FORMAT, ATTR FROM RPSCFM WHERE PATH = '%s'", 
            szConfig);

      result = PQexec(pRepo->conn, pRepo->szStatement);    
    
      if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        CSJSON_Destructor(&(pConfig->pJson));
        free(pConfig);
        PQclear(result);
        return NULL;
      }

      if (PQntuples(result) < 1) {
        CSJSON_Destructor(&(pConfig->pJson));
        free(pConfig);
        PQclear(result);
        return NULL;
      } 

      if (!strcmp("*DATABASE", PQgetvalue(result, 0, 0))) {

        sprintf(pRepo->szStatement,
              "SELECT PARAM, VALUE FROM RPSCFP WHERE PATH = '%s'", 
              szConfig);

        result = PQexec(pRepo->conn, pRepo->szStatement);    
        
        if (PQresultStatus(result) != PGRES_TUPLES_OK) {
          CSJSON_Destructor(&(pConfig->pJson));
          free(pConfig);
          PQclear(result);
          return NULL;
        }
    
        numRows = PQntuples(result);

        CSJSON_Init(pConfig->pJson, JSON_TYPE_OBJECT);
        CSJSON_MkDir(pConfig->pJson, "/", "param", JSON_TYPE_OBJECT);
        CSJSON_MkDir(pConfig->pJson, "/", "enum", JSON_TYPE_OBJECT);

        for(i=0; i<numRows; i++) {

          CSJSON_InsertString(pConfig->pJson, 
                              "/param", 
                              PQgetvalue(result, i, 0), 
                              PQgetvalue(result, i, 1));
        }      

        PQclear(result);

        sprintf(pRepo->szStatement,
              "Select Distinct param From rpsenm Where path = '%s'", 
              szConfig);
  
        result = PQexec(pRepo->conn, pRepo->szStatement);    

        numRows = PQntuples(result);

        for(i=0; i<numRows; i++) {

          szParam = PQgetvalue(result, i, 0);
 
          CSJSON_MkDir(pConfig->pJson, "/enum", szParam, JSON_TYPE_ARRAY);

          sprintf(pRepo->szStatement,
                 "Select value From rpsenm Where path = '%s' And param = '%s' order by seq", 
                 szConfig, szParam);

          result2 = PQexec(pRepo->conn, pRepo->szStatement);    

          numRows2 = PQntuples(result2);

          sprintf(pConfig->szPath, "/enum/%s", szParam);

          for (j=0; j<numRows2; j++) {

            CSJSON_InsertString(pConfig->pJson, 
                                pConfig->szPath, 
                                0, 
                                PQgetvalue(result2, j, 0));
          }

          PQclear(result2);
        }

        PQclear(result);

        return pConfig;
      }
      else {

        if (!strcmp("*FILE", PQgetvalue(result, 0, 0))) {

          szConfigFile = PQgetvalue(result, 0, 2);

          if (stat(szConfigFile, &fileInfo) == -1) {
            CSJSON_Destructor(&(pConfig->pJson));
            free(pConfig);
            return NULL;
          }

          szFileBuffer = (char*)malloc( (fileInfo.st_size + 1) * sizeof(char));

          stream = fopen(szConfigFile, "rb");

          if (!stream) {
            free(szFileBuffer);
            CSJSON_Destructor(&(pConfig->pJson));
            free(pConfig);
            return NULL;
          }

          size = fread(szFileBuffer, sizeof(char), fileInfo.st_size, stream);
          szFileBuffer[size] = 0;
          fclose(stream);

          if (CS_FAIL(CSJSON_Parse(pConfig->pJson, szFileBuffer, 0))) {
            free(szFileBuffer);
            CSJSON_Destructor(&(pConfig->pJson));
            free(pConfig);
            return NULL;
          }

          free(szFileBuffer);
          PQclear(result);
          return pConfig;
        }
        else {
          PQclear(result);
          CSJSON_Destructor(&(pConfig->pJson));
          free(pConfig);
          return NULL;
        }
      }
    }
    else {
       
      // The repository is file based

    }
  }
  else {


  } 

  return NULL;
}

CSRESULT
  CFSRPS_Close
    (CFSRPS** This) {

  if ((This != NULL) && (*This != NULL)) {
    
    PQfinish((*This)->conn);
    free(*This);
    *This= NULL;
  }

  return CS_SUCCESS;
}

#else

CFSRPS*
  CFSRPS_Open
    (char* szConnString) {

  CFSRPS* pRepo;

  if (szConnString != NULL) {

    pRepo = (CFSRPS*)malloc(sizeof(CFSRPS));

    // The repository is in a file or a different database name
    // is specified

    pRepo->pFileRepo = fopen(szConnString, "r");

    if (pRepo->pFileRepo == NULL) {
      free(pRepo);
      return NULL;
    }

    return pRepo;
  }

  return NULL;
}

CFSCFG*
  CFSRPS_OpenConfig
    (CFSRPS* pRepo,
     char* szConfig) {

  CFSCFG* pConfig;

  if (szConfig != NULL) {

    pConfig = (CFSCFG*)malloc(sizeof(CFSCFG));

    pConfig->pJson = CSJSON_Constructor();
    pConfig->enumIndex = 0;

    CSJSON_Init(pConfig->pJson, JSON_TYPE_OBJECT);
    CSJSON_MkDir(pConfig->pJson, "/", "param", JSON_TYPE_OBJECT);
    CSJSON_MkDir(pConfig->pJson, "/", "enum", JSON_TYPE_OBJECT);

    return pConfig;
  }

  return NULL;
}

CSRESULT
  CFSRPS_Close
    (CFSRPS** This) {

  if ((This != NULL) && (*This != NULL)) {
    
    fclose((*This)->pFileRepo);
    free(*This);
    *This= NULL;
  }

  return CS_SUCCESS;
}

#endif

char* 
  CFSRPS_LookupParam
    (CFSCFG* This,
     char* szParam) {

  if (CS_SUCCEED(CSJSON_LookupKey(This->pJson, "/param", szParam, &(This->lse)))) {

    return This->lse.szValue;
  }

  return NULL;
}

CSRESULT
  CFSRPS_IterStart
    (CFSCFG* This, 
     char* szEnum) {

  This->enumIndex = 0;
  sprintf(This->szPath, "/enum/%s", szEnum);

  if (CS_SUCCEED(CSJSON_LookupDir(This->pJson, This->szPath, &(This->dire)))) {
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

char*
  CFSRPS_IterNext
    (CFSCFG* This) {

  if (This->enumIndex < This->dire.numItems) {
    CSJSON_LookupIndex(This->pJson, This->szPath, This->enumIndex, &(This->lse));
    (This->enumIndex)++;
    return This->lse.szValue;
  }

  return NULL;
}

CSRESULT
  CFSRPS_CloseConfig
    (CFSCFG** pConfig) {

  if ((pConfig != NULL) && (*pConfig != NULL)) {

    CSJSON_Destructor(&((*pConfig)->pJson));
    free(*pConfig);
    *pConfig = NULL;
    return CS_SUCCESS;
  }

  return CS_FAILURE;
}
