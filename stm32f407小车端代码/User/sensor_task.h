#ifndef __SENSOR_TASK_H
#define __SENSOR_TASK_H

#include "stm32f4xx.h"

/* 由SensorTask周期更新，其他任务（避障/显示/上报）只读 */
extern volatile uint16_t SensorDistanceCm;	//超声波距离cm（2~400），999=无回波/超时
extern volatile uint8_t  SensorIR1;			//红外避障1（PE0）：1=检测到障碍
extern volatile uint8_t  SensorIR2;			//红外避障2（PE1）：1=检测到障碍

void SensorTask(void *pvParameters);		//传感器采集任务

#endif
