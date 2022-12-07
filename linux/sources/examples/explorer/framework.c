
/* ==========================================================================

  Clarasoft Foundation Server - Linux

  csap application framework
  Version 1.0.0

  Distributed under the MIT license

  Copyright (c) 2013 Clarasoft I.T. Solutions Inc.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sub-license, and/or sell
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

========================================================================== */

#define _GNU_SOURCE     /* for basename( ) in <string.h> */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <postgresql/libpq-fe.h>

#include <clarasoft/cslib.h>
#include <clarasoft/cfs.h>
#include <clarasoft/csjson.h>
#include <clarasoft/cscpt.h>

CSRESULT
  FRMWRK_Init
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char szStatement[1025];
  char szPath[1025];
  char szPath2[4097];

  char* pszValue;

  int numRows;
  int numRows2;
  int i, j;

  PGconn* conn;
  PGresult* result;
  PGresult* result2;

  CFSRPS pRepo;
  CFSCFG pConfig;
  
  CSJSON_LSENTRY lse;

  if (CS_SUCCEED(CSJSON_LookupKey(pJsonIn, "/ctl", "langID", &lse))) {

    // Connect to database

    conn = PQconnectdb("dbname=cfsrepo");

    if (PQstatus(conn) == CONNECTION_BAD) {
    
      PQfinish(conn);
      return CS_FAILURE;
    }

    CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);

    CSJSON_InsertString(pJsonOut, "/data", "system", "DVUAP001");
    CSJSON_InsertString(pJsonOut, "/data", "platform", "*DEV");
    CSJSON_InsertString(pJsonOut, "/data", "sector", "FES-A1");

    CSJSON_MkDir(pJsonOut, "/data", "menu", JSON_TYPE_OBJECT);
  
    sprintf(szStatement,
            "select mid, pid, caption from dcxmenu a "
            "join cptcptn b "
            "on a.captionid = b.captionid "
            "where a.realm = 'DC-EXPLORER' and b.langid = '%s' "
            "order by seq", 
            lse.szValue);

    result = PQexec(conn, szStatement);    

    numRows = PQntuples(result);

    CSJSON_MkDir(pJsonOut, "/data", "menu", JSON_TYPE_OBJECT);

    for (i=0; i<numRows; i++) {

      CSJSON_MkDir(pJsonOut, "/data/menu", PQgetvalue(result, i, 0), JSON_TYPE_OBJECT);
      sprintf(szPath, "/data/menu/%s", PQgetvalue(result, i, 0));
      CSJSON_InsertString(pJsonOut, szPath, "pid", PQgetvalue(result, i, 1));
      CSJSON_InsertString(pJsonOut, szPath, "caption", PQgetvalue(result, i, 2));
  
      sprintf(szStatement,
              "select mid, oid, otype, caption, config from dcxoption a "
              "join cptcptn b on a.captionid = b.captionid "
              "where a.mid = '%s' and b.langid = '%s' "
              "order by seq", 
              PQgetvalue(result, i, 0),
              lse.szValue);

      result2 = PQexec(conn, szStatement);    

      if (PQresultStatus(result2) == PGRES_TUPLES_OK) {

        numRows2 = PQntuples(result2);

        sprintf(szPath, "/data/menu/%s", PQgetvalue(result, i, 0));
        CSJSON_MkDir(pJsonOut, szPath, "options", JSON_TYPE_ARRAY);

        for (j=0; j<numRows2; j++) {
        
          sprintf(szPath, "/data/menu/%s/options", PQgetvalue(result, i, 0));
          CSJSON_MkDir(pJsonOut, szPath, NULL, JSON_TYPE_OBJECT);
          sprintf(szPath2, "%s/%d", szPath, j);

          CSJSON_InsertString(pJsonOut, szPath2, "oid", PQgetvalue(result2, j, 1));
          CSJSON_InsertString(pJsonOut, szPath2, "type", PQgetvalue(result2, j, 2));

          if (!strcmp(PQgetvalue(result2, j, 2), "*APPLI")) {
            CSJSON_InsertString(pJsonOut, szPath2, "config", PQgetvalue(result2, j, 4));
          }
          else {
            CSJSON_InsertNull(pJsonOut, szPath2, "config");
          }
          
          CSJSON_InsertString(pJsonOut, szPath2, "caption", PQgetvalue(result2, j, 3));
        }
      }

      PQclear(result2);
    }

    CSJSON_MkDir(pJsonOut, "/data", "applications", JSON_TYPE_ARRAY);
  
    pRepo = CFSRPS_Open(0);

    pConfig = CFSRPS_OpenConfig(pRepo, "CSAPAPP/FRAMEWORK/DC-EXPLORER");

    if (pConfig != NULL) {

      CFSCFG_IterStart(pConfig, "APPLICATIONS");

      while ((pszValue = CFSCFG_IterNext(pConfig)) != NULL) {
        CSJSON_InsertString(pJsonOut, "/data/applications", 0, pszValue);
      }

      CFSRPS_CloseConfig(pRepo, &pConfig);
    }

    CFSRPS_Close(&pRepo);

    PQclear(result);
    PQfinish(conn);
  }
  
  return CS_SUCCESS;
}

CSRESULT
  FRMWRK_DisplayMenu
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

/*
  int numRows;
  int i;

  //CSJSON_LSENTRY lse;

  PGresult* result;


  sprintf(appCtl.szStatement,
          "Select realmid From sdrlm");

  result = PQexec(appCtl.conn, appCtl.szStatement);    

  numRows = PQntuples(result);

  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_ARRAY);

  for (i=0; i<numRows; i++) {

    CSJSON_MkDir(pJsonOut, "/data", 0, JSON_TYPE_OBJECT);
    sprintf(appCtl.szRealmPath, "/data/%d", i);

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szRealmPath, 
                        "projectID", 
                        PQgetvalue(result, i, 0));

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szRealmPath, 
                        "desc", 
                        PQgetvalue(result, i, 1));
  }

  PQclear(result);
*/

  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);

  CSJSON_InsertString(pJsonOut, "/data", "option", "DCXPL - Display menu");

  return CS_SUCCESS;
}


CSRESULT
  FRMWRK_Login
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {


  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);

  CSJSON_InsertString(pJsonOut, "/data", "option", "DCXPL - Login");
  
  return CS_SUCCESS;
}


CSRESULT
  FRMWRK_Logout
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {


  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);

  CSJSON_InsertString(pJsonOut, "/data", "option", "DCXPL - Logout");

  return CS_SUCCESS;
}

CSRESULT
  FRMWRK_ChgLanguage
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);

  CSJSON_InsertString(pJsonOut, "/data", "option", "DCXPL - Change Language");

  return CS_SUCCESS;
}


CSRESULT
  FRMWRK_NoOp
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {


  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);

  CSJSON_InsertString(pJsonOut, "/data", "option", "DCXPL - NOOP");

  return CS_SUCCESS;
}
