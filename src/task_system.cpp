// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#include "task_system.h"

#include <ArduinoOTA.h>
#include <FS.h>
#include <LittleFS.h> 

#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif

#define WIFI_STA_SETUP_INTERVAL 50 // [msec]
#define WIFI_STA_SETUP_TIMEOUT 10000 // [msec]
#define WIFI_STA_TIMEOUT_COUNT (WIFI_STA_SETUP_TIMEOUT / WIFI_STA_SETUP_INTERVAL)

static system_config_t cfgSystem = {
	WIFI_STA,					// Wi-Fi mode
	"",							// Wi-Fi SSID
	"",							// Wi-Fi Password
	IPAddress(0, 0, 0, 0),		// local address
	IPAddress(0, 0, 0, 0),		// default gateway
	IPAddress(255, 255, 255, 0)	// subnet mask
};

TaskHandle_t hTaskSystem;
CallbackWifiEvent cbWifiEvent = NULL;

void sys_processTask(void* pvParameters);
void sys_onWiFiEvent(WiFiEvent_t event);

/******************************************************************************
* Function Name: SYS_initTask
* Description  : システムに関する処理の初期化
* Arguments    : cfg - system configration
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool SYS_initTask(system_config_t* cfg)
{
	Serial.println("System (SoC) initialize.");

	// ----- Fili System initialize -----
	Serial.println(" File System starting ...");
	if (false == LittleFS.begin()) {
		Serial.println("  [failure] Failed to initialize file system.");
		return false;
	}

	// ----- Wi-Fi initialize -----
#ifdef ARDUINO_XIAO_ESP32C6
	// anttena select
	//  low  : built-in antenna; 
	//  high : external antenna.
	pinMode(14, OUTPUT); 
	digitalWrite(14, LOW);
#endif

	if (nullptr != cfg) {
		cfgSystem.wifiMode = cfg->wifiMode;
		cfgSystem.wifiSsid = cfg->wifiSsid;
		cfgSystem.wifiPass = cfg->wifiPass;
		cfgSystem.ipLocal = cfg->ipLocal;
		cfgSystem.ipGateway = cfg->ipGateway;
		cfgSystem.ipSubnet = cfg->ipSubnet;
	}

	if (WIFI_MODE_STA == cfgSystem.wifiMode) {
		Serial.println(" Wi-Fi starting (station mode) ...");

		if (WL_CONNECTED == WiFi.status()) {
			Serial.println("  disconect");
			WiFi.disconnect(true);
			vTaskDelay(100);
		}

		WiFi.onEvent(sys_onWiFiEvent);

		if (!WiFi.config(cfgSystem.ipLocal, cfgSystem.ipGateway, cfgSystem.ipSubnet)) {
			Serial.println("  [warning] STA failed to configure. Please check the TCP/IP configuration.");
			return false;
		}
		wl_status_t wifiStatus = WiFi.begin();
		if (WL_CONNECT_FAILED == wifiStatus || WL_NO_SSID_AVAIL == wifiStatus) {
			Serial.printf("  [failure] Failed connect, status code : %d\n", wifiStatus);
			while (WiFi.status() != WL_CONNECTED) {
				Serial.println("  [info] Please update the Wi-Fi credential.");
				vTaskDelay(10000);
			}
			return false;
		}

		uint16_t timeout_cnt = 0;
		while (WiFi.status() != WL_CONNECTED) {
			if (WIFI_STA_TIMEOUT_COUNT < timeout_cnt) {
				Serial.println("  [warning] Connection to the access point timed out.");
				WiFi.disconnect();
				return false;
			}
	
			vTaskDelay(WIFI_STA_SETUP_INTERVAL);
			timeout_cnt++;
		}
		Serial.println(" Connected to the access point.");

	} else if (WIFI_AP == cfgSystem.wifiMode) {
		Serial.println(" Wi-Fi starting (access point mode) ...");

		if (0 == strlen(cfgSystem.wifiSsid) || 0 == strlen(cfgSystem.wifiPass)) {
			Serial.println(" [failure] The SSID and Password cannot be empty in access point mode.");
			return false;
		} else if (INADDR_NONE == cfgSystem.ipLocal || INADDR_NONE == cfgSystem.ipGateway || INADDR_NONE == cfgSystem.ipSubnet) {
			Serial.println(" [warning] TCP/IP settings are required in access point mode.");

			cfgSystem.ipLocal = IPAddress(192, 168, 0, 1);
			cfgSystem.ipGateway = IPAddress(192, 168, 0, 1);
			cfgSystem.ipSubnet = IPAddress(255, 255, 255, 0);
			Serial.printf("   The access point mode will start with the following TCP/IP settings.\n");
			Serial.printf("     local address   : %s\n", cfgSystem.ipLocal.toString().c_str());
			Serial.printf("     default gateway : %s\n", cfgSystem.ipGateway.toString().c_str());
			Serial.printf("     subnet mask     : %s\n", cfgSystem.ipSubnet.toString().c_str());
		}

		WiFi.mode(WIFI_AP);
		WiFi.onEvent(sys_onWiFiEvent);
		
		if (false == WiFi.softAP(cfgSystem.wifiSsid, cfgSystem.wifiPass)) {
			Serial.println(" [failure] AP setup failed.");
			return false;
		}

		vTaskDelay(100);
		if (false == WiFi.softAPConfig(cfgSystem.ipLocal, cfgSystem.ipGateway, cfgSystem.ipSubnet)) {
			Serial.println(" [failure] AP failed to configure. Please check the TCP/IP configuration.");
			return false;
		}
	}

	// ----- OTA initialize -----
	Serial.println(" OTA starting ...");
	ArduinoOTA
	.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
			type = "sketch";
		} else { // U_SPIFFS
			type = "filesystem";
		}

		// NOTE: if updating file system this would be the place to unmount file system using end()
		LittleFS.end();

		Serial.println("Start updating " + type);
	})
	.onEnd([]() {
		Serial.println("\nEnd");
	})
	.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	})
	.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.begin();

	Serial.println("System (SoC) task is now starting ...");
	BaseType_t taskCreated = xTaskCreateUniversal(sys_processTask, "sys_processTask", 4096, nullptr, 3, &hTaskSystem, APP_CPU_NUM);
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create System task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: sys_processTask
* Description  : システムに関する処理
* Arguments    : none
* Return Value : none
******************************************************************************/
void sys_processTask(void* pvParameters)
{
	while(true) {
		ArduinoOTA.handle();
		vTaskDelay(1);
	}
}

/******************************************************************************
* Function Name: sys_onWiFiEvent
* Description  : Wi-Fiのイベント処理
* Arguments    : event
* Return Value : none
******************************************************************************/
// WARNING: This function is called from a separate FreeRTOS task (thread)!
void sys_onWiFiEvent(WiFiEvent_t event)
{
	if (NULL != cbWifiEvent) {
		cbWifiEvent(event);
	}

	switch (event) {
		case ARDUINO_EVENT_WIFI_READY:
			Serial.println(" [WiFi] interface ready");
			break;
		case ARDUINO_EVENT_WIFI_SCAN_DONE:
			Serial.println(" [WiFi] Completed scan for access points");
			break;
		case ARDUINO_EVENT_WIFI_STA_START:
			Serial.println(" [WiFi] station mode started");
			break;
		case ARDUINO_EVENT_WIFI_STA_STOP:
			Serial.println(" [WiFi] station mode stopped");
			break;
		case ARDUINO_EVENT_WIFI_STA_CONNECTED:
			Serial.println(" [WiFi] Connected to access point");
			Serial.printf("   SSID : %s, RSSI : %d dBm\n", WiFi.SSID().c_str(), WiFi.RSSI());
			break;
		case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
			Serial.println(" [WiFi] Disconnected from access point");
			break;
		case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: 
			Serial.println(" [WiFi] Authentication mode of access point has changed");
			break;
		case ARDUINO_EVENT_WIFI_STA_GOT_IP:
			Serial.print(" [WiFi] Obtained IP address: ");
			Serial.println(WiFi.localIP());
			break;
		case ARDUINO_EVENT_WIFI_STA_LOST_IP:
			Serial.println(" [WiFi] Lost IP address and IP address is reset to 0");
			break;
		case ARDUINO_EVENT_WIFI_AP_START:
			Serial.println(" [WiFi] access point started");
			break;
		case ARDUINO_EVENT_WIFI_AP_STOP:
			Serial.println(" [WiFi] access point stopped");
			break;
		case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
			Serial.println(" [WiFi] client connected");
			break;
		case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
			Serial.println(" [WiFi] client disconnected");
			break;
		default:
			break;
	}
}

/******************************************************************************
* Function Name: SYS_attachWiFiEventListener
* Description  : WebSocket（バイナリデータ）受信時のコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
void SYS_attachWiFiEventListener(CallbackWifiEvent callback)
{
	cbWifiEvent = callback;
}