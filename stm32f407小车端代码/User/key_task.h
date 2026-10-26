#ifndef __KEY_TASK_H__
#define __KEY_TASK_H__

#include "stm32f4xx.h"

extern volatile uint8_t CarMode;	//小车工作模式：0=待机（可遥控）1=自动作业（直线行驶+识别害虫）

void KeyTask(void *pvParameters);

#endif
