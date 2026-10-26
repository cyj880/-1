#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f4xx.h"

void Encoder_Init(void);
int16_t Encoder1_Get(void);		//电机A（左）编码器增量，TIM3，读后自动清零
int16_t Encoder2_Get(void);		//电机B（右）编码器增量，TIM4，读后自动清零

#endif
