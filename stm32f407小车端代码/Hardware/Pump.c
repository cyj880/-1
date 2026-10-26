#include "stm32f4xx.h"                  // Device header
#include "Pump.h"

/*
 * 抽水泵调速驱动（TB6612通道A，PC9 -> PWMA）：
 * TIM8_CH4输出20kHz PWM，占空比0~100%对应泵转速（超出听觉频段，无啸叫）。
 * 方向脚接固定电平：AIN1接3.3V、AIN2接GND（固定正转），STBY接3.3V使能，
 * 软件只需控制PWMA这一个引脚。泵接AO1/AO2，TB6612自带续流二极管，
 * 不需要外接SS34。上电默认占空比0（泵关闭）。
 * 泵的转向由AIN1/AIN2电平决定（对调即反转），软件只调快慢。
 */

/*转速档位表：低/中/高（想改手感直接改这几个数）*/
static const uint8_t PumpSpeedTable[PUMP_LEVEL_NUM] = {40, 70, 100};

static uint8_t PumpLevel = 1;		//当前档位下标（默认中速70%）
static uint8_t PumpDuty = 0;		//当前实际占空比

/**
  * 函    数：水泵PWM初始化（TIM8_CH4 -> PC9，1kHz）
  * 参    数：无
  * 返 回 值：无
  */
void Pump_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);	//开启TIM8的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);	//开启GPIOC的时钟

	/*GPIO初始化：PC9复用为TIM8_CH4*/
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_TIM8);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	/*时基：168MHz不分频，ARR=8400-1 → PWM频率20kHz，1%占空比=84个计数*/
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1 - 1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 8400 - 1;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStructure);

	/*输出比较：PWM1模式，CNT<CCR输出高，占空比=CCR/8400*/
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC4Init(TIM8, &TIM_OCInitStructure);				//通道4 -> PC9
	TIM_OC4PreloadConfig(TIM8, TIM_OCPreload_Enable);

	TIM_SetCompare4(TIM8, 0);								//上电默认泵关闭

	TIM_ARRPreloadConfig(TIM8, ENABLE);
	TIM_CtrlPWMOutputs(TIM8, ENABLE);						//高级定时器必须：使能主输出
	TIM_Cmd(TIM8, ENABLE);
}

/**
  * 函    数：设置泵占空比
  * 参    数：Duty 占空比0~100，0为停止
  * 返 回 值：无
  */
void Pump_SetDuty(uint8_t Duty)
{
	if (Duty > 100) Duty = 100;
	PumpDuty = Duty;
	TIM_SetCompare4(TIM8, (uint16_t)Duty * 84);				//1%~100% -> CCR 84~8400
}

/**
  * 函    数：开/关泵（按当前档位转速）
  * 返 回 值：无
  */
void Pump_Toggle(void)
{
	if (PumpDuty > 0) Pump_SetDuty(0);						//开着 -> 关
	else Pump_SetDuty(PumpSpeedTable[PumpLevel]);			//关着 -> 按当前档位开
}

/**
  * 函    数：切换转速档位（低→中→高循环）
  * 说    明：泵开着时立即生效；关着时只改档位，下次开泵按新档位转
  * 返 回 值：无
  */
void Pump_NextLevel(void)
{
	PumpLevel = (PumpLevel + 1) % PUMP_LEVEL_NUM;
	if (PumpDuty > 0) Pump_SetDuty(PumpSpeedTable[PumpLevel]);
}

/**
  * 函    数：按档位设置泵（遥控器喷水命令/APP手动控制共用）
  * 参    数：Level 0=关 1=低40% 2=中70% 3=高100%
  * 返 回 值：无
  */
void Pump_SetLevel(uint8_t Level)
{
	if (Level > 3) Level = 0;
	if (Level == 0)
	{
		Pump_SetDuty(0);
	}
	else
	{
		PumpLevel = Level - 1;					//同步档位下标，KEY2切换从该档继续
		Pump_SetDuty(PumpSpeedTable[PumpLevel]);
	}
}

/**
  * 函    数：获取当前占空比（OLED显示/上报用）
  * 返 回 值：0~100
  */
uint8_t Pump_GetDuty(void)
{
	return PumpDuty;
}
