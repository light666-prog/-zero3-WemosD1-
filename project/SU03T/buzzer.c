#include <wiringPi.h>
#include <stdio.h>
#include <unistd.h>
#include "buzzer.h"
void buzzer_init(void){
    pinMode(10,OUTPUT); //设置io为输出模式
    digitalWrite(10,HIGH); //控制io口为高电平
}
void buzzer_on(void){
    digitalWrite(10,LOW); //控制io口为低电平

}
void buzzer_off(void){
    digitalWrite(10,HIGH); //控制io口为高电平
}

