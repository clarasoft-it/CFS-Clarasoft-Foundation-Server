/* Processed by ecpg (13.1 (Ubuntu 13.1-1.pgdg18.04+1)) */
/* These include files are added by the preprocessor */
#include <ecpglib.h>
#include <ecpgerrno.h>
#include <sqlca.h>
/* End of automatic include section */

#line 1 "./embedded-postgre-sql/cfsrepo.pgc"
/* ===========================================================================
  Clarasoft Foundation Server Repository
  cfsrepo.c

  This file contains EMBEDDED SQL instructions for POSTGRE SQL. The steps
  to precompile this file into C code are as follow:

   - On the command line, precompile this file:

      ecpg cfsrepo.pgc 

      This will generate a C source file named cfsrepo.c. The precompiled
      source file will include a set of header files that need to be 
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

---------------------------------------------------------------------------
  Change history
---------------------------------------------------------------------------

  2020-07-12
  Frederic Soucie
  creation

=========================================================================== */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<clarasoft/cslib.h>


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

#line 74 "./embedded-postgre-sql/cfsrepo.pgc"


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

  CSMAP config;
  char szDomain[33];
  char szSubDomain[33];
  char szName[33];
  char szPath[33];

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

  return Instance;
}

void
  CFSRPS_Destructor
    (CFSRPS** This) {

  CSMAP_Destructor(&((*This)->config));
  free(*This);
}

CSRESULT
  CFSRPS_LoadConfig
    (CFSRPS* This,
     CFSRPS_CONFIGINFO* configInfo) {

/* exec sql begin declare section */

   
   
   


#line 165 "./embedded-postgre-sql/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szCfgPath [ 100 ] ;
 
#line 166 "./embedded-postgre-sql/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szParam [ 33 ] ;
 
#line 167 "./embedded-postgre-sql/cfsrepo.pgc"
 char CFSRPS_LoadConfig_szValue [ 256 ] ;
/* exec sql end declare section */
#line 169 "./embedded-postgre-sql/cfsrepo.pgc"


  CFSRPS_PARAMINFO pi;

  { ECPGconnect(__LINE__, 0, "cfsrepo" , "clara" , NULL , "main", 0); }
#line 173 "./embedded-postgre-sql/cfsrepo.pgc"


  if (SQLCODE != 0) {
    return CS_FAILURE;
  }

  CSMAP_Clear(This->config);

  strcpy(CFSRPS_LoadConfig_szCfgPath, configInfo->szPath);

  ECPGset_var( 0, ( CFSRPS_LoadConfig_szCfgPath ), __LINE__);\
 /* declare C_3 cursor for select param , value from \"RPSCFP\" where path = $1  */
#line 188 "./embedded-postgre-sql/cfsrepo.pgc"

#line 188 "./embedded-postgre-sql/cfsrepo.pgc"


  if (SQLCODE == 0) {

     { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare C_3 cursor for select param , value from \"RPSCFP\" where path = $1 ", 
	ECPGt_char,(CFSRPS_LoadConfig_szCfgPath),(long)100,(long)1,(100)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);}
#line 192 "./embedded-postgre-sql/cfsrepo.pgc"


     if (SQLCODE == 0) {

       { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch C_3", ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_LoadConfig_szParam),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_LoadConfig_szValue),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 198 "./embedded-postgre-sql/cfsrepo.pgc"


       while (SQLCODE == 0) {

          strcpy(pi.szParam, CFSRPS_LoadConfig_szParam);
          strcpy(pi.szValue, CFSRPS_LoadConfig_szValue);

          CSMAP_Insert(This->config,
                       CFSRPS_LoadConfig_szParam,
                       (void*)(&pi),
                       sizeof(CFSRPS_PARAMINFO));

          { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch C_3", ECPGt_EOIT, 
	ECPGt_char,(CFSRPS_LoadConfig_szParam),(long)33,(long)1,(33)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(CFSRPS_LoadConfig_szValue),(long)256,(long)1,(256)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 212 "./embedded-postgre-sql/cfsrepo.pgc"

       }

       { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close C_3", ECPGt_EOIT, ECPGt_EORT);}
#line 215 "./embedded-postgre-sql/cfsrepo.pgc"

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

  { ECPGdisconnect(__LINE__, "ALL");}
#line 229 "./embedded-postgre-sql/cfsrepo.pgc"


  return CS_SUCCESS;
}

CSRESULT
  CFSRPS_LookupParam
    (CFSRPS* This,
     char* szParamName,
     CFSRPS_PARAMINFO* param) {

  CFSRPS_PARAMINFO* ppi;
  long size;

  size = sizeof(CFSRPS_PARAMINFO);
  if (CS_SUCCEED(CSMAP_Lookup(This->config,
                 szParamName, (void**)&ppi, &size))) {
    strcpy(param->szParam, ppi->szParam);
    strcpy(param->szValue, ppi->szValue);
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

/* exec sql begin declare section */

   
   
  //char TLSCFG_LsParam_szParamName[129];
   

   
   
   
  //long TLSCFG_LsParam_availLevel;


#line 263 "./embedded-postgre-sql/cfsrepo.pgc"
 char TLSCFG_LsParam_szCfgName [ 65 ] ;
 
#line 264 "./embedded-postgre-sql/cfsrepo.pgc"
 char TLSCFG_LsParam_szStatement [ 1025 ] ;
 
#line 266 "./embedded-postgre-sql/cfsrepo.pgc"
 char TLSCFG_LsParam_szValue [ 129 ] ;
 
#line 268 "./embedded-postgre-sql/cfsrepo.pgc"
 long TLSCFG_LsParam_param ;
 
#line 269 "./embedded-postgre-sql/cfsrepo.pgc"
 long TLSCFG_LsParam_type ;
 
#line 270 "./embedded-postgre-sql/cfsrepo.pgc"
 long TLSCFG_LsParam_level ;
/* exec sql end declare section */
#line 273 "./embedded-postgre-sql/cfsrepo.pgc"


  TLSCFG_PARAMINFO paramInfo;

  //TLSCFG_PARAMINFO* ppi;

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

  { ECPGprepare(__LINE__, NULL, 0, "stmt_1", TLSCFG_LsParam_szStatement);}
#line 311 "./embedded-postgre-sql/cfsrepo.pgc"


  /* declare C_1 cursor for $1 */
#line 313 "./embedded-postgre-sql/cfsrepo.pgc"


  if (SQLCODE == 0) {

    { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "declare C_1 cursor for $1", 
	ECPGt_char_variable,(ECPGprepared_statement(NULL, "stmt_1", __LINE__)),(long)1,(long)1,(1)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EOIT, ECPGt_EORT);}
#line 317 "./embedded-postgre-sql/cfsrepo.pgc"


    if (SQLCODE == 0) {

      { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch C_1", ECPGt_EOIT, 
	ECPGt_long,&(TLSCFG_LsParam_param),(long)1,(long)1,sizeof(long), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_long,&(TLSCFG_LsParam_type),(long)1,(long)1,sizeof(long), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_long,&(TLSCFG_LsParam_level),(long)1,(long)1,sizeof(long), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(TLSCFG_LsParam_szValue),(long)129,(long)1,(129)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 325 "./embedded-postgre-sql/cfsrepo.pgc"


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

        { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "fetch C_1", ECPGt_EOIT, 
	ECPGt_long,&(TLSCFG_LsParam_param),(long)1,(long)1,sizeof(long), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_long,&(TLSCFG_LsParam_type),(long)1,(long)1,sizeof(long), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_long,&(TLSCFG_LsParam_level),(long)1,(long)1,sizeof(long), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, 
	ECPGt_char,(TLSCFG_LsParam_szValue),(long)129,(long)1,(129)*sizeof(char), 
	ECPGt_NO_INDICATOR, NULL , 0L, 0L, 0L, ECPGt_EORT);}
#line 351 "./embedded-postgre-sql/cfsrepo.pgc"

      }

      { ECPGdo(__LINE__, 0, 1, NULL, 0, ECPGst_normal, "close C_1", ECPGt_EOIT, ECPGt_EORT);}
#line 354 "./embedded-postgre-sql/cfsrepo.pgc"

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

