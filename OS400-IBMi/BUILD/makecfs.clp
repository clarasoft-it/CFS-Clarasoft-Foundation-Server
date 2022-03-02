             PGM        PARM(&SRCLIB &OUTLIB &DATALIB)

             DCL VAR(&SRCLIB)  TYPE(*CHAR) LEN(10)
             DCL VAR(&OUTLIB)  TYPE(*CHAR) LEN(10)
             DCL VAR(&DATALIB) TYPE(*CHAR) LEN(10)


             CRTCMOD    MODULE(&OUTLIB/CSLIB) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CSLIB) +
                          DBGVIEW(*SOURCE)

             CRTCMOD    MODULE(&OUTLIB/CSJSON) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CSJSON) +
                          DBGVIEW(*SOURCE)

             CRTSQLCI OBJ(ZFSOUCIE/CFSREPO) +
                      SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CFSREPO) +
                      CLOSQLCSR(*ENDMOD) OUTPUT(*PRINT) +
                      COMPILEOPT('SYSIFCOPT(*IFSIO)') +
                      DBGVIEW(*SOURCE)

             CRTCMOD    MODULE(&OUTLIB/CFSAPI) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CFSAPI) +
                          DBGVIEW(*SOURCE)

             CRTCMOD    MODULE(&OUTLIB/CSHTTP) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CSHTTP) +
                          DBGVIEW(*SOURCE)

             CRTCMOD    MODULE(&OUTLIB/CSWSCK) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CSWSCK) +
                          DBGVIEW(*SOURCE)

             CRTCMOD    MODULE(&OUTLIB/CSAP) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CSAP) +
                          DBGVIEW(*SOURCE)

             CRTCMOD    MODULE(&OUTLIB/CSAPBRKR) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) +
                          DBGVIEW(*SOURCE) SYSIFCOPT(*IFSIO)

             CRTCMOD    MODULE(&OUTLIB/CLARAD) +
                          SRCFILE(&SRCLIB/QCSRC_CFS) SRCMBR(CLARAD) +
                          DBGVIEW(*SOURCE)

             CRTSRVPGM  SRVPGM(&OUTLIB/CSLIB) +
                          MODULE(CSLIB CSJSON) +
                          EXPORT(*SRCFILE) SRCFILE(*LIBL/QSRVSRC)

             CRTSRVPGM  SRVPGM(&OUTLIB/CFSAPI) +
                          MODULE(CFSAPI CSHTTP CSWSCK CFSREPO CSAP) +
                          EXPORT(*SRCFILE) SRCFILE(*LIBL/QSRVSRC) +
                          BNDSRVPGM((CSLIB))

             CRTPGM     PGM(&OUTLIB/CLARAD) MODULE(&SRCLIB/CLARAD) +
                          BNDSRVPGM((CFSAPI)(CSLIB))

             CRTPGM     PGM(&OUTLIB/CSAPBRKR) MODULE(&SRCLIB/CSAPBRKR) +
                          BNDSRVPGM((CFSAPI) (CSLIB))

             CHGCURLIB CURLIB(&DATALIB)
             RUNSQLSTM SRCFILE(&SRCLIB/QSQLSRC) SRCMBR(CFSREPO)
END:
             ENDPGM

