/* ==========================================================================

  Clarasoft JSON Object - OS/400
  csjson.c

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

========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcsrc/cslib.h"

#define JSON_SERIALIZE_SLAB  65536

#define JSON_UNICODE_NOCONVERT   0
#define JSON_UNICODE_CONVERT     1

#define JSON_TOK_BOOL_FALSE      0
#define JSON_TOK_BOOL_TRUE       1
#define JSON_TOK_NULL            3
#define JSON_TOK_LBRACE         10
#define JSON_TOK_RBRACE         11
#define JSON_TOK_LBRACKET       20
#define JSON_TOK_RBRACKET       21
#define JSON_TOK_COLON          30
#define JSON_TOK_COMMA          31
#define JSON_TOK_NUMERIC        51
#define JSON_TOK_STRING         52

#define JSON_TYPE_BOOL_FALSE     0
#define JSON_TYPE_BOOL_TRUE      1
#define JSON_TYPE_NULL           3
#define JSON_TYPE_OBJECT        10
#define JSON_TYPE_ARRAY         20
#define JSON_TYPE_NUMERIC       51
#define JSON_TYPE_STRING        52
#define JSON_TYPE_UNKNOWN       99

#define JSON_PATH_SEP       '\x1B'

typedef struct tagCSJSON
{
  CSLIST Tokens;
  CSLIST unicodeTokens;

  void*  pIterListing;

  CSMAP Object;

  long slabSize;
  long nextSlabSize;
  long iterIndex;
  long iterType;

  char* szSlab;

} CSJSON;

typedef struct tagCSJSON_TOKENINFO
{
  int  type;
  long size;
  char *szToken;

} CSJSON_TOKENINFO;

typedef struct tagCSJSON_DIRENTRY
{
  int    type;
  long   numItems;
  CSLIST Listing;

} CSJSON_DIRENTRY;

typedef struct tagCSJSON_LSENTRY
{
  int   type;
  char* szKey;
  long  keySize;
  char* szValue;
  long  valueSize;

} CSJSON_LSENTRY;

/* ---------------------------------------------------------------------------
 * private methods
 * -------------------------------------------------------------------------*/

long
  CSJSON_PRIVATE_Serialize
    (CSJSON* This,
     char* szPath,
     char** szOutStream,
     long* curPos);

CSRESULT
  CSJSON_PRIVATE_O
    (CSJSON* This,
     long*  index,
     char*  szPath,
     long   len);

CSRESULT
  CSJSON_PRIVATE_A
    (CSJSON* This,
     long*  index,
     char*  szPath,
     long   len);

CSRESULT
  CSJSON_PRIVATE_VV
    (CSJSON* This,
     long*  index,
     char*  szPath,
     long len,
     CSLIST  listing);

CSRESULT
  CSJSON_PRIVATE_IsNumeric
    (char* szNumber);

/* ---------------------------------------------------------------------------
 * implementation
 * ------------------------------------------------------------------------ */

CSJSON*
  CSJSON_Constructor
    (void) {

  CSJSON *Instance;

  Instance = (CSJSON *)malloc(sizeof(CSJSON));

  Instance->Tokens = CSLIST_Constructor();
  Instance->unicodeTokens = CSLIST_Constructor();
  Instance->Object = CSMAP_Constructor();

  Instance->szSlab = (char*)malloc(JSON_SERIALIZE_SLAB * sizeof(char));

  Instance->slabSize = JSON_SERIALIZE_SLAB;
  Instance->nextSlabSize = 0;
  Instance ->iterIndex = 0;
  Instance->iterType = JSON_TYPE_UNKNOWN;

  Instance->pIterListing = NULL;

  return Instance;
}

CSRESULT
  CSJSON_Destructor
    (CSJSON **This) {

  char *pszKey;

  long valueSize;
  long count;
  long size;
  long i;

  CSJSON_DIRENTRY* pdire;
  CSJSON_LSENTRY* plse;

  if (This == 0 || *This == NULL) {
    return CS_FAILURE;
  }

  // Cleanup the previously built JSON object

  CSMAP_IterStart((*This)->Object, CSMAP_ASCENDING);

  while (CS_SUCCEED(CSMAP_IterNext((*This)->Object, &pszKey,
                                    (void **)(&pdire), &valueSize)))
  {
    if (pdire->Listing != 0)
    {
      if (pdire->type == JSON_TYPE_ARRAY) {

        count = CSLIST_Count(pdire->Listing);

        for (i=0; i<count; i++) {
          CSLIST_GetDataRef(pdire->Listing, (void**)(&plse), i);
          free(plse->szKey);
          free(plse->szValue);
        }

        CSLIST_Destructor(&(pdire->Listing));
      }
      else {

        CSMAP_IterStart(pdire->Listing, CSMAP_ASCENDING);

        while(CS_SUCCEED(CSMAP_IterNext(pdire->Listing, &pszKey,
                                        (void**)(&plse), &size))) {
          free(plse->szKey);
          free(plse->szValue);
        }

        CSMAP_Destructor(&(pdire->Listing));
      }
    }
  }

  CSMAP_Destructor(&((*This)->Object));

  CSLIST_Destructor(&((*This)->Tokens));
  CSLIST_Clear((*This)->unicodeTokens);
  CSLIST_Destructor(&((*This)->unicodeTokens));

  // Cleanup other allocations

  if ((*This)->szSlab) {
    free((*This)->szSlab);
  }

  free(*This);

  *This = 0;

  return CS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_PRIVATE_Tokenize
//
// Breaks down a string into individual tokens. Some validations are done
// at this point such as unicode point conversion to UTF-8 checking for valid
// numeric values, proper control character escape sequences, etc.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_PRIVATE_Tokenize
    (CSJSON *This,
     char *szJsonString) {

  int Stop;
  int haveDot;
  int haveExp;

  long n;
  long k;
  long t;
  long tok_i;
  long tok_j;
  long cpyStart;
  long cpyLen;
  long curTokenIndex;
  long tempIndex;
  long startToken;
  long unicodeTokenCount;
  long unicodePoint;
  long tokenOffset;

  char szBoolBuffer[6];

  char* pCurByte;

  CSJSON_TOKENINFO ti;

  struct UNICODETOKEN {
    long cpyIndex;
    char token[5];
  } unicodeToken;

  n = 0;
  tokenOffset = 0;
  pCurByte = szJsonString;

  while (pCurByte[n] != 0)
  {
    switch (szJsonString[n])
    {

    case ',':

      ti.szToken = 0;
      ti.type = JSON_TOK_COMMA;

      CSLIST_Insert(This->Tokens, &ti,
                    sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
      tokenOffset++;

      break;

    case '{':

      ti.szToken = 0;
      ti.type = JSON_TOK_LBRACE;

      CSLIST_Insert(This->Tokens, &ti,
                    sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
      tokenOffset++;

      break;

    case '}':

      ti.szToken = 0;
      ti.type = JSON_TOK_RBRACE;

      CSLIST_Insert(This->Tokens, &ti,
                    sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
      tokenOffset++;

      break;

    case '[':

      ti.szToken = 0;
      ti.type = JSON_TOK_LBRACKET;

      CSLIST_Insert(This->Tokens, &ti,
                    sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
      tokenOffset++;

      break;

    case ']':

      ti.szToken = 0;
      ti.type = JSON_TOK_RBRACKET;

      CSLIST_Insert(This->Tokens, &ti,
                    sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
      tokenOffset++;

      break;

    case ':':

      ti.szToken = 0;
      ti.type = JSON_TOK_COLON;

      CSLIST_Insert(This->Tokens, &ti,
                    sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
      tokenOffset++;

      break;

    case '"':

      Stop = 0;
      tempIndex = 0;
      n++;
      startToken = n;

      CSLIST_Clear(This->unicodeTokens);

      while (pCurByte[n] != 0 && !Stop)
      {
        ////////////////////////////////////////////////////////
        //    We need to detect escaped characters;
        ////////////////////////////////////////////////////////

        switch (szJsonString[n])
        {
          case '\\':

            tempIndex++;
            n++; // examine next character

            if (szJsonString[n] >= 0 && szJsonString[n] <= 0x1F) {
              // Legal escape sequence
              tempIndex++;
              n++; // examine next character
            }
            else {

              switch(szJsonString[n]) {

              case '/':
              case '\\':
              case 't':
              case 'r':
              case 'n':
              case 'b':
              case 'f':
              case '"':

                  // Legal escape sequence
                  tempIndex++;
                  n++; // examine next character
                  break;

                case 'u':

                  // UNICODE escaped code point; next 4 characters must be numeric

                  tempIndex++;
                  n++; // examine next character
                  unicodeToken.cpyIndex = n;

                  for (k=0; k<4; k++) {

                    if (!((szJsonString[n] >= '0' && szJsonString[n] <= '9') ||
                          (szJsonString[n] >= 'a' && szJsonString[n] <= 'f') ||
                          (szJsonString[n] >= 'A' && szJsonString[n] <= 'F'))) {

                      goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                    }

                    unicodeToken.token[k] = szJsonString[n];
                    tempIndex++;
                    n++; // examine next character
                  }

                  unicodeToken.token[k] = 0;

                  CSLIST_Insert(This->unicodeTokens,
                         &unicodeToken, sizeof(unicodeToken), CSLIST_BOTTOM);

                  break;

                default:
                  goto CSJSON_PRIVATE_TOKENIZE_ERROR;
              }
            }

            break;

          case '"':

            Stop = 1;
            break;

          default:

            if (szJsonString[n] >= 0 && szJsonString[n] <= 0x1F) {
                // non-escaped control character
                goto CSJSON_PRIVATE_TOKENIZE_ERROR;
            }
            else {
              // character is valid
              tempIndex++;
              n++; // examine next character
            }

            break;
        }
      }

      if (szJsonString[n] == '"')
      {
        // We copy the buffer as a NULL-terminated string; to transfer binary
        // data in a JSON string, one should encode the data in BASE-64. In this case,
        // we might allocate more than needed but this is ok.

        ti.szToken = (char*)malloc(tempIndex * sizeof(char) + 1);

        // We convert unicode escape sequences ... if any

        unicodeTokenCount = CSLIST_Count(This->unicodeTokens);

        if (unicodeTokenCount > 0) {

          ti.size = 0;
          cpyStart = startToken;
          curTokenIndex = 0;

          for (t=0; t<unicodeTokenCount; t++) {

            CSLIST_Get(This->unicodeTokens, (void*)&unicodeToken, t);

            cpyLen = unicodeToken.cpyIndex - cpyStart - 2;

            if (cpyLen > 0) {
              memcpy(&(ti.szToken[curTokenIndex]), &szJsonString[cpyStart], cpyLen);
              curTokenIndex += cpyLen;
              ti.size += cpyLen;
            }

            cpyStart = unicodeToken.cpyIndex + 4;

            // Convert code point text to integer

            unicodePoint = strtol(unicodeToken.token, 0, 16);

            // convert code point to utf8 and copy to result buffer

            if (unicodePoint >= 0x00000000 && unicodePoint <=0x0000007F) {
              // 1 byte
              ti.szToken[curTokenIndex] = (char)(unicodePoint & 0x0000007F);
              curTokenIndex += 1;
              ti.size += 1;
            }
            else {
              if (unicodePoint >= 0x00000080 && unicodePoint <= 0x000007FF) {
                // 2 bytes
                ti.szToken[curTokenIndex]   = (char)((unicodePoint >> 6 & 0x0000001F) | 0x000000C0);
                ti.szToken[curTokenIndex+1] = (char)((unicodePoint & 0x0000003F) | 0x00000080);
                curTokenIndex+=2;
                ti.size += 2;
              }
              else {
                if (unicodePoint >= 0x00000800 && unicodePoint <=0x0000FFFF) {
                  // 3 bytes
                  ti.szToken[curTokenIndex]   = (char)((unicodePoint >> 12 & 0x0000000F) | 0x000000E0);
                  ti.szToken[curTokenIndex+1] = (char)((unicodePoint >> 6  & 0x0000003F) | 0x00000080);
                  ti.szToken[curTokenIndex+2] = (char)((unicodePoint & 0x0000003F) | 0x00000080);
                  curTokenIndex += 3;
                  ti.size += 3;
                }
                else {
                  if (unicodePoint >= 0x00010000 && unicodePoint <=0x001FFFFF) {
                    // 4 bytes
                    ti.szToken[curTokenIndex]   = (char)((unicodePoint >> 18 & 0x00000007) | 0x000000F0);
                    ti.szToken[curTokenIndex+1] = (char)((unicodePoint >> 12 & 0x0000003F) | 0x00000080);
                    ti.szToken[curTokenIndex+2] = (char)((unicodePoint >> 6  & 0x0000003F) | 0x00000080);
                    ti.szToken[curTokenIndex+3] = (char)((unicodePoint & 0x0000003F) | 0x00000080);
                    curTokenIndex += 4;
                    ti.size += 4;
                  }
                  else {
                    // invalid code point
                    free(ti.szToken);
                    goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                  }
                }
              }
            }
          }

          // Copy tail end of the token
          cpyLen = n - cpyStart;
          if (cpyLen > 0) {
            memcpy(&(ti.szToken[curTokenIndex]), &szJsonString[cpyStart], cpyLen);
            ti.size += cpyLen;
          }

          ti.size += 1;
        }
        else {
          ti.size = tempIndex + 1;
          memcpy(ti.szToken, &(szJsonString[startToken]), tempIndex);
        }

        ti.szToken[ti.size-1] = 0;

        // We must clean up escape characters (\)
        // note that the token size includes the NULL.

        for (tok_i=0, tok_j=0; tok_i<ti.size; tok_i++, tok_j++) {

          if (ti.szToken[tok_i] == '\\') {

            // if this is not a unicode escape sequence ...

            if (ti.szToken[tok_i+1] != 'u') {

              tok_i++;
              switch(ti.szToken[tok_i]) {
                case '\\':
                  ti.szToken[tok_j] = '\\';
                  break;
                case '"':
                  ti.szToken[tok_j] = '"';
                  break;
                case '/':
                  ti.szToken[tok_j] = '/';
                  break;
                case 'b':
                  ti.szToken[tok_j] = '\b';
                  break;
                case 'f':
                  ti.szToken[tok_j] = '\f';
                  break;
                case 'n':
                  ti.szToken[tok_j] = '\n';
                  break;
                case 'r':
                  ti.szToken[tok_j] = '\r';
                  break;
                case 't':
                  ti.szToken[tok_j] = '\t';
                  break;
                default:
                  ti.szToken[tok_j] = ti.szToken[tok_i];
                  break;
              }
            }
          }
          else {
            ti.szToken[tok_j] = ti.szToken[tok_i];
          }
        }

        ti.size = tok_j;

        ti.type = JSON_TOK_STRING;

        CSLIST_Insert(This->Tokens, &ti,
                      sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
        tokenOffset++;
      }
      else
      {
        goto CSJSON_PRIVATE_TOKENIZE_ERROR;
      }

      break;

    default:

      if ((szJsonString[n] >= '0' && szJsonString[n] <= '9') ||
          (szJsonString[n] == '-'))
      {
        // Token may be numeric

        tempIndex = 1;  // there is at least one digit
        startToken = n;

        Stop = 0;
        haveDot = 0;
        haveExp = 0;

        n++;
        while (pCurByte[n] != 0 && !Stop)
        {
          switch (szJsonString[n])
          {
            case '\t':
            case '\r':
            case '\n':
            case ' ':
            case ',':
            case ':':
            case '[':
            case ']':
            case '{':
            case '}':

              n--;
              Stop = 1;
              break;

            default:

              switch(szJsonString[n]) {

                case '.':

                  // Only one dot and it must precede the exponent character and
                  // follow a digit

                  if (!haveDot && !haveExp) {

                    if (szJsonString[startToken] == '-') {

                      // There must be at least one digit in between the
                      // minus sign and the dot... if dot is at index 1,
                      // then it's an error.

                      if (tempIndex == 1) {
                        goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                      }

                      // check that if first digit is zero, then there are no
                      // other digits between the zero and the dot

                      if (szJsonString[startToken+1] == '0') {
                        if (tempIndex == 2) {
                          // Ok, number starts with
                          // -0 and next character is dot
                          tempIndex++;
                          n++;
                        }
                        else {
                          goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                        }
                      }
                      else {
                        // Ok, number starts with - and some non-zero digit
                        tempIndex++;
                        n++;
                      }
                    }
                    else {
                      if (szJsonString[startToken] == '0') {
                        if (tempIndex == 1) {
                          // Ok, number starts with 0 and
                          // next character is dot
                          tempIndex++;
                          n++;
                        }
                        else {
                          goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                        }

                      }
                      else {
                        // Ok, number starts some non-zero digit
                        tempIndex++;
                        n++;
                      }
                    }

                    haveDot = 1;
                  }
                  else {
                    goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                  }

                  break;

                case '+':
                case '-':

                  // This must immediately follow the exponent character
                  if (szJsonString[startToken+tempIndex-1] == 'e' ||
                      szJsonString[startToken+tempIndex-1] == 'E') {
                    tempIndex++;
                    n++;
                  }
                  else {
                    goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                  }

                  break;

                case 'e':
                case 'E':

                  if (!haveExp) {

                    // This character must be preceded by a digit

                    if (szJsonString[startToken+tempIndex-1] >= '0' &&
                        szJsonString[startToken+tempIndex-1] <= '9') {
                      tempIndex++;
                      haveExp = 1;
                      n++;
                    }
                    else {
                      goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                    }
                  }
                  else {
                    goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                  }

                  break;

                default:

                  if ((szJsonString[n] >= '0' && szJsonString[n] <= '9'))
                  {
                    if (szJsonString[startToken+tempIndex-1] == 'E' ||
                        szJsonString[startToken+tempIndex-1] == 'e') {

                      // If the previous character is the exponent
                      // and we have a zero, we can ignore it because
                      // exponenets don't have leading zeroes

                      if (szJsonString[n] != '0') {
                        tempIndex++;
                      }
                    }
                    else {
                      tempIndex++;
                    }
                    n++;
                  }
                  else {
                    goto CSJSON_PRIVATE_TOKENIZE_ERROR;
                  }

                  break;
              }

              break;
          }
        }

        //////////////////////////////////////////////////////////////
        // Check following cases:
        //
        // 1) last digit is e, E +, - or dot
        // 2) leading zero not immediately followed by a dot
        //////////////////////////////////////////////////////////////

        if (szJsonString[startToken + tempIndex-1] == 'e' ||
            szJsonString[startToken + tempIndex-1] == 'E' ||
            szJsonString[startToken + tempIndex-1] == '+' ||
            szJsonString[startToken + tempIndex-1] == '-' ||
            szJsonString[startToken + tempIndex-1] == '.') {
          goto CSJSON_PRIVATE_TOKENIZE_ERROR;
        }

        if (szJsonString[startToken] == '0') {
          if (tempIndex > 1) {
            if (szJsonString[startToken+1] != '.' &&
                szJsonString[startToken+1] != 'e' &&
                szJsonString[startToken+1] != 'E') {
              goto CSJSON_PRIVATE_TOKENIZE_ERROR;
            }
          }
        }

        if (szJsonString[startToken] == '-') {
          if (szJsonString[startToken+1] == '0') {
            if (tempIndex > 2) {
              if (szJsonString[startToken+2] != '.' &&
                  szJsonString[startToken+2] != 'e' &&
                  szJsonString[startToken+2] != 'E') {
                goto CSJSON_PRIVATE_TOKENIZE_ERROR;
              }
            }
          }
        }

        ti.type = JSON_TOK_NUMERIC;
        ti.size = tempIndex + 1;
        ti.szToken = (char*)malloc((ti.size) * sizeof(char));
        memcpy(ti.szToken, &(szJsonString[startToken]), ti.size-1);
        ti.szToken[ti.size-1] = 0;

        CSLIST_Insert(This->Tokens, &ti,
                        sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
        tokenOffset++;
      }
      else {

        if ((szJsonString[n] == 'f') ||
            (szJsonString[n] == 't') ||
            (szJsonString[n] == 'n'))
        {

          // Could be null or a boolean token

          tempIndex = 0;
          szBoolBuffer[tempIndex] = szJsonString[n];
          startToken = n;
          tempIndex++;

          Stop = 0;

          n++;
          while (pCurByte[n] != 0 && !Stop && tempIndex < 6)
          {
            switch (szJsonString[n])
            {
              case '\t':
              case '\r':
              case '\n':
              case ' ':
              case ',':
              case ':':
              case '[':
              case ']':
              case '{':
              case '}':

                n--;
                Stop = 1;
                break;

              default:

                szBoolBuffer[tempIndex] = szJsonString[n];
                tempIndex++;
                n++;
                break;
            }
          }

          szBoolBuffer[tempIndex] = 0;

          if (!strcmp(szBoolBuffer, "false")) {

            ti.type = JSON_TOK_BOOL_FALSE;
            ti.szToken = 0;
            CSLIST_Insert(This->Tokens, &ti,
                          sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
            tokenOffset++;
          }
          else if (!strcmp(szBoolBuffer, "true")) {

            ti.type = JSON_TOK_BOOL_TRUE;
            ti.szToken = 0;
            CSLIST_Insert(This->Tokens, &ti,
                          sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
            tokenOffset++;
          }
          else if (!strcmp(szBoolBuffer, "null")) {

            ti.type = JSON_TOK_NULL;
            ti.szToken = 0;
            CSLIST_Insert(This->Tokens, &ti,
                          sizeof(CSJSON_TOKENINFO), CSLIST_BOTTOM);
            tokenOffset++;
          }
          else {
            goto CSJSON_PRIVATE_TOKENIZE_ERROR;
          }
        }
        else {

          // We can accept white spaces between tokens and ignore them,
          // otherwise, the JSON is invalid.

          if (!((szJsonString[n] == ' ')  ||
                (szJsonString[n] == '\t') ||
                (szJsonString[n] == '\r') ||
                (szJsonString[n] == '\n')))
          {
            goto CSJSON_PRIVATE_TOKENIZE_ERROR;
          }
        }
      }

      break;
    }

    n++;

  }

  This->nextSlabSize = (n >= JSON_SERIALIZE_SLAB ? (n+1) : JSON_SERIALIZE_SLAB);

  if (This->nextSlabSize > This->slabSize) {
    This->slabSize = This->nextSlabSize;
    free(This->szSlab);
    This->szSlab = (char*)malloc((This->nextSlabSize + 1) * sizeof(char));
  }

  return CS_SUCCESS;

  CSJSON_PRIVATE_TOKENIZE_ERROR:

  This->nextSlabSize = (n >= JSON_SERIALIZE_SLAB ? (n+1) : JSON_SERIALIZE_SLAB);

  if (This->nextSlabSize > This->slabSize) {
    This->slabSize = This->nextSlabSize;
    free(This->szSlab);
    This->szSlab = (char*)malloc((This->nextSlabSize + 1) * sizeof(char));
  }

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_Parse
//
// Validates the input string as a JSON formated string.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_Parse
    (CSJSON *This,
     char *pJsonString,
     int parseMode)
{
  CSRESULT Rc;
  CSJSON_DIRENTRY*  pdire;
  CSJSON_LSENTRY* plse;
  CSJSON_TOKENINFO* pti;

  char* pszKey;

  char rootPath[2];

  long valueSize;
  long index;
  long count;
  long i;
  long size;

  if (pJsonString == 0) {
    return CS_FAILURE;
  }

  // Cleanup the previous object

  CSMAP_IterStart(This->Object, CSMAP_ASCENDING);

  while (CS_SUCCEED(CSMAP_IterNext(This->Object, &pszKey,
                                    (void **)(&pdire), &valueSize)))
  {
    if (pdire->Listing != 0)
    {
      if (pdire->type == JSON_TYPE_ARRAY) {

        count = CSLIST_Count(pdire->Listing);

        for (i=0; i<count; i++) {
          CSLIST_GetDataRef(pdire->Listing, (void**)(&plse), i);
          free(plse->szKey);
          free(plse->szValue);
        }

        CSLIST_Destructor(&(pdire->Listing));
      }
      else {

        CSMAP_IterStart(pdire->Listing, CSMAP_ASCENDING);

        while(CS_SUCCEED(CSMAP_IterNext(pdire->Listing, &pszKey,
                                        (void**)(&plse), &size))) {
          free(plse->szKey);
          free(plse->szValue);
        }

        CSMAP_Destructor(&(pdire->Listing));
      }
    }
  }

  CSLIST_Clear(This->Tokens);
  CSMAP_Clear(This->Object);

  // Let's see if this is a valid JSON string

  if (CS_SUCCEED(CSJSON_PRIVATE_Tokenize(This, pJsonString)))
  {
      // Get first token to determine JSON type
    if (CS_SUCCEED(CSLIST_GetDataRef(This->Tokens, (void**)(&pti), 0))) {

      index = 1;  // advance token read index

      switch(pti->type) {

        case JSON_TOK_LBRACE:
          // This is a JSON object
          rootPath[0] = JSON_PATH_SEP;
          rootPath[1] = 0;
          if (CS_FAIL(CSJSON_PRIVATE_O(This, &index, rootPath, 1))) {
            goto CSJSON_PARSE_FAILURE_CLEANUP;
          }

          break;

        case JSON_TOK_LBRACKET:
          // This is a JSON array
          rootPath[0] = JSON_PATH_SEP;
          rootPath[1] = 0;
          if (CS_FAIL(Rc = CSJSON_PRIVATE_A(This, &index, rootPath, 1))) {
            goto CSJSON_PARSE_FAILURE_CLEANUP;
          }

          break;

        default:

          // the string is not a valid JSON string
          goto CSJSON_PARSE_FAILURE_CLEANUP;
      }

      ////////////////////////////////////////////////////
      //
      // Consider this: {}}}}} or []]]]]]
      //
      // The CSJSON_PRIVATE_O/A functions will return success
      // because it will find a matching }/] to the first {/[
      // and return immediately, not examining what follows.
      //
      // We need to check if there are remaining tokens and
      // if there are some, then the JSON is ill-formed.
      ////////////////////////////////////////////////////

      if (index == CSLIST_Count(This->Tokens))
      {
        return CS_SUCCESS;
      }
    }
  }

  ///////////////////////////////////////////////////////////////
  // Branching label
  CSJSON_PARSE_FAILURE_CLEANUP:

  // Tokenization or parse has failed ... some tokens are invalid
  // and we need to cleanup token values; the object might have
  // been partially built so we need to clean it as well.

  count = CSLIST_Count(This->Tokens);

  for (i=0; i<count; i++) {
    CSLIST_GetDataRef(This->Tokens, (void**)(&pti), i);

    free(pti->szToken);
  }

  CSLIST_Clear(This->Tokens);

  /////////////////////////////////////////////////////////////////
  // We may need to cleanup the object; at this point, listing
  // entries all point to token fields because we are parsing a
  // string.
  //
  // The object may be partially built such that deep cleaning the
  // Object listing entries will leave tokens unaffected with
  // token buffers left un-freed. So in this particualr case, we
  // cleanup the tokens and simply clear the object of its
  // listing entries.
  //
  // In a situation where the string is parsed successfully,
  // then doing a deep cleaning of the Object is appropriate.
  /////////////////////////////////////////////////////////////////

  CSMAP_IterStart(This->Object, CSMAP_ASCENDING);

  while (CS_SUCCEED(CSMAP_IterNext(This->Object, &pszKey,
                                    (void **)(&pdire), &valueSize)))
  {
    if (pdire->Listing != 0)
    {
      if (pdire->type == JSON_TYPE_ARRAY) {
        CSLIST_Destructor(&(pdire->Listing));
      }
      else {
        CSMAP_Destructor(&(pdire->Listing));
      }
    }
  }

  CSMAP_Clear(This->Object);

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_PRIVATE_O
//
// Determines if a set of tokens is a well formed JSON object.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_PRIVATE_O
    (CSJSON* This,
     long* index,
     char* szPath,
     long  len) {

  CSRESULT Rc;
  CSLIST Listing;
  CSJSON_TOKENINFO* pti;
  CSJSON_DIRENTRY dire;

  long vv;
  char* szNewPath;

  vv = 0;

  // Since this is an object, the directory listing will be a map
  Listing = CSMAP_Constructor();

  szNewPath = (char*)malloc((len * sizeof(char)) + 1);
  memcpy(szNewPath, szPath, len+1);

  while (CS_SUCCEED(Rc = CSJSON_PRIVATE_VV(This, index,
                                          szNewPath, len, Listing))) {

    // We check for an empty object; in this case, the VV function
    // will fetch the matching right brace for this object rather
    // than the code that follows the call to VV.

    if (CS_DIAG(Rc) == JSON_TOK_RBRACE) {
      // maybe this is an empty object
      if (vv == 0) {
        // This is the end of the object and there was no variable/value
        dire.type = JSON_TYPE_OBJECT;
        dire.Listing = Listing;
        dire.numItems  = vv;

        CSMAP_Insert(This->Object, szNewPath,
                   (void*)(&dire), sizeof(CSJSON_DIRENTRY));

        This->nextSlabSize += 2; // because we need 2 braces
        free(szNewPath);
        return CS_SUCCESS;
      }
      else {

        Rc = CS_FAILURE;
        goto CSJSON_PRIVATE_O_FAILURE;
      }
    }

    // Read next token; we MUST have either a comma or a right brace
    Rc = CSLIST_GetDataRef(This->Tokens, (void**)(&pti), *index);

    if (CS_FAIL(Rc)) {

      goto CSJSON_PRIVATE_O_FAILURE;
    }

    switch(pti->type) {

      case JSON_TOK_COMMA:

        vv++;
        (*index)++;
        (This->nextSlabSize)++;  // for the comma
        break;

      case JSON_TOK_RBRACE:

        vv++;
        (*index)++;
        // This is the end of the object
        dire.type = JSON_TYPE_OBJECT;
        dire.Listing = Listing;
        dire.numItems  = vv;

        CSMAP_Insert(This->Object, szNewPath,
                   (void*)(&dire), sizeof(CSJSON_DIRENTRY));

        This->nextSlabSize += 2; // because we need 2 braces
        free(szNewPath);
        return CS_SUCCESS;

      default:

        // Wrong token type
        goto CSJSON_PRIVATE_O_FAILURE;
    }
  }

  CSJSON_PRIVATE_O_FAILURE:

  free(szNewPath);
  return Rc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_PRIVATE_VV
//
// Determines if a set of tokens is a well formed variable/value pair within
// a JSON object
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_PRIVATE_VV
    (CSJSON* This,
     long*  index,
     char*  szPath,
     long   len,
     CSMAP  Listing) {

  char* pNewPath;

  long newPathLen;

  CSRESULT Rc;
  CSJSON_TOKENINFO* pti;
  CSJSON_TOKENINFO* ls_pti;
  CSJSON_LSENTRY lse;

  Rc = CSLIST_GetDataRef(This->Tokens,
                           (void**)(&pti), *index);

  if (Rc == CS_SUCCESS) {

    if (pti->type == JSON_TOK_STRING) {

      ls_pti = pti; // This is a pointer to the token and will be inserted
                    // into the directory listing; this avoids copying
                    // the pointer to the token value as well as its size.

      (*index)++;

      Rc = CSLIST_GetDataRef(This->Tokens, (void**)(&pti), *index);

      // No need to check for success; if we fail reading
      // a token, then the type will remain JSON_TOK_STRING and
      // the next condition will fail.

      if (pti->type == JSON_TOK_COLON) {

        (*index)++;

        Rc = CSLIST_GetDataRef(This->Tokens,
                             (void**)(&pti), *index);

        // No need to check for success again; if we fail reading
        // a token, then the type will remain JSON_TOK_COLON and
        // the next condition will fall to the default case and fail.

        switch(pti->type) {

          case JSON_TOK_LBRACE:

            if (len > 1) {
              // We are not at root
              if (ls_pti->size == 1) {
                // Key is empty
                newPathLen = len + 2; // must include NULL
                pNewPath = (char*)malloc(newPathLen * sizeof(char));
                memcpy(pNewPath, szPath, len);
                pNewPath[len] = JSON_PATH_SEP;
                pNewPath[len+1] = 0;
              }
              else {
                if (szPath[len-1] == JSON_PATH_SEP) {
                  // path ends with the path separator;
                  //we don't add another path separator
                  newPathLen = len + ls_pti->size; // already includes NULL
                  pNewPath = (char*)malloc(newPathLen * sizeof(char));
                  memcpy(pNewPath, szPath, len);
                  memcpy(&pNewPath[len], ls_pti->szToken, ls_pti->size);
                }
                else {
                  newPathLen = len + 1 + ls_pti->size; // already includes NULL
                  pNewPath = (char*)malloc(newPathLen * sizeof(char));
                  memcpy(pNewPath, szPath, len);
                  pNewPath[len] = JSON_PATH_SEP;
                  memcpy(&pNewPath[len+1], ls_pti->szToken, ls_pti->size);
                }
              }
            }
            else {
              //we are root
              if (ls_pti->size == 1) {
                // Key is empty
                newPathLen = 3;
                pNewPath = (char*)malloc(newPathLen * sizeof(char));
                pNewPath[0] = JSON_PATH_SEP;
                pNewPath[1] = JSON_PATH_SEP;
                pNewPath[2] = 0;
              }
              else {
                newPathLen = 1 + ls_pti->size; // already includes NULL
                pNewPath = (char*)malloc(newPathLen * sizeof(char));
                pNewPath[0] = JSON_PATH_SEP;
                memcpy(&pNewPath[1], ls_pti->szToken, ls_pti->size);
              }
            }

            (*index)++;
            Rc = CSJSON_PRIVATE_O(This, index, pNewPath, newPathLen-1)
                        | JSON_TOK_LBRACE;

            free(pNewPath);

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.szValue = 0;
            lse.type = JSON_TYPE_OBJECT;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 2 double quotes, a colon and the key value
            This->nextSlabSize = This->nextSlabSize + ls_pti->size + 3;

            break;

          case JSON_TOK_LBRACKET:

            if (len > 1) {
              // we are not at the root
              if (ls_pti->size == 1) {
                // Key is empty
                newPathLen = len + 2; // must include NULL
                pNewPath = (char*)malloc(newPathLen * sizeof(char));
                memcpy(pNewPath, szPath, len);
                pNewPath[len] = JSON_PATH_SEP;
                pNewPath[len+1] = 0;
              }
              else {
                if (szPath[len-1] == JSON_PATH_SEP) {
                  // path ends with the path separator;
                  //we don't add another path separator
                  newPathLen = len + ls_pti->size; // already includes NULL
                  pNewPath = (char*)malloc(newPathLen * sizeof(char));
                  memcpy(pNewPath, szPath, len);
                  memcpy(&pNewPath[len], ls_pti->szToken, ls_pti->size);
                }
                else {
                  newPathLen = len + 1 + ls_pti->size; // already includes NULL
                  pNewPath = (char*)malloc(newPathLen * sizeof(char));
                  memcpy(pNewPath, szPath, len);
                  pNewPath[len] = JSON_PATH_SEP;
                  memcpy(&pNewPath[len+1], ls_pti->szToken, ls_pti->size);
                }
              }
            }
            else {
              //we are at the root
              if (ls_pti->size == 1) {
                // Key is empty
                newPathLen = 3;
                pNewPath = (char*)malloc(newPathLen * sizeof(char));
                pNewPath[0] = JSON_PATH_SEP;
                pNewPath[1] = JSON_PATH_SEP;
                pNewPath[2] = 0;
              }
              else {
                newPathLen = 1 + ls_pti->size; // already includes NULL
                pNewPath = (char*)malloc(newPathLen * sizeof(char));
                pNewPath[0] = JSON_PATH_SEP;
                memcpy(&pNewPath[1], ls_pti->szToken, ls_pti->size);
              }
            }

            (*index)++;
            Rc = CSJSON_PRIVATE_A(This, index, pNewPath, newPathLen-1)
                        | JSON_TOK_LBRACKET;

            free(pNewPath);

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.szValue = 0;
            lse.type = JSON_TYPE_ARRAY;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 2 double quotes, a colon and the key value
            This->nextSlabSize = This->nextSlabSize + ls_pti->size + 3;

            break;

          case JSON_TOK_STRING:

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.type = JSON_TYPE_STRING;
            lse.szValue = pti->szToken;
            lse.valueSize = pti->size;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 4 double quotes, a colon and the key/value pair
            This->nextSlabSize = This->nextSlabSize +
                                    ls_pti->size + pti->size + 5;

            (*index)++;

            Rc = CS_SUCCESS | JSON_TOK_STRING;

            break;

          case JSON_TOK_NUMERIC:

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.type = JSON_TYPE_NUMERIC;
            lse.szValue = pti->szToken;
            lse.valueSize = pti->size;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 2 double quotes, a colon and the key/value pair
            This->nextSlabSize = This->nextSlabSize +
                                       ls_pti->size + pti->size + 3;

            (*index)++;

            Rc = CS_SUCCESS | JSON_TOK_NUMERIC;

            break;

          case JSON_TOK_BOOL_FALSE:

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.type = JSON_TYPE_BOOL_FALSE;
            lse.szValue = 0;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 2 double quotes, a colon and the key/value pair
            This->nextSlabSize = This->nextSlabSize + ls_pti->size + 8;

            (*index)++;

            Rc = CS_SUCCESS | JSON_TOK_BOOL_FALSE;

            break;

          case JSON_TOK_BOOL_TRUE:

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.type = JSON_TOK_BOOL_TRUE;
            lse.szValue = 0;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 2 double quotes, a colon and the key/value pair
            This->nextSlabSize = This->nextSlabSize + ls_pti->size + 7;

            (*index)++;

            Rc = CS_SUCCESS | JSON_TOK_BOOL_TRUE;

            break;

          case JSON_TOK_NULL:

            lse.szKey = ls_pti->szToken;
            lse.keySize = ls_pti->size;
            lse.type = JSON_TYPE_NULL;
            lse.szValue = 0;

            CSMAP_InsertKeyRef(Listing, ls_pti->szToken,
                         (void*)(&lse), sizeof(CSJSON_LSENTRY));

            // because we need 2 double quotes, a colon and the key/value pair
            This->nextSlabSize = This->nextSlabSize + ls_pti->size + 7;

            (*index)++;

            Rc = CS_SUCCESS | JSON_TOK_NULL;

            break;

          default:

            Rc = CS_FAILURE;

            break;
        }
      }
      else {
        Rc = CS_FAILURE;
      }
    }
    else {

      // We started with a left brace and the token is not a string.
      // maybe it's an empty object so we check for a matching right
      // brace; if it is a matching brace, then it is ok. Otherwise,
      // the variable/value pair (and therefore the object) is invalid

      if(pti->type == JSON_TOK_RBRACE) {
        (*index)++;
        Rc = CS_SUCCESS | pti->type;
      }
      else {
        Rc = CS_FAILURE;
      }
    }
  }

  return Rc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_PRIVATE_A
//
// Determines if a set of tokens is a well formed JSON array
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_PRIVATE_A
    (CSJSON* This,
     long*  index,
     char*  szPath,
     long   len) {

  CSRESULT Rc;
  CSJSON_TOKENINFO* pti;
  CSJSON_DIRENTRY dire;
  CSJSON_LSENTRY lse;
  CSJSON_LSENTRY* plse;
  CSLIST Listing;

  long indexLen;
  long start;
  long curIndex;
  long commaFlag;
  long newPathLen;
  long count;
  long i;

  char* szNewPath;

  char szIndex[11];

  Listing = CSLIST_Constructor();

  start = *index;
  curIndex = 0;
  commaFlag = 1;

  while (1) {

    if (CS_SUCCEED(CSLIST_GetDataRef(This->Tokens,
                        (void**)(&pti), start))) {

      switch (pti->type) {

        case JSON_TOK_LBRACE:

          if (commaFlag == 1) {

            if (len > 1) {

              // If the path ends with a slash, then the parent
              // has a null (empty) key. In this case,
              // we don't add another slash

              if (szPath[len-1] == JSON_PATH_SEP) {

                // allocate enough space for path and
                // 10 digits and the NULL

                szNewPath = (char*)malloc((len + 11) * sizeof(char));
                memcpy(szNewPath, szPath, len);
                sprintf(szIndex, "%ld", curIndex);
                indexLen = strlen(szIndex) + 1; // take NULL into account
                memcpy(&szNewPath[len], szIndex, indexLen);
                newPathLen = len + indexLen;
              }
              else {

                // allocate enough space for path, a slash,
                // 10 digits and the NULL

                szNewPath = (char*)malloc((len + 12) * sizeof(char));
                memcpy(szNewPath, szPath, len);
                szNewPath[len] = JSON_PATH_SEP; //'/';
                sprintf(szIndex, "%ld", curIndex);
                indexLen = strlen(szIndex) + 1;  // take NULL into account
                memcpy(&szNewPath[len+1], szIndex, indexLen);
                newPathLen = len + 1 + indexLen;
              }
            }
            else {
              // we are at the root; allocate enough space for
              // path, 10 digits and the NULL
              szNewPath = (char*)malloc(len + 11 * sizeof(char));
              memcpy(szNewPath, szPath, len);
              sprintf(szIndex, "%ld", curIndex);
              indexLen = strlen(szIndex) + 1;  // take NULL into account
              memcpy(&szNewPath[len], szIndex, indexLen);
              newPathLen = len + indexLen;
            }

            start++;
            Rc = CSJSON_PRIVATE_O(This, &start, szNewPath, newPathLen-1);
            free(szNewPath);

            curIndex++;
            commaFlag = 0;
            lse.szKey = 0;
            lse.szValue = 0;
            lse.type = JSON_TYPE_OBJECT;
            CSLIST_Insert(Listing, (void*)(&lse),
                          sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);
          }
          else {
            goto CSJSON_PRIVATE_A_CLEANUP;
          }

          break;

        case JSON_TOK_LBRACKET:

          if (commaFlag == 1) {

            if (len > 1) {

              // If the path ends with a slash, then the parent
              // has a null (empty) key. In this case,
              // we don't add another slash

              if (szPath[len-1] == JSON_PATH_SEP) {

                // allocate enough space for path and
                // 10 digits and the NULL

                szNewPath = (char*)malloc((len + 11) * sizeof(char));
                memcpy(szNewPath, szPath, len);
                sprintf(szIndex, "%ld", curIndex);
                indexLen = strlen(szIndex) + 1; // take NULL into account
                memcpy(&szNewPath[len], szIndex, indexLen);
                newPathLen = len + indexLen;
              }
              else {

                // allocate enough space for path, a slash,
                // 10 digits and the NULL

                szNewPath = (char*)malloc(len + 12 * sizeof(char));
                memcpy(szNewPath, szPath, len);
                szNewPath[len] = JSON_PATH_SEP;
                sprintf(szIndex, "%ld", curIndex);
                indexLen = strlen(szIndex) + 1;  // take NULL into account
                memcpy(&szNewPath[len+1], szIndex, indexLen);
                newPathLen = len + 1 + indexLen;
              }
            }
            else {
              // we are at the root; allocate enough space for
              // path, 10 digits and the NULL
              szNewPath = (char*)malloc(len + 11 * sizeof(char));
              memcpy(szNewPath, szPath, len);
              sprintf(szIndex, "%ld", curIndex);
              indexLen = strlen(szIndex) + 1;  // take NULL into account
              memcpy(&szNewPath[len], szIndex, indexLen);
              newPathLen = len + indexLen;
            }

            start++;
            Rc = CSJSON_PRIVATE_A(This, &start, szNewPath, newPathLen-1);
            free(szNewPath);

            curIndex++;
            commaFlag = 0;

            lse.szKey = 0;
            lse.szValue = 0;
            lse.type = JSON_TYPE_ARRAY;
            CSLIST_Insert(Listing, (void*)(&lse),
                          sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);
          }
          else {
            goto CSJSON_PRIVATE_A_CLEANUP;
          }

          break;

        case JSON_TOK_COMMA:

          if (commaFlag == 0) {
            commaFlag = 1;
            start++;
            // because we need room for the comma
            (This->nextSlabSize)++;
          }
          else {
            goto CSJSON_PRIVATE_A_CLEANUP;
          }

          break;

        case JSON_TOK_RBRACKET:

          if ((commaFlag == 0 && curIndex > 0) ||
              (commaFlag == 1 && curIndex == 0)) {

            // allocate enough space for path

            szNewPath = (char*)malloc((len + 1) * sizeof(char));
            memcpy(szNewPath, szPath, len+1);

            dire.type = JSON_TYPE_ARRAY;
            dire.Listing = Listing;
            dire.numItems  = curIndex;

            CSMAP_Insert(This->Object, szNewPath,
                        (void*)(&dire), sizeof(CSJSON_DIRENTRY));

            free(szNewPath);

            // because we need 2 brackets
            This->nextSlabSize += 2;

            *index = start + 1;
            Rc = CS_SUCCESS;
            goto CSJSON_PRIVATE_A_END;
          }
          else {
            goto CSJSON_PRIVATE_A_CLEANUP;
          }

          break;

        default:

          if (commaFlag == 1) {

            if (pti->type == JSON_TOK_STRING) {

              lse.szKey = 0;
              lse.szValue = pti->szToken;
              lse.valueSize = pti->size;
              lse.type = pti->type;
              CSLIST_Insert(Listing, (void*)(&lse),
                            sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);
              // because we need (maybe) 2 double quotes and the value
              This->nextSlabSize = This->nextSlabSize + lse.valueSize + 2;

              start++;
              curIndex++;
            }
            else if ( pti->type == JSON_TOK_NUMERIC) {

              lse.szKey = 0;
              lse.szValue = pti->szToken;
              lse.valueSize = pti->size;
              lse.type = pti->type;
              CSLIST_Insert(Listing, (void*)(&lse),
                            sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);
              // because we need (maybe) 2 double quotes and the value
              This->nextSlabSize = This->nextSlabSize + lse.valueSize + 2;

              start++;
              curIndex++;
            }
            else if (pti->type == JSON_TOK_BOOL_FALSE ||
                     pti->type == JSON_TOK_BOOL_TRUE ||
                     pti->type == JSON_TOK_NULL) {

              lse.szKey = 0;
              lse.szValue = 0;
              lse.type = pti->type;
              CSLIST_Insert(Listing, (void*)(&lse),
                            sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);
              // because we need up to the max value size
              This->nextSlabSize = This->nextSlabSize + 5;

              start++;
              curIndex++;
            }
            else {
              //CS_FAILURE... leave the loop
              goto CSJSON_PRIVATE_A_CLEANUP;
            }

            commaFlag = 0;
            break;
        }
        else {
          //CS_FAILURE... leave the loop
          goto CSJSON_PRIVATE_A_CLEANUP;
        }
      }
    }
    else {
      //CS_FAILURE... leave the loop
      goto CSJSON_PRIVATE_A_CLEANUP;
    }
  }

  ///////////////////////////////////////////////////////////
  // Branching Label
  CSJSON_PRIVATE_A_CLEANUP:
  //
  // We need to cleanup the listing and destroy it. Note that
  // the listing points to tokens and since they will be
  // released by the CSJSON_Parse method, we must NOT
  // release the tokens referenced by the listing entries.
  ///////////////////////////////////////////////////////////

  count = CSLIST_Count(Listing);

  for (i=0; i<count; i++) {

    CSLIST_GetDataRef(Listing, (void**)(&plse), i);

    if (plse->szKey != 0) {
      free(plse->szKey);
    }
  }

  CSLIST_Destructor(&Listing);

  Rc = CS_FAILURE;

  ///////////////////////////////////////////////////////////
  // Branching Label
  CSJSON_PRIVATE_A_END:

  return Rc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_LookupDir
//
// Retrieves inforamtion on a directory path (not including a value key)
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_LookupDir
    (CSJSON* This,
     char* szPath,
     CSJSON_DIRENTRY* pdire) {

  long size;
  long len;
  long i;

  char* tempPath;

  CSJSON_DIRENTRY* lpdire;

  if (szPath == 0 || pdire == 0) {

    pdire->Listing = 0;
    pdire->numItems = 0;
    pdire->type = JSON_TYPE_UNKNOWN;

    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                             tempPath,
                             (void**)(&lpdire),
                             &size))) {

    if (lpdire->type == JSON_TYPE_ARRAY ||
        lpdire->type == JSON_TYPE_OBJECT) {

      pdire->numItems = lpdire->numItems;
      pdire->type = lpdire->type;
      free(tempPath);
      return CS_SUCCESS;
    }
  }
  else {
    pdire->numItems = 0;
    pdire->type = JSON_TYPE_UNKNOWN;
  }

  free(tempPath);

  pdire->Listing = 0;
  pdire->numItems = 0;
  pdire->type = JSON_TYPE_UNKNOWN;

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_LookupKey
//
// Retrieves a value from an object at a specified path and key.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT CSJSON_LookupKey
  (CSJSON* This,
   char* szPath,
   char* szKey,
   CSJSON_LSENTRY* plse) {

  long size;
  long len;
  long i;

  char* tempPath;

  CSJSON_LSENTRY* lplse;
  CSJSON_DIRENTRY* lpdire;

  if (szPath == 0 || szKey == 0 || plse == 0) {

    plse->keySize = 0;
    plse->szKey = 0;
    plse->szValue = 0;
    plse->type = JSON_TYPE_UNKNOWN;
    plse->valueSize = 0;

    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                             tempPath,
                             (void**)(&lpdire),
                             &size))) {

    if (lpdire->type == JSON_TYPE_OBJECT) {

      if (CS_SUCCEED(CSMAP_Lookup(lpdire->Listing,
                                  szKey,
                                  (void**)(&lplse),
                                  &size))) {

        plse->type = lplse->type;
        plse->szKey = lplse->szKey;
        plse->keySize = lplse->keySize;

        if (lplse->type == JSON_TYPE_STRING ||
            lplse->type == JSON_TYPE_NUMERIC) {
          plse->szValue = lplse->szValue;
          plse->valueSize = lplse->valueSize;
        }
        else {
          plse->szValue = 0;
          plse->valueSize = 0;
        }

        free(tempPath);
        return CS_SUCCESS;
      }
      else {
        return CS_FAILURE;
      }
    }
  }

  free(tempPath);

  plse->keySize = 0;
  plse->szKey = 0;
  plse->szValue = 0;
  plse->type = JSON_TYPE_UNKNOWN;
  plse->valueSize = 0;

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_LookupIndex
//
// Retrieves a value from an array at a specified path and index.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_LookupIndex
    (CSJSON* This,
     char* szPath,
     long index,
     CSJSON_LSENTRY* plse) {

  long size;
  long len;
  long i;

  char* tempPath;

  CSJSON_LSENTRY* lplse;
  CSJSON_DIRENTRY* lpdire;

  if (szPath == 0 || plse == 0) {

    plse->keySize = 0;
    plse->szKey = 0;
    plse->szValue = 0;
    plse->type = JSON_TYPE_UNKNOWN;
    plse->valueSize = 0;

    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                             tempPath,
                             (void**)(&lpdire),
                             &size))) {

    if (lpdire->type == JSON_TYPE_ARRAY) {

      if (CS_SUCCEED(CSLIST_GetDataRef(lpdire->Listing,
                                  (void**)(&lplse),
                                  index))) {

          plse->type = lplse->type;
          plse->szKey = lplse->szKey;
          plse->keySize = lplse->keySize;

        if (lplse->type == JSON_TYPE_STRING ||
            lplse->type == JSON_TYPE_NUMERIC) {
          plse->szValue = lplse->szValue;
          plse->valueSize = lplse->valueSize;
        }
        else {
          plse->szValue = 0;
          plse->valueSize = 0;
        }

        free(tempPath);
        return CS_SUCCESS;
      }
      else {
        return CS_FAILURE;
      }
    }
  }

  plse->keySize = 0;
  plse->szKey = 0;
  plse->szValue = 0;
  plse->type = JSON_TYPE_UNKNOWN;
  plse->valueSize = 0;

  free(tempPath);

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_Dump
//
// Displays the JSON instance structure as a tree (terminals are not shown).
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_Dump
    (CSJSON* This,
     char sep) {

  long count;
  long i, j;
  long size;
  long tempKeyLen;
  char* szKey;
  char* tempKey;

  CSJSON_DIRENTRY* pdire;
  CSJSON_LSENTRY* pls;

  CSMAP_IterStart(This->Object, CSMAP_ASCENDING);

  while(CS_SUCCEED(CSMAP_IterNext(This->Object, &szKey,
                                  (void**)(&pdire), &size)))
  {

    if (szKey[0] != 0) {

      ///////////////////////////////////////////////////////////////////
      // Convert submitted path to internal representation. The
      // internal path separator is the ESC character.
      ///////////////////////////////////////////////////////////////////

      switch (pdire->type) {
        case JSON_TYPE_STRING:
          printf("\n%s : STRING", szKey);
          break;
        case JSON_TYPE_NUMERIC:
          printf("\n%s : NUMERIC", szKey);
          break;
        case JSON_TYPE_BOOL_FALSE:
          printf("\n%s : BOOL", szKey);
          break;
        case JSON_TYPE_BOOL_TRUE:
          printf("\n%s : BOOL", szKey);
          break;
        case JSON_TYPE_NULL:
          printf("\n%s : NULL", szKey);
          break;
        case JSON_TYPE_ARRAY:

          tempKeyLen = strlen(szKey);
          tempKey = (char*)malloc(tempKeyLen * sizeof(char) + 1);
          j=0;
          while (szKey[j] != 0) {
            if (szKey[j] == JSON_PATH_SEP) {
              tempKey[j] = sep;
            }
            else {
              tempKey[j] = szKey[j];
            }
            j++;
          }
          tempKey[j] = 0;

          printf("\n%s : <ARRAY>", tempKey);
          free(tempKey);

          count = CSLIST_Count(pdire->Listing);

          for (i=0; i<count; i++) {

            CSLIST_GetDataRef(pdire->Listing, (void**)(&pls), i);

            switch (pls->type) {
              case JSON_TYPE_STRING:
                printf("\n\tSTRING");
                break;
              case JSON_TYPE_NUMERIC:
                printf("\n\tNUMERIC");
                break;
              case JSON_TYPE_BOOL_FALSE:
                printf("\n\tBOOL");
                break;
              case JSON_TYPE_BOOL_TRUE:
                printf("\n\tBOOL");
                break;
              case JSON_TYPE_NULL:
                printf("\n\tNULL");
                break;
              case JSON_TYPE_ARRAY:
                printf("\n\t<ARRAY>");
                break;
              case JSON_TYPE_OBJECT:
                printf("\n\t<OBJECT>");
                break;
              default:
                printf("\n\t<UNKNOWN>");
                break;
            }
          }

          break;

        case JSON_TYPE_OBJECT:

          tempKeyLen = strlen(szKey);
          tempKey = (char*)malloc(tempKeyLen * sizeof(char) + 1);
          j=0;
          while (szKey[j] != 0) {
            if (szKey[j] == JSON_PATH_SEP) {
              tempKey[j] = sep;
            }
            else {
              tempKey[j] = szKey[j];
            }
            j++;
          }
          tempKey[j] = 0;

          printf("\n%s : <OBJECT>", tempKey);
          free(tempKey);

          CSMAP_IterStart(pdire->Listing, CSMAP_ASCENDING);

          while (CS_SUCCEED(CSMAP_IterNext(pdire->Listing, &szKey,
                                           (void**)(&pls), &size))) {

            switch (pls->type) {

              case JSON_TYPE_STRING:
                printf("\n\t%s : STRING", pls->szKey);
                break;
              case JSON_TYPE_NUMERIC:
                printf("\n\t%s : NUMERIC", pls->szKey);
                break;
              case JSON_TYPE_BOOL_FALSE:
                printf("\n\t%s : BOOL", pls->szKey);
                break;
              case JSON_TYPE_BOOL_TRUE:
                printf("\n\t%s : BOOL", pls->szKey);
                break;
              case JSON_TYPE_NULL:
                printf("\n\t%s : NULL", pls->szKey);
                break;
              case JSON_TYPE_ARRAY:
                printf("\n\t%s : <ARRAY>", pls->szKey);
                break;
              case JSON_TYPE_OBJECT:
                printf("\n\t%s : <OBJECT>", szKey);
                break;
              default:
                printf("\n\t<UNKNOWN>");
                break;
            }
          }

          break;
        default:
          printf("<UNKNOWN>");
          break;
      }
    }
  }

  return CS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_Init
//
// Creates an empty JSON instance (either an object or an array).
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_Init
    (CSJSON* This,
     long type) {

  CSRESULT Rc;

  CSJSON_DIRENTRY dire;
  CSJSON_DIRENTRY* pdire;
  CSJSON_LSENTRY* plse;

  char* pszKey;

  char szRoot[2];

  long valueSize;
  long count;
  long i;
  long size;

  Rc = CS_SUCCESS;

  This->nextSlabSize = 2;

  // Cleanup the previous object

  CSMAP_IterStart(This->Object, CSMAP_ASCENDING);

  while (CS_SUCCEED(CSMAP_IterNext(This->Object, &pszKey,
                                    (void **)(&pdire), &valueSize)))
  {
    if (pdire->Listing != 0)
    {
      if (pdire->type == JSON_TYPE_ARRAY) {

        count = CSLIST_Count(pdire->Listing);

        for (i=0; i<count; i++) {
          CSLIST_GetDataRef(pdire->Listing, (void**)(&plse), i);
            free(plse->szKey);
            free(plse->szValue);
        }

        CSLIST_Destructor(&(pdire->Listing));
      }
      else {

        CSMAP_IterStart(pdire->Listing, CSMAP_ASCENDING);

        while(CS_SUCCEED(CSMAP_IterNext(pdire->Listing, &pszKey,
                                        (void**)(&plse), &size))) {
          free(plse->szKey);
          free(plse->szValue);
        }

        CSMAP_Destructor(&(pdire->Listing));
      }
    }
  }

  CSMAP_Clear(This->Object);

  szRoot[0] = JSON_PATH_SEP;
  szRoot[1] = 0;

  dire.type = type;
  dire.numItems  = 0;

  switch(type) {

    case JSON_TYPE_ARRAY:

      dire.Listing = CSLIST_Constructor();

      CSMAP_Insert(This->Object, szRoot,
                        (void*)(&dire), sizeof(CSJSON_DIRENTRY));

      break;

    case JSON_TYPE_OBJECT:

      dire.Listing = CSMAP_Constructor();

        CSMAP_Insert(This->Object, szRoot,
                    (void*)(&dire), sizeof(CSJSON_DIRENTRY));
      break;

    default:

      Rc = CS_FAILURE;
      break;
  }

  return Rc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_Serialize
//
// Returns the string representation of the JSON instance.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

long
  CSJSON_Serialize
    (CSJSON* This,
     char* szPath,
     char** szOutStream,
     int mode) {

  long curPos;
  long len;
  long i;

  char* tempPath;

  if (szPath == 0 || szOutStream == 0) {
    return CS_FAILURE;
  }

  ////////////////////////////////////////////////////////////////////////
  // Allocate slab if the present one is too small:
  // note that the slab size may be actually larger than the actual
  // output stream. This is ok because we will return the actual size
  // of the output stream and as long as the slab is large enough,
  // it will be fine.
  ////////////////////////////////////////////////////////////////////////

  if (This->nextSlabSize > This->slabSize) {
    This->slabSize = This->nextSlabSize;
    free(This->szSlab);
    This->szSlab = (char*)malloc((This->slabSize +1) *sizeof(char));
  }

  ////////////////////////////////////////////////////////////////////////
  // We hand over the slab to the caller; it is under agreement
  // that the caller will not use the slab other than for reading it
  // and/or copying it. Caller should not write
  // into the slab nor deallocate it
  ////////////////////////////////////////////////////////////////////////

  *szOutStream = This->szSlab;
  curPos = 0;

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  CSJSON_PRIVATE_Serialize(This, tempPath, szOutStream, &curPos);
  free(tempPath);
  (*szOutStream)[curPos] = 0;

  return curPos;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_PRIVATE_Serialize
//
// REcursively converts the JSON instance into its string representation.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

long
  CSJSON_PRIVATE_Serialize
    (CSJSON* This,
     char* szPath,
     char** szOutStream,
     long* curPos) {

  long count;
  long i;
  long size;
  long pathLen;
  long indexLen;
  long ti, tj;

  char* szKey;
  char* szSubPath;

  char szIndex[11];

  CSJSON_DIRENTRY* dire;
  CSJSON_LSENTRY* pls;

  if(CS_SUCCEED(CSMAP_Lookup(This->Object, szPath,
                             (void**)(&dire), &size))) {

    switch (dire->type) {

      case JSON_TYPE_ARRAY:

        (*szOutStream)[*curPos] = '[';
        (*curPos)++;

        if (dire->Listing != 0) {

          count = CSLIST_Count(dire->Listing);

          for (i=0; i<count; i++) {

            CSLIST_GetDataRef(dire->Listing, (void**)(&pls), i);

            if (pls->type == JSON_TYPE_STRING) {

              (*szOutStream)[*curPos] = '"';
              (*curPos)++;

              tj = pls->valueSize-1;
              for (ti=0; ti<tj; ti++) {

                switch(pls->szValue[ti]) {

                  case '"':
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = '"';
                    (*curPos)++;
                    break;

                  case '\b':
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = 'b';
                    (*curPos)++;
                    break;

                  case '\f':
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = 'f';
                    (*curPos)++;
                    break;

                  case '\n':
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = 'n';
                    (*curPos)++;
                    break;

                  case '\r':
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = 'r';
                    (*curPos)++;
                    break;

                  case '\t':
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = 't';
                    (*curPos)++;
                    break;

                  case '\\':
                    if (pls->szValue[ti+1] == 'u') {
                      (*szOutStream)[*curPos] = pls->szValue[ti];
                      (*curPos)++;
                    }
                    else {
                      (*szOutStream)[*curPos] = '\\';
                      (*curPos)++;
                      (*szOutStream)[*curPos] = pls->szValue[ti];
                      (*curPos)++;
                    }
                    break;

                  default:
                    (*szOutStream)[*curPos] = pls->szValue[ti];
                    (*curPos)++;
                    break;
                }
              }

              (*szOutStream)[*curPos] = '"';
              (*curPos)++;
            }
            else {

              switch (pls->type) {

                case JSON_TYPE_NUMERIC:

                  memcpy(&(*szOutStream)[*curPos],
                         pls->szValue, pls->valueSize-1);
                  (*curPos) += (pls->valueSize-1);
                  break;

                case JSON_TYPE_BOOL_FALSE:

                  memcpy(&(*szOutStream)[*curPos], "false", 5);
                  (*curPos) += 5;
                  break;

                case JSON_TYPE_BOOL_TRUE:

                  memcpy(&(*szOutStream)[*curPos], "true", 4);
                  (*curPos) += 4;
                  break;

                case JSON_TYPE_NULL:

                  memcpy(&(*szOutStream)[*curPos], "null", 4);
                  (*curPos) += 4;
                  break;

                case JSON_TYPE_ARRAY:

                  sprintf(szIndex, "%ld", i);
                  pathLen = strlen(szPath);
                  indexLen = strlen(szIndex);
                  szSubPath =
                     (char*)malloc((pathLen + indexLen + 3) * sizeof(char));

                  memcpy(szSubPath, szPath, pathLen);

                  if (pathLen > 1) { // we are not root path
                    if (szPath[pathLen-1] == JSON_PATH_SEP) {
                      memcpy(&szSubPath[pathLen], szIndex, indexLen);
                      szSubPath[pathLen + indexLen] = 0;
                    }
                    else {
                      szSubPath[pathLen] = JSON_PATH_SEP;
                      memcpy(&szSubPath[pathLen+1], szIndex, indexLen);
                      szSubPath[pathLen + 1 + indexLen] = 0;
                    }
                  }
                  else {
                    memcpy(&szSubPath[pathLen], szIndex, indexLen);
                    szSubPath[pathLen + indexLen] = 0;
                  }

                  CSJSON_PRIVATE_Serialize
                                   (This, szSubPath, szOutStream, curPos);

                  free(szSubPath);

                  break;

                case JSON_TYPE_OBJECT:

                  sprintf(szIndex, "%ld", i);
                  pathLen = strlen(szPath);
                  indexLen = strlen(szIndex);
                  szSubPath =
                    (char*)malloc((pathLen + indexLen + 3) * sizeof(char));

                  memcpy(szSubPath, szPath, pathLen);

                  if (pathLen > 1) { // we are not root path

                    if (szPath[pathLen-1] == JSON_PATH_SEP) {
                      memcpy(&szSubPath[pathLen], szIndex, indexLen);
                      szSubPath[pathLen + indexLen] = 0;
                    }
                    else {
                      szSubPath[pathLen] = JSON_PATH_SEP;
                      memcpy(&szSubPath[pathLen+1], szIndex, indexLen);
                      szSubPath[pathLen + 1 + indexLen] = 0;
                    }
                  }
                  else {
                    memcpy(&szSubPath[pathLen], szIndex, indexLen);
                    szSubPath[pathLen + indexLen] = 0;
                  }

                  CSJSON_PRIVATE_Serialize
                                   (This, szSubPath, szOutStream, curPos);

                  free(szSubPath);

                  break;

                default:

                  break;
              }
            }

            (*szOutStream)[*curPos] = ',';
            (*curPos)++;
          }

          // to erase dangling comma after last element, if there are some
          if (count > 0) {
            (*curPos)--;
          }
        }

        (*szOutStream)[*curPos] = ']';
        (*curPos)++;

        break;

      case JSON_TYPE_OBJECT:

        (*szOutStream)[*curPos] = '{';
        (*curPos)++;

        CSMAP_IterStart(dire->Listing, CSMAP_ASCENDING);
        count = 0;

        while(CS_SUCCEED(CSMAP_IterNext(dire->Listing, &szKey,
                                        (void**)(&pls), &size))) {

          count++;

          // Copy key with escape sequences

          (*szOutStream)[*curPos] = '"';
          (*curPos)++;

          tj = pls->keySize-1;
          for (ti=0; ti<tj; ti++) {

            switch(pls->szKey[ti]) {

              case '"':
                (*szOutStream)[*curPos] = '\\';
                (*curPos)++;
                (*szOutStream)[*curPos] = '"';
                (*curPos)++;
                break;

              case '\b':
                (*szOutStream)[*curPos] = '\\';
                (*curPos)++;
                (*szOutStream)[*curPos] = 'b';
                (*curPos)++;
                break;

              case '\f':
                (*szOutStream)[*curPos] = '\\';
                (*curPos)++;
                (*szOutStream)[*curPos] = 'f';
                (*curPos)++;
                break;

              case '\n':
                (*szOutStream)[*curPos] = '\\';
                (*curPos)++;
                (*szOutStream)[*curPos] = 'n';
                (*curPos)++;
                break;

              case '\r':
                (*szOutStream)[*curPos] = '\\';
                (*curPos)++;
                (*szOutStream)[*curPos] = 'r';
                (*curPos)++;
                break;

              case '\t':
                (*szOutStream)[*curPos] = '\\';
                (*curPos)++;
                (*szOutStream)[*curPos] = 't';
                (*curPos)++;
                break;

              case '\\':
                if (pls->szKey[ti+1] == 'u') {
                  (*szOutStream)[*curPos] = pls->szKey[ti];
                  (*curPos)++;
                }
                else {
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = pls->szKey[ti];
                  (*curPos)++;
                }
                break;

              default:
                (*szOutStream)[*curPos] = pls->szKey[ti];
                (*curPos)++;
                break;
            }
          }

          (*szOutStream)[*curPos] = '"';
          (*curPos)++;
          (*szOutStream)[*curPos] = ':';
          (*curPos)++;

          if (pls->type == JSON_TYPE_STRING) {

            (*szOutStream)[*curPos] = '"';
            (*curPos)++;

            // Copy string value with escape sequences

            tj = pls->valueSize-1;
            for (ti=0; ti<tj; ti++) {

              switch(pls->szValue[ti]) {

                case '"':
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = '"';
                  (*curPos)++;
                  break;

                case '\b':
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = 'b';
                  (*curPos)++;
                  break;

                case '\f':
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = 'f';
                  (*curPos)++;
                  break;

                case '\n':
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = 'n';
                  (*curPos)++;
                  break;

                case '\r':
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = 'r';
                  (*curPos)++;
                  break;

                case '\t':
                  (*szOutStream)[*curPos] = '\\';
                  (*curPos)++;
                  (*szOutStream)[*curPos] = 't';
                  (*curPos)++;
                  break;
                case '\\':
                  if (pls->szValue[ti+1] == 'u') {
                    (*szOutStream)[*curPos] = pls->szValue[ti];
                    (*curPos)++;
                  }
                  else {
                    (*szOutStream)[*curPos] = '\\';
                    (*curPos)++;
                    (*szOutStream)[*curPos] = pls->szValue[ti];
                    (*curPos)++;
                  }
                  break;

                default:
                  (*szOutStream)[*curPos] = pls->szValue[ti];
                  (*curPos)++;
                  break;
              }
            }

            (*szOutStream)[*curPos] = '"';
            (*curPos)++;
          }
          else {

            switch (pls->type) {

              case JSON_TYPE_NUMERIC:

                memcpy(&(*szOutStream)[*curPos], pls->szValue, pls->valueSize-1);
                (*curPos) += (pls->valueSize-1);

                break;
              case JSON_TYPE_BOOL_FALSE:

                memcpy(&(*szOutStream)[*curPos], "false", 5);
                (*curPos) += 5;

                break;
              case JSON_TYPE_BOOL_TRUE:

                memcpy(&(*szOutStream)[*curPos], "true", 4);
                (*curPos) += 4;
                break;

              case JSON_TYPE_NULL:

                memcpy(&(*szOutStream)[*curPos], "null", 4);
                (*curPos) += 4;
                break;

              case JSON_TYPE_ARRAY:

                pathLen = strlen(szPath);
                szSubPath =
                   (char*)malloc((pathLen + pls->keySize + 3) * sizeof(char));

                if (pathLen > 1) { // we are not root path
                  if (pls->keySize == 1) { // Key is empty
                    szSubPath =
                      (char*)malloc((pathLen + 2) * sizeof(char));
                    memcpy(szSubPath, szPath, pathLen);
                    szSubPath[pathLen] = JSON_PATH_SEP;
                    szSubPath[pathLen+1] = 0;
                  }
                  else {
                    if(szPath[pathLen-1] == JSON_PATH_SEP) {
                      szSubPath =
                        (char*)malloc((pathLen + pls->keySize) * sizeof(char));
                      memcpy(szSubPath, szPath, pathLen);
                      memcpy(&szSubPath[pathLen], pls->szKey, pls->keySize);
                    }
                    else {
                      szSubPath =
                        (char*)malloc((pathLen + pls->keySize + 1) * sizeof(char));
                      memcpy(szSubPath, szPath, pathLen);
                      szSubPath[pathLen] = JSON_PATH_SEP;
                      memcpy(&szSubPath[pathLen+1], pls->szKey, pls->keySize);
                    }
                  }
                }
                else {
                  // we are at root
                  if (pls->keySize == 1) { // Key is empty
                    szSubPath =
                      (char*)malloc(3 * sizeof(char));
                    szSubPath[0] = JSON_PATH_SEP;
                    szSubPath[1] = JSON_PATH_SEP;
                    szSubPath[2] = 0;
                  }
                  else {
                    szSubPath =
                      (char*)malloc((pls->keySize + 1) * sizeof(char));
                    szSubPath[0] = JSON_PATH_SEP;
                    memcpy(&szSubPath[1], pls->szKey, pls->keySize);
                  }
                }

                // serialize subtree
                CSJSON_PRIVATE_Serialize
                       (This, szSubPath, szOutStream, curPos);

                free(szSubPath);

                break;

              case JSON_TYPE_OBJECT:

                pathLen = strlen(szPath);

                if (pathLen > 1) { // we are not root path
                  if (pls->keySize == 1) { // Key is empty
                    szSubPath =
                      (char*)malloc((pathLen + 2) * sizeof(char));
                    memcpy(szSubPath, szPath, pathLen);
                    szSubPath[pathLen] = JSON_PATH_SEP;
                    szSubPath[pathLen+1] = 0;
                  }
                  else {
                    if(szPath[pathLen-1] == JSON_PATH_SEP) {
                      szSubPath =
                        (char*)malloc((pathLen + pls->keySize) * sizeof(char));
                      memcpy(szSubPath, szPath, pathLen);
                      memcpy(&szSubPath[pathLen], pls->szKey, pls->keySize);
                    }
                    else {
                      szSubPath =
                        (char*)malloc((pathLen + pls->keySize + 1) * sizeof(char));
                      memcpy(szSubPath, szPath, pathLen);
                      szSubPath[pathLen] = JSON_PATH_SEP;
                      memcpy(&szSubPath[pathLen+1], pls->szKey, pls->keySize);
                    }
                  }
                }
                else {
                  // we are at root
                  if (pls->keySize == 1) { // Key is empty
                    szSubPath =
                      (char*)malloc(3 * sizeof(char));
                    szSubPath[0] = JSON_PATH_SEP;
                    szSubPath[1] = JSON_PATH_SEP;
                    szSubPath[2] = 0;
                  }
                  else {
                    szSubPath =
                      (char*)malloc((pls->keySize + 1) * sizeof(char));
                    szSubPath[0] = JSON_PATH_SEP;
                    memcpy(&szSubPath[1], pls->szKey, pls->keySize);
                  }
                }

                // serialize subtree
                CSJSON_PRIVATE_Serialize
                     (This, szSubPath, szOutStream, curPos);

                free(szSubPath);

                break;

              default:
                break;
            }
          }

          (*szOutStream)[*curPos] = ',';
          (*curPos)++;
        }

        // to erase dangling comma after last element, if there are some
        if (count > 0) {
          (*curPos)--;
        }

        (*szOutStream)[*curPos] = '}';
        (*curPos)++;

        break;

      default:

        break;
    }
  }

  return *curPos;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_InsertBool
//
// Inserts a boolean value into the JSON instance under a path.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_InsertBool
    (CSJSON* This,
     char*   szPath,
     char*   szKey,
     int     boolValue) {

  long keySize;
  long size;
  long len;
  long i;

  char* tempPath;

  CSJSON_DIRENTRY* ppve;
  CSJSON_LSENTRY lse;
  CSJSON_LSENTRY* plse;

  if (szPath == 0) {
    return CS_FAILURE;
  }

  if (boolValue != JSON_TYPE_BOOL_FALSE) {
    boolValue = JSON_TYPE_BOOL_TRUE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                              tempPath,
                              (void**)(&ppve),
                              &size)))
  {
    free(tempPath);

    switch(ppve->type) {

      case JSON_TYPE_ARRAY:

        lse.szKey = 0;
        lse.keySize = 0;
        lse.szValue = 0;
        lse.type = boolValue;
        lse.valueSize = 0;

        CSLIST_Insert(ppve->Listing, (void*)(&lse),
                      sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);

        ppve->numItems++;
        This->nextSlabSize = This->nextSlabSize + 6;

        break;

      case JSON_TYPE_OBJECT:

        if (szKey == 0) {
          return CS_FAILURE;
        }

        if (CS_SUCCEED(CSMAP_Lookup(ppve->Listing,
                                    szKey,
                                    (void**)(&plse),
                                    &size))) {

          // replace existing key

          switch(plse->type) {
            case JSON_TYPE_STRING:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize -2;
              break;
            case JSON_TYPE_NUMERIC:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize;
              break;
            case JSON_TYPE_BOOL_FALSE:
              This->nextSlabSize = This->nextSlabSize - 5;
              break;
            case JSON_TYPE_BOOL_TRUE:
              This->nextSlabSize = This->nextSlabSize - 5;
              break;
            case JSON_TYPE_NULL:
              This->nextSlabSize = This->nextSlabSize - 4;
              break;
            case JSON_TYPE_ARRAY:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
            case JSON_TYPE_OBJECT:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
          }

          plse->valueSize = 0;
          plse->szValue = 0;
          plse->type = boolValue;
          This->nextSlabSize = This->nextSlabSize + 5;
        }
        else {

          keySize = strlen(szKey);

          lse.keySize = keySize+1;
          lse.szKey = (char*)malloc(lse.keySize * sizeof(char));
          memcpy(lse.szKey, szKey, lse.keySize);
          lse.valueSize = 0;
          lse.szValue = 0;
          lse.type = boolValue;

          CSMAP_InsertKeyRef(ppve->Listing, lse.szKey,
                       (void*)(&lse), sizeof(CSJSON_LSENTRY));

          ppve->numItems++;

          // We can potentially expand the string value if there are
          // characters that must be escaped. To be on the safe side,
          // we assume the worst case where every character would
          // need to be escaped.

          This->nextSlabSize = This->nextSlabSize + (keySize * 2) +  + 9;
        }

        break;

      default:

        return CS_FAILURE;
    }

    return CS_SUCCESS;
  }
  else {
    free(tempPath);
  }

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_InsertNull
//
// Inserts a null value into the JSON instance under a path.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_InsertNull
    (CSJSON* This,
     char*   szPath,
     char*   szKey) {

  CSJSON_DIRENTRY* ppve;
  CSJSON_LSENTRY lse;
  CSJSON_LSENTRY* plse;

  long keySize;
  long size;
  long len;
  long i;

  char* tempPath;

  if (szPath == 0) {
    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                              tempPath,
                              (void**)(&ppve),
                              &size)))
  {
    free(tempPath);

    switch(ppve->type) {

      case JSON_TYPE_ARRAY:

        lse.szKey = 0;
        lse.keySize = 0;
        lse.szValue = 0;
        lse.type = JSON_TYPE_NULL;
        lse.valueSize = 0;

        CSLIST_Insert(ppve->Listing, (void*)(&lse),
                      sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);

        ppve->numItems++;
        This->nextSlabSize = This->nextSlabSize + 5;

        break;

      case JSON_TYPE_OBJECT:

        if (szKey == 0) {
          return CS_FAILURE;
        }

        // Check if key exists

        if (CS_SUCCEED(CSMAP_Lookup(ppve->Listing,
                                    szKey,
                                    (void**)(&plse),
                                    &size))) {

          // replace existing key

          switch(plse->type) {
            case JSON_TYPE_STRING:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize -2;
              break;
            case JSON_TYPE_NUMERIC:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize;
              break;
            case JSON_TYPE_BOOL_FALSE:
              This->nextSlabSize = This->nextSlabSize - 5;
              break;
            case JSON_TYPE_BOOL_TRUE:
              This->nextSlabSize = This->nextSlabSize - 5;
              break;
            case JSON_TYPE_NULL:
              This->nextSlabSize = This->nextSlabSize - 4;
              break;
            case JSON_TYPE_ARRAY:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
            case JSON_TYPE_OBJECT:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
          }

          plse->valueSize = 0;
          plse->szValue = 0;
          plse->type = JSON_TYPE_NULL;
          This->nextSlabSize = This->nextSlabSize + 5;

        }
        else {

          keySize = strlen(szKey);

          lse.keySize = keySize+1;
          lse.szKey = (char*)malloc(lse.keySize * sizeof(char));
          memcpy(lse.szKey, szKey, lse.keySize);
          lse.valueSize = 0;
          lse.szValue = 0;
          lse.type = JSON_TYPE_NULL;

          CSMAP_InsertKeyRef(ppve->Listing, lse.szKey,
                       (void*)(&lse), sizeof(CSJSON_LSENTRY));

          ppve->numItems++;

          // We can potentially expand the string value if there are
          // characters that must be escaped. To be on the safe side,
          // we assume the worst case where every character would
          // need to be escaped.

          This->nextSlabSize = This->nextSlabSize + (keySize * 2) + 8;
        }

        break;

      default:

        return CS_FAILURE;
    }

    return CS_SUCCESS;
  }
  else {
    free(tempPath);
  }

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_InsertNumeric
//
// Inserts a numeric value into the JSON instance under a path.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_InsertNumeric
    (CSJSON* This,
     char*   szPath,
     char*   szKey,
     char*   szValue) {

  CSJSON_DIRENTRY* ppve;
  CSJSON_LSENTRY lse;
  CSJSON_LSENTRY* plse;

  long valueSize;
  long keySize;
  long size;
  long len;
  long i;

  char* tempPath;

  if (szPath == 0) {
    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_FAIL(CSJSON_PRIVATE_IsNumeric(szValue))) {
    return CS_FAILURE;
  }

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                              tempPath,
                              (void**)(&ppve),
                              &size)))
  {
    free(tempPath);

    valueSize = strlen(szValue);

    switch(ppve->type) {

      case JSON_TYPE_ARRAY:

        lse.szKey = 0;
        lse.keySize = 0;
        lse.szValue = (char*)malloc((valueSize+1) * sizeof(char));
        memcpy(lse.szValue, szValue, valueSize+1);
        lse.type = JSON_TYPE_NUMERIC;
        lse.valueSize = valueSize+1;

        CSLIST_Insert(ppve->Listing, (void*)(&lse),
                      sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);

        ppve->numItems++;
        This->nextSlabSize = This->nextSlabSize + valueSize + 1;

        break;

      case JSON_TYPE_OBJECT:

        if (szKey == 0) {
          return CS_FAILURE;
        }

        // Check if key exists

        if (CS_SUCCEED(CSMAP_Lookup(ppve->Listing,
                                    szKey,
                                    (void**)(&plse),
                                    &size))) {

          // replace existing key

          switch(plse->type) {
            case JSON_TYPE_STRING:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize -2;
              break;
            case JSON_TYPE_NUMERIC:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize;
              break;
            case JSON_TYPE_BOOL_FALSE:
              This->nextSlabSize = This->nextSlabSize - 5;
              break;
            case JSON_TYPE_BOOL_TRUE:
              This->nextSlabSize = This->nextSlabSize - 4;
              break;
            case JSON_TYPE_NULL:
              This->nextSlabSize = This->nextSlabSize - 4;
              break;
            case JSON_TYPE_ARRAY:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
            case JSON_TYPE_OBJECT:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
          }

          plse->valueSize = valueSize+1;
          plse->szValue = (char*)malloc(plse->valueSize * sizeof(char));
          memcpy(plse->szValue, szValue, plse->valueSize);
          plse->type = JSON_TYPE_NUMERIC;
          This->nextSlabSize = This->nextSlabSize + valueSize + 4;

        }
        else {

          keySize = strlen(szKey);

          lse.keySize = keySize+1;
          lse.szKey = (char*)malloc(lse.keySize * sizeof(char));
          memcpy(lse.szKey, szKey, lse.keySize);
          lse.valueSize = valueSize+1;
          lse.szValue = (char*)malloc((lse.valueSize) * sizeof(char));
          memcpy(lse.szValue, szValue, lse.valueSize);
          lse.type = JSON_TYPE_NUMERIC;

          CSMAP_InsertKeyRef(ppve->Listing, lse.szKey,
                       (void*)(&lse), sizeof(CSJSON_LSENTRY));

          ppve->numItems++;

          // We can potentially expand the string value if there are
          // characters that must be escaped. To be on the safe side,
          // we assume the worst case where every character would
          // need to be escaped.

          This->nextSlabSize = This->nextSlabSize +
                           (keySize * 2) + valueSize + 4;
        }

        break;

      default:

        return CS_FAILURE;
    }

    return CS_SUCCESS;
  }
  else {
    free(tempPath);
  }

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_InsertString
//
// Inserts a string value into the JSON instance under a path.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_InsertString
    (CSJSON* This,
     char*  szPath,
     char*  szKey,
     char*  szValue) {

  CSJSON_DIRENTRY* ppve;
  CSJSON_LSENTRY lse;
  CSJSON_LSENTRY* plse;

  long valueSize;
  long keySize;
  long size;
  long len;
  long i;

  char* tempPath;

  if (szPath == 0 || szValue == 0) {
    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                              tempPath,
                              (void**)(&ppve),
                              &size)))
  {
    free(tempPath);

    valueSize = strlen(szValue);

    switch(ppve->type) {

      case JSON_TYPE_ARRAY:

        lse.szKey = 0;
        lse.keySize = 0;
        lse.szValue = (char*)malloc((valueSize+1) * sizeof(char));
        memcpy(lse.szValue, szValue, valueSize+1);
        lse.type = JSON_TYPE_STRING;
        lse.valueSize = valueSize+1;
        CSLIST_Insert(ppve->Listing, (void*)(&lse),
                      sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);

        ppve->numItems++;

        // We can potentially expand the string value if there are
        // characters that must be escaped. To be on the safe side,
        // we assume the worst case where every character would
        // need to be escaped.

        This->nextSlabSize = This->nextSlabSize + (2 * valueSize) + 3;

        break;

      case JSON_TYPE_OBJECT:

        if (szKey == 0) {
          return CS_FAILURE;
        }

        // Check if key exists

        if (CS_SUCCEED(CSMAP_Lookup(ppve->Listing,
                                    szKey,
                                    (void**)(&plse),
                                    &size))) {

          switch(plse->type) {
            case JSON_TYPE_STRING:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize -2;
              break;
            case JSON_TYPE_NUMERIC:
              free(plse->szValue);
              This->nextSlabSize = This->nextSlabSize - plse->valueSize;
              break;
            case JSON_TYPE_BOOL_FALSE:
              This->nextSlabSize = This->nextSlabSize - 5;
              break;
            case JSON_TYPE_BOOL_TRUE:
              This->nextSlabSize = This->nextSlabSize - 4;
              break;
            case JSON_TYPE_NULL:
              This->nextSlabSize = This->nextSlabSize - 4;
              break;
            case JSON_TYPE_ARRAY:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
            case JSON_TYPE_OBJECT:
              // Cannot replace tree with value (yet)
              return CS_FAILURE;
          }

          plse->valueSize = valueSize+1;
          plse->szValue = (char*)malloc(plse->valueSize * sizeof(char));
          memcpy(plse->szValue, szValue, plse->valueSize);
          plse->type = JSON_TYPE_STRING;

          // We can potentially expand the string value if there are
          // characters that must be escaped. To be on the safe side,
          // we assume the worst case where every character would
          // need to be escaped.

          This->nextSlabSize = This->nextSlabSize + (valueSize * 2) + 6;

        }
        else {

          keySize = strlen(szKey);

          lse.keySize = keySize+1;
          lse.szKey = (char*)malloc(lse.keySize * sizeof(char));
          memcpy(lse.szKey, szKey, lse.keySize);
          lse.valueSize = valueSize+1;
          lse.szValue = (char*)malloc((lse.valueSize) * sizeof(char));
          memcpy(lse.szValue, szValue, lse.valueSize);
          lse.type = JSON_TYPE_STRING;

          CSMAP_InsertKeyRef(ppve->Listing, lse.szKey,
                       (void*)(&lse), sizeof(CSJSON_LSENTRY));


          ppve->numItems++;

          // We can potentially expand the string value if there are
          // characters that must be escaped. To be on the safe side,
          // we assume the worst case where every character would
          // need to be escaped.

          This->nextSlabSize = This->nextSlabSize +
                               (keySize *2 ) + (valueSize * 2) + 6;
        }

        break;

      default:

        return CS_FAILURE;
    }

    return CS_SUCCESS;
  }
  else {
    free(tempPath);
  }

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_MkDir
//
// Creates a directory (path) into the JSON instance under an existing path.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_MkDir
    (CSJSON* This,
     char*  szPath,
     char*  szKey,
     int    type) {

  CSRESULT Rc;
  CSJSON_DIRENTRY* ppdire;
  CSJSON_DIRENTRY dire;
  CSJSON_LSENTRY lse;

  char* szNewPath;
  char* tempPath;

  char szIndex[11];

  long i;
  long size;
  long len;
  long indexLen;
  long totalSize;
  long keySize;

  if (type != JSON_TYPE_ARRAY && type != JSON_TYPE_OBJECT) {
    return CS_FAILURE;
  }

  if (szPath == 0) {
    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                              tempPath,
                              (void**)(&ppdire),
                              &size)))
  {
    switch (ppdire->type) {

      case JSON_TYPE_ARRAY:

        sprintf(szIndex, "%ld", ppdire->numItems);
        indexLen = strlen(szIndex);
        totalSize = len + indexLen + 2;

        szNewPath = (char*)malloc(totalSize * sizeof(char));
        memcpy(szNewPath, tempPath, len);

        if (tempPath[1] == 0) {
          // we are at root
          memcpy(&szNewPath[1], szIndex, indexLen);
          szNewPath[len + indexLen] = 0;
        }
        else {
          if (tempPath[len-1] == JSON_PATH_SEP) {
            memcpy(&szNewPath[len], szIndex, indexLen);
            szNewPath[len + indexLen] = 0;
          }
          else {
            szNewPath[len] = JSON_PATH_SEP;
            memcpy(&szNewPath[len+1], szIndex, indexLen);
            szNewPath[len + 1 + indexLen] = 0;
          }
        }

        lse.szKey = 0;
        lse.szValue = 0;
        lse.type = type;
        lse.keySize = 0;
        lse.valueSize = 0;

        CSLIST_Insert(ppdire->Listing, (void*)(&lse),
                      sizeof(CSJSON_LSENTRY), CSLIST_BOTTOM);

        ppdire->numItems++;

        dire.type = type;
        dire.numItems = 0;

        if (type == JSON_TYPE_ARRAY) {
          dire.Listing = CSLIST_Constructor();
        }
        else {
          dire.Listing = CSMAP_Constructor();
        }

        CSMAP_Insert(This->Object, szNewPath,
                     (void*)(&dire), sizeof(CSJSON_DIRENTRY));

        This->nextSlabSize += 3; // two braces/brackets and possibly a comma

        Rc = CS_SUCCESS;
        break;

      case JSON_TYPE_OBJECT:

        if (szKey == 0) {
          free(tempPath);
          return CS_FAILURE;
        }

        // Check if key already exists under the path
        if (CS_SUCCEED(CSMAP_Lookup(ppdire->Listing,
                                    szKey,
                                    (void**)(&lse),
                                    &size))) {
          free(tempPath);
          return CS_FAILURE;
        }

        keySize = strlen(szKey);

        if (len > 1) {  // we are not at root path
          if (keySize == 0) { // Key is empty
            szNewPath = (char*)malloc((len + 2) * sizeof(char));
            memcpy(szNewPath, tempPath, len);
            szNewPath[len] = JSON_PATH_SEP;
            szNewPath[len+1] = 0;
          }
          else {
            if (tempPath[len-1] == JSON_PATH_SEP) {
              szNewPath = (char*)malloc((len + keySize + 1) * sizeof(char));
              memcpy(szNewPath, tempPath, len);
              memcpy(&szNewPath[len], szKey, keySize);
              szNewPath[len + keySize] = 0;
            }
            else {
              szNewPath = (char*)malloc((len + keySize + 2) * sizeof(char));
              memcpy(szNewPath, tempPath, len);
              szNewPath[len] = JSON_PATH_SEP;
              memcpy(&szNewPath[len+1], szKey, keySize);
              szNewPath[len + keySize + 1] = 0;
            }
          }
        }
        else {
          if (keySize == 0) { // Key is empty
            szNewPath = (char*)malloc(3 * sizeof(char));
            szNewPath[0] = JSON_PATH_SEP;
            szNewPath[1] = JSON_PATH_SEP;
            szNewPath[2] = 0;
          }
          else {
            szNewPath = (char*)malloc((keySize + 2) * sizeof(char));
            szNewPath[0] = JSON_PATH_SEP;
            memcpy(&szNewPath[1], szKey, keySize);
            szNewPath[keySize + 1] = 0;
          }
        }

        lse.szKey = (char*)malloc((keySize+1) * sizeof(char));
        memcpy(lse.szKey, szKey, keySize+1);
        lse.keySize = keySize+1;
        lse.type = type;
        lse.valueSize = 0;
        lse.szValue = 0;

        CSMAP_Insert(ppdire->Listing, lse.szKey,
                       (void*)(&lse), sizeof(CSJSON_LSENTRY));

        ppdire->numItems++;

        dire.type = type;
        dire.numItems = 0;

        if (type == JSON_TYPE_ARRAY) {
          dire.Listing = CSLIST_Constructor();
        }
        else {
          dire.Listing = CSMAP_Constructor();
        }

        CSMAP_Insert(This->Object, szNewPath,
                      (void*)(&dire), sizeof(CSJSON_DIRENTRY));

        // two braces/brackets and possibly a comma and a colon
        This->nextSlabSize = This->nextSlabSize + lse.keySize + 6;

        Rc = CS_SUCCESS;

        break;

      default:
        Rc = CS_FAILURE;
    }
  }
  else {
    Rc = CS_FAILURE;
  }

  free(tempPath);
  return Rc;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_Rm
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_Rm
    (CSJSON* This,
     char*  szPath,
     char*  szKey,
     int    type) {

  return CS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_ParseAt
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_ParseAt
    (CSJSON* This,
     char*  szPath,
     char*  szKey,
     char* str) {

  return CS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_IterStart
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_IterStart
    (CSJSON* This,
     char*  szPath,
     CSJSON_DIRENTRY* pDire) {

  CSJSON_DIRENTRY* ppve;

  long size;
  long len;
  long i;

  char* tempPath;

  if (szPath == NULL || pDire == NULL) {
    return CS_FAILURE;
  }

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if (CS_SUCCEED(CSMAP_Lookup(This->Object,
                              tempPath,
                              (void**)(&ppve),
                              &size)))
  {
    free(tempPath);

    switch(ppve->type) {

      case JSON_TYPE_ARRAY:
        This->iterIndex = 0;
        This->iterType = JSON_TYPE_ARRAY;
        This->pIterListing = ppve->Listing;
        pDire->Listing = 0;
        pDire->numItems = ppve->numItems;
        pDire->type = ppve->type;
        return CS_SUCCESS;

      case JSON_TYPE_OBJECT:
        This->iterIndex = 0;
        This->iterType = JSON_TYPE_OBJECT;
        This->pIterListing = ppve->Listing;
        pDire->Listing = 0;
        pDire->numItems = 0;
        pDire->type = ppve->type;
        CSMAP_IterStart((CSMAP*)(This->pIterListing), CSMAP_ASCENDING);
        return CS_SUCCESS;

      default:
        This->iterIndex = 0;
        This->iterType = JSON_TYPE_UNKNOWN;
        This->pIterListing = NULL;
        pDire->Listing = 0;
        pDire->numItems = 0;
        pDire->type = ppve->type;
        return CS_FAILURE;
    }
  }
  else {
    free(tempPath);
    This->iterIndex = 0;
    This->iterType = JSON_TYPE_UNKNOWN;
    This->pIterListing = NULL;
    pDire->Listing = 0;
    pDire->numItems = 0;
    pDire->type = JSON_TYPE_UNKNOWN;
  }

  return CS_FAILURE;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_IterNext
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_IterNext
    (CSJSON* This,
     CSJSON_LSENTRY* plse) {

  char* szKey;

  long size;

  CSJSON_LSENTRY* lplse;

  if (plse == NULL) {
    return CS_FAILURE;
  }

  if (This->pIterListing == NULL) {
    return CS_FAILURE;
  }

  switch(This->iterType) {

    case JSON_TYPE_ARRAY:

      if (CS_SUCCEED(CSLIST_GetDataRef((CSLIST*)(This->pIterListing),
                                       (void**)(&lplse),
                                       This->iterIndex))) {

        plse->szKey = 0;
        plse->keySize = 0;
        (This->iterIndex)++;
      }
      else {
        plse->szValue = 0;
        plse->type = JSON_TYPE_UNKNOWN;
        plse->szKey = 0;
        plse->keySize = 0;
        return CS_FAILURE;
      }

      break;

    case JSON_TYPE_OBJECT:

      if (CS_FAIL(CSMAP_IterNext((CSMAP*)(This->pIterListing),
                                     &szKey,
                                     (void**)(&lplse), &size))) {
        plse->szValue = 0;
        plse->type = JSON_TYPE_UNKNOWN;
        plse->szKey = 0;
        plse->keySize = 0;
        return CS_FAILURE;
      }

      plse->szKey = lplse->szKey;
      plse->keySize = lplse->keySize;

      break;

    default:

      return CS_FAILURE;
  }

  plse->type = lplse->type;

  if (lplse->type == JSON_TYPE_STRING ||
      lplse->type == JSON_TYPE_NUMERIC) {
    plse->szValue = lplse->szValue;
    plse->valueSize = lplse->valueSize;
  }
  else {
    plse->szValue = 0;
    plse->valueSize = 0;
  }

  return CS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_Ls
//
// Returns a listing of information structures for all values
// under an existing path.
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSJSON_Ls
    (CSJSON* This,
     char* szPath,
     CSLIST listing) {

  CSJSON_DIRENTRY* pve;
  CSJSON_LSENTRY* plse;

  char* szKey;

  long size;
  long count;
  long i;
  long len;

  char* tempPath;

  if (szPath == 0) {
    return CS_FAILURE;
  }

  CSLIST_Clear(listing);

  ///////////////////////////////////////////////////////////////////
  // Convert submitted path to internal representation. The
  // internal path separator is the ESC character.
  ///////////////////////////////////////////////////////////////////

  len = strlen(szPath);
  tempPath = (char*)malloc(len * sizeof(char) + 1);
  i=0;
  while (szPath[i] != 0) {
    if (szPath[i] == szPath[0]) {
      tempPath[i] = JSON_PATH_SEP;
    }
    else {
      tempPath[i] = szPath[i];
    }
    i++;
  }
  tempPath[i] = 0;

  if(CS_SUCCEED(CSMAP_Lookup(This->Object, tempPath, (void**)(&pve), &size)))  {

    if (pve->type == JSON_TYPE_OBJECT) {

      CSMAP_IterStart(pve->Listing, CSMAP_ASCENDING);

      while(CS_SUCCEED(CSMAP_IterNext(pve->Listing, &szKey,
                                      (void**)(&plse), &size))) {

        // insert address of listing node
        CSLIST_Insert(listing, (void*)(&plse), sizeof(plse), CSLIST_BOTTOM);
      }
    }
    else {

      if (pve->type == JSON_TYPE_ARRAY) {

        count = CSLIST_Count(pve->Listing);

        for (i=0; i<count; i++) {
          CSLIST_GetDataRef(pve->Listing, (void**)(&plse), i);
          CSLIST_Insert(listing, (void*)(&plse), sizeof(plse), CSLIST_BOTTOM);
        }
      }
      else {
        free(tempPath);
        return JSON_TYPE_UNKNOWN;
      }
    }
  }
  else {
    free(tempPath);
    return JSON_TYPE_UNKNOWN;
  }

  free(tempPath);
  return pve->type;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// CSJSON_PRIVATE_IsNumeric
//
// Validates the format of a numeric value
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CSRESULT CSJSON_PRIVATE_IsNumeric(char* szNumber) {

  long n;

  int haveDot;
  int haveExp;

  haveDot = 0;
  haveExp = 0;
  n=0;

  if ((szNumber[n] >= '0' && szNumber[n] <= '9') ||
        (szNumber[n] == '-'))
  {
    haveDot = 0;
    haveExp = 0;

    n++;
    while (szNumber[n] != 0) {

      switch (szNumber[n])
      {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          n++;
          break;

        case '.':

          // Only one dot and it must precede the exponent character

          if (!haveDot && !haveExp) {

            // check that if first digit is zero, then there are no
            // other digits between the zero and the dot

            if (szNumber[0] == '-') {
              if (szNumber[1] == '0') {
                if (n != 2) {
                  //number starts with -0
                  //but next character is not dot
                  return CS_FAILURE;
                }
                else {
                  n++;
                }
              }
              else {
                n++;
              }
            }
            else {
              if (szNumber[0] == '0') {
                if (n != 1) {
                  //number starts with 0 but next character is not dot
                  return CS_FAILURE;
                }
                else {
                  n++;
                }
              }
              else {
                n++;
              }
            }

            haveDot = 1;
          }
          else {
            return CS_FAILURE;
          }

          break;

        case '+':
        case '-':

          // This must immediately follow the exponent character
          if (szNumber[n-1] == 'e' || szNumber[n-1] == 'E') {
            n++;
          }
          else {
            return CS_FAILURE;
          }

          break;

        case 'e':
        case 'E':

          if (!haveExp) {
            // This character must be preceded by a digit
            if (szNumber[n-1] >= '0' && szNumber[n-1] <= '9') {
              haveExp = 1;
              n++;
            }
            else {
              return CS_FAILURE;
            }
          }
          else {
            return CS_FAILURE;
          }

          break;

        default:

          return CS_FAILURE;
      }
    }
  }
  else {
    return CS_FAILURE;
  }

  //////////////////////////////////////////////////////////////
  // Check following cases:
  //
  // 1) last digit is e, E +, - or dot
  // 2) leading zero not immediately followed by a dot
  //////////////////////////////////////////////////////////////

  if (szNumber[n-1] == 'e' ||
      szNumber[n-1] == 'E' ||
      szNumber[n-1] == '+' ||
      szNumber[n-1] == '-' ||
      szNumber[n-1] == '.') {
    return CS_FAILURE;
  }

  return CS_SUCCESS;
}
