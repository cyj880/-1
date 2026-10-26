#include "FreeRTOS.h"
#include "task.h"
#include "Ultrasonic.h"
#include "IR.h"
#include "sensor_task.h"

/*
 * 传感器采集任务：
 * 每60ms测一次超声波距离（HC-SR04要求两次触发间隔>=60ms，防止回波串扰），
 * 同时读两路红外避障状态，结果发布到全局变量供显示/上报/后续避障逻辑使用。
 * 超过500ms没有收到有效回波，距离置999（无回波/超时）。
 */

volatile uint16_t SensorDistanceCm = 999;
volatile uint8_t  SensorIR1 = 0;
volatile uint8_t  SensorIR2 = 0;

void SensorTask(void *pvParameters)
{
	TickType_t LastValidTick = 0;
	uint16_t LastDistance = 999;

	Ultrasonic_Init();			//超声波初始化（TIM9输入捕获）
	IR_Init();					//双红外初始化

	for (;;)
	{
		Ultrasonic_Trigger();	//发触发脉冲，回波在中断里自动完成测量

		SensorIR1 = IR1_Detected();
		SensorIR2 = IR2_Detected();

		if (Ultrasonic_GetCm() != LastDistance)			//有新的有效回波
		{
			LastDistance = Ultrasonic_GetCm();
			SensorDistanceCm = LastDistance;
			LastValidTick = xTaskGetTickCount();
		}
		else if ((xTaskGetTickCount() - LastValidTick) > pdMS_TO_TICKS(500))
		{
			SensorDistanceCm = 999;						//长时间无回波：超时
		}

		vTaskDelay(pdMS_TO_TICKS(60));
	}
}
