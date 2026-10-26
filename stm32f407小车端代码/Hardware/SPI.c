#include "stm32f4xx.h"                  // Device header
#include "SPI.h"

/**
  * 函    数：SPI2初始化（主机模式）
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用SPI2硬件外设，引脚固定为SCK->PB13、MISO->PB14、MOSI->PB15（与小车原理图一致）
  *           直接按NRF24L01要求的SPI模式0（CPOL=0/CPHA=1边沿）初始化
  *           SPI2挂在APB1上（42MHz），8分频后为5.25MHz，低于NRF24L01的10MHz上限
  */
void SPI2_Init(void)
{
	/*开启时钟*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);	//开启GPIOB的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);	//开启SPI2的时钟

	/*GPIO初始化：PB13/14/15复用为SPI2*/
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;			//复用功能模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;			//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;			//上拉，NRF未选中时MISO保持高电平
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/*SPI初始化*/
	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;		//双线双向全双工
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;							//主机模式
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;						//8位帧结构
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;								//时钟悬空低电平
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;							//第一个时钟沿采样
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;								//NSS信号由软件控制
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;	//先按最低速率初始化，收发前再切换
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;						//从MSB位开始传输
	SPI_InitStructure.SPI_CRCPolynomial = 7;								//CRC多项式，复位值
	SPI_Init(SPI2, &SPI_InitStructure);
	SPI_Cmd(SPI2, ENABLE);													//使能SPI2
}

/**
  * 函    数：SPI2读写一个字节
  * 参    数：TxData 要写入（发送）的字节
  * 返 回 值：读取到的字节
  */
uint8_t SPI2_ReadWriteByte(uint8_t TxData)
{
	uint8_t retry = 0;
	/*等待发送缓冲区空（TXE置1）*/
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET)
	{
		retry++;
		if (retry > 200) return 0;
	}
	SPI_I2S_SendData(SPI2, TxData);			//发送一个数据
	retry = 0;
	/*等待接收缓冲区非空（RXNE置1）*/
	while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET)
	{
		retry++;
		if (retry > 200) return 0;
	}
	return SPI_I2S_ReceiveData(SPI2);		//返回接收到的数据
}

/**
  * 函    数：设置SPI2速度
  * 参    数：SPI_BaudRatePrescaler 波特率预分频值（SPI_BaudRatePrescaler_2~_256）
  * 返 回 值：无
  */
void SPI2_SetSpeed(uint8_t SPI_BaudRatePrescaler)
{
	SPI2->CR1 &= 0XFFC7;					//位3-5清零，用来设置波特率
	SPI2->CR1 |= SPI_BaudRatePrescaler;		//设置SPI2速度
	SPI_Cmd(SPI2, ENABLE);					//使能SPI2
}
