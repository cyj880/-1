#ifndef __SPI_H
#define __SPI_H

#include "stm32f4xx.h"

void SPI2_Init(void);
uint8_t SPI2_ReadWriteByte(uint8_t TxData);
void SPI2_SetSpeed(uint8_t SPI_BaudRatePrescaler);

#endif
