#ifndef __ath20_H
#define __ath20_H
#include <stdint.h>

int AHT20_Read_TEMP(float *temperature,int fd);
int AHT20_Read_HUMI(float *humidity,int fd);

int AHT20_Init(void);
void Oled_Refresh_TEMP(float temperature);
void Oled_Refresh_HUMI(float humidity);

void UART_SendArray_Temp(int fd,uint8_t *tx_buffer, int size);
void UART_SendArray_Humi(int fd,uint8_t *tx_buffer, int size);


#endif