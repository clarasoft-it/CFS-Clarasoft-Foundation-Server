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

  This implementation uses PostgreSQL.

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
#include <postgresql/libpq-fe.h>
#include <clarasoft/cslib.h>
#include <clarasoft/csjson.h>

typedef struct tagCFSRPS {

  FILE* pFileRepo;
  PGconn* conn;
  char szStatement[512];
  char szPath[256];
  CSJSON pJson;
  CSJSON_DIRENTRY dire;
  long enumIndex;
  CSJSON_LSENTRY lse;

} CFSRPS;

CFSRPS*
  CFSRPS_Open
    (char* fileName) {

  CFSRPS* pRepo;
  PGconn* conn;

  if (fileName != NULL) {
    // The repository is in a file
    return NULL;
  }
  else {
    conn = PQconnectdb("dbname=cfsrepo");

    if (PQstatus(conn) == CONNECTION_BAD) {
    
      PQfinish(conn);
      return NULL;
    }
  }    

  pRepo = (CFSRPS*)malloc(sizeof(CFSRPS));
  pRepo->conn = conn;
  pRepo->pJson = CSJSON_Constructor();
  memset(&(pRepo->dire), 0, sizeof(CSJSON_DIRENTRY));
  memset(&(pRepo->lse), 0, sizeof(CSJSON_LSENTRY));
  pRepo->enumIndex = 0;

  return pRepo;
}

CSRESULT
  CFSRPS_LoadConfig
    (CFSRPS* This,
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

  if (szConfig != NULL) {

    sprintf(This->szStatement,
            "SELECT STORAGE, FORMAT, ATTR FROM RPSCFM WHERE PATH = '%s'", 
            szConfig);

    result = PQexec(This->conn, This->szStatement);    
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {

      PQclear(result);
      return CS_FAILURE;
    }

    if (PQntuples(result) < 1) {
      PQclear(result);
      return CS_FAILURE;
    } 

    if (!strcmp("*DATABASE", PQgetvalue(result, 0, 0))) {

      sprintf(This->szStatement,
              "SELECT PARAM, VALUE FROM RPSCFP WHERE PATH = '%s'", 
              szConfig);

      result = PQexec(This->conn, This->szStatement);    
        
      if (PQresultStatus(result) != PGRES_TUPLES_OK) {

        PQclear(result);
        return CS_FAILURE;
      }
    
      numRows = PQntuples(result);

      CSJSON_Init(This->pJson, JSON_TYPE_OBJECT);
      CSJSON_MkDir(This->pJson, "/", "param", JSON_TYPE_OBJECT);
      CSJSON_MkDir(This->pJson, "/", "enum", JSON_TYPE_OBJECT);

      for(i=0; i<numRows; i++) {

        CSJSON_InsertString(This->pJson, 
                            "/param", 
                            PQgetvalue(result, i, 0), 
                            PQgetvalue(result, i, 1));
      }      

      PQclear(result);

      sprintf(This->szStatement,
              "Select Distinct param From rpsenm Where path = '%s'", 
              szConfig);

      result = PQexec(This->conn, This->szStatement);    

      numRows = PQntuples(result);

      for(i=0; i<numRows; i++) {

        szParam = PQgetvalue(result, i, 0);
 
        CSJSON_MkDir(This->pJson, "/enum", szParam, JSON_TYPE_ARRAY);

        sprintf(This->szStatement,
                "Select value From rpsenm Where path = '%s' And param = '%s' order by seq", 
                szConfig, szParam);

        result2 = PQexec(This->conn, This->szStatement);    

        numRows2 = PQntuples(result2);

        sprintf(This->szPath, "/enum/%s", szParam);

        for (j=0; j<numRows2; j++) {

          CSJSON_InsertString(This->pJson, 
                              This->szPath, 
                              0, 
                              PQgetvalue(result2, j, 0));
        }

        PQclear(result2);
      }

      PQclear(result);

      return CS_SUCCESS;
    }
    else {

      if (!strcmp("*FILE", PQgetvalue(result, 0, 0))) {

        szConfigFile = PQgetvalue(result, 0, 2);

        if (stat(szConfigFile, &fileInfo) == -1) {
          return CS_FAILURE;
        }

        szFileBuffer = (char*)malloc( (fileInfo.st_size + 1) * sizeof(char));

        stream = fopen(szConfigFile, "rb");

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
        PQclear(result);
        return CS_SUCCESS;
      }
      else {
        PQclear(result);
        return CS_FAILURE;
      }
    }
  }

  return CS_FAILURE;
}

char* 
  CFSRPS_LookupParam
    (CFSRPS* This,
     char* szParam) {

  if (CS_SUCCEED(CSJSON_LookupKey(This->pJson, "/param", szParam, &(This->lse)))) {

    return This->lse.szValue;
  }

  return NULL;
}

CSRESULT
  CFSRPS_IterStart
    (CFSRPS* This, 
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
    (CFSRPS* This) {

  if (This->enumIndex < This->dire.numItems) {
    CSJSON_LookupIndex(This->pJson, This->szPath, This->enumIndex, &(This->lse));
    (This->enumIndex)++;
    return This->lse.szValue;
  }

  return NULL;
}

CSRESULT
  CFSRPS_Close
    (CFSRPS** This) {

  if ((This != NULL) && (*This != NULL)) {
    
    PQfinish((*This)->conn);
    CSJSON_Destructor(&((*This)->pJson));
  
    free(*This);
    *This= NULL;
  }

  return CS_SUCCESS;
}

