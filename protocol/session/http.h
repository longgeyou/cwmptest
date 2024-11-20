
#ifndef _HTTP_H_
#define _HTTP_H_

#include "tcp.h"
#include "log.h"
//#include "keyvalue.h"
#include "httphead.h"
#include "strpro.h"
#include "base64.h"

#include "auth.h"
#include "digcalc.h"

#include "httpmsg.h"


/*=============================================================
                        数据结构
==============================================================*/

//http 服务器相关
#define HTTP_USER_NUM TCP_USER_NUM  //服务器用户个数


//http 报文接收 相关    
//#define HTTP_HEAD_DATA_MAX_SIZE 1024
#define HTTP_PAYLOAD_BUF_SIZE 512   //http净荷
#define HTTP_USER_BUF_SIZE (TCP_USER_BUF_SIZE * 2)  //用户缓存长度
#define HTTP_USER_BUF_A_BLOCK_SIZE (HTTP_USER_BUF_SIZE / 2) //在逻辑上把 http 缓存分为A、B两个块

#define HTTP_METHOD_STRING_SIZE 32      //请求方法
#define HTTP_URL_STRING_SZIE 256        //资源请求定位 最多字 符个数，256 可能有点小了...
#define HTTP_VERSION_STRING_SIZE 32     //http版本
#define HTTP_HEAD_KEYVALUE_MAX_SIZE 30  //键值对最大个数
#define HTTP_HEAD_BYTE_CNT_MAX_SIZE 2048    //【谨慎设置】处理的头部数据字节计数（不包括第一行），阈值

//http 认证相关
#define HTTP_USE_SSL 1  //是否使用 SSL
//#define HTTP_USE_DIGEST_AUTH  1  //是否使用摘要认证

//http 报文发送相关
#define HTTP_SEND_BUF_SIZE (TCP_USER_BUF_SIZE)  //发送缓存长度





//http头部
typedef struct http_head_t{
    //int len;
    //unsigned char data[HTTP_HEAD_DATA_MAX_SIZE];    //存储http头部信息，这里有最大值的限制

    //头部内容解析
    //1、第一行
    char method[HTTP_METHOD_STRING_SIZE + 8];  //方法，加冗余
    char url[HTTP_URL_STRING_SZIE + 8];
    char version[HTTP_VERSION_STRING_SIZE + 8];
    
    //2、头部其他行的解析结果 （参见 httphead.c ）
    httphead_head_t *parse;  
       
}http_head_t;


//http净荷
typedef struct http_payload_t{
    int len;
    unsigned char buf[HTTP_PAYLOAD_BUF_SIZE];   //buf是缓存区，并不存储所有的净荷
}http_payload_t;


//接收数据状态机
typedef enum{
    http_status_start = 0,
    http_status_recv_head_find, //htpp头部位置 查找
    http_status_recv_head,
    http_status_head_pro,
    http_status_recv_payload,
    http_status_data_pro
}http_status_e;


//http 用户对象
typedef struct httpUser_obj_t{

    //1、http recv (接收报文) 处理
    http_head_t head;
    http_payload_t payload;    
    //1.1 状态机 
    http_status_e status;   //http头部处理状态    
    //1.2 报文接收
    unsigned char buf[HTTP_USER_BUF_SIZE + 8];    //是tcp user 缓存的两倍，可以看做 A、B 两个块
    int bufLen;    
    int headByteCnt;    //处理的头部数据字节计数（不包括第一行），超过阈值 
                        //HTTP_HEAD_BYTE_CNT_MAX_SIZE 视为报文无效 ！！

    //2、http send（报文发送）
    unsigned char sendBuf[HTTP_SEND_BUF_SIZE + 8];    //是tcp user 缓存的两倍，可以看做 A、B 两个块
    int sendBufLen;
    char sendReady;    
    pthread_mutex_t sendMutex;   //互斥锁  
    pthread_cond_t sendCond;   //条件变量

    //3、http auth（认证）
    server_digest_auth_obj_t auth;
    char useSSL;    //是否使用SSL
    char useDigestAuth; //是否使用摘要认证

    
}httpUser_obj_t;


//http 作为服务器
typedef struct http_server_t{    

    tcp_server_t *tcp;

    //用户
    httpUser_obj_t user[HTTP_USER_NUM];
    
    //线程
    pthread_t data_accept;  //接收线程
    pthread_t data_send;    //发送线程

    //信号量
    sem_t semSendReady;

    //用于认证的账户表单
    keyvalue_obj_t *account;    //如果数据比较大，可以用字典 dic_obj_t


    
}http_server_t;



//---------------------------------------------------------客户端
#define HTTP_MSG_STATUS_CODE_STRING_SIZE 16
#define HTTP_MSG_REASON_STRING_SIZE 64
typedef struct http_client_parse_t{
    //1、头部
    char version[HTTP_VERSION_STRING_SIZE + 8]; //版本 默认为 HTTP/1.1
    char codeStr[HTTP_MSG_STATUS_CODE_STRING_SIZE + 8];  //状态码
    int code;
    char reason[HTTP_MSG_REASON_STRING_SIZE + 8];  //状态码的原因
       
    httphead_head_t *head;     //每一行（除了第一行）的解析

    //2、负载
    http_payload_t payload;   
}http_client_parse_t;


//http 作为客户端
typedef struct http_client_t{
    http_client_parse_t parse;

    tcp_client_t *tcp; 

    //1、数据接收
      
    pthread_t dataAccept;  //接收线程
    http_status_e status;   //状态机
    unsigned char buf[HTTP_USER_BUF_SIZE + 8];    //是tcp user 缓存的两倍，可以看做 A、B 两个块
    int bufLen; //缓存长度
    int headByteCnt;    //字节计数

    //2、数据发送
     sem_t semSendReady;    //信号量

    //3、http 认证
    client_auth_obj_t auth;
    char useSSL;    //是否使用SSL
    char useDigestAuth; //是否使用摘要认证

     
    
}http_client_t;
//---------------------------------------------------------客户端



/*==============================================================
                        接口
==============================================================*/
void http_mg_init();
void http_test();
void http_other_test();


void http_client_test(char *ipv4, int port, char *targetIpv4, int targetPort);



#endif




