#include "task_input.h"

#include "board.h"
#include "task_cfg.h"
#include "task_rmpp.h"

#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
#include <FreeRTOS.h>
#include <task.h>
#endif

#if defined(ESP32)
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
#endif

#define INPUT_TASK_TIMER 1

/* �Z�������莞�� [ms]*/
#define INPUT_PRESS_TIME_MIN 50
/* ���������莞�� [ms]*/
#define INPUT_HOLD_TIME_MIN 1000
/* �T���v�����O���� [ms]*/
#define INPUT_SAMPLE_PERIOD 25

/* SW�Z��������J�E���^ */
#define INPUT_PRESS_COUNT ((INPUT_PRESS_TIME_MIN / INPUT_SAMPLE_PERIOD) / portTICK_PERIOD_MS)
/* SW����������J�E���^ */
#define INPUT_HOLD_COUNT ((INPUT_HOLD_TIME_MIN / INPUT_SAMPLE_PERIOD) / portTICK_PERIOD_MS)

/* SW����������J�E���^ */
#define INPUT_ACTIVE_LOW 0
/* SW����������J�E���^ */
#define INPUT_ACTIVE_HIGH 1

typedef enum {
	INPUT_ST_NULL = 0,		// ����`
	INPUT_ST_OFF,			// OFF
	INPUT_ST_ON,			// ON
	INPUT_ST_PRESS,			// ON�i�Z�������j
	INPUT_ST_HOLD,			// ON�i���������j
	INPUT_ST_FALL_PRESS,	// �Z�����m��
	INPUT_ST_RISE_HOLD,		// �������m��
	INPUT_ST_RISE_ON,		// OFF����ON�֕ω�
	INPUT_ST_FALL_OFF		// ON����OFF�֕ω�
} input_status_t;

typedef	struct {
	uint8_t		previousLevel;	// �O�񃌃x��
	uint8_t		activeLevel;	// �A�N�e�B�u���x��
	uint8_t		pinNumber;		// �s���ԍ�
	uint8_t		cnt;	// SW�ǂݎ��J�E���^
	input_status_t	state;	// SW�X�e�[�^�X
} input_signal_t;

/* �����{�^���X�C�b�`1 */
static input_signal_t btnMain;
/* �f�B�b�v�X�C�b�`1 */
//input_signal_t dsw1[4];

/* process task handle */
static TaskHandle_t hTaskBtn;

static void inp_processTask(void* pvParameters);

static void inp_initSignal(input_signal_t* pInput, uint8_t activeLevel, uint8_t pin, uint8_t mode = INPUT);
static void inp_judgeButton(input_signal_t* pInput, uint8_t currentLevel);
static void inp_judgeButton(input_signal_t* pInput);
static void inp_judgeSwitch(input_signal_t* pInput, uint8_t currentLevel);
static void inp_judgeSwitch(input_signal_t* pInput);

static uint8_t inp_wasButtonPress(input_signal_t* pInput);
static uint8_t inp_isButtonPress(input_signal_t* pInput);
static uint8_t inp_wasButtonHold(input_signal_t* pInput);
static uint8_t inp_isButtonHold(input_signal_t* pInput);
static uint8_t inp_isSwitchOn(input_signal_t* pInput);
static uint8_t inp_isSwitchOff(input_signal_t* pInput);

/*****************************************************************************
* Function Name: INP_initTask
* Description  : ���͂Ɋւ��鏈���̏�����
* Arguments    : none
* Return Value : none
******************************************************************************/
bool INP_initTask(void)
{
#if 0
	// DIP�X�C�b�`
	inp_initSignal(dsw1[0], INPUT_ACTIVE_LOW);
	inp_initSignal(dsw1[1], INPUT_ACTIVE_LOW);
	inp_initSignal(dsw1[2], INPUT_ACTIVE_LOW);
	inp_initSignal(dsw1[3], INPUT_ACTIVE_LOW);
#endif

	// �X�C�b�`
#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
	inp_initSignal(&btnMain, INPUT_ACTIVE_HIGH, 0);
#else
	inp_initSignal(&btnMain, INPUT_ACTIVE_LOW, PIN_SW);
#endif

	Serial.println("Button task is now starting ...");
#if defined(ESP32)
	BaseType_t taskCreated = xTaskCreateUniversal(inp_processTask, "btn_task", 2048 , nullptr, 2, &hTaskBtn, APP_CPU_NUM);
#else
	BaseType_t taskCreated = xTaskCreate(inp_processTask, "btn_task", configMINIMAL_STACK_SIZE * 2, nullptr, 2, &hTaskBtn);
#endif
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create button task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/*****************************************************************************
* Function Name: inp_processTask
* Description  : ���͂Ɋւ��鏈��
* Arguments    : none
* Return Value : none
******************************************************************************/
void inp_processTask(void* pvParameters)
{
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

	while (1) {
		// DIP�X�C�b�`�ibit0-3�j�ǂݎ��
		//inp_judgeSwitch(&dsw1[0]);
		//inp_judgeSwitch(&dsw1[1]);
		//inp_judgeSwitch(&dsw1[2]);
		//inp_judgeSwitch(&dsw1[3]);

		// ���]�X�C�b�` �ǂݎ��
#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
		uint8_t tmp = BOOTSEL;
		inp_judgeButton(&btnMain, tmp);
#else
		inp_judgeButton(&btnMain);
#endif
		if (inp_wasButtonPress(&btnMain)) {
			//Serial.println("Button is clicked.");
			RMPP_stopOutput(true);
		} else if (inp_wasButtonHold(&btnMain)) {
			//Serial.println("Button is holding.");
			CFG_toggleApMode();
		}

		vTaskDelayUntil(&xLastWakeTime, (INPUT_SAMPLE_PERIOD / portTICK_PERIOD_MS));
	}
}

/*****************************************************************************
* Function Name: inp_initSignal
* Description  : ���͐M���̏�����
* Arguments    : pInput = ��ԕێ��\����, activeLevel = �A�N�e�B�u���x��
                 pin = �s���ԍ�, mode = �s���̐ݒ�
* Return Value : none
******************************************************************************/
void inp_initSignal(input_signal_t* pInput, uint8_t activeLevel, uint8_t pin, uint8_t mode)
{
	if (pin) {
		pinMode(pin, mode);
	}

	pInput->cnt = 0;
	pInput->pinNumber = pin;
	pInput->state = INPUT_ST_NULL;
	pInput->activeLevel = activeLevel;
	pInput->previousLevel = activeLevel ? 0 : 1;
}

/*****************************************************************************
* Function Name: inp_wasButtonPress
* Description  : �{�^���̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
uint8_t inp_wasButtonPress(input_signal_t* pInput)
{
	return INPUT_ST_FALL_PRESS == pInput->state;
}

/*****************************************************************************
* Function Name: inp_isButtonPress
* Description  : �{�^���̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
uint8_t inp_isButtonPress(input_signal_t* pInput)
{
	return INPUT_ST_FALL_PRESS == pInput->state || INPUT_ST_PRESS == pInput->state;
}

/*****************************************************************************
* Function Name: inp_wasButtonHold
* Description  : �{�^���̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
uint8_t inp_wasButtonHold(input_signal_t* pInput)
{
	return INPUT_ST_RISE_HOLD == pInput->state;
}

/*****************************************************************************
* Function Name: inp_isButtonHold
* Description  : �{�^���̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
uint8_t inp_isButtonHold(input_signal_t* pInput)
{
	return INPUT_ST_RISE_HOLD == pInput->state || INPUT_ST_HOLD == pInput->state;
}

/*****************************************************************************
* Function Name: inp_isSwitchOn
* Description  : �X�C�b�`�̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
uint8_t inp_isSwitchOn(input_signal_t* pInput)
{
	return INPUT_ST_ON == pInput->state;
}

/*****************************************************************************
* Function Name: inp_isSwitchOff
* Description  : �X�C�b�`�̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
uint8_t inp_isSwitchOff(input_signal_t* pInput)
{
	return INPUT_ST_OFF == pInput->state;
}

/*****************************************************************************
* Function Name: inp_judgeButton
* Description  : �^�N�g�X�C�b�`�̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
void inp_judgeButton(input_signal_t* pInput)
{
	uint8_t currentLevel = digitalRead(pInput->pinNumber);
	inp_judgeButton(pInput, currentLevel);
}

/*****************************************************************************
* Function Name: inp_judgeButton
* Description  : �^�N�g�X�C�b�`�̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����, currentLevel : ���݂̐M�����x��
* Return Value : none
******************************************************************************/
void inp_judgeButton(input_signal_t* pInput, uint8_t currentLevel)
{
	if (currentLevel == pInput->previousLevel) {
		pInput->cnt++;
		// �X�C�b�`��Ԃ̊m�蔻��
		if (INPUT_PRESS_COUNT <= pInput->cnt) {
			if (pInput->activeLevel == currentLevel) {
				// SW��ON�̎�
				if (INPUT_HOLD_COUNT <= pInput->cnt) {
					if (pInput->state == INPUT_ST_PRESS) {
						// �J�E���^��臒l�𒴂�����
						// �i�������m��j
						pInput->state = INPUT_ST_RISE_HOLD;
					} else {
						pInput->state = INPUT_ST_HOLD;
					}
				} else {
					// �Z������
					pInput->state = INPUT_ST_PRESS;
				}
			} else {
				// SW��OFF�̎�
				if (INPUT_ST_PRESS == pInput->state) {
					// ON����OFF�ɕς������
					// �i�Z�����̃����[�X�m��j
					pInput->state = INPUT_ST_FALL_PRESS;
				} else {
					pInput->state = INPUT_ST_OFF;
					pInput->cnt = 0;
				}
			}
		}
	} else {
		pInput->cnt = 0;
	}
	pInput->previousLevel = currentLevel;
}

/*****************************************************************************
* Function Name: inp_judgeSwitch
* Description  : DIP�X�C�b�`�̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����
* Return Value : none
******************************************************************************/
void inp_judgeSwitch(input_signal_t* pInput)
{
	uint8_t currentLevel = digitalRead(pInput->pinNumber);
	inp_judgeSwitch(pInput, currentLevel);
}

/*****************************************************************************
* Function Name: inp_judgeSwitch
* Description  : DIP�X�C�b�`�̏�Ԕ���
* Arguments    : pInput = ��ԕێ��\����, currentLevel : ���݂̐M�����x��
* Return Value : none
******************************************************************************/
void inp_judgeSwitch(input_signal_t* pInput, uint8_t currentLevel)
{
	if (currentLevel != pInput->previousLevel) {
		pInput->cnt++;
		if (INPUT_PRESS_COUNT <= pInput->cnt) {
			pInput->previousLevel = currentLevel;
			pInput->cnt = 0;

			if (pInput->activeLevel == currentLevel) {
				pInput->state = INPUT_ST_RISE_ON;
			} else {
				pInput->state = INPUT_ST_FALL_OFF;
			}
		}
	} else {
		pInput->cnt = 0;

		if (pInput->activeLevel == pInput->previousLevel) {
			pInput->state = INPUT_ST_ON;
		} else {
			pInput->state = INPUT_ST_OFF;
		}
	}
}
