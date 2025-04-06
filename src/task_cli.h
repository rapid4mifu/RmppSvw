// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

#ifndef __TASK_CLI_H__	/* “ñd’è‹`–h~ */

#include <Arduino.h>

typedef struct {
	String command2;
	String command3;
	String command4;
} cli_cmd_t;

bool CLI_initTask(Stream &serial = Serial);

void CLI_processCommand(String command, uint32_t id = 0);
void CLI_addCommand(String command, void (*function)(cli_cmd_t));

#endif /* __TASK_CLI_H__*/	/* “ñd’è‹`–h~ */
#define __TASK_CLI_H__	/* “ñd’è‹`–h~ */

