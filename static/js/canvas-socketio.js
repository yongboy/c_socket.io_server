socket = io.connect(connUrl, connOptions);
socket.on('connect', function() {
	socket.emit('roomNotice', {room:roomID});
});

socket.on('clientId', function(obj){
	currentClientId = obj.id;
});

socket.on('disconnect', function() {
  setStatus("Disconnected from Server.");
});

socket.on('clearCanvas', function(obj){
	if(!obj.points)
		context.clearRect(0, 0, canvas.width, canvas.height);
	else{
		context.clearRect(obj.points[0], obj.points[1], obj.width, obj.height);
	}
});

socket.on('drawRec', function(obj){
	processClientAttributeUpdate(obj.client, Attributes.THICKNESS, obj.thickness);
	processClientAttributeUpdate(obj.client, Attributes.COLOR, obj.color);
	
	context.beginPath();
	var points = obj.points;
    context.rect(points[0], points[1], points[2], points[3]);
    context.closePath();
    
    context.lineWidth = obj.line;
    context.strokeStyle = obj.style;
    context.stroke();
});

socket.on('roomCount', function(obj){
	  var numOccupants = obj.num;
	  if (numOccupants == 1) {
	    setStatus("Now drawing on your own (no one else is here at the moment)");
	  } else if (numOccupants == 2) {
	    setStatus("Now drawing with " + (numOccupants-1) + " other person");
	  } else {
	    setStatus("Now drawing with " + (numOccupants-1) + " other people");
	  }
});

socket.on('attrUpdate', function(jobj){
	processClientAttributeUpdate(jobj.client, jobj.attr, jobj.val);
});

socket.on('clientQuit', function(obj){
	clientRemovedFromRoomListener(obj.room, obj.client);
});

socket.on('moveMsg', function(obj){
	userThicknesses[obj.client] = obj.thickness;
	userColors[obj.client] = obj.color;
	moveMessageListener(obj.client, obj.val);
});

socket.on('pathMsg', function(obj){
	pathMessageListener(obj.client, obj.val);
});