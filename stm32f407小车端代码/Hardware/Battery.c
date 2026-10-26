#include "stm32f4xx.h"                  // Device header
#include "Battery.h"

/*
 * 电池电压检测：轮趣TB6612驱动板把电池电压分压后从ADC引脚（PA5）送出。
 * ADC读数 * 3.3 / 4095 得到PA5引脚电压，再乘分压比得到电池电压。
 * 分压比BATTERY_DIVIDER按轮趣板载分压默认11倍，可用万用表实测电池电压后校准：
 *   BATTERY_DIVIDER = 实测电池电压 / (ADC原始值 * 3.3 / 4095)
 * 后续可在Battery_GetVoltage基础上换算成电量百分比（按3S锂电11.0~12.6V区间）。
 */

#define BATTERY_DIVIDER		11.0f	//驱动板分压比，按实测校准

/**
  * 函    数：电池电压检测ADC初始化（ADC1通道5，PA5）
  * 参    数：无
  * 返 回 值：无
  */
void Battery_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;

	/*开启时钟*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	//开启ADC1的时钟

	/*GPIO初始化：PA5模拟输入*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;			//模拟功能
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/*ADC公共初始化：独立模式，时钟84MHz/4=21MHz（低于36MHz上限）*/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/*ADC1初始化：12位右对齐，单次软件触发转换*/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_Cmd(ADC1, ENABLE);									//使能ADC1
}

/**
  * 函    数：读取一次ADC原始值
  * 参    数：无
  * 返 回 值：ADC转换结果0~4095
  */
uint16_t Battery_GetRaw(void)
{
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, 1, ADC_SampleTime_480Cycles);	//通道5（PA5），长采样时间更稳
	ADC_SoftwareStartConv(ADC1);							//软件触发转换
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);	//等待转换完成
	return ADC_GetConversionValue(ADC1);
}

/**
  * 函    数：获取电池电压
  * 参    数：无
  * 返 回 值：电池电压（V）
  */
float Battery_GetVoltage(void)
{
	return Battery_GetRaw() * 3.3f / 4095.0f * BATTERY_DIVIDER;
}

/**
  * 函    数：获取电池电量百分比
  * 说    明：按3S锂电池电压区间11.0V（空）~12.6V（满）线性估算，限幅0~100
  * 返 回 值：0~100
  */
uint8_t Battery_GetPercent(void)
{
	int32_t Percent = (int32_t)((Battery_GetVoltage() - 11.0f) / 1.6f * 100);

	if (Percent < 0) Percent = 0;
	if (Percent > 100) Percent = 100;
	return (uint8_t)Percent;
}
