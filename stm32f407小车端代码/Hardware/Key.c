#include "stm32f4xx.h"                  // Device header
#include "Key.h"

//根据按键所在端口使能对应的AHB1时钟
static void Key_EnablePortClock(GPIO_TypeDef *Port)
{
	if (Port == GPIOA)      RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	else if (Port == GPIOB) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	else if (Port == GPIOC) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	else if (Port == GPIOD) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	else if (Port == GPIOE) RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
}

//按键初始化函数（配置引脚为上拉输入，并复位扫描状态）
void Key_Init(KeyHandle_TypeDef *Handle)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	Key_EnablePortClock(Handle->GPIO_Port);

	//按键引脚上拉输入，按键另一端接GND，按下为低电平
	GPIO_InitStructure.GPIO_Pin = Handle->GPIO_Pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(Handle->GPIO_Port, &GPIO_InitStructure);

	Handle->Previous = Bit_SET;
	Handle->Current = Bit_SET;
}

//按键扫描函数（需周期性调用，检测到按键松开时触发回调）
void Key_Scan(KeyHandle_TypeDef *Handle)
{
	Handle->Current = (BitAction)GPIO_ReadInputDataBit(Handle->GPIO_Port, Handle->GPIO_Pin);

	if(Handle->Previous == Bit_RESET && Handle->Current == Bit_SET)
	{
		Handle->ClickedCallback();
	}

	Handle->Previous = Handle->Current;
}
