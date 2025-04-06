// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include "task_led.h"

#include "board.h"

#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#endif

#if defined(ESP32)
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
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

#if defined(FastLED)
CRGB leds[NUM_OF_LED_PINS];
#endif

/* �T���v�����O���� [ms]*/
#define LED_CONTROL_PERIOD 50

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} led_rgb_t;

typedef struct {
	uint8_t index;
	uint8_t pin;			// �s���ԍ�
	led_type_t type;		// LED���
	led_pattern_t ptn;		// �_���p�^�[��
	led_pattern_t ptn_once;	// �_���p�^�[���i�ꎞ�j
	uint32_t phase;			// ����t�F�[�Y
	led_rgb_t col;			// �_���F
	led_rgb_t col_stby;		// �_���F�i�X�^���p�C�p�r�j
} led_control_t;

/* LED�\���p�^�[�� */
typedef enum {
	LED_REQ_PTN,		// �_���p�^�[����ύX
	LED_REQ_PTN_ONCE,	// �_���p�^�[�����ꎞ�I�ɕύX
	LED_REQ_COL,		// �_���F��ύX
	LED_REQ_COL_STBY	// �_���F�i�X�^���p�C�p�r�j��ύX
} led_req_t;

typedef struct {
	led_control_t led;
	led_req_t req;
} led_req_que_t;

led_control_t stLeds[NUM_OF_LED_PINS];

/* LED ON/OFF����J�E���^ �ő�l */
const uint32_t LED_BLINK_PHASE_MAX = (1 << 19);
/* LED ON/OFF����J�E���^ �ŏ��l */
const uint32_t LED_BLINK_PHASE_MIN = (1 << 0);

/* LED ON/OFF����e�[�u�� */
const uint32_t led_blink_table[LED_PT_SIZE] ={
	0b00000000000000000000000000000000,	// ����
	0b00000000000011111111111111111111,	// �_��
	0b00000000000000000000000000000011, // �_�ŁE1�b���� (ON:90��, OFF:10%)
	0b00000000000000111111111111111111, // �_�ŁE1�b���� (ON:10��, OFF:90%)
	0b00000000000001010101010101010101,	// �_�ŁE0.1�b����
	0b00000000000000000111110000011111,	// �_�ŁE0.25�b����
};

/* process task handle */
static TaskHandle_t hTaskLED = NULL;
/* control queue handle */
static QueueHandle_t xQueLED = NULL;

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
* Description  : LED�Ɋւ��鏈���̏�����
* Arguments    : pin - pin number of default led, type - default led type
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool LED_initTask(uint8_t pin, led_type_t type, led_pattern_t ptn)
{
	for (uint8_t i = 0; i < NUM_OF_LED_PINS; i++) {
		stLeds[i].index = i;
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
#if defined(ESP32)
	BaseType_t taskCreated = xTaskCreateUniversal(led_processTask, "led_task", 2048, nullptr, tskIDLE_PRIORITY, &hTaskLED, APP_CPU_NUM);
#else
	BaseType_t taskCreated = xTaskCreate(led_processTask, "led_task", configMINIMAL_STACK_SIZE * 4, nullptr, tskIDLE_PRIORITY, &hTaskLED);
#if defined(PICO_CYW43_SUPPORTED)
	// The PicoW WiFi chip controls the LED, and only core 0 can make calls to it safely
	vTaskCoreAffinitySet(hTaskLED, 1 << 0);
#endif
#endif
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create LED task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: LED_addPin
* Description  : LED��ڑ�����s���̐ݒ�
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
* Description  : LED��ڑ�����s���̐ݒ�
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
#if defined(ESP32)
		pinMode(pLed->pin, OUTPUT);
#elif defined(FastLED)
		FastLED.addLeds<NEOPIXEL, pLed->pin>(leds[pLed->index], 1);
#else
		Serial.println("[warning] This platform does not support Serial RGB LED.");
#endif
		break;

	default:
		Serial.printf("[warning] led type is not found, pin number is %d\n", pLed->pin);
		break;
	}

	return true;
}

/******************************************************************************
* Function Name: led_processTask
* Description  : LED�Ɋւ��鏈��
* Arguments    : none
* Return Value : none
******************************************************************************/
void led_processTask(void* pvParameters)
{
	led_req_que_t reqCtrl;
	led_control_t * p_led;
	led_pattern_t * p_led_ptn;

	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

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

				// �\���e�[�u�����琧��_�����擾
				if (led_blink_table[*p_led_ptn] & p_led->phase) {
					led_turnOn(p_led);
				} else {
					led_turnOff(p_led);
				}

				// LED����J�E���^���X�V
				if (LED_BLINK_PHASE_MIN < p_led->phase) {
					p_led->phase = p_led->phase >> 1;
				} else {
					p_led->phase = LED_BLINK_PHASE_MAX;
					// �ꎞ�I�ȓ_���p�^�[�����I��
					p_led->ptn_once = LED_PT_OFF;
				}
			}
		}

		vTaskDelayUntil(&xLastWakeTime, (LED_CONTROL_PERIOD / portTICK_PERIOD_MS));
	}
}

/******************************************************************************
* Function Name: led_processTask
* Description  : LED�Ɋւ��鏈��
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
			// �X�^���o�C�p�r�̓_���F�֕ύX
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
* Description  : LED�̓_���p�^�[����ݒ�
* Arguments    : none
* Return Value : none
******************************************************************************/
void LED_setLightPattern(led_pattern_t ptn, uint8_t pin)
{
	if (NULL == xQueLED) {
		return;
	}

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
* Description  : LED�̈ꎞ�I�ȓ_���p�^�[����ݒ�
* Arguments    : cycle -> control cycle of led
* Return Value : none
******************************************************************************/
void LED_setLightPatternOnce(led_pattern_t ptn, uint8_t cycle, uint8_t pin)
{
	if (NULL == xQueLED) {
		return;
	}

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
* Description  : LED�_���F��ݒ�
* Arguments    : r - red, g - green, b - blule
* Return Value : none
******************************************************************************/
void LED_setColorRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t pin)
{
	if (NULL == xQueLED) {
		return;
	}

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
* Description  : LED�_���F��ݒ�
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
* Description  : LED�_���F��ݒ�i�X�^���o�C�p�r�j
* Arguments    : r - red, g - green, b - blule
* Return Value : none
******************************************************************************/
void LED_setColorForStandbyRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t pin)
{
	if (NULL == xQueLED) {
		return;
	}
	
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
* Description  : LED�_���F��ݒ�i�X�^���o�C���j
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
* Description  : LED�̐F��RGB�l�ɕϊ�
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
* Description  : �����ς݂�LED����\���̂��擾
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
* Description  : LED����\���̂��擾
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
* Description  : LED��_������
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
#if defined(ESP32)
		rgbLedWrite(p_led->pin, p_led->col.r, p_led->col.g, p_led->col.b);
#elif defined(FastLED)
		leds[p_led->index] = CRGB::Black;
		FastLED.show();			
#endif
		break;

	}
}

/******************************************************************************
* Function Name: led_turnOff
* Description  : LED����������
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
#if defined(ESP32)
		rgbLedWrite(p_led->pin, 0, 0, 0);
#elif defined(FastLED)
		leds[p_led->index] = CRGB::Black;
		FastLED.show();			
#endif
		break;
	}
}
