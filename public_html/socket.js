// 接続先URI
var uri = "ws://" + location.hostname + "/ws";

var ws_opened = false;
var pingPongTimer = null;
var callbackMessage = null;

// WebSocketオブジェクト
const webSocket = new ReconnectingWebSocket(uri, null, {debug:true, binaryType:"arraybuffer"});

const checkConnection = () => {
	setTimeout(() => {
		if (null != webSocket && ws_opened) {
			webSocket.send("ping");
		}
		pingPongTimer = setTimeout(() => {
			console.warn('try to reconnect...');
			pingPongTimer = null;
			//webSocket.reconnect();
			//webSocket = null;
			webSocket.refresh();
		}, 1000);
	}, 4000);
};

webSocket.onopen = () => {
	console.info('socket is opened : ', new Date());
	ws_opened = true;
	checkConnection();
};

webSocket.onmessage = (event) => {
	//console.info(event);
	//var invalid = 0;
	if ('pong' === event.data) {
		clearTimeout(pingPongTimer);
		return checkConnection();
	} else if (event.data.constructor === ArrayBuffer) {
		var arr = new Uint8Array(event.data);
		if (callbackMessage) {
			callbackMessage(arr);
		}
	} else {
		console.warn(event);
	}
};

export const setCallbackMessage = (newCallback) => {
	callbackMessage = newCallback;
};

export const sendWebSocketDataHex = (hexStrings) => {
	if (null != webSocket && ws_opened) {
		const hexNumbers = hexStrings.map((hex) => Number('0x' + hex));
		const u8 = new Uint8Array(hexNumbers);
		sendWebSocketData(u8);
	}
};

export const sendWebSocketData = (data) => {
	if (null != webSocket && ws_opened) {
		webSocket.send(data);
	}
};
