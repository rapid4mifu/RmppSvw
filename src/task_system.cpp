// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include "task_system.h"

#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
#include <FreeRTOS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <task.h>
#endif

#include <ArduinoOTA.h>
#include <FS.h>
#include <LittleFS.h> 

#if defined(ESP32)
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
#endif

#define WIFI_STA_SETUP_INTERVAL 500 // [msec]
#define WIFI_STA_SETUP_TIMEOUT 20000 // [msec]
#define WIFI_STA_TIMEOUT_COUNT (WIFI_STA_SETUP_TIMEOUT / WIFI_STA_SETUP_INTERVAL)

static system_config_t cfgSystem = {
	WIFI_STA,					// Wi-Fi mode
	"",							// Wi-Fi SSID
	"",							// Wi-Fi Password
	IPAddress(0, 0, 0, 0),		// local address
	IPAddress(0, 0, 0, 0),		// default gateway
	IPAddress(255, 255, 255, 0)	// subnet mask
};

/* process task handle */
static TaskHandle_t hTaskSystem = NULL;
static CallbackWifiEvent cbWifiEvent = NULL;
static bool wifiAvailable = false;

static void sys_processTask(void* pvParameters);

static void sys_updateWifiStatus(void);
static void sys_onWiFiEvent(SYS_WIFI_EVENT_PARAM param);

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

	if (WIFI_STA == cfgSystem.wifiMode) {
		Serial.println(" Wi-Fi starting (station mode) ...");

#ifndef ESP32
		if (0 == strlen(cfgSystem.wifiSsid) || 0 == strlen(cfgSystem.wifiPass)) {
			Serial.println(" [warning] The SSID and Password cannot be empty.");
			return true;
		}
#endif
		if (WL_CONNECTED == WiFi.status()) {
			Serial.println("  disconect");
			WiFi.disconnect(true);
			vTaskDelay(100);
		}

		WiFi.mode(WIFI_STA);
#if defined(ESP32)
		WiFi.onEvent(sys_onWiFiEvent);
		if (!WiFi.config(cfgSystem.ipLocal, cfgSystem.ipGateway, cfgSystem.ipSubnet)) {
			Serial.println("  [warning] STA failed to configure. Please check the TCP/IP configuration.");
			return false;
		}
		wl_status_t wifiStatus = WiFi.begin();
#else
		WiFi.config(cfgSystem.ipLocal, cfgSystem.ipGateway, cfgSystem.ipSubnet);
		int wifiStatus = WiFi.begin(cfgSystem.wifiSsid, cfgSystem.wifiPass);
#endif

		sys_updateWifiStatus();
		if (WL_CONNECT_FAILED == wifiStatus || WL_NO_SSID_AVAIL == wifiStatus) {
			Serial.printf("  [failure] Failed connect, status code : %d\n", wifiStatus);
			Serial.printf("   ssid = %s, password = %s\n", cfgSystem.wifiSsid, cfgSystem.wifiPass);
			while (WiFi.status() != WL_CONNECTED) {
				Serial.println("  [info] Please update the Wi-Fi credential.");
				vTaskDelay(10000);
			}
			return false;
		}

		uint16_t timeout_cnt = 0;
		while (WiFi.status() != WL_CONNECTED) {
			if (WIFI_STA_TIMEOUT_COUNT <= timeout_cnt) {
				Serial.println("  [warning] Connection to the access point timed out.");
				WiFi.disconnect();
				return false;
			}

			vTaskDelay(WIFI_STA_SETUP_INTERVAL);
			sys_updateWifiStatus();
			timeout_cnt++;
		}

	} else if (WIFI_AP == cfgSystem.wifiMode) {
		Serial.println(" Wi-Fi starting (access point mode) ...");

		if (0 == strlen(cfgSystem.wifiSsid) || 0 == strlen(cfgSystem.wifiPass)) {
			Serial.println(" [warning] The SSID and Password cannot be empty.");
			return true;
		}

		if (INADDR_NONE == cfgSystem.ipLocal || INADDR_NONE == cfgSystem.ipGateway || INADDR_NONE == cfgSystem.ipSubnet) {
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
#if defined(ESP32)
		WiFi.onEvent(sys_onWiFiEvent);
#endif
		
		if (false == WiFi.softAP(cfgSystem.wifiSsid, cfgSystem.wifiPass)) {
			Serial.println(" [failure] AP setup failed.");
			sys_updateWifiStatus();
			return false;
		}

		vTaskDelay(100);
		if (false == WiFi.softAPConfig(cfgSystem.ipLocal, cfgSystem.ipGateway, cfgSystem.ipSubnet)) {
			Serial.println(" [failure] AP failed to configure. Please check the TCP/IP configuration.");
			sys_updateWifiStatus();
			return false;
		}
	}
	sys_updateWifiStatus();

	if (SYS_isWiFiAvailable()) {
		// ----- OTA initialize -----
		Serial.println(" OTA starting ...");

		ArduinoOTA.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH) {
				type = "sketch";
			} else { // U_SPIFFS
				type = "filesystem";
			}

			// NOTE: if updating file system this would be the place to unmount file system using end()
			LittleFS.end();

			Serial.println("Start updating " + type);
		});
		ArduinoOTA.onEnd([]() {
			Serial.println("\nEnd");
		});
		ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
			Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
		});
		ArduinoOTA.onError([](ota_error_t error) {
			Serial.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
			else if (error == OTA_END_ERROR) Serial.println("End Failed");
		});
		
		ArduinoOTA.begin();
	}

	Serial.println("System (SoC) task is now starting ...");
#if defined(ESP32)
	BaseType_t taskCreated = xTaskCreateUniversal(sys_processTask, "sys_task", 4096, nullptr, 3, &hTaskSystem, APP_CPU_NUM);
#else
	BaseType_t taskCreated = xTaskCreate(sys_processTask, "sys_task", configMINIMAL_STACK_SIZE * 4, nullptr, 3, &hTaskSystem);
#if defined(PICO_CYW43_SUPPORTED)
	// The PicoW WiFi chip controls the LED, and only core 0 can make calls to it safely
	vTaskCoreAffinitySet(hTaskSystem, 1 << 0);
#endif
#endif
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
		if (SYS_isWiFiAvailable()) {
			ArduinoOTA.handle();
		}

		sys_updateWifiStatus();

		vTaskDelay(1);
	}
}

/******************************************************************************
* Function Name: sys_updateWifiStatus
* Description  : Wi-Fiのイベント処理
* Arguments    : none
* Return Value : none
******************************************************************************/
void sys_updateWifiStatus(void)
{
#ifndef ESP32
	static wl_status_t preEvent = WL_IDLE_STATUS;
	wl_status_t status = (wl_status_t)WiFi.status();

	if (preEvent == status) {
		return;
	}
	preEvent = status;

	sys_onWiFiEvent((SYS_WIFI_EVENT_PARAM)status);
#endif
}

/******************************************************************************
* Function Name: sys_onWiFiEvent
* Description  : Wi-Fiのイベント処理
* Arguments    : event
* Return Value : none
******************************************************************************/
// WARNING: This function is called from a separate FreeRTOS task (thread)!
void sys_onWiFiEvent(SYS_WIFI_EVENT_PARAM param)
{
	if (NULL != cbWifiEvent) {
		cbWifiEvent(param);
	}

	if (WL_CONNECTED == WiFi.status()) {
		wifiAvailable = true;
	} else {
		wifiAvailable = false;
	}

#if defined(ESP32)
	switch (param) {
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
		wifiAvailable = false;
		Serial.println(" [WiFi] station mode stopped");
		break;
	case ARDUINO_EVENT_WIFI_STA_CONNECTED:
		wifiAvailable = true;
		Serial.println(" [WiFi] Connected to access point");
		Serial.printf("   SSID : %s, RSSI : %d dBm\n", WiFi.SSID().c_str(), WiFi.RSSI());
		break;
	case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
		wifiAvailable = false;
		Serial.println(" [WiFi] Disconnected from access point");
		break;
	case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: 
		Serial.println(" [WiFi] Authentication mode of access point has changed");
		break;
	case ARDUINO_EVENT_WIFI_STA_GOT_IP:
		Serial.printf(" [WiFi] Obtained IP locak address : %s\n", WiFi.localIP().toString().c_str());
		break;
	case ARDUINO_EVENT_WIFI_STA_LOST_IP:
		Serial.println(" [WiFi] Lost IP address and IP address is reset to 0");
		break;
	case ARDUINO_EVENT_WIFI_AP_START:
		wifiAvailable = true;
		Serial.println(" [WiFi] access point started");
		break;
	case ARDUINO_EVENT_WIFI_AP_STOP:
		wifiAvailable = false;
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

#else
	switch (param) {
		case WL_IDLE_STATUS:
			Serial.println(" [WiFi] Interface is ilding.");
			break;
		case WL_NO_SSID_AVAIL:
			Serial.println(" [WiFi] There is no available SSID.");
			break;
		case WL_SCAN_COMPLETED:
			Serial.println(" [WiFi] Completed scan for access points.");
			break;
		case WL_CONNECTED:
			wifiAvailable = true;
			Serial.println(" [WiFi] Connected to access point.");
			Serial.printf("   SSID : %s, RSSI : %d dBm\n", WiFi.SSID().c_str(), WiFi.RSSI());
			Serial.printf("   IP local address : %s\n", WiFi.localIP().toString().c_str());
			break;
		case WL_CONNECT_FAILED:
			Serial.println(" [WiFi] Failed to connect to the access point.");
			break;
		case WL_CONNECTION_LOST:
			Serial.println(" [WiFi] Lost IP address and IP address is reset to 0");
			break;
		case WL_DISCONNECTED: 
			Serial.println(" [WiFi] Disconnected from access point");
			break;
		case WL_AP_LISTENING:
			wifiAvailable = true;
			Serial.println(" [WiFi] client listening.");
			break;
		case WL_AP_CONNECTED:
			Serial.println(" [WiFi] client connected.");
			break;
		case WL_AP_FAILED:
			wifiAvailable = false;
			Serial.println(" [WiFi] access point faild.");
			break;
		default:
			break;
	}
#endif
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

/******************************************************************************
* Function Name: SYS_isWiFiAvailable
* Description  : WebSocket（バイナリデータ）受信時のコールバック関数を設定
* Arguments    : callback - function pointer
* Return Value : none
******************************************************************************/
bool SYS_isWiFiAvailable(void)
{
	return wifiAvailable;
}
