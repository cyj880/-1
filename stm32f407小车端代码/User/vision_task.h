#ifndef __VISION_TASK_H
#define __VISION_TASK_H

#include "stm32f4xx.h"

/* 视觉识别结果（由VisionTask更新，其他任务只读） */
extern volatile int16_t VisionX, VisionY;		//主目标中心像素坐标（-1=当前无目标）
extern volatile uint16_t VisionW, VisionH;		//主目标框宽高（目标大小，喷药量参考）
extern volatile uint16_t VisionPestCount;		//累计病虫害数量（APP查看用）
extern volatile uint8_t  VisionTargetFound;		//1=当前识别到目标（对靶喷药用）
extern volatile uint8_t  VisionOnline;			//1=视觉模块通信正常（1秒内有有效帧）

void VisionTask(void *pvParameters);			//视觉数据处理任务

#endif
