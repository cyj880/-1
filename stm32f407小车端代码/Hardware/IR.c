#include "stm32f4xx.h"                  // Device header
#include "IR.h"

/*
 * 红外避障传感器（两路，数字输出）：
 * 红外避障1 -> PE0，红外避障2 -> PE1，均3.3V供电（模块VCC接3V3）
 * 常见红外避障模块：检测到障碍时OUT输出低电平，无障碍输出高电平，
 * 检测距离由模块上的电位器调节（一般2~30cm）。
 * 若所用模块为高电平有效（少数），把下面两个函数里的==0改成==1即可。
 */

/**
  * 函    数：红外避障初始化
  * 参    数：无
  * 返 回 值：无
  */
void IR_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);	//开启GPIOE的时钟

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;	//PE0、PE1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;			//输入模式
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;			//上拉（悬空时视为无障碍）
	GPIO_Init(GPIOE, &GPIO_InitStructure);
}

/**
  * 函    数：红外避障1状态（PE0）
  * 返 回 值：1=检测到障碍，0=无障碍
  */
uint8_t IR1_Detected(void)
{
	return GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_0) == Bit_RESET ? 1 : 0;
}

/**
  * 函    数：红外避障2状态（PE1）
  * 返 回 值：1=检测到障碍，0=无障碍
  */
uint8_t IR2_Detected(void)
{
	return GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_1) == Bit_RESET ? 1 : 0;
}
