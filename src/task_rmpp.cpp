// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

// This software module is part of the Railway Model Power Pack (RMPP)

#include "task_rmpp.h"
#include "task_system.h"
#include "task_server.h"
#include "task_cfg.h"
#include "task_led.h"

#include "board.h"
#include "rmpp_cmd.h"

#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif

#define RMPP_STATUS_INTERVAL 200 // [ms]
#define RMPP_ALIVE_TIMEOUT 3000 // [ms]
#define RMPP_SERIAL_DEBUG_INTERVAL 10000 // [ms]

#define PWM_DUTY_100 (1 << PWM_RES)
#define PWM_DUTY_MAX (PWM_DUTY_100 - 1)

typedef enum {
	RMPP_MODE_INIT = 0,	/* 初期化 */
	RMPP_MODE_OFF,		/* 出力オフ */
	RMPP_MODE_ON,		/* 出力オン */
	RMPP_MODE_FAULT,	/* 障害要因発生 */
	RMPP_MODE_FAIL		/* 故障要因発生 */
} rmpp_mode_t;

typedef struct {
	uint8_t mode:4;
	uint8_t fwd:1;
	uint8_t rvs:1;
	uint8_t rsv6:1;
	uint8_t rsv7:1;
} output_flag_t;

typedef struct {
	uint8_t ext_ctrl:1;
	uint8_t rsv1:1;
	uint8_t rsv2:1;
	uint8_t rsv3:1;
	uint8_t OverCurrent:1;
	uint8_t rsv5:1;
	uint8_t rsv6:1;
	uint8_t rsv7:1;
} status_flag_t;

typedef struct {
	union {
		uint8_t ui8;
		output_flag_t bit;
	} output;
	union {
		uint8_t ui8;
		status_flag_t bit;
	} status;
	uint16_t volt_in;		/* Input voltage */
	uint16_t duty_set;		/* Output duty */
	int8_t temp_cpu;		/* CPU temperture */
} rmpp_info_t;

rmpp_info_t stRmpp;

TaskHandle_t hTaskRmpp;
TickType_t tickAlive;
size_t connectClients;

uint8_t rx_data[RMPP_PACKET_LEN_MAX];

static void rmpp_processTask(void* pvParameters);

static void rmpp_handleWsBinaryData(uint8_t * data, size_t len, uint32_t id);
static void rmpp_handleWsClientChange(uint32_t id, size_t clientCount);
static void rmpp_handleWiFiEvent(WiFiEvent_t event);
static void rmpp_handleCfgChangeSuccess(void);

static void rmpp_parseOutputCommand(uint8_t * data, uint16_t len);
static void rmpp_stopOutputOnFault(void);
static void rmpp_turnOutputOff(void);
static void rmpp_clearFault(void);

/******************************************************************************
* Function Name: RMPP_initTask
* Description  : パワーパックに関する処理の初期化
* Arguments    : none
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool RMPP_initTask(void)
{
	stRmpp.output.bit.mode = RMPP_MODE_INIT;
	connectClients = 0;

	ledcAttach(PIN_PWM1, PWM_FRQ, PWM_RES);
	ledcAttach(PIN_PWM2, PWM_FRQ, PWM_RES);
	ledcWrite(PIN_PWM1, 0);
	ledcWrite(PIN_PWM2, 0);

	pinMode(PIN_VIN, INPUT);
	pinMode(PIN_FAULT, INPUT);

	SRV_attachWsBinaryListener(rmpp_handleWsBinaryData);
	SRV_attachWsTextListener(CFG_processCommand);
	SRV_attachWsConnectListener(rmpp_handleWsClientChange);
	SRV_attachWsDisconnectListener(rmpp_handleWsClientChange);
	SYS_attachWiFiEventListener(rmpp_handleWiFiEvent);
	CFG_attachChangeSuccessListener(rmpp_handleCfgChangeSuccess);

	Serial.println("RMPP (Railway Model Power Pack) task is now starting ...");
	BaseType_t taskCreated = xTaskCreateUniversal(rmpp_processTask, "rmpp_processTask", 2048 , nullptr, 2, &hTaskRmpp, APP_CPU_NUM);
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create RMPP task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: rmpp_processTask
* Description  : パワーパックに関する処理
* Arguments    : none
* Return Value : none
******************************************************************************/
void rmpp_processTask(void* pvParameters)
{
	TickType_t tickQueueData = xTaskGetTickCount();
	TickType_t tickDebug = xTaskGetTickCount();
	uint8_t cmdToClient[RMPP_CMD_LEN_RD_STATUS];

	stRmpp.output.bit.mode = RMPP_MODE_OFF;

	while(true) {
		// fault signal monitoring
		if (FAULT_DURING == GPIO_INPUT_GET(PIN_FAULT)) {
			if (0 == stRmpp.status.bit.OverCurrent) {
				rmpp_stopOutputOnFault();
			}
		}

		// Alive monitoring
		if (RMPP_ALIVE_TIMEOUT < (xTaskGetTickCount() - tickAlive)) {
			// Turn off the output when the alive monitoring timeout
			if (RMPP_MODE_ON == stRmpp.output.bit.mode) {
				RMPP_stopOutput();
				Serial.println("alive monitoring timeout");
			}
		}

		// send power pack status to the client (web browser)
		if (RMPP_STATUS_INTERVAL < (xTaskGetTickCount() - tickQueueData)) {
			tickQueueData = xTaskGetTickCount();
			
			// input voltage (in units of 0.1V)
			stRmpp.volt_in = analogRead(PIN_VIN) * 360 / 4095;

			// cpu temperature (in units of 1deg)
			stRmpp.temp_cpu = (int8_t)temperatureRead();

			// command binary data 
			cmdToClient[0] = RMPP_CMDID_RD_STATUS;
			cmdToClient[1] = stRmpp.output.ui8;
			cmdToClient[2] = stRmpp.status.ui8;
			if (255 > stRmpp.volt_in) {
				cmdToClient[3] = stRmpp.volt_in;
			} else {
				cmdToClient[3] = 0xFF;
			}
			cmdToClient[4] = stRmpp.temp_cpu + 128;

			SRV_pushWsBinaryToQueue(&cmdToClient[0], RMPP_CMD_LEN_RD_STATUS);
		}

#ifdef RMPP_SERIAL_DEBUG_INTERVAL
		if (RMPP_SERIAL_DEBUG_INTERVAL < (xTaskGetTickCount() - tickDebug)) {
			tickDebug = xTaskGetTickCount();
			
			float vin = (float)analogRead(PIN_VIN) * 36.0 / 4095.0;
			Serial.printf("Input Voltage   : %.2f V\n", vin);
			Serial.printf("Output Duty     : %d\n", stRmpp.duty_set);
			Serial.printf("CPU Temperature : %.2f deg\n", temperatureRead());
		}
#endif

		vTaskDelay(1);
	}
}

/******************************************************************************
* Function Name: rmpp_handleWsBinaryData
* Description  : WebSocketのバイナリデータを受信したときのコールバック関数
* Arguments    : data - received data, len - received length
* Return Value : none
******************************************************************************/
void rmpp_handleWsBinaryData(uint8_t * data, size_t len, uint32_t id)
{
	uint8_t len_cmd = RMPP_GET_CMD_LEN(data[0]);

	if (RMPP_CMDID_WR_OUTPUT == data[0]) {
		rmpp_parseOutputCommand(data, len_cmd);
	}
}

/******************************************************************************
* Function Name: rmpp_handleWsClientChange
* Description  : WebSocketのクライアントが接続／切断したときのコールバック関数
* Arguments    : clientCount - connected clients
* Return Value : none
******************************************************************************/
void rmpp_handleWsClientChange(uint32_t id, size_t clientCount)
{
	connectClients = clientCount;
	
	if (clientCount) {
		LED_setLightPattern(LED_PT_BLINK_ON90);
	} else {
		LED_setLightPattern(LED_PT_BLINK_ON10);
	}
}

/******************************************************************************
* Function Name: rmpp_handleWiFiEvent
* Description  : Wi-Fiイベントが発生したときのコールバック関数
* Arguments    : event - WiFi event code
* Return Value : none
******************************************************************************/
void rmpp_handleWiFiEvent(WiFiEvent_t event)
{
	switch (event) {
		case ARDUINO_EVENT_WIFI_READY:
			LED_setLightPattern(LED_PT_BLINK_SLOW);
			break;
		case ARDUINO_EVENT_WIFI_SCAN_DONE:
			break;
		case ARDUINO_EVENT_WIFI_STA_START:
			break;
		case ARDUINO_EVENT_WIFI_STA_STOP:
			LED_setLightPattern(LED_PT_ON);
			break;
		case ARDUINO_EVENT_WIFI_STA_CONNECTED:
			LED_setLightPattern(LED_PT_BLINK_FAST);
			break;
		case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
			LED_setLightPattern(LED_PT_BLINK_SLOW);
			break;
		case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: 
			break;
		case ARDUINO_EVENT_WIFI_STA_GOT_IP:
			LED_setLightPattern(LED_PT_BLINK_ON10);
			break;
		case ARDUINO_EVENT_WIFI_STA_LOST_IP:
			break;
		case ARDUINO_EVENT_WIFI_AP_START:
			LED_setLightPattern(LED_PT_BLINK_ON10);
			break;
		case ARDUINO_EVENT_WIFI_AP_STOP:
			LED_setLightPattern(LED_PT_ON);
			break;
		case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
			break;
		case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
			break;
		default:
			break;
	}
}

/******************************************************************************
* Function Name: rmpp_handleCfgChangeSuccess
* Description  : 設定変更が成功したときのコールバック関数
* Arguments    : none
* Return Value : none
******************************************************************************/
void rmpp_handleCfgChangeSuccess(void)
{
	LED_setLightPatternOnce(LED_PT_BLINK_FAST, 4);
}

/******************************************************************************
* Function Name: rmpp_parseOutputCommand
* Description  : 出力制御コマンドを解析
* Arguments    : data - command data,
                 size - command length
* Return Value : none
******************************************************************************/
void rmpp_parseOutputCommand(uint8_t * data, uint16_t len)
{
	uint16_t duty;
	uint8_t dir = *(data + 2) & 0xC0;

	tickAlive = xTaskGetTickCount();

	if (dir) {
		if (RMPP_MODE_OFF == stRmpp.output.bit.mode) {
			if (dir & 0x40) {
				RMPP_startOutput(RMPP_DIR_FWD);
			} else if (dir & 0x80) {
				RMPP_startOutput(RMPP_DIR_RVS);
			}
		}

		if (RMPP_MODE_ON == stRmpp.output.bit.mode) {
			duty = *(data + 2) & 0x3F;
			duty = duty << 8;
			duty = duty + *(data + 1);

			// duty update
			RMPP_setOutputDuty(duty);
		}	
	} else if (RMPP_MODE_OFF != stRmpp.output.bit.mode) {
		RMPP_stopOutput();
	}
}

/******************************************************************************
* Function Name: rmpp_startOutput
* Description  : 出力動作を開始する
* Arguments    : dir - 進行方向
* Return Value : none
******************************************************************************/
void RMPP_startOutput(rmpp_dir_t dir)
{
	if (RMPP_DIR_NULL == dir) {
		return;
	}

	Serial.println("[info] output on !");

	stRmpp.output.bit.mode = RMPP_MODE_ON;
	stRmpp.status.bit.ext_ctrl = 1;

	LED_setColor(LED_COL_GREEN);
	if (connectClients) {
		LED_setLightPattern(LED_PT_BLINK_ON90);
	} else {
		LED_setLightPattern(LED_PT_ON);
	}

	// output circuit is in brake mode
	ledcWrite(PIN_PWM1, PWM_DUTY_100);
	ledcWrite(PIN_PWM2, PWM_DUTY_100);

	if ((RMPP_DIR_FWD == dir) && (0 == stRmpp.output.bit.rvs)) {
		stRmpp.output.bit.fwd = 1;
	} else if ((RMPP_DIR_RVS == dir) && (0 == stRmpp.output.bit.fwd)) {
		stRmpp.output.bit.rvs = 1;
	}
}

/******************************************************************************
* Function Name: RMPP_setOutputDuty
* Description  : 出力デューティを設定する
* Arguments    : duty = 0 -> Duty 0%, 4095 -> Duty 100%
* Return Value : none
******************************************************************************/
void RMPP_setOutputDuty(uint16_t duty)
{
	if (RMPP_MODE_ON == stRmpp.output.bit.mode) {
		stRmpp.duty_set = (uint16_t)duty;

		// duty cycle inversion
		// (becase PWM signal is active low)
		duty = PWM_DUTY_100 - duty;
		if (PWM_DUTY_MAX < duty) {
			duty = PWM_DUTY_MAX;
		}

		if (stRmpp.output.bit.fwd) {
			// output circuit is Forward mode
			// (voltage polarity : OUT1 -> OUT2)
			ledcWrite(PIN_PWM2, duty);
		} else if (stRmpp.output.bit.rvs) {
			// output circuit is Reverse mode
			// (voltage polarity : OUT2 -> OUT1)
			ledcWrite(PIN_PWM1, duty);
		}
	} else {
		stRmpp.duty_set = 0;
	}
}

/******************************************************************************
* Function Name: RMPP_stopOutput
* Description  : 出力動作を停止する
* Arguments    : none
* Return Value : none
******************************************************************************/
void RMPP_stopOutput(void)
{
	if (RMPP_MODE_ON == stRmpp.output.bit.mode) {
		rmpp_turnOutputOff();
		LED_setColor(LED_COL_STNDBY);
		if (connectClients) {
			LED_setLightPattern(LED_PT_BLINK_ON90);
		} else {
			LED_setLightPattern(LED_PT_BLINK_ON10);
		}
	
		stRmpp.output.bit.mode = RMPP_MODE_OFF;
	} else if (RMPP_MODE_FAULT == stRmpp.output.bit.mode) {
		rmpp_clearFault();
	}
}

/******************************************************************************
* Function Name: rmpp_stopOutputOnFault
* Description  : フォルト要因により出力動作を停止する
* Arguments    : none
* Return Value : none
******************************************************************************/
void rmpp_stopOutputOnFault(void)
{
	rmpp_turnOutputOff();
	LED_setColor(LED_COL_RED);
	LED_setLightPattern(LED_PT_BLINK_FAST);

	stRmpp.output.bit.mode = RMPP_MODE_FAULT;
	stRmpp.status.bit.OverCurrent = 1;

	Serial.println("[warning] motor driver has entered protection mode.");
}

/******************************************************************************
* Function Name: rmpp_turnOutputOff
* Description  : 出力をオフにする
* Arguments    : none
* Return Value : none
******************************************************************************/
void rmpp_turnOutputOff(void)
{
	// output circuit is Hi-Z mode
	ledcWrite(PIN_PWM1, 0);
	ledcWrite(PIN_PWM2, 0);

	stRmpp.output.bit.fwd = 0;
	stRmpp.output.bit.rvs = 0;
	stRmpp.status.bit.ext_ctrl = 0;
	stRmpp.duty_set = 0;

	Serial.println("[info] output off !");
}

/******************************************************************************
* Function Name: rmpp_clearFault
* Description  : フォルト状態をクリアする
* Arguments    : none
* Return Value : none
******************************************************************************/
void rmpp_clearFault(void)
{
	if (FAULT_CLEAR != GPIO_INPUT_GET(PIN_FAULT)) {
		Serial.println("[warning] motor driver is in protection mode.");
		return;
	}

	Serial.println("[info] motor driver has resumed normal operation mode.");

	stRmpp.output.bit.mode = RMPP_MODE_OFF;
	stRmpp.status.bit.OverCurrent = 0;

	LED_setColor(LED_COL_STNDBY);
	if (connectClients) {
		LED_setLightPattern(LED_PT_BLINK_ON90);
	} else {
		LED_setLightPattern(LED_PT_BLINK_ON10);
	}
}

