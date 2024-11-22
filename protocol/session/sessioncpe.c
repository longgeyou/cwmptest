




#include <stdio.h>
#include <stdlib.h>

#include "sessioncpe.h"
#include "log.h"
#include "pool2.h"
#include "http.h"
#include "queue.h"
#include "httpmsg.h"




/*==============================================================
                        数据结构
==============================================================*/

//会话状态机
typedef enum sessioncpe_status_e{
    sessioncpe_status_start,    //开始
    sessioncpe_status_establish_connection,     //建立连接
    sessioncpe_status_send,     //发送状态
    sessioncpe_status_wait_recv,    //等待接收状态
    sessioncpe_status_disconnect,   //会话完成，请求断开状态
    sessioncpe_status_end   //结束
}sessioncpe_status_e; 

//会话模式
//typedef enum sessioncpe_mode_e{
//    //sessioncpe_mode_test,    //测试模式
//    sessioncpe_mode_spare,     //空闲模式，被动接收消息并处理
//    sessioncpe_mode_command     //命令模式，主动向对端发送rpc方法
//}sessioncpe_mode_e; 

    
//cpe 会话对象
typedef struct sessioncpe_obj_t{    

    http_client_t *client; 
    int cwmpid;  
    
    //状态机     
    sessioncpe_status_e status;    

    //线程
    pthread_t threadRecv;  //接收线程
    pthread_t threadSession;
    
    //soap 信息队列
    queue_obj_t *msgQueue;    

    //模式，在不同模式下会话有不同的行为
    //int mode;
    //char commandName[128];  //命令模式下，指示命令的名称，例如

    
    
}sessioncpe_obj_t;
 

/*==============================================================
                        本地管理
==============================================================*/
#define SESSIONCPE_INIT_CODE 0x8080

typedef struct sessioncpe_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    
    int initCode;
}sessioncpe_manager_t;

sessioncpe_manager_t sessioncpe_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(sessioncpe_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define SESSIONCPE_POOL_NAME "sessioncpe"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void sessioncpe_mg_init()
{    
    if(sessioncpe_local_mg.initCode == SESSIONCPE_INIT_CODE)return ;   //防止重复初始化
    sessioncpe_local_mg.initCode = SESSIONCPE_INIT_CODE;
    
    strncpy(sessioncpe_local_mg.poolName, SESSIONCPE_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    sessioncpe_local_mg.poolId = pool_apply_user_id(sessioncpe_local_mg.poolName); 

    pool_init();    //依赖于pool2.c，所以要初始化
    http_mg_init();
    queue_mg_init();
}


/*==============================================================
                        对象
==============================================================*/
#define SESSION_CPE_MSG_QUEUE_SIZE 10
//创建会话 
sessioncpe_obj_t *sessioncpe_create()
{
    sessioncpe_obj_t *session = (sessioncpe_obj_t *)MALLOC(sizeof(sessioncpe_obj_t));

    if(session == NULL)return NULL;

    session->msgQueue = queue_create(SESSION_CPE_MSG_QUEUE_SIZE);
    session->status = sessioncpe_status_start;
    session->cwmpid = 0;

    return session;
}

//创建会话，同时给定 http 客户端指针
sessioncpe_obj_t *sessioncpe_create_set_poniter(http_client_t *client)
{
    sessioncpe_obj_t *session = sessioncpe_create();
    if(session == NULL)return NULL;

    session->client = client;

    return session;
}

//
//创建会话，同时创建 http 客户端
sessioncpe_obj_t *sessioncpe_create_and_client()
{
    sessioncpe_obj_t *session = sessioncpe_create();
    if(session == NULL)return NULL;

    session->client = http_create_client();

    return session;
}

//摧毁会话对象
void sessioncpe_destroy(sessioncpe_obj_t *session)
{
    if(session == NULL)return ;

    //线程回收
    //pthread_cancel(session->threadRecv);
    //pthread_cancel(session->threadSession);

    //摧毁消息队列，并且释放指向的数据
    queue_destroy_and_data(session->msgQueue);
    
    //释放自己
    FREE(session);
}



/*==============================================================
                        应用
==============================================================*/
//消息接收线程
void *thread_sessioncpe_recv(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_http_client_recv in is  NULL");
        return NULL;
    }
    //sessioncpe_obj_t *session = (sessioncpe_obj_t *)in;

    while(1)
    {

        ;
    }
    
    return NULL;
}



//认证进度
int __http_auth(sessioncpe_obj_t *session)
{
    char buf2048[2048] = {0};
    if(session == NULL)return -1;
    //发送简单 http 信息给acs，看是否要认证
    char *p = buf2048;
    httpmsg_client_say_hello(p, 2048);
    http_client_send(session->client, buf2048, 2048);

    //等待接收回复
    

    
   
    return 0;
}



//会话线程
void *thread_sessioncpe(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_sessioncpe: in is NULL");
        return NULL;
    }

    sessioncpe_obj_t *session = (sessioncpe_obj_t *)in;

    //while(1)
    {
        switch(session->status)     //状态机
        {
            case sessioncpe_status_start:   //开始，需要确定http认证
            {
                __http_auth(session);

            }break;
            case sessioncpe_status_establish_connection:
            {

            }break;
            case sessioncpe_status_send:
            {

            }break;
            case sessioncpe_status_wait_recv:
            {

            }break;
            case sessioncpe_status_disconnect:
            {

            }break;
            case sessioncpe_status_end:
            {

            }break;

        } 

        //system("sleep 2");
    }

    return NULL;
}




//开启一个cpe 会话
int sessioncpe_start(sessioncpe_obj_t *session, char *ipv4, int port, char *targetIpv4, int targetPort)
{

    if(session == NULL)return RET_FAILD;
    int ret;
    ret = http_client_start(session->client, ipv4, port, targetIpv4, targetPort);
    if(ret != RET_OK)
    {
        LOG_ALARM("http_client_start faild");
        return RET_FAILD;
    }


    //开启接收线程
    //ret = pthread_create(&(session->threadRecv), NULL, thread_sessioncpe_recv, (void *)session);
    //if(ret != RET_OK){LOG_ALARM("thread_sessioncpe_recv");return ret;}

 
    //开启会话线程
    session->status = sessioncpe_status_start;
    ret = pthread_create(&(session->threadSession), NULL, thread_sessioncpe, (void *)session);
    if(ret != RET_OK)
    {
        LOG_ALARM("thread_sessioncpe_recv");
        return ret;
    }
   

    return 0;
}



/*==============================================================
                        测试
==============================================================*/
void sessioncpe_test(char *ipv4, int port, char *targetIpv4, int targetPort)
{
    //printf("cpe ssession test ... \n");

    sessioncpe_obj_t *session = sessioncpe_create_and_client();  
    sessioncpe_start(session, ipv4, port, targetIpv4, targetPort);

    while(1)
    {
        ;
    }
    
}








