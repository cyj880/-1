#include "stm32f4xx.h"                  // Device header
#include "Motor.h"

/*
 * TB6612电机驱动（轮趣驱动板接口）：
 * PWMA->PB8（TIM10_CH1） AIN1->PC0  AIN2->PC1  ——A电机
 * PWMB->PB9（TIM11_CH1） BIN1->PC2  BIN2->PC3  ——B电机
 * STBY->PC4（拉高使能驱动芯片）
 *
 * 说明：PWM原来用TIM4_CH3/CH4，TIM4让给电机B编码器（PD12/13）后，
 * PB8/PB9引脚不变，改用TIM10/TIM11（APB2，168MHz）。
 * PWM频率仍为20kHz（84分频=2MHz计数/100），占空比0~100即速度0~100。
 * TB6612真值表：xIN1=1,xIN2=0正转；xIN1=0,xIN2=1反转；00滑行停止；11刹车。
 * 若整车方向不对（前变后/左右反），优先对调电机接线，或在Motor_SetSpeed里改符号。
 */

/*电机A方向控制宏*/
#define MOTOR_AIN1(x)	((x) ? GPIO_SetBits(GPIOC, GPIO_Pin_0) : GPIO_ResetBits(GPIOC, GPIO_Pin_0))
#define MOTOR_AIN2(x)	((x) ? GPIO_SetBits(GPIOC, GPIO_Pin_1) : GPIO_ResetBits(GPIOC, GPIO_Pin_1))
/*电机B方向控制宏*/
#define MOTOR_BIN1(x)	((x) ? GPIO_SetBits(GPIOC, GPIO_Pin_2) : GPIO_ResetBits(GPIOC, GPIO_Pin_2))
#define MOTOR_BIN2(x)	((x) ? GPIO_SetBits(GPIOC, GPIO_Pin_3) : GPIO_ResetBits(GPIOC, GPIO_Pin_3))

/**
  * 函    数：TB6612初始化（TIM10/TIM11双路PWM + 方向引脚 + STBY）
  * 参    数：无
  * 返 回 值：无
  */
void Motor_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);	//开启TIM10的时钟（PWMA）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, ENABLE);	//开启TIM11的时钟（PWMB）
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);	//开启GPIOB的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);	//开启GPIOC的时钟

	/*PWM引脚：PB8/PB9分别复用为TIM10_CH1/TIM11_CH1*/
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_TIM10);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_TIM11);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*方向与STBY引脚：PC0~PC4推挽输出*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	MOTOR_AIN1(0); MOTOR_AIN2(0);			//上电默认滑行停止
	MOTOR_BIN1(0); MOTOR_BIN2(0);
	GPIO_SetBits(GPIOC, GPIO_Pin_4);		//STBY拉高，使能TB6612

	/*时基：TIM10/TIM11时钟168MHz，84分频=2MHz计数，ARR=100-1 → PWM频率20kHz，占空比=CCR(0~100)%*/
	TIM_TimeBaseInitStructure.TIM_Prescaler = 84 - 1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM10, &TIM_TimeBaseInitStructure);
	TIM_TimeBaseInit(TIM11, &TIM_TimeBaseInitStructure);

	/*输出比较：PWM1模式，高电平有效（TIM10/11没有BDTR，无需使能主输出）*/
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(TIM10, &TIM_OCInitStructure);			//TIM10通道1 -> PWMA（PB8）
	TIM_OC1Init(TIM11, &TIM_OCInitStructure);			//TIM11通道1 -> PWMB（PB9）
	TIM_OC1PreloadConfig(TIM10, TIM_OCPreload_Enable);
	TIM_OC1PreloadConfig(TIM11, TIM_OCPreload_Enable);

	TIM_SetCompare1(TIM10, 0);				//上电占空比0
	TIM_SetCompare1(TIM11, 0);

	TIM_ARRPreloadConfig(TIM10, ENABLE);
	TIM_ARRPreloadConfig(TIM11, ENABLE);
	TIM_Cmd(TIM10, ENABLE);					//使能TIM10
	TIM_Cmd(TIM11, ENABLE);					//使能TIM11
}

/**
  * 函    数：设置两个电机的速度和方向
  * 参    数：SpeedA A电机（左）速度 -100~100；SpeedB B电机（右）速度 -100~100
  *           正值正转，负值反转，0为滑行停止
  * 返 回 值：无
  */
void Motor_SetSpeed(int16_t SpeedA, int16_t SpeedB)
{
	/*A电机：PWMA=TIM10_CH1*/
	if (SpeedA > 0)			//正转
	{
		MOTOR_AIN1(1); MOTOR_AIN2(0);
		TIM_SetCompare1(TIM10, SpeedA);
	}
	else if (SpeedA < 0)	//反转
	{
		MOTOR_AIN1(0); MOTOR_AIN2(1);
		TIM_SetCompare1(TIM10, -SpeedA);
	}
	else					//滑行停止
	{
		MOTOR_AIN1(0); MOTOR_AIN2(0);
		TIM_SetCompare1(TIM10, 0);
	}

	/*B电机：PWMB=TIM11_CH1*/
	if (SpeedB > 0)
	{
		MOTOR_BIN1(1); MOTOR_BIN2(0);
		TIM_SetCompare1(TIM11, SpeedB);
	}
	else if (SpeedB < 0)
	{
		MOTOR_BIN1(0); MOTOR_BIN2(1);
		TIM_SetCompare1(TIM11, -SpeedB);
	}
	else
	{
		MOTOR_BIN1(0); MOTOR_BIN2(0);
		TIM_SetCompare1(TIM11, 0);
	}
}
