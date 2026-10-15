#include "main_function.h"
#include "mq2.h"
#include "buzzer.h"
int main() {
    // 1. 硬件初始化建议放在最前面
    wiringPiSetup();
    Led_Init();
    OLED_Init();
    if (AHT20_Init() != 0) {
        fprintf(stderr, "AHT20 init failed\n");
        return -1;
    }
    
    // 重要：MQ2初始化只在这里做一次！
    MQ2_Init(); 
    buzzer_init();
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -2;
    }

    if (pid == 0) {
        // ================= 子进程：只负责 MQ2 和 OLED =================
        while (1) {
            MQ2_OLED_Refresh();
            sleep(2); // 核心关键！必须加延时，2秒刷新一次，保证屏幕不闪且 CPU 不爆满
        }
    } 
    else {
        // ================= 父进程：负责语音、TCP、红外 =================
        main_function(); // 确保 main_function 内部也有一个死循环，且里面不要操作 OLED！
    }

    return 0;
}