// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under MIT license.
// https://opensource.org/licenses/mit-license.php
// --------------------------------------------------------

#include "task_led.h"

#include "board.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif

#ifndef NUM_OF_LED_PINS
#define NUM_OF_LED_PINS 10
#endif

#ifndef PIN_LED_ON
#define PIN_LED_ON HIGH
#endif

#ifndef PIN_LED_OFF
#define PIN_LED_OFF LOW
#endif

#define LED_QUE_SEND_WAIT (10 / portTICK_PERIOD_MS)

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} led_rgb_t;

typedef struct {
	uint8_t pin;			// ピン番号
	led_type_t type;		// LED種類
	led_pattern_t ptn;		// 点灯パターン
	led_pattern_t ptn_once;	// 点灯パターン（一時）
	uint32_t phase;			// 制御フェーズ
	led_rgb_t col;			// 点灯色
	led_rgb_t col_stby;		// 点灯色（スタンパイ用途）
} led_control_t;

/* LED表示パターン */
typedef enum {
	LED_REQ_PTN,		// 点灯パターンを変更
	LED_REQ_PTN_ONCE,	// 点灯パターンを一時的に変更
	LED_REQ_COL,		// 点灯色を変更
	LED_REQ_COL_STBY	// 点灯色（スタンパイ用途）を変更
} led_req_t;

typedef struct {
	led_control_t led;
	led_req_t req;
} led_req_que_t;

led_control_t stLeds[NUM_OF_LED_PINS];

/* LED ON/OFF制御カウンタ 最大値 */
const uint32_t LED_BLINK_PHASE_MAX = (1 << 19);
/* LED ON/OFF制御カウンタ 最小値 */
const uint32_t LED_BLINK_PHASE_MIN = (1 << 0);

/* LED ON/OFF制御テーブル */
const uint32_t led_blink_table[LED_PT_SIZE] ={
	0b00000000000000000000000000000000,	// 消灯
	0b00000000000011111111111111111111,	// 点灯
	0b00000000000000000000000000000011, // 点滅・1秒周期 (ON:90％, OFF:10%)
	0b00000000000000111111111111111111, // 点滅・1秒周期 (ON:10％, OFF:90%)
	0b00000000000001010101010101010101,	// 点滅・0.1秒周期
	0b00000000000000000111110000011111,	// 点滅・0.25秒周期
};

/* led task handle */
TaskHandle_t hTaskLED;
/* led control queue handle */
QueueHandle_t xQueLED;

static void led_processTask(void* pvParameters);
static void led_processRequest(led_req_que_t * req);

static bool led_setupPin(led_control_t * p_led, uint8_t pin, led_type_t type, led_pattern_t ptn);
static led_control_t* led_getAssignedCtrlStruct(const uint8_t pin = 0);
static led_control_t* led_getCtrlStruct(const uint8_t pin = 0);

static void led_convertColorToRGB(led_rgb_t * rgb, LED_COLOR color);
static void led_turnOn(led_control_t * p_led);
static void led_turnOff(led_control_t * p_led);

/******************************************************************************
* Function Name: LED_initTask
* Description  : LEDに関する処理の初期化
* Arguments    : pin - pin number of default led, type - default led type
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool LED_initTask(uint8_t pin, led_type_t type, led_pattern_t ptn)
{
	for (uint8_t i = 0; i < NUM_OF_LED_PINS; i++) {
		stLeds[i].pin = 0;
		stLeds[i].ptn = LED_PT_OFF;
		stLeds[i].ptn_once = LED_PT_OFF;
	}

	if (false == led_setupPin(&stLeds[0], pin, type, ptn)) {
		Serial.println(" [failure] Failed to add LED");
		return false;
	}

	xQueLED = xQueueCreate(10, sizeof(led_req_que_t));	
	if (NULL == xQueLED) {
		Serial.println(" [failure] Failed to create LED queue.");
		return false;
	}
	
	Serial.println("LED task is now starting ...");
	BaseType_t taskCreated = xTaskCreateUniversal(led_processTask, "led_processTask", 2048, nullptr, tskIDLE_PRIORITY, &hTaskLED, APP_CPU_NUM);
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create LED task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: LED_addPin
* Description  : LEDを接続するピンの設定
* Arguments    : pin - led pin number, type - led type
* Return Value : true -> led pin add successed
******************************************************************************/
bool LED_addPin(uint8_t pin, led_type_t type, led_pattern_t ptn)
{
	led_control_t * pLed = led_getCtrlStruct();
	if (NULL == pLed) {
		return false;
	}

	return led_setupPin(pLed, pin, type, ptn);
}

/******************************************************************************
* Function Name: led_setupPin
* Description  : LEDを接続するピンの設定
* Arguments    : pin - led pin number, type - led type
* Return Value : true -> led pin add successed
******************************************************************************/
bool led_setupPin(led_control_t * pLed, uint8_t pin, led_type_t type, led_pattern_t ptn)
{
	if (NULL == pLed) {
		return false;
	}

	pLed->pin = pin;
	pLed->type = type;
	pLed->ptn = ptn;
	pLed->ptn_once = LED_PT_OFF;
	pLed->phase = LED_BLINK_PHASE_MAX;

	pLed->col.r = 0xFF;
	pLed->col.g = 0xFF;
	pLed->col.b = 0xFF;

	pLed->col_stby.r = 0xFF;
	pLed->col_stby.g = 0xFF;
	pLed->col_stby.b = 0xFF;

	switch (type) {
	case LED_TYPE_1COLOR:
		// ----- Single color LED initialize -----
		pinMode(pLed->pin, OUTPUT);
		digitalWrite(pLed->pin, PIN_LED_OFF);
		break;

	case LED_TYPE_RGB_SERIAL:
		// ----- Serial RGB LED initialize -----
		pinMode(pLed->pin, OUTPUT);
		break;

	default:
		Serial.printf("[warning] led type is not found, pin number is %d\n", pLed->pin);
		break;
	}

	return true;
}

/******************************************************************************
* Function Name: led_processTask
* Description  : LEDに関する処理
* Arguments    : none
* Return Value : none
******************************************************************************/
void led_processTask(void* pvParameters)
{
	led_req_que_t reqCtrl;
	led_control_t * p_led;
	led_pattern_t * p_led_ptn;
	const TickType_t xDelay = 50 / portTICK_PERIOD_MS;

	while (1) {
		if (pdPASS == xQueueReceive(xQueLED, &reqCtrl, 0)) {
			led_processRequest(&reqCtrl);
		}

		for (uint8_t i = 0; i < NUM_OF_LED_PINS; i++) {
			p_led = &stLeds[i];

			if (p_led->pin) {
				if (LED_PT_OFF != p_led->ptn_once) {
					p_led_ptn = &p_led->ptn_once;
				} else {
					p_led_ptn = &p_led->ptn;
				}

				// 表示テーブルから制御論理を取得
				if (led_blink_table[*p_led_ptn] & p_led->phase) {
					led_turnOn(p_led);
				} else {
					led_turnOff(p_led);
				}

				// LED制御カウンタを更新
				if (LED_BLINK_PHASE_MIN < p_led->phase) {
					p_led->phase = p_led->phase >> 1;
				} else {
					p_led->phase = LED_BLINK_PHASE_MAX;
					// 一時的な点灯パターンを終了
					p_led->ptn_once = LED_PT_OFF;
				}
			}
		}

		vTaskDelay(xDelay);
	}
}

/******************************************************************************
* Function Name: led_processTask
* Description  : LEDに関する処理
* Arguments    : none
* Return Value : none
******************************************************************************/
void led_processRequest(led_req_que_t * req)
{
	led_control_t * pLed = led_getAssignedCtrlStruct(req->led.pin);
	if (NULL == pLed) {
		return;
	}

	if (LED_REQ_PTN == req->req) {
		pLed->ptn = req->led.ptn;

	} else if (LED_REQ_PTN_ONCE == req->req) {
		pLed->ptn_once = req->led.ptn_once;
		pLed->phase = req->led.phase;

	} else if (LED_REQ_COL == req->req) {
		if (req->led.col.r || req->led.col.g || req->led.col.b) {
			pLed->col.r = req->led.col.r;
			pLed->col.g = req->led.col.g;
			pLed->col.b = req->led.col.b;
		} else {
			// スタンバイ用途の点灯色へ変更
			pLed->col.r = pLed->col_stby.r;
			pLed->col.g = pLed->col_stby.g;
			pLed->col.b = pLed->col_stby.b;
		}

	} else if (LED_REQ_COL_STBY == req->req) {
		pLed->col_stby.r = req->led.col_stby.r;
		pLed->col_stby.g = req->led.col_stby.g;
		pLed->col_stby.b = req->led.col_stby.b;
	}
}

/******************************************************************************
* Function Name: LED_setLightPattern
* Description  : LEDの点灯パターンを設定
* Arguments    : none
* Return Value : none
******************************************************************************/
void LED_setLightPattern(led_pattern_t ptn, uint8_t pin)
{
	led_req_que_t req;
	req.req = LED_REQ_PTN;
	req.led.pin = pin;
	req.led.ptn = ptn;

	if (pdTRUE != xQueueSend(xQueLED, &req, LED_QUE_SEND_WAIT)) {
		Serial.println("[ERROR] led queue send is fail.");
	}
}

/******************************************************************************
* Function Name: LED_setLightPatternOnce
* Description  : LEDの一時的な点灯パターンを設定
* Arguments    : cycle -> control cycle of led
* Return Value : none
******************************************************************************/
void LED_setLightPatternOnce(led_pattern_t ptn, uint8_t cycle, uint8_t pin)
{
	led_req_que_t req;
	req.req = LED_REQ_PTN_ONCE;
	req.led.pin = pin;
	req.led.ptn_once = ptn;
	req.led.phase = (1 << cycle);

	if (pdTRUE != xQueueSend(xQueLED, &req, LED_QUE_SEND_WAIT)) {
		Serial.println("[ERROR] led queue send is fail.");
	}
}

/******************************************************************************
* Function Name: LED_setColorRGB
* Description  : LED点灯色を設定
* Arguments    : r - red, g - green, b - blule
* Return Value : none
******************************************************************************/
void LED_setColorRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t pin)
{
	led_req_que_t req;
	
	req.req = LED_REQ_COL;
	req.led.pin = pin;
	req.led.col.r = r;
	req.led.col.g = g;
	req.led.col.b = b;

	if (pdTRUE != xQueueSend(xQueLED, &req, LED_QUE_SEND_WAIT)) {
		Serial.println("[ERROR] led queue send is fail.");
	}
}

/******************************************************************************
* Function Name: LED_setColor
* Description  : LED点灯色を設定
* Arguments    : color - LED_COLOR enum
* Return Value : none
******************************************************************************/
void LED_setColor(LED_COLOR color, uint8_t pin)
{
	led_rgb_t rgb;
	led_convertColorToRGB(&rgb, color);
	
	LED_setColorRGB(rgb.r, rgb.g, rgb.b, pin);
}

/******************************************************************************
* Function Name: LED_setColorForStandbyRGB
* Description  : LED点灯色を設定（スタンバイ用途）
* Arguments    : r - red, g - green, b - blule
* Return Value : none
******************************************************************************/
void LED_setColorForStandbyRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t pin)
{
	led_req_que_t req;

	req.req = LED_REQ_COL_STBY;
	req.led.pin = pin;
	req.led.col_stby.r = r;
	req.led.col_stby.g = g;
	req.led.col_stby.b = b;

	if (pdTRUE != xQueueSend(xQueLED, &req, LED_QUE_SEND_WAIT)) {
		Serial.println("[ERROR] led queue send is fail.");
	}
}

/******************************************************************************
* Function Name: LED_setColorForStandby
* Description  : LED点灯色を設定（スタンバイ時）
* Arguments    : color - LED_COLOR enum
* Return Value : none
******************************************************************************/
void LED_setColorForStandby(LED_COLOR color, uint8_t pin)
{
	led_rgb_t rgb;
	led_convertColorToRGB(&rgb, color);

	LED_setColorForStandbyRGB(rgb.r, rgb.g, rgb.b, pin);
}

/******************************************************************************
* Function Name: led_convertColorToRGB
* Description  : LEDの色をRGB値に変換
* Arguments    : color - LED_COLOR enum
* Return Value : none
******************************************************************************/
void led_convertColorToRGB(led_rgb_t * rgb, LED_COLOR color)
{
	switch (color) {
	case LED_COL_STNDBY:
		rgb->r = 0;
		rgb->g = 0;
		rgb->b = 0;
		break;
	case LED_COL_RED:
		rgb->r = 255;
		rgb->g = 0;
		rgb->b = 0;
		break;
	case LED_COL_GREEN:
		rgb->r = 0;
		rgb->g = 255;
		rgb->b = 0;
		break;
	case LED_COL_BLUE:
		rgb->r = 0;
		rgb->g = 0;
		rgb->b = 255;
		break;
	case LED_COL_YELLOW:
		rgb->r = 255;
		rgb->g = 255;
		rgb->b = 0;
		break;
	case LED_COL_MAGENTA:
		rgb->r = 255;
		rgb->g = 0;
		rgb->b = 255;
		break;
	case LED_COL_AQUA:
		rgb->r = 0;
		rgb->g = 255;
		rgb->b = 255;
		break;
	case LED_COL_WHITE:
		rgb->r = 255;
		rgb->g = 255;
		rgb->b = 255;
		break;
	default:
		break;
	}
}

/******************************************************************************
* Function Name: led_getAssignedCtrlStruct
* Description  : 割当済みのLED制御構造体を取得
* Arguments    : pin - pin number (0 -> default led)
* Return Value : NULL -> Unassigned struct (don't use)
******************************************************************************/
led_control_t* led_getAssignedCtrlStruct(const uint8_t pin)
{
	if (0 == pin) {
		// default led
		return &stLeds[0];
	}

	return led_getCtrlStruct(pin);
}

/******************************************************************************
* Function Name: led_getCtrlStruct
* Description  : LED制御構造体を取得
* Arguments    : pin - pin number (0 -> empty struct)
* Return Value : NULL -> Unassigned struct (don't use)
******************************************************************************/
led_control_t* led_getCtrlStruct(const uint8_t pin)
{
	led_control_t* pLed = NULL;

	for (uint8_t i = 0; i < NUM_OF_LED_PINS; i++) {
		if (pin == stLeds[i].pin) {
			pLed = &stLeds[i];
			break;
		}
	}

	return pLed;
}

/******************************************************************************
* Function Name: led_turnOn
* Description  : LEDを点灯する
* Arguments    : p_led - pointer of led_control_t
* Return Value : none
******************************************************************************/
void led_turnOn(led_control_t * p_led)
{
	if (NULL == p_led) {
		return;
	}

	switch (p_led->type) {
	case LED_TYPE_1COLOR:
		digitalWrite(p_led->pin, PIN_LED_ON);
		break;

	case LED_TYPE_RGB_SERIAL:
		neopixelWrite(p_led->pin, p_led->col.r, p_led->col.g, p_led->col.b);
		break;

	}
}

/******************************************************************************
* Function Name: led_turnOff
* Description  : LEDを消灯する
* Arguments    : p_led - pointer of led_control_t
* Return Value : none
******************************************************************************/
void led_turnOff(led_control_t * p_led)
{
	if (NULL == p_led) {
		return;
	}

	switch (p_led->type) {
	case LED_TYPE_1COLOR:
		digitalWrite(p_led->pin, PIN_LED_OFF);
		break;

	case LED_TYPE_RGB_SERIAL:
		neopixelWrite(p_led->pin, 0, 0, 0);
		break;
	}
}
