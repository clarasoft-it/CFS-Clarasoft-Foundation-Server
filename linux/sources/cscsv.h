#ifndef __CLARASOFT_CSCSV_H__
#define __CLARASOFT_CSCSV_H__


#define CSCSV_CVT_MAP              0x00000000
#define CSCSV_CVT_ARRAY            0x00001000

#define CSCSV_NO_HEADINGS          0x00000000
#define CSCSV_HEADINGS             0x00000001

#define CSCSV_DISCARD_INVALIDREC   0x00000010
#define CSCSV_KEEP_INVALIDREC      0x00000020

#define CSCSV_ERR_PARSE            0x00000001
#define CSCSV_ERR_PARSE_NUMCOLUMNS 0x00000002

typedef void* CSCSV;

CSCSV
  CSCSV_Constructor
    (void);

CSRESULT
  CSCSV_Destructor
    (CSCSV* This);

CSRESULT
  CSCSV_IterStart
    (CSCSV This,
     char separator,
     char* filePath,
     long recordSize);

CSRESULT
  CSCSV_IterNext
    (CSCSV This,
     CSLIST record);

CSRESULT
  CSCSV_ImportToJSON
    (CSCSV This,
     char separator,
     char* filePath,
     CSJSON pJson,
     CSLIST Headings,
     long flags);

#endif

