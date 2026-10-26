#include "stm32f4xx.h"                  // Device header
#include "WaterLevel.h"

/*
 * 药箱液位检测（电阻式水位传感器，PA4 = ADC1_IN4）：
 * 传感器竖直固定在药箱内，输出电压随浸没深度线性变化，
 * 两点标定（空箱/满箱的ADC读数）后换算成余量百分比。
 *
 * !! 使用步骤：
 * 1. 传感器到位后把WATER_SENSOR_PRESENT改为1
 * 2. 空箱时读一次ADC填到WATER_ADC_EMPTY（可在Keil调试器看Battery同款函数，
 *    或临时用OLED显示原始值）
 * 3. 装满后读一次填到WATER_ADC_FULL
 * 4. 上报JSON的"tank"字段即为余量百分比
 *
 * 换成电容式非接触传感器（贴杯壁）时，标定方法相同。
 */

#define WATER_SENSOR_PRESENT	0		//传感器已接入：1=启用，0=未接入（恒返回0）
#define WATER_ADC_EMPTY			800		//空箱时的ADC读数（标定值）
#define WATER_ADC_FULL			3000	//满箱时的ADC读数（标定值）

#if WATER_SENSOR_PRESENT

/*读取液位ADC原始值（ADC1通道4，PA4）*/
static uint16_t WaterLevel_GetRaw(void)
{
	ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_480Cycles);
	ADC_SoftwareStartConv(ADC1);
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
	return ADC_GetConversionValue(ADC1);
}

#endif

/**
  * 函    数：液位检测初始化（PA4模拟输入，与电池检测共用ADC1）
  * 参    数：无
  * 返 回 值：无
  */
void WaterLevel_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);	//开启ADC1的时钟（Battery_Init已开，重复调用无害）

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;				//PA4
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;			//模拟功能
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/**
  * 函    数：获取药箱余量百分比
  * 说    明：连续采8次取平均（滤除泵抽水时的液面波动），两点标定换算并限幅
  * 返 回 值：0~100；传感器未接入（WATER_SENSOR_PRESENT=0）时恒为0
  */
uint8_t WaterLevel_GetPercent(void)
{
#if WATER_SENSOR_PRESENT
	uint32_t Sum = 0;
	uint8_t i;
	int32_t Percent;

	for (i = 0; i < 8; i++)									//8次滑动平均抗液面波动
	{
		Sum += WaterLevel_GetRaw();
	}
	Percent = (int32_t)(Sum / 8 - WATER_ADC_EMPTY) * 100 / (WATER_ADC_FULL - WATER_ADC_EMPTY);

	if (Percent < 0) Percent = 0;
	if (Percent > 100) Percent = 100;
	return (uint8_t)Percent;
#else
	return 0;												//传感器未接入
#endif
}
