#ifndef __NRF_TASK_H
#define __NRF_TASK_H

#include "stm32f4xx.h"

/* 以下变量由NrfTask写入，其他任务（如OLED显示、电机控制）只读 */

extern volatile uint16_t NrfRxValue[4];		//遥控器发来的4路摇杆ADC原始值（0~4095）
											//[0]前后 [1]转向 [2]云台水平 [3]云台俯仰
extern volatile uint8_t  NrfSprayCmd;		//遥控器喷水命令：0关 1低40% 2中70% 3高100%
extern volatile uint8_t  NrfLinkAlive;		//1=遥控器处于遥控模式且通信正常；0=静止模式或失联
extern volatile uint32_t NrfRxCount;		//累计收到的数据包数量

void NrfTask(void *pvParameters);			//NRF接收任务

#endif
