/* ==========================================================================

  Clarasoft Foundation Server - Linux

  csap test application service
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BUILD:
//
// gcc -g -c sdlc.c -o sdlc.o
// gcc -shared -fPIC -Wl,-soname,libsdlc.so -o libsdlc.so sdlc.o -lc -lcfsapi -lcslib -lcscpt
// sudo cp libsdlc.so /usr/lib/clarasoft
// sudo ldconfig -n /usr/lib/clarasoft
// sudo ln -sf /usr/lib/clarasoft/libsdlc.so /usr/lib/libsdlc.so
//
// Running the service:
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


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

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

typedef struct tagSDLCCTL {

  PGconn* conn;

  char szStatement[4097];
  char szPath[256];
  char szRealmPath[513];
  char szProjectPath[1025];
  char szTaskPath[2051];
  char szLangID[33];

} SDLCCTL;

char szStmt_SELECT_REALMS[] =
       "select a.realmid, b.caption "
       "from sdrlm a join cptcptn b "
       "on a.captionid = b.captionid "
       "where b.langid = '%s' order by realmid";

char szStmt_SELECT_PROJECTS[] =
       "select prjid, descr "
       "from sdprj "
       "where realmid = '%s' order by prjid";

char szStmt_SELECT_TASKS[] =
       "select taskid, title, descr "
       "from sdtsk "
       "where realmid = '%s' and prjid = '%s' order by taskid";

char szStmt_INSERT_PRJ[] =
       "Insert into sdprj "
       "values('%s', '%s', '%s', "
       "'*CFS', current_timestamp, '*CFS', current_timestamp)";

char szStmt_INSERT_TSK[] =
       "Insert into sdtsk "
       "values('%s', '%s', '%s', '%s', '%s', '%s', "
       "'*CFS', current_timestamp, '*CFS', current_timestamp)";

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

SDLCCTL appCtl;

CSRESULT
  SDLC_PRV_LoadData
    (CSJSON pJsonOut) {

  char* pszRealmID;
  char* pszPrjID;
  char* pszTaskID;

  char szLevel[32];
  char szLabel[256];

  int numRows;
  int numRows2;
  int numRows3;

  int i, j, k;

  //CSJSON_LSENTRY lse;

  PGresult* result;
  PGresult* result2;
  PGresult* result3;

  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);
  CSJSON_MkDir(pJsonOut, "/data", "sdlcTree", JSON_TYPE_ARRAY);
  CSJSON_MkDir(pJsonOut, "/data", "webFacing", JSON_TYPE_OBJECT);

  CSJSON_InsertString(pJsonOut, 
                      "/data/webFacing", 
                      "panelID", 
                      "split");

  CSJSON_MkDir(pJsonOut, "/data/webFacing", "html", JSON_TYPE_ARRAY);

  CSJSON_InsertString(pJsonOut, 
                      "/data/webFacing/html", 
                      NULL, 
                      "http://localhost/index.html");

  CSJSON_InsertString(pJsonOut, 
                      "/data/webFacing/html", 
                      NULL, 
                      "sdlc-topic.html");

  CSJSON_InsertString(pJsonOut, 
                      "/data/webFacing", 
                      "script", 
                      "http://localhost/sdlc.js");

  CSJSON_InsertString(pJsonOut, 
                      "/data/webFacing", 
                      "css", 
                      "./sdlc.css");

  sprintf(appCtl.szStatement,
          szStmt_SELECT_REALMS, 
          appCtl.szLangID);

  result = PQexec(appCtl.conn, appCtl.szStatement);    

  if (PQresultStatus(result) != PGRES_TUPLES_OK) {
    PQclear(result);
    return CS_FAILURE;
  }

  numRows = PQntuples(result);

  // Load realms

  for (i=0; i<numRows; i++) {

    CSJSON_MkDir(pJsonOut, "/data/sdlcTree", 0, JSON_TYPE_OBJECT);
    sprintf(appCtl.szRealmPath, "/data/sdlcTree/%d", i);

    pszRealmID = PQgetvalue(result, i, 0);

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szRealmPath, 
                        "key", 
                        pszRealmID);

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szRealmPath, 
                        "label", 
                        PQgetvalue(result, i, 1));

    szLevel[0] = '0';
    szLevel[1] = 0;
    CSJSON_InsertNumeric(pJsonOut, 
                         appCtl.szRealmPath, 
                         "level", 
                         szLevel);

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szRealmPath, 
                        "class", 
                        "REALM");

    CSJSON_MkDir(pJsonOut, appCtl.szRealmPath, "items", JSON_TYPE_ARRAY);

    // Load projects

    sprintf(appCtl.szStatement,
            szStmt_SELECT_PROJECTS,
            pszRealmID);

    result2 = PQexec(appCtl.conn, appCtl.szStatement);    

    if (PQresultStatus(result2) != PGRES_TUPLES_OK) {
      PQclear(result);
      PQclear(result2);
      return CS_FAILURE;
    }

    numRows2 = PQntuples(result2);

    for (j=0; j<numRows2; j++) {

      sprintf(appCtl.szProjectPath, "%s/items", appCtl.szRealmPath);
      CSJSON_MkDir(pJsonOut, appCtl.szProjectPath, 0, JSON_TYPE_OBJECT);
      sprintf(appCtl.szProjectPath, "%s/items/%d",appCtl.szRealmPath, j);

      pszPrjID = PQgetvalue(result2, j, 0);

      CSJSON_InsertString(pJsonOut, 
                          appCtl.szProjectPath, 
                          "key", 
                          pszPrjID);
 
      sprintf(szLabel, "%s - %s", pszPrjID, PQgetvalue(result2, j, 1));

      CSJSON_InsertString(pJsonOut, 
                          appCtl.szProjectPath, 
                          "label", 
                          szLabel);

      szLevel[0] = '1';
      szLevel[1] = 0;

      CSJSON_InsertNumeric(pJsonOut, 
                           appCtl.szProjectPath, 
                           "level", 
                           szLevel);      

      CSJSON_InsertString(pJsonOut, 
                          appCtl.szProjectPath, 
                          "class", 
                          "PROJECT");

      CSJSON_MkDir(pJsonOut, appCtl.szProjectPath, "items", JSON_TYPE_ARRAY);

      // Load projects

      sprintf(appCtl.szStatement,
              szStmt_SELECT_TASKS,
              pszRealmID, pszPrjID);

      result3 = PQexec(appCtl.conn, appCtl.szStatement);    

      if (PQresultStatus(result3) != PGRES_TUPLES_OK) {
        PQclear(result);
        PQclear(result2);
        PQclear(result3);
        return CS_FAILURE;
      }

      // Load tasks

      numRows3 = PQntuples(result3);

      for (k=0; k<numRows3; k++) {

        sprintf(appCtl.szTaskPath, "%s/items", appCtl.szProjectPath);
        CSJSON_MkDir(pJsonOut, appCtl.szTaskPath, 0, JSON_TYPE_OBJECT);
        sprintf(appCtl.szTaskPath, "%s/items/%d",appCtl.szProjectPath, k);

        pszTaskID = PQgetvalue(result3, k, 0);

        CSJSON_InsertString(pJsonOut, 
                            appCtl.szTaskPath, 
                            "key", 
                            pszTaskID);
 
        sprintf(szLabel, "%s - %s", pszTaskID, PQgetvalue(result3, k, 1));

        CSJSON_InsertString(pJsonOut, 
                            appCtl.szTaskPath, 
                            "label", 
                            szLabel);
 
        CSJSON_InsertString(pJsonOut, 
                            appCtl.szTaskPath, 
                            "desc", 
                            PQgetvalue(result3, k, 2));

        szLevel[0] = '2';
        szLevel[1] = 0;
        CSJSON_InsertNumeric(pJsonOut, 
                             appCtl.szTaskPath, 
                             "level", 
                             szLevel);      

        CSJSON_InsertString(pJsonOut, 
                            appCtl.szTaskPath, 
                            "class", 
                            "TASK");

        CSJSON_MkDir(pJsonOut, appCtl.szTaskPath, "items", JSON_TYPE_ARRAY);
      }

      PQclear(result3);
    }

    PQclear(result2);
  }

  PQclear(result);

  return CS_SUCCESS;
}

CSRESULT
  SDLC_EnumRealms
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

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
      
  return CS_SUCCESS;
}

CSRESULT
  SDLC_EnumProjects
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {


/*
  keys: [["k1", "kb"], ["k4", "kb"]],
  data: [["CFSAPI", "*SRVPGM", "CLE", "LMENTOBJ", "FES_BAS"],
         ["CSLIB", "*SRVPGM", "CLE", "LMENTOBJ", "FES_BAS"]],
*/
  int numRows;
  int i;

  CSJSON_LSENTRY lse;

  PGresult* result;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "realmID", &lse))) {
    return CS_FAILURE;
  }

  sprintf(appCtl.szStatement,
          szStmt_SELECT_PROJECTS, 
          lse.szValue);

  result = PQexec(appCtl.conn, appCtl.szStatement);    

  numRows = PQntuples(result);

  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);
  CSJSON_MkDir(pJsonOut, "/data", "keys", JSON_TYPE_ARRAY);
  CSJSON_MkDir(pJsonOut, "/data", "data", JSON_TYPE_ARRAY);

  // Get captions IDs for columns

  for (i=0; i<numRows; i++) {

    CSJSON_MkDir(pJsonOut, "/data/keys", 0, JSON_TYPE_ARRAY);
    sprintf(appCtl.szPath, "/data/keys/%d", i);
    
    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        lse.szValue);
    
    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        PQgetvalue(result, i, 0));

    CSJSON_MkDir(pJsonOut, "/data/data", 0, JSON_TYPE_ARRAY);
    sprintf(appCtl.szPath, "/data/data/%d", i);

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        PQgetvalue(result, i, 0));

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        PQgetvalue(result, i, 1));
  }

  PQclear(result);
      
  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////

CSRESULT
  SDLC_EnumTasks
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pszRealmID;

  int numRows;
  int i;

  CSJSON_LSENTRY lse;

  PGresult* result;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "realmID", &lse))) {
    return CS_FAILURE;
  }

  pszRealmID = lse.szValue;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "prjID", &lse))) {
    return CS_FAILURE;
  }

  sprintf(appCtl.szStatement,
          szStmt_SELECT_TASKS, 
          pszRealmID, lse.szValue);

  result = PQexec(appCtl.conn, appCtl.szStatement);    

  numRows = PQntuples(result);

  CSJSON_MkDir(pJsonOut, "/", "data", JSON_TYPE_OBJECT);
  CSJSON_MkDir(pJsonOut, "/data", "keys", JSON_TYPE_ARRAY);
  CSJSON_MkDir(pJsonOut, "/data", "data", JSON_TYPE_ARRAY);

  for (i=0; i<numRows; i++) {

    CSJSON_MkDir(pJsonOut, "/data/keys", 0, JSON_TYPE_ARRAY);
    sprintf(appCtl.szPath, "/data/keys/%d", i);
    
    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        pszRealmID);
    
    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        lse.szValue);
    
    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        0, 
                        PQgetvalue(result, i, 0));

    CSJSON_MkDir(pJsonOut, "/data/data", 0, JSON_TYPE_ARRAY);
    sprintf(appCtl.szPath, "/data/data/%d", i);

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        "taskID", 
                        PQgetvalue(result, i, 0));

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        "title", 
                        PQgetvalue(result, i, 1));

    CSJSON_InsertString(pJsonOut, 
                        appCtl.szPath, 
                        "desc", 
                        PQgetvalue(result, i, 2));
  }

  PQclear(result);
      
  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////

CSRESULT
  SDLC_CreateProject
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pszRealmID;
  char* pszPrjID;

  CSRESULT hResult;

  CSJSON_LSENTRY lse;

  PGresult* result;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "realmID", &lse))) {
    return CS_FAILURE;
  }

  pszRealmID = lse.szValue;
  
  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "prjID", &lse))) {
    return CS_FAILURE;
  }

  pszPrjID = lse.szValue;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "prjDesc", &lse))) {
    return CS_FAILURE;
  }

  sprintf(appCtl.szStatement,
          szStmt_INSERT_PRJ,
          pszRealmID, pszPrjID, lse.szValue);

  result = PQexec(appCtl.conn, appCtl.szStatement);
        
  if (PQresultStatus(result) != PGRES_COMMAND_OK) {
    PQclear(result);    
    hResult = CS_FAILURE;
  }
  else {
    PQclear(result);    
    hResult = SDLC_PRV_LoadData(pJsonOut);
  }
  
  return hResult;
}

//////////////////////////////////////////////////////////////

CSRESULT
  SDLC_CreateTask
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  char* pszRealmID;
  char* pszPrjID;
  char* pszTskID;
  char* pszTskTitle;
  char* pszTskDesc;

  CSRESULT hResult;

  CSJSON_LSENTRY lse;

  PGresult* result;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "realmID", &lse))) {
    return CS_FAILURE;
  }

  pszRealmID = lse.szValue;
  
  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "prjID", &lse))) {
    return CS_FAILURE;
  }

  pszPrjID = lse.szValue;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "taskID", &lse))) {
    return CS_FAILURE;
  }

  pszTskID = lse.szValue;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "taskTitle", &lse))) {
    return CS_FAILURE;
  }

  pszTskTitle = lse.szValue;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "taskDesc", &lse))) {
    return CS_FAILURE;
  }

  pszTskDesc = lse.szValue;

  if (CS_FAIL(CSJSON_LookupKey(pJsonIn, "/data", "taskRef", &lse))) {
    return CS_FAILURE;
  }

  sprintf(appCtl.szStatement,
          szStmt_INSERT_TSK,
          pszRealmID, pszPrjID, pszTskID, pszTskTitle, pszTskDesc, lse.szValue);

  result = PQexec(appCtl.conn, appCtl.szStatement);
        
  if (PQresultStatus(result) != PGRES_COMMAND_OK) {
    PQclear(result);    
    hResult = CS_FAILURE;
  }
  else {
    PQclear(result);    
    hResult = SDLC_PRV_LoadData(pJsonOut);
  }
  
  return hResult;
}

//////////////////////////////////////////////////////////////

CSRESULT
  SDLC_Init
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  CSJSON_LSENTRY lse;

  if (CS_SUCCEED(CSJSON_LookupKey(pJsonIn, "/ctl", "langID", &lse))) {

    // Connect to database

    appCtl.conn = PQconnectdb("dbname=cfsrepo");

    if (PQstatus(appCtl.conn) == CONNECTION_BAD) {
    
      PQfinish(appCtl.conn);
      return CS_FAILURE;
    }

    strncpy(appCtl.szLangID, lse.szValue, 32);
    appCtl.szLangID[32] = 0;
    return SDLC_PRV_LoadData(pJsonOut);
  }

  return CS_FAILURE;
}

CSRESULT  SDLC_DisplayTask
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {
      
  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////

void
  SDLC_Exit
    (CSJSON pJsonIn,
     CSJSON pJsonOut) {

  PQfinish(appCtl.conn);
}


