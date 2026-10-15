#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <errno.h>
#include "oled_front.h"
#include "oled.h"
#include <wiringPi.h> 
#include "uart.h"

#define AHT20_ADDR          0x38    // AHT20 I2C 地址（7位）
#define AHT20_CMD_INIT      0xBE    // 初始化命令
#define AHT20_CMD_TRIGGER   0xAC    // 触发测量命令
#define AHT20_CMD_SOFTRESET 0xBA    // 软复位命令

void Oled_Refresh_HUMI(float humidity);
void Oled_Refresh_TEMP(float temperature);
void UART_SendArray_Temp(int fd,uint8_t *tx_buffer, int size);
void UART_SendArray_Humi(int fd,uint8_t *tx_buffer, int size);


// 从 oled.c 中引用已打开的 I2C 文件描述符
extern int i2c_fd;

/* 向 I2C 设备写一个字节（不包含寄存器地址） */
static int aht20_write_byte(uint8_t cmd)
{
    if (i2c_fd < 0) return -1;
    if (ioctl(i2c_fd, I2C_SLAVE, AHT20_ADDR) < 0) {
        perror("AHT20 ioctl slave");
        return -1;
    }
    uint8_t buf[1] = {cmd};
    return write(i2c_fd, buf, 1);
}

/* 向 I2C 设备写多个字节 */
static int aht20_write_bytes(uint8_t *data, int len)
{
    if (i2c_fd < 0) return -1;
    if (ioctl(i2c_fd, I2C_SLAVE, AHT20_ADDR) < 0) {
        perror("AHT20 ioctl slave");
        return -1;
    }
    return write(i2c_fd, data, len);
}

/* 从 I2C 设备读取多个字节 */
static int aht20_read_bytes(uint8_t *buf, int len)
{
    if (i2c_fd < 0) return -1;
    if (ioctl(i2c_fd, I2C_SLAVE, AHT20_ADDR) < 0) {
        perror("AHT20 ioctl slave");
        return -1;
    }
    return read(i2c_fd, buf, len);
}

int AHT20_Init(void)
{
        if (i2c_fd < 0) {
    fprintf(stderr, "I2C bus not opened\n");
    return -1;
    }

    uint8_t status;
    
    // 软复位，确保传感器处于已知状态
    if (aht20_write_byte(AHT20_CMD_SOFTRESET) < 0) {
        fprintf(stderr, "AHT20 soft reset failed\n");
        return -1;
    }
    usleep(50000);  // 等待复位完成

    // 读取状态寄存器
    uint8_t buf[1];
    if (aht20_read_bytes(buf, 1) < 0) {
        fprintf(stderr, "AHT20 read status failed\n");
        return -1;
    }
    status = buf[0];

    // 检查是否需要校准（bit3 为 0 表示未校准）
    if (!(status & 0x08)) {
        // 发送初始化命令
        uint8_t init_cmd[2] = {AHT20_CMD_INIT, 0x08};
        if (aht20_write_bytes(init_cmd, 2) < 0) {
            fprintf(stderr, "AHT20 init command failed\n");
            return -1;
        }
        usleep(10000);  // 等待初始化完成

        // 再次读取状态，确认校准成功
        if (aht20_read_bytes(buf, 1) < 0) {
            fprintf(stderr, "AHT20 read status after init failed\n");
            return -1;
        }
        status = buf[0];
        if (!(status & 0x08)) {
            fprintf(stderr, "AHT20 calibration failed\n");
            return -1;
        }
    }
    return 0;
}

/* 触发一次测量并读取温湿度数据 */
int AHT20_Read_TEMP(float *temperature,int fd){
    // 触发测量命令：0xAC 0x33 0x00
    uint8_t trigger[3] = {AHT20_CMD_TRIGGER, 0x33, 0x00};
    if (aht20_write_bytes(trigger, 3) < 0) {
        fprintf(stderr, "AHT20 trigger failed\n");
        return -1;
    }

    // 等待测量完成（典型 80ms）
    usleep(80000);

    // 读取 6 字节数据（状态 + 湿度 + 温度）
    uint8_t data[6];
    if (aht20_read_bytes(data, 6) < 0) {
        fprintf(stderr, "AHT20 read data failed\n");
        return -1;
    }

    // 检查状态位（bit7 为忙标志，为 0 表示数据就绪）
    if (data[0] & 0x80) {
        fprintf(stderr, "AHT20 measurement not ready\n");
        return -1;
    }

    
    uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) |
                        ((uint32_t)data[4] << 8) |
                        (uint32_t)data[5];

    *temperature = (raw_temp * 200.0f) / 1048576.0f - 50.0f;

    float temp_float = *temperature; // 例如 27.0

// 2. 提取整数部分
// 注意：如果是负温度，这里需要处理绝对值
        uint8_t temp_int_part = (uint8_t)temp_float; // 27 -> 0x1B

// 3. 提取小数部分（假设接收端是按除以100来显示小数）
// 27.5 - 27 = 0.5，乘以100得到50 (0x32)
    uint8_t temp_dec_part = (uint8_t)((temp_float - temp_int_part) * 100); 


    uint8_t tx_buffer[7];
    tx_buffer[0] = 0xAA;         // 帧头1
    tx_buffer[1] = 0x55;         // 帧头2
    tx_buffer[2] = 0x01;         // 指令码（假设读取温度是01）
    tx_buffer[3] = temp_int_part; // 整数部分
    tx_buffer[4] = temp_dec_part; // 小数部分
    tx_buffer[5] = 0x55;         // 帧尾1
    tx_buffer[6] = 0xAA;         // 帧尾2

    // 5. 通过串口发送给模块
    UART_SendArray_Temp(fd,tx_buffer, 7); // 这里的函数名替换成你实际使用的串口发送函数


    Oled_Refresh_TEMP(*temperature);
       
    return 0;
}
void UART_SendArray_Temp(int fd,uint8_t *tx_buffer, int size){
    
       int bytes_written = 0; // 已经写入的字节数
        int ret;

    // 循环直到所有数据都发出去
    while (bytes_written < size) {
        // 尝试发送剩余的数据
        ret = write(fd, tx_buffer + bytes_written, size - bytes_written);

        if (ret == -1) {
            if (errno == EINTR) {
                // 如果被信号打断，继续重试
                continue; 
            }
            // 其他真正的错误，打印并退出
            perror("UART write error");
            return; 
        }
        
        // 累加已成功发送的字节数
        bytes_written += ret;
    }
    
    // 走到这里说明 size 个字节全部发送成功
    printf("发送成功，共发送 %d 字节\n", bytes_written);
}

void Oled_Refresh_TEMP(float temperature){
        
             char line1[17];
            snprintf(line1, sizeof(line1), "Temp: %.1f C", temperature);

            static int first_run=1;
            if(first_run){
            OLED_Clear();
            first_run=0;}
        
            OLED_ShowString(1, 1, line1);
            

            sleep(2);
            printf("%s\n", line1);
}

int AHT20_Read_HUMI(float *humidity,int fd)
{
    // 触发测量命令：0xAC 0x33 0x00
    uint8_t trigger[3] = {AHT20_CMD_TRIGGER, 0x33, 0x00};
    if (aht20_write_bytes(trigger, 3) < 0) {
        fprintf(stderr, "AHT20 trigger failed\n");
        return -1;
    }

    // 等待测量完成（典型 80ms）
    usleep(80000);

    // 读取 6 字节数据（状态 + 湿度 + 温度）
    uint8_t data[6];
    if (aht20_read_bytes(data, 6) < 0) {
        fprintf(stderr, "AHT20 read data failed\n");
        return -1;
    }

    // 检查状态位（bit7 为忙标志，为 0 表示数据就绪）
    if (data[0] & 0x80) {
        fprintf(stderr, "AHT20 measurement not ready\n");
        return -1;
    }

    // 计算湿度（20位原始值）
    uint32_t raw_humi = ((uint32_t)data[1] << 12) |
                        ((uint32_t)data[2] << 4) |
                        ((uint32_t)data[3] >> 4);
    *humidity = (raw_humi * 100.0f) / 1048576.0f;  // 2^20 = 1048576

   
    float humi_float = *humidity; // 例如 27.0


        uint8_t humi_int_part = (uint8_t)humi_float; // 27 -> 0x1B


    uint8_t humi_dec_part = (uint8_t)((humi_float - humi_int_part) * 100); 


    uint8_t tx_buffer[7];
    tx_buffer[0] = 0xAA;         // 帧头1
    tx_buffer[1] = 0x55;         // 帧头2
    tx_buffer[2] = 0x02;         // 指令码（假设读取温度是01）
    tx_buffer[3] = humi_int_part; // 整数部分
    tx_buffer[4] = humi_dec_part; // 小数部分
    tx_buffer[5] = 0x55;         // 帧尾1
    tx_buffer[6] = 0xAA;         // 帧尾2

    // 5. 通过串口发送给模块
    UART_SendArray_Humi(fd,tx_buffer, 7); // 这里的函数名替换成你实际使用的串口发送函数

    Oled_Refresh_HUMI(*humidity);
       
    return 0;
}
void Oled_Refresh_HUMI(float humidity){
        
             char line2[17];
            snprintf(line2, sizeof(line2), "Humi: %.1f %%", humidity);

            static int first_run=1;
            if(first_run){
            OLED_Clear();
            first_run=0;}
        
            OLED_ShowString(2, 1, line2);
        
            sleep(2);
            printf("%s\n",line2);
}

void UART_SendArray_Humi(int fd,uint8_t *tx_buffer, int size){
    
       int bytes_written = 0; // 已经写入的字节数
        int ret;

    // 循环直到所有数据都发出去
    while (bytes_written < size) {
        // 尝试发送剩余的数据
        ret = write(fd, tx_buffer + bytes_written, size - bytes_written);

        if (ret == -1) {
            if (errno == EINTR) {
                // 如果被信号打断，继续重试
                continue; 
            }
            // 其他真正的错误，打印并退出
            perror("UART write error");
            return; 
        }
        
        // 累加已成功发送的字节数
        bytes_written += ret;
    }
    
    // 走到这里说明 size 个字节全部发送成功
    printf("发送成功，共发送 %d 字节\n", bytes_written);
}
