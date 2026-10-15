#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <sys/select.h>
#include <sqlite3.h>
#include "wrap.h"
#include "db.h"

#define port 8888 


void handle_Picture(int connfd);
void Login_Picture(int connfd);
void Sqlite3_Insert(const char *name, const char *person_id);

struct s_info {                     //定义一个结构体, 将地址结构跟cfd捆绑  
    struct sockaddr_in cliaddr;
    int connfd;  
};  

void GetFaceId(int connfd,const char *person_id){

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./tencent register Sign.jpg %s > vertify.txt", person_id);
    int ret=system(cmd);

    if (ret == -1) {
        perror("system 调用失败");
        return; // 如果失败了，直接退出
    }

    // 2. system 执行完了，此时 vertify.txt 才有数据！现在去读
    char *result = NULL;
    FILE *fp_result = fopen("vertify.txt", "rb"); // 用 rb，只读方式
    if (fp_result != NULL) {
        fseek(fp_result, 0, SEEK_END);
        long file_size = ftell(fp_result);
        fseek(fp_result, 0, SEEK_SET);

        result = (char *)malloc(file_size + 1);
        if (result != NULL) {
            fread(result, 1, file_size, fp_result);
            result[file_size] = '\0';
        }
        fclose(fp_result);
    }

    // 3. 判断结果，回传给手机
    if (result != NULL && strstr(result, "FACEID:") != NULL) {
    send(connfd, "Sign_OK\n", 8, 0);
    printf("注册成功，已通知手机\n");
    } else {
    send(connfd, "Sign_FAIL\n", 10, 0);
    printf("注册失败，已通知手机\n");
    if (result) printf("tencent 返回: %s\n", result);
    }
    free(result);
}
unsigned char *recv_picture1(int connfd){
    ssize_t n;
    char pic_buff[4096];
    int img_len=0;
    size_t total = 0;

   if (recv_all(connfd, &img_len, 4) != 4) {
    printf("读取长度失败\n");
         return NULL;
    }
    img_len = ntohl(img_len); // 转换字节序，得到 12084
    printf("手机发来的图片实际大小: %d bytes\n", img_len);

    if (img_len <= 0) {
        printf("图片大小为0,停止接收\n");
         return NULL;
    }

    unsigned char *buf = malloc(img_len);
    if (!buf) {
        perror("malloc");
        return NULL;
    }

    FILE *fp = fopen("Sign.jpg", "wb");
    if (!fp) {
    perror("fopen");
    close(connfd);
    exit(1);
    }

    while (total < img_len) {

      int to_read = sizeof(pic_buff);
    if (img_len - total < to_read) {
        to_read = img_len - total;
    }
    int n = recv_all(connfd, pic_buff, to_read);

    if (n == 0) {
        break; // 客户端关闭写端，传输结束
    }
    if (n < 0) {
        if (errno == EINTR) continue;
        perror("read");
        break;
    }

    if (fwrite(pic_buff, 1, n, fp) != (size_t)n) {
        perror("fwrite");
        break;
    }
    memcpy(buf + total, pic_buff, n); 
    total += n;

    if (n >= 0) {
    pic_buff[n] = '\0';
    }

    printf("%zu bytes\n", total);
    }
    fclose(fp);
    return buf;
}
unsigned char * recv_picture2(int connfd){
    ssize_t n;
    char pic_buff[4096];
    int img_len=0;
    size_t total = 0;

   if (recv_all(connfd, &img_len, 4) != 4) {
    printf("读取长度失败\n");
    return NULL;
    }
    img_len = ntohl(img_len); // 转换字节序，得到 12084
    printf("手机发来的图片实际大小: %d bytes\n", img_len);

    if (img_len <= 0) {
        printf("图片大小为0,停止接收\n");
        return NULL;
    }

    unsigned char *buf = malloc(img_len);
    if (!buf) {
        perror("malloc");
        return NULL;
    }

    FILE *fp = fopen("Login.jpg", "wb");
    if (!fp) {
    perror("fopen");
    close(connfd);
    exit(1);
    }

    while (total < img_len) {

      int to_read = sizeof(pic_buff);
    if (img_len - total < to_read) {
        to_read = img_len - total;
    }
    int n = recv_all(connfd, pic_buff, to_read);

    if (n == 0) {
        break; // 客户端关闭写端，传输结束
    }
    if (n < 0) {
        if (errno == EINTR) continue;
        perror("read");
        break;
    }

    if (fwrite(pic_buff, 1, n, fp) != (size_t)n) {
        perror("fwrite");
        break;
    }
    memcpy(buf + total, pic_buff, n); 
    total += n;

    if (n >= 0) {
    pic_buff[n] = '\0';
    }

    printf("%zu bytes\n", total);
    }
    fclose(fp);
    return buf;
}

ssize_t ReadName(int connfd, char *name, size_t maxlen){
    ssize_t n;

    while (1) {
        n = Readline(connfd, name, maxlen);
        if (n == 0) {
            return 0;               // 对端关闭
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;              // 出错
        }

        // 去掉末尾的 '\n'
        if (name[n - 1] == '\n') {
            name[n - 1] = '\0';
            n--;
        }
        return n;                   // 返回名字长度
    }
}

void *mywrite(void *pvoid)
{   
    char str[INET_ADDRSTRLEN];
	struct s_info *ts = (struct s_info*) pvoid;  
    char buf[1024];
    int i ,n;


    printf("received from %s at port %d\n",  
                inet_ntop(AF_INET, &(*ts).cliaddr.sin_addr, str, sizeof(str)),  
                ntohs((*ts).cliaddr.sin_port));
     
        n = read(ts->connfd, buf, 1024);                     //读客户端  
        if (n == 0) {  
        printf("the client %d closed...\n", ts->connfd);  
             close(ts->connfd);                             //跳出循环,关闭cfd  
        }
     

    if (strncmp(buf, "Sign", 4) == 0) {
    printf("收到注册请求\n");
    fflush(stdout);
    
    char name[128];
    size_t n = ReadName(ts->connfd, name, sizeof(name));
    if (n > 0) {
        printf("注册名字: %s\n", name);
    }
    char person_id[128];
    size_t m=ReadName(ts->connfd, person_id, sizeof(person_id));
    if (m> 0) {
        printf("注册人id: %s\n", person_id);
    }

    send(ts->connfd, "Hadwrite\n", 9, 0); 
    
    unsigned char *picture = recv_picture1(ts->connfd);

    if(picture){
    Sqlite3_Insert(name,person_id);
    GetFaceId(ts->connfd,person_id);
    }

    free(picture); // 释放内存
}


else if (strncmp(buf, "Login", 5) == 0) {
    printf("收到登录请求\n");
    fflush(stdout);
    send(ts->connfd, "READY\n", 6, 0); 
    
    unsigned char *picture = recv_picture2(ts->connfd);
    if(picture){
    Login_Picture(ts->connfd);
    }
    free(picture);
}

    // 3. 处理控制指令：Android发来 "cmd 3" 或 "cmd 4"
    else if (strncmp(buf, "cmd 3", 5) == 0) {
        printf("执行：空调开启\n");
        // AC_ON(); // 你的红外发射逻辑
        send(ts->connfd, "OK\n", 3, 0);
    }
    else if (strncmp(buf, "cmd 4", 5) == 0) {
        printf("执行：空调关闭\n");
        // AC_OFF(); // 你的红外发射逻辑
        send(ts->connfd, "OK\n", 3, 0);
    }
    else {
        send(ts->connfd, "OK\n", 3, 0);
    }
         
        close(ts->connfd);
        return (void *)0;
    }

void Sqlite3_Insert(const char *name, const char *person_id){
    
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO SignPeople ( name,person_id) VALUES (?,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
         fprintf(stderr, "无法准备SQL语句: %s\n", sqlite3_errmsg(db));
         return;
    }
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,2,person_id,-1, SQLITE_STATIC);
     // 执行SQL语句
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "无法执行SQL语句: %s\n", sqlite3_errmsg(db));
        return;
    }
    sqlite3_finalize(stmt);
    
}
void Login_Picture(int connfd) {
    int ret = system("./tencent login Login.jpg > vertify.txt");
    if (ret == -1) {
        perror("system 调用失败");
        send(connfd, "AUTH_FAIL\n", 10, 0);
        return;
    }

    char *result = NULL;
    FILE *fp = fopen("vertify.txt", "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        result = malloc(size + 1);
        if (result) {
            fread(result, 1, size, fp);
            result[size] = '\0';
        }
        fclose(fp);
    }

    if (result) {
        printf("tencent 返回: %s\n", result);

        if (strstr(result, "NAME:") != NULL) {
            // 提取 name
            char *p = strstr(result, "NAME:") + 5;
            char name[64] = {0};
            int i = 0;
            while (*p && *p != '\n' && i < 63) name[i++] = *p++;

            printf("登录成功: %s\n", name);
            send(connfd, "AUTH_OK\n", 8, 0);
        } else {
            printf("未匹配到人脸\n");
            send(connfd, "AUTH_FAIL\n", 10, 0);
        }
        free(result);
    } else {
        send(connfd, "AUTH_FAIL\n", 10, 0);
    }
}


int main(){

     // 1. 打开数据库
    if (sqlite3_open("Sign.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Database error: %s\n", sqlite3_errmsg(db));
        exit(1);
    }
    fprintf(stderr, ">>> 数据库打开成功\n");

    // 测试数据初始化Manager
    char *err_msg = 0;
    sqlite3_exec(db, 
    "CREATE TABLE IF NOT EXISTS SignPeople ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "  
    "name TEXT, "
     "person_id TEXT UNIQUE,"
    "face_id TEXT"
    ");", 
    0, 0, &err_msg);
    printf("创建成功\n");
    // sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS History (id integer,name text);", 0, 0, &err_msg);

    int listen_fd;
    pthread_t tid;
    struct sockaddr_in cliaddr,server_addr;
     socklen_t cliaddr_len;
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    listen(listen_fd, 5);

        struct s_info ts[256];
        int i=0;

    printf("TCP 服务端启动，端口 %d\n", port);
    while (1) {  
        cliaddr_len = sizeof(cliaddr);  
        int connfd = accept(listen_fd, (struct sockaddr *)&cliaddr, &cliaddr_len);   //阻塞监听客户端链接请求  
        if(connfd==-1){
            perror("accept error\n");
            return -1;
        }
        ts[i].cliaddr = cliaddr;  
        ts[i].connfd = connfd;  
  
        pthread_create(&tid, NULL, mywrite, (void*)&ts[i]);  
        pthread_detach(tid);                                                    //子线程分离,防止僵线程产生.  
        i++;  
    }  

        sqlite3_close(db);
        return 0;
}