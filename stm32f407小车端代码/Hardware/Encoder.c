#include "stm32f4xx.h"                  // Device header
#include "Encoder.h"

/*
 * 硬件编码器计数（正交编码器，TI12双边沿=4倍频），参考江协F1编码器接口测速例程：
 *   电机A编码器：E1A/E1B -> PA6/PA7 = TIM3_CH1/CH2
 *   电机B编码器：E2A/E2B -> PD12/PD13 = TIM4_CH1/CH2
 * F1工程里PD12/13用TIM4需要"完全重映射"，F407的GPIO自带AF矩阵，
 * 直接把PD12/PD13配置成AF2即可等效实现，不需要重映射寄存器。
 *
 * 计数器由编码器脉冲驱动自由增减（PSC=0，ARR=65535），
 * EncoderX_Get()返回自上次调用以来的增量并清零，由控制任务周期调用即为测速。
 * 若实际转向与计数符号相反，对调该电机编码器的A/B两根线即可。
 */

/**
  * 函    数：编码器初始化（TIM3+TIM4编码器模式）
  * 参    数：无
  * 返 回 值：无
  */
void Encoder_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;

	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);	//开启TIM3的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);	//开启TIM4的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);	//开启GPIOD的时钟

	/*GPIO初始化：编码器输入引脚复用为定时器，上拉*/
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_TIM3);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_TIM3);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource12, GPIO_AF_TIM4);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_TIM4);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;			//上拉，配合编码器开漏/集电极输出

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;	//电机A编码器
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13;	//电机B编码器
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/*时基初始化：PSC=0每个脉冲计一次，ARR=65535自由计数（增减由编码器方向决定）*/
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1 - 1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 65536 - 1;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);

	/*输入捕获初始化：只用于设置输入滤波器为最大（0xF），滤除电机火花干扰毛刺*/
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 0xF;
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInit(TIM4, &TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInit(TIM4, &TIM_ICInitStructure);

	/*编码器模式：TI12，A/B两相双边沿计数（4倍频），极性都不反相*/
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	TIM_EncoderInterfaceConfig(TIM4, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

	TIM_Cmd(TIM3, ENABLE);									//使能TIM3
	TIM_Cmd(TIM4, ENABLE);									//使能TIM4
}

/**
  * 函    数：读取电机A（左）编码器增量
  * 参    数：无
  * 返 回 值：自上次调用以来的计数值（带符号，正转/反转决定正负），读后计数器清零
  */
int16_t Encoder1_Get(void)
{
	int16_t Temp = TIM_GetCounter(TIM3);
	TIM_SetCounter(TIM3, 0);
	return Temp;
}

/**
  * 函    数：读取电机B（右）编码器增量
  * 参    数：无
  * 返 回 值：自上次调用以来的计数值（带符号），读后计数器清零
  */
int16_t Encoder2_Get(void)
{
	int16_t Temp = TIM_GetCounter(TIM4);
	TIM_SetCounter(TIM4, 0);
	return Temp;
}
