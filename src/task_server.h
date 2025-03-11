// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#ifndef __TASK_SERVER_H__	/* ìÒèdíËã`ñhé~ */

#include <Arduino.h>

#define WS_LEN_BINARY_MAX 64

typedef void (*CallbackOnWsConnect)(uint32_t, size_t);
typedef void (*CallbackOnWsDisconnect)(uint32_t, size_t);
typedef void (*CallbackOnSocketBinary)(uint8_t *, size_t, uint32_t);
typedef void (*CallbackOnSocketText)(String, uint32_t);

bool SRV_initTask(String hostName = "");

void SRV_pushWsBinaryToQueue(uint8_t * data, uint8_t len, uint32_t id = 0);
void SRV_pushWsTextToQueue(String data, uint32_t id = 0);

void SRV_attachWsConnectListener(CallbackOnWsConnect callback);
void SRV_attachWsDisconnectListener(CallbackOnWsDisconnect callback);
void SRV_attachWsBinaryListener(CallbackOnSocketBinary callback);
void SRV_attachWsTextListener(CallbackOnSocketText callback);

#endif /* __TASK_SERVER_H__*/	/* ìÒèdíËã`ñhé~ */
#define __TASK_SERVER_H__	/* ìÒèdíËã`ñhé~ */

