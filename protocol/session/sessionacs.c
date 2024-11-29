


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
#define SESSION_ACS_REQUEST_QUEUE_SIZE 10
// 请求 数据结构
typedef struct sessionacs_request_t{
    soap_node_t *node;   //用soap节点来存储请求信息
    char nodeEn;
    char active;    //是否被激活：1 表示被激活，0表示没有被激活 
}sessionacs_request_t;

//会话状态机
typedef enum sessionacs_status_e{
    sessionacs_status_start,    //开始
    sessionacs_status_establish_connection,     //建立连接
    sessionacs_status_send,     //发送状态
    sessionacs_status_recv,    //等待接收状态
    sessionacs_status_disconnect,   //会话完成，请求断开状态
    sessionacs_status_end,   //结束
    sessionacs_status_error    //错误
}sessionacs_status_e;

//acs 会话对象的 子成员
#define SESSION_ACS_MEMBER_MSG_QUEUE_SIZE 10
#define SESSION_ACS_MEMBER_TASK_QUEUE_SIZE 10

#define SESSIONACS_MSG_SIZE 10240
typedef struct sessionacs_obj_t sessionacs_obj_t;
typedef struct sessionacs_member_t{
    //数据收发接口
    httpUser_obj_t *user;   //htt用户

    //信息接收
    char msg[SESSIONACS_MSG_SIZE + 8];
    int msgLen;
    int msgReady;  //msg是否已准备好，1表示准备好了，0表示没有准备好   
    pthread_mutex_t mutexMsg; //互斥锁，保护msgQueue资源的互斥锁
    pthread_cond_t condMsgReady;   //条件变量，表示有消息
    
    //状态机
    sessionacs_status_e status;     
    
    //线程
    pthread_t threadSession;    //会话线程
    pthread_t threadMsgRecv;    //消息接收线程

    //会话控制信息
    //char HoldRequests;  //soap头部控制信息，用于维持链接，1.4版本弃用；但是cpe还是需要支持，0代表false
    char cwmpid[64];
    char lastRecvRequestExist; //上一次的接收有acs请求，这意味着下一次发送不应该是请求（详情参考会话流程）
    char HoldRequests; 
    
    //请求队列
    queue_obj_t *requestQueue;
    pthread_mutex_t mutexRequestQueue; //互斥锁，保护 retquestQueue 资源的互斥锁

    //会话对象接口
    //sessionacs_obj_t *session;
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
                        request 队列
==============================================================*/
//创建 request 对象
sessionacs_request_t *sessionacs_request_create()
{
    sessionacs_request_t *request = (sessionacs_request_t *)MALLOC(sizeof(sessionacs_request_t));

    if(request == NULL)return NULL;

    request->active = 0;
    request->nodeEn = 0;
    
    
    return request;
}

//摧毁 request 对象
void sessionacs_request_destroy(sessionacs_request_t *request)
{
    if(request == NULL)return ;

    FREE(request);
}

//request 队列入队列
int sessionacs_requestQueue_in(sessionacs_member_t *member, sessionacs_request_t *request)
{
    if(member == NULL || request == NULL || member->requestQueue == NULL)return RET_FAILD;

    return queue_in_set_pointer(member->requestQueue, (void *)request);
    
}

//request 入队列，创建新的request 并返回指针
sessionacs_request_t *sessionacs_requestQueue_in_new(sessionacs_member_t *member)
{
    int ret;
    if(member == NULL ||  member->requestQueue == NULL)return NULL;

    sessionacs_request_t *request = sessionacs_request_create();
    if(request == NULL)return NULL;
    ret = sessionacs_requestQueue_in(member, request);
    if(ret == RET_OK)
    {
        return request;
    }
    else
    {
        sessionacs_request_destroy(request);
    }

    return NULL;
}


//request 队列出队列
int sessionacs_requestQueue_out(sessionacs_member_t *member, sessionacs_request_t **out)
{
    int ret;
    void *tmp = NULL;
    if(member == NULL || member->requestQueue == NULL || out == NULL)return RET_FAILD;
    ret = queue_out_get_pointer(member->requestQueue, &tmp);
    if(ret == RET_OK && tmp != NULL)
    {
        *out = (sessionacs_request_t *)tmp;
        return RET_OK;
    }
    return RET_FAILD;
}
//request 得到队列头元素
int sessionacs_requestQueue_get_head(sessionacs_member_t *member, sessionacs_request_t **out)
{
    int ret;
    void *tmp = NULL;
    if(member == NULL || member->requestQueue == NULL || out == NULL)return RET_FAILD;
    
    ret = queue_get_head_pointer(member->requestQueue, &tmp);

    if(ret == RET_OK && tmp != NULL)
    {
        *out = (sessionacs_request_t *)tmp;
        return RET_OK;
    }
    return RET_FAILD;
}

//request 队列清空
void sessionacs_requestQueue_clear(sessionacs_member_t *member)
{
    if(member == NULL || member->requestQueue == NULL)return ;
    QUEUE_FOEWACH_START(member->requestQueue, probe)
    {
        if(probe->en == 1 && probe->d != NULL)
        {
            sessionacs_request_t *iter = (sessionacs_request_t *)(probe->d);
            sessionacs_request_destroy(iter);
        }
    }QUEUE_FOEWACH_END

    queue_clear(member->requestQueue);
}


/*==============================================================
                        会话对象
==============================================================*/
//创建会话 成员
sessionacs_member_t *sessionacs_create_member()
{
    sessionacs_member_t *member = (sessionacs_member_t *)MALLOC(sizeof(sessionacs_member_t));
    if(member == NULL)return NULL;

    //数据收发接口
    member->user = NULL; 

    //信息接收
    memset(member->msg, '\0', SESSIONACS_MSG_SIZE);
    member->msgLen = 0;
    member->msgReady = 0;  //默认不允许信息进入队列
    pthread_mutex_init(&(member->mutexMsg), NULL);   //初始化互斥锁
    pthread_cond_init(&(member->condMsgReady), NULL);  //准备好信息的条件变量

    //状态机
    member->status = sessionacs_status_start;
    
    //线程（不用操作）
    
    //会话控制信息
    memset(member->cwmpid, '\0', 64);     
    member->lastRecvRequestExist = 0;
 
    
    //request 队列
    member->requestQueue = queue_create(SESSION_ACS_REQUEST_QUEUE_SIZE);
    pthread_mutex_init(&(member->mutexRequestQueue), NULL);   //初始化互斥锁 

    //会话对象接口
    //member->session = NULL;

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

    //释放 request 队列
    queue_destroy(member->requestQueue);
    
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

//向会话添加 子成员
int sessionacs_append(sessionacs_obj_t *session, sessionacs_member_t *member)
{
    if(session == NULL || member == NULL)return RET_FAILD;

    //member->session = session;
    link_append_by_set_pointer(session->memberLink, (void *)member);

    return RET_OK;
}


#if 0

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
    
   
    //没有得到 Inform 消息，发送回复，不进行下面动作，而是直接返回（响应 http 认证，需要优化）
    if(informExit == 0) 
    {
        httpmsg_server_response_hello(buf1024, 1024);
        //LOG_SHOW("开始发送 http 的回复信息\n");
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

    //2.2 生成soap信封，并通过http 接口发送给对方   
    soap_node_to_str(soap->root, buf, &pos, BUF_SIZE);
    
    http_send_msg(member->user, (void *)buf, strlen(buf), 200, "OK");

    
    
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
                LOG_SHOW("\n-------------------【cpe会话 start】---------------  \n");
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
                LOG_SHOW("__aux_establish_connection 返回值 ret:%d  \n", ret);
                if(ret == 0)
                {
                    //到任务处理前的操作
                    //sessionacs_taskQueue_clear(member);     //清空任务列表
                    member->msgRecvEn = 1;  //允许消息接收
                
                    member->status = sessionacs_status_task;    //状态转移
                }
                else if(ret == -1)
                {
                    ;//LOG_SHOW("__aux_establish_connection 失败，返回值 -1\n");
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

#endif

/*==============================================================
                        acs为主体，接收来自cpe的请求
==============================================================*/
//1、对 "cwmp:GetRPCMethods" 的响应
static int __acs_pro_GetRPCMethods(sessionacs_member_t *member)
{
    int i;
    int num;
    soap_node_t *nodeBody;
    char buf[SESSIONACS_MSG_SIZE + 8] = {0};
    int posTmp;
    
    if(member == NULL)return -1;

    //1、本地处理（无）
    
    //2、发送响应消息
    //2.1 构建信封基础
    soap_obj_t *soap = soap_create();
    soap_header_t header;
    strcpy(header.ID, member->cwmpid);    //和
    header.idEn = 1,
    header.holdRequestsEn = 0;    
    
    soapmsg_set_base(soap, &header);
    nodeBody = soap_node_get_son(soap->root, "soap:Body");

    //2.2 构建响应节点
    rpc_GetRPCMethodsResponse_t dataGetRPCMethodsResponse;
    dataGetRPCMethodsResponse.MethodList = link_create();
    char *methodList[] = {       //或者用 cwmprpc_acs_methodResponse_g
        "GetRPCMethods",
        "SetParameterValues",
        "GetParameterValues",
        "GetParameterNames",
        "SetParameterAttributes",
        "GetParameterAttributes",
        "AddObject",
        "DeleteObject",
        "Reboot",
        "Download"
    }; 
    num = sizeof(methodList) / sizeof(methodList[0]);
    for(i = 0; i < num; i++)
    {
        link_append_by_set_value(dataGetRPCMethodsResponse.MethodList, 
                                    (void *)(methodList[i]), strlen(methodList[i]) + 1);
    }

    soap_node_append_son(nodeBody, soapmsg_to_node_GetRPCMethodsResponse(dataGetRPCMethodsResponse));
    
    link_destroy_and_data(dataGetRPCMethodsResponse.MethodList);    //回收内存

    //3、发送 信息
    posTmp = 0;
    soap_node_to_str(soap->root, buf, &posTmp, SESSIONACS_MSG_SIZE);
    soap_destroy(soap); //回收内存
    LOG_SHOW("__acs_pro_GetRPCMethods 要发送的数据长度:%d\n", posTmp);
    http_send_msg(member->user, (void *)buf, posTmp, 200, "OK");   

    return 0;
}


//acs为主体，接收来自cpe的请求
/*
    "cwmp:Inform"
    "cwmp:GetRPCMethods"
    "cwmp:TransferComplete"
*/
int sessionacs_request_msg_pro(sessionacs_member_t *member, soap_node_t *node)
{
    int ret;
    if(member == NULL || node == NULL)return -1;

    if(strcmp(node->name, "cwmp:Inform") == 0)
    {
        //动作...
        return 0;
    }
    else if(strcmp(node->name, "cwmp:GetRPCMethods") == 0)
    {
        ret = __acs_pro_GetRPCMethods(member);
        LOG_SHOW("acs为主体，接收来自cpe的请求 GetRPCMethods， 返回值 ret:%d\n", ret);
        return 0;
    }
    else if(strcmp(node->name, "cwmp:TransferComplete") == 0)
    {
        //动作...
        return 0;
    }
    

    
    return -2;  //没有匹配到对应的响应函数
}

/*==============================================================
                        acs为主体，接收来自cpe的响应 
==============================================================*/
//处理 GetRPCMethodsResponse 信息
static int __acs_pro_GetRPCMethodsResponse(sessionacs_member_t *member, soap_node_t *node)
{
    //测试
    char buf[SESSIONACS_MSG_SIZE + 8] = {0};
    int posTmp;

    if(member == NULL || node == NULL)return -1;

    posTmp = 0;
    soap_node_to_str(node, buf, &posTmp, SESSIONACS_MSG_SIZE);
    LOG_SHOW("GetRPCMethodsResponse 信息：【%s】\n", buf);

    return 0;
}

//处理 SetParameterValuesResponse 信息 
static int __acs_pro_SetParameterValuesResponse(sessionacs_member_t *member, soap_node_t *node)
{
    //测试
    char buf[SESSIONACS_MSG_SIZE + 8] = {0};
    int posTmp;

    if(member == NULL || node == NULL)return -1;

    posTmp = 0;
    soap_node_to_str(node, buf, &posTmp, SESSIONACS_MSG_SIZE);
    LOG_SHOW("SetParameterValuesResponse 信息：【%s】\n", buf);

    return 0;
}


//acs为主体，接收来自cpe的响应 
/*
   "cwmp:GetRPCMethodsResponse",
   "cwmp:SetParameterValuesResponse",
   "cwmp:GetParameterValuesResponse",
   "cwmp:GetParameterNamesResponse",
   "cwmp:SetParameterAttributesResponse",
   "cwmp:GetParameterAttributesResponse",
   "cwmp:AddObjectResponse",
   "cwmp:DeleteObjectResponse",
   "cwmp:RebootResponse",
   "cwmp:DownloadResponse"

*/
int sessionacs_response_msg_pro(sessionacs_member_t *member, soap_node_t *node)
{
    int ret;
    int result;
    if(member == NULL || node == NULL)return -99;
    
    //请求队列（requestQueue）的对应，这里只考虑了队列头元素，
     //如果要考虑全部请求是否和响应匹配，则比较复杂
    sessionacs_request_t *request = NULL;
    sessionacs_requestQueue_get_head(member, &request);
    //exist = 0;
    if(request != NULL && request->active == 1)
    {
        result = sessionacs_requestQueue_out(member, &request);
        if(result == RET_OK && request != NULL && request->nodeEn == 1 && request->node != NULL)
        {
            ;   //exist = 1;
        }
        else
        {
            LOG_ALARM("request 不符合要求");
            return -4;
        }
    }
    else
    {
        
        return -3;  //请求为NULL或者没有被激活
    }
    
    ret = -2;   //没有匹配到对应的响应函数
    if(strcmp(node->name, "cwmp:GetRPCMethodsResponse") == 0)
    {
        if(strcmp(request->node->name, "cwmp:GetRPCMethods") != 0)
        {
            LOG_ALARM("响应【%s】和当前的请求列表头【%s】不匹配", node->name, request->node->name);
            ret =  -5;  //响应和激活的请求不匹配
        }
        else
        {
             result = __acs_pro_GetRPCMethodsResponse(member, node);
            LOG_SHOW("处理 GetRPCMethodsResponse 信息， 返回值 result:%d\n", result);
            ret = 0;
        }
    }
    else if(strcmp(node->name, "cwmp:SetParameterValuesResponse") == 0)
    {
        if(strcmp(request->node->name, "cwmp:SetParameterValues") != 0)
        {
            LOG_ALARM("响应【%s】和当前的请求列表头【%s】不匹配", node->name, request->node->name);
            
            ret = -5;  //响应和激活的请求不匹配
        }
        else
        {
            result = __acs_pro_SetParameterValuesResponse(member, node);
            LOG_SHOW("处理 SetParameterValuesResponse 信息， 返回值 result:%d\n", result);
            ret = 0;
        }   
    }
    else if(strcmp(node->name, "cwmp:GetParameterValuesResponse") == 0)
    {
        ;
    }

    
    sessionacs_request_destroy(request);    //释放内存
    
    return ret;  
}
/*==============================================================
                        rpc 错误码的处理
==============================================================*/
int sessionacs_fault_msg_pro(sessionacs_member_t *member, soap_node_t *node)
{
    //int ret;
    if(member == NULL || node == NULL)return -1;

    soap_node_show(node);
    
    return 0;
}


/*==============================================================
                        会话过程辅助函数
==============================================================*/
//0.1 等待接收
#define SESSIONCPE_RECV_TIMEOUT 30
static int __acsAux_wait_msg_timeout(sessionacs_member_t *member, int timeout)
{
    int ret;
    int result;

    LOG_SHOW("超时等待msg，超时时间%d秒\n", timeout);
    
    if(member == NULL)
    {
        return -99;  //参数出错
    }

    if(member->msgReady == 1)
    {
        LOG_ALARM("session->msgReady 不为 0");
        return -1;  
    }

    ret = -4;   //未知错误
    if(timeout == 0 )
    {
        return -2;  //无等待，参数错误
    }
    else if(timeout < 0)   //表示需要一直等待
    {
        pthread_mutex_lock(&(member->mutexMsg));
        while (member->msgReady == 0) {
            //LOG_SHOW("阻塞等待消息队列非空...\n");
            
            // 阻塞等待条件变量
            pthread_cond_wait(&(member->condMsgReady), &(member->mutexMsg)); 
        }
        
        pthread_mutex_unlock(&(member->mutexMsg));
        ret = 0;    //一直等待成功
    }
    else //超时等待
    {
        ret = 0;    //超时等待成功
        pthread_mutex_lock(&(member->mutexMsg));
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout; // 设置超时时间为 x 秒后
        while (member->msgReady == 0) {
            //LOG_SHOW("阻塞等待消息队列非空...\n");
            
            // 阻塞等待条件变量
            result = pthread_cond_timedwait(&(member->condMsgReady), &(member->mutexMsg), &ts);
            if (result == ETIMEDOUT) 
            {
               LOG_SHOW("等待超时，不再等待\n");
               ret = -3;    //超时等待失败
               break;
            }
        }
        pthread_mutex_unlock(&(member->mutexMsg));

    }

    return ret;    
}

//0.2 soap头部处理
static int __acsAux_saop_header_pro(sessionacs_member_t *member, soap_node_t *header)
{
    soap_node_t *nodeID;
    //soap_node_t *nodeHoldRequests;
    
    if(member == NULL || header == NULL)
        return -1;

    nodeID = soap_node_get_son(header, "cwmp:ID");
    if(nodeID != NULL)
    {
        strcpy(member->cwmpid, nodeID->name);
    }

    //1.4 版本 acs弃用 HoldRequests
//    nodeHoldRequests = soap_node_get_son(header, "cwmp:HoldRequests");
//    if(nodeHoldRequests != NULL)
//    {
//        if(strcmp(nodeHoldRequests->value, "true") == 0)
//            member->HoldRequests = 1;
//        else if(strcmp(nodeHoldRequests->value, "false") == 0)
//            member->HoldRequests = 0;
//    }
    
    return 0;
}

//0.3 soap信封 soap:body 处理
static int __acsAux_saop_body_pro(sessionacs_member_t *member, soap_node_t *body)
{
    int ret;
    soap_node_t *nodeTmp;
    if(member == NULL || body == NULL || body->son == NULL)return -99;

    //LOG_SHOW("要处理的信封:");
    //soap_node_show(body);
    //1、查找响应：acs为主体，接收来自cpe的响应 
    LINK_DATA_FOREACH_START(body->son, probeData)
    {   
        nodeTmp = (soap_node_t *)probeData;

        ret = cwmprpc_cpe_methodResponse_soap_name_match(nodeTmp->name);
        if(ret == 0)    //匹配成功
        {
            sessionacs_response_msg_pro(member, nodeTmp);
        }
    
    }LINK_DATA_FOREACH_END
        
    //2、查找请求：acs为主体，接收来自cpe的请求
    member->lastRecvRequestExist = 0;  ////此条信息会影响 cpe 是否发送请求
    LINK_DATA_FOREACH_START(body->son, probeData)
    {   
        nodeTmp = (soap_node_t *)probeData;

        ret = cwmprpc_acs_method_soap_name_match(nodeTmp->name);
        //LOG_SHOW("cwmprpc_acs_method_soap_name_match name:%s ret:%d\n", nodeTmp->name, ret);
        if(ret == 0)    //匹配成功
        {
            sessionacs_request_msg_pro(member, nodeTmp);
            if(member->lastRecvRequestExist == 0)  
            {
                member->lastRecvRequestExist = 1;
            }
        }
    
    }LINK_DATA_FOREACH_END

    //3、错误码
    LINK_DATA_FOREACH_START(body->son, probeData)
    {   
        nodeTmp = (soap_node_t *)probeData;

        ret = strcmp(nodeTmp->name, "soap:Fault");
        if(ret == 0)    //匹配成功
        {
            sessionacs_fault_msg_pro(member, nodeTmp);
        }
    
    }LINK_DATA_FOREACH_END

   return 0;
}


//1、建立连接（默认acs 被动接收到 Inform 消息，然后开始建立会话）
static int __acsAux_establish_connection(sessionacs_member_t *member)
{
#undef BUF_SIZE
#define BUF_SIZE 4096
    char buf[BUF_SIZE + 8] = {0};
    int pos;
    soap_node_t *nodeRoot;
    soap_node_t *nodeHeader;
    soap_node_t *nodeInform;
    int ret;
    int result;
    char buf1024[1024 + 8] = {0};

    if(member == NULL)return RET_FAILD;


    //1、等待可用msg
    pthread_mutex_lock(&(member->mutexMsg));
    member->msgReady = 0; 
    pthread_mutex_unlock(&(member->mutexMsg));
        
    
    result = __acsAux_wait_msg_timeout(member, 30);  //超时时间 x 秒
    if(result != 0)
    {
        LOG_SHOW("没等到可用的 msg\n");
        return -1;  //没等到可用的 msg
    }

    //2、取出队列第一个 msg，看是否匹配 Inform 消息
    ret = -99;    //未知错误
    pthread_mutex_lock(&(member->mutexMsg));
    if(member->msgLen > 0)
    {
        LOG_SHOW("接收到http msg消息 长度:%d 内容:\n【%s】\n", member->msgLen, member->msg);
        nodeRoot = soap_str2node(member->msg);
        //soap_node_show(soapNodeRoot);

        //soap 头部处理
        nodeHeader = soap_node_get_son(
                            soap_node_get_son(
                            nodeRoot, 
                            "soap:Envelope"), 
                            "soap:Header");
        result = __acsAux_saop_header_pro(member, nodeHeader); //返回值暂不作处理
       
        // 找到 "cwmp:InformResponse" 节点
        nodeInform = soap_node_get_son(
                            soap_node_get_son(
                            soap_node_get_son(
                            nodeRoot, 
                            "soap:Envelope"), 
                            "soap:Body"), 
                            "cwmp:Inform");
       if(nodeInform != NULL)  
       {
            //开始处理 Inform 信息 （这里略过，以后补足）
       
            ret = 0;     //找到了节点 "cwmp:Inform"
       }    
       else
       {
            ret = -2;    //没找到节点
       }
       soap_destroy_node(nodeRoot);  //用完后需及时释放内存
    }
    else
    {
        LOG_SHOW("接收到空的http msg信息\n");
        ret = -3;   //空的msg
    }
    pthread_mutex_unlock(&(member->mutexMsg));  

    if(ret == -2 || ret == -3)//不是预期的msg，为了 http 认证动作，这里选择回复简单http 内容
    {
        httpmsg_server_response_hello(buf1024, 1024);
        LOG_SHOW("开始发送 http 的回复信息\n");
        httpUser_send_str2(member->user, buf1024);
        
        return ret;
    }
    
    //3、 构建 InformResponse 消息
    //3.1 添加基础内容
    soap_obj_t *soap = soap_create();
    soap_header_t header;
    strcpy(header.ID, member->cwmpid);
    header.idEn = 1;
    header.holdRequestsEn = 0;  //1.4版本的acs弃用 HoldRequests
   
    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;

    soap_node_t *body = soap_node_get_son(root, "soap:Body");
    
    //3.2 添加rpc方法 InformResponse
    rpc_InformResponse_t dataInformResponse = {.MaxEnvelopes = 1};
    soap_node_append_son(body, soapmsg_to_node_InformResponse(dataInformResponse));

    //3.3 生成soap信封，并通过http 接口发送给对方   
    soap_node_to_str(soap->root, buf, &pos, BUF_SIZE);
    
    http_send_msg(member->user, (void *)buf, strlen(buf), 200, "OK");

    
    
    return 0;    
}


//2、接收和处理msg
static int __acsAux_recv(sessionacs_member_t *member)
{
    int ret;
    soap_node_t *nodeRoot;
    soap_node_t *nodeHeader;
    soap_node_t *nodeBody;
    int result;
    
    if(member == NULL)return -99;

    //1、超时接收新的信息
    pthread_mutex_lock(&(member->mutexMsg));  
    member->msgReady = 0;
    pthread_mutex_unlock(&(member->mutexMsg));
    result = __acsAux_wait_msg_timeout(member, 30); //SESSIONCPE_RECV_TIMEOUT
    if(result != 0)
    {
        LOG_SHOW("没等到可用的 msg\n");
        return -1;  //没等到可用的 msg，超时
    }

    //2、把soap信封解析为soap节点，注意：这里只处理单个信封，不支持多个信封
    //可能要考虑处理多个信封，以便兼容旧版本
    ret = -99;    //未知错误
    pthread_mutex_lock(&(member->mutexMsg));
    if(member->msgLen > 0)
    { 
        LOG_SHOW("接收到http信息，长度：%d，内容：\n【%s】\n", member->msgLen, member->msg);
        nodeRoot = soap_str2node(member->msg);
        if(nodeRoot == NULL)
        {
            ret = -2;  //msg 并不是预期的soap 信封，或者解析出错
        }
        else
        {
            //3.1 处理soap信封的 头部    
            nodeHeader = soap_node_get_son(
                                    soap_node_get_son(
                                    nodeRoot, 
                                    "soap:Envelope"), 
                                    "soap:Header");
            if(nodeHeader != NULL)
            {
                __acsAux_saop_header_pro(member, nodeHeader); 
            }
            
            //3.2 处理 soap:Body 里面的信息，包括请求、响应和错误码
            nodeBody = soap_node_get_son(
                                    soap_node_get_son(
                                    nodeRoot, 
                                    "soap:Envelope"), 
                                    "soap:Body");
            if(nodeBody != NULL)
            {
                ret = __acsAux_saop_body_pro(member, nodeBody); 

                ret = 0;
            }
            else
            {
                ret = -3;  //msg 并不是预期的soap 信封，或者解析出错
            }
        }  
    }
    else
    {
        LOG_SHOW("接收到空的http信息\n");
        ret = -4;   //空msg信息
    }
    
    pthread_mutex_unlock(&(member->mutexMsg));
    
    return ret;  //未知错误
}

//3、发送处理
static int __acsAux_send(sessionacs_member_t *member)
{
    int ret;
    char buf[SESSIONACS_MSG_SIZE + 8];
    int tmp;
    if(member == NULL || member->requestQueue == NULL)return -99;
    
    //acs 获得主动权，开始处理本地的请求队列
    //0、停止接收消息（或许不用）
    pthread_mutex_lock(&(member->mutexMsg));  
    member->msgReady = 1;
    pthread_mutex_unlock(&(member->mutexMsg));  
    
    //1、如果本地请求队列为空，那么发送 空http 报文；
    
    //pthread_mutex_lock(&(session->mutexRequestQueue));    //资源锁，如果有其他线程要访问requestQueue，则需要
    
    if(queue_isEmpty(member->requestQueue))
    {
        LOG_SHOW("请求队列为空\n");
        
        ret = http_send_msg(member->user, NULL, 0, 200, "OK");
        
        if(ret != RET_OK)
        {
            LOG_ALARM("报文发送失败");
            return -1;
        }
        return 1;
    }
    //2、开始处理请求队列
    else
    {
        
        sessionacs_request_t *request = NULL;
        //sessioncpe_request_t *requestTmp = NULL;
        ret = sessionacs_requestQueue_get_head(member, &request);
        if(request->active == 1)
        {
            if(request->node != NULL)
            {
                LOG_ALARM("已激活的请求【%s】在下一次的交流中，没有收到有效回复", request->node->name);
            }   
            else
            {
                LOG_ALARM("已激活的请求，既没有收到回复，而且 request->node 为 NULL");
            }
                
            sessionacs_requestQueue_out(member, &request); //出队列并释放该请求
            sessionacs_request_destroy(request);    
            return -4;  //已激活的请求在下一次交流中，没有收到有效回复
        }
        
        if(ret == RET_OK && request->nodeEn == 1)
        {
            //2.1 创建soap信封节点
            soap_obj_t *soap = soap_create();
            soap_header_t header = {
                .ID = "cwmp",
                .idEn = 1,
                //.HoldRequests = "false",
                .holdRequestsEn = 0
            };

            soapmsg_set_base(soap, &header);
            soap_node_t *root = soap->root;
            soap_node_t *body = soap_node_get_son(root, "soap:Body");
            soap_node_append_son(body, request->node);
            
            //2.2 激活请求，节点转化为字符串并发送信息
            request->active = 1;
            tmp = 0;    //代表要发送内容的起始字节，必须先清零
            soap_node_to_str(soap->root, buf, &tmp, SESSIONACS_MSG_SIZE);
            
            
            link_remove_node_by_data_pointer(body->son, (void *)(request->node));  //先剥离request->node，再释放soap节点 
            soap_destroy(soap);     
            
            ret = http_send_msg(member->user, buf, tmp, 200, "OK");
            if(ret != RET_OK)
            {
                LOG_ALARM("报文发送失败");
                return -2;  //报文发送失败
            }
            return 2;   //成功发送请求
        }
        else
        {
            return -3; //得到的request不可用，或者没有得到
        }  
    }
    
    return -99;     //未知错误
    
}


/*==============================================================
                        会话
==============================================================*/

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
    int faildCnt;
    
    while(1)
    {
        switch(member->status)
        {
           case sessionacs_status_start:{
                LOG_SHOW("\n-------------------【acs会话 start】---------------  \n");
                //开始允许 msgQueue 接收信息
                pthread_mutex_lock(&(member->mutexMsg));
                member->msgReady = 0;
                pthread_mutex_unlock(&(member->mutexMsg));
                faildCnt = 0;   //失败次数
                
                //进入下一阶段：建立连接
                member->status = sessionacs_status_establish_connection;    
                
           }break;
           case sessionacs_status_establish_connection:{    //建立连接
                LOG_SHOW("\n-------------------【acs会话 establish_connection】---------------  \n");
                ret = __acsAux_establish_connection(member);
                LOG_SHOW("__acsAux_establish_connection 返回值 ret:%d  \n", ret);
                if(ret == 0)
                {
                    //进入下一状态 recv；一般情况下，acs在上一次没收到请求时候才能到 send 状态
                    member->status = sessionacs_status_recv;
                }
                //【-1】等待超时【-2】没找到Inform节点【-3】空msg消息
                else if(ret == -1 || ret == -2 || ret == -3)
                {
                    faildCnt++;
                    if(faildCnt > 2)
                    {
                        LOG_ALARM("失败次数到达限定值，faildCnt:%d", faildCnt);
                        member->status = sessionacs_status_error;
                    }
                    
                    //如果无转移状态，将重入
                }
                else    //例如 -99
                {
                    member->status = sessionacs_status_error;
                }
               
                //outWhile = 1;
           }break;
           case sessionacs_status_send:{
                LOG_SHOW("\n-------------------【acs会话 send】---------------  \n");
                ret = __acsAux_send(member);
                LOG_SHOW("__acsAux_send 处理完成，返回值 ret:%d  \n", ret);
                // 【1】没有请求 【2】请求发送成功
                if(ret == 1 || ret == 2)
                {
                    //进入 recv 状态
                    member->status = sessionacs_status_recv;  
                }
                else 
                {   
                    //【-1】空报文发送失败 【-2】有msg报文发送失败 【-3】请求队列无法获取第一个元素 
                    if(ret == -1 || ret == -2 || ret == -3)  
                    {
                        member->status = sessionacs_status_disconnect;   
                    }
                    if(ret == -4)   //已激活的请求在下一次交流中，没有收到有效回复
                    {
                        ;   //无状态转移，表示重入此状态
                    }
                    //【-99】其他情况
                    else    
                    {
                        member->status = sessionacs_status_error; 
                    }   
                }

         
                //outWhile = 1;
           }break;    
           case sessionacs_status_recv:{  // taskQueue为空的情况下进入
                LOG_SHOW("\n-------------------【acs会话 recv】---------------  \n");

                ret = __acsAux_recv(member);
                LOG_SHOW("__acsAux_recv 处理完成，返回值 ret:%d  \n", ret);

                if(ret == 0)    //完成接收和处理
                {
                    //上一次接收到了请求，对方可能还有请求，所以acs应该等待
                    if(member->lastRecvRequestExist == 1)  
                    {
                        ;   //无状态转移，则会重入当前状态
                    }
                    else
                    {
                        //进入接收状态
                        member->status = sessionacs_status_send;
                    }
                    
                }
                else if(ret == -4)  //接收到空的msg 信息，表示cpe已经没有请求
                {
                    member->status = sessionacs_status_send;
                }                
                //【-1】等待超时【-2】未解析出nodeRoot节点【-3】未解析出nodeBody节点
                else if(ret == -1 || ret == -2 || ret == -3)
                {
                    member->status = sessionacs_status_disconnect;
                }
                else    //其他情况，例如 -99
                {
                    member->status = sessionacs_status_error;
                }
                //outWhile = 1;
           }break;
           case sessionacs_status_disconnect:{
                LOG_SHOW("\n-------------------【acs会话 disconnect】---------------  \n");

                
                outWhile = 1;
           }break;
           case sessionacs_status_end:{
                LOG_SHOW("\n-------------------【acs会话 end---------------  \n");

                outWhile = 1;
           }break;
           case sessionacs_status_error:{
                LOG_SHOW("\n-------------------【acs会话 error】---------------  \n");              
                outWhile = 1;

           }break;
           default:{    //其他的情况
                LOG_SHOW("\n-------------------【acs会话 default】---------------  \n");
                outWhile = 1;
           }break;
           //outWhile = 1;
         
        }

        
        if(outWhile)
        {
            LOG_SHOW("【acs会话，即将退出循环 ......】");
            break;
        }
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

        httpUser->retHttpMsg = -1;  //无动作

        pthread_mutex_lock(&(member->mutexMsg));
        if(member->msgReady == 0)  //说明msg被处理完了，现在可以接收新的消息
        {
            memset(member->msg, '\0', SESSIONACS_MSG_SIZE);
            if(payload->len <= 0 )    //载荷可能为空
            {   
                member->msgLen = 0;           
            }
            else
            {
                memcpy(member->msg, (void *)(payload->buf), payload->len);
                member->msgLen = payload->len;
            }
            member->msgReady = 1;
            
            pthread_cond_signal(&(member->condMsgReady)); //发送条件变量
            httpUser->retHttpMsg = 0;
        }
        else
        {
            httpUser->retHttpMsg = -2;   //返回信息：msg 还未被处理
        }

        pthread_mutex_unlock(&(member->mutexMsg));
        
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
        //一些测试，比如预先设置 member的请求队列
        sessionacs_request_t *requestTmp;
        char *strTmp;
        char buf64[64] = {0};
        //if(newMember->user->tcpUser->port == 8081 || newMember->user->tcpUser->port == 8082)  //特定端口测试
        if(newMember->user->tcpUser->port > 8080)
        {
            //清空 任务队列
            sessionacs_requestQueue_clear(newMember);
            //0、添加rpc方法 SetParameterValues
            //0.1 新任务
            requestTmp = sessionacs_requestQueue_in_new(newMember);  
            requestTmp->active = 0;
            requestTmp->nodeEn = 1;
            if(requestTmp != NULL)
            {
                //0.2 soap_node 存储数据
                rpc_GetRPCMethods_t dataGetRPCMethods ={
                    .null = NULL      
                };
                //0.3 节点存入新的 请求
                requestTmp->node = soapmsg_to_node_GetRPCMethods(dataGetRPCMethods);
            }
            //1、添加rpc方法 GetParameterValues
            //1.1 新任务
            requestTmp = sessionacs_requestQueue_in_new(newMember);  
            requestTmp->active = 0; //未被激活状态
            requestTmp->nodeEn = 1;
            if(requestTmp != NULL)
            {
                //1.2 soap_node 存储数据
                link_obj_t *linkParameterNames = link_create();     //链表成员数据的类型 ： char [256]

                strTmp = (char *)link_append_by_malloc(linkParameterNames, sizeof(256)); 
                strcpy(strTmp, "parameter_name_1");     //用于测试的参数名

                strTmp = (char *)link_append_by_malloc(linkParameterNames, sizeof(256)); 
                strcpy(strTmp, "parameter_name_2"); 

                rpc_GetParameterValues_t dataGetParameterValues = {
                    .ParameterNames = linkParameterNames     //链表成员数据的类型 ： char [256]
                };

                //1.3 节点存入新的 request
                requestTmp->node = soapmsg_to_node_GetParameterValues(dataGetParameterValues);
                link_destroy_and_data(linkParameterNames);

            }
            //2、添加rpc方法 SetParameterValues
            //2.1 新任务
            requestTmp = sessionacs_requestQueue_in_new(newMember);  
            requestTmp->active = 0;
            requestTmp->nodeEn = 1;
            if(requestTmp != NULL)
            {
                //2.2 soap_node 存储数据
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

                //2.3 节点存入新的 请求
                requestTmp->node = soapmsg_to_node_SetParameterValues(dataSetParameterValues);
                link_destroy_and_data(linkParameterList);

            }

            LOG_SHOW("当前请求列表的个数：%d\n", queue_get_num(newMember->requestQueue));   
        }
        else
        {
            LOG_SHOW("request 队列添加测试：未匹配到 tcp 接口 port:%d\n", newMember->user->tcpUser->port);
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








