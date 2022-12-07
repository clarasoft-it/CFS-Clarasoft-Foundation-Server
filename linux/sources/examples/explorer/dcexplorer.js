
/* ============================================================================================================
  Global variables
============================================================================================================ */

var hSession = CSAP();
var currentPanel = null;
var execMode = "CMD";
var curApplication = null;
var menuSet = null
var breadcrumbs = [];
var lpVtbl = null;

/* ============================================================================================================
  CSAP
============================================================================================================ */

function onOpen() {

}

function onOpenCB() {

}

function onCloseCB() {
 
}

function onOpenError() {
  alert("Open Error: check connection parameters");
}

function onHandshakeError() {
  alert("Handshake Error");
}

function onCtlFrameError() {
  alert("Receive CTL Error");
}

function onUsrCtlFrameError() {
  alert("Receive USRCTL Error");
}

function onDataError() {
  alert("Receive Error");
}

function onCloseError() {
  alert("Close Error");
}

function onHandsahkeCB() {

  JsonIn = { ctl: { op: "FRAMEWORK-INIT", config: "CSAPAPP/FRAMEWORK/DC-EXPLORER", langID: "FR-CAN"}};
  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");

  currentPanel = document.getElementById("project-list");
}

function onDataCB(state) {

  switch (state) {

    case "CSAP_CTL":

      break;

    case "CSAP_USRCTL":

      break;

    case "CSAP_DATA":

      inData = JSON.parse(hSession.ReceiveData());

      switch(inData.ctl.op) {

        case "FRAMEWORK-INIT":

          FRMPROC_Init(inData);
          break;

        case "CMD":

          framework.methods[inData.ctl.method](inData);
          break;

        case "EXEC":

          FRMPROC_RunApplication(inData.data);
          break;

        case "CALL":

          framework.applications[curApplication].lpVtbl[inData.ctl.method](inData)
          break;
      }

      break;
  }
}

/* ------------------------------------------------------------------------
  Set CSAP callbacks
------------------------------------------------------------------------ */

hSession.SetOpenCallback(onOpenCB, onOpenError);
hSession.SetHandshakeCallback(onHandsahkeCB, onHandshakeError);
hSession.SetDataCallback(onDataCB, onDataError);
hSession.SetCloseCallback(onCloseCB, onCloseError);

/* ============================================================================================================
  FRAMEWORK
============================================================================================================ */

function
  FRMPROC_OpenService
    () {

  var host = "localhost";
  var service = "CSAP/HANDLERS/CSAPAPP";
  var u = "";
  var p = "";

  var port = Number.parseInt("11012", 10);

  if (Number.isNaN(port)) {
    alert("Invalid port number");
    return;
  }

  hSession.Open(host,
                port,
                service, "", u, p);
}

function
  FRMPROC_CloseService
    () {

  hSession.Close();
}

function EXEC(config, langID) {

  curApplication = config;
  var JsonIn = { ctl: { op: "EXEC", config: null, langID: null}};
  JsonIn.ctl.config = config;
  JsonIn.ctl.langID = langID;
  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function CALL(method) {

  //JsonIn.ctl.method = method;
  JsonIn = { ctl: { "op": "CALL", method: "LIST-TASK" }, data: { realmID: "PRJ", prjID: "865"}};

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function CMD(method, data) {

  JsonIn = { ctl: { "op": "CMD", method: null }};
  JsonIn.ctl.method = method;

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function FRMPROC_DisplayPanel(panelID) {

  document.getElementById("home").style.display = "none";
  document.getElementById("menu").style.display = "none";
  document.getElementById("split").style.display = "none";
  document.getElementById("field").style.display = "none";

  document.getElementById("rink").style.display = "block";
  document.getElementById(panelID).style.display = "block";
}

function FRMPROC_Noop() {
  // no operation ... does nothing by definition
  return;
}

function FRMPROC_BreadCrumbOptionSelect(e) {

  //alert(JSON.stringify(breadcrumbs[parseInt(e.target.getAttribute("data-option-index"))]));

  // Send a No-OP to framewqork; if an application is running, this will close it
  CMD("NOOP", null);

  var bc = document.getElementById("breadcrumbs");
  bc.innerHTML = "";

  var bclength = parseInt(e.target.getAttribute("data-option-index"));
  breadcrumbs.length = bclength;

  if (bclength === 0) {
    FRMPROC_DisplayMenuSel(e.target.getAttribute("data-option"));
    return;
  }

  for (var i=0; i<bclength; i++) {

    var li = document.createElement("li");

    var anchor = document.createElement("a");
    anchor.href = "#";
    anchor.setAttribute("data-option", breadcrumbs[i].oid);
    anchor.setAttribute("data-option-caption", breadcrumbs[i].caption);
    anchor.setAttribute("data-option-index", i);
  
    anchor.innerHTML = breadcrumbs[i].caption;
    anchor.onclick = FRMPROC_BreadCrumbOptionSelect;
    li.appendChild(anchor);
    bc.appendChild(li);
  }

  FRMPROC_DisplayMenuSel(e.target.getAttribute("data-option"));
}

function FRMPROC_DisplayMenuSel(oid) {

  FRMPROC_DisplayPanel("menu");

  var panel = document.getElementById("tiles");
  panel.innerHTML = "";

  var li = document.createElement("li");

  var anchor = document.createElement("a");
  anchor.href = "#";
  anchor.setAttribute("data-option", oid);
  anchor.setAttribute("data-option-caption", menuSet[oid].caption);
  anchor.setAttribute("data-option-index", breadcrumbs.length + "");
  var bcData = {oid:null, caption:null, index:null};

  bcData.index = breadcrumbs.length;
  bcData.oid = oid;
  bcData.caption = menuSet[oid].caption;

  breadcrumbs.push(bcData);

  anchor.innerHTML = menuSet[oid].caption;
  anchor.onclick = FRMPROC_BreadCrumbOptionSelect;

  li.appendChild(anchor);
  var bc = document.getElementById("breadcrumbs");
  bc.appendChild(li);

  for (var i=0; i<menuSet[oid].options.length; i++) {
    var div = document.createElement("div");
    div.setAttribute("data-option", menuSet[oid].options[i].oid);
    div.setAttribute("data-option-type", menuSet[oid].options[i].type);

    if (menuSet[oid].options[i].type === "*APPLI") {
      div.setAttribute("data-option-config", menuSet[oid].options[i].config);
    }

    div.setAttribute("data-option-caption", menuSet[oid].options[i].caption);
    div.style.cursor = "pointer";
    div.innerHTML = menuSet[oid].options[i].caption;
    div.onclick = FRMPROC_OptionSelect;
    panel.appendChild(div);
  }
}

function FRMPROC_OptionSelect(e) {

  if (e.target.getAttribute("data-option-type") === "*MENU") {
    FRMPROC_DisplayMenuSel(e.target.getAttribute("data-option"));
  }
  else {
    // EXEC application
    EXEC(e.target.getAttribute("data-option-config"), 'FR-CAN');
  }
}

function FRMPROC_Init(data) {

  var bc = document.getElementById("breadcrumbs");
  bc.innerHTML = "";
  breadcrumbs.length = 0;
  menuSet = data.data.menu;

  framework.applications = {};

  for (var i=0; i<data.data.applications.length; i++) {
    framework.applications[data.data.applications[i]] = {};
    framework.applications[data.data.applications[i]].lpVtbl = null;
  }
  FRMPROC_DisplayMenuSel("0");
}

function FRMPROC_Login() {
  FRMPROC_OpenService();
  document.getElementById("home").style.display = "none";
  document.getElementById("split").style.display = "none";
  document.getElementById("field").style.display = "none";
  document.getElementById("rink").style.display = "block";
  document.getElementById("menu").style.display = "block";
}

function FRMPROC_Logout() {
  FRMPROC_CloseService();
  document.getElementById("split").style.display = "none";
  document.getElementById("field").style.display = "none";
  document.getElementById("rink").style.display = "none";
  document.getElementById("tiles").innerHTML = "";
  document.getElementById("menu").style.display = "none";
  document.getElementById("home").style.display = "block";
}

function FRMPROC_RunApplication(data) {

  FRMPROC_DisplayPanel(data.webFacing.panelID);

  var sc = document.createElement("script"); 

  sc.addEventListener('load', function() {
    // The script is loaded completely
    // Do something
    //framework.applications[curApplication].lpVtbl = SDLC_Vtbl;
  });  

  sc.setAttribute("src",  data.webFacing.script);
  sc.setAttribute("type", "text/javascript");
  sc.setAttribute("async", false);
  document.head.appendChild(sc);

  if (data.webFacing.panelID === "split") {
    fetch(data.webFacing.html[1])
   .then(response=> response.text())
   .then(text=> document.getElementById("topic").innerHTML = text);    

   genTree("treeviewPanel", data.sdlcTree);
  }
  else {

  }
}

var framework = {
  methods: {
    "INIT": FRMPROC_Init,
    "LOGIN": FRMPROC_Login,
    "LOGOUT": FRMPROC_Logout,
    "NOOP": FRMPROC_Noop
  },
  applications: null
}

var SDLC_Treeview_vtbl = null;

/* ============================================================================================================
  Splitter
============================================================================================================ */

function moveStart(e) {
    document.getElementById("splitter").setPointerCapture(e.pointerId);
    document.getElementById("splitter").onpointermove = move;
  }
  
  function move(e) {
    document.getElementById("navtree").style.width = e.clientX+"px";
    document.getElementById("splitter").style.left = e.clientX+"px";
    document.getElementById("topic").style.left = e.clientX+10+"px";
  }
      
  function moveDone(e) {
   document.getElementById("splitter").onpointermove = null;
   document.getElementById("splitter").releasePointerCapture(e.pointerId);
   document.getElementById("splitter").style.left = e.clientX+"px";
   document.getElementById("navtree").style.width = e.clientX+"px";
   document.getElementById("topic").style.left = e.clientX+10+"px";
  }
  
  function setMouseCursor() {
  
     
  }
  
  /* ============================================================================================================
      Tree-view
    ============================================================================================================ */
 

  function selectTreeItem(source) {
  
    if (curTreeSel !== null) { 
      document.getElementById(curTreeSel).classList.remove('dir-node-a-selected');
      document.getElementById(curTreeSel).classList.add('dir-node-a');
    }
  
    curTreeSel = source.target.getAttribute("id");
    source.target.classList.remove('dir-node-a');
    source.target.classList.add('dir-node-a-selected');

    treeviewCurSel.class = source.target.getAttribute("data-class");
    treeviewCurSel.keys = JSON.parse(source.target.getAttribute("data-key"));
    treeviewCurSel.label = source.target.getAttribute("data-label");

    SDLC_Treeview_vtbl[source.target.getAttribute("data-class")](JSON.parse(source.target.getAttribute("data-key")));

    //alert(source.target.getAttribute("data-key") + " - " + source.target.getAttribute("data-class"));
  }
  
  function toggleTree(source) {
    if (source.target.getAttribute("data-state") === "closed") {
      source.target.setAttribute("data-state", "opened");
      source.target.src = "./icon-minus.png";
      document.getElementById("panel-" + source.target.getAttribute("data-key")).style.display = "block";
      document.getElementById("item-icon-" + source.target.getAttribute("data-key")).src = "./icon-project-opened.png";
    }
    else {
      source.target.setAttribute("data-state", "closed");
      source.target.src = "./icon-add.png";
      document.getElementById("panel-" + source.target.getAttribute("data-key")).style.display = "none";
      document.getElementById("item-icon-" + source.target.getAttribute("data-key")).src = "./icon-project.png";
    }
  }
  
  function clearTree(panelID) {
      document.getElementById(panelID).innerHTML = "";
  }
  
  function treeviewSelectItem(keys) {
   
  }
  
  function treeviewInsertItem(keys) {
   
  }
  
  function treeviewDeleteItem(keys) {
   
  }
  
  function treeviewUpdateItem(keys) {
   
  }

  function genTree(panelID, treeData) {
  
  /*
    var treeDisplay = [
      {icon_closed:"./icon-project.png", icon_opened: "./icon-project-opened.png"}
  
    ];
    var treeData = 
    [
      {label:"Projets", 
       key:"P", 
       items:[
         {label:"724 - SOX Remediation", key: "724", items:[]},
         {label:"865 - Digital Accelerated Technology", key: "865", items:[
           {label:"0007F - NXP:Calcul de prix", key: "0007F", items:[]},
           {label:"0008F - NXP - CN", key: "0008F", items:[]},
           {label:"0009F - NXP - ASN unique", key: "0009F", items:[]}]},
       ]
      },
      {label:"Service Now - Help Desk",
       key:"SNOW",
       items: [
        {label:"INC - Incidents", key:"INC", items:[
          {label:"0002F", key: "0007F", items:[]},
          {label:"0003F", key: "0008F", items:[]}
        ]},
        {label:"PRB - Problemes", key:"PRB", items:[
          {label:"0001F", key: "0007F", items:[]},
          {label:"0002F", key: "0008F", items:[]}
        ]}
       ]
      },
      {label:"LIste des MEPs",
       key:"MEP",
       items: [
        {label:"INC - Incidents", key:"INC", items:[
          {label:"0002F", key: "0007F", items:[]},
          {label:"0003F", key: "0008F", items:[]}
        ]},
        {label:"PRB - Problemes", key:"PRB", items:[
          {label:"0001F", key: "0007F", items:[]},
          {label:"0002F", key: "0008F", items:[]}
        ]}
       ]
      }   
    ];
  */
    //document.getElementById(panelID).innerHTML = "";
    parent = document.getElementById(panelID);
    parent.innerHTML = "";

    for (var i=0; i<treeData.length; i++) {
  
      d = document.createElement("div");
      d.setAttribute("class", "dir-node");
      d.setAttribute("data-key", treeData[i].key);
      parent.appendChild(d);
  
      if (treeData[i].items.length > 0) {
  
        img = document.createElement("img");
        img.src = "./icon-minus.png";
        img.setAttribute("width","20px");
        img.setAttribute("height","20px");
        img.setAttribute("data-key", treeData[i].key);
        img.setAttribute("data-state", "opened");
        img.onclick = toggleTree;
        d.appendChild(img);
  
        imgCat = document.createElement("img");
        imgCat.src = "./icon-project.png";
        imgCat.setAttribute("width","20px");
        imgCat.setAttribute("height","20px");
        imgCat.setAttribute("id", "item-icon-" + treeData[i].key);
        d.appendChild(imgCat);
  
        anchor = document.createElement("a");
        anchor.href = "#";
        anchor.innerHTML = treeData[i].label;
        anchor.setAttribute("class", "dir-node-a");
        anchor.setAttribute("id", "item-label-" + treeData[i].key);

        var keys = [];
        keys[0] = treeData[i].key;
        anchor.setAttribute("data-key", JSON.stringify(keys));
        anchor.setAttribute("data-class", treeData[i].class);
        anchor.setAttribute("data-label", treeData[i].label);
        anchor.onclick = selectTreeItem;
        d.appendChild(anchor);
  
        dd = document.createElement("div");
        dd.setAttribute("id", "panel-" + treeData[i].key);
        d.appendChild(dd);
  
        mkDirNode(dd, treeData[i].items, keys);
      }
      else {
        img = document.createElement("img");
        img.src = "./icon-blank.png";
        img.setAttribute("width","20px");
        img.setAttribute("height","20px");
        img.setAttribute("data-key", treeData[i].key);
        d.appendChild(img);
  
        imgCat = document.createElement("img");
        imgCat.src = "./icon-project.png";
        imgCat.setAttribute("width","20px");
        imgCat.setAttribute("height","20px");
        imgCat.setAttribute("id", "item-icon-" + treeData[i].key);
        d.appendChild(imgCat);
  
        anchor = document.createElement("a");
        anchor.href = "#";
        anchor.innerHTML = treeData[i].label;
        anchor.setAttribute("class", "dir-node-a");
        var keys = [];
        keys[0] = treeData[i].key;
        anchor.setAttribute("data-key", JSON.stringify(keys));
        anchor.setAttribute("data-class", treeData[i].class);
        anchor.setAttribute("data-label", treeData[i].label);
        anchor.onclick = selectTreeItem;
        d.appendChild(anchor);
      } 
    }
  
    return;
  }
  
  function mkDirNode(parent, data, keys) {
  
    for (var i=0; i<data.length; i++) {
  
      d = document.createElement("div");
      d.setAttribute("class", "dir-node");
      d.setAttribute("data-key", data[i].key);
      parent.appendChild(d);

      var itemKeys = JSON.stringify(keys);
      var k = JSON.parse(itemKeys)
      k[k.length] = data[i].key;

      if (data[i].items.length > 0) {
   
        img = document.createElement("img");
        img.src = "./icon-minus.png";
        img.setAttribute("width","20px");
        img.setAttribute("height","20px");

        img.setAttribute("data-key", JSON.stringify(k) + "-" + data[i].key);
        img.setAttribute("data-state", "opened");
        img.onclick = toggleTree;
        d.appendChild(img);
  
        imgCat = document.createElement("img");
        imgCat.src = "./icon-project.png";
        imgCat.setAttribute("width","20px");
        imgCat.setAttribute("height","20px");
        imgCat.setAttribute("id", "item-icon-" + JSON.stringify(k) + "-" + data[i].key);
        d.appendChild(imgCat);
  
        anchor = document.createElement("a");
        anchor.href = "#";
        anchor.innerHTML = data[i].label;
        anchor.setAttribute("class", "dir-node-a");
        anchor.setAttribute("id", "item-label-" + JSON.stringify(k) + "-" + data[i].key);
        anchor.setAttribute("data-key", JSON.stringify(k));
        anchor.setAttribute("data-class", data[i].class);
        anchor.setAttribute("data-label", data[i].label);
        anchor.onclick = selectTreeItem;
        d.appendChild(anchor);
  
        dd = document.createElement("div");
        dd.setAttribute("id", "panel-" + JSON.stringify(k) + "-" + data[i].key);
        d.appendChild(dd);
  
        mkDirNode(dd, data[i].items, k);
      }
      else {
        img = document.createElement("img");
        img.src = "./icon-blank.png";
        img.setAttribute("width","20px");
        img.setAttribute("height","20px");
        img.setAttribute("data-key", JSON.stringify(k) + "-" + data[i].key);
        d.appendChild(img);
  
        imgCat = document.createElement("img");
        imgCat.src = "./icon-project.png";
        imgCat.setAttribute("width","20px");
        imgCat.setAttribute("height","20px");
        imgCat.setAttribute("id", "item-icon-" + JSON.stringify(k) + "-" + data[i].key);
        d.appendChild(imgCat);
  
        anchor = document.createElement("a");
        anchor.href = "#";
        anchor.innerHTML = data[i].label;
        anchor.setAttribute("class", "dir-node-a");
        anchor.setAttribute("id", "item-label-" + JSON.stringify(k) + "-" + data[i].key);
        anchor.setAttribute("data-key", JSON.stringify(k));
        anchor.setAttribute("data-class", data[i].class);
        anchor.setAttribute("data-label", data[i].label);
        anchor.onclick = selectTreeItem;
        d.appendChild(anchor);
      }
    }
  
    return 
  }
  

/* ============================================================================================================
  Data loading functions
============================================================================================================ */

function fillTable(gridID, result, ctl) {

    rowSelect = function(event) {
      index = event.currentTarget.getAttribute("data-row-index");
  
      alert(result.data[index][0] + ":" + result.keys[index][0]);
    }
  
    table = document.createElement("table");
    table.setAttribute("class", "clara-table");
    
    table.setAttribute("id", "resultSet");
    hdr = table.createTHead();
    row = hdr.insertRow();
    for (var i=0; i<result.headings.length + 1; i++) {
      if (i == result.headings.length) {
  
        if (ctl[0] === true) {
          var th = document.createElement('th');
          th.innerHTML = "";
          row.appendChild(th);
        }
  
        if (ctl[1] === true) {
          var th = document.createElement('th');
          th.innerHTML = "";
          row.appendChild(th);
        }
  
        if (ctl[2] === true) {
  
          var th = document.createElement('th');
          th.innerHTML = "";
          row.appendChild(th);
        }
      }
      else {
        var th = document.createElement('th');
        th.innerHTML = result.headings[i];
        row.appendChild(th);
      }
    }
  
    body = table.createTBody(); 
    for (var i=0; i<result.data.length; i++) {
      row = body.insertRow(-1);
      row.setAttribute("data-row-index", i);
      for (j=0; j<result.data[i].length+1; j++) {
  
        if (j == result.data[i].length) {

          cell = row.insertCell(j);

          if (ctl[0] === true) {
            img = document.createElement("img");
            img.src = "./icon-add.png";
            img.setAttribute("width","18px");
            img.setAttribute("height","18px");
            anchor = document.createElement("a");
            anchor.href = "#";
            anchor.onclick = document_add;
            anchor.appendChild(img);
            cell.appendChild(anchor);
          }

          if (ctl[1] === true) {
            img = document.createElement("img");
            img.src = "./icon-edit.png";
            img.setAttribute("width","18px");
            img.setAttribute("height","18px");
            anchor = document.createElement("a");
            anchor.href = "#";
            anchor.onclick = document_edit;
            if (result.handlers.update !== null) {
              anchor.onclick = result.handlers.update;
            }
            anchor.appendChild(img);
            cell.appendChild(anchor);
          }

          if (ctl[2] === true) {
            img = document.createElement("img");
            img.src = "./icon-delete.png";
            img.setAttribute("width","18px");
            img.setAttribute("height","18px");
            anchor = document.createElement("a");
            anchor.href = "#";
            anchor.onclick = document_delete;
            anchor.appendChild(img);
            cell.appendChild(anchor);
          }

          cell = row.insertCell(j+1);
        }
        else {
          cell = row.insertCell(j);
          cell.innerHTML = result.data[i][j];
        }
      }
    }
  
    document.getElementById(gridID).appendChild(table);
  }

  
/* ============================================================================================================
  Event handlers
============================================================================================================ */

function document_add(e) {
  alert("Document add");
  e.stopPropagation()
}

function document_edit(e) {
  alert("Document edit");
  e.stopPropagation()
}

function document_delete(e) {
  alert("Document delete");
  e.stopPropagation()
}

