#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f4xx.h"

/*
 * ESP8266 WiFi模块驱动（AT固件，USART2，115200）
 * 接线（小车原理图ESP8266接口）：
 *   STM32 PA2(TX) -> ESP8266 RX
 *   STM32 PA3(RX) <- ESP8266 TX
 *   3.3V供电（注意ESP8266发射时峰值电流可达300mA以上，3.3V电源要足）
 *   若模块是ESP-01，CH_PD/EN引脚必须拉高模块才会启动
 *
 * 工作流程（由esp_task调用）：
 *   ESP8266_Init()                  USART2硬件初始化
 *   ESP8266_SendCmd()               发送一条AT指令并等待应答
 *   ESP8266_SendData()              AT+CIPSEND流程发送一段数据
 */

void ESP8266_Init(void);										//USART2硬件初始化
uint8_t ESP8266_SendCmd(const char *cmd, const char *ack, uint32_t timeout_ms);	//发AT指令并等应答，1成功/0超时或ERROR
uint8_t ESP8266_SendData(const char *data, uint16_t len);		//发送一段数据（需已建立TCP连接）
void ESP8266_SendString(const char *str);						//阻塞发送原始字符串（不经AT）
uint8_t ESP8266_GetCommand(char *out);							//取出一条APP下行命令（如"#SPRAY=2"），1=有

/*
 * APP下行命令协议（手机APP通过TCP发送，以#开头、\n结尾）：
 *   #SPRAY=0   关闭喷药
 *   #SPRAY=1/2/3  喷药低/中/高档（40%/70%/100%）
 *   #Q         查询，小车立即回一帧状态JSON
 * 模块收到TCP数据会以"+IPD,长度:数据"形式从串口吐出，驱动自动扫描提取。
 */

#endif
