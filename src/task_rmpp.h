// --------------------------------------------------------
// Copyright (c) 2025 rapid4mifu 
//
// These codes are licensed under GPL v3.0
// https://opensource.org/license/GPL-3.0
// --------------------------------------------------------

// This software module is part of the Railway Model Power Pack (RMPP)

#ifndef __TASK_RMPP_H__	/* 二重定義防止 */

#include <Arduino.h>

typedef enum {
	RMPP_DIR_NULL = 0,	/* 進行方向・未指定 */
	RMPP_DIR_FWD,		/* 進行方向・前進 */
	RMPP_DIR_RVS		/* 進行方向・後退 */
} rmpp_dir_t;

bool RMPP_initTask(void);

void RMPP_resetOutput(void);
void RMPP_startOutput(rmpp_dir_t dir);
void RMPP_stopOutput(bool inhbit = false);
void RMPP_setOutputDuty(uint16_t duty);

#endif /* __TASK_RMPP_H__*/	/* 二重定義防止 */
#define __TASK_RMPP_H__	/* 二重定義防止 */

