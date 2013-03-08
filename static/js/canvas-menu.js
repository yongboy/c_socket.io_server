function thicknessSelectListener (e) {
  var newThickness = this.options[this.selectedIndex].value;
  localLineThickness = getValidThickness(newThickness);
  iPhoneToTop();
}

function colorSelectListener (e) {
	var newColor = this.options[this.selectedIndex].value;
	localLineColor = newColor;
	iPhoneToTop();
}

function clearCanvas(){
	socket.emit('clearCanvas', {room:roomID, client:currentClientId});
	context.clearRect(0, 0, canvas.width, canvas.height);
}

var shapeObj = {type:'rectangle'};
var registerEraseInputListeners = function () {
	canvas.onmousedown = function (e) {
		if (hasTouch) {
			return;
		}
		
		isPenDown = true;
	};
	canvas.onmousemove = function (e) {
		if (hasTouch) {
			return;
		}
		var event = e || window.event; // IE uses window.event, not e
		var x = event.clientX - canvas.offsetLeft;
		var y = event.clientY - canvas.offsetTop;
		
		if(isPenDown){
			// IE8 下面出现清除整体数据
			socket.emit('clearCanvas', {room:roomID, client:currentClientId, points:[x,y], width:20, height:20});
			context.clearRect(x, y, 20, 20);
		}
	};
	canvas.onmouseup = function (e) {
		if (hasTouch) {
			return;
		}
		isPenDown = false;
	};
	
  canvas.ontouchstart = function (e) {
	  hasTouch = true;
	  isPenDown = true;
  };
  canvas.ontouchmove = function (e) {
	  hasTouch = true;
	  e.preventDefault();
	  var x = e.changedTouches[0].clientX - canvas.offsetLeft;
	  var y = e.changedTouches[0].clientY - canvas.offsetTop;
		if(isPenDown){
			socket.emit('clearCanvas', {room:roomID, client:currentClientId, points:[x,y], width:20, height:20});
			context.clearRect(x, y, 20, 20);
		}
	};
	canvas.ontouchend = function () {
	  isPenDown = false;
	  hasTouch = false;
    };
};

var registerShapeInputListeners = function () {
	  canvas.onmousedown = function (e) {
		  if (hasTouch) {
		    return;
		  }
		  
		  var event = e || window.event;
		  shapeObj.x = event.clientX - canvas.offsetLeft;
		  shapeObj.y = event.clientY - canvas.offsetTop;
		  
		  // 保存一份当前的canvas克隆原本
		  var oriCanvas = clone(canvas);
		  shapeObj.canvas = oriCanvas;
	  };
	  canvas.onmousemove = function (e) {
	  	  if (hasTouch) {
		    return;
		  }
		  var event = e || window.event;
	  	  
	        var sx = shapeObj.x;
	        var sy = shapeObj.y;
	        var ex = event.clientX - canvas.offsetLeft;
	        var ey = event.clientY - canvas.offsetTop;
	        tmp = 0;
	        if (ex < sx) {
		        tmp = sx;
		        sx = ex;
		        ex = tmp;
	        }
	        if (ey < sy) {
		        tmp = sy;
		        sy = ey;
		        ey = tmp;
	        }
	        
	        if (shapeObj.canvas) {
	            wid = canvas.width;
	            hei = canvas.height;
		        context.clearRect(0, 0, wid, hei);
		        context.drawImage(shapeObj.canvas, 0, 0);
	        }
	        context.beginPath();
	        context.rect(sx, sy, ex-sx, ey-sy);
	        context.closePath();
	        context.stroke();
	  	  
		  // Prevent default browser actions, such as text selection
		  if (event.preventDefault) {
		    event.preventDefault();
		  } else {
		    return false;  // IE
		  }
	  };
	  canvas.onmouseup = function (e) {
		  if (hasTouch) {
			    return;
		  }
	  };
	  
	  canvas.ontouchstart = function (e) {
		  hasTouch = true;
//		  if (event.target.nodeName != "SELECT") {
//		    e.preventDefault();
//		  }
		  var touchX = e.changedTouches[0].clientX - canvas.offsetLeft;
		  var touchY = e.changedTouches[0].clientY - canvas.offsetTop;
		  if(!isPenDown){
			isPenDown = true;
		    shapeObj.x = touchX;
			shapeObj.y = touchY;
		  }
		};
		canvas.ontouchmove = function (e) {
		  hasTouch = true;
		  e.preventDefault();
		  var x = e.changedTouches[0].clientX - canvas.offsetLeft;
		  var y = e.changedTouches[0].clientY - canvas.offsetTop;
		  
		  shapeObj.lastX = x;
		  shapeObj.lastY = y;
		};
		canvas.ontouchend = function () {
		  if(shapeObj.lastX == 0){
				return;
		  }
		  var x = shapeObj.lastX;
		  var y = shapeObj.lastY;
		  drawRecFn(x, y);
		  
		  shapeObj.lastX = 0;
		  shapeObj.lastY = 0;
		  isPenDown = false;
		  hasTouch = false;
	  };
	  
	  function drawRecFn(x, y){
		  socket.emit('drawRec', {room:roomID, client:currentClientId, line:localLineThickness, style:localLineColor, points:[shapeObj.x, shapeObj.y, x-shapeObj.x, y-shapeObj.y], color:localLineColor, thickness:localLineThickness});
		  context.beginPath();
	      context.rect(shapeObj.x, shapeObj.y, x-shapeObj.x, y-shapeObj.y);
	      context.closePath();
	        
	      context.lineWidth = localLineThickness;
	      context.strokeStyle = localLineColor;
	      context.stroke();
	      
	      shapeObj.x = 0;
	      shapeObj.y = 0;
	  }
	  
	  function clone(myObj){
		  if(typeof(myObj) != 'object') return myObj;
		  if(myObj == null) return myObj;
		  
		  var myNewObj = new Object();
		  
		  for(var i in myObj)
		    myNewObj[i] = clone(myObj[i]);
		  
		  return myNewObj;
	  }
};

function initMenuFunction(){
	document.getElementById("thickness").onchange = thicknessSelectListener;
	document.getElementById("color").onchange = colorSelectListener;
	
	document.getElementById("clear").onclick = clearCanvas;
	document.getElementById("eraser").onclick = function (){
		registerEraseInputListeners();
		if(window.ActiveXObject){
			document.getElementById('canvas').style.cursor = "url(img/eraser.ico);";
		}else{
			document.getElementById('canvas').style.cssText = "cursor: url(img/eraser.gif), auto;";
		}
	};
	isFirefox = (navigator.userAgent.indexOf("Firefox")!=-1);
	document.getElementById("pencil").onclick = function(){
		registerInputListeners();
		if(window.ActiveXObject){
			document.getElementById('canvas').style.cursor = "url(img/pencil.cur), default;";
		}else{
			document.getElementById('canvas').style.cssText = "cursor: url(img/pencil.gif), auto;";
		}
	};
	
	document.getElementById("eraser").style.display = "none";
	if(isFirefox){
		document.getElementById('canvas').style.cssText = "cursor: url(img/pencil.gif), auto;";
	}
}
function resetCanvas(){
	  canvas.width  = 600;
	  canvas.height = 400;
}