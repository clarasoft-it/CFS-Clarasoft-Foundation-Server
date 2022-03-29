#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

#include<clarasoft/cslib.h>
#include<clarasoft/cfsapi.h>
#include<clarasoft/cswsck.h>
#include<clarasoft/csap.h>

int main(int argc, char** argv) {

    char szBuffer[256];
    char szTrim[256];
    char* pTok;
    char* pData;

    CSWSCK pWS;
    int e;
    uint64_t size;
    CSAP pCSAP;
    CSAPSTAT handshake;
    CSAPCTL ctl;

    if (argc < 2) {
      printf("\nMissing argument: ws | csap\n");
      return 1;
    }

    if (!strcmp(argv[1], "ws")) {

      pWS = CSWSCK_Constructor();

      CSWSCK_OpenSession(pWS, 0, 0, "localhost", "11007", &e);

      strcpy(szBuffer, "{\"op\":\"open\", \"service\":\"CSAP/SERVICE/ECHO\", \"u\":\"\", \"p\":\"\"}");
      size = strlen(szBuffer);

      CSWSCK_Send(pWS, CSWSCK_OPER_TEXT, szBuffer, size, CSWSCK_FIN_ON, -1);
      CSWSCK_Receive(pWS, &size, -1);
      pData = (char*)CSWSCK_GetDataRef(pWS);
      printf("\n%s", pData);

      strcpy(szBuffer, "CSAP070000000061fc744e-ac76-11ec-b909-0242ac120002TEXT      00000000000000000012Hello world!");

      CSWSCK_Send(pWS, CSWSCK_OPER_TEXT, szBuffer, size, CSWSCK_FIN_ON, -1);
      CSWSCK_Receive(pWS, &size, -1);
      pData = (char*)CSWSCK_GetDataRef(pWS);
      printf("\n%s\n", pData);

      CSWSCK_Destructor(&pWS);
    }
    else if (!strcmp(argv[1], "csap")) {

      pCSAP = CSAP_Constructor();

      if (CS_FAIL(CSAP_OpenService(pCSAP, "CSAP/CLIENT/ECHO_LOCAL", &handshake))) {
        printf("\nCSAP failed");
        return 1;
      }

      strcpy(szBuffer, "Hello world!");
      CSAP_Put(pCSAP, szBuffer, strlen(szBuffer));
      CSAP_Send(pCSAP, 0, 0, 0, CSAP_FMT_DEFAULT);

      if (CS_SUCCEED(CSAP_Receive(pCSAP, &ctl))) {
        pData = CSAP_GetDataRef(pCSAP);
        printf("\n%s\n", pData);
      }

      CSAP_CloseService(pCSAP);
      CSAP_Destructor(&pCSAP);
    }
    else {
      printf("\n invalid argument: use csap | ws\n");
    }

    return 0;
}
