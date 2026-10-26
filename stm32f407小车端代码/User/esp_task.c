#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include "ESP8266.h"
#include "Battery.h"
#include "WaterLevel.h"
#include "Pump.h"
#include "nrf_task.h"
#include "sensor_task.h"
#include "vision_task.h"
#include "esp_task.h"

/*
 * ESP8266联网上报+APP下行控制任务（局域网TCP直连方案）：
 * 1. ESP8266以STA模式加入WiFi，主动连接手机端app开的TCP服务器
 * 2. 每秒上报一次状态JSON（字段见SendStatus），APP据此显示
 *    电池电量、药箱余量、病虫害数量、障碍距离、喷药状态等
 * 3. 接收APP下行命令（ESP8266.h头部有协议说明）：
 *    #SPRAY=0~3 手动控制喷药开关/档位（任务书"手动控制喷药开关"）
 *    #Q         查询，立即回一帧状态
 * 4. 联网失败只影响上报和APP控制，不影响小车本地控制
 *
 * 手机端测试：网络调试助手开TCP Server，接收框看JSON，
 * 发送框发"#SPRAY=2\n"（记得带换行）即可控制喷药。
 *
 * !! 使用前修改下面4个宏：WiFi名、WiFi密码、手机IP、端口 !!
 */

#define WIFI_SSID			"Redmi K60"			//WiFi名称（手机热点，2.4GHz）
#define WIFI_PASSWORD		"11118888"			//WiFi密码
#define TCP_SERVER_IP		"192.168.43.1"		//手机app（TCP服务器）的IP（安卓开热点时一般是192.168.43.1，以APP首屏显示为准）
#define TCP_SERVER_PORT		"8080"				//TCP服务器端口
#define UPLOAD_PERIOD_MS	1000				//上报周期

volatile uint8_t EspLinkStatus = 0;			//1=已连上TCP服务器，0=未连接/连接中

/*完整联网流程：AT握手 -> STA模式 -> 连WiFi -> 建TCP连接，成功返回1*/
static uint8_t ESP_Setup(void)
{
	uint8_t retry;

	for (retry = 0; retry < 5; retry++)					//模块上电启动需要1~2秒，多试几次AT
	{
		if (ESP8266_SendCmd("AT\r\n", "OK", 1000)) break;
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	if (retry >= 5) return 0;							//模块没应答：检查接线/供电/波特率

	ESP8266_SendCmd("ATE0\r\n", "OK", 1000);			//关回显，应答更干净（失败不致命）
	if (!ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 3000)) return 0;	//STA模式
	if (!ESP8266_SendCmd("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"\r\n",
						"GOT IP", 25000)) return 0;		//连WiFi等IP（路由器慢，超时给足）
	if (!ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"" TCP_SERVER_IP "\"," TCP_SERVER_PORT,
						"CONNECT", 15000)) return 0;	//连手机TCP服务器
	return 1;
}

/*上报一帧状态JSON，成功返回1（失败表示连接断了，任务会重连）*/
static uint8_t ESP_SendStatus(char *Msg)
{
	float Vbat = Battery_GetVoltage();
	uint16_t Len;

	Len = (uint16_t)sprintf(Msg,
		"{\"vbat\":%d.%d,\"bat\":%u,\"tank\":%u,\"pest\":%u,\"dist\":%u,\"ir\":%u,\"spray\":%u,\"link\":%u,\"rx\":%u}\r\n",
		(int)Vbat, (int)(Vbat * 10) % 10, Battery_GetPercent(), WaterLevel_GetPercent(),
		VisionPestCount, SensorDistanceCm, SensorIR1 | (SensorIR2 << 1),
		NrfSprayCmd, NrfLinkAlive, NrfRxCount);

	return ESP8266_SendData(Msg, Len);
}

/*处理APP下行命令，返回1=收到#Q需要立即回帧*/
static uint8_t ESP_HandleCommand(void)
{
	char Cmd[24];
	uint8_t Immediate = 0;

	while (ESP8266_GetCommand(Cmd))
	{
		if (strncmp(Cmd, "#SPRAY=", 7) == 0)			//手动控制喷药：#SPRAY=0~3
		{
			uint8_t Level = (uint8_t)(Cmd[7] - '0');
			if (Level <= 3)
			{
				Pump_SetLevel(Level);					//0关 1低 2中 3高
			}
		}
		else if (strcmp(Cmd, "#Q") == 0)				//查询：立即回一帧
		{
			Immediate = 1;
		}
	}
	return Immediate;
}

void EspTask(void *pvParameters)
{
	char Msg[128];

	for (;;)
	{
		EspLinkStatus = 0;

		if (!ESP_Setup())								//联网失败：3秒后重来
		{
			vTaskDelay(pdMS_TO_TICKS(3000));
			continue;
		}
		EspLinkStatus = 1;

		/*已连上：周期处理命令+上报，发送失败（断网/服务器关闭）则退出去重连*/
		for (;;)
		{
			uint8_t Immediate = ESP_HandleCommand();

			if (!ESP_SendStatus(Msg)) break;

			if (!Immediate)	vTaskDelay(pdMS_TO_TICKS(UPLOAD_PERIOD_MS));
		}
	}
}
