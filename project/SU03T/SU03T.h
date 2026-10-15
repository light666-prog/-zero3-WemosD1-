#ifndef __SU03T_H
#define __SU03T_H
#include "uart.h"
#include "Control_led.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <wiringPi.h>
#include "ath20.h"
#include "oled.h"
#include "fan.h"
#include "Usb.h"
#include "mq2.h"

void send_voice_cmd(int fd,const unsigned char* cmd, int len);
void main_function();
#endif