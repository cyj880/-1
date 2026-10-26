#ifndef __PUMP_H
#define __PUMP_H

#include "stm32f4xx.h"

#define PUMP_LEVEL_NUM	3		//转速档位数（对应Pump.c里的档位表）

void Pump_Init(void);
void Pump_Toggle(void);			//开/关泵（按当前档位转速）
void Pump_NextLevel(void);		//切换转速档位（40%/70%/100%循环，开着时立即生效）
uint8_t Pump_GetDuty(void);		//当前占空比0~100（OLED显示用）
void Pump_SetDuty(uint8_t Duty);	//直接设置占空比0~100（留给自动模式/安全逻辑）
void Pump_SetLevel(uint8_t Level);	//按档位设置：0=关 1=低40% 2=中70% 3=高100%（遥控/APP共用）

#endif
