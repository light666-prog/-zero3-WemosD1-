#ifndef __mq2_H
#define __mq2_H
#include "oled.h"
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include "buzzer.h"
#include <stdint.h>

void MQ2_Init(void);
float MQ2_GetFire_ppm(void);
void MQ2_OLED_Refresh(void);




#endif