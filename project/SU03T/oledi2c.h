#ifndef __oled_H
#define __oled_H
#include <stdint.h>

void OLED_Init(void);
void OLED_Clear(void);
void OLED_SetCursor(uint8_t Y, uint8_t X);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
float ADS1115_Read_AIN0(void);

//float ADS1115_Read_AIN0(void);




#endif
