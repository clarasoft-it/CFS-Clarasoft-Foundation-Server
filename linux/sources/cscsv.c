


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <clarasoft/cslib.h>
#include <clarasoft/csjson.h>

#define CSCSV_MASK_ARRAY           0x00001000
#define CSCSV_MASK_HEADINGS        0x00000001
#define CSCSV_MASK_KEEP_INVALIDREC 0x00000020

#define CSCSV_CVT_MAP              0x00000000
#define CSCSV_CVT_ARRAY            0x00001000

#define CSCSV_NO_HEADINGS          0x00000000
#define CSCSV_HEADINGS             0x00000001

#define CSCSV_DISCARD_INVALIDREC   0x00000010
#define CSCSV_KEEP_INVALIDREC      0x00000020

#define CSCSV_ERR_PARSE            0x00000001
#define CSCSV_ERR_PARSE_NUMCOLUMNS 0x00000002

typedef struct tagCSCSV {

  CSLIST record;
  FILE* file;
  char separator;
  long lineSize;
  char* lineBuffer;

} CSCSV;

CSCSV* 
  CSCSV_Constructor
    (void);

CSRESULT
  CSCSV_Destructor
    (CSCSV** This);

CSRESULT
  CSCSV_IterStart
    (CSCSV* This,
     char separator,
     char* filePath,
     long recordSize);

CSRESULT
  CSCSV_IterNext
    (CSCSV* This,
     CSLIST record);

CSRESULT
  CSCSV_ImportToJSON
    (CSCSV* This,
     char separator,
     char* filePath,
     CSJSON pJson,
     CSLIST Headings,
     long flags);

CSCSV* 
  CSCSV_Constructor
    (void) {

  CSCSV* instance;

  instance = (CSCSV*)malloc(sizeof(CSCSV));

  instance->file = NULL;
  instance->record = CSLIST_Constructor();
  instance->lineBuffer = (char*)malloc(65536 * sizeof(char));
  instance->lineSize = 65535;

  return instance;
}

CSRESULT
  CSCSV_Destructor
    (CSCSV** This) {

  if (*This == NULL) {
    return CS_FAILURE;
  }

  if ((*This)->file != NULL) {
    fclose((*This)->file);
  }

  free((*This)->lineBuffer);

  CSLIST_Destructor(&((*This)->record));

  return CS_SUCCESS;
}

CSRESULT
  CSCSV_IterStart
    (CSCSV* This,
     char separator,
     char* szFilePath,
     long recordSize) {

  if (This->file != NULL) {
    fclose(This->file);
  }

  This->file = fopen(szFilePath, "r");

  if (This->file == NULL) {
    return CS_FAILURE;
  }

  if (This->lineSize < recordSize) {
    free(This->lineBuffer);
    This->lineSize = recordSize;
    This->lineBuffer = (char*)malloc((This->lineSize + 1) * sizeof(char));
  }

  This->separator = separator;

  return CS_SUCCESS;
}

CSRESULT
  CSCSV_IterNext
    (CSCSV* This,
     CSLIST record) {

  int i;
  int stop;
  
  char c;

  CSLIST_Clear(record);

  // Skip over empty lines

  while (1) {

    // read a character
    c = fgetc(This->file);

    if (feof(This->file)) {
      return CS_FAILURE;
    }
 
    if (c != '\r' && c != '\n') {
      break;
    }
  }

  i = 0;
  while(1) {

    if (c == '"') {

      // read a character
      c = fgetc(This->file);

      if (feof(This->file)) {
        return CS_FAILURE;
      }

      // The separator may be in the token; any double quote
      // is escaped with a preceding double quote

      stop = 0;
      while (!stop) {

        switch(c) {

          case '\r':

            This->lineBuffer[i] = c;
            i++;

            // read a character
            c = fgetc(This->file);

            if (feof(This->file)) {
              return CS_FAILURE;
            }

            break;

          case '\n':

            This->lineBuffer[i] = c;
            i++;

            // read a character
            c = fgetc(This->file);

            if (feof(This->file)) {
              return CS_FAILURE;
            }
            
            break;

          case '"':

            // two possibilities: if next character is double quote, then 
            // this is an escaped double quote part of the token; we must
            // therefore skip over two characters to reach the next
            // token character.
            // 
            // If the next character is not a double quote, then this marks
            // end of the token.

            // read next character
            c = fgetc(This->file);

            if (feof(This->file)) {
              // this means we have reached the field delimiter
              // and that there are no more fileds in the record
              This->lineBuffer[i] = 0;
              CSLIST_Insert(record, This->lineBuffer, 
                            strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);
              //printf("%s\n", This->lineBuffer);
              return CS_SUCCESS;
            }

            if (c == '"') {

              This->lineBuffer[i] = c;
              i++;
              // read a character
              c = fgetc(This->file);

              if (feof(This->file)) {
                return CS_FAILURE; 
              }
            }
            else {

              This->lineBuffer[i] = 0;
              i=0;
              CSLIST_Insert(record, This->lineBuffer, 
                            strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);

              // This is the end of the token, we should be 
              // at the separator else this is a format error.

              if (c == This->separator) {
                // skip over separator
                c = fgetc(This->file);

                if (feof(This->file)) {
                  return CS_SUCCESS;
                }
              }
              else {

                if (c == '\r' || c == '\n') {
                  // we are at the end of the line
                  return CS_SUCCESS;
                }
                else {
                  // format error
                  return CS_FAILURE;
                }
              }

              stop = 1; 
            }

            break;

          default:

            This->lineBuffer[i] = c;
            i++;

            // read a character
            c = fgetc(This->file);

            if (feof(This->file)) {
              return CS_FAILURE;
            }

            break;
        }

      }   

    }
    else {

      if (c == '\r' || c == '\n') {
        return CS_SUCCESS;
      }

      if (c == This->separator) {

        This->lineBuffer[i] = 0;
        i=0;
        CSLIST_Insert(record, This->lineBuffer, 
                        strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);

        do {
          // read a character
          c = fgetc(This->file);

          if (feof(This->file)) {
            return CS_SUCCESS;
          }
        }
        while (c == '\t' || c == ' ' );

      }
      else {

        This->lineBuffer[i] = c;
        i++;
        stop = 0;
        while (!stop) {

          // read a character
          c = fgetc(This->file);

          if (feof(This->file)) {
            This->lineBuffer[i] = 0;
            CSLIST_Insert(record, This->lineBuffer, 
                          strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);
            return CS_SUCCESS;            
          }

          if (c == This->separator) {
            
            This->lineBuffer[i] = 0;
            i=0;
            CSLIST_Insert(record, This->lineBuffer, 
                          strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);
            stop = 1;

            // read a character
            c = fgetc(This->file);

            if (feof(This->file)) {
              This->lineBuffer[i] = 0;
              CSLIST_Insert(record, This->lineBuffer, 
                            strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);
              return CS_SUCCESS;            
            }
          }
          else {

            switch(c) {

              case '\r':

                This->lineBuffer[i] = 0;
                i=0;
                CSLIST_Insert(record, This->lineBuffer, 
                              strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);

                return CS_SUCCESS;

              case '\n':

                This->lineBuffer[i] = 0;
                i=0;
                CSLIST_Insert(record, This->lineBuffer, 
                              strlen(This->lineBuffer) + 1, CSLIST_BOTTOM);

                return CS_SUCCESS;

              default:

                This->lineBuffer[i] = c;
                i++;
                break;
            }
          }  
        }
      }
    }
  }

  return CS_SUCCESS;
}

CSRESULT
  CSCSV_ImportToJSON
    (CSCSV* This,
     char separator,
     char* filePath,
     CSJSON pJson,
     CSLIST headings,
     long flags) {

  CSLIST tokens;
  CSLIST pHeadings;
  CSRESULT hResult;

  long row;
  long count;
  long i;
  long columnCount;
  long diag;

  char szPath[128];
  
  char* pszToken;
  char* pszHeading;

  tokens = CSLIST_Constructor();

  if (CS_SUCCEED(CSCSV_IterStart(This, separator, filePath, 65535))) {

    if (flags & CSCSV_MASK_ARRAY) {
      
      diag = 0;

      if (flags & CSCSV_HEADINGS) {

        CSJSON_Init(pJson, JSON_TYPE_OBJECT);

        if (headings != NULL) {
          pHeadings = headings;
        }
        else {
          pHeadings = CSLIST_Constructor();
          if (CS_FAIL(CSCSV_IterNext(This, pHeadings))) {
            CSLIST_Destructor(&pHeadings);
            return CS_FAILURE;
          }
        }

        CSJSON_MkDir(pJson, "/", "headings", JSON_TYPE_ARRAY);
        columnCount = CSLIST_Count(pHeadings);

        for (i=0; i<columnCount; i++) {
          CSLIST_GetDataRef(pHeadings, (void**)(&pszHeading), i);
          CSJSON_InsertString(pJson, "/headings", 0, pszHeading);
        }
        
        CSJSON_MkDir(pJson, "/", "data", JSON_TYPE_ARRAY);

        row = 0;
        while (CS_SUCCEED(CSCSV_IterNext(This, tokens))) {

          count = CSLIST_Count(tokens);

          if (count != columnCount) {

            diag = CSCSV_ERR_PARSE_NUMCOLUMNS;
 
            if (flags & CSCSV_MASK_KEEP_INVALIDREC) {

              CSJSON_MkDir(pJson, "/data", 0, JSON_TYPE_ARRAY);

              sprintf(szPath, "/data/%ld", row);

              if (count <= columnCount) {
                for (i=0; i<count; i++) {
                  CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
                  CSJSON_InsertString(pJson, szPath, 0, pszToken);
                }
              }
              else {

                for (i=0; i<columnCount; i++) {
                  CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
                  CSJSON_InsertString(pJson, szPath, 0, pszToken);
                }
              }

              row++;
            }
          }
          else {

            CSJSON_MkDir(pJson, "/data", 0, JSON_TYPE_ARRAY);

            sprintf(szPath, "/data/%ld", row);

            for (i=0; i<count; i++) {
              CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
              CSJSON_InsertString(pJson, szPath, 0, pszToken);
            }

            row++;
          }
        }

        if (headings == 0) {
          CSLIST_Destructor(&pHeadings);
        }

        hResult = CS_SUCCESS | diag;
      }
      else {

        CSJSON_Init(pJson, JSON_TYPE_ARRAY);

        row = 0;
        while (CS_SUCCEED(CSCSV_IterNext(This, tokens))) {

          CSJSON_MkDir(pJson, "/", 0, JSON_TYPE_ARRAY);

          count = CSLIST_Count(tokens);

          sprintf(szPath, "/%ld", row);

          for (i=0; i<count; i++) {
            CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
            CSJSON_InsertString(pJson, szPath, 0, pszToken);
          }

          row++;;
        }
      }

      hResult = diag | CS_SUCCESS;
    }
    else {

      diag = 0;
      CSJSON_Init(pJson, JSON_TYPE_ARRAY);

      if (headings != NULL) {
        pHeadings = headings;
      }
      else {
        pHeadings = CSLIST_Constructor();
        if (CS_FAIL(CSCSV_IterNext(This, pHeadings))) {
          CSLIST_Destructor(&pHeadings);
          return CS_FAILURE;
        }
      }

      columnCount = CSLIST_Count(pHeadings);

      row = 0;
      while (CS_SUCCEED(CSCSV_IterNext(This, tokens))) {

        count = CSLIST_Count(tokens);

        if (count != columnCount) {

          diag = CSCSV_ERR_PARSE_NUMCOLUMNS;

          if (flags & CSCSV_MASK_KEEP_INVALIDREC) {

            CSJSON_MkDir(pJson, "/", 0, JSON_TYPE_OBJECT);

            if (count <= columnCount) {
              for (i=0; i<count; i++) {
                sprintf(szPath, "/%ld", row);
                CSLIST_GetDataRef(pHeadings, (void**)(&pszHeading), i);
                CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
                CSJSON_InsertString(pJson, szPath, pszHeading, pszToken);
              }
            }
            else {
              for (i=0; i<columnCount; i++) {
                sprintf(szPath, "/%ld", row);
                CSLIST_GetDataRef(pHeadings, (void**)(&pszHeading), i);
                CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
                CSJSON_InsertString(pJson, szPath, pszHeading, pszToken);
              }
            }

            row++;
          }
        }
        else {

          CSJSON_MkDir(pJson, "/", 0, JSON_TYPE_OBJECT);

          for (i=0; i<count; i++) {
            sprintf(szPath, "/%ld", row);
            CSLIST_GetDataRef(pHeadings, (void**)(&pszHeading), i);
            CSLIST_GetDataRef(tokens, (void**)(&pszToken), i);
            CSJSON_InsertString(pJson, szPath, pszHeading, pszToken);
          }

          row++;
        }
      }

      if (headings == 0) {
        CSLIST_Destructor(&pHeadings);
      }

      hResult = CS_SUCCESS | diag;
    }
  }
  else {
    hResult = CS_FAILURE | CSCSV_ERR_PARSE;
  }

  CSLIST_Destructor(&tokens);

  return hResult;
}