// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#ifndef __BOARD_H__	/* ìÒèdíËã`ñhé~ */

#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif

#define PWM_FRQ 19000
#define PWM_RES 12

#ifdef ARDUINO_M5Stack_ATOM
// board : M5Stack ATOM Lite
#include <esp32/rom/gpio.h>
#define PIN_PWM1 19
#define PIN_PWM2 23
#define PIN_VIN 33

#define PIN_SW 39
#define SW_PRESS LOW
#define SW_RELEASE HIGH

#define PIN_FAULT 22
#define FAULT_DURING LOW
#define FAULT_CLEAR HIGH

#define PIN_LED 27
#else
#error "pin define is not found"
#endif

#endif /* __BOARD_H__*/	/* ìÒèdíËã`ñhé~ */
#define __BOARD_H__	/* ìÒèdíËã`ñhé~ */