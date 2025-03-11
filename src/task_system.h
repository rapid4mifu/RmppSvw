// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#ifndef __TASK_SYSTEM_H__	/* “ñd’è‹`–h~ */

#include <Arduino.h>
#include <WiFi.h>

typedef void (*CallbackWifiEvent)(WiFiEvent_t);

typedef struct {
	wifi_mode_t wifiMode;
	const char * wifiSsid;
	const char * wifiPass;
	IPAddress ipLocal;
	IPAddress ipGateway;
	IPAddress ipSubnet;
} system_config_t;

bool SYS_initTask(system_config_t* cfg = nullptr);

void SYS_attachWiFiEventListener(CallbackWifiEvent callback);

#endif /* __TASK_SYSTEM_H__*/	/* “ñd’è‹`–h~ */
#define __TASK_SYSTEM_H__	/* “ñd’è‹`–h~ */

