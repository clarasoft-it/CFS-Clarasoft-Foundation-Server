
#include<stdio.h>
#include<string.h>


#include<clarasoft/cslib.h>
#include<clarasoft/cfsapi.h>

#include<clarasoft/csap.h>


int main(int argc, char** argv) {

    CSAP pCSAP;
    CSAPSTAT csapstat;

    pCSAP = CSAP_Constructor();



    if (CS_FAIL(CSAP_OpenService(pCSAP, argv[1], &csapstat))) {

      CSAP_Destructor(&pCSAP);

    }


    CSAP_Destructor(&pCSAP);


    return 0;
}