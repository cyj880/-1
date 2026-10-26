#ifndef __MOTOR_TASK_H
#define __MOTOR_TASK_H

#include "stm32f4xx.h"

extern volatile int16_t MotorEncoderA;	//A电机（左）20ms编码器增量
extern volatile int16_t MotorEncoderB;	//B电机（右）20ms编码器增量

void MotorTask(void *pvParameters);		//电机控制任务

#endif
