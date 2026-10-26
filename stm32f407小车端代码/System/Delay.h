#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f4xx.h"

void Delay_Init(u8 SYSCLK);
void Delay_us(u32 nus);
void Delay_ms(u16 nms);

#endif
