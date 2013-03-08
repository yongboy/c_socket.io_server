function touchDownListener (e) {
  hasTouch = true;
//  if (event.target.nodeName != "SELECT") {
//    e.preventDefault();
//  }
  var touchX = e.changedTouches[0].clientX - canvas.offsetLeft;
  var touchY = e.changedTouches[0].clientY - canvas.offsetTop;
  if (!isPenDown) {
    penDown(touchX, touchY);
  }
}

function touchMoveListener (e) {
  hasTouch = true;
  e.preventDefault();
  var touchX = e.changedTouches[0].clientX - canvas.offsetLeft;
  var touchY = e.changedTouches[0].clientY - canvas.offsetTop;
  touchPenMove(touchX, touchY);
}

function touchPenMove (x, y) {
	  if (isPenDown) {
	    var value = x + "," + y;
	    socket.emit('pathMsg', {room:roomID, client:currentClientId, attr:Messages.PATH, val:value});
	    
	    drawLine(localLineColor, localLineThickness, localPen.x, localPen.y, x, y);
	    
	    localPen.x = x;
	    localPen.y = y;
	  }
	}

function touchUpListener () {
  penUp();
}