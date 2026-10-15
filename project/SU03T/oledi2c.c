#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include "oled_front.h"
#include "oled.h"
#include <wiringPi.h> 
 
int i2c_fd=-1;
#define OLED_ADDR 0x3C

static int i2c_write_bytes(uint8_t slave_addr, uint8_t *buf, int len)
{
    if(i2c_fd < 0) return -1;
    if(ioctl(i2c_fd, I2C_SLAVE, slave_addr) < 0){
        perror("ioctl slave");
        return -1;
    }
    return write(i2c_fd, buf, len);
}

/* OLED写命令 */
void OLED_WriteCommand(uint8_t Command)
{
    uint8_t buf[2] = {0x00, Command};
    i2c_write_bytes(OLED_ADDR, buf, 2);
}

/* OLED写数据 */
void OLED_WriteData(uint8_t Data)
{
    uint8_t buf[2] = {0x40, Data};
    i2c_write_bytes(OLED_ADDR, buf, 2);
}

/* OLED设置光标位置 */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

/* OLED清屏 */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for(i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/* OLED显示一个字符 8*16 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i]);
    }
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
    }
}

/* OLED显示字符串 */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

/* OLED显示无符号数字 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/* OLED初始化 */
void OLED_Init(void)
{
    i2c_fd = open("/dev/i2c-3", O_RDWR);
    if(i2c_fd < 0){
        perror("open /dev/i2c-3");
        return;
    }
    usleep(100000);

    OLED_WriteCommand(0xAE);
    OLED_WriteCommand(0xD5); OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8); OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3); OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA1);
    OLED_WriteCommand(0xC8);
    OLED_WriteCommand(0xDA); OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81); OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9); OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB); OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);
    OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0x8D); OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);

    OLED_Clear();
}

#define ADS1115_ADDR 0x48

float ADS1115_Read_AIN0(void)
{
    if(i2c_fd < 0)
        return -1.0f;

    if(ioctl(i2c_fd, I2C_SLAVE, ADS1115_ADDR) < 0)
    {
        perror("ADS1115 ioctl");
        return -1.0f;
    }

    // 0xC1：PGA=±4.096V，AIN0单端，单次转换
    //配置值 0xC3E3 的含义（二进制：1100 0011 1110 0011）：
    uint8_t cfg_buf[3] = {0x01, 0xC3, 0xE3}; // 寄存器地址0x01，高字节0xC3，低字节0xE3
     write(i2c_fd, cfg_buf, 3);
     usleep(50000);

    uint8_t reg = 0x00;
    write(i2c_fd, &reg, 1);//将 ADS1115 的内部寄存器指针指向转换结果寄存器（地址 0x00），为后续读取做准备

    uint8_t data[2];
    read(i2c_fd, data, 2);

    int16_t raw = (int16_t)((data[0] << 8) | data[1]);
    // ±4.096V：1LSB = 0.125 mV
    float volt_div = raw * 0.000125f;
    // 1/2分压，乘以2还原MQ3真实输出电压
    float vout_real = volt_div * 2.0f;

    return vout_real;
}



