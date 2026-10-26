#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "OLED.h"
#include "key_task.h"
#include "nrf_task.h"
#include "gimbal_task.h"
#include "motor_task.h"
#include "esp_task.h"
#include "sensor_task.h"
#include "vision_task.h"
#include "aim_task.h"
#include "Battery.h"
#include "WaterLevel.h"
#include "Pump.h"
#include "ESP8266.h"

/*
 * 小车端（接收）：
 * NrfTask负责接收遥控器数据（4路摇杆ADC + 链路状态），结果放在
 * nrf_task.h里的全局变量NrfRxValue[]/NrfLinkAlive/NrfRxCount中。
 * GimbalTask把AD2/AD3映射为云台两个舵机角度（PE9/PE11）。
 * MotorTask把AD0/AD1换算成差速速度驱动TB6612电机（PB8/PB9 PWM）。
 * EspTask通过ESP8266（USART2）连WiFi，把电池电压等数据JSON上报手机app。
 * KeyTask扫描两个板载按键（PD14、PD15）：KEY1启动/停止自动作业（直线行驶+
 * 识别害虫，CarMode），KEY2切换泵转速档位（水泵PWM在PC9=TIM8_CH4，见Pump.c）。
 * OLED任务显示链路状态、泵占空比、4路ADC、收包计数和电池电压（PA5）。
 */

static void OLED_Task(void *pvParameters);

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	OLED_Init();
	Battery_Init();
	WaterLevel_Init();	//药箱液位ADC初始化（PA4，传感器未接入时上报0）
	Pump_Init();		//水泵PWM初始化（PC9，默认关闭）
	ESP8266_Init();		//ESP8266的USART2硬件初始化（联网流程在EspTask里）

	configASSERT(xTaskCreate(OLED_Task, "OLED Task", 256, NULL, 1, NULL) == pdPASS);
	configASSERT(xTaskCreate(EspTask, "ESP Task", 256, NULL, 1, NULL) == pdPASS);
	configASSERT(xTaskCreate(KeyTask, "KEY Task", 128, NULL, 3, NULL) == pdPASS);
	configASSERT(xTaskCreate(NrfTask, "NRF Task", 256, NULL, 3, NULL) == pdPASS);
	configASSERT(xTaskCreate(GimbalTask, "GIMBAL Task", 128, NULL, 2, NULL) == pdPASS);
	configASSERT(xTaskCreate(MotorTask, "MOTOR Task", 128, NULL, 2, NULL) == pdPASS);
	configASSERT(xTaskCreate(SensorTask, "SENSOR Task", 128, NULL, 2, NULL) == pdPASS);
	configASSERT(xTaskCreate(VisionTask, "VISION Task", 256, NULL, 2, NULL) == pdPASS);
	configASSERT(xTaskCreate(AimTask, "AIM Task", 256, NULL, 2, NULL) == pdPASS);

	vTaskStartScheduler();

	while(1)
	{
	}
}

static void OLED_Task(void *pvParameters)
{
	OLED_Clear();

	while(1)
	{
		float Vbat = Battery_GetVoltage();

		/*第0行：工作模式 + 水泵占空比。AUTO=自动作业（KEY1启动）；MANU=待机可遥控*/
		OLED_Printf(0, 0, OLED_8X16, "MODE:%s P:%u", CarMode ? "AUTO" : "MANU", Pump_GetDuty());

		/*第1、2行：遥控器发来的4路摇杆ADC原始值（0~4095）*/
		OLED_Printf(0, 16, OLED_8X16, "A0:%4u A1:%4u", NrfRxValue[0], NrfRxValue[1]);
		OLED_Printf(0, 32, OLED_8X16, "A2:%4u A3:%4u", NrfRxValue[2], NrfRxValue[3]);

		/*第3行：超声波距离cm（999=无回波）+ 红外避障状态（0无障碍）+ 电池电压*/
		OLED_Printf(0, 48, OLED_8X16, "D:%03u I:%u%u %d.%dV", SensorDistanceCm, SensorIR1, SensorIR2, (int)Vbat, (int)(Vbat * 10) % 10);

		OLED_Update();

		vTaskDelay(pdMS_TO_TICKS(100));
	}
}


/* configSUPPORT_STATIC_ALLOCATION=1 时内核需要的内存回调 */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/* configCHECK_FOR_STACK_OVERFLOW=2 时内核需要的栈溢出钩子 */
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
	(void) xTask;
	(void) pcTaskName;
	for( ;; );
}
