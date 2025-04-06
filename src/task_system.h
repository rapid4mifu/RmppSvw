// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#ifndef __TASK_SYSTEM_H__	/* ìÒèdíËã`ñhé~ */

#include <Arduino.h>
#include <WiFi.h>

#if defined(ESP32)
#define SYS_WIFI_EVENT_PARAM WiFiEvent_t
#else
#define SYS_WIFI_EVENT_PARAM wl_status_t
#endif
typedef void (*CallbackWifiEvent)(SYS_WIFI_EVENT_PARAM);

typedef struct {
	WiFiMode_t wifiMode;
	const char * wifiSsid;
	const char * wifiPass;
	IPAddress ipLocal;
	IPAddress ipGateway;
	IPAddress ipSubnet;
} system_config_t;

bool SYS_initTask(system_config_t* cfg = nullptr);

void SYS_attachWiFiEventListener(CallbackWifiEvent callback);
bool SYS_isWiFiAvailable(void);

#endif /* __TASK_SYSTEM_H__*/	/* ìÒèdíËã`ñhé~ */
#define __TASK_SYSTEM_H__	/* ìÒèdíËã`ñhé~ */

