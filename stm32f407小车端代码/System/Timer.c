#include "stm32f4xx.h"

/*
 * 1ms定时中断（原来用TIM3，TIM3让给电机A编码器后改用TIM7基本定时器）
 * TIM7挂在APB1上，定时器时钟84MHz：84分频=1MHz，1000计数=1ms
 */

/* 1ms定时中断初始化 */
void Timer_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);	//使能TIM7时钟

	TIM_TimeBaseInitStructure.TIM_Period = 1000 - 1;		//自动重装载值：1MHz/1000 = 1ms
	TIM_TimeBaseInitStructure.TIM_Prescaler = 84 - 1;		//预分频：84MHz/84 = 1MHz
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;

	TIM_TimeBaseInit(TIM7, &TIM_TimeBaseInitStructure);		//初始化TIM7

	TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE);				//允许TIM7更新中断
	TIM_Cmd(TIM7, ENABLE);									//使能TIM7

	NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;			//TIM7中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;	//抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;	//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/* TIM7中断服务函数在main.c中定义 */
