

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

typedef struct __tcpUser_obj{       //用户
    char ipv4[TCP_IPV4_NAME_MAX_LEN];
    int port;    
    int fd;
    char used;
}tcpUser_obj_t;

//服务器
typedef struct __tcp_server{
    char ipv4[TCP_IPV4_NAME_MAX_LEN];
    int port;
    int fd;
    tcpUser_obj_t user[TCP_USER_NUM];
    int userCnt;
    pthread_t accept;


    //ssl 的使用
    ssl_obj_t *ssl;
    
}tcp_server_t;


//客户端
typedef struct __tcp_clinet{
    char ipv4[TCP_IPV4_NAME_MAX_LEN];
    int port;
    int fd;

    //ssl 的使用
    ssl_obj_t *ssl;
}tcp_client_t;

#define TCP_INIT_CODE 0x8080

/*=============================================================
                      结构
==============================================================*/

void tcp_init();
void tcp_test(char *ipv4, int port);
void tcp_client_test(char *ipv4, int port, char *targetIpv4, int targetPort);




#endif





