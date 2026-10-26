#include "FreeRTOS.h"
#include "task.h"
#include "Motor.h"
#include "Encoder.h"
#include "nrf_task.h"
#include "aim_task.h"
#include "key_task.h"
#include "motor_task.h"

/*
 * 电机控制任务（按工作模式选择数据来源）：
 * 待机模式（CarMode=0）：遥杆1的X/Y（AD0=前后、AD1=转向）差速控制，
 *   链路断开（遥控器静止模式或失联）时自动停车；
 * 自动作业模式（CarMode=1，KEY1启动）：电机指令来自对靶任务AimTask
 *   （巡逻直行/转向对准/停车微调，近障停车也在对靶任务里处理）。
 * 两种模式均读取编码器增量供测速/里程使用。
 */

#define MOTOR_DEADZONE	6		//摇杆中位死区（约±3%），消除摇杆漂移

volatile int16_t MotorEncoderA = 0;		//A电机（左）20ms编码器增量，供后续速度闭环/里程使用
volatile int16_t MotorEncoderB = 0;		//B电机（右）20ms编码器增量

/*ADC值（0~4095）换算成-100~100，中位附近视为0*/
static int16_t ADC_To_Percent(uint16_t adc)
{
	int32_t v = ((int32_t)adc - 2048) * 100 / 2047;

	if (v > -MOTOR_DEADZONE && v < MOTOR_DEADZONE) return 0;
	if (v > 100) v = 100;
	if (v < -100) v = -100;
	return (int16_t)v;
}

/*限幅到±100*/
static int16_t Limit100(int32_t v)
{
	if (v > 100) v = 100;
	if (v < -100) v = -100;
	return (int16_t)v;
}

void MotorTask(void *pvParameters)
{
	int16_t Throttle, Turn, SpeedA, SpeedB;

	Motor_Init();					//TB6612初始化，上电默认停车
	Encoder_Init();					//编码器初始化（TIM3/TIM4硬件计数）

	for (;;)
	{
		if (CarMode)				//自动作业模式：执行对靶任务输出的电机指令
		{
			SpeedA = AimDriveL;
			SpeedB = AimDriveR;
		}
		else if (NrfLinkAlive)		//待机模式：遥控驾驶
		{
			Throttle = ADC_To_Percent(NrfRxValue[0]);	//AD0：前后
			Turn     = ADC_To_Percent(NrfRxValue[1]);	//AD1：转向

			/*差速控制：A=左电机，B=右电机（若实车方向相反，对调电机接线或在此改符号）*/
			SpeedA = Limit100(Throttle + Turn);
			SpeedB = Limit100(Throttle - Turn);
		}
		else						//待机且无遥控：停车
		{
			SpeedA = 0;
			SpeedB = 0;
		}

		Motor_SetSpeed(SpeedA, SpeedB);

		/*读取20ms内的编码器增量（开环阶段仅作记录，闭环/自动行驶阶段用于测速）*/
		MotorEncoderA = Encoder1_Get();
		MotorEncoderB = Encoder2_Get();

		vTaskDelay(pdMS_TO_TICKS(20));	//与遥控器约50Hz的发送频率同步
	}
}
