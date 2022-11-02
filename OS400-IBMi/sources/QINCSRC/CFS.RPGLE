
      //------------------------------------------------------------------------------------------
      //
      // Clarasoft Foundation Server - OS/400 IBM i
      // ILE RPG Header file for Clarasoft Foundation Server - OS/400 IBM i
      //
      //------------------------------------------------------------------------------------------

      /IF NOT DEFINED(CFS)
      /DEFINE CFS

       Dcl-C CFS_NTOP_ADDR_MAX            Const(1025);
       Dcl-C CFS_NTOP_PORT_MAX            Const(9);

       // Operation codes

       Dcl-C CFS_OPER_WAIT                Const(x'00010000');
       Dcl-C CFS_OPER_READ                Const(x'01010000');
       Dcl-C CFS_OPER_WRITE               Const(x'01020000');
       Dcl-C CFS_OPER_CONFIG              Const(x'01030000');

       // Diagnostic codes

       Dcl-C CFS_DIAG_CONNCLOSE           Const(x'0000F001');
       Dcl-C CFS_DIAG_WOULDBLOCK          Const(x'0000F002');
       Dcl-C CFS_DIAG_READNOBLOCK         Const(x'0000F003');
       Dcl-C CFS_DIAG_WRITENOBLOCK        Const(x'0000F004');
       Dcl-C CFS_DIAG_TIMEDOUT            Const(x'0000F005');
       Dcl-C CFS_DIAG_ALLDATA             Const(x'0000F006');
       Dcl-C CFS_DIAG_PARTIALDATA         Const(x'0000F007');
       Dcl-C CFS_DIAG_NODATA              Const(x'0000F008');
       Dcl-C CFS_DIAG_INVALIDSIZE         Const(x'0000F009');
       Dcl-C CFS_DIAG_ENVOPEN             Const(x'0000F00A');
       Dcl-C CFS_DIAG_APPID               Const(x'0000F00B');
       Dcl-C CFS_DIAG_SESSIONTYPE         Const(x'0000F00C');
       Dcl-C CFS_DIAG_ENVINIT             Const(x'0000F00D');
       Dcl-C CFS_DIAG_SOCOPEN             Const(x'0000F00E');
       Dcl-C CFS_DIAG_SETFD               Const(x'0000F00F');
       Dcl-C CFS_DIAG_SOCINIT             Const(x'0000F010');
       Dcl-C CFS_DIAG_NOTFOUND            Const(x'0000F011');
       Dcl-C CFS_DIAG_SESSIONINIT         Const(x'0000F012');
       Dcl-C CFS_DIAG_SEQNUM_EXHAUSTED    Const(x'0000F013');
       Dcl-C CFS_DIAG_LIBNOTFOUND         Const(x'0000F014');
       Dcl-C CFS_DIAG_SRVCNOTFOUND        Const(x'0000F015');
       Dcl-C CFS_DIAG_PROCNOTFOUND        Const(x'0000F016');
       Dcl-C CFS_DIAG_SECMODE             Const(x'00000017');
       Dcl-C CFS_DIAG_SYSTEM              Const(x'0000FFFE');
       Dcl-C CFS_DIAG_UNKNOWN             Const(x'0000FFFF');

       /////////////////////////////////////////////////////////////////
       // CFS
       /////////////////////////////////////////////////////////////////

       Dcl-Pr CFS_CloseChannel Int(10)
                               ExtProc('CFS_CloseChannel');
         Session Pointer Value;
       End-Pr;

       Dcl-Pr CFS_CloseEnv  Int(10)
                            ExtProc('CFS_CloseEnv');
         Session Pointer;
       End-Pr;

       Dcl-Pr CFS_CloseSession Int(10)
                               ExtProc('CFS_CloseSession');
         Session Pointer Value;
       End-Pr;

       Dcl-Pr CFS_OpenEnv Pointer
                          ExtProc('CFS_OpenEnv');
         szConfig Pointer Value;
       End-Pr;

       Dcl-Pr CFS_OpenChannel Pointer
                              ExtProc('CFS_OpenChannel');
         pEnv         Pointer Value;
         szSessConfig Pointer Value;
         szHost       Pointer Value;
         szPort       Pointer Value;
       End-Pr;

       Dcl-Pr CFS_OpenSession Pointer
                ExtProc('CFS_OpenSession');
         pEnv         Pointer Value;
         szSessConfig Pointer Value;
         szHost       Pointer Value;
         szPort       Pointer Value;
       End-Pr;

       Dcl-Pr CFS_Receive Int(10)
                          ExtProc('CFS_Receive');
         pSession Pointer Value;
         Buffer Pointer Value;
         Size Pointer Value;
       End-Pr;

       Dcl-Pr CFS_ReceiveRecord Int(10)
                          ExtProc('CFS_ReceiveRecord');
         pSession Pointer Value;
         Buffer Pointer Value;
         Size Pointer Value;
       End-Pr;

       Dcl-Pr CFS_Send Int(10)
                          ExtProc('CFS_Send');
         pSession Pointer Value;
         Buffer Pointer Value;
         Size Pointer Value;
       End-Pr;

       Dcl-Pr CFS_SendRecord Int(10)
                          ExtProc('CFS_SendRecord');
         pSession Pointer Value;
         Buffer Pointer Value;
         Size Pointer Value;
       End-Pr;

      /ENDIF

