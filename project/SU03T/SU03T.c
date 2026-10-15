#include "main_function.h"

//灯的命令
const unsigned char CMD_OPEN_living[]  = {0xAA, 0x55, 0x03, 0x55, 0xAA};
const unsigned char CMD_CLOSE_living[] = {0xAA, 0x55, 0x04, 0x55, 0xAA};
const unsigned char CMD_OPEN_Bed[]     = {0xAA, 0x55, 0x05, 0x55, 0xAA};
const unsigned char CMD_CLOSE_Bed[]    = {0xAA, 0x55, 0x06, 0x55, 0xAA};
const unsigned char CMD_OPEN_Bath[]    = {0xAA, 0x55, 0x07, 0x55, 0xAA};
const unsigned char CMD_CLOSE_Bath[]   = {0xAA, 0x55, 0x08, 0x55, 0xAA};
const unsigned char CMD_OPEN_Kichen[]  = {0xAA, 0x55, 0x09, 0x55, 0xAA};
const unsigned char CMD_CLOSE_Kichen[] = {0xAA, 0x55, 0x0A, 0x55, 0xAA};

//温湿度命令
const unsigned char CMD_TEMP[] = {0x77,0xA3,0x9B,0xEE};
const unsigned char CMD_HUMI[] = {0x98 ,0xA2 ,0x08 ,0xDE};
//空调和电机命令
const unsigned char  CMD_AC_ON[] = { 0x45, 0xA3, 0x0C, 0xDF };
const unsigned char  CMD_AC_OFF[] = { 0x34, 0xA4, 0x0F, 0xED };
const unsigned char  CMD_Motor_Front[]={0x78, 0xA3 ,0xB5 ,0xEE};
const unsigned char  CMD_Motor_Side[]={0x78, 0xA4 ,0xF4 ,0x90};
const unsigned char  CMD_Motor_OFF[]={0x88, 0xA5 ,0x0D ,0xFF};


#define CMD_LEN 5
#define CMD_LEN2 4

void send_voice_cmd(int fd,const unsigned char* cmd, int len) {
     if (fd < 0) {
        printf("串口未打开，无法发送语音指令！\n");
        return;
    }
      tcflush(fd, TCIFLUSH);                 // 清掉上次残留
    ssize_t ret = write(fd, cmd, len);
    if (ret != len) {
    perror("write incomplete");
    return;
    }
    
    printf("[系统] 已向语音模块发送播报指令\n");
}

void main_function(){
   
    float temp;
    float humi;

   int usb_fd0=serial_usb_init0("/dev/ttyUSB0");
    if(usb_fd0==-1){
        perror("usb init error\n");
        return;
    }
    int usb_fd1=serial_usb_init1("/dev/ttyUSB1");
    if(usb_fd1==-1){
        perror("usb init error\n");
        return;
    }
    while(1) {
        

    unsigned char usb_buf0[1024]; // 删除初始化 {"0"}，改成 unsigned char
    unsigned char usb_buf1[1024];
    int m=read(usb_fd0, usb_buf0, sizeof(usb_buf0));
    int n = read(usb_fd1, usb_buf1, sizeof(usb_buf1));

    if (n < 0 && m<0) {
        perror("read error:");
        return;
    } 
    else if (n == 0 &&m==0) {
        // 没有数据时不要疯狂打印，可以加个延时
        usleep(10000); 
        continue; 
    } 
    else {
        printf("ttyUSB1 收到 %d 字节 (HEX): \n", n);
        for (int i = 0; i < n; i++) printf("%02X ", usb_buf1[i]);
        printf("\n");
        fflush(stdout); // ⚠️ 强制刷新缓冲区
        if (m > 0) {
        printf("ttyUSB0 收到 %d 字节 (HEX): ", m);
         for (int j = 0; j < m; j++) printf("%02X ", usb_buf0[j]);
        printf("\n");
        fflush(stdout); // ⚠️ 强制刷新缓冲区
        }
    }
        printf("\n");

        unsigned char *buf = NULL;
        int len = 0;

        if (n >= CMD_LEN) {
            buf = usb_buf1;
            len = n;
        } else if (m >= CMD_LEN) {
            buf = usb_buf0;
            len = m;
        }

        if (buf != NULL) {
    // 打印一下收到的指令，方便调试
    printf("收到 %d 字节指令: ", len);
    char hex_str[512];
    int pos = 0;
    for (int i = 0; i < len && pos < (int)sizeof(hex_str) - 3; i++) {
    pos += snprintf(hex_str + pos, sizeof(hex_str) - pos, "%02X ", buf[i]);
    }
    if (pos > 0) hex_str[pos - 1] = '\0';

    printf("收到: %s\n", hex_str);
    printf("\n");
    
    // --- 开关灯指令 ---
    if (memcmp(buf, CMD_OPEN_living, CMD_LEN) == 0) {
        printf("开客厅灯\n");
        send_voice_cmd(usb_fd1,CMD_OPEN_living,CMD_LEN);
        
    } 
    else if (memcmp(buf, CMD_CLOSE_living, CMD_LEN) == 0) {
        send_voice_cmd(usb_fd1,CMD_CLOSE_living,CMD_LEN);
    }
    else if (memcmp(buf, CMD_OPEN_Bed, CMD_LEN) == 0) {
       send_voice_cmd(usb_fd1,CMD_OPEN_Bed,CMD_LEN);
    } 
    else if (memcmp(buf, CMD_CLOSE_Bed, CMD_LEN) == 0) {
      send_voice_cmd(usb_fd1,CMD_CLOSE_Bed,CMD_LEN);

    }else if (memcmp(buf, CMD_OPEN_Bath, CMD_LEN) == 0) {
        send_voice_cmd(usb_fd1, CMD_OPEN_Bath,CMD_LEN);
    } 
    else if (memcmp(buf, CMD_CLOSE_Bath, CMD_LEN) == 0) {
        send_voice_cmd(usb_fd1,CMD_CLOSE_Bath,CMD_LEN);
    }
    else if (memcmp(buf, CMD_OPEN_Kichen, CMD_LEN) == 0) {
        send_voice_cmd(usb_fd1,CMD_OPEN_Kichen,CMD_LEN);
    } 
    else if (memcmp(buf, CMD_CLOSE_Kichen, CMD_LEN) == 0) {
        send_voice_cmd(usb_fd1,CMD_CLOSE_Kichen,CMD_LEN);
    }
    
 }      //控制温湿度
        else if(n>= CMD_LEN2 && memcmp(usb_buf1, CMD_TEMP, CMD_LEN2) == 0){
            AHT20_Read_TEMP(&temp,usb_fd1);
        }
             else if(n>= CMD_LEN2 && memcmp(usb_buf1, CMD_HUMI, CMD_LEN2) == 0){
           AHT20_Read_HUMI(&humi,usb_fd1);
     }else if (n>= CMD_LEN2 && memcmp(usb_buf1, CMD_AC_ON, CMD_LEN2) == 0)
     {  
        send_voice_cmd(usb_fd0,CMD_AC_ON,CMD_LEN2);
        printf("打开空调\n");
        //控制空调
     }else if (n>= CMD_LEN2 &&memcmp(usb_buf1, CMD_AC_OFF, CMD_LEN2)==0)
     {
        send_voice_cmd(usb_fd0,CMD_AC_OFF,CMD_LEN2);
        printf("关闭空调\n");

        //控制电机
     }else if(n>= CMD_LEN2 &&memcmp(usb_buf1, CMD_Motor_Front, CMD_LEN2)==0){
            send_voice_cmd(usb_fd0,CMD_Motor_Front,CMD_LEN2);
     }else if(n>= CMD_LEN2 &&memcmp(usb_buf1, CMD_Motor_Side, CMD_LEN2)==0){
            send_voice_cmd(usb_fd0,CMD_Motor_Side,CMD_LEN2);
     }else if(n>= CMD_LEN2 &&memcmp(usb_buf1, CMD_Motor_OFF, CMD_LEN2)==0){
            send_voice_cmd(usb_fd0,CMD_Motor_OFF,CMD_LEN2);
     }
   }
  }

