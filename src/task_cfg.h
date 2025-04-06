// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#ifndef __TASK_CFG_H__	/* ��d��`�h�~ */

#include <Arduino.h>

typedef void (*CallbackOnChangeSuccess)(void);

bool CFG_initTask(void);

void CFG_attachChangeSuccessListener(CallbackOnChangeSuccess callback);

void CFG_resetSavedData(void);
void CFG_printSavedData(void);

void CFG_setApMode(bool enable);
void CFG_toggleApMode(void);
bool CFG_isApModeEnabled(void);
const char * CFG_getHostName(void);
const char * CFG_getApModeSSID(void);
const char * CFG_getApModePass(void);
const char * CFG_getStaModeSSID(void);
const char * CFG_getStaModePass(void);
IPAddress CFG_getLocalAddress(void);
IPAddress CFG_getDefaultGateway(void);
IPAddress CFG_getSubnetMask(void);

#endif /* __TASK_CFG_H__*/	/* ��d��`�h�~ */
#define __TASK_CFG_H__	/* ��d��`�h�~ */

