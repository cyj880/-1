#ifndef __WATERLEVEL_H
#define __WATERLEVEL_H

#include "stm32f4xx.h"

void WaterLevel_Init(void);
uint8_t WaterLevel_GetPercent(void);	//药箱余量百分比0~100（传感器未接入时恒为0）

#endif
