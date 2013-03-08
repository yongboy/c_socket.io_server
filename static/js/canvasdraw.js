var socket;
var Attributes = {THICKNESS:"thickness", 
                  COLOR:"color"};
var Messages = {MOVE:"MOVE", 
                PATH:"PATH"};

var isPenDown = false;

var maxLineThickness = 30;

var localPen = {};

var localLineColor = "#AAAAAA";
var localLineThickness = 1;
var userCurrentPositions = {};
var userCommands = {};
var userColors = {};
var userThicknesses = {};

var canvas;
var context;
var DrawingCommands = {LINE_TO:       "lineTo",
                       MOVE_TO:       "moveTo",
                       SET_THICKNESS: "setThickness",
                       SET_COLOR:     "setColor"};

var hasTouch = false;

window.onload = init;

function init () {
  initCanvas();
  initMenuFunction();
  registerInputListeners();
  iPhoneToTop();
  
  setStatus("Connecting to Server...");
}

function initCanvas () {
  canvas = document.getElementById("canvas");
  
  // If IE8, do IE-specific canvas initialization (required by excanvas.js)
  if (typeof G_vmlCanvasManager != "undefined") {
    this.canvas = G_vmlCanvasManager.initElement(this.canvas);
  }
  
  canvas.width  = 600;
  canvas.height = 400;
  
  context = canvas.getContext('2d');
  context.lineCap = "round";  
}

function registerInputListeners () {
  canvas.onmousedown = pointerDownListener;
  canvas.onmousemove = pointerMoveListener;
  canvas.onmouseup = pointerUpListener;
  
  canvas.ontouchstart = touchDownListener;
  canvas.ontouchmove = touchMoveListener;
  canvas.ontouchend = touchUpListener;
}

// socket.io code here ... see canvas-socketio.js
// @call canvas-socketio.js

function clientRemovedFromRoomListener (roomID, clientID) {
  delete userThicknesses[clientID];
  delete userColors[clientID];
  delete userCommands[clientID];
  delete userCurrentPositions[clientID];
}

function processClientAttributeUpdate (clientID, attrName, attrVal) {
  if (attrName == Attributes.THICKNESS) {
    addDrawingCommand(clientID, DrawingCommands.SET_THICKNESS, getValidThickness(attrVal));
  } else if (attrName == Attributes.COLOR) {
    addDrawingCommand(clientID, DrawingCommands.SET_COLOR, attrVal);
  }
}

function moveMessageListener (fromClientID, coordsString) {
  var coords = coordsString.split(",");
  var position = {x:parseInt(coords[0]), y:parseInt(coords[1])};
  addDrawingCommand(fromClientID, DrawingCommands.MOVE_TO, position);
}

function pathMessageListener (fromClientID, pathString) {
  var path = pathString.split(",");
  var position;
  for (var i = 0; i < path.length; i+=2) {
    position = {x:parseInt(path[i]), y:parseInt(path[i+1])};
    addDrawingCommand(fromClientID, DrawingCommands.LINE_TO, position);
  }
}

function addDrawingCommand (clientID, commandName, arg) {
  if (userCommands[clientID] == undefined) {
    userCommands[clientID] = [];
  }
  var command = {};
  command["commandName"] = commandName;
  command["arg"] = arg;
  userCommands[clientID].push(command);
  
  doProcessCommands(clientID);
}

function doProcessCommands(clientID){
    if (userCommands[clientID].length == 0) {
        return;
      }
      
      var command = userCommands[clientID].shift();
      switch (command.commandName) {
        case DrawingCommands.MOVE_TO:
          userCurrentPositions[clientID] = {x:command.arg.x, y:command.arg.y};
          break;
          
        case DrawingCommands.LINE_TO:
          if (userCurrentPositions[clientID] == undefined) {
            userCurrentPositions[clientID] = {x:command.arg.x, y:command.arg.y};
          } else {
            drawLine(userColors[clientID] || localLineColor, 
                     userThicknesses[clientID] || localLineThickness, 
                     userCurrentPositions[clientID].x, 
                     userCurrentPositions[clientID].y,
                     command.arg.x, 
                     command.arg.y);
             userCurrentPositions[clientID].x = command.arg.x; 
             userCurrentPositions[clientID].y = command.arg.y; 
          }
          break;
          
        case DrawingCommands.SET_THICKNESS:
          userThicknesses[clientID] = command.arg;
          break;
          
        case DrawingCommands.SET_COLOR:
          userColors[clientID] = command.arg;
          break;
      }
}

function pointerDownListener (e) {
  if (hasTouch) {
    return;
  }
  
  var event = e || window.event; 
  var mouseX = event.clientX - canvas.offsetLeft;
  var mouseY = event.clientY - canvas.offsetTop;
  
  penDown(mouseX, mouseY);

  if (event.preventDefault) {
    if (event.target.nodeName != "SELECT") {
      event.preventDefault();
    }
  } else {
    return false;  // IE
  }
}

function pointerMoveListener (e) {
  if (hasTouch) {
    return;
  }
  var event = e || window.event; // IE uses window.event, not e
  var mouseX = event.clientX - canvas.offsetLeft;
  var mouseY = event.clientY - canvas.offsetTop;
  
  touchPenMove(mouseX, mouseY);

  if (event.preventDefault) {
    event.preventDefault();
  } else {
    return false;  // IE
  }
}

function pointerUpListener (e) {
  if (hasTouch) {
    return;
  }
  var event = e || window.event; // IE uses window.event, not e
  var mouseX = event.clientX - canvas.offsetLeft;
  var mouseY = event.clientY - canvas.offsetTop;
  
  penUp(mouseX, mouseY);
}

function penDown (x, y) {
  isPenDown = true;
  localPen.x = x;
  localPen.y = y;
  
  var value = x + "," + y;
  var jsonObj = {room:roomID, client:currentClientId, attr:Messages.MOVE, val:value, color:localLineColor, thickness:localLineThickness};
  socket.emit('moveMsg', jsonObj);
}

function penUp (x, y) {
  isPenDown = false;
}

function drawLine (color, thickness, x1, y1, x2, y2) {
  context.strokeStyle = color;
  context.lineWidth   = thickness;
  
  context.beginPath();
  context.moveTo(x1, y1);
  context.lineTo(x2, y2);
  context.stroke();
}

function setStatus (message) {
  document.getElementById("status").innerHTML = message;
}

function iPhoneToTop () {
  if (navigator.userAgent.indexOf("iPhone") != -1) {
    setTimeout (function () {
      window.scroll(0, 0);
    }, 100);
  }
}

function getValidThickness (value) {
  value = parseInt(value);
  var thickness = isNaN(value) ? localLineThickness : value;
  return Math.max(1, Math.min(thickness, maxLineThickness));
}

// menu code here see canvas-menu.js
// @call canvas-menu.js

// touch code here see canvas-touch.js
// @call canvas-touch.js