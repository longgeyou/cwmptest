


//acs 会话


#include <stdio.h>

#include "sessionacs.h"
#include "log.h"
#include "pool2.h"
#include "http.h"
#include "link.h"
#include "queue.h"
#include "httpmsg.h"
#include "soap.h"
#include "soapmsg.h"


/*==============================================================
                        数据结构
==============================================================*/
//消息队列的数据元，用于存储 soap 信封
typedef struct sessionacs_msgdata_t{
    void *d;    //数据指针
    int len;    //长度
}sessionacs_msgdata_t;

/*------------------------------------------------------------任务队列*/
//任务类型
typedef enum sessionacs_task_type_e{       
    sessionacs_task_type_rpc_request,   //请求对方，这里是acs调用cpe方法
    sessionacs_task_type_rpc_response,   //响应对方，这里是响应 cpe 的请求
    sessionacs_task_type_other  //其他任务，有待拓展
}sessionacs_task_type_e;

//1、sessionacs_task_type_rpc_request 类型  下的任务数据
typedef enum sessionacs_task_rpc_request_status_e{
    sessionacs_task_rpc_request_status_start,   
    sessionacs_task_rpc_request_status_send,   
    sessionacs_task_rpc_request_status_getResposne,
    sessionacs_task_rpc_request_status_end
}sessionacs_task_rpc_request_status_e;

typedef struct sessionacs_task_data_rpc_request_t{
    //状态机
    sessionacs_task_rpc_request_status_e status;

    char method[64];
    soap_node_t *soapNode;  //soap 节点，用于存储rpc request方法
    soap_node_t *soapNodeResponse;  //soap 节点，用于存储对方回复的数据
    //char soapNodeEn;
}sessionacs_task_data_rpc_request_t;

//sessionacs_task_type_rpc_response 类型  下的任务数据
typedef enum sessionacs_task_rpc_response_status_e{
    sessionacs_task_rpc_response_status_start,   
    sessionacs_task_rpc_response_status_response,   
    sessionacs_task_rpc_response_status_end  
}sessionacs_task_rpc_response_status_e;

typedef struct sessionacs_task_data_rpc_response_t{
    //状态机
    sessionacs_task_rpc_response_status_e status;

    char method[64];
    soap_node_t *soapNode;  //soap 节点，用于存储回复对方的数据
    //char soapNodeEn;
}sessionacs_task_data_rpc_response_t;



//任务队列的数据元，用于存储 会话拆分成的一个个小任务
typedef struct sessionacs_task_t{
    sessionacs_task_type_e type;
    union{ //共用体，不同任务类型所需的数据
        sessionacs_task_data_rpc_request_t rpcRequest;  
        sessionacs_task_data_rpc_response_t rpcResponse;
    }data;       
}sessionacs_task_t;
/*------------------------------------------------------------任务队列*/

//会话状态机
typedef enum sessionacs_status_e{
    sessionacs_status_start,    //开始
    sessionacs_status_establish_connection,     //建立连接
    //sessionacs_status_send,     //发送状态
    //sessionacs_status_wait_recv,    //等待接收状态
    sessionacs_status_task,     //任务处理状态
    sessionacs_status_task_empty,   //任务为空
    sessionacs_status_disconnect,   //会话完成，请求断开状态
    sessionacs_status_end,   //结束
    sessionacs_status_error    //错误
}sessionacs_status_e;

//acs 会话对象的 子成员
#define SESSION_ACS_MEMBER_MSG_QUEUE_SIZE 10
#define SESSION_ACS_MEMBER_TASK_QUEUE_SIZE 10

typedef struct sessionacs_obj_t sessionacs_obj_t;    //声明
typedef struct sessionacs_member_t{
    sessionacs_status_e status;     //状态机
    httpUser_obj_t *user;   //htt用户

    //cwmp:ID 号
    //我理解为一个用户同时只有一次会话，每次会话只有一个cwmpid 事务进行，只有
    //等待上一次事务所结束后才能进行下一次事务，或者另一个cwmpid（当然可能理
    //解不对）看情况吧，以后可能要改动
    int cwmpid;   

    //soap 信息队列
    queue_obj_t *msgQueue;
    pthread_mutex_t mutexMsgQueue; //互斥锁，保护msgQueue资源的互斥锁
    int msgRecvEn;  //控制消息队列的接收行为，1表示可以接收，0表示不允许
    pthread_cond_t condMsgQueueReady;   //条件变量，表示有消息

    //任务队列
    queue_obj_t *taskQueue;
    pthread_mutex_t mutexTaskQueue; //互斥锁，保护 taskQueue 资源的互斥锁
    char timeout;   //超时标志
    char timeoutValue;    //超时的数值，单位是秒

    //线程
    pthread_t threadSession;    //会话线程
    pthread_t threadMsgRecv;    //消息接收线程


    //指向包含自己的sessionacs_obj_t
    sessionacs_obj_t *session;
}sessionacs_member_t;


//acs 会话对象
typedef struct sessionacs_obj_t{    
    http_server_t *http;    //http服务器接口，用于发送数据
    
    link_obj_t *memberLink;    //成员链表，成员类型为 sessionacs_member_t

    int cwmpidCnt;  //通过递增来区别不同的cwmpid

    //线程
    pthread_t threadNewUser;  //接收线程
    

    
}sessionacs_obj_t;
 

/*==============================================================
                        本地管理
==============================================================*/
#define SESSIONACS_INIT_CODE 0x8080

typedef struct sessionacs_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    
    int initCode;
}sessionacs_manager_t;

sessionacs_manager_t sessionacs_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(sessionacs_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define SESSIONACS_POOL_NAME "sessionacs"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void sessionacs_mg_init()
{    
    if(sessionacs_local_mg.initCode == SESSIONACS_INIT_CODE)return ;   //防止重复初始化
    sessionacs_local_mg.initCode = SESSIONACS_INIT_CODE;
    
    strncpy(sessionacs_local_mg.poolName, SESSIONACS_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    sessionacs_local_mg.poolId = pool_apply_user_id(sessionacs_local_mg.poolName); 

    pool_init();    //依赖于pool2.c，所以要初始化
    http_mg_init();
    link_mg_init();
    queue_mg_init();
}


/*==============================================================
                        taskQueue 对象
==============================================================*/
//创建任务 对象    
sessionacs_task_t *sessionacs_task_create(sessionacs_task_type_e type)
{
    sessionacs_task_t *task = (sessionacs_task_t *)MALLOC(sizeof(sessionacs_task_t));
    if(task == NULL)
    {
        LOG_ALARM("task malloc faild");
        return NULL;
    }

    task->type = type;
    if(type == sessionacs_task_type_rpc_request)
    {
        task->data.rpcRequest.status = sessionacs_task_rpc_request_status_start;
        memset(task->data.rpcRequest.method, '\0', 64);
        task->data.rpcRequest.soapNode = NULL;
        task->data.rpcRequest.soapNodeResponse = NULL;
        //task->data.rpcRequest.soapNode = soap_create_node_default_size();
        //task->data.rpcRequest.soapNodeResponse = soap_create_node_default_size();
        
    }
    else if(type == sessionacs_task_type_rpc_response)
    {
        task->data.rpcResponse.status = sessionacs_task_rpc_response_status_start;
        memset(task->data.rpcResponse.method, '\0', 64);
        task->data.rpcResponse.soapNode = NULL;
        //task->data.rpcResponse.soapNode = soap_create_node_default_size();

    }  
    else if(type == sessionacs_task_type_other)
    {
        ;
    }
    else
    {
        FREE(task);
        return NULL;
    }
        


    return task;
}

//销毁任务对象
void sessionacs_task_destroy(sessionacs_task_t *task)
{
    if(task == NULL)return;

    //释放成员 
    switch(task->type)
    {
        case sessionacs_task_type_rpc_request:{
            soap_destroy_node(task->data.rpcRequest.soapNode);
            soap_destroy_node(task->data.rpcRequest.soapNodeResponse);
                
        }break;
        case sessionacs_task_type_rpc_response:{
            soap_destroy_node(task->data.rpcResponse.soapNode);
        }break;    
        case sessionacs_task_type_other:{
            ;
        }break;
        default:{
            ;

        }break;
       
    }

    //释放自己
    FREE(task);
}

//会话的taskQueue 添加任务，如果成功则返回 任务结构体 的指针
sessionacs_task_t *sessionacs_taskQueue_in(sessionacs_member_t *member, sessionacs_task_type_e type)
{
    int ret;
    if(member == NULL || member->taskQueue == NULL)return NULL;

    sessionacs_task_t *task = sessionacs_task_create(type);
    if(task == NULL)
    {
        LOG_ALARM("sessionacs_task_create 失败 ");
        return NULL;
    }

    ret = queue_in_set_pointer(member->taskQueue, (void *)task);

    //LOG_SHOW("----------------------- debug    empty:%d\n", queue_isEmpty(session->taskQueue));
    
    if(ret == RET_OK)
    {
        return task;
    }
    else
    {
        sessionacs_task_destroy(task);
        return NULL;
    }
          
    return NULL;
}

//会话的taskQueue 取出任务
sessionacs_task_t *sessionacs_taskQueue_out(sessionacs_member_t *member)
{
    int ret;
    if(member == NULL || member->taskQueue == NULL)return NULL;

    sessionacs_task_t *task = NULL;
    ret = queue_out_get_pointer(member->taskQueue, (void **)(&task));
    if(ret == RET_OK)
    {
        return task;
    }
      
    return NULL;
}

//会话的taskQueue 清空任务
int sessionacs_taskQueue_clear(sessionacs_member_t *member)
{
    LOG_SHOW("开始清空 taskQueue\n");

    if(member == NULL || member->taskQueue == NULL)return RET_FAILD;

    QUEUE_FOEWACH_START(member->taskQueue, probe)
    {
        sessionacs_task_destroy((sessionacs_task_t *)(probe->d));
    }QUEUE_FOEWACH_END;
    
    queue_clear(member->taskQueue);

    return RET_OK;
}



/*==============================================================
                        对象
==============================================================*/
/*-------------------------------------------------msg 数据元 */
//创建 msg 数据元
sessionacs_msgdata_t *sessionacs_create_msg(int size)
{
    if(size < 0)return NULL;
    sessionacs_msgdata_t *msgdata = (sessionacs_msgdata_t *)MALLOC(sizeof(sessionacs_msgdata_t));
    if(msgdata == NULL)return NULL;

    if(size == 0)
    {
        msgdata->d = NULL;
        msgdata->len = 0;
    }
    if(size > 0)
    {
        msgdata->d = (void *)MALLOC(size + 8);  //多分配几个字节，用于存字符串结尾字符 '\0'
        msgdata->len = size;
        if(msgdata->d == NULL)  //失败
        {
            FREE(msgdata);
            return NULL;
        }
        
    }
        
    return msgdata;
}

//创建 msg 数据元，同时给定 数据
sessionacs_msgdata_t *sessionacs_create_msg_set_data(void *data, int len)
{
    if(len < 0 || data == NULL)return NULL;
    sessionacs_msgdata_t *msgdata = (sessionacs_msgdata_t *)MALLOC(sizeof(sessionacs_msgdata_t));
    if(msgdata == NULL)return NULL;

    if(len == 0 || data == NULL)    //空数据
    {
        msgdata->d = NULL;
        msgdata->len = 0;
    }
    if(len > 0)
    {
        msgdata->d = (void *)MALLOC(len);
        msgdata->len = len;
        if(msgdata->d == NULL)  //失败
        {
            FREE(msgdata);
            return NULL;
        }

        memcpy(msgdata->d, data, len);
    }
        
    return msgdata;
}
/*-------------------------------------------------msg 数据元 */

//创建会话 成员
sessionacs_member_t *sessionacs_create_member()
{
    sessionacs_member_t *member = (sessionacs_member_t *)MALLOC(sizeof(sessionacs_member_t));
    if(member == NULL)return NULL;
    
    member->status = sessionacs_status_start;
    member->user = NULL;
    member->cwmpid = 0;

    //soap 信息队列
    member->msgQueue = queue_create(SESSION_ACS_MEMBER_MSG_QUEUE_SIZE);
    pthread_mutex_init(&(member->mutexMsgQueue), NULL);   //初始化互斥锁
    member->msgRecvEn = 0;  //默认不允许信息进入队列
    pthread_cond_init(&(member->condMsgQueueReady), NULL);  //准备好信息的条件变量

    //任务队列
    member->taskQueue = queue_create(SESSION_ACS_MEMBER_TASK_QUEUE_SIZE);
    pthread_mutex_init(&(member->mutexTaskQueue), NULL);   //初始化互斥锁
    member->timeout = 0;
    
    member->session = NULL;

    return member;
}
//创建会话成员， 并给定user 指针
sessionacs_member_t *sessionacs_create_member_set_user(httpUser_obj_t *user)
{
    sessionacs_member_t *member = sessionacs_create_member();

    if(member == NULL)return NULL;

    member->user = user;

    return member;
}


//创建一个会话对象
sessionacs_obj_t *sessionacs_create()
{   
    sessionacs_obj_t *session = (sessionacs_obj_t *)MALLOC(sizeof(sessionacs_obj_t));
    if(session == NULL)return NULL;

    //结构体成员设置
    session->http = NULL;
    
    session->memberLink = link_create();
    if(session->memberLink == NULL)
        LOG_ALARM("link 创建失败!");

    session->cwmpidCnt = 0;
    
    return session;
}

//创建一个会话对象， 并设置 http 指针
sessionacs_obj_t *sessionacs_create_set_httpPointer(http_server_t *http)
{
    sessionacs_obj_t *session = sessionacs_create();
    if(session == NULL)return NULL;

    session->http = http;
    
    return session;
}
//创建一个会话对象， 并指定 http 服务器的参数
sessionacs_obj_t *sessionacs_create_set_httpParamters(char *ipv4, int port)
{
    sessionacs_obj_t *session = sessionacs_create();
    if(session == NULL)return NULL;

    session->http = http_create_server(ipv4, port);
    
    return session;
}



//销毁会话 成员
void sessionacs_destroy_member(sessionacs_member_t *member)
{
    if(member == NULL)return ;

    //释放 soap 信息队列
    queue_destroy(member->msgQueue);
    
    FREE(member);
}

//销毁会话 对象
void sessionacs_destroy(sessionacs_obj_t *session)
{
    if(session == NULL)return ;

    //销毁成员链表
    LINK_FOREACH(session->memberLink, probe)
    {
        if(probe->data != NULL && probe->en == 1)
        {
            sessionacs_destroy_member((sessionacs_member_t *)(probe->data));
        }
    }
    
    link_destory(session->memberLink);

    //释放自己    
    FREE(session);
}

//销毁会话 对象，同时销毁 http 服务器
void sessionacs_destroy_and_http(sessionacs_obj_t *session)
{
    if(session == NULL)return ;

    //销毁成员链表
    LINK_FOREACH(session->memberLink, probe)
    {
        if(probe->data != NULL && probe->en == 1)
        {
            sessionacs_destroy_member((sessionacs_member_t *)(probe->data));
        }
    }
    
    link_destory(session->memberLink);

    //销毁服务器
    
    

    //释放自己    
    FREE(session);
}

/*==============================================================
                        基本操作
==============================================================*/
//向会话添加 子成员
int sessionacs_append(sessionacs_obj_t *session, sessionacs_member_t *member)
{
    if(session == NULL || member == NULL)return RET_FAILD;

    member->session = session;
    link_append_by_set_pointer(session->memberLink, (void *)member);

    return RET_OK;
}







/*==============================================================
                        应用
==============================================================*/
//显示msgQueue 消息
void sessionacs_show_msgQUeue(sessionacs_member_t *member)
{
    LOG_SHOW("\n---------------- show msg queue ----------------\n");
    if(member == NULL || member->msgQueue == NULL)
    {
        LOG_SHOW("[void]\n");
        return ;
    }

    char *p;
    int cnt;
    QUEUE_FOEWACH_START(member->msgQueue, probe)
    {
        if(probe->en == 1 && probe->d != NULL)
        {
            sessionacs_msgdata_t *data = (sessionacs_msgdata_t *)(probe->d);

            if(data->d == NULL || data->len <= 0)
            {
                LOG_SHOW("cnt:%d len:%d msg:\n【空】\n", cnt, data->len)
            }
            else
            {
                p = (char *)(data->d);
                p[data->len] = '\0';
                LOG_SHOW("cnt:%d len:%d msg(字符串表示):\n【%s】\n", cnt, data->len, p);
            }
            
            cnt++;
        }        
    }QUEUE_FOEWACH_END;
    
    
}

//显示 taskQueue 
void sessionacs_show_taskQUeue(sessionacs_member_t *member)
{
    LOG_SHOW("\n---------------- show task queue ----------------\n");
    if(member == NULL || member->taskQueue == NULL)
    {
        LOG_SHOW("[void]\n");
        return ;
    }
    
    int cnt = 0;
    QUEUE_FOEWACH_START(member->taskQueue, probe)
    {
        if(probe->en == 1 && probe->d != NULL)
        {
            sessionacs_task_t *data = (sessionacs_task_t *)(probe->d);

            if(data->type == sessionacs_task_type_rpc_request)
            {
                LOG_SHOW("cnt:%d 请求任务 status:%d method:%s soapNode:%p soapNodeResponse:%d\n",
                            cnt, data->data.rpcRequest.status, data->data.rpcRequest.method,
                            data->data.rpcRequest.soapNode, data->data.rpcRequest.soapNodeResponse);
            }
            else if(data->type == sessionacs_task_type_rpc_response)
            {
                LOG_SHOW("cnt:%d 回复任务 status:%d method:%s soapNode:%p \n",
                            cnt, data->data.rpcResponse.status, data->data.rpcResponse.method,
                            data->data.rpcResponse.soapNode);
            }
            else
            {
                LOG_SHOW("cnt:%d 未知类型 \n");
            }
            
            cnt++;
        }        
    }QUEUE_FOEWACH_END;  
}



//-----------------------------------------------------------------会话过程辅助函数
//0.1、处理 msgQueue 的信息，取出soap 消息的第一个 信封的 soap:Body 节点
//返回值；注意参数 outNode，会指向新分配的动态内存，用完时请释放
static int _aux_msgQueue_pro(sessionacs_member_t *member, soap_node_t **outNode)
{
    sessionacs_msgdata_t *msgData;
    char* strp;
    soap_node_t *nodeRoot;
    soap_node_t *nodeBody;
    
    *outNode = NULL;    //预设为空指针
    
    if(member == NULL || member->msgQueue == NULL)
        return -1;  //参数问题
    if(queue_isEmpty(member->msgQueue))
        return -2;  //队列为空

    queue_out_get_pointer(member->msgQueue, (void**)(&msgData));   
    if(msgData->d != NULL)
        return -3;  //信息为空

    strp = (char *)(msgData->d);
    strp[msgData->len] = '\0';
    //LOG_SHOW("-->msgData len:%d msg:\n【%s】\n", msgData->len, strp);
    nodeRoot = soap_str2node(strp);
    //soap_node_show(nodeRoot);
    
    //这里只考虑 soap消息的第一个 soap 信封
    nodeBody = soap_node_get_son(
                        soap_node_get_son(
                        nodeRoot, 
                        "soap:Envelope"), 
                        "soap:Body");
   if(nodeBody != NULL && nodeBody->son != NULL)
   {
        *outNode = nodeBody;
   }
   else
   {
        return -4;  //表示 字符串到soap节点解析错误，或者字符串格式不是预期的soap封装
   }
   
    return 0;       
}

//0.2、超时等待 msgQueue 信息，用条件变量
#define SESSIONACS_MSG_TIME_OUT 5
static void *__aux_thread_wait_msg(void *in)
{
    if(in == NULL)return NULL;
    int ret;
    //sessionacs_msgdata_t *msgData;
    //char *strp;
    
    sessionacs_member_t *member = (sessionacs_member_t *)in;
    
    member->timeout = 0;
    
    //等待msgQueue有可用信息 ，需要条件变量   
    pthread_mutex_lock(&(member->mutexMsgQueue));
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += member->timeoutValue; // 设置超时时间为 x 秒后
    
    while (queue_isEmpty(member->msgQueue)) {
        LOG_SHOW("等待消息队列非空...\n");

        // 阻塞等待条件变量
        ret = pthread_cond_timedwait(&(member->condMsgQueueReady), &(member->mutexMsgQueue), &ts); 
        if (ret == ETIMEDOUT) {
            LOG_SHOW("等待超时，不再等待。\n");
            member->timeout = 1;
            break;
        }
    }

//    if(!queue_isEmpty(member->msgQueue))
//    {   
//        //取出第一个 msg
//        LOG_SHOW("开始处理消息队列表...\n");
//        
//        queue_out_get_pointer(member->msgQueue, (void**)(&msgData));    
//        strp = (char *)(msgData->d);
//        strp[msgData->len] = '\0';
//        LOG_SHOW("msgData len:%d msg:\n【%s】\n", msgData->len, strp);
//        
//        ret = 0;
//    }
    
    pthread_mutex_unlock(&(member->mutexMsgQueue));

    return NULL;
}


//1、建立连接（默认acs 被动接收到 Inform 消息，然后开始建立会话）
static int __aux_establish_connection(sessionacs_member_t *member)
{
    if(member == NULL)return RET_FAILD;
#undef BUF_SIZE
#define BUF_SIZE 4096
    char buf[BUF_SIZE + 8] = {0};
    //char buf64[64] = {0};
    int pos = 0;
    int ret;
    sessionacs_msgdata_t *msgData;
    soap_node_t *soapNodeRoot;
    soap_node_t *soapNodeInform;
    char *strp;
    int informExit;
    char buf1024[1024 + 8] = {0};
    
    //1、等待msgQueue有可用信息 ，需要条件变量   
    pthread_mutex_lock(&(member->mutexMsgQueue));
    while (queue_isEmpty(member->msgQueue)) {
        LOG_SHOW("等待消息队列非空...\n");
        
        // 阻塞等待条件变量
        pthread_cond_wait(&(member->condMsgQueueReady), &(member->mutexMsgQueue)); 
    }
    LOG_SHOW("开始处理消息队列表...\n");
    //sessionacs_show_msgQUeue(member);

    informExit = 0;
    while(!queue_isEmpty(member->msgQueue))
    {                     
        queue_out_get_pointer(member->msgQueue, (void**)(&msgData));  
        if(msgData->d != NULL)  //msgData 可能是空消息，这里要注意
        {
            strp = (char *)(msgData->d);
            strp[msgData->len] = '\0';
            LOG_SHOW("-->msgData len:%d msg:\n【%s】\n", msgData->len, strp);


            //开始解析成 soap 节点，这里简单规定，第一个字节点应该是 "soap:Envelope"
            //里面包含的 Inform 方法 用于acs的初始化连接；其他的话不处理，
            //一律回复 HTTP/1.1 200 OK；后面需要优化；对于htpp 的错误码响应的实现，但
            //是要重构 http ，以后再说吧
            soapNodeRoot = soap_str2node(strp);
            soap_node_show(soapNodeRoot);

            
            //这里只考虑 soap消息的第一个 soap 信封（1.4 新版本只支持一个信封， 老版本
            //则需要协商个数）
            soapNodeInform = soap_node_get_son(
                                soap_node_get_son(
                                soap_node_get_son(
                                soapNodeRoot, 
                                "soap:Envelope"), 
                                "soap:Body"), 
                                "cwmp:Inform");
           if(soapNodeInform != NULL)
           {
                informExit = 1;
                break;
           }

        }
        else
        {
            LOG_SHOW("-->msgData len:%d msg:\n【空】\n", msgData->len);
        }
        
    }
    
    pthread_mutex_unlock(&(member->mutexMsgQueue));
    
   
    //没有得到 Inform 消息，发送回复，不进行下面动作，而是直接返回
    if(informExit == 0) 
    {
        httpmsg_server_response_hello(buf1024, 1024);
        LOG_SHOW("开始发送 http 的回复信息\n");
        httpUser_send_str(member->session->http, member->user, buf1024); 
        
        return -1;  //没有得到 Inform 消息，发送回复（测试行为）
    }

    

    //2、完成建立后，发送 Infor response 消息，告知 cpe
    //得到了 Inform 消息，显示结果
    if(member->user->id >= 0 && member->user->id < HTTP_USER_NUM)
    {
        tcpUser_obj_t *tcpUser = &((member->session->http->tcp->user)[member->user->id]);
        LOG_SHOW("收到来自 %s:%d 的 Inform 消息，开始握手.....\n", tcpUser->ipv4, tcpUser->port);

    }
    //2.1 构建 InformResponse 消息
    //2.1.1 添加基础内容
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "long123",
        .idEn = 1,
        .HoldRequests = "false",
        .holdRequestsEn = 1
    };
    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;

    soap_node_t *body = soap_node_get_son(root, "soap:Body");
    
    //2.1.2 添加rpc方法 InformResponse
    rpc_InformResponse_t dataInformResponse = {.MaxEnvelopes = 1};
    soap_node_append_son(body, soapmsg_to_node_InformResponse(dataInformResponse));

    //2.2 生成soap信封，并通过http 接口发送给 acs   
    soap_node_to_str(soap->root, buf, &pos, BUF_SIZE);
    
    ret = http_send_msg(member->user, (void *)buf, strlen(buf), 200, "OK");

    if(ret == 0)
    {
        //LOG_SHOW("发送 Inform 完成\n");
    }
    
    return 0;    
}

//2、会话任务处理
static int __aux_task_pro(sessionacs_member_t *member)
{ 
    if(member == NULL || member->taskQueue == NULL)return -99;   //参数错误

    LOG_SHOW("__aux_task_pro  来了 ......\n");
    
    if(queue_isEmpty(member->taskQueue))  
        return -1;  //任务队列为空

    //2、从列表中取得当前任务，然后处理
    sessionacs_show_taskQUeue(member);
    
    sessionacs_task_t *task = NULL;
    queue_out_get_pointer(member->taskQueue, (void *)(&task));
    if(task == NULL)
        return -99;     //出现预料之外的错误
    switch(task->type)  //按照类型的不同来处理任务
    {
        case sessionacs_task_type_rpc_request:{
            

        }break;
        case sessionacs_task_type_rpc_response:{

        }break;
        default:{

        }break;
    }

  
    return 0;
}


//3、任务队列为空时候的处理
static int __aux_task_empty_pro(sessionacs_member_t *member)
{
    if(member == NULL)return -99;   //参数错误
    
    int ret;
    pthread_t threadWaitMsg;
    soap_node_t *nodeTmp;
    sessionacs_task_t *taskTmp;
    soap_node_t *nodeBody;

    //1、如果msg队列为空，则等待新的msg消息
    //超时等待新的msg，超时时间可设为30秒，测试时可以更小
    //或者等待新的 task 出现，这里没有设置，以后可以考虑加上 
    if(queue_isEmpty(member->msgQueue))
    {
        member->timeoutValue = 5;   //超时 x 秒
        member->timeout = 0;
        pthread_create(&threadWaitMsg, NULL, __aux_thread_wait_msg, (void *)member);
        pthread_join(threadWaitMsg, NULL);
        LOG_SHOW(" 超时等待相关的值 timeout:%d\n", member->timeout);

        if(member->timeout == 1)
        {
            LOG_SHOW("msg等待%d秒超时，会话未成功终止\n", member->timeoutValue);
            return -1;  //超时
            
        }
    }
    
  
    //2、处理 msgQueue，看能否激活新的任务  
    pthread_mutex_lock(&(member->mutexMsgQueue));
    ret = _aux_msgQueue_pro(member, &nodeBody);     //取出一条msg进行处理
    pthread_mutex_unlock(&(member->mutexMsgQueue));

    //LOG_SHOW("-----------------------mark 1、任务列表为空\n");
    soap_node_show(nodeBody);
    if(ret == 0)
    {   
        //查看soap封装的信息
        LINK_DATA_FOREACH_START(nodeBody->son, probeData){
            nodeTmp = (soap_node_t *)probeData;
            ret = cwmprpc_acs_method_soap_name_match(nodeTmp->name);    //匹配 acs方法才有用
            if(ret == 0)
            {
                pthread_mutex_lock(&(member->mutexTaskQueue));
                taskTmp = sessionacs_taskQueue_in(member, sessionacs_task_type_rpc_response);
                pthread_mutex_unlock(&(member->mutexTaskQueue));
                if(taskTmp != NULL) //添加任务队列
                {   
                    strcpy(taskTmp->data.rpcResponse.method, nodeTmp->name);
                    taskTmp->data.rpcResponse.soapNode = nodeTmp;
                    taskTmp->data.rpcResponse.status = sessionacs_task_rpc_response_status_start;
                }
            }
            
        }LINK_DATA_FOREACH_END  

    }
    else if(ret == -1 || ret == -2)   
    {
        return -99;  //_aux_msgQueue_pro 处理出现故障
    }
    else if(ret == -3)  
    {
        return -2;  //msg 为空信息
    }
    else if(ret == -4)  
    {
        return -3;  //代表任务列表为空，msg 不为空，但是解析内容不是期望的soap封装
    }

        
    return 0;   //成功解析
}




//会话线程
void *thread_sessionacs(void *in)
{   
    if(in == NULL)
    {
        LOG_ALARM("thread_sessionacs: in is NULL");
        return NULL;
    }
    
    sessionacs_member_t *member = (sessionacs_member_t *)in;
    //httpUser_obj_t *user = member->user;
    int ret;
    int outWhile = 0;
    
    while(1)
    {
        switch(member->status)
        {
           case sessionacs_status_start:{
                
                //开始允许 msgQueue 接收信息
                pthread_mutex_lock(&(member->mutexMsgQueue));
                member->msgRecvEn = 1;
                pthread_mutex_unlock(&(member->mutexMsgQueue));
                
                //进入下一阶段：建立连接
                member->status = sessionacs_status_establish_connection;    
                
           }break;
           case sessionacs_status_establish_connection:{    //建立连接
                LOG_SHOW("\n-------------------【cpe会话 establish_connection】---------------  \n");
                ret = __aux_establish_connection(member);
                
                if(ret == 0)
                {
                    //到任务处理前的操作
                    //sessionacs_taskQueue_clear(member);     //清空任务列表
                    member->msgRecvEn = 1;  //允许消息接收
                
                    member->status = sessionacs_status_task;    //状态转移
                }
                else if(ret == -1)
                {
                    LOG_SHOW("__aux_establish_connection 失败，返回值 -1\n");
                }
                

               
                //outWhile = 1;
           }break;
           case sessionacs_status_task:{
                LOG_SHOW("\n-------------------【cpe会话 task】---------------  \n");
                ret = __aux_task_pro(member);
                
                LOG_SHOW("__aux_task_pro 返回值 ret:%d  \n", ret);
                switch(ret)
                {
                    case 0:{    
                        
                    }break; 
                    case -1:{   //任务列表为空，msgQueue 也为空
                        member->status = sessionacs_status_task_empty;    //状态转移
                    }break;
                    case -99:{
                        LOG_ALARM("_aux_msgQueue_pro 处理出现故障");
                        member->status = sessionacs_status_end; 
                    }break;
                    default:{

                    }break;
            
                }


                
                outWhile = 1;
           }break;    
           case sessionacs_status_task_empty:{  // taskQueue为空的情况下进入
                LOG_SHOW("\n-------------------【cpe会话 task_empty】---------------  \n");

                ret = __aux_task_empty_pro(member);

                LOG_SHOW("__aux_task_empty_pro 处理完成，返回值 ret:%d  \n", ret);

                if(!queue_isEmpty(member->taskQueue))
                {
                    member->status = sessionacs_status_task;    //重入 task 状态
                }
                else if(ret == -99) 
                {
                    member->status = sessionacs_status_error;   //处理发生故障，进入 error 状态
                }
                else if(!queue_isEmpty(member->msgQueue))
                {
                    ;   //还有数据处理，状态不变
                }
                else
                {
                    if(ret == -1)
                    {
                        LOG_SHOW("msg 等待超时\n");
                    }      
                    else if(ret == -2)
                    {
                        LOG_SHOW("msg 为空\n");
                    }  
                    else if(ret == -3)
                    {
                        LOG_SHOW("msg 的内容没有解析出 soap:Body\n"); 
                    }
                             
                    member->status = sessionacs_status_disconnect;
                }

                //outWhile = 1;
           }break;
           case sessionacs_status_disconnect:{
                LOG_SHOW("\n-------------------【cpe会话 disconnect】---------------  \n");

                //向cpe发送空消息体的http 响应，告知对方结束会话
                ret = http_send_msg(member->user, NULL, 0, 200, "OK");
                if(ret != RET_OK)
                {
                    LOG_ALARM("故障：无法发送数据");
                    member->status = sessionacs_status_error;    //状态转移    
                }
                

                outWhile = 1;
           }break;
           case sessionacs_status_end:{


           }break;
           case sessionacs_status_error:{
                LOG_SHOW("\n-------------------【cpe会话 error】---------------  \n");              
                outWhile = 1;

           }break;
           default:{    //其他的情况
               
                LOG_ALARM("thread_sessionacs switch default ......");
           }break;
           //outWhile = 1;
         
        }

        
        if(outWhile)break;
    }    
    
    return NULL;
}


//消息接收线程
void *thread_msg_recv(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_msg_recv:in is NULL");
        return NULL;
    }

    sessionacs_member_t *member = (sessionacs_member_t *)in;
    httpUser_obj_t *httpUser = member->user;
    //queue_member_t *qm;
    while(1)
    {
        sem_wait(&(httpUser->semHttpMsgReady));  

        
        http_payload_t *payload = &(httpUser->payload);

        httpUser->retHttpMsg = -2;  //无动作

        pthread_mutex_lock(&(member->mutexMsgQueue));
        if(member->msgRecvEn == 1)  //允许接收
        {
           
            if(queue_isFull(member->msgQueue))
            {
                httpUser->retHttpMsg = -1;  
                
            }
            else
            {
                queue_in_set_pointer(member->msgQueue, 
                    (void *)sessionacs_create_msg_set_data((void *)(payload->buf), payload->len));            
                httpUser->retHttpMsg = 0;              
            }
            
        }
        else
        {
            httpUser->retHttpMsg = -3;   //返回信息：msgQueue 不允许增加信息
        }

        if(!queue_isEmpty(member->msgQueue))
        {
            LOG_SHOW("开始发送条件变量....\n");
            pthread_cond_signal(&(member->condMsgQueueReady)); //发送条件变量
        }
            
        
        pthread_mutex_unlock(&(member->mutexMsgQueue));
        
        sem_post(&(httpUser->semHttpMsgRecvComplete));  //完成的信号
        
    }
    
    return NULL;
}


//acs会话：接收数据线程，一般来说，接收的是soap信封，用 soap_obj_t 对象表示
void *thread_new_user(void *in)
{
    int id;
    int ret;
    if(in == NULL)
    {
        LOG_ALARM("thread_sessionacs_recv");
        return NULL;
    }

    sessionacs_obj_t *session = (sessionacs_obj_t *)in;

    tcp_server_t *tcp = session->http->tcp;
    
    sessionacs_member_t *newMember;
    while(1)
    {
        sem_wait(&(tcp->semUserConnect));

        id = tcp->newUserId;
        
        newMember = sessionacs_create_member_set_user(&((session->http->user)[id]));
        if(newMember == NULL)
        {
            LOG_ALARM("创建成员失败\n");
            continue;   //跳过
        }

        //------------------------------------------------------------------------------------------
        //一些测试，比如预先设置 member的任务队列
        sessionacs_task_t *taskTmp;
        char *strTmp;
        char buf64[64] = {0};
        if(newMember->user->tcpUser->port == 8081 || newMember->user->tcpUser->port == 8082)  //特定端口测试
        {
            //清空 任务队列
            sessionacs_taskQueue_clear(newMember);
            //1、添加rpc方法 GetParameterValues
            //1.1 新任务
            taskTmp = sessionacs_taskQueue_in(newMember, sessionacs_task_type_rpc_request);  
          
            //LOG_SHOW("thread_new_user sessionacs_taskQueue_in taskTmp:%p\n", taskTmp);
            if(taskTmp != NULL)
            {
                //1.2 请求的方法名
                strcpy(taskTmp->data.rpcRequest.method, "GetParameterValues");
                //1.3 soap_node 存储数据
                link_obj_t *linkParameterNames = link_create();     //链表成员数据的类型 ： char [256]

                strTmp = (char *)link_append_by_malloc(linkParameterNames, sizeof(256)); 
                strcpy(strTmp, "parameter_name_1");     //用于测试的参数名

                strTmp = (char *)link_append_by_malloc(linkParameterNames, sizeof(256)); 
                strcpy(strTmp, "parameter_name_2"); 

                rpc_GetParameterValues_t dataGetParameterValues = {
                    .ParameterNames = linkParameterNames     //链表成员数据的类型 ： char [256]
                };

                //1.4 节点存入新的 task
                taskTmp->data.rpcRequest.soapNode = soapmsg_to_node_GetParameterValues(dataGetParameterValues);
                link_destroy_and_data(linkParameterNames);

            }
            //2、添加rpc方法 SetParameterValues
            //2.1 新任务
            taskTmp = sessionacs_taskQueue_in(newMember, sessionacs_task_type_rpc_request);  
            
            if(taskTmp != NULL)
            {
                //1.2 请求的方法名
                strcpy(taskTmp->data.rpcRequest.method, "SetParameterValues");
                //2.3 soap_node 存储数据
                link_obj_t *linkParameterList = link_create();
                ParameterValueStruct *pvs1 = (ParameterValueStruct *)link_append_by_malloc
                                                (linkParameterList, sizeof(ParameterValueStruct));    
                strcpy(pvs1->Name, "parameter_name_1");
                strcpy(buf64, "value_1");
                pvs1->Value = buf64;
                pvs1->valueLen = strlen(pvs1->Value);

                rpc_SetParameterValues_t dataSetParameterValues = {
                    .ParameterList = linkParameterList,
                    .ParameterKey = "paramter_key_1"
                };

                //2.4 节点存入新的 task
                taskTmp->data.rpcRequest.soapNode = soapmsg_to_node_SetParameterValues(dataSetParameterValues);
                link_destroy_and_data(linkParameterList);

            }
            
        }
        else
        {
            LOG_SHOW("未匹配到 tcp 接口 port:%d\n", newMember->user->tcpUser->port);
        }
        //------------------------------------------------------------------------------------------

        
        sessionacs_append(session, newMember);
        //开启消息接收线程
        ret = pthread_create(&(newMember->threadMsgRecv), NULL, thread_msg_recv, (void *)newMember);
        if(ret != 0)
        {
            LOG_ALARM("thread_msg_recv");
        }

        //开启会话线程
        ret = pthread_create(&(newMember->threadMsgRecv), NULL, thread_sessionacs, (void *)newMember);
        if(ret != 0)
        {
            LOG_ALARM("thread_sessionacs");
        }


        
    }

    return NULL;
}


//开启会话
int sessionacs_start(sessionacs_obj_t *session)
{
    int ret;
    
    if(session == NULL)return RET_FAILD;
    
    http_server_start(session->http);

    //开启接收消息的服务
    ret = pthread_create(&(session->threadNewUser), NULL, thread_new_user, (void *)session);
    if(ret != 0){LOG_ALARM("thread_new_user"); return ret;};
    
    return RET_OK;
}


/*==============================================================
                        测试
==============================================================*/
void sessionacs_test()
{
    //printf("sessionacs test start ...\n");
    
    sessionacs_obj_t * session = sessionacs_create_set_httpParamters("192.168.1.20", 8080);

    sessionacs_start(session);

    printf("acs session start ...\n");

    for(;;);    
}








