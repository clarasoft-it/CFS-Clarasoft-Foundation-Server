/* ===========================================================================

  Clarasoft Core definitions - Linux
  cslib.h

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

#ifndef __CLARASOFT_CSLIB_H__
#define __CLARASOFT_CSLIB_H__

#include <inttypes.h>

#define CS_SUCCESS                     (0x00000000)
#define CS_FAILURE                     (0x80000000)

#define CS_MASK_OPER                   (0x0FFF0000)
#define CS_MASK_DIAG                   (0x0000FFFF)

#define CS_SUCCEED(x)                  (((x) & (0xF0000000)) == (0))
#define CS_FAIL(x)                     ((x) & (CS_FAILURE))
#define CS_OPER(x)                     ((x) & (CS_MASK_OPER))
#define CS_DIAG(x)                     ((x) & (CS_MASK_DIAG))

#define CS_OPER_NOVALUE                (0x00000000)
#define CS_DIAG_NOVALUE                (0x00000000)
#define CS_DIAG_UNKNOWN                (0x0000FFFF)

typedef long CSRESULT;

#define CSLIST_TOP                     (0x00000000)   // at the beginning of the
#define CSLIST_BOTTOM                  (0x7FFFFFFF)   // at the end of the list

#define CSMAP_ASCENDING                (0x0000001)
#define CSMAP_DESCENDING               (0x0000002)

#define CSSTR_B64_MODE_STRICT          (0x00000000)
#define CSSTR_B64_MODE_ACCEPTLINEBREAK (0x00000100)

#define CSSTR_B64_LINEBREAK_OFFSET     (0x0000004D)
#define CSSTR_B64_MASK_LINEBREAK       (0x0000000F)
#define CSSTR_B64_LINEBREAK_NONE       (0x00000000)
#define CSSTR_B64_LINEBREAK_LF         (0x00000001)
#define CSSTR_B64_LINEBREAK_CRLF       (0x00000002)
#define CSSTR_B64_IGNOREINVALIDCHAR    (0x00000100)

#define CSSTR_URLENCODE_SPACETOPLUS    (0x00000100)
#define CSSTR_URLENCODE_CONVERTALL     (0x00000200)

#define CSSYS_UUID_BUFFERSIZE          (37)
#define CSSYS_UUID_UPPERCASE           (0x00000000)
#define CSSYS_UUID_LOWERCASE           (0x00000001)
#define CSSYS_UUID_DASHES              (0x00000002)

#define RM_PATTERN_STARTSWITH          (0x00000001)
#define RM_PATTERN_CONTAINS            (0x00000002)


typedef void* CSLIST;
typedef void* CSMAP;
typedef void* CSSTRCV;

/* --------------------------------------------------------------------------
  Linked List
-------------------------------------------------------------------------- */

CSLIST
  CSLIST_Constructor
    (void);

CSRESULT
  CSLIST_Destructor
    (CSLIST*);

CSRESULT
  CSLIST_Insert
    (CSLIST This,
     void* value,
     long  valueSize,
     long  index);

CSRESULT
  CSLIST_Remove
    (CSLIST This,
     long   index);

CSRESULT
  CSLIST_Get
    (CSLIST This,
     void*  value,
     long   index);

CSRESULT
  CSLIST_Set
    (CSLIST This,
     void*  value,
     long   valueSize,
     long   index);

long
  CSLIST_Count
    (CSLIST This);

void
  CSLIST_Clear
    (CSLIST This);

long
  CSLIST_ItemSize
    (CSLIST This,
     long   index);

CSRESULT
  CSLIST_GetDataRef
    (CSLIST This,
     void** value,
     long   index);

/* --------------------------------------------------------------------------
  Hash Map
-------------------------------------------------------------------------- */

CSMAP
  CSMAP_Constructor
    (void);

CSRESULT
  CSMAP_Destructor
    (CSMAP*);

CSRESULT
  CSMAP_Insert
    (CSMAP This,
     char* key,
     void* value,
     long  valueSize);

CSRESULT
  CSMAP_InsertKeyRef
    (CSMAP This,
     char* key,
     void* value,
     long  valueSize);

CSRESULT
  CSMAP_Remove
    (CSMAP This,
     char* key);

CSRESULT
  CSMAP_Clear
    (CSMAP This);

CSRESULT
  CSMAP_Lookup
    (CSMAP  This,
     char*  key,
     void** value,
     long*  valueSize);

CSRESULT
  CSMAP_IterStart
    (CSMAP This,
     long mode);

CSRESULT
  CSMAP_IterNext
    (CSMAP  This,
     char** key,
     void** value,
     long*  valueSize);

long
  CSSTR_Trim
    (char* source,
     char* target);

long
  CSSTR_FromBase64
    (unsigned char* inBuffer,
     long inSize,
     unsigned char* outBuffer,
     long flags);

long
  CSSTR_ToBase64
    (unsigned char* inBuffer,
     long inSize,
     unsigned char* outBuffer,
     long flags);
     
int
  CSSTR_ToUpperCase
    (char* buffer,
     int size);

long
  CSSTR_UrlDecode
    (unsigned char* inStr,
     long InSize,
     unsigned char* out,
     long flags);

long
  CSSTR_UrlEncode
    (unsigned char* inStr,
     long InSize,
     unsigned char* out,
     long flags);

CSRESULT
  CSSYS_MakeUUID
    (char* szUUID,
     int mode);

CSRESULT
  CSSYS_RemoveFiles
    (char* szPath,
     char* szPattern,
     long patternMode,
     int days);

#endif

