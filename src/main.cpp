// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "board.h"
#include "task_cfg.h"
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
	auto cfg = M5.config();
	M5.begin(cfg);

	// ----- RMPP (Railway Model Power Pack) task initialize -----
	if (false == RMPP_initTask()) {
		reboot();
	}

	// ----- LED task initialize -----
	if (false == LED_initTask(PIN_LED, LED_TYPE_RGB_SERIAL)) {
		reboot();
	}
	LED_setColor(LED_COL_WHITE);
	
	// ----- UART initialize -----
	Serial.begin(115200);
	vTaskDelay(1000);
	
	// ----- Configuration task initialize -----
	if (false == CFG_initTask()) {
		reboot();
	}

	// ----- Wi-Fi setting -----
	system_config_t cfgSys;
	if (CFG_isApModeEnabled()) {
		cfgSys.wifiMode = WIFI_MODE_AP;
		cfgSys.wifiSsid = CFG_getApModeSSID();
		cfgSys.wifiPass = CFG_getApModePass();
		cfgSys.ipLocal = IPAddress(192, 168, 0, 1);
		cfgSys.ipGateway = IPAddress(192, 168, 0, 1);
		cfgSys.ipSubnet = IPAddress(255, 255, 255, 0);

		// color for access point mode operation
		LED_setColorForStandby(LED_COL_MAGENTA);
	} else {
		cfgSys.wifiMode = WIFI_MODE_STA;
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

	// ----- Web Server task initialize -----
	} else if (false == SRV_initTask(CFG_getHostName())) {
		reboot();
	}
}

void loop() {
	M5.update();

	if (M5.BtnA.wasClicked()) {
		Serial.println("Buttun is clicked");
		RMPP_stopOutput();
	} else if (M5.BtnA.wasHold()) {
		Serial.println("Buttun is long pressed.");		
		CFG_toggleApMode();
	}

	vTaskDelay(50);
}
