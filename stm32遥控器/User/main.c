#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "AD.h"
#include "Key.h"
#include "NRF24L01.h"

/*
 * 发送数据包格式（小车端按同样格式解析）：
 * 32字节有效数据，前9字节有效，ADC每路2字节，低字节在前（小端）：
 *   TxBuf[0..1] = AD_Value[0]  PA0  摇杆1 X轴 -> 小车前后
 *   TxBuf[2..3] = AD_Value[1]  PA1  摇杆1 Y轴 -> 小车左右转向
 *   TxBuf[4..5] = AD_Value[2]  PA2  摇杆2 X轴 -> 云台水平
 *   TxBuf[6..7] = AD_Value[3]  PA3  摇杆2 Y轴 -> 云台俯仰
 *   TxBuf[8]    = 喷水命令：0=关 1=低档40% 2=中档70% 3=高档100%
 * 无线参数与参考例程一致：通道40，2Mbps，16位CRC，自动应答+自动重发
 *
 * 按键功能（上电默认静止模式、不喷水）：
 *   PB8：喷水开关。按下开始喷水（若还处于静止模式会自动进入遥控模式开始发送），
 *        再次按下结束喷水
 *   PB9：喷水进行中 = 切换喷水档位（低->中->高循环）；
 *        喷水结束后 = 恢复原功能，启停遥控模式
 */

uint8_t TxBuf[TX_PLOAD_WIDTH];			//发送缓冲区

int main(void)
{
	uint8_t status;
	uint8_t Mode = 0;					//工作模式：0静止，1遥控，上电默认静止
	uint8_t SprayOn = 0;				//喷水开关：0关，1开
	uint8_t SprayLevel = 1;				//喷水档位下标0~2（默认中速70%）
	uint32_t SendCount = 0;
	uint8_t DispTick = 0;
	uint8_t KeyNum;

	/*模块初始化*/
	OLED_Init();				//OLED初始化
	AD_Init();					//AD初始化（PA0~PA3四路摇杆，DMA循环采集到AD_Value[4]）
	Key_Init();					//按键初始化（PB8、PB9）
	NRF24L01_Init();			//NRF24L01初始化（CE默认拉低，待机不发送）

	/*显示静态字符串*/
	OLED_ShowString(1, 1, "AD0:    AD1:");
	OLED_ShowString(2, 1, "AD2:    AD3:");
	OLED_ShowString(3, 1, "TX:IDLE N:00000 ");
	OLED_ShowString(4, 1, "MODE:STOP SPRY:0");

	/*检测NRF24L01模块，检测失败则闪烁提示*/
	while (NRF24L01_Check())
	{
		OLED_ShowString(4, 1, "NRF24L01 ERROR! ");
		Delay_ms(300);
		OLED_ShowString(4, 1, "                ");
		Delay_ms(300);
	}
	OLED_ShowString(4, 1, "MODE:STOP SPRY:0");	//检测通过，恢复默认显示

	while (1)
	{
		KeyNum = Key_GetNum();

		if (KeyNum == 1)			//PB8：喷水开/关
		{
			SprayOn = !SprayOn;
			if (SprayOn && Mode == 0)	//喷水命令要发出去才有效：静止模式自动进入遥控模式
			{
				Mode = 1;
				NRF24L01_TX_Mode();
			}
		}

		if (KeyNum == 2)			//PB9：喷水中调档，喷水结束后恢复启停遥控功能
		{
			if (SprayOn)
			{
				SprayLevel = (SprayLevel + 1) % 3;	//低->中->高循环
			}
			else
			{
				Mode = !Mode;
				if (Mode)			//静止 -> 遥控：配置发送模式
				{
					NRF24L01_TX_Mode();
					OLED_ShowString(3, 4, "RUN  ");
				}
				else				//遥控 -> 静止：CE拉低待机
				{
					NRF24L01_CE = 0;
					OLED_ShowString(3, 4, "IDLE ");
				}
			}
		}

		if (Mode)				//遥控模式下持续发送
		{
			/*把4路ADC值打包进发送缓冲区：每路2字节，低字节在前*/
			TxBuf[0] = (uint8_t)(AD_Value[0] & 0xFF);
			TxBuf[1] = (uint8_t)(AD_Value[0] >> 8);
			TxBuf[2] = (uint8_t)(AD_Value[1] & 0xFF);
			TxBuf[3] = (uint8_t)(AD_Value[1] >> 8);
			TxBuf[4] = (uint8_t)(AD_Value[2] & 0xFF);
			TxBuf[5] = (uint8_t)(AD_Value[2] >> 8);
			TxBuf[6] = (uint8_t)(AD_Value[3] & 0xFF);
			TxBuf[7] = (uint8_t)(AD_Value[3] >> 8);

			TxBuf[8] = SprayOn ? (SprayLevel + 1) : 0;		//喷水命令：0关 1低 2中 3高

			status = NRF24L01_TxPacket(TxBuf);	//发送一个数据包
			SendCount++;

			if (status == TX_OK)		OLED_ShowString(3, 4, "OK   ");
			else if (status == MAX_TX)	OLED_ShowString(3, 4, "MAXRT");	//接收端没收到，达到最大重发次数
			else						OLED_ShowString(3, 4, "ERR  ");
		}

		/*每10次循环刷新一次显示，避免OLED刷新太慢影响发送频率*/
		DispTick++;
		if (DispTick >= 10)
		{
			DispTick = 0;
			OLED_ShowNum(1, 5, AD_Value[0], 4);		//摇杆1 X
			OLED_ShowNum(1, 13, AD_Value[1], 4);	//摇杆1 Y
			OLED_ShowNum(2, 5, AD_Value[2], 4);		//摇杆2 X
			OLED_ShowNum(2, 13, AD_Value[3], 4);	//摇杆2 Y
			OLED_ShowNum(3, 11, SendCount, 5);		//发送计数
			OLED_ShowString(4, 1, Mode ? "MODE:RUN  SPRY:" : "MODE:STOP SPRY:");
			OLED_ShowNum(4, 16, SprayOn ? (SprayLevel + 1) : 0, 1);	//喷水档位0~3
		}

		Delay_ms(20);				//遥控模式下约50Hz发送频率
	}
}
