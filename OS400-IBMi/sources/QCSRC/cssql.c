/* ===========================================================================
  Clarasoft Foundation Server for AS400

  cssql.c
  SQL class
  Version 1.0.0

  Distributed under the MIT license
  Copyright (c) 2013 Clarasoft I.T. Solutions Inc.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "qcsrc_cfs/cslib.h"

Exec SQL Set Option CLOSQLCSR = *ENDMOD;

EXEC SQL INCLUDE SQLCA;
EXEC SQL INCLUDE SQLDA;

#define CSSQL_FMT_LTRIM 0x01
#define CSSQL_FMT_RTRIM 0x02
#define CSSQL_FMT_TRIM  0x03

#define CSSQL_MAX_FIELDSIZE_VARBINARY   32767
#define CSSQL_MAX_FIELDSIZE_VARCHAR     32767
#define CSSQL_MAX_FIELDSIZE_VARGRAPHIC  32767

typedef struct tagCSSQL {

EXEC SQL BEGIN DECLARE SECTION;

  struct sqlda* pSQLDA;
  short pNULLIND[512];

EXEC SQL END DECLARE SECTION;

  struct sqlvar* pColumns;

  long cursorState;

} CSSQL;

typedef struct tagCSSQLINFO {

   long numColumns;
   long sqlcode;
   char sqlstate[6];
   long recordSize;

} CSSQLINFO;

CSSQL*
  CSSQL_Constructor
    (void);

void
  CSSQL_Destructor
    (CSSQL** This);

CSRESULT
  CSSQL_OpenCursor
    (CSSQL* This,
     char* szStatement,
     struct sqlvar** pColumns,
     CSSQLINFO* sqlInfo);

CSRESULT
  CSSQL_FetchCursor
    (CSSQL* This,
     CSSQLINFO* sqlInfo);

CSRESULT
  CSSQL_CloseCursor
    (CSSQL* This);

CSRESULT
  ConvertPackedToString
    (char* in,
     char* out,
     int precision,
     int scale,
     int mode);

CSRESULT
  ConvertZonedToString
    (char* in,
     char* out,
     int precision,
     int scale,
     int mode);


CSSQL*
  CSSQL_Constructor
    (void) {

  CSSQL* Instance;

  Instance = (CSSQL*)malloc(sizeof(CSSQL));

  Instance->pSQLDA = (struct sqlda*)malloc
           (sizeof(struct sqlda) + 511 * (sizeof(struct sqlvar)));

  Instance->cursorState = 0;
  Instance->pColumns = &(Instance->pSQLDA->sqlvar[0]);

  return Instance;
}

void
  CSSQL_Destructor
    (CSSQL** This) {

  free((*This)->pSQLDA);

  free(*This);
}

CSRESULT
  CSSQL_OpenCursor
    (CSSQL* This,
     char* szStatement,
     struct sqlvar** pColumns,
     CSSQLINFO* sqlInfo) {

  struct sqlda* pSQLDA;
  short size;
  long i;

EXEC SQL BEGIN DECLARE SECTION;

  char pszStatement[4097];

EXEC SQL END DECLARE SECTION;

  if (This->cursorState == 1) {

    for (i=0; i<This->pSQLDA->sqld; i++) {

      // Release the result buffers
      free((This->pSQLDA->sqlvar[i]).sqldata);
    }

    This->cursorState = 0;
  }

  sqlInfo->recordSize = 0;

  pSQLDA = This->pSQLDA;
  pSQLDA->sqln = pSQLDA->sqld = 512;
  memcpy(pSQLDA->sqldaid, "SQLDA   ", 8);

  strcpy(pszStatement, szStatement);
  Exec SQL Prepare S_STMT From : pszStatement;

  if (sqlca.sqlcode != 0) {
    sqlInfo->sqlcode = sqlca.sqlcode;
    memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
    sqlInfo->numColumns = 0;
    return CS_FAILURE;
  }

  Exec SQL DESCRIBE S_STMT Into :*pSQLDA;

  if (sqlca.sqlcode != 0) {
    sqlInfo->sqlcode = sqlca.sqlcode;
    memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
    sqlInfo->numColumns = 0;
    return CS_FAILURE;
  }

  for (i=0; i<pSQLDA->sqld; i++) {

    switch((pSQLDA->sqlvar[i]).sqltype) {

       case 384:
       case 385:
       case 388:
       case 389:
       case 392:
       case 393:
       case 396:
       case 397:
       case 452:
       case 453:
       case 468:
       case 469:

         // DATE
         // TIME
         // TIMESTAMP
         // DATA LINK
         // CHAR
         // GRAPHIC

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           ((pSQLDA->sqlvar[i]).sqllen * sizeof(unsigned char) + 1);

         (pSQLDA->sqlvar[i]).sqldata[(pSQLDA->sqlvar[i]).sqllen] = 0;

         break;

       case 456:
       case 457:

         // LONG VARCHAR

         // add 2 bytes for size attribute and 1 for NULL
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (CSSQL_MAX_FIELDSIZE_VARCHAR * sizeof(unsigned char) + 3);

         (pSQLDA->sqlvar[i]).sqldata[CSSQL_MAX_FIELDSIZE_VARCHAR + 2] = 0;


       case 464:
       case 465:

         // GRAPHIC VARCHAR

         // add 2 bytes for size attribute and 1 for NULL
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (CSSQL_MAX_FIELDSIZE_VARGRAPHIC + 3);

         (pSQLDA->sqlvar[i]).sqldata[CSSQL_MAX_FIELDSIZE_VARGRAPHIC + 2] = 0;

         break;

       case 472:
       case 473:

         // LONG GRAPHIC VARCHAR

         // add 2 bytes for size attribute and 1 for NULL
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (CSSQL_MAX_FIELDSIZE_VARGRAPHIC * sizeof(unsigned char) + 3);

         (pSQLDA->sqlvar[i]).sqldata[CSSQL_MAX_FIELDSIZE_VARGRAPHIC + 2] = 0;

         break;

       case 448:
       case 449:

         // VARCHAR

         // add 2 bytes for size attribute and 1 for NULL
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (CSSQL_MAX_FIELDSIZE_VARCHAR * sizeof(unsigned char) + 3);

         (pSQLDA->sqlvar[i]).sqldata[CSSQL_MAX_FIELDSIZE_VARCHAR + 2] = 0;

         break;

       case 400:
       case 401:
       case 460:
       case 461:

         // This will be a NULL-terminated string

         // add 1 for NULL
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
                    ((CSSQL_MAX_FIELDSIZE_VARCHAR + 1) * sizeof(char));

         (pSQLDA->sqlvar[i]).sqllen = CSSQL_MAX_FIELDSIZE_VARCHAR;

         (pSQLDA->sqlvar[i]).sqldata[CSSQL_MAX_FIELDSIZE_VARCHAR] = 0;

         break;

       case 404:
       case 405:
       case 408:
       case 409:
       case 412:
       case 413:
       case 476:
       case 477:

         // LOB
         (pSQLDA->sqlvar[i]).sqldata = NULL;
         break;

       case 480:
       case 481:

         // FLOAT/DOUBLE
         if ((pSQLDA->sqlvar[i]).sqllen == 4) {
           (pSQLDA->sqlvar[i]).sqldata =
                   (unsigned char*)malloc(4 * sizeof(unsigned  char));
         }
         else {
           (pSQLDA->sqlvar[i]).sqldata =
                   (unsigned char*)malloc(8 * sizeof(unsigned char));
         }

         break;

       case 484:
       case 485:
       case 488:
       case 489:

         // Packed decimal;
         // Zoned decimal
         // two more for decimal point and sign

         size = ((pSQLDA->sqlvar[i]).sqllen >> 8) * sizeof(unsigned char) + 3;
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (size * sizeof(unsigned char));

         (pSQLDA->sqlvar[i]).sqldata[size] = 0;

         break;

       case 492:
       case 493:

         // long long integer

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           ((pSQLDA->sqlvar[i]).sqllen * sizeof(long long));

         break;

       case 496:
       case 497:

         // long integer

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           ((pSQLDA->sqlvar[i]).sqllen * sizeof(long));

         break;

       case 500:
       case 501:

         // short integer

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           ((pSQLDA->sqlvar[i]).sqllen * sizeof(short int));

         break;

       case 504:
       case 505:
         break;
       case 904:
       case 905:

         // ROWID

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           ((pSQLDA->sqlvar[i]).sqllen * sizeof(unsigned char) + 1);

         (pSQLDA->sqlvar[i]).sqldata[(pSQLDA->sqlvar[i]).sqllen] = 0;

         break;

       case 908:
       case 909:

         // VAR BINARY STRING

         // add 2 bytes for size attribute
         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (CSSQL_MAX_FIELDSIZE_VARBINARY * sizeof(unsigned char) + 3);

         break;

       case 912:
       case 913:

         // BINARY STRING

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (CSSQL_MAX_FIELDSIZE_VARBINARY * sizeof(unsigned char) + 1);

         break;

       case 916:
       case 917:
       case 920:
       case 921:
       case 924:
       case 925:

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (267 * sizeof(unsigned char) + 1);

         (pSQLDA->sqlvar[i]).sqldata[(pSQLDA->sqlvar[i]).sqllen] = 0;

         break;

       case 960:
       case 961:
       case 964:
       case 965:
       case 968:
       case 969:

         // LOB LOCATOR

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (4 * sizeof(unsigned char) + 1);

         (pSQLDA->sqlvar[i]).sqldata[(pSQLDA->sqlvar[i]).sqllen] = 0;

         break;

       case 972:

         (pSQLDA->sqlvar[i]).sqldata = NULL;
         break;

       case 988:
       case 989:

         (pSQLDA->sqlvar[i]).sqldata = NULL;
         break;

       case 996:
       case 997:

         // DECFLOAT

         (pSQLDA->sqlvar[i]).sqldata =
           (unsigned char*)malloc
           (16 * sizeof(unsigned char) + 1);

         (pSQLDA->sqlvar[i]).sqldata[(pSQLDA->sqlvar[i]).sqllen] = 0;

         break;

       case 2452:
       case 2453:

         (pSQLDA->sqlvar[i]).sqldata = NULL;
         break;

       default:

         if ((pSQLDA->sqlvar[i]).sqllen > 0) {
           (pSQLDA->sqlvar[i]).sqldata =
             (unsigned char*)malloc
             ((pSQLDA->sqlvar[i]).sqllen * sizeof(unsigned char) + 1);

           sqlInfo->recordSize += ((pSQLDA->sqlvar[i]).sqllen + 1);
           (pSQLDA->sqlvar[i]).sqldata[(pSQLDA->sqlvar[i]).sqllen] = 0;
         }
         else {
           (pSQLDA->sqlvar[i]).sqldata = NULL;
         }

         break;
    }

    // set nul indicator
    (pSQLDA->sqlvar[i]).sqlind = &(This->pNULLIND)[i];
  }

  This->cursorState = 1;

  Exec SQL Declare C_CURSOR Cursor For S_STMT;

  if (sqlca.sqlcode == 0) {

    Exec SQL Open C_CURSOR;

    if (sqlca.sqlcode == 0) {

      sqlInfo->sqlcode = sqlca.sqlcode;
      memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
      sqlInfo->numColumns = pSQLDA->sqld;
      *pColumns = This->pColumns;

      return CS_SUCCESS;
    }
    else {
      sqlInfo->sqlcode = sqlca.sqlcode;
      memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
      sqlInfo->numColumns = pSQLDA->sqld;
    }
  }
  else {
    sqlInfo->sqlcode = sqlca.sqlcode;
    memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
    sqlInfo->numColumns = 0;
  }

  *pColumns = 0;
  return CS_FAILURE;
}

CSRESULT
  CSSQL_FetchCursor
    (CSSQL* This,
     CSSQLINFO* sqlInfo) {

  struct sqlda* pSQLDA;

  pSQLDA = This->pSQLDA;

  Exec SQL Fetch C_CURSOR Using Descriptor :*pSQLDA;

  if (sqlca.sqlcode == 0) {

    sqlInfo->sqlcode = sqlca.sqlcode;
    memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
    sqlInfo->numColumns = pSQLDA->sqld;

    return CS_SUCCESS;
  }

  sqlInfo->sqlcode = sqlca.sqlcode;
  memcpy(sqlInfo->sqlstate, sqlca.sqlstate, 6);
  sqlInfo->numColumns = 0;

  return CS_FAILURE;
}

CSRESULT
  CSSQL_CloseCursor
    (CSSQL* This) {

  long i;

  if (This->cursorState == 1) {

    Exec SQL Close C_CURSOR;

    for (i=0; i<This->pSQLDA->sqld; i++) {

      // Release the result buffers
      free((This->pSQLDA->sqlvar[i]).sqldata);
    }

    This->cursorState = 0;
  }

  return CS_SUCCESS;
}

/*
  Low-order byte contains a digit and the sign:

    DDDDSSSS

    Low-order nibble is the sign: a value of F means the number is positive.
    So we test ((in[] & 0x0F) == 0x0F). If true, then number ispositive.

    The high order nibble is a digit.
*/

CSRESULT ConvertPackedToString(char* in, char* out, int precision, int scale, int mode) {

  long i, j, k;

  long number;
  long digit;
  long offset;
  long nBytes;
  long trim;

  char buffer[256];

  j=0;
  if (precision > 0) {

    // determine sign
    nBytes = (precision+1)/2;
    if ((in[nBytes-1] & 0x0F) != 0x0F) {
      buffer[0] = '-';
      j++;
    }

    // set digits
    for (offset=0, i=0, k=0; k<precision-scale; j++, k++) {

      if (offset == 0) {
        // Extract high-order nibble
        digit = ((in[i] >> 4) | 0xF0);
        buffer[j] = digit;
        offset = 1;
      }
      else {
        // Extract low-order nibble
        digit = (in[i] | 0xF0);
        buffer[j] = digit;
        offset = 0;
        i++;  // advance to next packed byte
      }
    }

    // set dot

    if (scale > 0) {
      buffer[j] = '.';
      j++;

      // set decimals
      for (k=0; k<scale; j++, k++) {

        if (offset == 0) {
          // Extract high-order nibble
          digit = ((in[i] >> 4) | 0xF0);
          buffer[j] = digit;
          offset = 1;
        }
        else {
          // Extract low-order nibble
          digit = (in[i] | 0xF0);
          buffer[j] = digit;
          offset = 0;
          i++;  // advance to next packed byte
        }
      }
    }
  }

  buffer[j] = 0;

  if (mode == CSSQL_FMT_TRIM || mode == CSSQL_FMT_LTRIM) {
    trim = precision-scale-1;
    for (i=0; i<trim; i++) {
      if (buffer[i] != '0') {
        break;
      }
    }
  }

  for (j=0; buffer[i] != 0; i++, j++) {
    out[j] = buffer[i];
  }

  out[j] = 0;

  return CS_SUCCESS;
}


CSRESULT ConvertZonedToString(char* in, char* out, int precision, int scale, int mode) {

  long i, j, k;

  long number;
  long digit;
  long offset;
  long trim;

  char buffer[256];

  j=0;
  if (precision > 0) {

    // determine sign
    if ((in[precision-1] & 0xF0) != 0xF0) {
      buffer[0] = '-';
      j++;
    }

    // set digits
    for (i=0, k=0; k<precision-scale; j++, k++) {

      // Extract low-order nibble
      digit = (in[i] | 0xF0);
      buffer[j] = digit;
      offset = 0;
      i++;  // advance to next packed byte
    }

    // set dot

    if (scale > 0) {
      buffer[j] = '.';
      j++;

      // set decimals
      for (k=0; k<scale; j++, k++) {

        // Extract low-order nibble
        digit = (in[i] | 0xF0);
        buffer[j] = digit;
        offset = 0;
        i++;  // advance to next packed byte
      }
    }
  }

  buffer[j] = 0;

  if (mode == CSSQL_FMT_TRIM || mode == CSSQL_FMT_LTRIM) {
    trim = precision-scale-1;
    for (i=0; i<trim; i++) {
      if (buffer[i] != '0') {
        break;
      }
    }
  }

  for (j=0; buffer[i] != 0; i++, j++) {
    out[j] = buffer[i];
  }

  out[j] = 0;

  return CS_SUCCESS;
}
 