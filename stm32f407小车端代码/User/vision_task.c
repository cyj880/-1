#include "FreeRTOS.h"
#include "task.h"
#include "Vision.h"
#include "key_task.h"
#include "vision_task.h"

/*
 * 视觉数据处理任务：
 * 1. 每10ms解析一次MaixCAM发来的坐标帧（USART3，协议见Hardware/Vision.h）
 * 2. 发布目标坐标/大小/是否在线到全局变量，供对靶云台、自动喷药、APP上报使用
 * 3. 病虫害累计计数：目标从"出现到消失"算1只（消失超过600ms才判定离开，
 *    防止检测抖动导致重复计数）；仅自动作业模式（KEY1启动，CarMode=1）下计数
 */

volatile int16_t VisionX = -1, VisionY = -1;
volatile uint16_t VisionW = 0, VisionH = 0;
volatile uint16_t VisionPestCount = 0;
volatile uint8_t  VisionTargetFound = 0;
volatile uint8_t  VisionOnline = 0;

void VisionTask(void *pvParameters)
{
	uint8_t Present = 0;						//目标出现状态
	TickType_t AbsentSince = 0;				//目标消失起始时刻（0=未开始计时）

	Vision_Init();							//USART3初始化（PD8/PD9，115200）

	for (;;)
	{
		Vision_Parse();						//解析接收缓冲里的所有字节

		VisionOnline = Vision_Online();		//视觉模块是否在线

		if (Vision_GotTarget())
		{
			VisionX = Vision_GetX();			//目标中心像素坐标
			VisionY = Vision_GetY();
			VisionW = Vision_GetW();
			VisionH = Vision_GetH();
			VisionTargetFound = 1;
			Present = 1;
			AbsentSince = 0;
		}
		else
		{
			if (Present)						//从有目标到无目标：开始防抖计时
			{
				if (AbsentSince == 0) AbsentSince = xTaskGetTickCount();
				if ((xTaskGetTickCount() - AbsentSince) > pdMS_TO_TICKS(600))
				{
					if (CarMode) VisionPestCount++;	//自动作业模式下才计数（一次"出现→消失"=1只）
					Present = 0;
					AbsentSince = 0;
					VisionTargetFound = 0;
					VisionX = -1;
					VisionY = -1;
				}
			}
			else
			{
				VisionTargetFound = 0;
				VisionX = -1;
				VisionY = -1;
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
