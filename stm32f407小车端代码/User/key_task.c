#include "FreeRTOS.h"
#include "task.h"
#include "Key.h"
#include "Pump.h"

/*
 * 按键扫描任务：每10ms扫描一次两个按键（PD14、PD15），松手时触发回调
 * 按键另一端接GND，内部上拉，按下为低电平
 * 按键1（PD14）：启动/停止自动作业（直线行驶+识别害虫）
 *               上电默认待机，待机时可正常遥控；启动后电机不听摇杆，由自动逻辑驱动
 * 按键2（PD15）：切换水泵转速档位（40%/70%/100%循环）
 */

static KeyHandle_TypeDef hkey1;		//按键1：PD14
static KeyHandle_TypeDef hkey2;		//按键2：PD15

volatile uint8_t CarMode = 0;		//工作模式：0待机 1自动作业（上电默认待机）

static void KEY1_ClickedCallback(void);
static void KEY2_ClickedCallback(void);

//按键扫描任务：每10ms扫描一次按键
void KeyTask(void *pvParameters)
{
	hkey1.GPIO_Port = KEY1_GPIO_Port;
	hkey1.GPIO_Pin = KEY1_Pin;
	hkey1.ClickedCallback = KEY1_ClickedCallback;

	hkey2.GPIO_Port = KEY2_GPIO_Port;
	hkey2.GPIO_Pin = KEY2_Pin;
	hkey2.ClickedCallback = KEY2_ClickedCallback;

	Key_Init(&hkey1);
	Key_Init(&hkey2);

	for(;;)
	{
		Key_Scan(&hkey1);
		Key_Scan(&hkey2);

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

//按键1（PD14）松开回调函数：启动/停止自动作业
static void KEY1_ClickedCallback(void)
{
	CarMode = !CarMode;				//切换待机/自动作业（停止时电机任务会自动停车）
}

//按键2（PD15）松开回调函数：切换水泵转速档位
static void KEY2_ClickedCallback(void)
{
	Pump_NextLevel();
}
