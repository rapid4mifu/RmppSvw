// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include <Arduino.h>
#include <WiFi.h>

#include "board.h"
#include "task_cfg.h"
#include "task_cli.h"
#include "task_input.h"
#include "task_led.h"
#include "task_rmpp.h"
#include "task_server.h"
#include "task_system.h"

void reboot(void) {
	Serial.println("will be restarted soon ...");
	vTaskDelay(5000);
	ESP.restart();
}

void setup() {
	// ----- output reset (set to Hi-Z) -----
	RMPP_resetOutput();

	// ----- UART initialize -----
	Serial.begin(115200);
	vTaskDelay(2000);

	// ----- CLI (Command Line Interface) task initialize -----
	if (false == CLI_initTask()) {
		reboot();
	}
		
	// ----- RMPP (Railway Model Power Pack) task initialize -----
	if (false == RMPP_initTask()) {
		reboot();
	}
	
	// ----- Button task initialize -----
	if (false == INP_initTask()) {
		reboot();
	}

	// ----- LED task initialize -----
#if defined(ARDUINO_M5Stack_ATOM)
	led_type_t typeLed = LED_TYPE_RGB_SERIAL;
#elif defined(ARDUINO_RASPBERRY_PI_PICO_2W)
	led_type_t typeLed = LED_TYPE_1COLOR;
#endif
	if (false == LED_initTask(PIN_LED, typeLed)) {
		reboot();
	}
	LED_setColor(LED_COL_WHITE);

	// ----- Configuration task initialize -----
	if (false == CFG_initTask()) {
		reboot();
	}

	// ----- Wi-Fi setting -----
	system_config_t cfgSys;
	if (CFG_isApModeEnabled()) {
		cfgSys.wifiMode = WIFI_AP;
		cfgSys.wifiSsid = CFG_getApModeSSID();
		cfgSys.wifiPass = CFG_getApModePass();
		cfgSys.ipLocal = IPAddress(192, 168, 0, 1);
		cfgSys.ipGateway = IPAddress(192, 168, 0, 1);
		cfgSys.ipSubnet = IPAddress(255, 255, 255, 0);

		// color for access point mode operation
		LED_setColorForStandby(LED_COL_MAGENTA);
	} else {
		cfgSys.wifiMode = WIFI_STA;
		cfgSys.wifiSsid = CFG_getStaModeSSID();
		cfgSys.wifiPass = CFG_getStaModePass();
		cfgSys.ipLocal = CFG_getLocalAddress();
		cfgSys.ipGateway = CFG_getDefaultGateway();
		cfgSys.ipSubnet = CFG_getSubnetMask();
	
		// color for station mode operation
		LED_setColorForStandby(LED_COL_BLUE);
	}
	LED_setColor(LED_COL_STNDBY);

	// ----- System on Chip task initialize -----
	if (false == SYS_initTask(&cfgSys)) {
		reboot();
	}

	// ----- Web Server task initialize -----
	if (SYS_isWiFiAvailable()) {
		if (false == SRV_initTask(CFG_getHostName())) {
			reboot();
		}
	}
}

void loop()
{
	vTaskDelay(1000);
}
