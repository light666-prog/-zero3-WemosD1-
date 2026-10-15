#include "mq2.h"


#define MQ2_RL      10000.0f
#define MQ2_R0      10000.0f

void MQ2_Init(void)
{
    OLED_ShowString(3,1,"MQ2 Ready");
    printf("火灾检测启动\n");
}
float MQ2_GetFire_ppm(void)
{
    float vout = ADS1115_Read_AIN0();
    if(vout < 0)
    {
        return -1.0f;
    }

    float rs = (3.3f - vout) / vout * MQ2_RL;
    float ratio = rs / MQ2_R0;

    if(ratio <= 0.01f) ratio = 0.01f;
    float log_ratio = log10f(ratio);
    float log_ppm = (log_ratio - 0.62f) / (-0.45f);
    float ppm = powf(10.0f, log_ppm);

    if(ppm < 0) ppm = 0;
    if(ppm > 500) ppm = 500;
    
    return ppm;
}
void MQ2_OLED_Refresh(void)
{
     static uint8_t first_run = 1;
    float ppm = MQ2_GetFire_ppm();

    if(first_run)
    {
        OLED_Clear();
        OLED_ShowString(3, 1, "FDet:");
        first_run = 0;
    }

    // PPM 显示：固定 3 位整数 + 小数点 + 1 位小数
    uint32_t ppm_x10 = (uint32_t)(ppm * 10.0f);
    char ppm_str[12];
    snprintf(ppm_str, sizeof(ppm_str), "%3d.%1d", ppm_x10 / 10, ppm_x10 % 10);

    // 直接显示，不需要那个无用的 for 循环求和！
    OLED_ShowString(3, 6, ppm_str); 
    OLED_ShowString(3, 11, "ppm");
    int yuzhi=atof(ppm_str);
    if(yuzhi>40){
        buzzer_on();

    }if(yuzhi<40){
        buzzer_off();
    }
}
