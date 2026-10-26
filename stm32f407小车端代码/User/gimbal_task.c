#include "FreeRTOS.h"
#include "task.h"
#include "Servo.h"
#include "nrf_task.h"
#include "key_task.h"
#include "gimbal_task.h"

/*
 * 云台控制任务（仅待机模式）：
 * 把遥杆2的X/Y（AD2=云台水平、AD3=云台俯仰，0~4095）映射为
 * 两个舵机的角度（0~180°），摇杆居中（约2047）时舵机约90°。
 * 链路断开（遥控器静止模式或失联）时舵机自动回中90°。
 * 自动作业模式（CarMode=1）下云台由对靶任务AimTask控制，本任务不写舵机。
 */

#define GIMBAL_SERVO_RANGE	180		//舵机角度范围
#define GIMBAL_ADC_MAX		4095	//ADC满量程

void GimbalTask(void *pvParameters)
{
	Servo_Init();					//TIM1双路PWM初始化，上电默认90°回中

	for (;;)
	{
		if (!CarMode)				//仅待机模式手动控制云台
		{
			if (NrfLinkAlive)
			{
				/*AD2 -> 水平舵机（PE9），AD3 -> 俯仰舵机（PE11）*/
				Servo_SetAngle1((uint8_t)((uint32_t)NrfRxValue[2] * GIMBAL_SERVO_RANGE / GIMBAL_ADC_MAX));
				Servo_SetAngle2((uint8_t)((uint32_t)NrfRxValue[3] * GIMBAL_SERVO_RANGE / GIMBAL_ADC_MAX));
			}
			else
			{
				Servo_SetAngle1(90);	//失联保护：云台回中
				Servo_SetAngle2(90);
			}
		}

		vTaskDelay(pdMS_TO_TICKS(20));	//与遥控器约50Hz的发送频率同步
	}
}
