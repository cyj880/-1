#ifndef __ESP_TASK_H
#define __ESP_TASK_H

#include "stm32f4xx.h"

extern volatile uint8_t EspLinkStatus;		//1=已连上TCP服务器，0=未连接/连接中

void EspTask(void *pvParameters);			//ESP8266联网上报任务

#endif
