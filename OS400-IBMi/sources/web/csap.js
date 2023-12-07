// CSAP class

function CSAP() {

    if ("WebSocket" in window) {
      return CSAP_Session();
    }
    else {
      return null;
    }
  }
  
  function CSAP_Session() {
       
      var wsck = null;
      var CSAP_State = "CSAP_CLOSED";
      var Service = "";
      var Handshake = "";
      var InMessage = "";
      var OutMessage = "";
      var CtlFrame;	
      var UsrCtlFrame;
      var MessageData = "";
      var u;
      var p;
      var ReqCtlFrame = "";
      var Callbacks = {
            onHandshakeCB: null,
            onOpenCB: null,
            onDataCB: null,
            onCloseCB: null,
            onHandshakeErrorCB: null,
            onOpenErrorCB: null,
            onCtlFramErrorCB: null,
            onUsrCtlFrameErrorCB: null,
            onDataErrorCB: null,
            onCloseErrorCB: null
        };
      
      function Send(userCtlFrame) {
      
          var usrCtlSize;
          var dataSize;
          var buffer;
  
          InMessage = "";

          var ctlFrame = {ctl:{usrCtlSize:0, dataSize: 0, fmt:"text"}};

          if (userCtlFrame === null || userCtlFrame === undefined) {
            ctlFrame.ctl.usrCtlSize = 0;
          }
          else {
            ctlFrame.ctl.usrCtlSize = userCtlFrame.toString().length;
          }
  
          ctlFrame.ctl.dataSize = OutMessage.length;
 
          try {
              wsck.send(JSON.stringify(ctlFrame));
          }
          catch(e) {
              //alert("CSAP Send failure");
          }
        
          try {
              if (userCtlFrame.length > 0) {
                wsck.send(userCtlFrame);
              }
          }
          catch(e) {
              //alert("CSAP Send failure");
          }
        
          try {
              if (OutMessage.length) {
               wsck.send(OutMessage);
              }
          }
          catch(e) {
              //alert("CSAP Send failure");
          }
  
          OutMessage = "";
      } 
  
      function PutData(data) {
          OutMessage += data;
      }
  
      function GetHandshake() {
          return Handshake;	
      }
    
      function GetReqCtlFrame() {
          return ReqCtlFrame;
      }
  
      function ReceiveCtlFrame() {
          return CtlFrame;
      }
  
      function ReceiveUsrCtlFrame() {
          return UsrCtlFrame;
      }
     
      function ReceiveData() {
          return MessageData;
      } 
     
      function SetOpenCallback(cb,errCB) {
          Callbacks.onOpenCB = cb;	
          Callbacks.onOpenErrorCB = errCB;	
      } 
     
      function SetHandshakeCallback(cb,errCB) {
          Callbacks.onHandshakeCB = cb;	
          Callbacks.onHandshakeErrorCB = errCB;	
      } 
  
      function SetDataCallback(cb, errCB) {
          Callbacks.onDataCB = cb;	
          Callbacks.onDataErrorCB = errCB;	
      } 
     
      function SetCloseCallback(cb, errCB) {
          Callbacks.onCloseCB = cb;	
          Callbacks.onCloseErrorCB = errCB;	
      } 
      
      function Close() {
            
          wsck.close();
          CSAP_State = "CSAP_CLOSED";
      }
             
      onError = function() {
        //alert("WS Error");
      }
             
      onOpen = function() {
          
          try {
      
              Handshake = {op:"open",service:"", u:"", p:""};
        
              Handshake.service = Service;
              Handshake.u = u;
              Handshake.p = p;
  
              CSAP_State = "CSAP_HANDSHAKE";
              wsck.send(JSON.stringify(Handshake));
              Callbacks.onOpenCB();
          }
          catch(e) {
              
              CSAP_State = "CSAP_CLOSED";
              Callbacks.onOpenErrorCB();
              wsck.close();
              wsck = null;
              return;
          }		
          
      };
                        
      onData = function(evt) {
                   
          var UsrCtlFrameSize;
          var DataSize;
      
          switch(CSAP_State) {
      
              case "CSAP_HANDSHAKE":
                    
                  try {
                         Handshake = JSON.parse(evt.data);
                      CSAP_State = "CSAP_CTL";
                      Callbacks.onHandshakeCB();
                  }
                  catch(e) {
                      wsck.close();
                      wsck = null;
                      CSAP_State = "CSAP_CLOSED";
                  }
  
                  break;
                     
                case "CSAP_CTL":
                
                  try {
  
                      CtlFrame = evt.data;
                      InMessage = JSON.parse(evt.data);
  
                      UsrCtlFrameSize = InMessage.ctl.usrCtlSize;
                      DataSize = InMessage.ctl.dataSize;

                      Callbacks.onDataCB("CSAP_CTL");
  
                      if (UsrCtlFrameSize > 0) {
                          CSAP_State = "CSAP_USRCTL";
                      }
                      else {
                        if (DataSize > 0) {
                          CSAP_State = "CSAP_DATA";
                        }
                        else {
                          CSAP_State = "CSAP_CTL";
                        }
                      }
                  }
                  catch(e) {
                      wsck.close();
                      wsck = null;
                      CSAP_State = "CSAP_CLOSED";
                  }
  
                     break;
                     
              case "CSAP_USRCTL":
                
                  try {
      
                    UsrCtlFrame = evt.data;
                    Callbacks.onDataCB("CSAP_USRCTL");
                    CSAP_State = "CSAP_DATA";
                  }
                  catch(e) {
                    wsck.close();
                    wsck = null;
                    CSAP_State = "CSAP_CLOSED";
                  }
      
                  break;
  
                case "CSAP_DATA":
                
                  try {
          
                    MessageData = evt.data;
                    Callbacks.onDataCB("CSAP_DATA");
                    CSAP_State = "CSAP_CTL";
                  }
                  catch(e) {
                     wsck.close();
                     wsck = null;
                     CSAP_State = "CSAP_CLOSED";
                  }
          
                  break;
              }
      };
              
      onClose = function() { 
          Callbacks.onCloseCB();
          wsck = null;
            CSAP_State = "CSAP_CLOSED";
      };
  
      function Open(host, port, service, mode, uu, pp) {
  
          u = uu;
          p = pp;
  
          if (CSAP_State !== "CSAP_CLOSED" && wsck !== null) {
              wsck.close();
          }
          else {
          }
  
          var Host = host;
          var Port = port;
  
          //for (var i=0; i<3; i++) {
            try {
  
              wsck = new WebSocket("ws://" + host + ":" + port);
              wsck.onerror=function(event){
                console.log("Error");
              }
              /*
              if (mode === "SSL") {
                  wsck = new WebSocket("wss://" + host + ":" + port);
              }
              else {
                  wsck = new WebSocket("ws://" + host + ":" + port);
              }
              */
  
              wsck.onopen = onOpen;
              wsck.onmessage = onData;
              wsck.onclose = onClose;
                  
              Service = service;
              return;
            }
            catch(e) {
              CSAP_State = "CSAP_CLOSED";
              Callbacks.onOpenErrorCB();
              wsck = null;
              return;
            }
          //}
  
  /*
          try {
              wsck = new WebSocket("wss://" + host + ":" + port);
          }
          catch(e) {
  
              try {
                wsck = new WebSocket("ws://" + host + ":" + port);
  
              }
              catch(e) {
                CSAP_State = "CSAP_CLOSED";
                Callbacks.onOpenErrorCB();
                wsck = null;
                return;
              }
          }
  */
  
      }
                  
      return {
          Open:Open, 
          Close:Close,
          PutData: PutData,
          Send: Send,
          GetHandshake: GetHandshake,
          GetReqCtlFrame: GetReqCtlFrame,
          ReceiveCtlFrame: ReceiveCtlFrame,
          ReceiveUsrCtlFrame: ReceiveUsrCtlFrame,
          ReceiveData: ReceiveData,
          SetHandshakeCallback: SetHandshakeCallback,
          SetOpenCallback: SetOpenCallback,
          SetDataCallback: SetDataCallback,
          SetCloseCallback:SetCloseCallback
    };
  
  };
  
  function CSAP_CreateUUID() {
    return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
      (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
    )
  }
