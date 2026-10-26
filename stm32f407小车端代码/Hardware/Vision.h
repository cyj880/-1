#ifndef __VISION_H
#define __VISION_H

#include "stm32f4xx.h"

/*
 * MaixCAM视觉模块串口驱动（USART3，PD8=TX，PD9=RX，115200，中断+环形缓冲）
 * 帧协议（13字节/帧，小端）：
 *   AA 55 | x(int16) y(int16) w(uint16) h(uint16) count(uint16) | sum校验
 *   x,y=主目标中心像素坐标；count=本帧目标数（0=无目标）
 *   sum校验 = 前10字节 payload 累加和的低8位
 */

void Vision_Init(void);				//USART3硬件初始化
void Vision_Parse(void);			//解析接收缓冲中的字节（任务周期调用）
uint8_t Vision_GotTarget(void);		//最近有效帧是否有目标
uint8_t Vision_Online(void);		//1=最近1秒内收到过有效帧（模块通信正常）
int16_t Vision_GetX(void);			//目标中心x（像素）
int16_t Vision_GetY(void);			//目标中心y（像素）
uint16_t Vision_GetW(void);			//目标框宽
uint16_t Vision_GetH(void);			//目标框高
uint16_t Vision_GetNum(void);		//本帧目标总数

#endif
