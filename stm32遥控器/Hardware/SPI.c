#include "stm32f10x.h"                  // Device header
#include "SPI.h"

/**
  * 函    数：SPI1初始化（主机模式）
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用SPI1硬件外设，引脚固定为SCK->PA5、MISO->PA6、MOSI->PA7（与NRF24L01参考例程一致）
  *           此处先按参考例程的参数初始化，NRF24L01_Init中会按模式0（CPOL=0/CPHA=1边沿）重新配置
  */
void SPI1_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);	//开启SPI1的时钟

	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//复用推挽输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7);	//PA5/6/7先置高电平

	/*SPI初始化*/
	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;		//双线双向全双工
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;							//主机模式
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;						//8位帧结构
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;								//时钟悬空高电平（与参考例程一致）
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;							//第二个跳变沿采样
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;								//NSS信号由软件控制
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;	//波特率预分频256，最低速率
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;						//从MSB位开始传输
	SPI_InitStructure.SPI_CRCPolynomial = 7;								//CRC多项式，复位值
	SPI_Init(SPI1, &SPI_InitStructure);
	SPI_Cmd(SPI1, ENABLE);													//使能SPI1
}

/**
  * 函    数：SPI1读写一个字节
  * 参    数：TxData 要写入（发送）的字节
  * 返 回 值：读取到的字节
  */
uint8_t SPI1_ReadWriteByte(uint8_t TxData)
{
	uint8_t retry = 0;
	/*等待发送缓冲区空（TXE置1）*/
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET)
	{
		retry++;
		if (retry > 200) return 0;
	}
	SPI_I2S_SendData(SPI1, TxData);			//发送一个数据
	retry = 0;
	/*等待接收缓冲区非空（RXNE置1）*/
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET)
	{
		retry++;
		if (retry > 200) return 0;
	}
	return SPI_I2S_ReceiveData(SPI1);		//返回接收到的数据
}

/**
  * 函    数：设置SPI1速度
  * 参    数：SPI_BaudRatePrescaler 波特率预分频值（SPI_BaudRatePrescaler_2~_256）
  * 返 回 值：无
  */
void SPI1_SetSpeed(uint8_t SPI_BaudRatePrescaler)
{
	SPI1->CR1 &= 0XFFC7;					//位3-5清零，用来设置波特率
	SPI1->CR1 |= SPI_BaudRatePrescaler;		//设置SPI1速度
	SPI_Cmd(SPI1, ENABLE);					//使能SPI1
}
