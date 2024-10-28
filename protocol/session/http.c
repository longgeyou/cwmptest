

//注：协议所用到的 http 是 HTTP 1.1版本 ，文档 tr-069-1-6-1.pdf 有所描述


#include <string.h>
#include <stdlib.h>

#include "http.h"
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



//请求方法 关键字
const char *http_requst_method[] = { "GET", "POST", "PUT", "DELETE", 
                                        "HEAD", "CONNECT", "OPTIONS", 
                                        "TRACE", "PATCH" };

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


/*=============================================================
                        本地管理
==============================================================*/
#define HTTP_MG_INIT_CODE 0x8080
typedef struct http_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int initCode;
    
    int serverCnt;
    int clientCnt;
}http_manager_t;

http_manager_t http_local_mg = {0};
    
//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(http_local_mg.poolId, x)
#define POOL_FREE(x) pool_user_free(x)
#define HTTP_POOL_NAME "http"
    
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void http_mg_init()
{    
    if(http_local_mg.initCode == HTTP_MG_INIT_CODE) return; //一次初始化
    
    strncpy(http_local_mg.poolName, HTTP_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    http_local_mg.poolId = pool_apply_user_id(http_local_mg.poolName); 
    http_local_mg.initCode = HTTP_MG_INIT_CODE;
    
    http_local_mg.serverCnt = 0;
    http_local_mg.clientCnt = 0;

    //keyvalue_init();
    httpmsg_mg_init();
}


/*==============================================================
                        对象
==============================================================*/
////释放一个http user 子成员的动态内存
//void http_user_release(httpUser_obj_t *user)
//{
//    ;
//}


#define HTTP_AUTH_DEFAULT_REALM "/cwmp"
#define HTTP_AUTH_DEFAULT_QOP "auth-int"
//创建一个http server
http_server_t *http_create_server(char *ipv4, int port)
{
    http_server_t *http = (http_server_t *)POOL_MALLOC(sizeof(http_server_t));
    if(http != NULL)
    {
        http->tcp = tcp_server_create(ipv4, port);
    }
    int i;

    httpUser_obj_t *user;
    //1、http用户 设置
    for(i = 0; i < HTTP_USER_NUM; i++)
    {
        user = &(http->user[i]);
        user->status = http_status_start;   //状态机处于初始状态

        //1.1 http 头部数据结构（除了第一行）
        user->head.parse = httphead_create_head();
        if(user->head.parse == NULL)
        {
            LOG_ALARM("http 头部数据结构");
        }

         //1.2 认证
        user->useDigestAuth = 1;   //默认使用 摘要认证
        //user->useDigestAuth = 0; 
        serverauth_obj_init(&(user->auth), HTTP_AUTH_DEFAULT_REALM, HTTP_AUTH_DEFAULT_QOP);  //默认配置
        //LOG_INFO("http_create_server: nonce:%s opaque:%s\n", http->user[i].auth.nonce, http->user[i].auth.opaque);

        //1.3 发送
        memset(user->sendBuf, 0, HTTP_SEND_BUF_SIZE);
        user->sendBufLen = 0;
        user->sendReady = 0; 
        pthread_mutex_init(&(user->sendMutex), NULL);
        pthread_cond_init(&(user->sendCond), NULL);
 
    }
    
    //2、初始化信号量
    sem_init(&(http->semSendReady), 0, 0);
    

   //3、用于认证的账户表单
    http->account = keyvalue_create(HTTP_USER_NUM + 8);
    serverauth_parse_account(http->account);
    //keyvalue_show(http->account);

    http_local_mg.serverCnt++;    
    return http;
}

////摧毁一个http server
//void http_destory_server(http_server_t *http)
//{
//    if(http == NULL)return;
//    int i;
//
//    //释放http user 成员
//    for(i = 0; i < HTTP_USER_NUM; i++)
//    {
//        ;//if(http->user[i].head.keyvalue != NULL)
//            //keyvalue_obj_destory(http->user[i].head.keyvalue);
//    }
//        
//    //释放自己
//    POOL_FREE(http);   
//}

//创建一个http client

#define TEST_CLIENT_USERNAME "admin"
#define TEST_CLIENT_PASSWORD "123456"
http_client_t *http_create_client()
{
    http_client_t *client = (http_client_t *)POOL_MALLOC(sizeof(http_client_t));
    http_local_mg.clientCnt++;

    //1、数据接收
    //1.1 缓存
    memset(client->buf, 0, client->bufLen);
    client->bufLen = 0;
    //1.2 状态机
    client->status = http_status_start;
    
    //1.3 头部解析
    client->parse.head = httphead_create_head();    
    if(client->parse.head == NULL)
    {
        LOG_ALARM("http 头部数据结构");
    }

    //1.4 认证
    client->useSSL = 1;
    //client->useDigestAuth = 1;   //默认使用 摘要认证
    client->useDigestAuth = 0; 
    clientauth_init(&(client->auth), TEST_CLIENT_USERNAME, TEST_CLIENT_PASSWORD, 100);  //默认配置
    //LOG_INFO("client auth: nonce:%s opaque:%s\n", client->auth.nonce, client->auth.opaque);

    //2、发送

    
    return client;
}

////摧毁一个http client
//void http_destory_client(http_client_t *client)
//{
//    if(client == NULL)return ;
//
//    //int i;
//    //释放 成员
//    
//    
//                    
//    //释放自己
//    POOL_FREE(client);
//}


/*==============================================================
                        http 报文处理（服务器）
==============================================================*/
/* 请求头
GET /index.html HTTP/1.1  
Host: www.example.com  
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 
Safari/537.3  
Accept: xx
Accept-Encoding: gzip, deflate, sdch, br  
Accept-Language: en-US,en;q=0.8  
Cookie: name=value; anothername=anothervalue  
Connection: keep-alive
*/
#define HTTP_HEAD_FIND_KEYWORD "HTTP/1.1"       //http头部第一行的关键词
#define HTTP_HEAD_FIND_KEYWORD_B "http/1.1"       


//同步：找到 http 头部第一行位置；然后解析
static int __http_message_first_line_parse(httpUser_obj_t *user, int *endPos)
{
    if(user == NULL)return RET_FAILD;
    
    int step;
    int i;
    int num;
    int len, lenVersion, lenMethod;
    int posStart;
    int posMethod, posVersion, posEnter;
    char *find, *find2;
    int pos;
    char *p;
    //int bufLen;

    
    char line[HTTP_USER_BUF_SIZE + 8];
    char buf[HTTP_USER_BUF_SIZE + 8];
    memcpy(buf, user->buf, user->bufLen);   //注意：user->buf 中可能包含字符串结束符号 '\0'，所以 ...
    
    buf[user->bufLen] = '\0';

    LOG_SHOW("__http_message_first_line_parse len:%d buf:【%s】\n", strlen(buf), buf);
    
    //参考 GET /index.html HTTP/1.1  
    //三个步骤完成，才认为第一行数据有效

    posStart = 0;
    
    while(1)
    {
        step = 0;
        if(posStart >= user->bufLen)break;
        //1、找到换行标志
        find = strstr(buf + posStart, "\n");  
        
        if(buf[posStart] == '\n')   //换行符前面没有数据
        {
            posStart += 1;
            //posEnter = posStart;
            continue;
        }
        
        if(find != NULL )
        {
            len = find - buf + 1;
            
            
            memcpy(line, buf + posStart, len);
            line[len] = '\0';
            posStart += len;
            posEnter = len - 1;
            
            step = 1;            
            
            
        }
        else
        {
            *endPos = user->bufLen - 1;
            //*endPos = bufLen - 1;
            return RET_FAILD;  
        }
        

        //2、请求方法 的匹配
        if(step == 1)   //上一步成功
        {
            num = (int)(sizeof(http_requst_method) / sizeof(const char *));
            for(i = 0; i < num; i++)
            {
                find = strstr(line, http_requst_method[i]);
                if(find != NULL)
                {                 
                    lenMethod = strlen(http_requst_method[i]);
                    strncpy(user->head.method, http_requst_method[i], HTTP_METHOD_STRING_SIZE);
                    posMethod = find - line;   //位置
                    
                    step = 2;   //步骤标明
                    break;
                }
            }

        }
          
        //3、"HTTP/1.1" 匹配（或者其他）
        if(step == 2)
        {
            find = strstr(line, HTTP_HEAD_FIND_KEYWORD);
            find2 = strstr(line, HTTP_HEAD_FIND_KEYWORD_B);
            if(find == NULL)find = find2;
            if(find != NULL)
            {
                lenVersion = strlen(HTTP_HEAD_FIND_KEYWORD);    //HTTP/1.1 和 http/1.1 一样长度
                strncpy(user->head.version, HTTP_HEAD_FIND_KEYWORD, lenVersion);

                posVersion = find - line;   //位置
                step = 3;
            }
        }

        //4、位置（最后一步）
        if(step == 3)
        {
            if(posMethod < posVersion   &&  posVersion < posEnter) 
            {            
                //解析 url
                pos = posMethod  + lenMethod + 1;
                len = posVersion - pos - 1;
                if(len > 0)
                {
                    strncpy(user->head.url, line + pos, len);
                    p = user->head.url;                    
                    strpro_clean_space(&p);   //去除两边空格
                    
                }
                

                //结束的位置
                *endPos = posEnter;

                //LOG_INFO("method:%s url:%s version:%s", user->head.method, user->head.url, user->head.version);
                return RET_OK;
            }
        }
    }
    return RET_FAILD;     
}


//http 头部解析（除了第一行）
//返回值 RET_OK：找到了空行               RET_FAILD：没找到空行
// 参数 endPos 表示处理过的数据的 最后位置
static int __http_message_head_parse(httpUser_obj_t *user, int *endPos)
{
    if(user == NULL || endPos == NULL)return RET_FAILD;
    
    char *find;
    int findPos, newStartPos;
    //int step;
    //int len;

    int ret;
    char buf[HTTP_USER_BUF_SIZE + 8];   //http 用户缓存长度
    
    memcpy(buf, user->buf, user->bufLen);
    buf[user->bufLen] = '\0';

    //str = buf;
    ret = RET_FAILD;
    newStartPos = 0;
    while(1)
    {        
        find = strstr(buf + newStartPos, "\n");
        if(find == NULL)break;
        
        findPos = find - buf;
        newStartPos = findPos + 1;

        //找到http  头部和 负荷 的分界位置
        if((newStartPos <= user->bufLen && buf[newStartPos] == '\n'))
        {
            *endPos = newStartPos;
            ret = RET_OK;
            break;
        } 
        
        if((newStartPos + 1) <= user->bufLen  && buf[newStartPos] == '\r' && buf[newStartPos + 1] == '\n') 
        {
            *endPos = newStartPos + 1;
            ret = RET_OK;
            break;
        }
        

        //后面没有数据了
        if(newStartPos >= user->bufLen)
        {
            *endPos = findPos;
            break;
        }

        //后面还有数据，则进入下一个寻找 '\n' 的循环  
        *endPos = findPos;
    }

    if(findPos >= 1)
    {
        buf[findPos + 1] = '\0';

        //先清空解析的内容
        httphead_clear_head(user->head.parse);
        
        
        //解析
        httphead_parse_head(user->head.parse, buf);
        
        //LOG_SHOW("标志 .................... 1\n");
        httphead_show(user->head.parse);
        //LOG_SHOW("标志 .................... 2\n");
    }
    
    return ret;
}



//------------------------------------------------------------------------发送相关
typedef struct http_send_arg_t{
    http_server_t *http;
    int userId;
    void *data;    //要发送的数据
    char mallocEn;  //指示 data 指针是指向动态内存
    int dataLen;

    void (* finish_pro)(void *in);  //回调函数（用于处理发送完成后的动作，例如动态内存释放）
}http_send_arg_t;

//发送线程
void *__http_server_user_send(void *in)
{
    int pos;
    int len;
    int sendLen;

    if(in == NULL)return NULL;

    http_send_arg_t *arg = (http_send_arg_t *)in;
    http_server_t *http = arg->http;

    if(arg->userId < 0 || arg->userId >= HTTP_USER_NUM)
    {
        LOG_ALARM("arg->userId 越界");
        
        arg->finish_pro(in);
        return NULL;
    }
    
    httpUser_obj_t *user = &(http->user[arg->userId]);

    pos = 0;
    len = arg->dataLen;
    sendLen = 0;

    while(1)
    {
        len = arg->dataLen - pos;
        
        if(len > HTTP_SEND_BUF_SIZE)
            sendLen = HTTP_SEND_BUF_SIZE;  
        else
            sendLen = len;
        if(sendLen <= 0)break;

        
            
        //数据放入缓存
        pthread_mutex_lock(&(user->sendMutex)); 
        while(user->sendReady == 1)
        {
            pthread_cond_wait(&(user->sendCond), &(user->sendMutex));   //条件变量：等待发送完成
        }
         
        memcpy(user->sendBuf, arg->data + pos, sendLen);
        user->sendBufLen = sendLen;
                    
        user->sendReady = 1; 
        //发送信号量
        sem_post(&(http->semSendReady)); 
        pthread_mutex_unlock(&(user->sendMutex));

        //LOG_SHOW("pos:%d sendLen=%d len:%d buf:%s\n", pos, sendLen,arg->dataLen, user->sendBuf);
        pos += sendLen;
        //if(pos >= arg->dataLen)break;
    }
   
    arg->finish_pro(in);

    return NULL;
}

//发送完成后的处理（回调函数）
void __send_finish_call_back(void *in)  //动态内存回收
{
    if(in == NULL)return;
    http_send_arg_t *arg = (http_send_arg_t *)in;

    if(arg->data != NULL && arg->mallocEn == 1)POOL_FREE(arg->data);

    POOL_FREE(arg);
}

//简单发送数据
int http_send(http_server_t *http, int userId, void *data, int dataLen)
{
    int ret;
    if(http == NULL || data == NULL || dataLen <= 0)return RET_FAILD;

    http_send_arg_t *arg = (http_send_arg_t *)POOL_MALLOC(sizeof(http_send_arg_t));
    if(arg == NULL)return RET_FAILD;

    arg->http = http;
    arg->userId = userId;
    arg->data = POOL_MALLOC(dataLen + 8);    //开辟动态内存（不适合要发送大数据的情况）
    if(arg->data == NULL)
    {
        LOG_ALARM("arg->data 动态内存开辟失败");
        POOL_FREE(arg);
        return RET_FAILD;
    }
    memcpy(arg->data, data, dataLen);
    arg->mallocEn = 1;
    arg->dataLen = dataLen;
    arg->finish_pro = __send_finish_call_back;
    
    //开启发送线程
    pthread_t th;
    ret = pthread_create(&th, NULL, __http_server_user_send, arg);
    if(ret != 0)
    {
        LOG_ALARM("__http_server_user_send 线程开启失败"); 
        POOL_FREE(arg);
        POOL_FREE(arg->data);
    };

    return ret;
}
//简单发送字符串
int http_send_str(http_server_t *http, int userId, char *str)
{
    return http_send(http, userId, (void *)str, strlen(str) + 1);
}


//------------------------------------------------------------------------发送线相关






#define HTTP_AUTH_STR_A "Authorization"
#define HTTP_AUTH_STR_B "Basic"
#define HTTP_DIGWST_AUTH_NAME "Authorization"
#define HTTP_DIGWST_AUTH_DIGEST "Digest"
#define HTTP_DIGWST_AUTH_USERNAME "username"
#define HTTP_DIGWST_AUTH_REALM "realm"
#define HTTP_DIGWST_AUTH_NONCE "nonce"
#define HTTP_DIGWST_AUTH_URI "uri"
#define HTTP_DIGWST_AUTH_RESPONSE "response"
#define HTTP_DIGWST_AUTH_MOPAQUE "opaque"
#define HTTP_DIGWST_AUTH_QOP "qop"
#define HTTP_DIGWST_AUTH_NC "nc"
#define HTTP_DIGWST_AUTH_CNONCE "cnonce"

#define BUF_SIZE_128 128
#define BUF_SIZE_256 256
#define BUF_SIZE_1024 1024
#define BUF_SIZE_2048 2048
//http 报文头部处理
 int __http_head_pro(http_server_t *http, int userId)
{
    LOG_SHOW("__http_head_pro start ...\n");
    
    int exist;
    int authOk;
    int len, lenTmp;
    httphead_line_t *line;

    httpUser_obj_t *user = &(http->user[userId]);
    
    //1、http 认证
    //const char str[] = "Authorization";
    lenTmp = strlen(HTTP_AUTH_STR_A);    
    link_obj_t *link = user->head.parse->link;

    //1.1 查看是否存在 "Authorization"
    exist = 0;
    LINK_FOREACH(link, probe)
    {
        httphead_line_t *iter = (httphead_line_t *)(probe->data);        
        len = strlen(iter->name);
        if(len > lenTmp)len = lenTmp;
        if(probe->en == 1 && strncmp(iter->name, HTTP_AUTH_STR_A, len) == 0) //第三个参数可能要取最小值
        {
            LOG_SHOW("存在 Authorization，可以开始认证 len:%d result:%d str:%s name:%s\n", len, strncmp(iter->name, HTTP_AUTH_STR_A, len), HTTP_AUTH_STR_A, iter->name);
            line = iter;
            exist = 1;
            break;
        }
            
    }   

    authOk = 0; //默认需要认证，无论是基础认证还是摘要认证，都要求认证
#define BUF_SIZE 64
#define BUF_VALUE_LEN 128
    char buf[BUF_SIZE + 8] = {0};
    char bufTmp[BUF_SIZE * 2 + 8] = {0};
    char bufValue[BUF_VALUE_LEN] = {0};
    char buf1024[BUF_SIZE_1024 + 8] = {0};
    
    //int ret;
    char *p1, *p2;
    char *find;
    int pos;
    int ret;
    int step;
    //int tmp;
    char *p;

    keyvalue_data_t *data_username;
    keyvalue_data_t *data_realm;
    keyvalue_data_t *data_nonce;
    keyvalue_data_t *data_uri;
    keyvalue_data_t *data_response;
    keyvalue_data_t *data_opaque;
    keyvalue_data_t *data_qop;
    keyvalue_data_t *data_nc;
    keyvalue_data_t *data_cnonce;
    
    if(exist == 1)
    {
        httphead_show_line(line); 
        
        //1.2 摘要认证（部分功能未实现，例如 opaque 更新策略）
        if(user->useDigestAuth == 1)
        {
            LOG_SHOW("摘要认证 ...\n");
            //user->auth.staus = digest_auth_status_wait_reply;   //测试用
            switch(user->auth.staus)    //状态机
            {
                case digest_auth_status_start:  //认证初始阶段，还没有向客户端发送认证请求
                {
                    ;   //无动作（默认认证失败）
                }break;
                case digest_auth_status_wait_reply: //已经向客户端发送认证要求，等待回复
                {
                    LOG_SHOW("///////////////////////////////////\n");
                    step = 0;
                    //1.2.1 找到包含 "Digest" 的关键字
                    len = strlen(HTTP_DIGWST_AUTH_DIGEST);
                    KEYVALUE_FOREACH_START(line->keyvalue, iter){
                        //LOG_SHOW("----->key:%s \n", iter->key);
                        if(iter->keyEn == 1 && iter->keyLen > len && iter->key != NULL)
                        {
                            //LOG_SHOW("----->key:%s \n", iter->key);
                            find = strstr(iter->key, HTTP_DIGWST_AUTH_DIGEST); 
                            if(find != NULL)    //找到了 "Digest" 
                            {
                                strcpy(buf, find + len);
                                p1 = buf;
                                strpro_clean_space(&p1);   //去空格
                                keyvalue_append_set_str(line->keyvalue, p1, iter->value);   //新增键值对
                                LOG_SHOW("新键值对 key:%s value:%s\n", p1, iter->value);
                                
                                step = 1;   //可以进入下一步
                            }

                        }
                    }KEYVALUE_FOREACH_END
                    
                    //1.2.2 获取相关键值对
                    if(step == 1)
                    {
                        data_username = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_USERNAME);
                        data_realm = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_REALM);
                        data_nonce = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_NONCE);
                        data_uri = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_URI);
                        data_response = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_RESPONSE);
                        data_opaque = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_MOPAQUE);
                        data_qop = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_QOP);
                        data_nc = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_NC);
                        data_cnonce = keyvalue_get_data_by_str(line->keyvalue, HTTP_DIGWST_AUTH_CNONCE);
                        
                        if(data_username != NULL && data_username->valueEn == 1 &&
                            data_realm != NULL && data_realm->valueEn == 1 &&
                            data_nonce != NULL && data_nonce->valueEn == 1 &&
                            data_uri != NULL && data_uri->valueEn == 1 &&
                            data_response != NULL && data_response->valueEn == 1 &&
                            data_opaque != NULL && data_opaque->valueEn == 1 &&
                            data_qop != NULL && data_qop->valueEn == 1 &&
                            data_nc != NULL && data_nc->valueEn == 1 &&
                            data_cnonce != NULL && data_cnonce->valueEn == 1 )
                                step = 2;   //可以进入下一步
                    }

                    //1.2.3 客户端上来的 nonce和 opaque 需要 和服务器本地 匹配
                    if(step == 2)
                    {
                        LOG_SHOW("客户端上来的 nonce:%s 本地:%s\n", data_nonce->value, user->auth.nonce);
                        if(data_nonce != NULL)
                        {
                            if(data_nonce->value != NULL && data_nonce->valueEn == 1)
                            {
                                strcpy(bufValue, data_nonce->value);
                                p1 = bufValue;
                                strpro_clean_by_ch(&p1, '"');    //去掉两边的双引号
                                
                                if(strcmp(p1, user->auth.nonce) == 0)
                                {
                                    
                                    //step = 3;
                                    step = 4;   
                                }  

                            }  
                        }   
                    }
                    
//                    if(step == 3)
//                    {
//                        if(data_opaque != NULL)
//                        {
//                            if(data_opaque->value != NULL && data_opaque->valueEn == 1)
//                            {
//                                strcpy(bufValue, data_opaque->value);
//                                p1 = bufValue;
//                                strpro_clean_by_ch(&p1, '"');    //去掉两边的双引号
//                                
//                                if(strcmp(p1, user->auth.opaque) == 0)
//                                {
//                                    
//                                    step = 4;
//                                }  
//
//                            }  
//                        }   
//                    }

                    //if(step == 2)step = 4;  //测试用
                    
                    //1.2.3 判断 nc 值是否符合要求（递增）
                    if(step == 4)
                    {
                        if(data_nc != NULL)
                        {
                            int count = (unsigned long)(atoi(data_nc->value));
                            LOG_SHOW("本地nc值:%d 报文nc值:%d\n", user->auth.nc, count);
                            if(count > user->auth.nc)   //需要验证上来的摘要认证内容 nc 在自增 
                            {
                                user->auth.nc = count;
                                step = 5;
                            }  
                        }   
                    }

                    //1.2.5 摘要值计算
                    if(step == 5)
                    {
                        char * pszNonce = (char *)(data_nonce->value);
                        char * pszCNonce = (char *)(data_cnonce->value);
                        char * pszUser = (char *)(data_username->value);
                        char * pszRealm = (char *)(data_realm->value);

                        strpro_clean_by_ch(&pszNonce, '"'); //去掉双引号
                        strpro_clean_by_ch(&pszCNonce, '"');
                        strpro_clean_by_ch(&pszUser, '"');  
                        strpro_clean_by_ch(&pszRealm, '"');

                        char *msgUri = (char *)(data_uri->value);
                        char *msgResponse = (char *)(data_response->value);
                        strpro_clean_by_ch(&msgUri, '"');  
                        strpro_clean_by_ch(&msgResponse, '"');
                        
                        //密码查找                         
                        keyvalue_get_value_by_str(http->account, pszUser, bufValue, BUF_VALUE_LEN);
                        char * pszPass = bufValue;                        
                        LOG_INFO("用户名：%s 密码：%s\n", pszUser, pszPass);
                                               
                        char * pszAlg = "md5";
                        char szNonceCount[9];   //8 hex digits 
                        snprintf(szNonceCount, 9, "%08d", atoi(data_nc->value) % 99999999);
                        
                        char * pszMethod = (char *)(user->head.method);
                        char * pszQop = (char *)(data_qop->value);
                        char * pszURI = msgUri;
                        HASHHEX HA1;
                        HASHHEX HA2 = "";
                        HASHHEX Response;

                        LOG_SHOW("计算摘要值所需参数：\n"
                                    "pszNonce:%s\n"
                                    "pszCNonce:%s\n"
                                    "pszUser:%s\n"
                                    "pszRealm:%s\n"
                                    "pszPass:%s\n"
                                    "pszAlg:%s\n"
                                    "szNonceCount:%s\n"
                                    "pszMethod:%s\n"
                                    "pszQop:%s\n"
                                    "pszURI:%s\n",
                                    pszNonce,
                                    pszCNonce,
                                    pszUser,
                                    pszRealm,
                                    pszPass,
                                    pszAlg,
                                    szNonceCount,
                                    pszMethod,
                                    pszQop,
                                    pszURI);

                        DigestCalcHA1(pszAlg, pszUser, pszRealm, pszPass, pszNonce, pszCNonce, HA1);
                        DigestCalcResponse(HA1, pszNonce, szNonceCount, pszCNonce, pszQop, pszMethod, pszURI, HA2, Response);
                        LOG_SHOW("Response = %s client Response:%s\n", Response, msgResponse);

                        if(strcmp(Response, msgResponse) == 0)
                        {
                            authOk = 1;
                            LOG_SHOW("摘要认证成功！\n");
                        }
                    }
                    
                    LOG_SHOW("step:%d\n", step);   
                    
                }break;
            }
        }

        
        //1.3 基本认证
        else
        {   
            //ret = keyvalue_get_value_by_str(line->keyvalue, "Basic base64", buf, BUF_SIZE);
            KEYVALUE_FOREACH_START(line->keyvalue, iter)
            {               
                len = strlen(HTTP_AUTH_STR_B);
                if(iter->keyEn == 1 && iter->keyLen > len )
                {
                    if(strncmp(iter->key, HTTP_AUTH_STR_B, len) == 0)
                    {
                        //1.3.1 获得账户密码
                        LOG_SHOW("基本认证     key:%s \n", iter->key);                        
                        strncpy(buf, iter->key + len, iter->keyLen - len);
                        p1 = buf;
                        strpro_clean_space(&p1); //去空格（参数应该是可以改变数值的指针，而不是const指针，或者数组名）
                        LOG_SHOW("base64:%s \n", p1);                        
                        p2 = bufTmp;
                        base64_decode((const char *)p1, &p2);
                        
                        LOG_SHOW("decode:%s \n", bufTmp);
                        
                        find = strstr(bufTmp, ":");

                        len = strlen(bufTmp);    //bufTmp 例子： "username:password"
                        if(find != NULL && len >= 3 )
                        {   
                            pos = find - bufTmp;
                            if(pos >= 1 && (len - pos) >= 2 )   //确保 用户名和密码不为空
                            {
                                bufTmp[pos] = '\0';
                                //1.3.2 验证账户和密码
                                ret = keyvalue_get_value_by_str(http->account, bufTmp, bufValue, BUF_VALUE_LEN);
                                LOG_SHOW("ret:%d find:【%s】 value:【%s】\n", ret, find + 1, bufValue);
                                if(strcmp(find + 1, bufValue) == 0)
                                {
                                    LOG_INFO("http 基础认证成功");
                                    authOk = 1;
                                }
                            }
                        } 

                        //break;
                    }
                }  
            }KEYVALUE_FOREACH_END   //遍历结束
            
        }

    }
    
    if(authOk == 0)     //认证失败，向客户端发送 请求认证报文
    {
        //1.4 认证失败，要求 摘要认证
        if(user->useDigestAuth == 1)
        {
            if(user->auth.staus == digest_auth_status_start)
                user->auth.staus = digest_auth_status_wait_reply;     //状态转移，等待客户端的报文
             
//             char str[] =   "HTTP/1.1 401 Unauthorized\n"
//                            "WWW-Authenticate: Digest realm=\"Restricted Area\","
//                            "qop=\"auth\","
//                            "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\"," 
//                            "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\","
//                            "qop=\"auth-int\"\n"
//                            "\n";

             
             p = buf1024;
             pos = 0;
             httpmsg_server_first_line(&p, BUF_SIZE_1024, &len, "http/1.1", "401", "Unauthorized");
             pos += len; 
             p = buf1024 + pos;
             httpmsg_server_digest_auth_append(&p, BUF_SIZE_1024 - pos, &len, &(user->auth));
             pos += len; 
             
             buf1024[pos++] = '\n';
             buf1024[pos++] = '\0';

             LOG_SHOW("认证失败，要求 摘要认证...\n");               
             http_send_str(http, userId, buf1024);
             //tcp_server_send(http->tcp, userId, str, strlen(str));

             
        }
        //1.5 认证失败，要求 基本认证
        else
        {
            char str[] =    "HTTP/1.1 401 Unauthorized\n"
                            "WWW-Authenticate: Basic realm=\"Restricted Area\"\n"
                            "Content-Type: text/html\n"
                            "Content-Length: 137\n"
                            "\n";
             LOG_SHOW("认证失败，要求基础认证...\n");               
             http_send_str(http, userId, str);
        }

        return RET_FAILD;   //认证失败，返回
    }
        
    

    return RET_OK;
    
}



/*==============================================================
                        应用（服务器）
==============================================================*/
//来自用户的数据接收（并且解析为 http 报文）
int http_data_accept(http_server_t *http, int userId)
{
    
    httpUser_obj_t *httpUser = &(http->user[userId]);
    tcp_server_t *tcp = http->tcp;
    tcpUser_obj_t *tcpUser = &(tcp->user[userId]);

    //if(httpUser->isCompleted == 1)return RET_FAILD;  //防止数据处理完成后又进入
    
    //int tmp;
    int ret;
    //unsigned char localBuf[HTTP_USER_BUF_SIZE + 8] = {0};
    //int len;
    int pos;
    int endPos;
    


    //接收数据（来自 tcp 接口的数据）
    //LOG_SHOW("新数据来了 bufReady:%d http len:%d tcp len:%d\n", tcpUser->bufReady, httpUser->bufLen, tcpUser->bufLen);
    pthread_mutex_lock(&(tcpUser->mutex));  //注意 锁的范围 应该合理                                 
    if(tcpUser->bufReady == 1)
    {
        
        //因为 http user 缓存大小设计为 tcp user 的两倍，所以可以分为前后A、B两个块
        //httpUser->bufLen  是之前剩余缓存的长度，新的数据放到后面
        if(httpUser->bufLen > TCP_USER_BUF_SIZE)    //要保证新的数据有足够空间，那么剩余缓存的长度控制到 A 块以内
        {
            pos = TCP_USER_BUF_SIZE - httpUser->bufLen;
            httpUser->bufLen = TCP_USER_BUF_SIZE;
            memmove(httpUser->buf, httpUser->buf + pos, httpUser->bufLen);               
        }  
        
        memcpy(httpUser->buf + httpUser->bufLen, tcpUser->buf, tcpUser->bufLen);   

        httpUser->bufLen += tcpUser->bufLen;               
        tcpUser->bufReady = 0;

        //tcpUser->bufLen = 0;  
    }
    pthread_mutex_unlock(&(tcpUser->mutex));

    LOG_SHOW("\n-------------------------------------\n");
    LOG_SHOW("status:%d dataLen:%d \ndataIn:【%s】\n\n", httpUser->status, httpUser->bufLen, httpUser->buf);
    
    //状态机
    switch(httpUser->status)   //状态变化？
    {
        case http_status_start: //开始
        {
            if(httpUser->bufLen > 0)    //说明有数据可用
            {              
                httpUser->status = http_status_recv_head_find; 
                
                http_data_accept(http, userId); //状态转移，然后重入
            }   
        }break;
        case http_status_recv_head_find:    //查找头部位置
        {
            if(httpUser->bufLen > 0)    //说明有数据可用
            {
                //printf("\n\n--------------- mark\n\n");
                ret = __http_message_first_line_parse(httpUser, &endPos);

                //LOG_SHOW("\nlen:%d\nbuf:%s\nret:%d endPos:%d\n\n", httpUser->bufLen, httpUser->buf, ret, endPos);

                //去掉处理过的字符
                httpUser->bufLen = httpUser->bufLen - endPos - 1;   //注：memmove 用于有重叠的内存块
                memmove(httpUser->buf, httpUser->buf + endPos + 1, httpUser->bufLen);   
                httpUser->buf[httpUser->bufLen] = '\0';     //可能要用到字符串 
                
                if(ret == RET_OK)  //找到了头部    
                {
                    //len = tcpUser->bufLen - ret;
                    //memcpy(localBuf, httpUser->buf + ret, len);
                    //memcpy(httpUser->buf, localBuf, len);
                    //tcpUser->bufLen = len;
                
                    httpUser->status = http_status_recv_head;
                    //预制初始值
                    http_data_accept(http, userId); //状态转移，然后重入
                }
                
            }
            
            
        }break;
        case http_status_recv_head: //处理头部
        {
            if(httpUser->bufLen > 0)    //说明有数据可用
            {
                //printf("\n\n--------------- mark\n\n");
                ret = __http_message_head_parse(httpUser, &endPos);

                //LOG_SHOW("\nlen:%d\nbuf:%s\nret:%d endPos:%d\n\n", httpUser->bufLen, httpUser->buf, ret, endPos);

                //去掉处理完成的字符
                if(endPos >= httpUser->bufLen - 1)endPos = httpUser->bufLen - 1;    //注意 endPos 不能超过缓存大小
                httpUser->bufLen = httpUser->bufLen - endPos - 1;   //注：memmove 用于有重叠的内存块
                memmove(httpUser->buf, httpUser->buf + endPos + 1, httpUser->bufLen);   
                httpUser->buf[httpUser->bufLen] = '\0';     //可能要用到字符串 


                httpUser->headByteCnt = httpUser->headByteCnt + endPos + 1;
                if(ret == RET_FAILD)    //没有找到空行
                {   
                    //如果处理的字数超过预定值，认为失败，需要回到 查找头部位置的状态
                    if(httpUser->headByteCnt >= HTTP_HEAD_BYTE_CNT_MAX_SIZE)
                    {
                        LOG_ALARM("http 头部处理的字节数超过预期 ...");
                        httpUser->status = http_status_recv_head_find;
                        http_data_accept(http, userId); //状态转移      ，然后重入
                    }   
                }
                else if(ret == RET_OK)  //找到了空行    
                {
                  
                    LOG_INFO("http 头部处理完毕 ...");
                    httpUser->status = http_status_head_pro;
                    http_data_accept(http, userId); //状态转移，然后重入
                }
            }
                
            
        }break;
        case http_status_head_pro:
        { 
            ret = __http_head_pro(http, userId);

            
//            if(ret == RET_FAILD)    //认证失败或者其他
//            {   
//                httpUser->status = http_status_recv_head_find;  //状态转移至 http_status_recv_head_find，然后重入
//                http_data_accept(http, userId); 
//            }
//            else
//            {
//                httpUser->status = http_status_recv_head_find;  //测试
//                http_data_accept(http, userId); 
//            }

            httpUser->status = http_status_recv_head_find;  //测试用

            
            
        }break;
        case http_status_recv_payload:
        {
            
        }break;
        case http_status_data_pro:
        {
            
        }break;
    }
    
    //httpUser->isCompleted = 1;     //处理完成的标志
    return RET_OK;
}



//数据接收线程 （被动接收）
void *thread_http_data_recv(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_http_data_recv");
        return NULL;
    }

    int i;
    //int tmp;
    
    http_server_t *http = (http_server_t *)in;

    tcp_server_t *tcp = http->tcp;
    if(tcp == NULL){LOG_ALARM("tcp");return NULL;}

    tcpUser_obj_t *tcpUserArray = tcp->user;
    //httpUser_obj_t *httpUserArray = http->user;
    
    //接收可用的数据
    while(1)
    {
        sem_wait(&(tcp->semBufReady));

        //LOG_SHOW("--->sem    wait\n");
    
        for(i = 0; i < TCP_USER_NUM; i++)
        {
            if(tcpUserArray[i].used == 1 )
            {
                //LOG_SHOW("被激活 tcp num ：%d\n", i);
                
                http_data_accept(http, i);
            }
        }
        
        //system("sleep 2");

    }
    
    return NULL;
}



//http 服务器 发送线程
//注：可能要考虑使用 poll 函数，确定哪些 fd 的读操作已经准备好（以后优化）
void * thread_http_data_send(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_http_data_send");
        return NULL;
    }

    int i;
    httpUser_obj_t *httpUser;
    //tcpUser_obj_t *tcpUser;
    
    http_server_t *http = (http_server_t *)in;
    httpUser_obj_t *httpUserArray = http->user;
    tcpUser_obj_t *tcpUserArray = http->tcp->user;

    //等待发送缓存准备好
    while(1)
    {
        sem_wait(&(http->semSendReady));

        //LOG_SHOW("--->sem    wait\n");
    
        for(i = 0; i < TCP_USER_NUM; i++)
        {
            if(tcpUserArray[i].used == 1 )  //对应的tcp接口可用
            { 
                httpUser = &(httpUserArray[i]);
                //tcpUser = &(tcpUserArray[i]);

                //接收数据（来自 tcp 接口的数据）
                pthread_mutex_lock(&(httpUser->sendMutex));  //注意 锁的范围 应该合理                                 
                if(httpUser->sendReady == 1)
                {                    
                    if(httpUser->sendBuf != NULL && httpUser->sendBufLen > 0)
                    {
                        //LOG_SHOW("http 服务器发送：%s\n", (char *)(httpUser->sendBuf));
                        tcp_server_send(http->tcp, i, (char *)(httpUser->sendBuf), httpUser->sendBufLen); 
                    }   
                        
                    httpUser->sendReady = 0;

                    pthread_cond_signal(&(httpUser->sendCond));     //给出发送完成的信号
                }
                pthread_mutex_unlock(&(httpUser->sendMutex));
                            
            }
        }
        
        //system("sleep 2");
    }
    
}


//http 服务器开启
int http_server_start(http_server_t *http)
{
    int ret;
    //int i;
    if(http == NULL)return RET_FAILD;
    //for(i = 0; i < HTTP_USER_NUM; i++)http->user[i].status = http_status_start;
    
    tcp_server_start(http->tcp);

    
    ret = pthread_create(&(http->data_accept), NULL, thread_http_data_recv, (void *)http);
    if(ret != 0){LOG_ALARM("thread_http_data_recv"); return ret;};

    ret = pthread_create(&(http->data_send), NULL, thread_http_data_send, (void *)http);
    if(ret != 0){LOG_ALARM("thread_http_data_send"); return ret;};
    

    return RET_OK;
}





/*==============================================================
                        应用（客户端）
==============================================================*/

/* 响应
Below is an example HTTP Response from an ACS containing a SOAP Request:

HTTP/1.1 200 OK
Content-Type: text/xml; charset="utf-8"
Content-Length: xyz
    
<soap:Envelope
	xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
	xmlns:cwmp="urn:dslforum-org:cwmp-1-2">
	<soap:Body>
		<cwmp:Request>
			<argument>value</argument>
		</cwmp:Request>
	</soap:Body>
</soap:Envelope>
*/


//http 响应报文头部 第一行寻找和解析
static int __http_client_msg_first_line_parse(http_client_t *client, int *endPos)
{
    if(client == NULL || endPos == NULL)return RET_FAILD;

    
    int step;
    int len;
    int posStart;
    int posEnter;
    char *find, *lastFind;
    char *find2;
    int pos;

    
    char line[HTTP_USER_BUF_SIZE + 8];
    char buf[HTTP_USER_BUF_SIZE + 8];
    memcpy(buf, client->buf, client->bufLen);
    buf[client->bufLen] = '\0';
    
    //参考 HTTP/1.1 200 OK 
    //三个步骤完成，才认为第一行数据有效

    posStart = 0;
    
    while(1)
    {
        step = 0;
        if(posStart >= client->bufLen)break;
        //1、找到换行标志
        find = strstr(buf + posStart, "\n");       
        if(find != NULL)
        {
            len = find - buf + 1;
            memcpy(line, buf + posStart, len);
            line[len] = '\0';
            posStart += len;
            posEnter = len - 1;
            
            step = 1;

            *endPos = posEnter;
        }
        else
        {
            *endPos = client->bufLen - 1;
            return RET_FAILD;  
        }
        

        //2、找到最后一个 HTTP/1.1，并获取 version、code、reason
        if(step == 1)   //上一步成功
        {
            find = NULL;
            pos = 0;
            len = strlen(HTTP_HEAD_FIND_KEYWORD);
            find2 = strstr(line, HTTP_HEAD_FIND_KEYWORD_B);
            if(find == NULL)find = find2;
            while(1)
            {
                lastFind = find;
                find = strstr(line + pos, HTTP_HEAD_FIND_KEYWORD);
                if(find == NULL)break;
                pos = find - line + len;
            }

            if(lastFind != NULL)
            {
                sscanf(lastFind, "%s%s%s", client->parse.version, client->parse.codeStr, client->parse.reason);
                step = 2;
            }

        }
          
        //3、分析version、code、reason 是否可用
        if(step == 2)
        {
            
            client->parse.code = atoi(client->parse.codeStr);
            if(client->parse.code <= 0)break;
                
            LOG_SHOW("解析头部 version:%s code:%d reason:%s\n", client->parse.version, client->parse.code, client->parse.reason);
            return RET_OK;
        }
        
    }
    return RET_FAILD; 
}

//参考 __http_message_head_parse
static int __http_client_msg_head_parse(http_client_t *client, int *endPos)
{
    if(client == NULL || endPos == NULL)return RET_FAILD;
    http_client_t *user= client;
    
    char *find;
    int findPos, newStartPos;
    //int step;
    //int len;

    int ret;
    char buf[HTTP_USER_BUF_SIZE + 8];   //http 用户缓存长度
    
    memcpy(buf, user->buf, user->bufLen);
    buf[user->bufLen] = '\0';

    //str = buf;
    ret = RET_FAILD;
    newStartPos = 0;
    while(1)
    {        
        find = strstr(buf + newStartPos, "\n");
        if(find == NULL)break;
        
        findPos = find - buf;
        newStartPos = findPos + 1;

        //找到http  头部和 负荷 的分界位置
        if((newStartPos <= user->bufLen && buf[newStartPos] == '\n'))
        {
            *endPos = newStartPos;
            ret = RET_OK;
            break;
        } 
        
        if((newStartPos + 1) <= user->bufLen  && buf[newStartPos] == '\r' && buf[newStartPos + 1] == '\n') 
        {
            *endPos = newStartPos + 1;
            ret = RET_OK;
            break;
        }
        

        //后面没有数据了
        if(newStartPos >= user->bufLen)
        {
            *endPos = findPos;
            break;
        }

        //后面还有数据，则进入下一个寻找 '\n' 的循环  
        *endPos = findPos;
    }

    if(findPos >= 1)
    {
        buf[findPos + 1] = '\0';
                           
        httphead_parse_head(client->parse.head, buf); 

        httphead_show(client->parse.head);
        
    }
    
    return ret;
}


//客户端响应信息的处理
#define HTTP_CLIENT_MSG_AUTH_REASON "Unauthorized"
#define HTTP_CLIENT_MSG_AUTH_NAME "WWW-Authenticate"
#define HTTP_CLIENT_MSG_AUTH_BASIC "Basic"
#define HTTP_CLIENT_MSG_AUTH_DIGEST "Digest"
static int __http_client_msg_recv_pro(http_client_t *client)
{

    //httphead_line_t *line;
    keyvalue_obj_t *keyvalue;
    enum {_isBasic, _isDigest, _noAuth} authType = _noAuth;
#define BUF_KEY_SIZE 32
    char bufkey[BUF_KEY_SIZE + 8] = {0};
#define BUF_AUTH_SIZE 3072      //注意大小
    char bufAuth[BUF_AUTH_SIZE + 8] = {0};    
    char *p;
    //char *find;
    int len;
    int pos;
    int lenBasic, lenDigest;
    
    LOG_SHOW(" 客户端 http 报文接收处理\n");

    if(client == NULL) return RET_FAILD;

    //1、认证要求的处理，例如 HTTP/1.1 401 Unauthorized
    httphead_head_t *head = client->parse.head;
    if(head == NULL)
    {
        LOG_ALARM("没有可用的头部解析");
        return RET_FAILD;
    }
    if(client->parse.code == 401 && strcmp(client->parse.reason, HTTP_CLIENT_MSG_AUTH_REASON) == 0)  //需要认证
    {
        
        LINK_FOREACH(client->parse.head->link, probe)
        {
            httphead_line_t *lineIter = (httphead_line_t *)(probe->data);
            if(lineIter != NULL)
            {
                if(strcmp(lineIter->name, HTTP_CLIENT_MSG_AUTH_NAME) == 0)
                {
                    //line = lineIter;
                    keyvalue = lineIter->keyvalue;
                    if(keyvalue != NULL)
                    {
                        KEYVALUE_FOREACH_START(keyvalue, iter) 
                        {
                            keyvalue_data_t *data = (keyvalue_data_t *)iter;
                            lenBasic = strlen(HTTP_CLIENT_MSG_AUTH_BASIC);
                            lenDigest = strlen(HTTP_CLIENT_MSG_AUTH_DIGEST);
                            
                            //1.1 基础认证     HTTP/1.1 401 Unauthorized \n WWW-Authenticate: Basic realm="Restricted Area"
                            if(data->keyEn == 1 && 
                                //data->valueEn == 1 && 
                                strncmp(HTTP_CLIENT_MSG_AUTH_BASIC, data->key, lenBasic) == 0)
                            {
                                LOG_SHOW("需要基础认证 ...\n");                 
                                authType = _isBasic;
                                client->useDigestAuth = 0;  //标记

                                break;
                            }

                            //1.2 摘要认证
                            /*
                                HTTP/1.1 401 Unauthorized
                                WWW-Authenticate: Digest realm="Restricted Area", \
                                qop="auth", nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", \
                                opaque="5ccc069c403ebaf9f0171e9517f40e41"
                                Content-Type: text/html
                                Content-Length: 137
                            */
                            else if(data->keyEn == 1 && 
                                //data->valueEn == 1 && 
                                strncmp(HTTP_CLIENT_MSG_AUTH_DIGEST, data->key, lenDigest) == 0)
                            {
                                LOG_SHOW("需要摘要认证 ...\n");
                                strcpy(bufkey, data->key);
                               
                                
                                p = bufkey + lenDigest;
                                strpro_clean_space(&p); //  去掉空格
                                keyvalue_append_set_str(keyvalue, p, data->value);  //添加键值对 
                                
                                client->useDigestAuth = 1;  //标记

                                authType = _isDigest;
                                break;

                            }
                            

                        }KEYVALUE_FOREACH_END;
                    }

                    break;
                }               
            }
        }
        
    }

    //测试：基本认证消息发送
    if(authType == _isBasic && client->auth.authFaildCnt < 3)
    {
        clientauth_updata(&(client->auth));  //更新 client->auth ，为认证做准备
        
        client->auth.authFaildCnt++;

        pos = 0;   
        p = bufAuth;
        httpmsg_client_first_line(&p, BUF_AUTH_SIZE, &len, "GET", "/cwmp", "http/1.1");
        pos += len;
        
        p += pos;
        httpmsg_client_basic_auth_append(&p, BUF_AUTH_SIZE - pos, &len, &(client->auth));
        pos += len;

        bufAuth[pos++] = '\n';
        bufAuth[pos++] = '\0';
        
        LOG_SHOW("发送认证测试 长度:%d 内容:%s\n", pos, bufAuth);
        tcp_client_send(client->tcp, bufAuth, pos);
        
    }
    //测试：摘要认证消息发送
    else if(authType == _isDigest && client->auth.authFaildCnt < 3)
    {  
        client->auth.authFaildCnt++;    //认证失败次数限制

        clientauth_updata(&(client->auth));  //更新 client->auth ，为认证做准备，主要是更新 cnonce 
        
        //------------------------------------------------------------------------------计算摘要值
        //1.2.1 从响应报文中获取 realm、nonce、opaque、qop                                
        keyvalue_data_t *data_realm;
        keyvalue_data_t *data_nonce;
        keyvalue_data_t *data_opaque;
        keyvalue_data_t *data_qop;

        keyvalue_show(keyvalue);
        
        data_realm = keyvalue_get_data_by_str(keyvalue, HTTP_DIGWST_AUTH_REALM);
        data_nonce = keyvalue_get_data_by_str(keyvalue, HTTP_DIGWST_AUTH_NONCE);
        data_opaque = keyvalue_get_data_by_str(keyvalue, HTTP_DIGWST_AUTH_MOPAQUE);
        data_qop = keyvalue_get_data_by_str(keyvalue, HTTP_DIGWST_AUTH_QOP); 

        if(data_realm != NULL && 
            data_nonce != NULL &&
            data_opaque != NULL &&
            data_qop != NULL)
        {
            LOG_SHOW("计算摘要认证 ...\n");
            //1.2.2 确定摘要认证所需的变量
            char * pszNonce = (char *)(data_nonce->value);
            char * pszCNonce = client->auth.cnonce;
            char * pszUser = client->auth.username;
            char * pszRealm = (char *)(data_realm->value);
            char * pszPass = client->auth.password;                                     
            char * pszAlg = "md5";


            if(strcmp(client->auth.oldNonce, pszNonce) == 0)
            {
                client->auth.nc++;  //nc需要自增，并告知服务器
            }
            else
            {
                strcpy(client->auth.oldNonce, pszNonce);
                client->auth.nc = 10;
            }
            char szNonceCount[9] = {0};  
            snprintf(szNonceCount, 9, "%08d", client->auth.nc % 9999999);
            
            char * pszMethod = "GET";   //测试用
            char * pszQop = (char *)(data_qop->value);
            char * pszURI = "/cwmp";    //测试用
            HASHHEX HA1;
            HASHHEX HA2 = "";
            HASHHEX Response;

            LOG_SHOW("计算摘要值所需参数：\n"
                                    "pszNonce:%s\n"
                                    "pszCNonce:%s\n"
                                    "pszUser:%s\n"
                                    "pszRealm:%s\n"
                                    "pszPass:%s\n"
                                    "pszAlg:%s\n"
                                    "szNonceCount:%s\n"
                                    "pszMethod:%s\n"
                                    "pszQop:%s\n"
                                    "pszURI:%s\n",
                                    pszNonce,
                                    pszCNonce,
                                    pszUser,
                                    pszRealm,
                                    pszPass,
                                    pszAlg,
                                    szNonceCount,
                                    pszMethod,
                                    pszQop,
                                    pszURI);
            
            DigestCalcHA1(pszAlg, pszUser, pszRealm, pszPass, pszNonce, pszCNonce, HA1);
            DigestCalcResponse(HA1, pszNonce, szNonceCount, pszCNonce, pszQop, pszMethod, pszURI, HA2, Response);
            LOG_SHOW("Response = %s\n", Response);


            //配置认证对象 auth
            /*
            GET /protected/resource HTTP/1.1
            Host: example.com
            Authorization: Digest username="user", \
            realm="Restricted Area", \
            nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", \ 
            uri="/protected/resource", \
            response="6629fae49393a05397450978507c4ef1", \
            opaque="5ccc069c403ebaf9f0171e9517f40e41", \
            qop=auth, \
            nc=00000001, \
            cnonce="0a4f113b"
            */

            //把摘要认证所需参数存入本地
            strcpy(client->auth.realm, data_realm->value);  //来自于服务器
            strcpy(client->auth.opaque, data_opaque->value);
            strcpy(client->auth.qop, data_qop->value);
            strcpy(client->auth.nonce, data_nonce->value);

            
            
            strcpy(client->auth.uri, pszURI);
            strcpy(client->auth.response, Response);
            //------------------------------------------------------------------------------

            pos = 0;   
            p = bufAuth;
            httpmsg_client_first_line(&p, BUF_AUTH_SIZE, &len, pszMethod, pszURI, "http/1.1");
            pos += len;
            
            p += pos;
            httpmsg_client_digest_auth_append(&p, BUF_AUTH_SIZE - pos, &len, &(client->auth));
            pos += len;

            bufAuth[pos++] = '\n';
            bufAuth[pos++] = '\0';
            
            LOG_SHOW("发送认证测试 长度:%d 内容:%s\n", pos, bufAuth);
            tcp_client_send(client->tcp, bufAuth, pos);

            
            //char tmpstr[] = "ok..........\n\n";
            //tcp_client_send(client->tcp, tmpstr, strlen(tmpstr));
        }
        else
        {
           LOG_ALARM("解析realm、nonce、opaque、qop出现问题，无法完成 摘要认证");
           
        }
        
    }


    //测试发送数据
//    httpClient_msg_t *msg = httpClient_create();
//
//    httpmsg_client_head_init(msg, "GET", "/CWMP", "HTTP/1.1", client->useDigestAuth, &(client->auth));
//    
//    if(msg->keyvalueEn == 1)
//    {
//        keyvalue_append_set_str(msg->keyvalue, "Content-Type", "text/html");
//        keyvalue_append_set_str(msg->keyvalue, "Content-Length", "137");     
//    }
//    httpmsg_client_send_msg(msg, client->tcp);
//    
//    httpCLient_destroy(msg);
 
    return RET_OK;
}   



//来自用户的数据接收（并且解析为 http 报文）
int http_client_accept(http_client_t *client)
{
    tcp_client_t *tcp = client->tcp;


    //int tmp;
    int ret;
    //unsigned char localBuf[HTTP_USER_BUF_SIZE + 8] = {0};
    //int len;
    int pos;
    int endPos;

    //接收数据（来自 tcp 接口的数据）
    pthread_mutex_lock(&(tcp->mutex));                               
    if(tcp->bufReady == 1)  //有新的数据准备好了
    {        
        //缓存有A、B块，老数据如果大于 A 块，则缓存不够，不能进行数据更新，
        //不然会造成数据丢失，需要及时处理
        if(client->bufLen <= TCP_USER_BUF_SIZE)  
        {
            memcpy(client->buf + client->bufLen, tcp->buf, tcp->bufLen);    //新数据   
            client->bufLen += tcp->bufLen;               
            tcp->bufReady = 0;
            tcp->bufLen = 0;
        }
        else    //超过TCP_USER_BUF_SIZE 的未处理数据丢弃
        {
            LOG_ALARM("缓存已满未及时处理，超过部分被丢弃");
            pos = client->bufLen - TCP_USER_BUF_SIZE;
            client->bufLen = TCP_USER_BUF_SIZE;
            memmove(client->buf, client->buf + pos, TCP_USER_BUF_SIZE);
        }
    }
    pthread_mutex_unlock(&(tcp->mutex));

    LOG_SHOW("\n-------------------------------------\n");
    LOG_SHOW("status:%d dataLen:%d \ndataIn:【%s】\n\n", client->status, client->bufLen, client->buf);

    //状态机
    switch(client->status)   //状态变化？
    {
        case http_status_start: //开始
        {
            if(client->bufLen > 0)    //说明有数据可用
            {              
                client->status = http_status_recv_head_find; 
                
                http_client_accept(client); //状态转移，然后重入
            }   
        }break;
        case http_status_recv_head_find:    //查找头部位置
        {
            if(client->bufLen > 0)    //说明有数据可用
            {
                endPos = 0;
                ret = __http_client_msg_first_line_parse(client, &endPos);

                //LOG_SHOW("\nlen:%d\nbuf:%s\nret:%d endPos:%d\n\n", client->bufLen, client->buf, ret, endPos);

                //去掉处理过的字符
                client->bufLen = client->bufLen - endPos - 1;   //注：memmove 用于有重叠的内存块
                memmove(client->buf, client->buf + endPos + 1, client->bufLen);   
                client->buf[client->bufLen] = '\0';     //可能要用到字符串 
                
                if(ret == RET_OK)  //找到了头部    
                {
                    client->status = http_status_recv_head;
                    client->headByteCnt = 0;
                    http_client_accept(client); //状态转移，然后重入
                }
            }
            
            
        }break;
        case http_status_recv_head: //处理头部
        {
            if(client->bufLen > 0)    //说明有数据可用
            {
                ret = __http_client_msg_head_parse(client, &endPos);

                //LOG_SHOW("\n头部处理 len:%d\nbuf:%s\nret:%d endPos:%d\n\n", httpUser->bufLen, httpUser->buf, ret, endPos);

                //去掉处理完成的字符
                if(endPos >= client->bufLen - 1)endPos = client->bufLen - 1;    //注意 endPos 不能超过缓存大小
                client->bufLen = client->bufLen - endPos - 1;   
                memmove(client->buf, client->buf + endPos + 1, client->bufLen);   
                client->buf[client->bufLen] = '\0';     //可能要用到字符串 


                client->headByteCnt = client->headByteCnt + endPos + 1;
                if(ret == RET_FAILD)    //没有找到空行
                {   
                    //如果处理的字数超过预定值，认为失败，需要回到 查找头部位置的状态
                    if(client->headByteCnt >= HTTP_HEAD_BYTE_CNT_MAX_SIZE)
                    {
                        LOG_ALARM("http 头部处理的字节数超过预期 ...");
                        client->status = http_status_recv_head_find;
                        http_client_accept(client); //状态转移      ，然后重入
                    }   
                }
                else if(ret == RET_OK)  //找到了空行    
                {
                  
                    LOG_INFO("http 头部处理完毕 ...");
                    client->status = http_status_head_pro;
                    http_client_accept(client); //状态转移，然后重入
                }
            }
                    
        }break;
        case http_status_head_pro:
        { 
            __http_client_msg_recv_pro(client);

            client->status = http_status_recv_head_find;    //测试
            http_client_accept(client); 
            
            
        }break;
        case http_status_recv_payload:
        {
            ;
        }break;
        case http_status_data_pro:
        {
            ;
        }break;
    }    

    return RET_OK;
}



//数据接收线程 （被动接收）
void *thread_http_client_recv(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_http_client_recv");
        return NULL;
    }

    http_client_t *client = (http_client_t *)in;

    tcp_client_t *tcp = client->tcp;
    if(tcp == NULL){LOG_ALARM("tcp");return NULL;}

    
    //接收可用的数据
    while(1)
    {
        sem_wait(&(tcp->semBufReady));

        //LOG_SHOW("--->sem    wait\n");
        http_client_accept(client);       
    }    
    return NULL;
}





//http 客户端开启
int http_client_start(http_client_t *client, char *ipv4, int port, char *targetIpv4, int targetPort)
{
    int ret;
    if(client == NULL)return RET_FAILD;

    client->tcp = tcp_client_create(ipv4, port);  

    ret = tcp_client_start(client->tcp, targetIpv4, targetPort);
     if(ret != 0){LOG_ALARM("tcp_client_start"); return ret;}
    
    ret = pthread_create(&(client->dataAccept), NULL, thread_http_client_recv, (void *)client);
    if(ret != 0){LOG_ALARM("thread_http_client_recv"); return ret;}


    return RET_OK;
}



/*==============================================================
                        测试
==============================================================*/


void http_test()
{

    
    http_server_t *http = http_create_server("192.168.1.20", 8080); 

    LOG_INFO("http_test start ......");

    http_server_start(http);

    for(;;);
    
}



void http_client_test(char *ipv4, int port, char *targetIpv4, int targetPort)
{

    http_client_t *client = http_create_client();

    if(RET_OK != http_client_start(client, ipv4, port, targetIpv4, targetPort))return; 

    LOG_SHOW("http 客户端开启 start ......\n");
    //pool_show();
    
//    char sendStr[] = "GET /protected/resource HTTP/1.1\n"
//                "Host: example.com\n"
//                "Authorization: Digest username=\"user\", "
//                "realm=\"Restricted Area\", "
//                "nonce=\"af8c7760297db973dfbbbe1d268cb6bd\", "
//                "uri=\"/protected/resource\", "
//                "response=\"6629fae49393a05397450978507c4ef1\", "
//                "opaque=\"8c85f23cbe747fe648bb2d130f2986a3\", "
//                "qop=auth, "
//                "nc=00000101, "
//                "cnonce=\"0a4f113b\" "
//                "\n\n";
                
    char sendStr[] = "GET /protected/resource HTTP/1.1\n"
                "Host: example.com\n"
                "\n\n\n";

    tcp_client_send(client->tcp, sendStr, strlen(sendStr));
    //tcp_client_send(client->tcp, "123", 3);
    
    

    for(;;);
    
}





