#include "stm32f4xx.h"                  // Device header
#include "Servo.h"

/*
 * 二维云台舵机驱动：TIM1两路PWM
 * 舵机1（云台水平）：PE9  = TIM1_CH1
 * 舵机2（云台俯仰）：PE11 = TIM1_CH2
 *
 * 标准舵机要求：50Hz（20ms周期），高电平脉宽0.5ms~2.5ms对应0°~180°
 * TIM1时钟168MHz（APB2），预分频168后计数频率1MHz（1计数=1µs），
 * 自动重装载20000-1即20ms周期，比较值即高电平脉宽（单位µs）。
 * TIM1是高级定时器，必须调用TIM_CtrlPWMOutputs使能主输出（MOE）。
 */

/**
  * 函    数：舵机初始化（TIM1双路PWM，50Hz）
  * 参    数：无
  * 返 回 值：无
  */
void Servo_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);	//开启TIM1的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);	//开启GPIOE的时钟

	/*GPIO初始化：PE9/PE11复用为TIM1*/
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_TIM1);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;			//复用功能模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;			//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;		//不上拉不下拉
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/*时基初始化：1µs一个计数，20ms一个周期（50Hz）*/
	TIM_TimeBaseInitStructure.TIM_Prescaler = 168 - 1;		//预分频：168MHz/168 = 1MHz
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;		//向上计数
	TIM_TimeBaseInitStructure.TIM_Period = 20000 - 1;		//自动重装载：20000µs = 20ms
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

	/*输出比较初始化：PWM1模式，CNT<CCR时输出高电平，脉宽=CCR值*/
	TIM_OCStructInit(&TIM_OCInitStructure);					//先填默认值（高级定时器结构体成员多）
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	//高电平为有效脉宽
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);				//通道1（PE9）
	TIM_OC2Init(TIM1, &TIM_OCInitStructure);				//通道2（PE11）
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);		//使能CCR1预装载
	TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);		//使能CCR2预装载

	/*上电默认90°（脉宽1.5ms），云台回中*/
	TIM_SetCompare1(TIM1, 1500);
	TIM_SetCompare2(TIM1, 1500);

	TIM_ARRPreloadConfig(TIM1, ENABLE);
	TIM_CtrlPWMOutputs(TIM1, ENABLE);						//高级定时器必须：使能主输出
	TIM_Cmd(TIM1, ENABLE);									//使能TIM1
}

/*当前角度记忆（对靶控制需要知道云台现在指哪里）*/
static uint8_t ServoAngle1 = 90;		//水平舵机当前角度
static uint8_t ServoAngle2 = 90;		//俯仰舵机当前角度

/**
  * 函    数：设置舵机1角度（云台水平，PE9）
  * 参    数：Angle 角度0~180
  * 返 回 值：无
  */
void Servo_SetAngle1(uint8_t Angle)
{
	ServoAngle1 = Angle;
	TIM_SetCompare1(TIM1, 500 + (uint16_t)Angle * 2000 / 180);	//0°=500µs，180°=2500µs
}

/**
  * 函    数：设置舵机2角度（云台俯仰，PE11）
  * 参    数：Angle 角度0~180
  * 返 回 值：无
  */
void Servo_SetAngle2(uint8_t Angle)
{
	ServoAngle2 = Angle;
	TIM_SetCompare2(TIM1, 500 + (uint16_t)Angle * 2000 / 180);	//0°=500µs，180°=2500µs
}

/**
  * 函    数：读取舵机当前角度（对靶增量控制用）
  */
uint8_t Servo_GetAngle1(void)	{ return ServoAngle1; }
uint8_t Servo_GetAngle2(void)	{ return ServoAngle2; }
