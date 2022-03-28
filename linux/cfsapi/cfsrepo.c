/* Processed by ecpg (12.9 (Ubuntu 12.9-0ubuntu0.20.04.1)) */
/* These include files are added by the preprocessor */
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
/* End of automatic include section */

#line 1 "./sources/embedded-SQL/cfsrepo.pgc"
/* ===========================================================================
  Clarasoft Foundation Server Repository
  cfsrepo.c
  
  This file contains EMBEDDED SQL instructions for POSTGRE SQL. The steps
  to precompile this file into C code are as follow:

   - On the command line, precompile this file:

      ecpg cfsrepo.pgc  

      This will generate a C source file named cfsrepo.c. The precompiled
      source file will include a set of header files that may need to be 
      qualified to the right include directory; as of this writing, the 
      postgre SQL header files are included in /usr/lib/postgrsql. So 
      you need to correct the generated source with the following inclusions:

        #include <postgresql/ecpglib.h>
        #include <postgresql/ecpgerrno.h>
        #include <postgresql/sqlca.h>

     Alternatively, you ca specify the include path in the gcc command
     (shown next).

   - Compile the generated C source file into an object module:

     gcc -I/usr/include/postgresql -c cfsrepo.c -o cfsrepo

    - When linkng your application with the cfsrepo module, 
      specify the following library:

      -L/usr/local/pgsql/lib -lecpg

      Example compilation and linking (assuming main() is in driver.c):

        gcc -o driver driver.o cfsrepo -L/usr/postgresql/lib -lecpg

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
/*
#include <postgresql/ecpglib.h>
#include <postgresql/ecpgerrno.h>
#include <postgresql/sqlca.h>
*/
    
#include <sys/stat.h>
#include <stdlib.h>
#include <clarasoft/cslib.h>
#include <clarasoft/csjson.h>


#line 1 "/usr/include/postgresql/sqlca.h"
#ifndef POSTGRES_SQLCA_H
#define POSTGRES_SQLCA_H

#ifndef PGDLLIMPORT
#if  defined(WIN32) || defined(__CYGWIN__)
#define PGDLLIMPORT __declspec (dllimport)
#else
#define PGDLLIMPORT
#endif							/* __CYGWIN__ */
#endif							/* PGDLLIMPORT */

#define SQLERRMC_LEN	150

#ifdef __cplusplus
extern "C"
{
#endif

struct sqlca_t
{
	char		sqlcaid[8];
	long		sqlabc;
	long		sqlcode;
	struct
	{
		int			sqlerrml;
		char		sqlerrmc[SQLERRMC_LEN];
	}			sqlerrm;
	char		sqlerrp[8];
	long		sqlerrd[6];
	/* Element 0: empty						*/
	/* 1: OID of processed tuple if applicable			*/
	/* 2: number of rows processed				*/
	/* after an INSERT, UPDATE or				*/
	/* DELETE statement					*/
	/* 3: empty						*/
	/* 4: empty						*/
	/* 5: empty						*/
	char		sqlwarn[8];
	/* Element 0: set to 'W' if at least one other is 'W'	*/
	/* 1: if 'W' at least one character string		*/
	/* value was truncated when it was			*/
	/* stored into a host variable.             */

	/*
	 * 2: if 'W' a (hopefully) non-fatal notice occurred
	 */	/* 3: empty */
	/* 4: empty						*/
	/* 5: empty						*/
	/* 6: empty						*/
	/* 7: empty						*/

	char		sqlstate[5];
};

struct sqlca_t *ECPGget_sqlca(void);

#ifndef POSTGRES_ECPG_INTERNAL
#define sqlca (*ECPGget_sqlca())
#endif

#ifdef __cplusplus
}
#endif

#endif

#line 71 "./sources/embedded-SQL/cfsrepo.pgc"


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

/* exec sql begin declare section */

  //char CFSRPS_LoadConfig_szCfgDomain[33];
  //char CFSRPS_LoadConfig_szCfgSubDomain[33];
  //char CFSRPS_LoadConfig_szCfgName[33];
   
  //char CFSRPS_LoadConfig_szDesc[129];
   
   
   
   
   

      
      


#line 189 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szCfgPath [ 100 ] ;
 
#line 191 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szParam [ 33 ] ;
 
#line 192 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szValue [ 256 ] ;
 
#line 193 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szStorage [ 33 ] ;
 
#line 194 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szFormat [ 33 ] ;
 
#line 195 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szAttr [ 256 ] ;
 
#line 197 "./sources/embedded-SQL/cfsrepo.pgc"
 const char * CFSRPS_LoadConfig_target = "clarasoft@localhost" ;
 
#line 198 "./sources/embedded-SQL/cfsrepo.pgc"
 const char * CFSRPS_LoadConfig_user = "postgres" ;
/* exec sql end declare section */
#line 200 "./sources/embedded-SQL/cfsrepo.pgc"


  struct stat fileInfo;
  char* pConfigBuffer;
  FILE* stream;
  //CSJSON pJson;
  CSRESULT hResult;
  //char szSQLSTATE[10];
  CFSRPS_PARAMINFO pi;

  { ECPGconnect(__LINE__, 0, CFSRPS_LoadConfig_target , CFSRPS_LoadConfig_user , NULL , NULL, 0); }
#line 210 "./sources/embedded-SQL/cfsrepo.pgc"
 // USING :passwd;

  strcpy(This->szPath, configInfo->szPath);
  strcpy(CFSRPS_LoadConfig_szCfgPath, configInfo->szPath);

  { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "select storage , format , attr from public . RPSCFM where path = $1 ", 
	ECPGt_char,(CFSRPS_LoadConfig_szCfgPath),(long)100,(long)1,(100)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_LoadConfig_szStorage),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_LoadConfig_szFormat),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_LoadConfig_szAttr),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 221 "./sources/embedded-SQL/cfsrepo.pgc"



  if (SQLCODE != 0) {
    return CS_FAILURE;
  }

  if (!strcmp(CFSRPS_LoadConfig_szStorage, "*DATABASE")) {

    This->Type = CFSREPO_TYPE_DB;
    This->Format = CFSREPO_FMT_SQL;

  CSMAP_Clear(This->config);

  ECPGset_var( 0, ( CFSRPS_LoadConfig_szCfgPath ), __LINE__);\
 /* declare CFSREPO_3 cursor for select param , value from public . RPSCFP where path = $1  */
#line 240 "./sources/embedded-SQL/cfsrepo.pgc"

#line 240 "./sources/embedded-SQL/cfsrepo.pgc"


  if (SQLCODE == 0) {

     { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare CFSREPO_3 cursor for select param , value from public . RPSCFP where path = $1 ", 
	ECPGt_char,(CFSRPS_LoadConfig_szCfgPath),(long)100,(long)1,(100)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);}
#line 244 "./sources/embedded-SQL/cfsrepo.pgc"


     if (SQLCODE == 0) {

       { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch CFSREPO_3", ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_LoadConfig_szParam),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_LoadConfig_szValue),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 250 "./sources/embedded-SQL/cfsrepo.pgc"


       while (SQLCODE == 0) {

          strcpy(pi.szParam, CFSRPS_LoadConfig_szParam);
          strcpy(pi.szValue, CFSRPS_LoadConfig_szValue);

          CSMAP_Insert(This->config,
                       CFSRPS_LoadConfig_szParam,
                       (void*)(&pi),
                       sizeof(CFSRPS_PARAMINFO));

          { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch CFSREPO_3", ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_LoadConfig_szParam),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_LoadConfig_szValue),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 264 "./sources/embedded-SQL/cfsrepo.pgc"

       }

        { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close CFSREPO_3", ECPGt_EOIT, ECPGt_EORT);}
#line 267 "./sources/embedded-SQL/cfsrepo.pgc"

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
  { ECPGdisconnect(__LINE__, "ALL");}
#line 336 "./sources/embedded-SQL/cfsrepo.pgc"

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

/* exec sql begin declare section */

  //char CFSRPS_IterStart_szCfgDomain[33];
  //char CFSRPS_IterStart_szCfgSubDomain[33];
  //char CFSRPS_IterStart_szCfgName[33];
   
  //char CFSRPS_IterStart_szDesc[129];
   
   

      
      


#line 397 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_IterStart_szCfgPath [ 100 ] ;
 
#line 399 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_IterStart_szParam [ 33 ] ;
 
#line 400 "./sources/embedded-SQL/cfsrepo.pgc"
 char CFSRPS_IterStart_szValue [ 256 ] ;
 
#line 402 "./sources/embedded-SQL/cfsrepo.pgc"
 const char * CFSRPS_IterStart_target = "clarasoft@localhost" ;
 
#line 403 "./sources/embedded-SQL/cfsrepo.pgc"
 const char * CFSRPS_IterStart_user = "postgres" ;
/* exec sql end declare section */
#line 405 "./sources/embedded-SQL/cfsrepo.pgc"


  CFSRPS_PARAMINFO pi;
  CSJSON_DIRENTRY pdire;
  CSJSON_LSENTRY plse;

  char szPath[129];
  long i;

  { ECPGconnect(__LINE__, 0, CFSRPS_IterStart_target , CFSRPS_IterStart_user , NULL , NULL, 0); }
#line 414 "./sources/embedded-SQL/cfsrepo.pgc"
 // USING :passwd;

  CSLIST_Clear(This->enumeration);
  This->CurIndex = 0;

  switch(This->Format) {

    case CFSREPO_FMT_SQL:

      strcpy(CFSRPS_IterStart_szCfgPath, This->szPath);
      strcpy(CFSRPS_IterStart_szParam, szParamName);

      ECPGset_var( 1, ( CFSRPS_IterStart_szParam ), __LINE__);\
 ECPGset_var( 2, ( CFSRPS_IterStart_szCfgPath ), __LINE__);\
 /* declare CFSREPO_4 cursor for select value from public . RPSENM where path = $1  and param = $2  order by seq */
#line 432 "./sources/embedded-SQL/cfsrepo.pgc"

#line 432 "./sources/embedded-SQL/cfsrepo.pgc"


      if (SQLCODE == 0) {

        { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare CFSREPO_4 cursor for select value from public . RPSENM where path = $1  and param = $2  order by seq", 
	ECPGt_char,(CFSRPS_IterStart_szCfgPath),(long)100,(long)1,(100)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_IterStart_szParam),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);}
#line 436 "./sources/embedded-SQL/cfsrepo.pgc"


        if (SQLCODE == 0) {

          { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch CFSREPO_4", ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_IterStart_szValue),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 441 "./sources/embedded-SQL/cfsrepo.pgc"


          while (SQLCODE == 0) {

            pi.fmt = 0;
            pi.type = CFSREPO_PRMFMT_STRING;
            strcpy(pi.szParam, szParamName);
            strcpy(pi.szValue, CFSRPS_IterStart_szValue);

            CSLIST_Insert(This->enumeration,
                       (void*)(&pi),
                       sizeof(CFSRPS_PARAMINFO), CSLIST_BOTTOM);

            { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch CFSREPO_4", ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_IterStart_szValue),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 455 "./sources/embedded-SQL/cfsrepo.pgc"

          }

          { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close CFSREPO_4", ECPGt_EOIT, ECPGt_EORT);}
#line 458 "./sources/embedded-SQL/cfsrepo.pgc"

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

