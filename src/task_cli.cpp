// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#include "task_cli.h"

#ifdef CLI_ATTACH_CALLBACK_FROM_SERVER
#include "task_server.h"
#endif

#if defined(TARGET_RP2040) || defined(TARGET_RP2350)
#include <FreeRTOS.h>
#include <task.h>
#endif

#if defined(ESP32)
#ifndef APP_CPU_NUM
#define APP_CPU_NUM (1)
#endif
#endif

#ifndef CLI_MAX_COMMAND
#define CLI_MAX_COMMAND 32
#endif

static Stream *_serial;
static void (*pFunctions[CLI_MAX_COMMAND])(cli_cmd_t);
static String commandList[CLI_MAX_COMMAND];
static uint8_t commandCount = 0;

/* process task handle */
static TaskHandle_t hTaskCmd = NULL;

static void cli_processTask(void* pvParameters);

static void cli_parseCommand(String command);
static void cli_handleReset(cli_cmd_t command);
static void cli_handleTask(cli_cmd_t command);
static void cli_handleHelp(cli_cmd_t command);

/******************************************************************************
* Function Name: CLI_initTask
* Description  : コンソールタスク初期化
* Arguments    : none
* Return Value : true  -> initialization succeeded, 
                 false -> initialization failed
******************************************************************************/
bool CLI_initTask(Stream &serial)
{
	size_t len = 0;
	_serial = &serial;
	commandCount = 0;

	// preset Reset Function
	CLI_addCommand("RESET", cli_handleReset);
	// preset Task function
	CLI_addCommand("TASK", cli_handleTask);

#ifdef CLI_ATTACH_CALLBACK_FROM_SERVER
	SRV_attachWsTextListener(CLI_processCommand);
#endif

	Serial.println("CLI (Command Line Interface) task is now starting ...");
#if defined(ESP32)
	BaseType_t taskCreated = xTaskCreateUniversal(cli_processTask, "cli_task", 4096, nullptr, 1, &hTaskCmd, APP_CPU_NUM);
#else
	BaseType_t taskCreated = xTaskCreate(cli_processTask, "cli_task", configMINIMAL_STACK_SIZE * 4, nullptr, 1, &hTaskCmd);
#if defined(PICO_CYW43_SUPPORTED)
	// The PicoW WiFi chip controls the LED, and only core 0 can make calls to it safely
	vTaskCoreAffinitySet(hTaskCmd, 1 << 0);
#endif
#endif
	if (pdPASS != taskCreated) {
		Serial.println(" [failure] Failed to create CLI (Command Line Interface) task.");
	}

	return (pdPASS == taskCreated) ? true : false;
}

/******************************************************************************
* Function Name: cli_processTask
* Description  : コンソールタスク
* Arguments    : none
* Return Value : none
******************************************************************************/
void cli_processTask(void* pvParameters)
{
	while (1)
	{
		while (_serial->available()) {
			String command = _serial->readStringUntil('\n');
			cli_parseCommand(command);
		}
		vTaskDelay(1);
	}
}

/******************************************************************************
* Function Name: cli_parseCommand
* Description  : コンソールタスク
* Arguments    : none
* Return Value : none
******************************************************************************/
void cli_parseCommand(String command)
{
	char input[256];
	cli_cmd_t cmd = {};

	command.trim();
	strncpy(input, command.c_str(), sizeof(input) - 1);
	char *command0 = strtok(input, " ");
	command = command0;
	command0 = strtok(NULL, " ");
	cmd.command2 = command0;
	command0 = strtok(NULL, " ");
	cmd.command3 = command0;
	command0 = strtok(NULL, " ");
	cmd.command4 = command0;

	for (int i = 0; i < commandCount; i++) {
		if (commandList[i] == command) {
			pFunctions[i](cmd);
		}
	}

	if (command == "") {
		// Skip
	} else if (command == "?") {
		cli_handleHelp(cmd);
	}

	_serial->println(">");
}

/******************************************************************************
* Function Name: cli_handleReset
* Description  : リセットに関するコンソール処理
* Arguments    : command
* Return Value : none
******************************************************************************/
void cli_handleReset(cli_cmd_t command)
{
#if defined(ESP32)
	ESP.restart();
#elif defined(TARGET_RP2040) || defined(TARGET_RP2350)
	rp2040.reboot();
#endif
}

/******************************************************************************
* Function Name: cli_handleTask
* Description  : タスクに関するコンソール処理
* Arguments    : command
* Return Value : none
******************************************************************************/
void cli_handleTask(cli_cmd_t command)
{
	static char buf[1024];
	vTaskList(buf);
	Serial.println("Name          Status  Priority  HiMark   ID");
	Serial.println("-----------------------------------------------------------");
	Serial.println(buf);
}

/******************************************************************************
* Function Name: cli_handleHelp
* Description  : ヘルプに関するコンソール処理
* Arguments    : command
* Return Value : none
******************************************************************************/
void cli_handleHelp(cli_cmd_t command)
{
	if (commandCount) {
		Serial.println("----- CLI Command List -----");
		for (int i = 0; i < commandCount; i++) {
			Serial.printf(" [%02d] %s\n", i, commandList[i].c_str());
		}
	} else {
		Serial.println("CLI command is not found.");
	}
}

/******************************************************************************
* Function Name: CLI_addCommand
* Description  : コンソールコマンドを追加する
* Arguments    : none
* Return Value : none
******************************************************************************/
void CLI_addCommand(String command, void (*function)(cli_cmd_t))
{
	if (CLI_MAX_COMMAND <= commandCount) {
		return;
	}

	commandList[commandCount] = command;
	pFunctions[commandCount] = function;
	commandCount++;
}

/******************************************************************************
* Function Name: CLI_processCommand
* Description  : コンソールコマンドを処理する
* Arguments    : id - for websocket client id (set to 0 when not in use)
* Return Value : none
******************************************************************************/
void CLI_processCommand(String command, uint32_t id)
{
	cli_parseCommand(command);
}
