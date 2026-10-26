#ifndef __IR_H
#define __IR_H

#include "stm32f4xx.h"

void IR_Init(void);
uint8_t IR1_Detected(void);			//红外避障1（PE0）：1=检测到障碍
uint8_t IR2_Detected(void);			//红外避障2（PE1）：1=检测到障碍

#endif
