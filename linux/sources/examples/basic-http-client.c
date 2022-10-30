#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clarasoft/cfs.h>

// gcc -g basic-http-client.c -o driver -lcfsapi

int main() {

  long size;

  char szBuffer[4097];

  CSHTTP pWeb;
  CFS_SESSION* pSession;

  pWeb = CSHTTP_Constructor();

  CSHTTP_StartRequest(pWeb, 
                      CSHTTP_METHOD_GET, 
                      CSHTTP_VER_1_0, 
                      "/");

  pSession = CFS_OpenSession(NULL, NULL, "localhost", "80");

  if (pSession != NULL) {
    CSHTTP_SendRequest(pWeb, pSession);

    printf("\n\n%s\n", CSHTTP_GetDataRef(pWeb));
  }
    
  CSHTTP_Destructor(&pWeb);
  CFS_CloseSession(&pSession);

  return 0;
}

