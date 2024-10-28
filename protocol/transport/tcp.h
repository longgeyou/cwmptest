

#ifndef __TCP_H
#define __TCP_H


#include "tcp.h"
#include "linux.h"
#include "log.h"
#include "pool2.h"
#include "linux_itf.h"
#include "ssl.h"




/*=============================================================
                      数据结构定义
==============================================================*/

#define TCP_IPV4_NAME_MAX_LEN 64
#define TCP_USER_NUM 32
#define TCP_USER_BUF_SIZE 512  //用户 接收数据 缓存区大小

typedef struct __tcpUser_obj{       //用户
    char ipv4[TCP_IPV4_NAME_MAX_LEN];
    int port;    
    int fd;
    char used;
    char disconnect;    //断开链接标记

    //接收数据缓存
    unsigned char buf[TCP_USER_BUF_SIZE + 8];  //考虑到字符串需要最后一位 '\0'，冗余
    char bufReady;  //状态位 1 ：缓存区准备好了被读取的数据
    //char bufMore;   //状态位 1：因为缓存区有限，如果读取的数据大于buf长度，则表示后面还有
    int bufLen;     //数据的长度
    
    //自旋锁
    //pthread_spinlock_t spinlock;
    //互斥锁
    pthread_mutex_t mutex;
}tcpUser_obj_t;

//服务器
typedef struct __tcp_server{
    char ipv4[TCP_IPV4_NAME_MAX_LEN];
    int port;
    int fd;
    tcpUser_obj_t user[TCP_USER_NUM];
    int userCnt;
    pthread_t accept;
    pthread_t transceiver;     //收发器


    //ssl 的使用
    ssl_obj_t *ssl;

    //读取操作互斥锁
    //pthread_mutex_t mutex;  //互斥锁
    //pthread_cond_t cond;    //条件变量
    //有数据待处理
    //char pollIn;

    //信号量
    sem_t semBufReady;
    
}tcp_server_t;


//客户端
typedef struct __tcp_clinet{
    char ipv4[TCP_IPV4_NAME_MAX_LEN];
    int port;
    int fd;

    //ssl 的使用
    ssl_obj_t *ssl;

    //线程
    pthread_t transceiver;     //收发器

    //用于数据接收
    unsigned char buf[TCP_USER_BUF_SIZE + 8];  //缓存
    char bufReady;  //状态
    int bufLen;     //缓存的长度
    sem_t semBufReady;  //信号量
    pthread_mutex_t mutex;  //互斥锁
    
}tcp_client_t;

#define TCP_INIT_CODE 0x8080

/*=============================================================
                      接口
==============================================================*/

void tcp_init();
void tcp_test(char *ipv4, int port);
void tcp_client_test(char *ipv4, int port, char *targetIpv4, int targetPort);

tcp_server_t *tcp_server_create(char *ipv4, int port);
void tcp_server_destory(tcp_server_t *tcp);
tcp_client_t *tcp_client_create(char *ipv4, int port);
void tcp_client_destory(tcp_client_t * client);


int tcp_server_start(tcp_server_t *tcp);
int tcp_client_start(tcp_client_t *client, char *ipv4, int port);
int tcp_server_send(tcp_server_t *tcp, int userId, char *msg, int size);
int tcp_server_read(tcp_server_t *tcp, int userId, char *content, int size) ;
int tcp_client_send(tcp_client_t *cl, char *msg, int size);
int tcp_client_read(tcp_client_t *cl, char *content, int size);







#endif





