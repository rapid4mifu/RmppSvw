// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#ifndef __TASK_LED_H__	/* 二重定義防止 */

#include <Arduino.h>

/* LEDタイプ */
typedef enum {
	LED_TYPE_1COLOR,
	LED_TYPE_RGB_SERIAL
} led_type_t;

/* LED表示色 */
typedef enum LED_COLOR {
	LED_COL_STNDBY,
	LED_COL_RED,
	LED_COL_GREEN,
	LED_COL_BLUE,
	LED_COL_YELLOW,
	LED_COL_MAGENTA,
	LED_COL_AQUA,
	LED_COL_WHITE
} led_color_t;

/* LED表示パターン */
typedef enum {
	LED_PT_OFF,			// 消灯
	LED_PT_ON,			// 点灯
	LED_PT_BLINK_ON10,	// 点滅・1秒周期 (ON:90％, OFF:10%)
	LED_PT_BLINK_ON90,	// 点滅・1秒周期 (ON:90％, OFF:10%)
	LED_PT_BLINK_FAST,	// 点滅・0.1秒周期
	LED_PT_BLINK_SLOW,	// 点滅・0.25秒周期
	LED_PT_SIZE,
} led_pattern_t;

bool LED_initTask(uint8_t pin, led_type_t type, led_pattern_t ptn = LED_PT_ON);
bool LED_addPin(uint8_t pin, led_type_t type, led_pattern_t ptn = LED_PT_ON);

void LED_setLightPattern(led_pattern_t ptn, uint8_t pin = 0);
void LED_setLightPatternOnce(led_pattern_t ptn, uint8_t cycle, uint8_t pin = 0);
void LED_setColorRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t pin = 0);
void LED_setColor(LED_COLOR color, uint8_t pin = 0);
void LED_setColorForStandbyRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t pin = 0);
void LED_setColorForStandby(LED_COLOR color, uint8_t pin = 0);

#endif /* __TASK_LED_H__*/	/* 二重定義防止 */
#define __TASK_LED_H__	/* 二重定義防止 */

