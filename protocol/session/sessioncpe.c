




#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sessioncpe.h"
#include "log.h"
#include "pool2.h"
#include "http.h"
#include "queue.h"
#include "httpmsg.h"
#include "soap.h"
#include "soapmsg.h"




/*==============================================================
                        数据结构
==============================================================*/
//请求 结构体
typedef struct sessioncpe_request_t{
    soap_node_t *node;   //用soap节点来存储请求信息
    char nodeEn;
    char active;    //是否被激活：1 表示被激活，0表示没有被激活    
}sessioncpe_request_t;


//会话状态机
typedef enum sessioncpe_status_e{
    sessioncpe_status_start,    //开始
    sessioncpe_status_establish_connection,     //建立连接
    sessioncpe_status_send,     //发送状态
    sessioncpe_status_recv,    //等待接收状态
    sessioncpe_status_disconnect,   //会话完成，请求断开状态
    sessioncpe_status_end,   //结束
    sessioncpe_status_error   //某种故障
}sessioncpe_status_e; 

    
//cpe 会话对象
#define SESSIONCPE_MSG_SIZE 10240   //10k的缓存
typedef struct sessioncpe_obj_t{    

    //数据收发接口
    http_client_t *client;  
        
    //信息接收
    char msg[SESSIONCPE_MSG_SIZE + 8];
    int msgLen;
    int msgReady;  //msg是否已准备好，1表示准备好了，0表示没有准备好
    pthread_mutex_t mutexMsg; //互斥锁，保护msgQueue资源的互斥锁   
    pthread_cond_t condMsgReady;   //条件变量，表示有消息
    
    //状态机     
    sessioncpe_status_e status;    

    //线程
    pthread_t threadRecv;  //接收线程
    pthread_t threadSession;    //会话线程

    //会话控制信息
    char HoldRequests;  //soap头部控制信息，用于维持链接，1.4版本弃用；但是cpe还是需要支持，0代表false
    char cwmpid[64];
    char lastRecvRequestExist; //上一次的接收有acs请求，这意味着下一次发送不应该是请求（详情参考会话流程）
    char sendStop;  //1：表示不可发送请求，结果应该是不可逆的
    
    //请求队列
    queue_obj_t *requestQueue;
    pthread_mutex_t mutexRequestQueue; //互斥锁，保护 retquestQueue 资源的互斥锁
    
      
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
                        request 队列
==============================================================*/
//创建 request 对象
sessioncpe_request_t *sessioncpe_request_create()
{
    sessioncpe_request_t *request = (sessioncpe_request_t *)MALLOC(sizeof(sessioncpe_request_t));

    if(request == NULL)return NULL;

    request->active = 0;
    request->nodeEn = 0;
    
    
    return request;
}
//摧毁 request 对象
void sessioncpe_request_destroy(sessioncpe_request_t *request)
{
    if(request == NULL)return ;
    if(request->node != NULL && request->nodeEn == 1)
    {
        soap_destroy_node(request->node);
    }
    
    FREE(request);
}

//request 队列入队列
int sessioncpe_requestQueue_in(sessioncpe_obj_t *session, sessioncpe_request_t *request)
{
    if(session == NULL || request == NULL || session->requestQueue == NULL)return RET_FAILD;

    return queue_in_set_pointer(session->requestQueue, request);
    
}

//request 入队列，创建新的request 并返回指针
sessioncpe_request_t *sessioncpe_requestQueue_in_new(sessioncpe_obj_t *session)
{
    int ret;
    if(session == NULL ||  session->requestQueue == NULL)return NULL;

    sessioncpe_request_t *request = sessioncpe_request_create();
    if(request == NULL)return NULL;
    ret = sessioncpe_requestQueue_in(session, request);
    if(ret == RET_OK)
    {
        return request;
    }
    else
    {
        sessioncpe_request_destroy(request);
    }

    return NULL;
}

//request 队列出队列
int sessioncpe_requestQueue_out(sessioncpe_obj_t *session, sessioncpe_request_t **out)
{
    int ret;
    void *tmp = NULL;
    if(session == NULL || session->requestQueue == NULL || out == NULL)return RET_FAILD;
    ret = queue_out_get_pointer(session->requestQueue, &tmp);
    if(ret == RET_OK && tmp != NULL)
    {
        *out = (sessioncpe_request_t *)tmp;
        return RET_OK;
    }
    return RET_FAILD;
}
//request 得到队列头元素
int sessioncpe_requestQueue_get_head(sessioncpe_obj_t *session, sessioncpe_request_t **out)
{
    int ret;
    void *tmp = NULL;
    if(session == NULL || session->requestQueue == NULL || out == NULL)return RET_FAILD;
    
    ret = queue_get_head_pointer(session->requestQueue, &tmp);

    if(ret == RET_OK && tmp != NULL)
    {
        *out = (sessioncpe_request_t *)tmp;
        return RET_OK;
    }
    return RET_FAILD;
}

//request 队列清空
void sessioncpe_requestQueue_clear(sessioncpe_obj_t *session)
{
    if(session == NULL || session->requestQueue == NULL)return ;
    QUEUE_FOEWACH_START(session->requestQueue, probe)
    {
        if(probe->en == 1 && probe->d != NULL)
        {
            sessioncpe_request_t *iter = (sessioncpe_request_t *)(probe->d);
            sessioncpe_request_destroy(iter);
        }
    }QUEUE_FOEWACH_END

    queue_clear(session->requestQueue);
}

/*==============================================================
                        sessioncpe 对象
==============================================================*/
#define SESSION_CPE_REQUEST_QUEUE_SIZE 10
//创建会话 
sessioncpe_obj_t *sessioncpe_create()
{
    sessioncpe_obj_t *session = (sessioncpe_obj_t *)MALLOC(sizeof(sessioncpe_obj_t));

    if(session == NULL)return NULL;

    //数据收发接口
    session->client = NULL; 

    //信息接收
    memset(session->msg, '\0', SESSIONCPE_MSG_SIZE);
    session->msgLen = 0;
    session->msgReady = 0;  //默认不允许信息进入队列
    pthread_mutex_init(&(session->mutexMsg), NULL);   //初始化互斥锁
    pthread_cond_init(&(session->condMsgReady), NULL);  //准备好信息的条件变量

    //状态机
    session->status = sessioncpe_status_start;
    
    //线程（不用操作）
    
    //会话控制信息
    session->HoldRequests = 0;  //soap 封装头部，header 的参数
    memset(session->cwmpid, '\0', 64);     
    session->lastRecvRequestExist = 0;
    session->sendStop = 0; //默认开启发送
    
    //request 队列
    session->requestQueue = queue_create(SESSION_CPE_REQUEST_QUEUE_SIZE);
    pthread_mutex_init(&(session->mutexRequestQueue), NULL);   //初始化互斥锁  
    
    
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
    queue_destroy_and_data(session->requestQueue);
    
    //释放自己
    FREE(session);
}



/*==============================================================
                        应用
==============================================================*/
#if 0



//显示msgQueue 消息
void sessioncpe_show_msgQUeue(sessioncpe_obj_t *session)
{
    LOG_SHOW("\n---------------- show msg queue ----------------\n");
    if(session == NULL || session->msgQueue == NULL)
    {
        LOG_SHOW("[void]\n");
        return ;
    }

    char *p;
    int cnt = 0;
    QUEUE_FOEWACH_START(session->msgQueue, probe)
    {
        if(probe->en == 1 && probe->d != NULL)
        {
            sessioncpe_msg_t *data = (sessioncpe_msg_t *)(probe->d);

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



//-----------------------------------------------------------------会话过程辅助函数
//0.1、处理 msgQueue 的信息，取出soap 消息的第一个 信封的 soap:Body 节点
//返回值；注意参数 outNode，会指向新分配的动态内存，用完时请释放
static int _aux_msgQueue_pro(sessioncpe_obj_t *session, soap_node_t **outNode)
{
    sessioncpe_msg_t *msgData;
    char* strp;
    soap_node_t *nodeRoot;
    soap_node_t *nodeHeader;
    soap_node_t *nodeBody;
    soap_node_t *nodeTmp;
    
    *outNode = NULL;    //预设为空指针
    
    if(session == NULL || session->msgQueue == NULL)
        return -1;  //参数问题
    if(queue_isEmpty(session->msgQueue))
        return -2;  //队列为空

    queue_out_get_pointer(session->msgQueue, (void**)(&msgData));   
    if(msgData->d != NULL)
        return -3;  //信息为空

    strp = (char *)(msgData->d);
    strp[msgData->len] = '\0';
    //LOG_SHOW("-->msgData len:%d msg:\n【%s】\n", msgData->len, strp);
    nodeRoot = soap_str2node(strp);
    //soap_node_show(nodeRoot);


    //解析头部的有用信息，例如 HoldRequests
    nodeHeader = soap_node_get_son(
                        soap_node_get_son(
                        nodeRoot, 
                        "soap:Envelope"), 
                        "soap:header");
    if(nodeHeader != NULL && nodeHeader->son != NULL)
    {
        nodeTmp = soap_node_get_son(nodeHeader, "cwmp:HoldRequests");
        if(nodeTmp != NULL && nodeTmp->valueEn == 1)
        {
            if(strcmp(nodeTmp->value, "true") == 0)
            {
                session->HoldRequests = 1;
            }
            else if(strcmp(nodeTmp->value, "false") == 0)
            {
                session->HoldRequests = 0;
            }
            
        }
    }

    
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
#define SESSIONCPE_MSG_TIME_OUT 5
static void *__aux_thread_wait_msg(void *in)
{
    if(in == NULL)return NULL;
    int ret;
    //sessioncpe_msg_t *msgData;
    //char *strp;
    
    sessioncpe_obj_t *session = (sessioncpe_obj_t *)in;
    
    session->timeout = 0;
    
    //等待msgQueue有可用信息 ，需要条件变量   
    pthread_mutex_lock(&(session->mutexMsgQueue));
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += session->timeoutValue; // 设置超时时间为 x 秒后
    
    while (queue_isEmpty(session->msgQueue)) {
        LOG_SHOW("等待消息队列非空...\n");

        // 阻塞等待条件变量
        ret = pthread_cond_timedwait(&(session->condMsgQueueReady), &(session->mutexMsgQueue), &ts); 
        if (ret == ETIMEDOUT) {
            LOG_SHOW("等待超时，不再等待。\n");
            session->timeout = 1;
            break;
        }
    }

//    if(!queue_isEmpty(session->msgQueue))
//    {   
//        //取出第一个 msg
//        LOG_SHOW("开始处理消息队列表...\n");
//        
//        queue_out_get_pointer(session->msgQueue, (void**)(&msgData));    
//        strp = (char *)(msgData->d);
//        strp[msgData->len] = '\0';
//        LOG_SHOW("msgData len:%d msg:\n【%s】\n", msgData->len, strp);
//        
//        ret = 0;
//    }
    
    pthread_mutex_unlock(&(session->mutexMsgQueue));

    return NULL;
}





//0.3 等待 msgQueue 非空
static int __cpeAux_wait_msg_timeout(sessioncpe_obj_t *session, int timeout)
{
    int ret;
    int result;
    
    if(session == NULL || session->msgQueue == NULL)
    {
        return -1;  //参数出错
    }

    if(!queue_isEmpty(session->msgQueue))
    {
        return 0;  //不需要等待
    }

    if(timeout == 0 && queue_isEmpty(session->msgQueue))
    {
        return -2;  //无等待，但是不符合条件
    }

    if(timeout < 0)   //表示需要一直等待
    {
        pthread_mutex_lock(&(session->mutexMsgQueue));
        while (queue_isEmpty(session->msgQueue)) {
            LOG_SHOW("阻塞等待消息队列非空...\n");
            
            // 阻塞等待条件变量
            pthread_cond_wait(&(session->condMsgQueueReady), &(session->mutexMsgQueue)); 
        }
        
        pthread_mutex_unlock(&(session->mutexMsgQueue));
        return 0;
    }

    //超时等待
    result = 0;
    pthread_mutex_lock(&(session->mutexMsgQueue));
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout; // 设置超时时间为 x 秒后
    while (queue_isEmpty(session->msgQueue)) {
        LOG_SHOW("阻塞等待消息队列非空...\n");
        
        // 阻塞等待条件变量
        ret = pthread_cond_timedwait(&(session->condMsgQueueReady), &(session->mutexMsgQueue), &ts);
        if (ret == ETIMEDOUT) 
        {
           LOG_SHOW("等待超时，不再等待\n");
           result = -3;
           break;
        }
    }
    pthread_mutex_unlock(&(session->mutexMsgQueue));
    
    return result;    
}






//1、认证进度
int __cpeAux_http_auth(sessioncpe_obj_t *session)
{
    char buf2048[2048] = {0};
    sessioncpe_msg_t *msgData;
    char *strp;
    int ret;
    
    if(session == NULL)return -1;
    //1、发送简单 http 信息给acs，看是否要认证
    char *p = buf2048;
    httpmsg_client_say_hello(p, 2048);
    http_client_send(session->client, buf2048, 2048);

    //2、等待可用msg
    if(queue_isEmpty(session->msgQueue))
    {
        //允许 msg 的接收
        pthread_mutex_lock(&(session->mutexMsgQueue));
        session->msgRecvEn = 1; 
        pthread_mutex_unlock(&(session->mutexMsgQueue));
        LOG_SHOW("超时等待msg %d秒\n", 30);
        ret = __cpeAux_wait_msg_timeout(session, 30);  //超时时间 x 秒
        if(ret != 0)
        {
            LOG_SHOW("没等到可用的 msg\n");
            return -2;  //没等到可用的 msg
        }
    }
        
    //3、取出队列中的第一个 msg
    pthread_mutex_lock(&(session->mutexMsgQueue));
    queue_out_get_pointer(session->msgQueue, (void**)(&msgData));  
    strp = (char *)(msgData->d);

    if(strp != NULL)
    {
        strp[msgData->len] = '\0';
        LOG_INFO("接收到http消息 长度:%d 内容:\n【%s】\n", msgData->len, strp);
    }
    else
    {
        LOG_INFO("接收到空的 http消息\n");
    }

    pthread_mutex_unlock(&(session->mutexMsgQueue));

   
    return 0;   //能接收到 msg 信息，说明可以通过http认证
}

//2、建立连接（默认是cpe主动发起连接请求）
static int __cpeAux_establish_connection(sessioncpe_obj_t *session)
{
    char buf64[64] = {0};
    char buf2048[2048 + 8] = {0};
    int pos = 0;
    int ret;
    sessioncpe_msg_t *msgData;
    char *strp;
    soap_node_t *nodeInformResponse;
    soap_node_t *soapNodeRoot;
    int result;

    if(session == NULL || session->msgQueue == NULL)return -1;
    
 
    //1、构建并发送 Inform 消息
    //1.1 添加基础内容
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "start",
        .idEn = 1,
        //.HoldRequests = "false",        //1.4 cwmp 版本弃用此元素
        //.holdRequestsEn = 1
    };

    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;
    soap_node_t *body = soap_node_get_son(root, "soap:Body");
    
    //1.2 添加rpc方法 Inform
    rpc_Inform_t dataInform;
    
    //1.2.1 Inform 的 DeviceId
    strcpy(dataInform.DeviceId.Manufacturer, "设备制造商的名称");
    strcpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符"); 
    //strncpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符", 6); //OUI只有6个字节
    strcpy(dataInform.DeviceId.ProductClass, "特定序列号的产品类别的标识符");
    strcpy(dataInform.DeviceId.SerialNumber, "特定设备的标识符");
    
    //1.2.2 Inform 的 Event、eventNum、MaxEnvelopes、 CurrentTime、 RetryCount
    
    dataInform.eventNum = 2;
    
    strcpy(dataInform.Event[0].EventCode, "0 BOOTSTRAP");
    strcpy(dataInform.Event[0].CommandKey, "CommandKey_0");
    strcpy(dataInform.Event[1].EventCode, "1 BOOT");
    strcpy(dataInform.Event[1].CommandKey, "CommandKey_1");
    
    dataInform.MaxEnvelopes = 1;
    cwmprpc_str2dateTime("2024-11-19T17:51:00", &(dataInform.CurrentTime));
    dataInform.RetryCount = 0;

    //1.2.3 Inform 的 ParameterList，链表成员类型为 ParameterValueStruct
    dataInform.ParameterList = link_create();
    ParameterValueStruct *informPvs1 = (ParameterValueStruct *)link_append_by_malloc
                                        (dataInform.ParameterList, sizeof(ParameterValueStruct));
    strcpy(informPvs1->Name, "parameter_1");
    strcpy(buf64, "value_1");
    informPvs1->Value = buf64;
    informPvs1->valueLen = strlen(informPvs1->Value);

    ParameterValueStruct *informPvs2 = (ParameterValueStruct *)link_append_by_malloc
                                        (dataInform.ParameterList, sizeof(ParameterValueStruct));
    strcpy(informPvs2->Name, "parameter_2");
    strcpy(buf64, "value_2");
    informPvs2->Value = buf64;
    informPvs2->valueLen = strlen(informPvs2->Value);
    
    soap_node_append_son(body, soapmsg_to_node_Inform(dataInform));

    link_destroy_and_data(dataInform.ParameterList);

    //1.3、生成soap信封，并通过http 接口发送给 acs   
    soap_node_to_str(soap->root, buf2048, &pos, 2048);
    
    http_client_send_msg(session->client, (void *)buf2048, strlen(buf2048), "POST", "/cwmp");
 
    //2、等待对方的响应  
    if(queue_isEmpty(session->msgQueue))
    {
        //允许 msg 的接收
        pthread_mutex_lock(&(session->mutexMsgQueue));
        session->msgRecvEn = 1; 
        pthread_mutex_unlock(&(session->mutexMsgQueue));
        LOG_SHOW("超时等待msg %d秒\n", 30);
        ret = __cpeAux_wait_msg_timeout(session, 30);  //超时时间 x 秒
        if(ret != 0)
        {
            LOG_SHOW("没等到可用的 msg\n");
            return -2;  //没等到可用的 msg
        }
    }
  

    //3、取出队列第一个 msg，看是否匹配 InformResponse 消息
    result = -5;    //未知错误
    pthread_mutex_lock(&(session->mutexMsgQueue));
    queue_out_get_pointer(session->msgQueue, (void**)(&msgData));    
    if(msgData->d != NULL)  //msgData 可能是空消息，这里要注意
    {
        strp = (char *)(msgData->d);
        strp[msgData->len] = '\0';
        LOG_SHOW("接收到http msg消息 长度:%d 内容:\n【%s】\n", msgData->len, strp);

        soapNodeRoot = soap_str2node(strp);
        //soap_node_show(soapNodeRoot);
        
        nodeInformResponse = soap_node_get_son(
                            soap_node_get_son(
                            soap_node_get_son(
                            soapNodeRoot, 
                            "soap:Envelope"), 
                            "soap:Body"), 
                            "cwmp:InformResponse");
       if(nodeInformResponse != NULL)    
            result = 0;     //找到了节点 "cwmp:InformResponse"
       else
            result = -3;    //没找到节点 
       
       soap_destroy_node(soapNodeRoot);  //用完后需及时释放内存
    }
    else
    {
        LOG_SHOW("接收到空的http msg信息\n");
        result = -4;   //空的msg
    }
    pthread_mutex_unlock(&(session->mutexMsgQueue));    

 
    
    return result;  
}

//3、任务队列处理
static int __aux_task_queue_pro(sessioncpe_obj_t *session)
{
    if(session == NULL || session->taskQueue == NULL)return -99;
    //sessioncpe_msg_t *msgData;
    //char *strp;
    //int i;
    int ret;
    //soap_node_t *nodeRoot;
    soap_node_t *nodeBody;
    soap_node_t *nodeTmp;
    sessioncpe_task_t *taskTmp;
    pthread_t threadWaitMsg;
    
    
    //1、任务列表为空
    if(queue_isEmpty(session->taskQueue))     
    {
        //如果 msgQueue 未空，那么处理队列的信息，有可能会激发新的任务，例如 rpc response
        
        pthread_mutex_lock(&(session->mutexMsgQueue));
        ret = _aux_msgQueue_pro(session, &nodeBody);
        pthread_mutex_unlock(&(session->mutexMsgQueue));

        //LOG_SHOW("-----------------------mark 1、任务列表为空\n");
        soap_node_show(nodeBody);
        if(ret == 0)
        {
            LINK_DATA_FOREACH_START(nodeBody->son, probeData){
                nodeTmp = (soap_node_t *)probeData;
                ret = cwmprpc_cpe_method_soap_name_match(nodeTmp->name);
                if(ret == 0)
                {
                    pthread_mutex_lock(&(session->mutexTaskQueue));
                    taskTmp = sessioncpe_taskQueue_in(session, sessioncpe_task_type_rpc_response);
                    pthread_mutex_unlock(&(session->mutexTaskQueue));
                    if(taskTmp != NULL) //添加任务队列
                    {   
                        strcpy(taskTmp->data.rpcResponse.method, nodeTmp->name);
                        taskTmp->data.rpcResponse.soapNode = nodeTmp;
                        taskTmp->data.rpcResponse.status = sessioncpe_task_rpc_response_status_start;
                   
                    }
                }
                
            }LINK_DATA_FOREACH_END  
            
            return -1;  //代表任务列表为空，但是成功处理了 msgQueue
        }
        else if(ret == -1)   //参数问题
        {
            LOG_ALARM("_aux_msgQueue_pro 参数问题");
            return -99;  //处理出现故障
        }
        else if(ret == -2)  //msgQueue 为空 
        {
            return -2;  //代表任务列表为空，msgQueue 也为空
        }
        else if(ret == -3)  //msg 信息为空 
        {
            return -3;  //代表任务列表为空，msg 也为空
        }
        else if(ret == -4)  //代表任务列表为空，msg 不为空，但是解析内容不是期望的soap封装
        {
            return -4;
        }
        
        return -99;
    }

    //2、从列表中取得当前任务，然后处理
    //LOG_SHOW("-----------------------mark 2、从列表中取得当前任务，然后处理\n");
    sessioncpe_task_t *task = NULL;
    queue_out_get_pointer(session->taskQueue, (void *)(&task));
    if(task == NULL)
        return -99;     //出现预料之外的错误

    switch(task->type)  //按照类型的不同来处理任务
    {
        case sessioncpe_task_type_rpc_request:{
            



            

        }break;
        case sessioncpe_task_type_rpc_response:{

        }break;
        case sessioncpe_task_type_void_post:{
            //任务为空，cpe主动发送空信息体的 http post报文；若30秒后未收到回复，
            //则造成“会话未成功终止”；测试时可能会把30秒调小
            //1、发送空信息报文
            ret = http_client_send_msg(session->client, NULL, 0, "POST", "/cwmp");
            if(ret != RET_OK)
            {
                LOG_ALARM("故障：无法发送数据");
                return -99;     
            }
            
            
            //2、希望得到回复的信息，30秒是超时时间（测试时可能会改小一点）
            pthread_create(&threadWaitMsg, NULL, __aux_thread_wait_msg, (void *)session);
            pthread_join(threadWaitMsg, NULL);
            LOG_SHOW(" 超时等待相关的值 timeout:%d\n", session->timeout);
            
            if(session->timeout == 1)
                return -5;  //sessioncpe_task_type_void_post 任务下，等待超时
            else
                return 1;   //任务 void_post 处理成功
        }break;
        default:{

        }break;
    }

    return 0;
}


//3、任务队列处理
static int __cpeAux_send(sessioncpe_obj_t *session)
{

    if(session == NULL)
    {
        return -1;  //参数错误 
    }

    
    




}







//会话线程
void *thread_sessioncpe(void *in)
{
    int ret;
    int outWhile;
    //static int faildCnt;
    
    if(in == NULL)
    {
        LOG_ALARM("thread_sessioncpe: in is NULL");
        return NULL;
    }

    sessioncpe_obj_t *session = (sessioncpe_obj_t *)in;
    sessioncpe_task_t *voidTask;
    outWhile = 0;
    while(1)
    {
        switch(session->status)     //状态机
        {
            case sessioncpe_status_start:   //开始，需要确定http认证
            {
                LOG_SHOW("\n-------------------【cpe会话 start】---------------  \n");
                ret = __cpeAux_http_auth(session);
                LOG_SHOW("__cpeAux_http_auth ret:%d\n", ret);
                if(ret == 0)    //说明通过了一次握手
                {
                    //状态转移前的准备：禁止接收msg信息，然后清空 msg 列表
                    pthread_mutex_lock(&(session->mutexMsgQueue));
                    session->msgRecvEn = 0; 
                    sessioncpe_msg_clear(session); 
                    pthread_mutex_unlock(&(session->mutexMsgQueue));
                    //进入 connection 状态
                    session->status = sessioncpe_status_establish_connection;      
                }
                else 
                {
                    session->status = sessioncpe_status_error;  //进入error 状态    
                }

            }break;
            case sessioncpe_status_establish_connection:
            {
                LOG_SHOW("\n-------------------【cpe会话 connection】---------------  \n");
                ret = __cpeAux_establish_connection(session);
                LOG_SHOW("__cpeAux_establish_connection ret:%d\n", ret);

                if(RET_OK == ret)
                {   
                    //状态转移前的准备：禁止接收msg信息，然后清空 msg 列表
                    pthread_mutex_lock(&(session->mutexTaskQueue));
                    session->msgRecvEn = 0; 
                    sessioncpe_taskQueue_clear(session);
                    pthread_mutex_unlock(&(session->mutexTaskQueue));
                   
                    //进入 send 状态
                    session->status = sessioncpe_status_send;   
                }    
                else
                {
                    session->status = sessioncpe_status_error;  //进入error 状态
                }
                
            }break;             
            case sessioncpe_status_send:  //主动向 acs发送 空http post 报文
            {
                LOG_SHOW("\n-------------------【cpe会话 send】---------------  \n");





                
                
                
            }break; 
            case sessioncpe_status_recv:  //主动向 acs发送 空http post 报文
            {
                LOG_SHOW("\n-------------------【cpe会话 task_void】---------------  \n");
                voidTask = sessioncpe_taskQueue_in(session, sessioncpe_task_type_void_post);
                if(voidTask == NULL)
                {
                    LOG_ALARM("任务新增失败\n");
                    session->status = sessioncpe_status_error;   //状态转移
                }
                                
                session->status = sessioncpe_status_task;   //状态转移
                
            }break; 
            case sessioncpe_status_disconnect:
            {
                LOG_SHOW("\n-------------------【cpe会话 disconnect】---------------  \n");
                LOG_SHOW("测试：cpe会话 离开循环 ......\n");
                outWhile = 1;
            }break;
            case sessioncpe_status_end:
            {

            }break;
            case sessioncpe_status_error:   //某种故障
            {
                LOG_ALARM("某种故障，开始离开循环");
                outWhile = 1;
            }break;
//            case sessioncpe_status_task:
//            {              
//                LOG_SHOW("\n-------------------【cpe会话 task_pro】---------------  \n");
//                ret = __aux_task_queue_pro(session);
//
//                LOG_SHOW("__aux_task_queue_pro 返回值 ret:%d \n", ret);
//                switch(ret)
//                {
//                    case -1:{   //代表任务列表为空，但是成功处理了 msgQueue
//                        ;
//                    }break;
//                    case -2:{   //任务列表为空，msgQueue 也为空
//                        session->timeoutValue = 5;   //超时 x 秒
//                        session->timeout = 0;
//                        session->status = sessioncpe_status_task_void;  //状态转移
//                    }break;
//                    case -3:{   //msg 信息为空 
//                        //如果soap头部元素 HoldRequests 为false， 那么需要断开连接，此元素1.4版本废除
//                        //但是如果cpe收到此元素，仍然需要支持
//                        if(session->HoldRequests == 0)
//                        {
//                            LOG_SHOW("收到 acs 空的http信息，会话即将断开连接\n");
//                            session->status = sessioncpe_status_disconnect;  //状态转移 至 断开连接
//                        }
//                        
//                    }break;
//                    case -4:{   //代表任务列表为空，msg 不为空，但是解析内容不是期望的soap封装
//                        ; 
//                    }break;
//                    case -5:{   //return -5;  //任务 void_post 下，等待msg 超时
//                        LOG_SHOW("等待 msg信息超时，会话未成功而终止\n");
//                        session->status = sessioncpe_status_disconnect;  //状态转移
//                    }break;
//                    case -99:{
//                        session->status = sessioncpe_status_error;  //状态转移
//                    }break;
//                    case 1:{   //任务 void_post 处理成功，说明有新的
//                        ;
//                    }break;
//
//                    default:{
//
//                    }break;
//                    
//
//                }
//                
//                
//                outWhile = 1;
//            }break;
            default:    //离开循环
            {
                LOG_SHOW("【cpe会话】离开循环 ......\n");
                outWhile = 1;
            }break;

        } 

        //system("sleep 2");
        if(outWhile)break;
    }

    return NULL;
}
#endif

/*==============================================================
                        cpe为主题，接收到来自acs的请求
==============================================================*/
//1、对 "cwmp:GetRPCMethods" 的响应
static int __cpe_pro_GetRPCMethods(sessioncpe_obj_t *session)
{
    int i;
    int num;
    soap_node_t *nodeBody;
    char buf[SESSIONCPE_MSG_SIZE + 8] = {0};
    int posTmp;
    
    if(session == NULL)return -1;

    //1、本地处理（无）
    
    //2、发送响应消息
    //2.1 构建信封基础
    soap_obj_t *soap = soap_create();
    soap_header_t header;
    strcpy(header.ID, session->cwmpid);    //和
    header.idEn = 1,
    header.holdRequestsEn = 0;    
    
    soapmsg_set_base(soap, &header);
    nodeBody = soap_node_get_son(soap->root, "soap:Body");

    //2.2 构建响应节点
    rpc_GetRPCMethodsResponse_t dataGetRPCMethodsResponse;
    dataGetRPCMethodsResponse.MethodList = link_create();
    char *methodList[] = {       //或者用 cwmprpc_acs_methodResponse_g
        "Inform",
        "GetRPCMethods",
        "TransferComplete"
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
    soap_node_to_str(soap->root, buf, &posTmp, SESSIONCPE_MSG_SIZE);
    soap_destroy(soap); //回收内存
    http_client_send_msg(session->client, (void *)buf, posTmp, "POST", "/cwmp");   

    return 0;
}

//2、对 "cwmp:SetParameterValues" 的响应
static int __cpe_pro_SetParameterValues(sessioncpe_obj_t *session, soap_node_t *node)
{
    //int i;
    //int num;
    soap_node_t *nodeBody;
    char buf[SESSIONCPE_MSG_SIZE + 8] = {0};
    int posTmp;
    soap_node_t *nodeTmp = NULL;
    
    if(session == NULL || node == NULL)return -1;

    //1、本地处理
    /*
        <cwmp:SetParameterValues>
            <ParameterList soapenc:arrayType=tns:ParameterValueStruct[1]>
                <ParameterValueStruct>
                    <Name>
                        parameter_name_1
                    </Name>
                    <Value>
                        76616c75655f31
                    </Value>
                </ParameterValueStruct>
            </ParameterList>
        </cwmp:SetParameterValues>

        关于Value：76616c75655f31，方法是用十六进制显示数据（这是个人方法），需要结合协议去优化
    */
  
    nodeTmp =   soap_node_get_son(
                soap_node_get_son(
                node, 
                "ParameterList"),
                "ParameterValueStruct");
                
    if(nodeTmp == NULL)
    {
        LOG_SHOW("nodeTmp is NULL\n");
    }
    else
    {
        posTmp = 0;
        soap_node_to_str(nodeTmp, buf, &posTmp, SESSIONCPE_MSG_SIZE);
        LOG_SHOW("要设置的参数，相关节点为：【%s】\n", buf);
    }
    
    //2、发送响应消息
    //2.1 构建信封基础
    soap_obj_t *soap = soap_create();
    soap_header_t header;
    strcpy(header.ID, session->cwmpid);    
    header.idEn = 1,
    header.holdRequestsEn = 0;    
    
    soapmsg_set_base(soap, &header);
    nodeBody = soap_node_get_son(soap->root, "soap:Body");

    //2.2 构建响应节点
    rpc_SetParameterValuesResponse_t dataSetParameterValuesResponse = {
        .Status = 1
    };
    soap_node_append_son(nodeBody, soapmsg_to_node_SetParameterValuesResponse(dataSetParameterValuesResponse));
    

    //3、发送 信息
    posTmp = 0;
    soap_node_to_str(soap->root, buf, &posTmp, SESSIONCPE_MSG_SIZE);
    soap_destroy(soap); //回收内存
    http_client_send_msg(session->client, (void *)buf, posTmp, "POST", "/cwmp");   

    return 0;

}



//2、对 "cwmp:GetParameterValues" 的响应
static int __cpe_pro_GetParameterValues(sessioncpe_obj_t *session, soap_node_t *node)
{
    //int i;
    //int num;
    soap_node_t *nodeBody;
    char buf[SESSIONCPE_MSG_SIZE + 8] = {0};
    int posTmp;
    soap_node_t *nodeTmp = NULL;
    char buf64[64] = {0};
    
    if(session == NULL || node == NULL)return -1;

    //1、本地处理
    posTmp = 0;
    soap_node_to_str(node, buf, &posTmp, SESSIONCPE_MSG_SIZE);
    LOG_SHOW("要获取的参数，相关节点为：【%s】\n", buf);
    
    
    //2、发送响应消息
    //2.1 构建信封基础
    soap_obj_t *soap = soap_create();
    soap_header_t header;
    strcpy(header.ID, session->cwmpid);    
    header.idEn = 1,
    header.holdRequestsEn = 0;    
    
    soapmsg_set_base(soap, &header);
    nodeBody = soap_node_get_son(soap->root, "soap:Body");

    //2.2 构建响应节点
    link_obj_t *gpvr_linkParameterList = link_create();  //链表成员类型 ParameterValueStruct
    
    ParameterValueStruct *gpvr_pvs1 = (ParameterValueStruct *)link_append_by_malloc
                                    (gpvr_linkParameterList, sizeof(ParameterValueStruct));
    strcpy(gpvr_pvs1->Name, "parameter_1");
    strcpy(buf64, "value");
    gpvr_pvs1->Value = buf64;
    gpvr_pvs1->valueLen = strlen(gpvr_pvs1->Value); 

    rpc_GetParameterValuesResponse_t dataGetParameterValuesResponse = {
        .ParameterList = gpvr_linkParameterList
    };
    
    soap_node_append_son(nodeBody, soapmsg_to_node_GetParameterValuesResponse(dataGetParameterValuesResponse));
    link_destroy_and_data(gpvr_linkParameterList);

    //3、发送 信息
    posTmp = 0;
    soap_node_to_str(soap->root, buf, &posTmp, SESSIONCPE_MSG_SIZE);
    soap_destroy(soap); //回收内存
    http_client_send_msg(session->client, (void *)buf, posTmp, "POST", "/cwmp");   

    return 0;

}




//cpe为主题，接收到来自acs的请求
/*
    参考 cwmprpc_cpe_method_g
    
    "cwmp:GetRPCMethods",
    "cwmp:SetParameterValues",
    "cwmp:GetParameterValues",
    "cwmp:GetParameterNames",
    "cwmp:SetParameterAttributes",
    "cwmp:GetParameterAttributes",
    "cwmp:AddObject",
    "cwmp:DeleteObject",
    "cwmp:Reboot",
    "cwmp:Download"

*/
int sessioncpe_request_msg_pro(sessioncpe_obj_t *session, soap_node_t *node)
{
    int ret;
    if(session == NULL || node == NULL)return -1;

    if(strcmp(node->name, "cwmp:GetRPCMethods") == 0)
    {
        ret = __cpe_pro_GetRPCMethods(session);
        LOG_SHOW("cpe为主题，接收到来自acs的请求 GetRPCMethods ， 返回值 ret:%d", ret);
        return 0;
    }
    else if(strcmp(node->name, "cwmp:SetParameterValues") == 0)
    {
        ret = __cpe_pro_SetParameterValues(session, node);
        LOG_SHOW("cpe为主题，接收到来自acs的请求 SetParameterValues ， 返回值 ret:%d", ret);
        return 0;
        
    }
    else if(strcmp(node->name, "cwmp:GetParameterValues") == 0)
    {
        ret = __cpe_pro_GetParameterValues(session, node);
        LOG_SHOW("cpe为主题，接收到来自acs的请求 GetParameterValues ， 返回值 ret:%d", ret);
        return 0;
    }

    //如果有些请求没有写，可以发送空http信息应付
    LOG_SHOW("因为没有配置【%s】的响应，所以用空消息发送应付（仅开发测试时候使用）\n", node->name);
    http_client_send_msg(session->client, NULL, 0, "POST", "/cwmp");   
   
    
    return -2;  //没有匹配到对应的响应函数
}

/*==============================================================
                        cpe为主体，收到来自acs的响应
==============================================================*/
//处理 GetRPCMethodsResponse 信息
static int __cpe_pro_GetRPCMethodsResponse(sessioncpe_obj_t *session, soap_node_t *node)
{
    if(session == NULL )return -1;
    //测试
    LOG_SHOW("GetRPCMethodsResponse 信息的处理，内容为（该内容控制台会显示但log不显示）：");
    soap_node_show(node);

    return 0;
}

//cpe为主体，收到来自acs的响应
/*
    "cwmp:InformResponse"
    "cwmp:GetRPCMethodsResponse"
    "cwmp:TransferCompleteResponse"
*/
int sessioncpe_response_msg_pro(sessioncpe_obj_t *session, soap_node_t *node)
{
    int ret;
    //int exist;
    if(session == NULL || node == NULL)return -1;
     //请求队列（requestQueue）的对应，这里只考虑了队列头元素，
     //如果要考虑全部请求是否和响应匹配，则比较复杂
    sessioncpe_request_t *request = NULL;
    sessioncpe_requestQueue_get_head(session, &request);
    //exist = 0;
    if(request != NULL && request->active == 1)
    {
        ret = sessioncpe_requestQueue_out(session, &request);
        if(ret == RET_OK && request != NULL && request->nodeEn == 1 && request->node != NULL)
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
        //请求列表头元素，和响应的内容不匹配，例如cwmp:GetRPCMethodsResponse 应该
        //和 cwmp:GetRPCMethods 对应
        return -3;  
    }
    

    
    //处理响应
    if(strcmp(node->name, "cwmp:InformResponse") == 0)
    {
        ;
    }
    if(strcmp(node->name, "cwmp:GetRPCMethodsResponse") == 0)
    {
        if(strcmp(request->node->name, "cwmp:GetRPCMethods") != 0)
        {
            LOG_ALARM("响应【%s】和当前的请求列表头【%s】不匹配", node->name, request->node->name);
            sessioncpe_request_destroy(request);    //释放内存
            return -5;
        }
        sessioncpe_request_destroy(request);    //释放内存
        
        ret = __cpe_pro_GetRPCMethodsResponse(session, node);
        LOG_SHOW("cpe为主体，收到来自acs的 GetRPCMethodsResponse 信息， 返回值 ret:%d\n", ret);
        return 0;
    }
    else if(strcmp(node->name, "cwmp:TransferCompleteResponse") == 0)
    {
        
        ;
    }
    
   
    
    //有些没有实现 ...... 
    
    return -2;  //没有匹配到对应的响应函数
}
/*==============================================================
                        rpc 错误码的处理
==============================================================*/
int sessioncpe_fault_msg_pro(sessioncpe_obj_t *session, soap_node_t *node)
{
    //int ret;
    if(session == NULL || node == NULL)return -1;

    soap_node_show(node);
    
    return 0;
}


/*==============================================================
                        会话辅助应用
==============================================================*/
//0.1 等待接收
#define SESSIONCPE_RECV_TIMEOUT 30
static int __cpeAux_wait_msg_timeout(sessioncpe_obj_t *session, int timeout)
{
    int ret;
    int result;

    LOG_SHOW("超时等待msg，超时时间%d秒\n", timeout);
    
    if(session == NULL)
    {
        return -99;  //参数出错
    }

    if(session->msgReady == 1)
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
        pthread_mutex_lock(&(session->mutexMsg));
        while (session->msgReady == 0) {
            //LOG_SHOW("阻塞等待消息队列非空...\n");
            
            // 阻塞等待条件变量
            pthread_cond_wait(&(session->condMsgReady), &(session->mutexMsg)); 
        }
        
        pthread_mutex_unlock(&(session->mutexMsg));
        ret = 0;
    }
    else //超时等待
    {
        ret = 0;
        pthread_mutex_lock(&(session->mutexMsg));
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout; // 设置超时时间为 x 秒后
        while (session->msgReady == 0) {
            //LOG_SHOW("阻塞等待消息队列非空...\n");
            
            // 阻塞等待条件变量
            result = pthread_cond_timedwait(&(session->condMsgReady), &(session->mutexMsg), &ts);
            if (result == ETIMEDOUT) 
            {
               LOG_SHOW("等待超时，不再等待\n");
               ret = -3;
               break;
            }
        }
        pthread_mutex_unlock(&(session->mutexMsg));

    }

    return ret;    
}

//0.2 soap头部处理
static int __cpeAux_saop_header_pro(sessioncpe_obj_t *session, soap_node_t *header)
{
    soap_node_t *nodeID;
    soap_node_t *nodeHoldRequests;
    
    if(session == NULL || header == NULL)
        return -1;

    nodeID = soap_node_get_son(header, "cwmp:ID");
    if(nodeID != NULL)
    {
        strcpy(session->cwmpid, nodeID->name);
    }

    nodeHoldRequests = soap_node_get_son(header, "cwmp:HoldRequests");
    if(nodeHoldRequests != NULL)
    {
        if(strcmp(nodeHoldRequests->value, "true") == 0)
            session->HoldRequests = 1;
        else if(strcmp(nodeHoldRequests->value, "false") == 0)
            session->HoldRequests = 0;
    }
    
    return 0;
}

//0.3 soap信封 soap:body 处理
static int __cpeAux_saop_body_pro(sessioncpe_obj_t *session, soap_node_t *body)
{
    int ret;
    soap_node_t *nodeTmp;
    if(session == NULL || body == NULL || body->son == NULL)return -99;

    //1、查找响应；cpe为主体，接收到来自acs的响应
    LINK_DATA_FOREACH_START(body->son, probeData)
    {   
        nodeTmp = (soap_node_t *)probeData;

        ret = cwmprpc_acs_methodResponse_soap_name_match(nodeTmp->name);
        if(ret == 0)    //匹配成功
        {
            sessioncpe_response_msg_pro(session, nodeTmp);
        }
    
    }LINK_DATA_FOREACH_END
        
    //2、查找请求；cpe为主题，接收到来自acs的请求
    session->lastRecvRequestExist = 0;  ////此条信息会影响 cpe 是否发送请求
    LINK_DATA_FOREACH_START(body->son, probeData)
    {   
        nodeTmp = (soap_node_t *)probeData;

        ret = cwmprpc_cpe_method_soap_name_match(nodeTmp->name);
        if(ret == 0)    //匹配成功
        {
            sessioncpe_request_msg_pro(session, nodeTmp);
            if(session->lastRecvRequestExist == 0)  
            {
                session->lastRecvRequestExist = 1;
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
            sessioncpe_fault_msg_pro(session, nodeTmp);
        }
    
    }LINK_DATA_FOREACH_END

   return 0;
}



//1、http认证过程（因为http功能没有完善，所以这里实现）
static int __cpeAux_http_auth(sessioncpe_obj_t *session)
{
    char buf2048[2048] = {0};
    char *strp;
    int ret;
    
    if(session == NULL)return -99;  //参数错误，-99代表致命错误？？
    //1、发送简单 http 信息给acs，看是否要认证
    strp = buf2048;
    httpmsg_client_say_hello(strp, 2048);
    http_client_send(session->client, buf2048, 2048);

    //2、等待可用msg
    pthread_mutex_lock(&(session->mutexMsg));
    session->msgReady = 0; 
    pthread_mutex_unlock(&(session->mutexMsg));
        
    
    ret = __cpeAux_wait_msg_timeout(session, 30);  //超时时间 x 秒
    if(ret != 0)
    {
        LOG_SHOW("没等到可用的 msg\n");
        return -1;  //没等到可用的 msg
    }

        
    //3、取出队列中的第一个 msg
    pthread_mutex_lock(&(session->mutexMsg));
    if(session->msgReady == 1)
    {
        LOG_INFO("接收到http消息 长度:%d 内容:\n【%s】\n", session->msgLen, session->msg);
    }
    else
    {
        LOG_INFO("接收到空的 http消息\n");
    }
    session->msgReady = 0;
    pthread_mutex_unlock(&(session->mutexMsg));

   
    return 0;   //能接收到 msg 信息，说明可以通过http认证
    

}

//2、http认证过程（因为http功能没有完善，所以这里实现）
static int __cpeAux_establish_connect(sessioncpe_obj_t *session)
{
    char buf64[64] = {0};
    char buf2048[2048 + 8] = {0};
    int pos = 0;
    int ret;
    int result;
    //char *strp;
    soap_node_t *nodeInformResponse;
    soap_node_t *soapNodeRoot;
    soap_node_t *nodeHeader;
    

    if(session == NULL )return -99;
    
 
    //1、构建并发送 Inform 消息【这里是测试】
    //1.1 添加基础内容
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "start",
        .idEn = 1,
        //.HoldRequests = "false",        //1.4 cwmp 版本弃用此元素，但是cpe还是要响应（兼容旧版本）
        //.holdRequestsEn = 1
    };

    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;
    soap_node_t *body = soap_node_get_son(root, "soap:Body");
    
    //1.2 添加rpc方法 Inform
    rpc_Inform_t dataInform;
    
    //1.2.1 Inform 的 DeviceId
    strcpy(dataInform.DeviceId.Manufacturer, "设备制造商的名称");
    strcpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符"); 
    //strncpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符", 6); //OUI只有6个字节
    strcpy(dataInform.DeviceId.ProductClass, "特定序列号的产品类别的标识符");
    strcpy(dataInform.DeviceId.SerialNumber, "特定设备的标识符");
    
    //1.2.2 Inform 的 Event、eventNum、MaxEnvelopes、 CurrentTime、 RetryCount
    
    dataInform.eventNum = 2;
    
    strcpy(dataInform.Event[0].EventCode, "0 BOOTSTRAP");
    strcpy(dataInform.Event[0].CommandKey, "CommandKey_0");
    strcpy(dataInform.Event[1].EventCode, "1 BOOT");
    strcpy(dataInform.Event[1].CommandKey, "CommandKey_1");
    
    dataInform.MaxEnvelopes = 1;
    cwmprpc_str2dateTime("2024-11-19T17:51:00", &(dataInform.CurrentTime));
    dataInform.RetryCount = 0;

    //1.2.3 Inform 的 ParameterList，链表成员类型为 ParameterValueStruct
    dataInform.ParameterList = link_create();
    ParameterValueStruct *informPvs1 = (ParameterValueStruct *)link_append_by_malloc
                                        (dataInform.ParameterList, sizeof(ParameterValueStruct));
    strcpy(informPvs1->Name, "parameter_1");
    strcpy(buf64, "value_1");
    informPvs1->Value = buf64;
    informPvs1->valueLen = strlen(informPvs1->Value);

    ParameterValueStruct *informPvs2 = (ParameterValueStruct *)link_append_by_malloc
                                        (dataInform.ParameterList, sizeof(ParameterValueStruct));
    strcpy(informPvs2->Name, "parameter_2");
    strcpy(buf64, "value_2");
    informPvs2->Value = buf64;
    informPvs2->valueLen = strlen(informPvs2->Value);
    
    soap_node_append_son(body, soapmsg_to_node_Inform(dataInform));

    link_destroy_and_data(dataInform.ParameterList);

    //1.3、生成soap信封，并通过http 接口发送给 acs   
    soap_node_to_str(soap->root, buf2048, &pos, 2048);
    
    http_client_send_msg(session->client, (void *)buf2048, strlen(buf2048), "POST", "/cwmp");
 
    //2、等待对方的响应  
    pthread_mutex_lock(&(session->mutexMsg));
    session->msgReady = 0; 
    pthread_mutex_unlock(&(session->mutexMsg));
    
    result = __cpeAux_wait_msg_timeout(session, 30); 
    if(result != 0)
    {
        LOG_SHOW("没等到可用的 msg\n");
        return -1;  //没等到可用的 msg
    }
    
    //3、取出队列第一个 msg，看是否匹配 InformResponse 消息
    ret = -99;    //未知错误
    pthread_mutex_lock(&(session->mutexMsg));
    if(session->msgLen > 0)
    {
        LOG_SHOW("接收到http msg消息 长度:%d 内容:\n【%s】\n", session->msgLen, session->msg);
        soapNodeRoot = soap_str2node(session->msg);
        //soap_node_show(soapNodeRoot);

        //soap 头部处理
        nodeHeader = soap_node_get_son(
                            soap_node_get_son(
                            soapNodeRoot, 
                            "soap:Envelope"), 
                            "soap:Header");
        result = __cpeAux_saop_header_pro(session, nodeHeader); //返回值暂不作处理
       
        // 找到 "cwmp:InformResponse" 节点
        nodeInformResponse = soap_node_get_son(
                            soap_node_get_son(
                            soap_node_get_son(
                            soapNodeRoot, 
                            "soap:Envelope"), 
                            "soap:Body"), 
                            "cwmp:InformResponse");
       if(nodeInformResponse != NULL)    
            ret = 0;     //找到了节点 "cwmp:InformResponse"
       else
            ret = -2;    //没找到节点 
       
       soap_destroy_node(soapNodeRoot);  //用完后需及时释放内存
    }
    else
    {
        LOG_SHOW("接收到空的http msg信息\n");
        ret = -3;   //空的msg
    }
    pthread_mutex_unlock(&(session->mutexMsg));    

    return ret;  
}

//3、发送
static int __cpeAux_send(sessioncpe_obj_t *session)
{
    int ret;
    char buf[SESSIONCPE_MSG_SIZE + 8];
    int tmp;
    if(session == NULL || session->requestQueue == NULL)return -99;

    if(session->sendStop == 1) 
    {
        LOG_SHOW("cpe 不再允许发送 请求");
        return 0;   
    }
    
    //cpe 获得主动权，则开始处理本地的请求队列
    //0、停止接收消息（或许不用）
    pthread_mutex_lock(&(session->mutexMsg));  
    session->msgReady = 1;
    pthread_mutex_unlock(&(session->mutexMsg));  
    
    //1、如果本地请求队列为空，那么发送 空http post报文；此时如果 HoldRequests 为 0
    //则cpe不可再发送请求，主动权交给 acs
    
    //pthread_mutex_lock(&(session->mutexRequestQueue));    //资源锁，如果有其他线程要访问requestQueue，则需要
    
    if(queue_isEmpty(session->requestQueue))
    {
        LOG_SHOW("请求队列为空\n");
        if(session->HoldRequests == 0)
        {
            session->sendStop = 1;     //如果 soap头部HoldRequests为false，那么cpe发送空http post后，
                                        //则不允许发送请求
        }
        ret = http_client_send_msg(session->client, NULL, 0, "POST", "/cwmp");
        
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
        
        sessioncpe_request_t *request = NULL;
        //sessioncpe_request_t *requestTmp = NULL;
        ret = sessioncpe_requestQueue_get_head(session, &request);
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
                
            sessioncpe_requestQueue_out(session, &request); //出队列并释放该请求
            sessioncpe_request_destroy(request);    
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
            soap_node_to_str(soap->root, buf, &tmp, SESSIONCPE_MSG_SIZE);
            
            
            link_remove_node_by_data_pointer(body->son, (void *)(request->node));  //先剥离request->node，再释放soap节点 
            soap_destroy(soap);     
            
            ret = http_client_send_msg(session->client, buf, tmp, "POST", "/cwmp");
            if(ret != RET_OK)
            {
                LOG_ALARM("报文发送失败");
                return -2;  //报文发送失败
            }
            return 2;   //成功发送请求
        }
        else
        {
            return -98; //获取request失败，致命错误
        }  
    }
    
    return -99;     //未知错误
}

//3、接收和处理
static int __cpeAux_recv(sessioncpe_obj_t *session)
{
    int ret;
    soap_node_t *nodeRoot;
    soap_node_t *nodeHeader;
    soap_node_t *nodeBody;
    int result;
    
    if(session == NULL)return -99;

    //1、超时接收新的信息
    pthread_mutex_lock(&(session->mutexMsg));  
    session->msgReady = 0;
    pthread_mutex_unlock(&(session->mutexMsg));
    result = __cpeAux_wait_msg_timeout(session, 30); //SESSIONCPE_RECV_TIMEOUT
    if(result != 0)
    {
        LOG_SHOW("没等到可用的 msg\n");
        return -1;  //没等到可用的 msg，超时
    }

    //2、把soap信封解析为soap节点，注意：这里只处理单个信封，不支持多个信封
    //可能要考虑处理多个信封，以便兼容旧版本
    ret = -5;    //未知错误
    pthread_mutex_lock(&(session->mutexMsg));
    if(session->msgLen > 0)
    { 
        LOG_SHOW("接收到http信息，长度：%d，内容：\n【%s】\n", session->msgLen, session->msg);
        nodeRoot = soap_str2node(session->msg);
        //soap_node_show(nodeRoot);
        //soap_node_show_print2log(nodeRoot);
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
                __cpeAux_saop_header_pro(session, nodeHeader); 
            }
            
            //3.2 处理 soap:Body 里面的信息，包括请求、响应和错误码
            nodeBody = soap_node_get_son(
                                    soap_node_get_son(
                                    nodeRoot, 
                                    "soap:Envelope"), 
                                    "soap:Body");
            if(nodeBody != NULL)
            {
                result = __cpeAux_saop_body_pro(session, nodeBody); 

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
        ret = -4;
    }
    
    pthread_mutex_unlock(&(session->mutexMsg));
    
    return ret;  //未知错误
}

/*==============================================================
                        开启会话
==============================================================*/
//会话线程
void *thread_sessioncpe(void *in)
{
    int ret;
    int outWhile;   
    //static int faildCnt;
    
    if(in == NULL)
    {
        LOG_ALARM("thread_sessioncpe: in is NULL");
        return NULL;
    }

    sessioncpe_obj_t *session = (sessioncpe_obj_t *)in;
    outWhile = 0;
    while(1)
    {
        switch(session->status)     //状态机
        {
            case sessioncpe_status_start:   //开始，需要确定http认证
            {
                LOG_SHOW("\n-------------------【cpe会话 start】---------------  \n");
                ret = __cpeAux_http_auth(session);
                LOG_SHOW("__cpeAux_http_auth ret:%d\n", ret);
                if(ret == 0)    //说明通过了一次握手
                {
                    //进入 connection 状态
                    session->status = sessioncpe_status_establish_connection;      
                }
                else 
                {   
                    if(ret == -1)
                    {
                        LOG_SHOW("__cpeAux_http_auth 等待超时\n");
                        session->status = sessioncpe_status_disconnect;  //进入error 状态
                    }
                    else if(ret == -99)
                    {
                        LOG_SHOW("__cpeAux_http_auth 故障\n");
                        session->status = sessioncpe_status_error;  //进入error 状态
                    }    
                }

            }break;
            case sessioncpe_status_establish_connection:
            {
                LOG_SHOW("\n-------------------【cpe会话 connection】---------------  \n"); 
                ret = __cpeAux_establish_connect(session);
                LOG_SHOW("__cpeAux_http_auth ret:%d\n", ret);
                if(ret == 0)
                {             
                    //进入 send 状态
                    session->status = sessioncpe_status_send;     
                }
                else 
                {
                    //-1：超时 -2：解析错误 -3：空msg
                    if(ret == -1 || ret == -2 || ret == -3 )  
                    {
                        session->status = sessioncpe_status_disconnect; 
                    }
                    else
                    {
                        session->status = sessioncpe_status_error; 
                    }   
                }
                
                //outWhile = 1;
            }break;             
            case sessioncpe_status_send:  //主动向 acs发送 空http post 报文
            {
                LOG_SHOW("\n-------------------【cpe会话 send】---------------  \n");
                ret = __cpeAux_send(session);
                LOG_SHOW("__cpeAux_send ret:%d\n", ret);

                //0：sendStop为1 1：没有请求 2：请求发送成功
                if(ret == 0 || ret == 1 || ret == 2)
                {
                    //进入 recv 状态
                    session->status = sessioncpe_status_recv;  
                }
                else 
                {   
                    //【-1】空报文发送失败 【-2】有msg报文发送失败【-3】请求队列无法获取第一个元素 
                    if(ret == -1 || ret == -2 || ret == -3)  
                    {
                        session->status = sessioncpe_status_disconnect;   
                    }
                    if(ret == -4)   //已激活的请求在下一次交流中，没有收到有效回复
                    {
                        ;   //无状态转移，表示重入此状态
                    }
                    //【-99】其他情况
                    else    
                    {
                        session->status = sessioncpe_status_error; 
                    }   
                }
                
                //outWhile = 1;
            }break; 
            case sessioncpe_status_recv:  //主动向 acs发送 空http post 报文
            {
                LOG_SHOW("\n-------------------【cpe会话 recv】---------------  \n");
                ret = __cpeAux_recv(session);
                LOG_SHOW("__cpeAux_recv ret:%d lastRecvRequestExist:%d\n", ret, 
                                                        session->lastRecvRequestExist);

                if(ret == 0)    //成功处理了soap信息
                {
                    if(session->lastRecvRequestExist == 0)  //如果上次没有请求，则cpe允许发送请求
                    {   
                        session->status = sessioncpe_status_send; 
                    }
                    else    //否则重入此状态
                    {
                        ;
                    }
                }
                //【-1】超时【-2】msg解析成soap节点失败【-3】msg中没有找到soap:Body 节点
                //【-4】空的msg信息
                else if(ret == -1 || ret == -2 || ret == -3 || ret == -4)  
                {
                    session->status = sessioncpe_status_disconnect; 
                }     
                else    //例如 -99
                {     
                    session->status = sessioncpe_status_error; 
                }
                
                //outWhile = 1;
            }break; 
            case sessioncpe_status_disconnect:
            {
                LOG_SHOW("\n-------------------【cpe会话 disconnect】---------------  \n");
                outWhile = 1;
            }break;
            case sessioncpe_status_end:
            {
                LOG_SHOW("\n-------------------【cpe会话 end】---------------  \n");
                outWhile = 1;
            }break;
            case sessioncpe_status_error:   //某种故障
            {
                LOG_SHOW("\n-------------------【cpe会话 error】---------------  \n");
                outWhile = 1;
            }break;
            default:    //离开循环
            {
                
                outWhile = 1;
            }break;

        } 

        //system("sleep 2");
        if(outWhile)
        {
            LOG_SHOW("【cpe会话】离开循环 ......\n");
            break;
        }
    }

    return NULL;
}


//消息接收线程
void *thread_sessioncpe_recv(void *in)
{
    if(in == NULL)
    {
        LOG_ALARM("thread_http_client_recv in is  NULL");
        return NULL;
    }
    sessioncpe_obj_t *session = (sessioncpe_obj_t *)in;
    http_client_t *client = session->client;

    while(1)
    {

        sem_wait(&(client->semHttpMsgReady));  

        
        http_payload_t *payload = &(client->parse.payload);

        client->retHttpMsg = -1;  //无动作

        pthread_mutex_lock(&(session->mutexMsg));
        if(session->msgReady == 0)  //说明msg被处理完了，现在可以接收新的消息
        {
            memset(session->msg, '\0', SESSIONCPE_MSG_SIZE);
            if(payload->len < 0)    //载荷可能为空
            {   
                session->msgLen = 0;           
            }
            else
            {
                memcpy(session->msg, (void *)(payload->buf), payload->len);
                session->msgLen = payload->len;
            }
            session->msgReady = 1;
            
            pthread_cond_signal(&(session->condMsgReady)); //发送条件变量
            client->retHttpMsg = 0;
        }
        else
        {
            client->retHttpMsg = -2;   //返回信息：msg 还未被处理
        }

        pthread_mutex_unlock(&(session->mutexMsg));
        
        sem_post(&(client->semHttpMsgRecvComplete));  //完成的信号
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
    ret = pthread_create(&(session->threadRecv), NULL, thread_sessioncpe_recv, (void *)session);
    if(ret != RET_OK){LOG_ALARM("thread_sessioncpe_recv");return ret;}

 
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

    /*---------------------------------------------------------------------*/
    //测试，为cpe添加请求到队列
    sessioncpe_request_t *requestTmp;
    //soap_node_t *node;
    if(session->requestQueue == NULL)
    {
        LOG_ALARM("session->requestQueue is NULL");
    }
    else
    {
        //1、清空 请求队列
        sessioncpe_requestQueue_clear(session);
        //2、请求队列增加成员，然后设置
        requestTmp = sessioncpe_requestQueue_in_new(session);  
        if(requestTmp == NULL)
        {
            LOG_ALARM("requestTmp is NULL\n");
        }
        else
        {
            rpc_GetRPCMethods_t dataGetRPCMethods ={
                .null = NULL      
            };
            requestTmp->node = soapmsg_to_node_GetRPCMethods(dataGetRPCMethods);
            requestTmp->nodeEn = 1;
            requestTmp->active = 0; //还没有被激活
         }
    }         
    LOG_SHOW("当前请求列表的个数：%d\n", queue_get_num(session->requestQueue));
    /*---------------------------------------------------------------------*/
    
    sessioncpe_start(session, ipv4, port, targetIpv4, targetPort);

    while(1)
    {
        ;
    }
    
}












