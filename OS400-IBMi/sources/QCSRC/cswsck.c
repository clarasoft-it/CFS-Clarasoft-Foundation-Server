/* ==========================================================================

  Clarasoft Foundation Server - OS/400
  Websocket Protocol Implementation

  Distributed under the MIT license

  Copyright (c) 2017 Clarasoft I.T. Solutions Inc.

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

#include <errno.h>
#include <iconv.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "QSYSINC/H/QC3HASH"

#include "qcsrc/cslib.h"
#include "qcsrc/cshttp.h"

/* ---------------------------------------------------------------------------

--------------------------------------------------------------------------- */

#define CSWSCK_MASK_OPERATION        (0x0FFF0000)

#define CSWSCK_MASK_RECV_CACHE       (0x00000001)
#define CSWSCK_MASK_RECV_STREAM      (0x00000002)

#define CSWSCK_OP_CONTINUATION       (0x00)
#define CSWSCK_OP_TEXT               (0x01)
#define CSWSCK_OP_BINARY             (0x02)
#define CSWSCK_OP_CLOSE              (0x08)
#define CSWSCK_OP_PING               (0x09)
#define CSWSCK_OP_PONG               (0x0A)

#define CSWSCK_FIN_OFF               (0x00)
#define CSWSCK_FIN_ON                (0x01)

#define CSWSCK_RCVMODE_ALLOC         (0x00000001)
#define CSWSCK_RCVMODE_COPY          (0x00000002)

#define CSWSCK_OPER_CONTINUATION     (0x00000000)
#define CSWSCK_OPER_TEXT             (0x00010000)
#define CSWSCK_OPER_BINARY           (0x00020000)
#define CSWSCK_OPER_CLOSE            (0x00080000)
#define CSWSCK_OPER_PING             (0x00090000)
#define CSWSCK_OPER_PONG             (0x000A0000)
#define CSWSCK_OPER_CFSAPI           (0x0F010000)
#define CSWSCK_OPER_OPENCHANNEL      (0x0F020000)

#define CSWSCK_DIAG_WEBSOCKET        (0x00000001)
#define CSWSCK_DIAG_HTTP             (0x00000002)
#define CSWSCK_DIAG_UNKNOWNPROTO     (0x00008001)
#define CSWSCK_DIAG_NOTSUPPORTED     (0x00008002)
#define CSWSCK_DIAG_NOPROTOCOL       (0x00008003)
#define CSWSCK_DIAG_SESSION          (0x00008004)

#define CSWSCK_E_NODATA              (0x00000001)
#define CSWSCK_E_PARTIALDATA         (0x00000002)
#define CSWSCK_E_ALLDATA             (0x00000003)
#define CSWSCK_E_NOTSUPPORTED        (0x000000F1)
#define CSWSCK_E_READ                (0x000000F2)
#define CSWSCK_E_WRITE               (0x000000F3)

#define CSWSCK_MOREDATA              (0x00000000)
#define CSWSCK_ENDOFDATA             (0x00000001)

#define CSWSCK_SR_CACHE              (0x00000001)
#define CSWSCK_SR_STREAM             (0x00000002)

#define CSWSCK_Fin(x)                (((x) & (0x80)) >> 7 )
#define CSWSCK_OpCode(x)             ((x)  & (0x0F))
#define CSWSCK_MaskCode(x)           (((x) & (0x80)) >> 7 )
#define CSWSCK_BaseLength(x)         (((x) & (0x7F)) )

#define CSWSCK_OPERATION(x)          ((x) & CSWSCK_MASK_OPERATION)

#define CSWSCK_MAX_FRAGMENTSIZE      LONG_MAX

typedef struct tagCSWSCK {

  char* dataSlab;
  char* auxDataSlab;

  uint64_t dataSize;
  uint64_t dataSlabSize;
  uint64_t auxDataSlabSize;

  CFS_SESSION* Session;
  CSLIST pFragments;
  CSHTTP Http;
  CSSTRCV cvt;

} CSWSCK;


/* ----------------------------------------------------------------------------

---------------------------------------------------------------------------- */

uint64_t ntohll(uint64_t value)
{
  union  {
     char bytes[2];
     uint16_t testVal;
  }  Integer16;

  union  {
     char bytes[8];
     uint64_t hll;
  }  Integer64;

  // Determine host endian-ness
  Integer16.testVal = 0x0001;

  if (Integer16.bytes[0] == 0x01) {

    // Little endian, we must convert

    Integer64.bytes[0] = value >> 56;
    Integer64.bytes[1] = value >> 48;
    Integer64.bytes[2] = value >> 40;
    Integer64.bytes[3] = value >> 32;
    Integer64.bytes[4] = value >> 24;
    Integer64.bytes[5] = value >> 16;
    Integer64.bytes[6] = value >> 8;
    Integer64.bytes[7] = value >> 0;

    return Integer64.hll;
  }

  return value;
}

uint64_t htonll(uint64_t value)
{
  return (ntohll(value));
}

//////////////////////////////////////////////////////////////////////////////
//
// CSSTR_PRV_StrTok
//
// This function is like the strtok function but can handle
// delimiters more than one charater in length.
// Like the strtok function, the input buffer will be
// modified by the function.
//
//////////////////////////////////////////////////////////////////////////////

char*
  CSSTR_PRV_StrTok
    (char* szBuffer,
     char* szDelimiter)
{
  long i, k, found;
  long startDelim;
  long delimLength;

  static char* nextToken;

  char* token;

  if (szBuffer != 0)
  {
    nextToken = szBuffer;
    token = 0;
  }
  else
  {
    token = nextToken;
  }

  if (nextToken != 0)
  {
    delimLength = strlen(szDelimiter);
    found = 0;

    i=0;
    while (*(nextToken + i) != 0)
    {
      if (*(nextToken + i) == szDelimiter[0])
      {
        startDelim = i;

        k=0;
        while(*(nextToken + i) != 0 &&
              *(nextToken + i) == szDelimiter[k] &&
              k<delimLength)
        {
          k++;
          i++;
        }

        if (k == delimLength)
        {
          // this means we have found a delimiter
          found = 1;

          // null the delimter's first char
          *(nextToken + startDelim) = 0;

          break;
        }
      }
      else
      {
        i++;
      }
    }

    if (found == 0)
    {
      // this means there are no more tokens
      nextToken = 0;
    }
    else
    {
      token = nextToken;

      if (*(nextToken + i) == 0)
      {
        // empty string following the delimiter
        nextToken = 0;
      }
      else
      {
        nextToken = nextToken + i;
      }
    }
  }
  else
  {
    token = 0;
  }

  return token;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_Send
//
// Sends a websocket operation to a peer over a non secure connection.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_Send
    (CSWSCK*  This,
     char     operation,
     char*    data,
     uint64_t iDataSize,
     char     fin) {

  uint16_t iSize16;
  uint64_t iSize64;
  uint64_t iOutDataSize;

  char ws_header[14];

  char* pOutData;

  long SegmentSize;

  CSRESULT hResult;

  memset(ws_header, 0, 14);

  if (data != 0 && iDataSize > 0)
  {
    if (operation == CSWSCK_OP_TEXT) {

      /////////////////////////////////////////////////////////////////////
      // Convert data to UTF8
      /////////////////////////////////////////////////////////////////////

      CSSTRCV_SetConversion(This->cvt, "00000", "01208");
      CSSTRCV_StrCpy(This->cvt, data, iDataSize);
      iOutDataSize = CSSTRCV_Size(This->cvt);

      if (This->auxDataSlabSize < iOutDataSize) {
        free(This->auxDataSlab);
        This->auxDataSlabSize = iOutDataSize;
        This->auxDataSlab =
          (char*)malloc(This->auxDataSlabSize * sizeof(char));
      }

      CSSTRCV_Get(This->cvt, This->auxDataSlab);
      pOutData = This->auxDataSlab;
    }
    else {
      pOutData = data;
      iOutDataSize = iDataSize;
    }

    if (fin & CSWSCK_FIN_ON) {
      ws_header[0] = 0x80 | operation;
    }
    else {
      ws_header[0] = 0x00 | operation;
    }

    if (iOutDataSize < 126) {
      ws_header[1] = 0x00 | iOutDataSize;
      SegmentSize = 2;
    }
    else if (iOutDataSize < 65536) {
      ws_header[1] = 0x00 | 126;
      iSize16 = htons(iOutDataSize);
      memcpy(&ws_header[2], &iSize16, sizeof(uint16_t));
      SegmentSize = 4;
    }
    else {
      ws_header[1] = 0x00 | 127;

      // For Portability; AS400 is already in NBO
      iSize64 = htonll(iOutDataSize);
      memcpy(&ws_header[2], &iSize64, sizeof(uint64_t));
      SegmentSize = 10;
    }

    // Send frame header.
    hResult = This->Session->lpVtbl->CFS_SendRecord(This->Session,
                                ws_header,
                                &SegmentSize, 1);

    if (CS_SUCCEED(hResult)) {

      SegmentSize = (long)iOutDataSize;
      hResult = This->Session->lpVtbl->CFS_SendRecord(This->Session,
                                 pOutData,
                                 &SegmentSize, 1);
    }
  }
  else {

    ////////////////////////////////////////////////////////////////////////
    // Send empty frame: the operation should only be
    // one of the following:
    //
    //   CLOSE
    //   PING
    //   PONG
    //
    ////////////////////////////////////////////////////////////////////////

    switch(operation) {

      case CSWSCK_OP_PING:
      case CSWSCK_OP_PONG:
      case CSWSCK_OP_CLOSE:

         ws_header[0] = 0x80 | operation;
         ws_header[1] = 0x00;

         SegmentSize = 2;

         hResult = This->Session->lpVtbl->CFS_SendRecord
                                 (This->Session,
                                  ws_header,
                                  &SegmentSize, 1);

         break;

      default:

        hResult = CS_FAILURE | operation | CSWSCK_E_NOTSUPPORTED;
    }
  }

  if (CS_SUCCEED(hResult)) {
     hResult = CS_SUCCESS;
  }
  else {
     hResult = CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
  }

  return hResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_Receive
//
// Waits for a websocket operation from a peer.
//
// If a buffer is provided (szBuffer is non-NULL), then the fuction will
// fill up the buffer as fragements (FIN is off) are forthcoming as long
// as the number of bytes received does not exceed the value of iDataSize.
//
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_Receive
    (CSWSCK* This,
     uint64_t* iDataSize,
     long toSlices) {

  char ws_mask[4];
  char ws_header[14];

  static int cvtFlag = 0;

  int iBaseDataLength;

  unsigned long MaskIsOn;

  uint16_t iDataSize_16;
  uint64_t iDataSize_64;
  uint64_t i, j;

  long SegmentSize;
  long fragmentSize;

  CSRESULT hResult;

  //////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////
  //
  // Branching label
  CSWSCK_LABEL_RECEIVE:
  //
  //////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////

  // A websocket frame has at least 2 bytes

  SegmentSize = 2;

  hResult = This->Session
                ->lpVtbl
                ->CFS_ReceiveRecord
                             (This->Session,
                              ws_header,
                              &SegmentSize, toSlices);

  if (CS_SUCCEED(hResult)) {

    ///////////////////////////////////////////////////////////////////
    // Examine the basic length byte; this will determine
    // how many additional bytes we will be reading next.
    ///////////////////////////////////////////////////////////////////

    iBaseDataLength = CSWSCK_BaseLength(ws_header[1]);

    switch(iBaseDataLength)
    {
      case 126:

        SegmentSize = 2;

        hResult = This->Session
                       ->lpVtbl
                       ->CFS_ReceiveRecord
                                    (This->Session,
                                     (char*)&iDataSize_16,
                                     &SegmentSize, toSlices);

        if (CS_FAIL(hResult)) {

          *iDataSize = 0;
          return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
        }

        This->dataSize = ntohs(iDataSize_16);
        break;

      case 127:

        SegmentSize = 8;

        hResult = This->Session
                       ->lpVtbl
                       ->CFS_ReceiveRecord
                                    (This->Session,
                                     (char*)&iDataSize_64,
                                     &SegmentSize, toSlices);

        if (CS_FAIL(hResult)) {

          *iDataSize = 0;
          return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
        }

        This->dataSize = ntohll(iDataSize_64);
        break;

      default:

        This->dataSize = iBaseDataLength;

        break;
    }

    ////////////////////////////////////////////////////////////////
    // If this is a PING or a PONG or a CLOSE ...
    ////////////////////////////////////////////////////////////////

    switch(CSWSCK_OpCode(ws_header[0])) {

      case CSWSCK_OP_CLOSE:

        *iDataSize = 0;
        return  CS_FAILURE | CSWSCK_OPER_CLOSE;

      case CSWSCK_OP_PING:

        // Send PONG response

        if (This->dataSize > 0) {

          /////////////////////////////////////////////////////////////
          // We got data to send back
          /////////////////////////////////////////////////////////////

          MaskIsOn = 0;

          if (CSWSCK_MaskCode(ws_header[1]) == 1) {

            MaskIsOn = 1;
            SegmentSize = 4;

            hResult = This->Session
                          ->lpVtbl
                          ->CFS_ReceiveRecord
                                       (This->Session,
                                        ws_mask,
                                        &SegmentSize, toSlices);
          }

          if (CS_FAIL(hResult)) {
            *iDataSize = 0;
            return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
          }

          SegmentSize = (long)(This->dataSize);
          hResult = This->Session
                        ->lpVtbl
                        ->CFS_ReceiveRecord
                                     (This->Session,
                                      This->dataSlab,
                                      &SegmentSize, toSlices);

          if (CS_FAIL(hResult)) {
            *iDataSize = 0;
            return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
          }

          if (MaskIsOn == 1) {

            /////////////////////////////////////////////////////////////
            // Unmask the data
            /////////////////////////////////////////////////////////////

            i=0;
            while (i<This->dataSize) {
              This->dataSlab[i] = This->dataSlab[i] ^ ws_mask[i % 4];
              i++;
            }
          }

          hResult = CSWSCK_Send(This,
                                CSWSCK_OP_PONG,
                                This->dataSlab,
                                This->dataSize,
                                CSWSCK_FIN_ON);

          if (CS_FAIL(hResult)) {
            *iDataSize = 0;
            return hResult;
          }
        }
        else {

          ////////////////////////////////////////////////////////////////
          // We got no data and hence just return an empty PONG.
          ////////////////////////////////////////////////////////////////

          hResult = CSWSCK_Send(This,
                                CSWSCK_OP_PONG,
                                0,
                                0,
                                CSWSCK_FIN_ON);

          if (CS_FAIL(hResult)) {
            *iDataSize = 0;
            return hResult;
          }
        }

        // Resume reading ...
        goto CSWSCK_LABEL_RECEIVE;

      case CSWSCK_OP_PONG:

        // Resume reading ...
        goto CSWSCK_LABEL_RECEIVE;
    }

    ////////////////////////////////////////////////////////////////
    // We now read the data, if any
    ////////////////////////////////////////////////////////////////

    if (This->dataSize > 0) {

      if (This->dataSlabSize < This->dataSize) {
        free(This->dataSlab);
        This->dataSlabSize = This->dataSize;
        This->dataSlab = (char*)malloc
              ((This->dataSlabSize + 1) * sizeof(char));
      }

      /////////////////////////////////////////////////////////////
      // Determine if we have a mask (we should always get one) and
      // read it.
      /////////////////////////////////////////////////////////////

      if (CSWSCK_MaskCode(ws_header[1]) == 1) {

        MaskIsOn = 1;
        SegmentSize = 4;

        hResult = This->Session->lpVtbl->CFS_ReceiveRecord(This->Session,
                                  ws_mask,
                                  &SegmentSize, toSlices);

        if (CS_FAIL(hResult)) {

          *iDataSize = 0;
          return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
        }
      }
      else {
        MaskIsOn = 0;
      }

      fragmentSize = (long)(This->dataSize);

      hResult = This->Session->lpVtbl->CFS_ReceiveRecord(This->Session,
                                  This->dataSlab,
                                  &fragmentSize, toSlices);

      if (CS_FAIL(hResult)) {
        *iDataSize = 0;
        This->dataSlab[0] = 0;
        return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
      }

      if (MaskIsOn == 1)
      {
        /////////////////////////////////////////////////////////////
        // Unmask the data
        /////////////////////////////////////////////////////////////

        for (j=0; j<This->dataSize; j++) {
            *(This->dataSlab + j) =
                  *(This->dataSlab + j) ^ ws_mask[j % 4];
        }
      }

      This->dataSlab[This->dataSize] = 0; // NULL-terminate

      *iDataSize = fragmentSize;

      switch(CSWSCK_OpCode(ws_header[0])) {

        case CSWSCK_OP_TEXT:

          cvtFlag = 1;

          ///////////////////////////////////////////////////////////////////
          // Convert data to UTF8
          ///////////////////////////////////////////////////////////////////

          CSSTRCV_SetConversion(This->cvt, "01208", "00000");
          CSSTRCV_StrCpy(This->cvt, This->dataSlab, This->dataSize);
          This->dataSize = CSSTRCV_Size(This->cvt);

          if (This->dataSlabSize < This->dataSize) {
            free(This->dataSlab);
            This->dataSlabSize = This->dataSize;
            This->dataSlab =
              (char*)malloc((This->dataSlabSize + 1) * sizeof(char));
          }

          CSSTRCV_Get(This->cvt, This->dataSlab);
          This->dataSlab[This->dataSize] = 0; // NULL-terminate

          hResult =   CS_SUCCESS
                    | CSWSCK_OPER_TEXT
                    | CSWSCK_Fin(ws_header[0]);
          break;

        case CSWSCK_OP_BINARY:

          cvtFlag = 0;

          hResult =    CS_SUCCESS
                     | CSWSCK_OPER_BINARY
                     | CSWSCK_Fin(ws_header[0]);
          break;

        case CSWSCK_OP_CONTINUATION:

          if (cvtFlag == 1) {

            /////////////////////////////////////////////////////////////////
            // Convert data to UTF8
            /////////////////////////////////////////////////////////////////

            CSSTRCV_SetConversion(This->cvt, "01208", "00000");
            CSSTRCV_StrCpy(This->cvt, This->dataSlab, This->dataSize);
            This->dataSize = CSSTRCV_Size(This->cvt);

            if (This->dataSlabSize < This->dataSize) {
              free(This->dataSlab);
              This->dataSlabSize = This->dataSize;
              This->dataSlab =
                (char*)malloc((This->dataSlabSize + 1) * sizeof(char));
            }

            CSSTRCV_Get(This->cvt, This->dataSlab);
            This->dataSlab[This->dataSize] = 0; // NULL-terminate
          }

          hResult =   CS_SUCCESS
                    | CSWSCK_OPER_CONTINUATION
                    | CSWSCK_Fin(ws_header[0]);
          break;
       }
    }
    else {

      // No data in frame
      This->dataSize = 0;
      This->dataSlab[0] = 0;

      *iDataSize += 0;  // in case this is a partial segment so we don't lose
                        // how musch has already been received.

      ////////////////////////////////////////////////////////////////////
      // if there is a mask, we must read it
      ////////////////////////////////////////////////////////////////////

      if (CSWSCK_MaskCode(ws_header[1]) == 1) {

        SegmentSize = 4;

        hResult = This->Session->lpVtbl->CFS_ReceiveRecord(This->Session,
                                    ws_mask,
                                    &SegmentSize, toSlices);
      }

      if (CS_FAIL(hResult)) {

        return CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
      }

      // If there is no data. then the FIN flag should be set
      // but who knows, maybe the client sends empty frames
      // followed by non-empty frames...

      if (!CSWSCK_Fin(ws_header[0])) {
        goto CSWSCK_LABEL_RECEIVE;
      }
      else {

        switch(CSWSCK_OpCode(ws_header[0])) {

          case CSWSCK_OP_TEXT:

            hResult =   CS_SUCCESS
                      | CSWSCK_OPER_TEXT
                      | CSWSCK_Fin(ws_header[0]);
            break;

          case CSWSCK_OP_BINARY:

            hResult =   CS_SUCCESS
                      | CSWSCK_OPER_BINARY
                      | CSWSCK_Fin(ws_header[0]);
            break;

          case CSWSCK_OP_CONTINUATION:

            hResult =   CS_SUCCESS
                      | CSWSCK_OPER_CONTINUATION
                      | CSWSCK_Fin(ws_header[0]);
            break;
        }
      }
    }
  }
  else {
    // Error getting first 2 bytes of protocol header
    hResult = CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
  }

  return hResult;
}

CSRESULT
  CSWSCK_ReceiveAll
    (CSWSCK* This,
     uint64_t* DataSize,
     long toSlice) {

  CSRESULT hResult;

  char* lpszFragment;

  long Count;
  long fragmentSize;
  long offset;
  long i;

  uint64_t Size;

  CSLIST_Clear(This->pFragments);

  *DataSize = 0;
  hResult = CSWSCK_Receive(This, &Size, toSlice);

  if (CS_SUCCEED(hResult)) {

    *DataSize += Size;
    CSLIST_Insert(This->pFragments, (void*)(This->dataSlab), Size, CSLIST_BOTTOM);

    if (CS_DIAG(hResult) == CSWSCK_FIN_OFF) {

      while (1) {

        hResult = CSWSCK_Receive(This, &Size, 1);

        if (CS_SUCCEED(hResult)) {

          *DataSize += Size;
          CSLIST_Insert(This->pFragments, (void*)(This->dataSlab), Size, CSLIST_BOTTOM);

          if (CS_DIAG(hResult) == CSWSCK_FIN_ON) {
            break;
          }
        }
        else {
          *DataSize = 0;
          break;
        }
      }
    }

    This->dataSize = *DataSize;

    if (This->dataSize > 0) {

      if (This->dataSlabSize < This->dataSize) {
        free(This->dataSlab);
        This->dataSlabSize = This->dataSize;
        This->dataSlab = (char*)malloc
              ((This->dataSlabSize + 1) * sizeof(char));
      }

      Count = CSLIST_Count(This->pFragments);

      for (i=0, offset=0; i<Count; i++, offset+=fragmentSize) {

        fragmentSize = CSLIST_ItemSize(This->pFragments, i);
        CSLIST_GetDataRef(This->pFragments, (void**)&lpszFragment, i);
        memcpy(This->dataSlab + offset, lpszFragment, fragmentSize);
      }

      This->dataSlab[This->dataSize] = 0;
    }
  }

  return hResult;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_Constructor
//
// Creates a CSWSCK instance
//
//////////////////////////////////////////////////////////////////////////////

CSWSCK*
  CSWSCK_Constructor
    (void) {

  CSWSCK* Instance;

  Instance = (CSWSCK*)malloc(sizeof(CSWSCK));

  Instance->pFragments = CSLIST_Constructor();
  Instance->Session = 0;
  Instance->Http = CSHTTP_Constructor();

  Instance->dataSlabSize = 65536LL;
  Instance->dataSlab = (char*)malloc
              ((Instance->dataSlabSize + 1) * sizeof(char));
  Instance->dataSize = 0;

  Instance->auxDataSlabSize = 65536LL;
  Instance->auxDataSlab =
        (char*)malloc((Instance->dataSlabSize + 1) * sizeof(char));

  return Instance;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_Destructor
//
// Releases the resources allocated by a CSWSCK instance
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_Destructor
    (CSWSCK** This) {

  if (This != NULL && *This != NULL) {

    CSLIST_Destructor(&((*This)->pFragments));
    CSHTTP_Destructor(&((*This)->Http));
    free((*This)->dataSlab);
    free((*This)->auxDataSlab);
    free(*This);

    return CS_SUCCESS;
  }

  return CS_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_Close
//
// Closes the websocket session: this sends the CLOSE websocket operation
// to the peer. The connection is then closed.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_CloseSession
    (CSWSCK* This,
     char* szBuffer,
     uint64_t iDataSize)
{
  if (This->Session != 0) {

    // Send Websocket close to client
    CSWSCK_Send(This, CSWSCK_OP_CLOSE,
                            szBuffer, iDataSize, CSWSCK_FIN_ON);

    CFS_CloseSession(&(This->Session));
    This->Session = 0;
    return CS_SUCCESS;
  }

   return CS_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_CloseChannel
//
// Closes the websocket session: this sends the CLOSE websocket operation
// to the peer. The connection is then closed.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_CloseChannel
    (CSWSCK* This,
     char* szBuffer,
     uint64_t iDataSize)
{
  if (This->Session != 0) {

    // Send Websocket close to client
    CSWSCK_Send(This, CSWSCK_OP_CLOSE,
                            szBuffer, iDataSize, CSWSCK_FIN_ON);

    CFS_CloseChannel(&(This->Session));
    This->Session = 0;
    return CS_SUCCESS;
  }

   return CS_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_GetData
//
// Copies the data received from a peer to a supplied buffer. This function
// can be used on either secure or non-secure connections.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_GetData
    (CSWSCK* This,
     char* szBuffer,
     uint64_t offset,
     uint64_t iMaxDataSize) {

  if (iMaxDataSize <= This->dataSize - offset) {
    memcpy(szBuffer, This->dataSlab + offset, iMaxDataSize);
  }
  else {
    memcpy(szBuffer, This->dataSlab + offset, This->dataSize - offset);
  }

  return CS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_GetDataRef
//
// Returns the address of the data slab.
//
//////////////////////////////////////////////////////////////////////////////

void*
  CSWSCK_GetDataRef
    (CSWSCK* This) {

  return This->dataSlab;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_OpenChannel
//
// Opens a non-secure websocket server session.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_OpenChannel
    (CSWSCK* This,
     CFSENV  pEnv,
     int     connfd) {


  CSRESULT hResult;

  char InpuFmt[9];
  char AlgDesc[9];
  char szHash[21];
  char szKeyHash[129];
  char szHTTPResponse[1024];
  char szBuffer[1025];

  char HashProvider;

  char* szHeader;
  char* pTok;

  int i;
  int n;
  int iSize;

  union {
    char bError[48];
    int iBytesProvided;
  } error;

  union {
    char padding[20];
    int Algorithm;
  }HashAlgorithm;

  unsigned char szChallenge[129];

  long found;
  long len;
  long size;

  if (This->Session != 0) {
    CFS_CloseChannel(&(This->Session));
  }

  if ((This->Session = CFS_OpenChannel
                            (pEnv,
                             connfd)) == 0) {

    return CS_FAILURE | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_SESSION;
  }

  ///////////////////////////////////////////////////////////////
  // Client sent HTTP request: if we get a connection upgrade,
  // then we switch to websocket protocole, otherwise, this is
  //  a regular HTTP request
  ///////////////////////////////////////////////////////////////

  if (CS_FAIL(CSHTTP_RecvRequest(This->Http, This->Session))) {
    CFS_CloseSession(&(This->Session));
    return CS_FAILURE | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_UNKNOWNPROTO;
  }

  if (strcmp(CSHTTP_GetRequestMethod(This->Http), "GET")) {
    CFS_CloseSession(&(This->Session));
    return CS_SUCCESS | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_HTTP;
  }

  // Check for "Connection" header with value of "Upgrade"

  szHeader = CSHTTP_GetStdHeader(This->Http, CSHTTP_Connection);

  if (szHeader == 0) {
    CFS_CloseSession(&(This->Session));
    return CS_SUCCESS | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_HTTP;
  }

  len = strlen(szHeader);
  CSSTR_ToUpperCase(szHeader, len);

  ///////////////////////////////////////////////////////
  // Browsers may send a comma separated list of values.
  // Firefox sends the following header:
  //
  // Connection: keep-alive, Upgrade
  //
  // So we must parse the header
  ///////////////////////////////////////////////////////

  found = 0;
  pTok = CSSTR_PRV_StrTok(szHeader, ",");

  if (pTok) {

    CSSTR_Trim(pTok, szBuffer);
    if (!strcmp(szBuffer, "UPGRADE")) {
      found = 1;
    }
    else {
      while ((pTok = CSSTR_PRV_StrTok(0, ",")) != 0) {
        CSSTR_Trim(pTok, szBuffer);
        if (!strcmp(szBuffer, "UPGRADE")) {
          found = 1;
          break;
        }
      }
    }
  }
  else {
    CSSTR_Trim(szHeader, szBuffer);
    if (!strcmp(szBuffer, "UPGRADE")) {
      found = 1;
    }
  }

  if (!found) {
    CFS_CloseSession(&(This->Session));
    return CS_SUCCESS | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_HTTP;
  }

  // Get name of protocol upgrade: must be websocket otherwise
  // we don't support any other, so far

  szHeader = CSHTTP_GetStdHeader(This->Http, CSHTTP_Upgrade);

  if (szHeader == 0) {
    // Missing upgrade request; can't determine protocol
    CFS_CloseSession(&(This->Session));
    return CS_FAILURE | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_NOPROTOCOL;
  }

  strcpy(szBuffer, szHeader);
  CSSTR_ToUpperCase(szBuffer, 9);
  szBuffer[9] = 0;

  if (strcmp(szBuffer, "WEBSOCKET")) {
    CFS_CloseSession(&(This->Session));
    return CS_FAILURE | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_NOTSUPPORTED;
  }

  szHeader = CSHTTP_GetStdHeader(This->Http, CSHTTP_Sec_WebSocket_Key);

  if (szHeader != 0) {
    strcpy((char*)szKeyHash, szHeader);
  }
  else {
    szKeyHash[0] = 0;
  }

  /////////////////////////////////////////////////////////////////////
  // We send the response to the client. The client sent us
  // a challenge. We must compute SHA-1 hash to challenge
  // and then encoding it to Base 64.
  /////////////////////////////////////////////////////////////////////

  // WebSocket protocol requires to append this UUID to Key header
  strcat((char*)szKeyHash, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

  // compute SHA-1 hash; for this we need to convert challenge to ASCII
  CSSTRCV_SetConversion(This->cvt, "00000", "00819");
  CSSTRCV_StrCpy(This->cvt, szKeyHash, strlen(szKeyHash));
  CSSTRCV_Get(This->cvt, szKeyHash);

  HashAlgorithm.Algorithm = 2;  // SHA-1
  HashProvider = '0';
  error.iBytesProvided = 48;

  memset(InpuFmt, 0, 9);
  memset(AlgDesc, 0, 9);

  iSize = strlen(szKeyHash);
  Qc3CalculateHash(szKeyHash,
                   &iSize,
                   "DATA0100",
                   HashAlgorithm.padding,
                   "ALGD0500",
                   &HashProvider,
                   0,
                   szHash,
                   &error);

  szHash[20] = 0;  // null terminate resulting hash

  memset(szChallenge, 0, 129);

  /////////////////////////////////////////////////////////////////////
  // The hash needs to be encoded to BASE64
  /////////////////////////////////////////////////////////////////////

  CSSTR_ToBase64(szHash, 20, szChallenge,
                 CSSTR_INPUT_ASCII | CSSTR_OUTPUT_ASCII);

  CSSTRCV_SetConversion(This->cvt, "00819", "00000");
                CSSTRCV_StrCpy(This->cvt, szChallenge, strlen(szChallenge));
                CSSTRCV_Get(This->cvt, szChallenge);

  // make HTTP response

  sprintf(szHTTPResponse,
                  "HTTP/1.1 101 Switching Protocols\x0D\x25"
                  "Upgrade: websocket\x0D\x25"
                  "Sec-WebSocket-Accept: %s\x0D\x25"
                  "Connection: Upgrade\x0D\x25\x0D\x25",
                  szChallenge);


  // send handshake response
  size = strlen(szHTTPResponse);

  // Convert response to ASCII
  CSSTRCV_SetConversion(This->cvt, "00000", "00819");
  CSSTRCV_StrCpy(This->cvt, szHTTPResponse, size);
  CSSTRCV_Get(This->cvt, szHTTPResponse);

  // send handshake response
  size = strlen(szHTTPResponse);

  hResult = This->Session->lpVtbl->CFS_SendRecord
                     (This->Session,
                      szHTTPResponse,
                      &size, 1);

  if (CS_FAIL(hResult)) {
    CFS_CloseSession(&(This->Session));
    return CS_FAILURE;
  }

  return CS_SUCCESS | CSWSCK_OPER_OPENCHANNEL | CSWSCK_DIAG_WEBSOCKET;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_OpenSession
//
// Establishes a client connection to a websocket server.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_OpenSession
    (CSWSCK* This,
     CFSENV  pEnv,
     char*   szConfig,
     char*   szHost,
     char*   szPort) {

  SESSIONINFO* pSessionInfo;

  if (This->Session != 0) {
    CFS_CloseSession(&(This->Session));
  }

  ////////////////////////////////////////////////////////////////////
  //
  // Submit following request:
  //
  //   GET / HTTP/1.1\r\n
  //   Host: %s\r\n
  //   Upgrade: websocket\r\n
  //   Connection: Upgrade\r\n
  //   Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n
  //   Sec-WebSocket-Protocol: cfs-websocket-generic\r\n
  //   Sec-WebSocket-Version: 13\r\n\r\n
  //
  ////////////////////////////////////////////////////////////////////

  CSHTTP_StartRequest(This->Http,
                      CSHTTP_METHOD_GET,
                      CSHTTP_VER_1_1, "/");

  CSHTTP_SetStdHeader(This->Http, CSHTTP_Upgrade,
                                 "websocket");
  CSHTTP_SetStdHeader(This->Http, CSHTTP_Connection,
                                 "Upgrade");
  CSHTTP_SetStdHeader(This->Http, CSHTTP_Sec_WebSocket_Key,
                                  "x3JJHMbDL1EzLkh9GBhXDw==");
  CSHTTP_SetStdHeader(This->Http, CSHTTP_Sec_WebSocket_Protocol,
                                  "cfs-websocket-generic");
  CSHTTP_SetStdHeader(This->Http, CSHTTP_Sec_WebSocket_Version,
                                  "13");

  if ((This->Session = CFS_OpenSession
                  (pEnv,
                   szConfig,
                   szHost,
                   szPort)) == 0) {

    return CS_FAILURE;
  }

  if (szHost == 0) {
    pSessionInfo = CFS_QuerySessionInfo(This->Session);
    CSHTTP_SetStdHeader(This->Http, CSHTTP_Host, pSessionInfo->szHost);
  }
  else {
    CSHTTP_SetStdHeader(This->Http, CSHTTP_Host, szHost);
  }

  //////////////////////////////////////////////////////////////////
  // HTTP object must use this instance's network interface;
  // this means we will open the network connection with
  // the HTTP object but we will not close it with it.
  // We must keep the connection going and we will close
  // it through our This->Sesssion instance. So we pass the
  // CFS sesion handle to the CSHTTP_SendRequest function
  //////////////////////////////////////////////////////////////////

  // Submit HTTP request; if server returns HTTP status 101, then
  // the websocket handshake worked.

  if (CS_SUCCEED(CSHTTP_SendRequest(This->Http,
                                    This->Session))) {

    switch(atoi(CSHTTP_GetRespStatus(This->Http))) {
      case 101:  // This is the successful upgrade status
        return CS_SUCCESS;
      default:
        return CS_FAILURE;
    }
  }

  return CS_FAILURE;
}

//////////////////////////////////////////////////////////////////////////////
//
// CSWSCK_Ping
//
// Sends a PING request over a non-secure connection.
//
//////////////////////////////////////////////////////////////////////////////

CSRESULT
  CSWSCK_Ping
    (CSWSCK* This,
     char* szData,
     uint64_t iDataSize) {

  char szFrameHdr[2];

  long iHeaderSize;
  long size;

  CSRESULT hResult;

  // PING operation code and header size

  szFrameHdr[0] = 0x89;
  iHeaderSize = 2;

  if (szData != NULL) {

    if (iDataSize > 125) {

      //////////////////////////////////////////////////////////////////////
      // Must not send more than 125 bytes; an EBCDIC
      // characters may translate to more than one byte
      // therefore causing overflow...
      //////////////////////////////////////////////////////////////////////

      return CS_FAILURE | CSWSCK_OPER_PING | CSWSCK_E_PARTIALDATA;
    }
    else {

      // Just take first 125 bytes; we need to down cast to long integer
      szFrameHdr[1] = 0x00 | iDataSize;

      // Send header
      hResult = This->Session->lpVtbl->CFS_SendRecord(This->Session,
                               szFrameHdr,
                               &iHeaderSize, 1);

      // Send data

      if (CS_SUCCEED(hResult)) {

        // we need to dowen cast to long integer.
        size = (long)iDataSize;
        hResult = This->Session->lpVtbl->CFS_SendRecord(This->Session,
                                 szData,
                                 &size, 1);
      }
    }
  }
  else {

    szFrameHdr[1] = 0x00;

    // Send header only
    hResult = This->Session->lpVtbl->CFS_SendRecord(This->Session,
                             szFrameHdr,
                             &iHeaderSize, 1);
  }

  if (CS_FAIL(hResult)) {

    hResult = CS_FAILURE | CSWSCK_OPER_CFSAPI | CS_DIAG(hResult);
  }

  return hResult;
}
