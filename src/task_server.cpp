// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include "task_server.h"

#ifdef HTTP_UPDATE_ENABLE
#include "http_update.h"
#endif

#include <Arduino.h>
#include <WiFi.h>

#if defined(ESP32)
#include <AsyncTcp.h>
#include <ESPmDNS.h>
#include <Update.h>

#elif defined(TARGET_RP2040) || defined(TARGET_RP2350)
#include <FreeRTOS.h>
#include <RPAsyncTCP.h>
#include <SimpleMDNS.h>
#include <Updater.h>
#include <WiFiClient.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

extern uint8_t _FS_start;
extern uint8_t _FS_end;
#endif

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#if defined(ESP32)
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
#endif

#define SERVER_QUE_SEND_WAIT (10 / portTICK_PERIOD_MS)

#ifndef SRV_CLEANUP_INTERVAL
#define SRV_CLEANUP_INTERVAL (1000 / portTICK_PERIOD_MS)
#endif

#ifndef SRV_REBOOT_DELAY
#define SRV_REBOOT_DELAY (3000 / portTICK_PERIOD_MS)
#endif

typedef struct {
	uint32_t id;	// client id
	uint8_t data[WS_LEN_BINARY_MAX];	// binary data
	uint8_t len;	// data length
} que_ws_binary_t;

typedef struct {
	uint32_t id;	// client id
	String data;	// text data
} que_ws_text_t;

static AsyncWebServer server(80);
static AsyncWebSocket webSocket("/ws");
static AsyncEventSource events("/events");

/* process task handle */
static TaskHandle_t hTaskServer = NULL;
/* reboot timer handle */
static TimerHandle_t hTimerReboot = NULL;
/* binary message queue handle */
static QueueHandle_t xQueBinToClient = NULL;
/* text message queue handle */
static QueueHandle_t xQueTxtToClient = NULL;

static CallbackOnWsConnect cbOnWsConnect = NULL;
static CallbackOnWsDisconnect cbOnWsDisconnect = NULL;
static CallbackOnSocketBinary cbOnSocketBinary = NULL;
static CallbackOnSocketText cbOnSocketText = NULL;

static String errLast = ""; 

static void srv_processTask(void* pvParameters);
static void srv_handleNotFound(AsyncWebServerRequest *request);
static void srv_handleWsEvents(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *payload, size_t len);
static void srv_onReboot(TimerHandle_t xTimer);

#ifdef HTTP_UPDATE_ENABLE
static void srv_setupHttpUpdate(void);
static void srv_printUpdateProgress(size_t progress, size_t size);
#endif

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

	// reboot timer
	hTimerReboot = xTimerCreate("alive_timer", SRV_REBOOT_DELAY, pdTRUE, 0, srv_onReboot);
	if (NULL == hTimerReboot) {
		Serial.println(" [failure] Failed to create server reboot timer.");
		return false;
	}

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

	server.onNotFound(srv_handleNotFound);
	//server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

#ifdef HTTP_UPDATE_ENABLE
	srv_setupHttpUpdate();
#endif

#if defined(ESP32)
	if (Update.setupCrypt()) {
		Serial.println("Upload Decryption Ready");
	}
#endif

	Serial.println(" Web Server starting ...");
	server.begin();

	Serial.println("Web Server task is now starting ...");
#if defined(ESP32)
	BaseType_t taskCreated = xTaskCreateUniversal(srv_processTask, "srv_task", 4096, nullptr, 3, &hTaskServer, APP_CPU_NUM);
#else
	BaseType_t taskCreated = xTaskCreate(srv_processTask, "srv_task", configMINIMAL_STACK_SIZE * 16, nullptr, 3, &hTaskServer);
#if defined(PICO_CYW43_SUPPORTED)
	// The PicoW WiFi chip controls the LED, and only core 0 can make calls to it safely
	vTaskCoreAffinitySet(hTaskServer, 1 << 0);
#endif
#endif
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
		if (pdPASS == xQueueReceive(xQueBinToClient, &queBinary, 0)) {
			if (queBinary.len) {
				if (queBinary.id) {
					webSocket.binary(queBinary.id, &queBinary.data[0], queBinary.len);
				} else {
					webSocket.binaryAll(&queBinary.data[0], queBinary.len);
				}
			}
		}

		// send text data to the client via WebSocket
		if (pdPASS == xQueueReceive(xQueTxtToClient, &queText, 0)) {
			if (queText.data.length()) {
				if (queText.id) {
					webSocket.text(queText.id, queText.data.c_str());
				} else {
					webSocket.textAll(queText.data.c_str());
				}
			}
		}

		if (SRV_CLEANUP_INTERVAL < (xTaskGetTickCount() - tickSendPre)) {
			tickSendPre = xTaskGetTickCount();
			webSocket.cleanupClients();
		}

		vTaskDelay(1);
	}
}
/******************************************************************************
* Function Name: srv_handleNotFound
* Description  : WebSocketイベント
* Arguments    : request
* Return Value : none
******************************************************************************/
void srv_handleNotFound(AsyncWebServerRequest *request)
{
	String dataType = "text/plain";
	String path = request->url();
	if (path.endsWith("/")) {
		path += "index.html";
	}

	if (path.endsWith(".src")) {
		path = path.substring(0, path.lastIndexOf("."));
	} else if (path.endsWith(".html")) {
		dataType = "text/html";
	} else if (path.endsWith(".css")) {
		dataType = "text/css";
	} else if (path.endsWith(".js")) {
		dataType = "text/javascript";
		//dataType = "application/javascript";
	} else if (path.endsWith(".ttf")) {
		dataType = "font/ttf";
	} else if (path.endsWith(".woff")) {
		dataType = "font/woff";
	} else if (path.endsWith(".woff2")) {
		dataType = "font/woff2";
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
	} else if (path.endsWith(".gz")) {
		dataType = "text/javascript";
	}

	if (request->hasArg("download")) {
		dataType = "application/octet-stream";
	}

	if (LittleFS.exists(path)) {// || LittleFS.exists(pathWithGz)) {
#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
		//bool gzipped = false;
		//if (LittleFS.exists(pathWithGz)) {
		//	gzipped = true;
		//	path += ".gz";
		//}
		
		//Serial.println(String("[WEBSERVER] opening file: ") + path);
		File file = LittleFS.open(path, "r");
		//String etag = String(file.size());
		AsyncWebServerResponse *response = request->beginChunkedResponse(
			dataType.c_str(), 
			[file](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
				maxLen = 1024;
				auto localHandle = file;
				//Serial.printf("[HTTP]  [%6d] INDEX [%6d] BUFFER_MAX [%6d] NAME [%s]\n", index, localHandle.size(), maxLen, localHandle.name());
				size_t len = localHandle.read(buffer, maxLen);
				//Serial.printf(">> Succcessful read of %d\n", len);
				if (len == 0)
				{
					//Serial.printf("Closing [%d]\n", file);
					localHandle.close();
				}
				return len;
			}
		);

		//if (gzipped) {
		//	response->addHeader(F("Content-Encoding"), F("gzip"));
		//}
		//response->addHeader(asyncsrv::T_ETag, etag);
		request->send(response);
#else
		request->send(LittleFS, path, dataType);
#endif

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
		//request->send(LittleFS, "/index.html", "text/plain");
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
#if DEBUG_TEXT_MESSAGE
				Serial.printf("ws[%s][%u] text-message: %s\n", server->url(), client->id(), msg.c_str());
#endif

				if (msg == "ping") {
					webSocket.text(client->id(), "pong");
				} else {
					if (NULL != cbOnSocketText) {
						cbOnSocketText(msg, client->id());
					}
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
#endif

			} else {
				webSocket.text(client->id(), "[warning] invalid command.\n");
			}
		}
		break;
	}
}

/******************************************************************************
* Function Name: srv_onReboot
* Description  : タイマイベントによる再起動処理
* Arguments    : none
* Return Value : none
******************************************************************************/
void srv_onReboot(TimerHandle_t xTimer)
{
	Serial.println("will be restarted soon ...");
#if defined(ESP32)
	ESP.restart();
#elif defined(TARGET_RP2040) || defined(TARGET_RP2350)
	rp2040.reboot();
#endif
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

#ifdef HTTP_UPDATE_ENABLE
/******************************************************************************
* Function Name: srv_setupHttpUpdate
* Description  : HTTP（Webブラウザ）アップデートの初期化
* Arguments    : none
* Return Value : none
******************************************************************************/
void srv_setupHttpUpdate(void)
{
	// handler for the update web page
	String pathUpdate = "/" + String(update_url);
	server.on(pathUpdate.c_str(), HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(200, PSTR("text/html"), update_html);
	});
	
	// handler for the update page form POST
	server.on(pathUpdate.c_str(), HTTP_POST, [](AsyncWebServerRequest *request){
		// handler when file upload finishes
		AsyncWebServerResponse* response;// = request->beginResponse((Update.hasError()) ? 400 : 200, "text/plain", (Update.hasError()) ? Update.errorString() : "OK");
		if (Update.hasError()) {
			Serial.printf("update faild. %s\n", errLast.c_str());
			response = request->beginResponse(400, "text/plain", errLast.c_str());
			response->addHeader("Connection", "close");
		} else {
			Serial.printf("update success.\n");
			xTimerStart(hTimerReboot, 0);
			response = request->beginResponse(200, "text/plain", "Update Success! Rebooting ...");
		}
		request->send(response);

	}, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
		// handler for the file upload, gets the sketch bytes, and writes
		// them through the Update object

		if (!index) {
			String message;
			int params = request->params();
			Serial.printf("%d params sent in\n", params);
			for (int i = 0; i < params; i++)
			{
				const AsyncWebParameter *p = request->getParam(i);
				if (p->isFile())
				{
					Serial.printf(" _FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
				}
				else if (p->isPost())
				{
					Serial.printf(" _POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
				}
				else
				{
					Serial.printf(" _GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
				}
			}
			// open the file on first call and store the file handle in the request object
			String fileType = "firmware";
			if (request->hasArg("type")) {
				fileType = request->getParam("type")->value();
			}

			uint32_t update_size = 0;
			int update_cmd = 0;
#if defined(ESP32)
			if (fileType == "filesystem") {
				Serial.printf("update filesystem : %s\n", filename.c_str());
				update_size = LittleFS.totalBytes();
				update_cmd = U_SPIFFS;
			} else if (fileType == "firmware") {
				Serial.printf("update firmware : %s\n", filename.c_str());
				update_size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
				update_cmd = U_FLASH;
			}
#elif defined(TARGET_RP2040) || defined(TARGET_RP2350)
			if (fileType == "filesystem") {
				Serial.printf("update filesystem : %s\n", filename.c_str());
				update_size = ((size_t)&_FS_end - (size_t)&_FS_start);
				update_cmd = U_FS;
			} else if (fileType == "firmware") {
				Serial.printf("update firmware : %s\n", filename.c_str());
				FSInfo i;
				LittleFS.begin();
				LittleFS.info(i);
				update_size = i.totalBytes - i.usedBytes;
				update_cmd = U_FLASH;
			}
#endif

			if (!Update.begin(update_size, update_cmd)) {
				StreamString str;
				Update.printError(str);
				errLast = str.c_str();
			}
		}

		if (len) {
			// stream the incoming chunk to the opened file
			if (Update.write(data, len) != len) {
				StreamString str;
				Update.printError(str);
				errLast = str.c_str();
				return request->send(400, "text/plain", "failed to write");
			}
		}

		if (final) {
			// close the file handle as the upload is now done
			if (Update.end(true)) {  //true to set the size to the current progress
				Serial.printf("Update Success: %u\nRebooting ...\n");//, upload.totalSize);
			} else {
				StreamString str;
				Update.printError(str);
				errLast = str.c_str();
			}
		}
	});

	Update.onProgress(srv_printUpdateProgress);
}

/******************************************************************************
* Function Name: srv_printUpdateProgress
* Description  : プログレス表示
* Arguments    : none
* Return Value : none
******************************************************************************/
void srv_printUpdateProgress(size_t progress, size_t size)
{
	static int last_progress = -1;
	if (size > 0) {
		progress = (progress * 100) / size;
		progress = (progress > 100 ? 100 : progress);  //0-100
		if (progress != last_progress) {
			Serial.printf("\nProgress: %d%%", progress);
			last_progress = progress;
		}
	}
}
#endif
