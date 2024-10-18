

//注：协议所用到的 http 是 HTTP 1.1版本 ，文档 tr-069-1-6-1.pdf 有所描述


#include <string.h>

#include "http.h"
#include "tcp.h"
#include "log.h"
#include "keyvalue.h"




/*=============================================================
                        数据结构
==============================================================*/

#define HTTP_HEAD_DATA_MAX_SIZE 1024
#define HTTP_PAYLOAD_BUF_SIZE 512

#define HTTP_USER_NUM TCP_USER_NUM  //用户个数
#define HTTP_USER_BUF_SIZE (TCP_USER_BUF_SIZE * 2)  //用户缓存长度


//http头部解析相关    
#define HTTP_METHOD_STRING_SIZE 32      //请求方法
#define HTTP_URL_STRING_SZIE 256        //资源请求定位 最多字 符个数，256 可能有点小了...
#define HTTP_VERSION_STRING_SIZE 32     //http版本
#define HTTP_HEAD_KEYVALUE_MAX_SIZE 30  //键值对最大个数
#define HTTP_HEAD_BYTE_CNT_MAX_SIZE 2048    //【谨慎设置】处理的头部数据字节计数（不包括第一行），阈值



//请求方法 关键字
const char *http_requst_method[] = { "GET", "POST", "PUT", "DELETE", 
                                        "HEAD", "CONNECT", "OPTIONS", 
                                        "TRACE", "PATCH" };


//http头部
typedef struct http_head_t{
    int len;
    //unsigned char data[HTTP_HEAD_DATA_MAX_SIZE];    //存储http头部信息，这里有最大值的限制

    //头部内容解析
    //1、第一行
    char method[HTTP_METHOD_STRING_SIZE + 8];  //方法，加冗余
    char url[HTTP_URL_STRING_SZIE + 8];
    char version[HTTP_VERSION_STRING_SIZE + 8];
    
    //2、键值对
    keyvalue_obj_t *keyvalue;   //需要动态分配内存
       
}http_head_t;

//http净荷
typedef struct http_payload_t{
    int len;
    unsigned char buf[HTTP_PAYLOAD_BUF_SIZE];   //buf是缓存区，并不存储所有的净荷
}http_payload_t;


//接收数据状态机
typedef enum{
    http_status_start,
    http_status_recv_head_find, //htpp头部位置 查找
    http_status_recv_head,
    http_status_head_pro,
    http_status_recv_payload,
    http_status_data_pro
}http_status_e;

//http 用户对象
typedef struct httpUser_obj_t{

    //是否被初始化和使能
    //char en;
     
    http_status_e status;   //http头部处理状态

    //接收到的数据
    http_head_t head;
    http_payload_t payload;
    
    //缓存
    unsigned char buf[HTTP_USER_BUF_SIZE + 8];    //是tcp user 缓存的两倍，可以看做两个块
    int bufLen;
    //char isCompleted;    //缓存处理完成（1表示完成）

    //处理的头部数据字节计数（不包括第一行），阈值 HTTP_HEAD_BYTE_CNT_MAX_SIZE
    int headByteCnt;    
    
}httpUser_obj_t;


//http 作为服务器
typedef struct http_server_t{    

    tcp_server_t *tcp;

    //用户
    httpUser_obj_t user[HTTP_USER_NUM];
    
    //线程
    pthread_t data_accept;
}http_server_t;


//http 作为客户端
typedef struct http_client_t{
    http_head_t head;
    http_payload_t payload;

    tcp_client_t *tcp;    
}http_client_t;




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

    keyvalue_init();
}


/*==============================================================
                        对象
==============================================================*/

//创建一个http server
http_server_t *http_create_server(char *ipv4, int port)
{
    http_server_t *http = (http_server_t *)POOL_MALLOC(sizeof(http_server_t));
    if(http != NULL)
    {
        http->tcp = tcp_server_create(ipv4, port);
    }
    int i;
    //http用户 初始化
    for(i = 0; i < HTTP_USER_NUM; i++)
    {
        http->user[i].status = http_status_start;   //状态机处于初始状态

        //http 头部解析键值对 分配内存
        http->user[i].head.keyvalue = keyvalue_create(HTTP_HEAD_KEYVALUE_MAX_SIZE);
        
    }
    
    
    http_local_mg.serverCnt++;
    
    return http;
}

//摧毁一个http server
void http_destory_server(http_server_t *http)
{
    if(http == NULL)return;
    int i;

    //释放http user 成员
    for(i = 0; i < HTTP_USER_NUM; i++)
    {
        if(http->user[i].head.keyvalue != NULL)
            keyvalue_obj_destory(http->user[i].head.keyvalue);
    }
        
    //释放自己
    POOL_FREE(http);   
}

//创建一个http client
http_client_t *http_create_client()
{
    http_client_t *client = (http_client_t *)POOL_MALLOC(sizeof(http_client_t));
    http_local_mg.clientCnt++;
    
    return client;
}

//摧毁一个http client
void http_destory_client(http_client_t *client)
{
    if(client == NULL)return ;

    //int i;
    //释放 成员
    
    
                    
    //释放自己
    POOL_FREE(client);
}


/*==============================================================
                        http 报文处理
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


//确定头部第一行位置，并且解析
static int __find_http_first_line(httpUser_obj_t *user, int *endPos)
{
    if(user == NULL)return RET_FAILD;
    int step;
    int i;
    int num;
    int len, lenVersion, lenMethod;
    int posStart;
    int posMethod, posVersion, posEnter;
    char *find;
    int pos;
    
    char line[HTTP_USER_BUF_SIZE + 8];
    char buf[HTTP_USER_BUF_SIZE + 8];
    memcpy(buf, user->buf, user->bufLen);
    buf[user->bufLen] = '\0';
    
    //参考 GET /index.html HTTP/1.1  
    //三个步骤完成，才认为第一行数据有效

    posStart = 0;
    
    while(1)
    {
        step = 0;
        if(posStart >= user->bufLen)break;
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
            
        }
        else
        {
            *endPos = user->bufLen - 1;
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
            if(find != NULL)
            {
                lenVersion = strlen(HTTP_HEAD_FIND_KEYWORD);
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
                if(len > 0)strncpy(user->head.url, line + pos, len);

                //结束的位置
                *endPos = posEnter;

                //LOG_INFO("method:%s url:%s version:%s", user->head.method, user->head.url, user->head.version);
                return RET_OK;
            }
        }
    }
    return RET_FAILD;     
}


//http 头部的键值信息，数据在 http user 的缓存中
//返回值 RET_OK：找到了空行               RET_FAILD：没找到空行
static int __get_http_head_keyvalue(httpUser_obj_t *user, int *endPos)
{
//    if(user == NULL || endPos == NULL)return RET_FAILD;
//    char *find;
//    
//    char buf[HTTP_USER_BUF_SIZE + 8];
//    char line[HTTP_USER_BUF_SIZE + 8];
//
//    int step;
//    int len;
//    int posStart, posEnter;
//    
//    memcpy(buf, user->buf, user->bufLen);
//    buf[user->bufLen] = '\0';
//
//    while(1)
//    {
//        step = 0;
//        find = strstr(buf, "\n");
//
//        //1、找到换行标志
//        if(find != NULL)
//        {
//            len = find - buf + 1;
//            memcpy(line, buf + posStart, len);
//            line[len] = '\0';
//            posStart += len;
//            posEnter = len - 1;
//            
//            step = 1;
//            
//        }
//        else
//        {
//            *endPos = user->bufLen - 1;
//            return RET_FAILD;
//        }
//
//        //2、每一行的信息表位键值对
//        
//    }
//    
    


    return RET_OK;
}


/*==============================================================
                        应用
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
    


    //接收数据
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
                ret = __find_http_first_line(httpUser, &endPos);

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
                ret = __get_http_head_keyvalue(httpUser, &endPos);

                //LOG_SHOW("\nlen:%d\nbuf:%s\nret:%d endPos:%d\n\n", httpUser->bufLen, httpUser->buf, ret, endPos);

                //去掉处理完成的字符
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
void *thread_http_data_accept(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_http_data_pro");
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

                //pthread_mutex_lock(&(tcpUserArray[i].mutex));  //注意 锁的范围 应该合理                                                   
                //if(tcpUserArray[i].bufReady == 1)httpUserArray[i].isCompleted = 0;
                //pthread_mutex_unlock(&(tcpUserArray[i].mutex));
                //LOG_SHOW("\n【thread_http_data_accept:%s】\n", tcpUserArray[i].buf);
                
                http_data_accept(http, i);
            }
        }
        
        system("sleep 2");

    }
    
    return NULL;
}



//http 服务器开启
int http_server_start(http_server_t *http)
{
    int ret;
    //int i;
    if(http == NULL)return RET_FAILD;
    //for(i = 0; i < HTTP_USER_NUM; i++)http->user[i].status = http_status_start;

    

    tcp_server_start(http->tcp);

    
    ret = pthread_create(&(http->data_accept), NULL, thread_http_data_accept, (void *)http);
    if(ret != 0){LOG_ALARM("http_server_start"); return ret;};

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








