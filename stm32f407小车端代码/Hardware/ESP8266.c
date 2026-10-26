#include "stm32f4xx.h"                  // Device header
#include "ESP8266.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/*
 * ESP8266 AT固件驱动：
 * USART2收发（PA2/PA3），接收采用中断+环形缓冲，避免丢字节；
 * 应答等待由任务轮询实现（在任务上下文调用，内部用vTaskDelay让出CPU）。
 */

#define ESP_RX_BUF_SIZE		1024		//接收环形缓冲大小
#define ESP_RESP_BUF_SIZE	256			//应答累积缓冲大小
#define ESP_POLL_MS			10			//应答轮询间隔ms

static volatile uint8_t EspRxBuf[ESP_RX_BUF_SIZE];
static volatile uint16_t EspRxHead = 0, EspRxTail = 0;

static char EspRespBuf[ESP_RESP_BUF_SIZE];		//应答累积缓冲（仅任务上下文访问）
static uint16_t EspRespLen = 0;

/*---------------- APP下行命令扫描 ----------------
 * 从字节流中提取以'#'开头、'\n'结尾的命令行（如+IPD包裹的"#SPRAY=2\n"），
 * 存入小队列供esp_task取用。仅在任务上下文调用。*/
#define ESP_CMD_MAX			24		//单条命令最大长度
#define ESP_CMD_QUEUE_LEN	4		//命令队列深度

static char EspCmdQueue[ESP_CMD_QUEUE_LEN][ESP_CMD_MAX];
static uint8_t EspCmdHead = 0, EspCmdTail = 0;
static char EspCmdBuf[ESP_CMD_MAX];				//命令扫描暂存
static uint8_t EspCmdLen = 0;					//0=当前不在命令中

static void ESP_ScanByte(uint8_t Data)
{
	if (Data == '#')							//命令开始
	{
		EspCmdLen = 0;
		EspCmdBuf[EspCmdLen++] = (char)Data;
	}
	else if (EspCmdLen > 0)						//命令进行中
	{
		if (Data == '\n' || Data == '\r')		//命令结束
		{
			uint8_t Next;
			if (EspCmdLen > 1)					//至少"#x"两个字符才入队
			{
				EspCmdBuf[EspCmdLen] = '\0';
				Next = (EspCmdHead + 1) % ESP_CMD_QUEUE_LEN;
				if (Next != EspCmdTail)			//队列未满才写入
				{
					strcpy(EspCmdQueue[EspCmdHead], EspCmdBuf);
					EspCmdHead = Next;
				}
			}
			EspCmdLen = 0;
		}
		else if (EspCmdLen < ESP_CMD_MAX - 1)	//命令内容
		{
			EspCmdBuf[EspCmdLen++] = (char)Data;
		}
		else									//超长丢弃
		{
			EspCmdLen = 0;
		}
	}
}

/*---------------- USART2底层 ----------------*/

/**
  * 函    数：ESP8266初始化（USART2，115200，8N1，开接收中断）
  * 参    数：无
  * 返 回 值：无
  */
void ESP8266_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/*开启时钟*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);	//开启GPIOA的时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);	//开启USART2的时钟

	/*GPIO初始化：PA2复用为USART2_TX，PA3复用为USART2_RX*/
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;			//上拉（RX空闲保持高电平）
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/*USART初始化*/
	USART_InitStructure.USART_BaudRate = 115200;			//ESP8266 AT固件默认波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART2, &USART_InitStructure);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);			//开启接收非空中断
	USART_Cmd(USART2, ENABLE);								//使能USART2

	/*中断优先级：中断里只写环形缓冲，不调用FreeRTOS API*/
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/**
  * 函    数：阻塞发送字符串（原始字节流，不加帧尾）
  */
void ESP8266_SendString(const char *str)
{
	while (*str)
	{
		while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
		USART_SendData(USART2, (uint8_t)*str++);
	}
}

/**
  * 函    数：USART2接收中断服务函数：字节入环形缓冲
  */
void USART2_IRQHandler(void)
{
	if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
	{
		uint8_t Data = (uint8_t)USART_ReceiveData(USART2);	//读DR自动清RXNE标志
		uint16_t Next = (EspRxHead + 1) % ESP_RX_BUF_SIZE;
		if (Next != EspRxTail)								//缓冲未满才写入
		{
			EspRxBuf[EspRxHead] = Data;
			EspRxHead = Next;
		}
	}
	if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET)	//溢出错误，读DR清除
	{
		USART_ReceiveData(USART2);
	}
}

/*---------------- 应答等待 ----------------*/

/*从环形缓冲取一个字节，空返回-1（仅任务上下文调用）*/
static int16_t ESP_RxReadByte(void)
{
	if (EspRxHead == EspRxTail) return -1;
	uint8_t Data = EspRxBuf[EspRxTail];
	EspRxTail = (EspRxTail + 1) % ESP_RX_BUF_SIZE;
	return Data;
}

/**
  * 函    数：等待应答中出现指定关键字（任务上下文，内部vTaskDelay让出CPU）
  * 参    数：token 成功关键字；fail 失败关键字（可为NULL）；timeout_ms 超时
  * 返 回 值：1匹配成功，0失败或超时
  */
static uint8_t ESP_WaitAck(const char *token, const char *fail, uint32_t timeout_ms)
{
	uint32_t Waited = 0;

	EspRespLen = 0;
	EspRespBuf[0] = '\0';

	while (Waited < timeout_ms)
	{
		/*把环形缓冲里已有的字节搬进应答累积缓冲*/
		int16_t Data;
		while ((Data = ESP_RxReadByte()) >= 0)
		{
			if (EspRespLen < ESP_RESP_BUF_SIZE - 1)
			{
				EspRespBuf[EspRespLen++] = (char)Data;
				EspRespBuf[EspRespLen] = '\0';
			}
			ESP_ScanByte((uint8_t)Data);		//顺带扫描APP下行命令
		}
		if (strstr(EspRespBuf, token) != NULL) return 1;	//等到成功关键字
		if (fail != NULL && strstr(EspRespBuf, fail) != NULL) return 0;	//等到失败关键字
		vTaskDelay(pdMS_TO_TICKS(ESP_POLL_MS));
		Waited += ESP_POLL_MS;
	}
	return 0;												//超时
}

/**
  * 函    数：发送一条AT指令并等待应答
  * 参    数：cmd 完整指令（含\r\n）；ack 成功关键字；timeout_ms 超时
  * 返 回 值：1成功，0失败
  */
uint8_t ESP8266_SendCmd(const char *cmd, const char *ack, uint32_t timeout_ms)
{
	ESP8266_SendString(cmd);
	return ESP_WaitAck(ack, "ERROR", timeout_ms);
}

/**
  * 函    数：发送一段数据（需已建立TCP连接）
  * 流程：AT+CIPSEND=len -> 等'>' -> 发数据 -> 等SEND OK
  * 返 回 值：1成功，0失败（调用方可据此重连）
  */
uint8_t ESP8266_SendData(const char *data, uint16_t len)
{
	char Cmd[32];

	sprintf(Cmd, "AT+CIPSEND=%u\r\n", len);
	ESP8266_SendString(Cmd);
	if (!ESP_WaitAck(">", "ERROR", 3000)) return 0;			//等发送提示符

	ESP8266_SendString(data);
	return ESP_WaitAck("SEND OK", "SEND FAIL", 5000);
}

/**
  * 函    数：取出一条APP下行命令
  * 说    明：先把环形缓冲中积压的字节喂给命令扫描器，再从命令队列取一条
  * 返 回 值：1=取出成功（out为命令串，如"#SPRAY=2"），0=当前没有命令
  */
uint8_t ESP8266_GetCommand(char *out)
{
	int16_t Data;

	while ((Data = ESP_RxReadByte()) >= 0)		//排空接收缓冲，喂给命令扫描器
	{
		ESP_ScanByte((uint8_t)Data);
	}

	if (EspCmdHead != EspCmdTail)
	{
		strcpy(out, EspCmdQueue[EspCmdTail]);
		EspCmdTail = (EspCmdTail + 1) % ESP_CMD_QUEUE_LEN;
		return 1;
	}
	return 0;
}
