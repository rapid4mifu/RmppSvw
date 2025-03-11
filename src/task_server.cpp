// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#include "task_server.h"

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif

#define SERVER_QUE_SEND_WAIT (10 / portTICK_PERIOD_MS)

typedef struct {
	uint32_t id;	// client id
	uint8_t data[WS_LEN_BINARY_MAX];	// binary data
	uint8_t len;	// data length
} que_ws_binary_t;

typedef struct {
	uint32_t id;	// client id
	String data;	// text data
} que_ws_text_t;

AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");
AsyncEventSource events("/events");

QueueHandle_t xQueBinToClient;
QueueHandle_t xQueTxtToClient;
TaskHandle_t hTaskServer;

CallbackOnWsConnect cbOnWsConnect = NULL;
CallbackOnWsDisconnect cbOnWsDisconnect = NULL;
CallbackOnSocketBinary cbOnSocketBinary = NULL;
CallbackOnSocketText cbOnSocketText = NULL;

static void srv_processTask(void* pvParameters);
static void srv_handleWsEvents(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *payload, size_t len);

/******************************************************************************
* Function Name: SRV_initTask
* Description  : Webサーバタスク初期化
* Arguments    : none
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool SRV_initTask(String hostName)
{
	Serial.println("Web Server initialize.");

	// send binary data to the client via WebSocket
	xQueBinToClient = xQueueCreate(10, sizeof(que_ws_binary_t));
	if (NULL == xQueBinToClient) {
		Serial.println(" [failure] Failed to create binary WebSocket queue.");
		return false;
	}

	// send text data to the client via WebSocket
	xQueTxtToClient = xQueueCreate(10, sizeof(que_ws_text_t));
	if (NULL == xQueTxtToClient) {
		Serial.println(" [failure] Failed to create text WebSocket queue.");
		return false;
	}

	if (hostName.length()) {
		Serial.println(" mDNS responder starting ...");
		// Set up mDNS responder:
		// - first argument is the domain name, in this example
		//   the fully-qualified domain name is "esp32.local"
		// - second argument is the IP address to advertise
		//   we send our IP address on the WiFi network
		if (MDNS.begin(hostName.c_str())) {
			Serial.printf("   host name : %s\n", hostName.c_str());
		} else {
			Serial.println(" [warning] Failed to set up the mDNS responder.");
		}
	}

	webSocket.onEvent(srv_handleWsEvents);
	server.addHandler(&webSocket);
	server.addHandler(&events);

	//server.on("/",handleRoot);
	server.on("/index", HTTP_ANY, [](AsyncWebServerRequest *request){
		request->send(LittleFS, "/index.html");
	});

	// Set setver all paths are not found so we can handle as per URI
	//server.onNotFound(handleWebRequests); 
	server.onNotFound([](AsyncWebServerRequest *request) {
		//if (loadFromSpiffs(request->url())) {
		//	return;
		//}
		String dataType = "text/plain";
		String path = request->url();
		if (path.endsWith("/")) {
			path += "index.html";
		}

		if (path.endsWith(".src")) {
			path = path.substring(0, path.lastIndexOf("."));
		} else if (path.endsWith(".html")) {
			dataType = "text/html";
		} else if (path.endsWith(".htm")) {
			dataType = "text/html";
		} else if (path.endsWith(".css")) {
			dataType = "text/css";
		} else if (path.endsWith(".js")) {
			dataType = "application/javascript";
		} else if (path.endsWith(".png")) {
			dataType = "image/png";
		} else if (path.endsWith(".gif")) {
			dataType = "image/gif";
		} else if (path.endsWith(".jpg")) {
			dataType = "image/jpeg";
		} else if (path.endsWith(".ico")) {
			dataType = "image/x-icon";
		} else if (path.endsWith(".xml")) {
			dataType = "text/xml";
		} else if (path.endsWith(".pdf")) {
			dataType = "application/pdf";
		} else if (path.endsWith(".zip")) {
			dataType = "application/zip";
		} else if (path.endsWith(".ttf")) {
			dataType = "font/ttf";
		} else if (path.endsWith(".woff")) {
			dataType = "font/woff";
		} else if (path.endsWith(".woff2")) {
			dataType = "font/woff2";
		}
	
		if (request->hasArg("download")) {
			dataType = "application/octet-stream";
		}

		if (LittleFS.exists(path)) {
			request->send(LittleFS, path, dataType);
		} else {
			String message = "File Not Detected\n\n";
			message += "URI: ";
			message += request->url();
			message += "\nMethod: ";
			message += (request->method() == HTTP_GET)?"GET":"POST";
			message += "\nArguments: ";
			message += request->args();
			message += "\n";

			for (uint8_t i = 0; i < request->args(); i++){
				message += " NAME:"+request->argName(i) + "\n VALUE:" + request->arg(i) + "\n";
			}
			request->send(404, "text/plain", message);
			Serial.println(message);		
			request->send(LittleFS, "/index.html", "text/plain");
		}
	});

	Serial.println(" Web Server starting ...");
	server.begin();

	Serial.println("Web Server task is now starting ...");
	BaseType_t taskCreated = xTaskCreateUniversal(srv_processTask, "srv_processTask", 4096, nullptr, 3, &hTaskServer, APP_CPU_NUM);
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create Web Server task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: srv_processTask
* Description  : Webサーバータスク
* Arguments    : none
* Return Value : none
******************************************************************************/
void srv_processTask(void* pvParameters)
{
	que_ws_binary_t queBinary;
	que_ws_text_t queText;
	TickType_t tickSendPre = xTaskGetTickCount();

	while(true) {
		// send binary data to the client via WebSocket
		if (pdPASS == xQueueReceive(xQueBinToClient, &queBinary, portMAX_DELAY)) {
			if (queBinary.len) {
				if (queBinary.id) {
					webSocket.binary(queBinary.id, &queBinary.data[0], queBinary.len);
				} else {
					webSocket.binaryAll(&queBinary.data[0], queBinary.len);
				}
			}
		}

		// send text data to the client via WebSocket
		if (pdPASS == xQueueReceive(xQueTxtToClient, &queText, portMAX_DELAY)) {
			if (queText.data.length()) {
				if (queText.id) {
					webSocket.text(queText.id, queText.data.c_str());
				} else {
					webSocket.textAll(queText.data.c_str());
				}
			}
		}

		if (1000 < (xTaskGetTickCount() - tickSendPre)) {
			tickSendPre = xTaskGetTickCount();
			webSocket.cleanupClients();
		}

		vTaskDelay(1);
	}
}

/******************************************************************************
* Function Name: srv_handleWsEvents
* Description  : WebSocketイベント
* Arguments    : none
* Return Value : none
******************************************************************************/
void srv_handleWsEvents(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *payload, size_t len)
{
	switch (type) {
	case WS_EVT_CONNECT:
		if (NULL != cbOnWsConnect) {
			cbOnWsConnect(client->id(), webSocket.count());
		}
		Serial.println("WS_EVT_CONNECT");
		{
			IPAddress ip = client->remoteIP();
			Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", client->id(), ip[0], ip[1], ip[2], ip[3], server->url());
		}
		break;

	case WS_EVT_DISCONNECT:
		if (NULL != cbOnWsDisconnect) {
			cbOnWsDisconnect(client->id(), webSocket.count());
		}
		Serial.println("WS_EVT_DISCONNECT");
		Serial.printf("[%u] Disconnected!\n", client->id());
		break;

	case WS_EVT_ERROR:
		Serial.printf("WS_EVT_ERROR [%s] [%u] (%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)payload);
		break;

	case WS_EVT_PONG:
		Serial.printf("WS_EVT_PONG [%s] [%u] (%u): %s\n", server->url(), client->id(), len, (len)?(char*)payload:"");
		break;

	case WS_EVT_DATA:
		AwsFrameInfo * info = (AwsFrameInfo*)arg;
		String msg = "";
		if(info->final && info->index == 0 && info->len == len){
			//the whole message is in a single frame and we got all of it's data
			if (info->opcode == WS_TEXT) {
				for(size_t i=0; i < info->len; i++) {
					msg += (char) payload[i];
				}
				Serial.printf("ws[%s][%u] text-message: %s\n", server->url(), client->id(), msg.c_str());

				if ("ping" == msg) {
					webSocket.text(client->id(), "pong");
				}
				
				if (NULL != cbOnSocketText) {
					cbOnSocketText(msg, client->id());
				}
			} else if (info->opcode == WS_BINARY) {
				if (NULL != cbOnSocketBinary) {
					cbOnSocketBinary(&payload[0], info->len, client->id());
				}

#if DEBUG_BINARY_MESSAGE
				char buff[3];
				uint8_t work;
				for (size_t i = 0; i < info->len; i++) {
					if (i < info->len) {
						work = payload[i];
					} else {
						work = 0;
					}

					sprintf(buff, "%02x ", (uint8_t)work);
					msg += buff;
				}
				Serial.printf("ws[%s][%u] binary-message[%llu] : %s\n", server->url(), client->id(), info->len, ,msg.c_str());
#else
				Serial.printf("ws[%s][%u] binary-message[%llu]\n", server->url(), client->id(), info->len);
#endif

			} else {
				webSocket.text(client->id(), "[warning] invalid command.\n");
			}
		}
		break;
	}
}

/******************************************************************************
* Function Name: SRV_pushWsBinaryToQueue
* Description  : WebSocketのバイナリデータを送信キューに追加する
* Arguments    : data - bainary data,
                 len - data bytes (with checksum)
* Return Value : none
******************************************************************************/
void SRV_pushWsBinaryToQueue(uint8_t * data, uint8_t len, uint32_t id)
{
	que_ws_binary_t queBinary;

	if (NULL == xQueBinToClient){
		return;
	}

	if (0 < len) {
		if (WS_LEN_BINARY_MAX < len) {
			len = WS_LEN_BINARY_MAX;
		}
		queBinary.len = len;
		queBinary.id = id;

		for (uint8_t i = 0; i < len; i++) {
			queBinary.data[i] = *(data + i);
		}
		xQueueSend(xQueBinToClient, &queBinary, SERVER_QUE_SEND_WAIT);
	}
}

/******************************************************************************
* Function Name: SRV_pushWsTextToQueue
* Description  : WebSocketのバイナリデータを送信キューに追加する
* Arguments    : data - bainary data,
                 len - data bytes (with checksum)
* Return Value : none
******************************************************************************/
void SRV_pushWsTextToQueue(String data, uint32_t id)
{
	que_ws_text_t queText;

	if (NULL == xQueTxtToClient){
		return;
	}

	if (0 < data.length()) {
		queText.id = id;
		queText.data = data;

		xQueueSend(xQueTxtToClient, &queText, SERVER_QUE_SEND_WAIT);
	}
}

/******************************************************************************
* Function Name: SRV_attachWsConnectListener
* Description  : WebSocketのクライアントが接続された時のコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
void SRV_attachWsConnectListener(CallbackOnWsConnect callback)
{
	cbOnWsConnect = callback;
}

/******************************************************************************
* Function Name: SRV_attachWsDisconnectListener
* Description  : WebSocketのクライアントが切断された時のコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
void SRV_attachWsDisconnectListener(CallbackOnWsDisconnect callback)
{
	cbOnWsDisconnect = callback;
}

/******************************************************************************
* Function Name: SRV_attachWsBinaryListener
* Description  : WebSocket（バイナリデータ）受信時のコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
void SRV_attachWsBinaryListener(CallbackOnSocketBinary callback)
{
	cbOnSocketBinary = callback;
}

/******************************************************************************
* Function Name: SRV_attachWsTextListener
* Description  : WebSocket（テキストデータ）受信時のコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
void SRV_attachWsTextListener(CallbackOnSocketText callback)
{
	cbOnSocketText = callback;
}

