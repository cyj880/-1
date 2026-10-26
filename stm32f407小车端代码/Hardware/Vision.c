#include "stm32f4xx.h"                  // Device header
#include "Vision.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * MaixCAM视觉模块驱动：
 * USART3（PD8=TX，PD9=RX，AF7，115200-8N1），接收中断入环形缓冲，
 * 任务周期调用Vision_Parse()取字节并按帧头+校验解析，
 * 抗串口毛刺：错误的字节流最多浪费13字节即重新同步，校验错直接丢帧。
 */

#define VISION_RX_BUF_SIZE	512		//接收环形缓冲（13字节/帧×30fps=390B/s，足够）
#define VISION_FRAME_LEN	13		//帧总长：帧头2+载荷10+校验1

static volatile uint8_t VisionRxBuf[VISION_RX_BUF_SIZE];
static volatile uint16_t VisionRxHead = 0, VisionRxTail = 0;

/*最近一帧解析结果（仅任务上下文更新）*/
static int16_t FrameX = 0, FrameY = 0;
static uint16_t FrameW = 0, FrameH = 0, FrameNum = 0;
static TickType_t LastFrameTick = 0;

/*帧接收状态机*/
static uint8_t RxState = 0;			//0等帧头AA 1等帧头55 2收载荷
static uint8_t RxIdx = 0;
static uint8_t RxBuf[VISION_FRAME_LEN - 2];	//载荷10字节+校验1字节

/**
  * 函    数：视觉串口初始化（USART3，PD8/PD9）
  * 参    数：无
  * 返 回 值：无
  */
void Vision_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/*开启时钟*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);	//开启GPIOD的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);	//开启USART3的时钟

	/*GPIO初始化：PD8复用为USART3_TX，PD9复用为USART3_RX*/
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
	GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;			//上拉（RX空闲保持高电平）
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/*USART初始化：115200-8N1*/
	USART_InitStructure.USART_BaudRate = 115200;			//与MaixCAM程序一致
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART3, &USART_InitStructure);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);			//开启接收非空中断
	USART_Cmd(USART3, ENABLE);								//使能USART3

	/*中断优先级：中断里只写环形缓冲，不调用FreeRTOS API*/
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x02;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/**
  * 函    数：USART3接收中断：字节入环形缓冲
  */
void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		uint8_t Data = (uint8_t)USART_ReceiveData(USART3);	//读DR自动清RXNE标志
		uint16_t Next = (VisionRxHead + 1) % VISION_RX_BUF_SIZE;
		if (Next != VisionRxTail)							//缓冲未满才写入
		{
			VisionRxBuf[VisionRxHead] = Data;
			VisionRxHead = Next;
		}
	}
	if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET)	//溢出错误，读DR清除
	{
		USART_ReceiveData(USART3);
	}
}

/*从环形缓冲取一个字节，空返回-1（仅任务上下文调用）*/
static int16_t Vision_RxReadByte(void)
{
	if (VisionRxHead == VisionRxTail) return -1;
	uint8_t Data = VisionRxBuf[VisionRxTail];
	VisionRxTail = (VisionRxTail + 1) % VISION_RX_BUF_SIZE;
	return Data;
}

/**
  * 函    数：逐字节帧解析（帧头AA 55 -> 收载荷11字节 -> 校验）
  * 说    明：校验通过才更新坐标，校验失败整帧丢弃重新同步
  */
static void Vision_ParseByte(uint8_t Data)
{
	switch (RxState)
	{
		case 0:										//等帧头第1字节
			if (Data == 0xAA) RxState = 1;
			break;

		case 1:										//等帧头第2字节
			if (Data == 0x55) { RxState = 2; RxIdx = 0; }
			else RxState = 0;
			break;

		case 2:										//收载荷+校验共11字节
			RxBuf[RxIdx++] = Data;
			if (RxIdx >= sizeof(RxBuf))
			{
				RxState = 0;
				uint8_t Sum = 0;
				uint8_t i;
				for (i = 0; i < sizeof(RxBuf) - 1; i++)	//前10字节累加
				{
					Sum += RxBuf[i];
				}
				if (Sum == RxBuf[10])					//校验通过，发布本帧结果
				{
					FrameX = (int16_t)((uint16_t)RxBuf[0] | ((uint16_t)RxBuf[1] << 8));
					FrameY = (int16_t)((uint16_t)RxBuf[2] | ((uint16_t)RxBuf[3] << 8));
					FrameW = (uint16_t)RxBuf[4] | ((uint16_t)RxBuf[5] << 8);
					FrameH = (uint16_t)RxBuf[6] | ((uint16_t)RxBuf[7] << 8);
					FrameNum = (uint16_t)RxBuf[8] | ((uint16_t)RxBuf[9] << 8);
					LastFrameTick = xTaskGetTickCount();
				}
			}
			break;

		default:
			RxState = 0;
			break;
	}
}

/**
  * 函    数：解析接收缓冲中的所有字节（任务周期调用，10ms一次即可）
  */
void Vision_Parse(void)
{
	int16_t Data;
	while ((Data = Vision_RxReadByte()) >= 0)
	{
		Vision_ParseByte((uint8_t)Data);
	}
}

/*以下为结果查询接口*/

uint8_t Vision_GotTarget(void)
{
	return FrameNum > 0 ? 1 : 0;
}

uint8_t Vision_Online(void)
{
	return (xTaskGetTickCount() - LastFrameTick) < pdMS_TO_TICKS(1000) ? 1 : 0;
}

int16_t Vision_GetX(void)	{ return FrameX; }
int16_t Vision_GetY(void)	{ return FrameY; }
uint16_t Vision_GetW(void)	{ return FrameW; }
uint16_t Vision_GetH(void)	{ return FrameH; }
uint16_t Vision_GetNum(void)	{ return FrameNum; }
