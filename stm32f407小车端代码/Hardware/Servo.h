#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f4xx.h"

void Servo_Init(void);
void Servo_SetAngle1(uint8_t Angle);	//舵机1：PE9  = TIM1_CH1（云台水平），0~180
void Servo_SetAngle2(uint8_t Angle);	//舵机2：PE11 = TIM1_CH2（云台俯仰），0~180
uint8_t Servo_GetAngle1(void);			//读取水平舵机当前角度（对靶增量控制用）
uint8_t Servo_GetAngle2(void);			//读取俯仰舵机当前角度

#endif
