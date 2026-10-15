#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include "Usb.h"


#define SERIAL_DEV0 "/dev/ttyUSB0"

#define BAUD_RATE0  B115200
int serial_usb_init0(const char *dev) {
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("usb0 open serial");
        return -1;
    }

    struct termios opt;
    if (tcgetattr(fd, &opt) != 0) {
        perror("tcgetattr usb0");
        close(fd);
        return -1;
    }

    cfmakeraw(&opt);                     // 原始模式
    cfsetispeed(&opt, BAUD_RATE0);        // 输入波特率
    cfsetospeed(&opt, BAUD_RATE0);        // 输出波特率

    opt.c_cflag |= (CLOCAL | CREAD);     // 启用接收，忽略调制解调器线
    opt.c_cflag &= ~PARENB;              // 无校验
    opt.c_cflag &= ~CSTOPB;              // 1 位停止位
    opt.c_cflag &= ~CSIZE;
    opt.c_cflag |= CS8;                  // 8 位数据位
    opt.c_cflag &= ~CRTSCTS;             // 关闭硬件流控

    opt.c_cc[VMIN]  = 0;                 // 非阻塞读取
    opt.c_cc[VTIME] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &opt) != 0) {
        perror("tcsetattr usb0");
        close(fd);
        return -1;
    }

    printf("USB0串口 %s 初始化成功，波特率 115200\n", dev);
    return fd;
}

#define SERIAL_DEV1 "/dev/ttyUSB1"

#define BAUD_RATE1  B9600
int serial_usb_init1(const char *dev) {
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("usb1 open serial");
        return -1;
    }

    struct termios opt;
    if (tcgetattr(fd, &opt) != 0) {
        perror("tcgetattr usb0");
        close(fd);
        return -1;
    }

    cfmakeraw(&opt);                     // 原始模式
    cfsetispeed(&opt, BAUD_RATE1);        // 输入波特率
    cfsetospeed(&opt, BAUD_RATE1);        // 输出波特率

    opt.c_cflag |= (CLOCAL | CREAD);     // 启用接收，忽略调制解调器线
    opt.c_cflag &= ~PARENB;              // 无校验
    opt.c_cflag &= ~CSTOPB;              // 1 位停止位
    opt.c_cflag &= ~CSIZE;
    opt.c_cflag |= CS8;                  // 8 位数据位
    opt.c_cflag &= ~CRTSCTS;             // 关闭硬件流控

    opt.c_cc[VMIN]  = 0;                 // 非阻塞读取
    opt.c_cc[VTIME] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &opt) != 0) {
        perror("tcsetattr usb0");
        close(fd);
        return -1;
    }

    printf("USB1串口 %s 初始化成功，波特率9600\n", dev);
    return fd;
}