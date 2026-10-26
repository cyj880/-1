#include "stm32f10x.h"                  // Device header
#include "NRF24L01.h"
#include "SPI.h"

/*
 * 本驱动移植自参考例程《STM32F103遥控器程序源码》的APP/NRF24L01，
 * 引脚与例程完全一致（CE->PB13 CSN->PB14 IRQ->PB15，SPI1 PA5/6/7），
 * 仅将u8改为uint8_t、去掉串口打印和LED依赖，功能与例程相同。
 */

const uint8_t TX_ADDRESS[TX_ADR_WIDTH] = {0x34, 0x43, 0x10, 0x10, 0x01};	//发送地址
const uint8_t RX_ADDRESS[RX_ADR_WIDTH] = {0x34, 0x43, 0x10, 0x10, 0x01};	//接收地址

/**
  * 函    数：NRF24L01初始化
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;

	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	//使能PB端口时钟

	/*PB12推挽输出上拉，防止干扰（与参考例程一致）*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_12);

	/*PB13(CE)、PB14(CSN)推挽输出，初始为低*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_Pin_13 | GPIO_Pin_14);

	/*PB15(IRQ)下拉输入*/
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_Pin_15);

	/*初始化SPI1，并按NRF24L01要求的SPI模式0重新配置*/
	SPI1_Init();

	SPI_Cmd(SPI1, DISABLE);													//SPI外设不使能

	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;		//双线双向全双工
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;							//主机
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;						//8位帧结构
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;								//时钟悬空低
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;							//数据捕获于第1个时钟沿
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;								//NSS信号由软件控制
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;		//波特率预分频16
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;						//数据传输从MSB位开始
	SPI_InitStructure.SPI_CRCPolynomial = 7;								//CRC值计算的多项式
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, ENABLE);													//使能SPI外设

	NRF24L01_CE = 0;			//CE拉低
	NRF24L01_CSN = 1;			//SPI片选取消
}

/**
  * 函    数：检测NRF24L01是否存在
  * 参    数：无
  * 返 回 值：0，成功；1，失败
  */
uint8_t NRF24L01_Check(void)
{
	uint8_t buf[5] = {0XA5, 0XA5, 0XA5, 0XA5, 0XA5};
	uint8_t i;
	SPI1_SetSpeed(SPI_BaudRatePrescaler_8);			//spi速度为9Mhz（24L01的最大SPI时钟为10Mhz）
	NRF24L01_Write_Buf(NRF_WRITE_REG + TX_ADDR, buf, 5);	//写入5个字节的地址
	NRF24L01_Read_Buf(TX_ADDR, buf, 5);				//读出写入的地址
	for (i = 0; i < 5; i++) if (buf[i] != 0XA5) break;
	if (i != 5) return 1;							//检测24L01错误
	return 0;										//检测到24L01
}

/**
  * 函    数：NRF24L01写寄存器
  * 参    数：reg 指定寄存器地址  value 写入的值
  * 返 回 值：status 状态值
  */
uint8_t NRF24L01_Write_Reg(uint8_t reg, uint8_t value)
{
	uint8_t status;
	NRF24L01_CSN = 0;								//使能SPI传输
	status = SPI1_ReadWriteByte(reg);				//发送寄存器号
	SPI1_ReadWriteByte(value);						//写入寄存器的值
	NRF24L01_CSN = 1;								//禁止SPI传输
	return status;									//返回状态值
}

/**
  * 函    数：NRF24L01读寄存器
  * 参    数：reg 指定寄存器地址
  * 返 回 值：reg_val 寄存器内容
  */
uint8_t NRF24L01_Read_Reg(uint8_t reg)
{
	uint8_t reg_val;
	NRF24L01_CSN = 0;								//使能SPI传输
	SPI1_ReadWriteByte(reg);						//发送寄存器号
	reg_val = SPI1_ReadWriteByte(0XFF);				//读取寄存器内容
	NRF24L01_CSN = 1;								//禁止SPI传输
	return reg_val;									//返回状态值
}

/**
  * 函    数：NRF24L01在指定位置读出指定长度的数据
  * 参    数：reg 指定寄存器地址  *pBuf 数据指针  len 数据长度
  * 返 回 值：status 此次读到的状态寄存器值
  */
uint8_t NRF24L01_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len)
{
	uint8_t status, u8_ctr;
	NRF24L01_CSN = 0;								//使能SPI传输
	status = SPI1_ReadWriteByte(reg);				//发送寄存器值(位置),并读取状态值
	for (u8_ctr = 0; u8_ctr < len; u8_ctr++) pBuf[u8_ctr] = SPI1_ReadWriteByte(0XFF);	//读出数据
	NRF24L01_CSN = 1;								//关闭SPI传输
	return status;									//返回读到的状态值
}

/**
  * 函    数：NRF24L01在指定位置写出指定长度的数据
  * 参    数：reg 指定寄存器地址  *pBuf 数据指针  len 数据长度
  * 返 回 值：status 此次写入的状态寄存器值
  */
uint8_t NRF24L01_Write_Buf(uint8_t reg, uint8_t *pBuf, uint8_t len)
{
	uint8_t status, u8_ctr;
	NRF24L01_CSN = 0;								//使能SPI传输
	status = SPI1_ReadWriteByte(reg);				//发送寄存器值(位置),并读取状态值
	for (u8_ctr = 0; u8_ctr < len; u8_ctr++) SPI1_ReadWriteByte(*pBuf++);	//写入数据
	NRF24L01_CSN = 1;								//关闭SPI传输
	return status;									//返回读到的状态值
}

/**
  * 函    数：启动NRF24L01发送一次数据
  * 参    数：txbuf 待发送的数据首地址
  * 返 回 值：TX_OK 发送完成；MAX_TX 达到最大重发次数；0xff 其他错误
  */
uint8_t NRF24L01_TxPacket(uint8_t *txbuf)
{
	uint8_t sta;
	SPI1_SetSpeed(SPI_BaudRatePrescaler_8);			//spi速度为9Mhz（24L01的最大SPI时钟为10Mhz）
	NRF24L01_CE = 0;
	NRF24L01_Write_Buf(WR_TX_PLOAD, txbuf, TX_PLOAD_WIDTH);	//写数据到TX BUF  32个字节
	NRF24L01_CE = 1;								//启动发送
	while (NRF24L01_IRQ != 0);						//等待发送完成
	sta = NRF24L01_Read_Reg(STATUS);				//读取状态寄存器的值
	NRF24L01_Write_Reg(NRF_WRITE_REG + STATUS, sta);	//清除TX_DS或MAX_RT中断标志
	if (sta & MAX_TX)								//达到最大重发次数
	{
		NRF24L01_Write_Reg(FLUSH_TX, 0xff);			//清除TX FIFO寄存器
		return MAX_TX;
	}
	if (sta & TX_OK)								//发送完成
	{
		return TX_OK;
	}
	return 0xff;									//其他原因发送失败
}

/**
  * 函    数：启动NRF24L01接收一次数据
  * 参    数：rxbuf 待接收数据首地址
  * 返 回 值：0 接收完成；其他 没收到任何数据
  */
uint8_t NRF24L01_RxPacket(uint8_t *rxbuf)
{
	uint8_t sta;
	SPI1_SetSpeed(SPI_BaudRatePrescaler_8);			//spi速度为9Mhz（24L01的最大SPI时钟为10Mhz）
	sta = NRF24L01_Read_Reg(STATUS);				//读取状态寄存器的值
	NRF24L01_Write_Reg(NRF_WRITE_REG + STATUS, sta);	//清除TX_DS或MAX_RT中断标志
	if (sta & RX_OK)								//接收到数据
	{
		NRF24L01_Read_Buf(RD_RX_PLOAD, rxbuf, RX_PLOAD_WIDTH);	//读取数据
		NRF24L01_Write_Reg(FLUSH_RX, 0xff);			//清除RX FIFO寄存器
		return 0;
	}
	return 1;										//没收到任何数据
}

/**
  * 函    数：初始化NRF24L01到RX模式
  * 说    明：设置RX地址,写RX数据宽度,选择RF频道,波特率和LNA HCURR
  *           当CE变高后,即进入RX模式,并可以接收数据了
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_RX_Mode(void)
{
	NRF24L01_CE = 0;
	NRF24L01_Write_Buf(NRF_WRITE_REG + RX_ADDR_P0, (uint8_t*)RX_ADDRESS, RX_ADR_WIDTH);	//写RX节点地址

	NRF24L01_Write_Reg(NRF_WRITE_REG + EN_AA, 0x01);		//使能通道0的自动应答
	NRF24L01_Write_Reg(NRF_WRITE_REG + EN_RXADDR, 0x01);	//使能通道0的接收地址
	NRF24L01_Write_Reg(NRF_WRITE_REG + RF_CH, 40);			//设置RF通信频率
	NRF24L01_Write_Reg(NRF_WRITE_REG + RX_PW_P0, RX_PLOAD_WIDTH);	//选择通道0的有效数据宽度
	NRF24L01_Write_Reg(NRF_WRITE_REG + RF_SETUP, 0x0f);		//设置TX发射参数,0db增益,2Mbps,低噪声增益开启
	NRF24L01_Write_Reg(NRF_WRITE_REG + CONFIG, 0x0f);		//配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,接收模式
	NRF24L01_CE = 1;										//CE为高,进入接收模式
}

/**
  * 函    数：初始化NRF24L01到TX模式
  * 说    明：设置TX自动应答的地址,填充TX发送数据,选择RF频道,波特率和
  *           LNA HCURR,PWR_UP,CRC使能,CE为高大于10us,则启动发送
  * 参    数：无
  * 返 回 值：无
  */
void NRF24L01_TX_Mode(void)
{
	NRF24L01_CE = 0;
	NRF24L01_Write_Buf(NRF_WRITE_REG + TX_ADDR, (uint8_t*)TX_ADDRESS, TX_ADR_WIDTH);	//写TX节点地址
	NRF24L01_Write_Buf(NRF_WRITE_REG + RX_ADDR_P0, (uint8_t*)RX_ADDRESS, RX_ADR_WIDTH);	//设置TX节点地址,主要为了使能ACK

	NRF24L01_Write_Reg(NRF_WRITE_REG + EN_AA, 0x01);		//使能通道0的自动应答
	NRF24L01_Write_Reg(NRF_WRITE_REG + EN_RXADDR, 0x01);	//使能通道0的接收地址
	NRF24L01_Write_Reg(NRF_WRITE_REG + SETUP_RETR, 0x1a);	//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
	NRF24L01_Write_Reg(NRF_WRITE_REG + RF_CH, 40);			//设置RF通道为40
	NRF24L01_Write_Reg(NRF_WRITE_REG + RF_SETUP, 0x0f);		//设置TX发射参数,0db增益,2Mbps,低噪声增益开启
	NRF24L01_Write_Reg(NRF_WRITE_REG + CONFIG, 0x0e);		//配置基本工作模式的参数;PWR_UP,EN_CRC,16BIT_CRC,发射模式
	NRF24L01_CE = 1;										//CE为高,10us后启动发送
}
