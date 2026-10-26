#ifndef __ULTRASONIC_H
#define __ULTRASONIC_H

#include "stm32f4xx.h"

void Ultrasonic_Init(void);
void Ultrasonic_Trigger(void);			//发一次测距触发脉冲（10us以上）
uint16_t Ultrasonic_GetCm(void);		//最近一次有效距离cm（2~400），999=超时无回波

#endif
