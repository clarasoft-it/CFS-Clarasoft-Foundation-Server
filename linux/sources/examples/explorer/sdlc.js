


var SDLC_Vtbl =  {
       //"INSERT-PRJ": SDLCPROC_Init,
       //"INSERT-PRJ": SDLCPROC_CreateProject,
       //"INSERT-TASK": SDLCPROC_CreateTask,
  "LIST-PRJ": SDLCPROC_ListProjects,
  "LIST-TASK": SDLCPROC_ListTasks,
  "DISPLAY-TASK": SDLCPROC_DisplayTask,
  "INIT": SDLCPROC_Init
};

var SDLC_CurrentPanel;

function SDLCPROC_Init(data) {
  alert(JSON.stringify(data));
}
  
function SDLCPROC_ListProjects(data) {

  //alert(JSON.stringify(data));

  var tableData = {headings:[], keys: null, data: null, handlers: {create:null, update:null, delete:null}};
  tableData.headings.push("Project");
  tableData.headings.push("Title");
  tableData.data = data.data.data;
  tableData.keys = data.data.keys;
  panel = document.getElementById("project-list");
  document.getElementById("project-list-table").innerHTML = "";
  fillTable("project-list-table", tableData, [false, true, true]);
  document.getElementById("section-caption").innerHTML = treeviewCurSel.label;
  document.getElementById("section-caption-new").innerHTML = "Nouveau projet";
  SDLC_CurrentPanel.style.display = "none";
  panel.style.display = "block";
  SDLC_CurrentPanel = panel;

}
  
function SDLCPROC_ListTasks(data) {

  //alert(JSON.stringify(data));

  var tableData = {headings:[], keys: null, data: null, handlers: {create:null, update:null, delete:null}};
  tableData.headings.push("Task");
  tableData.headings.push("Title");
  tableData.headings.push("Description");
  tableData.data = data.data.data;
  tableData.keys = data.data.keys;
  panel = document.getElementById("task-list");
  document.getElementById("task-list-table").innerHTML = "";
  fillTable("task-list-table", tableData, [false, true, true]);
  document.getElementById("task-list-hdr").innerHTML = treeviewCurSel.label;
  SDLC_CurrentPanel.style.display = "none";
  panel.style.display = "block";
  SDLC_CurrentPanel = panel;

}
  
function CreateProject () {

  var JsonIn = { ctl: { "op": "CALL", method: "INSERT-PRJ" }, data: { realmID: null, prjID: null, prjDesc: null } };

  JsonIn.data.realmID = document.getElementById("realmID").value;
  JsonIn.data.prjID = document.getElementById("prjID").value;
  JsonIn.data.prjDesc = document.getElementById("prjDesc").value;

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function CreateTask() {

  var JsonIn = { ctl: { "op": "CALL", method: "INSERT-TASK" }, data: { realmID: null, prjID: null, taskID: null, taskTitle: null, taskDesc: null, taskRef: null } };

  JsonIn.data.realmID = document.getElementById("realmID").value;
  JsonIn.data.prjID = "865"; //document.getElementById("prjID").value;
  JsonIn.data.taskID = document.getElementById("taskID").value;
  JsonIn.data.taskTitle = document.getElementById("taskTitle").value;
  JsonIn.data.taskDesc = document.getElementById("taskDesc").value;
  JsonIn.data.taskRef = document.getElementById("taskRef").value;

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");

}

function ListProjects (data) {

  //alert(data);

  var JsonIn = { ctl: { "op": "CALL", method: "LIST-PRJ" }, data: { realmID: null} };

  JsonIn.data.realmID = data[0];

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function ListTasks(data) {

  //alert(data);

  var JsonIn = { ctl: { "op": "CALL", method: "LIST-TASK" }, data: { realmID: null, prjID: null}};

  JsonIn.data.realmID = data[0];
  JsonIn.data.prjID = data[1];

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function DisplayTask(data) {

  //alert(data);

  var JsonIn = { ctl: { "op": "CALL", method: "DISPLAY-TASK" }, data: { realmID: null, prjID: null, taskID: null}};

  JsonIn.data.realmID = data[0];
  JsonIn.data.prjID = data[1];
  JsonIn.data.taskID = data[2];

  hSession.PutData(JSON.stringify(JsonIn));
  hSession.Send("");
}

function SDLCPROC_DisplayTask(data) {

  //alert(data);
  

  var task = {
    project_id: "865",
    project_description: "Digital Accelerated Technology",
    task_id: "0011F",
    task_description: "Correction Pick Notification",
    titre: "Correction Pick Notification",
    description: "Modification du OE_140RM pour les ASN des commandes NXP",
    justification: "Les ASN ne se rendent pas chez GPC parce que les ASN ne sont pas unique pour une commande",

    documents: {
        headings: ["Doucment", "Note"],
        keys: [["k1", "kb"],
        ["k4", "kb"]],
        data: [["Analyse", "b"],
        ["4", "b"]],
        handlers: {
            create: null,
            update: document_edit,
            delete: document_delete
        }
    },
    objects: {
        headings: ["Objet", "Type", "Attribut", "Lib. destination", "Release"],
        keys: [["k1", "kb"],
        ["k4", "kb"]],
        data: [["CFSAPI", "*SRVPGM", "CLE", "LMENTOBJ", "FES_BAS"],
        ["CSLIB", "*SRVPGM", "CLE", "LMENTOBJ", "FES_BAS"]],
        handlers: {
            create: null,
            update: null,
            delete: null
        }
    },
    one_shots: {
        before: {
            name: "M8650011FB",
            env: "' '",
            headings: ["Step", "Desc"],
            steps: {
                keys: [["k1", "kb"],
                ["k4", "kb"]],
                data: [["01", "Arret sous-systemes MULTI-DC, OBOUND_U"],
                ["07", "Arret CGIOBJ"]],
                handlers: {
                    create: null,
                    update: null,
                    delete: null
                }
            }
        },
        after: null
    },
    targets: {
        headings: ["Centre de Distriubution", "Date", "Heure debut", "Heure fin"],
        keys: [["k1", "kb"],
        ["k4", "kb"]],
        data: [["Montreal", "2022-01-22", "8:00", "22:00"],
        ["Moncton", "2022-01-26", "8:00", "22:00"],
        ["Quebec", "2022-01-26", "8:00", "22:00"],
        ["Long Sault", "2022-01-26", "8:00", "22:00"],
        ["Cambridge", "2022-01-26", "8:00", "22:00"],
        ["Winnipeg", "2022-01-26", "8:00", "22:00"],
        ["Calgary", "2022-01-26", "8:00", "22:00"],
        ["Edmonton", "2022-01-26", "8:00", "22:00"],
        ["Vancouver", "2022-01-26", "8:00", "22:00"]],
        handlers: {
            create: null,
            update: null,
            delete: null
        }
    },
    deployment: {
        pre: {

        },
        main: {

        },
        post: {

        }
    }
};

// Fill general information

document.getElementById("SDLC-project-id").innerHTML = task.project_id + " - " + task.project_description;
document.getElementById("SDLC-task-id").innerHTML = task.task_id + " - " + task.task_description;

document.getElementById("SDLC-description").innerHTML = task.description;
document.getElementById("SDLC-justification").innerHTML = task.justification;

fillTable("grid-documents", task.documents, [true, true, true]);
fillTable("grid-objects", task.objects, [false, false, false]);
fillTable("grid-cibles", task.targets, [true, true, true]);







  panel = document.getElementById("task-panel");
  SDLC_CurrentPanel.style.display = "none";
  panel.style.display = "block";
  SDLC_CurrentPanel = panel;
}


/* ============================================================================================================
        
  SDLC Task panels
       
============================================================================================================ */

function SDLC_PANEL_General(evt, SDLC_PANEL_Name_ID) {
  // Declare all variables
  var i, tabcontent, tablinks;

  // Get all elements with class="tabcontent" and hide them
  tabcontent = document.getElementsByClassName("tabcontent-SDLC-PANEL");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }

  // Get all elements with class="tablinks" and remove the class "active"
  tablinks = document.getElementsByClassName("tablinks-SDLC-PANEL");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(SDLC_PANEL_Name_ID).style.display = "block";
  if (evt !== null) {
    evt.currentTarget.className += " active";
  }
}

/* ============================================================================================================

    Distribution Centers tabs - pre-deployment

============================================================================================================ */

function SDLC_DC_PreDeploy(evt, SDLC_DC_PreDeploy_ID) {
  // Declare all variables
  var i, tabcontent, tablinks;

  // Get all elements with class="tabcontent" and hide them
  tabcontent = document.getElementsByClassName("tabcontent-PRE-DEPLOY");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }

  // Get all elements with class="tablinks" and remove the class "active"
  tablinks = document.getElementsByClassName("tablinks-PRE-DEPLOY");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(SDLC_DC_PreDeploy_ID).style.display = "block";
  evt.currentTarget.className += " active";
}

function SDLC_DC_Deploy(evt, SDLC_DC_Deploy_ID) {
  // Declare all variables
  var i, tabcontent, tablinks;

  // Get all elements with class="tabcontent" and hide them
  tabcontent = document.getElementsByClassName("tabcontent-DEPLOY");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }

  // Get all elements with class="tablinks" and remove the class "active"
  tablinks = document.getElementsByClassName("tablinks-DEPLOY");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(SDLC_DC_Deploy_ID).style.display = "block";
  evt.currentTarget.className += " active";
}

function SDLC_DC_PostDeploy(evt, SDLC_DC_PostDeploy_ID) {
  // Declare all variables
  var i, tabcontent, tablinks;

  // Get all elements with class="tabcontent" and hide them
  tabcontent = document.getElementsByClassName("tabcontent-POST-DEPLOY");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }

  // Get all elements with class="tablinks" and remove the class "active"
  tablinks = document.getElementsByClassName("tablinks-POST-DEPLOY");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }

  // Show the current tab, and add an "active" class to the button that opened the tab
  document.getElementById(SDLC_DC_PostDeploy_ID).style.display = "block";
  evt.currentTarget.className += " active";
}



/*                

            var task = {
                project_id: "865",
                project_description: "Digital Accelerated Technology",
                task_id: "0011F",
                task_description: "Correction Pick Notification",
                titre: "Correction Pick Notification",
                description: "Modification du OE_140RM pour les ASN des commandes NXP",
                justification: "Les ASN ne se rendent pas chez GPC parce que les ASN ne sont pas unique pour une commande",

                documents: {
                    headings: ["Doucment", "Note"],
                    keys: [["k1", "kb"],
                    ["k4", "kb"]],
                    data: [["Analyse", "b"],
                    ["4", "b"]],
                    handlers: {
                        create: null,
                        update: document_edit,
                        delete: document_delete
                    }
                },
                objects: {
                    headings: ["Objet", "Type", "Attribut", "Lib. destination", "Release"],
                    keys: [["k1", "kb"],
                    ["k4", "kb"]],
                    data: [["CFSAPI", "*SRVPGM", "CLE", "LMENTOBJ", "FES_BAS"],
                    ["CSLIB", "*SRVPGM", "CLE", "LMENTOBJ", "FES_BAS"]],
                    handlers: {
                        create: null,
                        update: null,
                        delete: null
                    }
                },
                one_shots: {
                    before: {
                        name: "M8650011FB",
                        env: "' '",
                        headings: ["Step", "Desc"],
                        steps: {
                            keys: [["k1", "kb"],
                            ["k4", "kb"]],
                            data: [["01", "Arret sous-systemes MULTI-DC, OBOUND_U"],
                            ["07", "Arret CGIOBJ"]],
                            handlers: {
                                create: null,
                                update: null,
                                delete: null
                            }
                        }
                    },
                    after: null
                },
                targets: {
                    headings: ["Centre de Distriubution", "Date", "Heure debut", "Heure fin"],
                    keys: [["k1", "kb"],
                    ["k4", "kb"]],
                    data: [["Montreal", "2022-01-22", "8:00", "22:00"],
                    ["Moncton", "2022-01-26", "8:00", "22:00"],
                    ["Quebec", "2022-01-26", "8:00", "22:00"],
                    ["Long Sault", "2022-01-26", "8:00", "22:00"],
                    ["Cambridge", "2022-01-26", "8:00", "22:00"],
                    ["Winnipeg", "2022-01-26", "8:00", "22:00"],
                    ["Calgary", "2022-01-26", "8:00", "22:00"],
                    ["Edmonton", "2022-01-26", "8:00", "22:00"],
                    ["Vancouver", "2022-01-26", "8:00", "22:00"]],
                    handlers: {
                        create: null,
                        update: null,
                        delete: null
                    }
                },
                deployment: {
                    pre: {

                    },
                    main: {

                    },
                    post: {

                    }
                }
            };

            // Fill general information

            document.getElementById("SDLC-project-id").innerHTML = task.project_id + " - " + task.project_description;
            document.getElementById("SDLC-task-id").innerHTML = task.task_id + " - " + task.task_description;

            document.getElementById("SDLC-description").innerHTML = task.description;
            document.getElementById("SDLC-justification").innerHTML = task.justification;

            fillTable("grid-documents", task.documents, [true, true, true]);
            fillTable("grid-objects", task.objects, [false, false, false]);
            fillTable("grid-cibles", task.targets, [true, true, true]);

            SDLC_PANEL_General(null, 'SDLC-GENERAL-PANEL');
            document.getElementById("tablinks-SDLC-PANEL-default").className += " active";
          SDLC_OpenService();

*/


var curTreeSel = null;
var sdlcData = null;
  
var treeviewCurSel = {keys:null, class:null, label:null};

function SDLC_InitInstance() {
  //alert("InitInstance");
  framework.applications["CSAP/APPLI/SDLC"].lpVtbl = SDLC_Vtbl;
  
  SDLC_Treeview_vtbl = {
    "REALM": ListProjects,
    "PROJECT": ListTasks,
    "TASK": DisplayTask
  };

  SDLC_CurrentPanel = document.getElementById("project-list");
};

const SDLC_instance = new SDLC_InitInstance();

