#include "stm32f4xx.h"                  // Device header
#include "Ultrasonic.h"

/*
 * HC-SR04超声波测距驱动：
 * Trig->PE7（GPIO输出），Echo->PE5（TIM9_CH1输入捕获，AF3）
 * 模块VCC接5V（按模块要求），GND共地。Trig发>=10us高电平后模块自动发8个
 * 40kHz脉冲，Echo脚输出一个高电平，宽度对应往返时间：距离cm = us/58。
 *
 * TIM9（APB2，定时器时钟168MHz）配成1MHz自由计数（1us一计数，16位回绕65.5ms，
 * 远大于最大回波38ms，不会回绕）。通道1双沿捕获：上升沿记起点，下降沿算宽度，
 * 全部在中断里完成，主任务只管周期性Trig和读结果。
 * TIM9的中断向量和TIM1刹车共用：TIM1_BRK_TIM9_IRQHandler。
 */

static volatile uint16_t UltrasonicDistanceCm = 999;	//最近一次有效距离，999=无回波
static volatile uint16_t CaptureStart = 0;				//上升沿时刻

/**
  * 函    数：超声波初始化（TIM9_CH1输入捕获 + Trig引脚）
  * 参    数：无
  * 返 回 值：无
  */
void Ultrasonic_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);	//开启TIM9的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);	//开启GPIOE的时钟

	/*Trig：PE7推挽输出，默认低*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOE, GPIO_Pin_7);

	/*Echo：PE5复用为TIM9_CH1*/
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource5, GPIO_AF_TIM9);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/*时基：168MHz/168 = 1MHz（1us一计数），自由计数*/
	TIM_TimeBaseInitStructure.TIM_Prescaler = 168 - 1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM9, &TIM_TimeBaseInitStructure);

	/*输入捕获：通道1，双沿触发，直连TI1*/
	TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_BothEdge;	//双沿捕获（F4库宏名无s）
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	//F4库宏名：不分频
	TIM_ICInitStructure.TIM_ICFilter = 0x0;
	TIM_ICInit(TIM9, &TIM_ICInitStructure);

	TIM_ITConfig(TIM9, TIM_IT_CC1, ENABLE);				//允许捕获中断
	TIM_Cmd(TIM9, ENABLE);								//使能TIM9

	/*中断：TIM9与TIM1刹车共用向量，抢占1（高于任务、低于关键中断）*/
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_BRK_TIM9_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/**
  * 函    数：发一次测距触发脉冲（约20us高电平）
  * 说    明：两次Trig间隔应>=60ms，防止上次回波串扰；在任务上下文调用
  * 返 回 值：无
  */
void Ultrasonic_Trigger(void)
{
	volatile uint32_t i;

	GPIO_SetBits(GPIOE, GPIO_Pin_7);
	for (i = 0; i < 500; i++);							//忙等约20us（HC-SR04要求>=10us）
	GPIO_ResetBits(GPIOE, GPIO_Pin_7);
}

/**
  * 函    数：获取最近一次有效距离
  * 返 回 值：距离cm（2~400）；999=尚无回波/超时（由sensor_task超时置位）
  */
uint16_t Ultrasonic_GetCm(void)
{
	return UltrasonicDistanceCm;
}

/**
  * 函    数：TIM9捕获中断（与TIM1刹车共用向量）
  * 说    明：上升沿记时刻；下降沿算脉宽并换算距离cm=us/58
  */
void TIM1_BRK_TIM9_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM9, TIM_IT_CC1) != RESET)
	{
		if (GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_5))	//Echo变高：回波起点
		{
			CaptureStart = TIM_GetCapture1(TIM9);
		}
		else											//Echo变低：回波结束
		{
			uint16_t WidthUs = TIM_GetCapture1(TIM9) - CaptureStart;
			if (WidthUs > 100 && WidthUs < 38000)		//有效范围约2cm~6.5m，滤毛刺
			{
				UltrasonicDistanceCm = WidthUs / 58;
			}
			else
			{
				UltrasonicDistanceCm = 999;				//异常脉宽视为无回波
			}
		}
		TIM_ClearITPendingBit(TIM9, TIM_IT_CC1);
	}
}
