#ifndef __AIM_TASK_H
#define __AIM_TASK_H

#include "stm32f4xx.h"

/* 对靶任务输出的电机指令（自动作业模式下MotorTask直接使用），-100~100 */
extern volatile int8_t AimDriveL;
extern volatile int8_t AimDriveR;

void AimTask(void *pvParameters);		//对靶喷药控制任务（仅CarMode=1时接管执行器）

#endif
