#ifndef __BATTERY_H
#define __BATTERY_H

#include "stm32f4xx.h"

void Battery_Init(void);
uint16_t Battery_GetRaw(void);			//读取ADC原始值（0~4095）
float Battery_GetVoltage(void);			//换算后的电池电压（V）
uint8_t Battery_GetPercent(void);		//电池电量百分比0~100（按3S锂电11.0~12.6V估算）

#endif
