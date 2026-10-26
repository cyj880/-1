#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f4xx.h"

void Motor_Init(void);
void Motor_SetSpeed(int16_t SpeedA, int16_t SpeedB);	//设置两电机速度，范围-100~100，负值为反转

#endif
