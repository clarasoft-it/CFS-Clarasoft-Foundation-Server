      //------------------------------------------------------------------------------------------
      // Clarasoft Foundation Server - OS/400 IBM i
      // T_ECHO.SQLRPGLE
      // Example client program connecting to basic echo service
      //
      // Build with:
      //
      //   CRTSQLRPGI OBJ(T_ECHO) OBJTYPE(*MODULE) DBGVIEW(*SOURCE)
      //   CRTPGM PGM(T_ECHO) BNDSRVPGM((CFSAPI))
      //
      // Example call:
      //
      //  The BASIC-ECHO service must be up and running by issuing
      //
      //    CALL CLARAD PARM('DAEMONS/EXAMPLES/BASIC-ECHO')
      //
      //  Then, this example client program can be called by:
      //
      //    CALL T_ECHO PARM('Hello World!')
      //
      //------------------------------------------------------------------------------------------

        Ctl-opt Main(Main);

      /Include QINCSRC,CFS

       Dcl-Pr Main ExtPgm('T_ECHO');
         @Message Char(32);
       End-Pr;

       Dcl-Proc Main;
       Dcl-Pi *N;
         @Message Char(32);
       End-Pi;

        Dcl-S pSession Pointer;
        Dcl-S Host Char(33);
        Dcl-S Port Char(6);
        Dcl-S Response Char(40);
        Dcl-S Size Int(10);

        // Null-terminate parameters
        Host = 'localhost' + x'00';
        Port = '11009' + x'00';

        // Connect to service
        pSession = CFS_OpenSession(*Null: *Null:
                                   %Addr(Host): %Addr(Port));

        If pSession <> *Null;

          // Send the message (we know how much data to send)
          Size = %Len(%Trim(@Message));
          CFS_SendRecord(pSession: %Addr(@Message): %Addr(Size));

          // Receive service response (we don't know how much we will receive)
          // We indicate up to how much data we can receive
          Size = 40;
          CFS_Receive(pSession: %Addr(Response): %Addr(Size));
          Dsply Response;
          CFS_CloseSession(pSession);

        EndIf;

        *InLr = *On;
        Return;

       End-Proc;
