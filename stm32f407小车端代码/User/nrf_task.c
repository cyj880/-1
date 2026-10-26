#include "FreeRTOS.h"
#include "task.h"
#include "NRF24L01.h"
#include "Pump.h"
#include "key_task.h"
#include "nrf_task.h"

/*
 * NRF接收任务：
 * 1. 初始化NRF24L01并检测模块（失败则周期性重试，等待模块插好）
 * 2. 配置为接收模式（地址/频道/速率与遥控器端一致）
 * 3. 循环轮询接收，解析数据包：
 *    前8字节 -> 4路ADC值存入NrfRxValue[]；
 *    第9字节[8] -> 喷水命令NrfSprayCmd（0关/1低40%/2中70%/3高100%），
 *    命令变化时立即设置泵占空比
 * 4. 超过500ms没收到数据判定失联（NrfLinkAlive=0），失联瞬间自动停喷（安全保护）
 *
 * 数据包格式（遥控器端main.c定义）：32字节定长，前9字节有效，
 * 每路ADC占2字节低字节在前，[8]=喷水命令。
 */

#define NRF_LINK_TIMEOUT_MS		500		//失联判定时间：超过该时间没收到数据视为失联

volatile uint16_t NrfRxValue[4];
volatile uint8_t  NrfSprayCmd = 0;		//喷水命令：0关 1低 2中 3高
volatile uint8_t  NrfLinkAlive = 0;
volatile uint32_t NrfRxCount = 0;

void NrfTask(void *pvParameters)
{
	uint8_t RxBuf[RX_PLOAD_WIDTH];
	TickType_t LastRxTick = 0;
	uint8_t LastSprayCmd = 0xFF;			//上次执行过的喷水命令（初值强制首包生效）
	uint8_t WasAlive = 0;					//上一轮的链路状态

	NRF24L01_Init();					//NRF模块初始化

	while (NRF24L01_Check())			//检测不到模块时周期性重试（模块没插好/接线错误）
	{
		vTaskDelay(pdMS_TO_TICKS(500));
	}

	NRF24L01_RX_Mode();					//配置为接收模式，开始等待遥控器数据

	for (;;)
	{
		/*把RX FIFO中已收到的包全部取出（遥控器约50Hz发包，5ms轮询不会积压）*/
		while (NRF24L01_RxPacket(RxBuf) == 0)
		{
			NrfRxValue[0] = (uint16_t)RxBuf[0] | ((uint16_t)RxBuf[1] << 8);		//AD0 前后
			NrfRxValue[1] = (uint16_t)RxBuf[2] | ((uint16_t)RxBuf[3] << 8);		//AD1 转向
			NrfRxValue[2] = (uint16_t)RxBuf[4] | ((uint16_t)RxBuf[5] << 8);		//AD2 云台水平
			NrfRxValue[3] = (uint16_t)RxBuf[6] | ((uint16_t)RxBuf[7] << 8);		//AD3 云台俯仰
			NrfSprayCmd = RxBuf[8] & 0x03;										//喷水命令（只取低2位）
			NrfRxCount++;
			LastRxTick = xTaskGetTickCount();
		}

		/*判断链路是否存活：刚收到过数据=遥控器在发送；超时=静止模式或失联*/
		if ((xTaskGetTickCount() - LastRxTick) > pdMS_TO_TICKS(NRF_LINK_TIMEOUT_MS))
		{
			NrfLinkAlive = 0;
		}
		else
		{
			NrfLinkAlive = 1;
		}

		/*喷水命令执行：待机模式下遥控命令控制泵；自动作业模式下泵由对靶任务接管*/
		if (NrfLinkAlive)
		{
			if (NrfSprayCmd != LastSprayCmd)
			{
				if (!CarMode)								//自动作业时不抢泵控制权
				{
					/*首次同步且命令为"关"时不强制停泵：保留APP/本地按键已开启的状态*/
					if (!(LastSprayCmd == 0xFF && NrfSprayCmd == 0))
					{
						Pump_SetLevel(NrfSprayCmd);
					}
				}
				LastSprayCmd = NrfSprayCmd;
			}
			WasAlive = 1;
		}
		else
		{
			if (WasAlive)				//遥控链路刚断开：立即停喷
			{
				Pump_SetDuty(0);
				LastSprayCmd = 0xFF;
				WasAlive = 0;
			}
		}

		vTaskDelay(pdMS_TO_TICKS(5));
	}
}
