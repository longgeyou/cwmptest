




#include <stdio.h>
#include <stdlib.h>

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
//消息队列的数据元，用于存储 soap 信封
typedef struct sessioncpe_msgdata_t{
    void *d;    //数据指针
    int len;    //长度
}sessioncpe_msgdata_t;

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
    pthread_mutex_t mutexMsgQueue; //互斥锁，保护msgQueue资源的互斥锁
    int msgRecvEn;  //控制消息队列的接收行为，1表示可以接收，0表示不允许
    pthread_cond_t condMsgQueueReady;   //条件变量，表示有消息
      
    
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
/*-------------------------------------------------msg 数据元 */     
//参考 sessionacs
//创建 msg 数据元
sessioncpe_msgdata_t *sessioncpe_create_msg(int size)
{
    if(size < 0)return NULL;
    sessioncpe_msgdata_t *msgdata = (sessioncpe_msgdata_t *)MALLOC(sizeof(sessioncpe_msgdata_t));
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
sessioncpe_msgdata_t *sessioncpe_create_msg_set_data(void *data, int len)
{
    if(len < 0 || data == NULL)return NULL;
    sessioncpe_msgdata_t *msgdata = (sessioncpe_msgdata_t *)MALLOC(sizeof(sessioncpe_msgdata_t));
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

//摧毁 essioncpe_msgdata_t ，以及指向的数据
void sessioncpe_destroy_msg(sessioncpe_msgdata_t *msg)
{
    if(msg == NULL)return ;

    if(msg->d != NULL)FREE(msg->d);

    FREE(msg);
}


/*-------------------------------------------------msg 数据元 */



#define SESSION_CPE_MSG_QUEUE_SIZE 10
//创建会话 
sessioncpe_obj_t *sessioncpe_create()
{
    sessioncpe_obj_t *session = (sessioncpe_obj_t *)MALLOC(sizeof(sessioncpe_obj_t));

    if(session == NULL)return NULL;

    session->status = sessioncpe_status_start;
    session->cwmpid = 0;

    //soap 信息队列
    session->msgQueue = queue_create(SESSION_CPE_MSG_QUEUE_SIZE);
    pthread_mutex_init(&(session->mutexMsgQueue), NULL);   //初始化互斥锁
    session->msgRecvEn = 0;  //默认不允许信息进入队列
    pthread_cond_init(&(session->condMsgQueueReady), NULL);  //准备好信息的条件变量
    
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
    queue_destroy_and_data(session->msgQueue);
    
    //释放自己
    FREE(session);
}



/*==============================================================
                        应用
==============================================================*/
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
            sessioncpe_msgdata_t *data = (sessioncpe_msgdata_t *)(probe->d);

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
//1、认证进度
int __aux_http_auth(sessioncpe_obj_t *session)
{
    char buf2048[2048] = {0};
    sessioncpe_msgdata_t *msgData;
    char *strp;
    int ret;
    
    if(session == NULL)return -1;
    //发送简单 http 信息给acs，看是否要认证
    char *p = buf2048;
    httpmsg_client_say_hello(p, 2048);
    http_client_send(session->client, buf2048, 2048);


    ret = -1;
    //等待msgQueue有可用信息 ，需要条件变量   
    pthread_mutex_lock(&(session->mutexMsgQueue));
    while (queue_isEmpty(session->msgQueue)) {
        LOG_SHOW("等待消息队列非空...\n");
        
        // 阻塞等待条件变量
        pthread_cond_wait(&(session->condMsgQueueReady), &(session->mutexMsgQueue)); 
    }
    LOG_SHOW("开始处理消息队列表...\n");
    //sessioncpe_show_msgQUeue(session);

    //取出第一个 msg
    if(!queue_isEmpty(session->msgQueue))
    {
        queue_out_get_pointer(session->msgQueue, (void**)(&msgData));    
        strp = (char *)(msgData->d);
        strp[msgData->len] = '\0';
        LOG_SHOW("msgData len:%d msg:\n【%s】\n", msgData->len, strp);
        
        ret = 0;
    }
    
    pthread_mutex_unlock(&(session->mutexMsgQueue));

   
    return ret;
}

//2、建立连接（默认是cpe主动发起连接请求）
static int __aux_establish_connection(sessioncpe_obj_t *session)
{
    if(session == NULL)return RET_FAILD;

    char buf64[64] = {0};
    char buf2048[2048 + 8] = {0};
    int pos = 0;
    int ret;
    sessioncpe_msgdata_t *msgData;
    char *strp;
    soap_node_t *nodeInformResponse;
    soap_node_t *soapNodeRoot;
    int InformResponseExit;

    ret = RET_FAILD;
    
    //1、清空msg queue 并禁止接收消息
    pthread_mutex_lock(&(session->mutexMsgQueue));
    session->msgRecvEn = 0;
    QUEUE_FOEWACH_START(session->msgQueue, probe){  //清除数据元
        if(probe->en == 1 && probe->d != NULL)
            sessioncpe_destroy_msg((sessioncpe_msgdata_t *)probe->d);
        
    }QUEUE_FOEWACH_END;
    
    queue_clear(session->msgQueue); //清空队列
    pthread_mutex_unlock(&(session->mutexMsgQueue));

    
    //2、构建并发送 Inform 消息
    //2.1 添加基础内容
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "long123",
        .idEn = 1,
        .HoldRequests = 1,
        .holdRequestsEn = 1
    };
    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;

    soap_node_t *body = soap_node_get_son(root, "soap:Body");
    
    //2.2 添加rpc方法 Inform
    rpc_Inform_t dataInform;
    
    //2.2.1 Inform 的 DeviceId
    strcpy(dataInform.DeviceId.Manufacturer, "设备制造商的名称");
    strcpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符"); 
    //strncpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符", 6); //OUI只有6个字节
    strcpy(dataInform.DeviceId.ProductClass, "特定序列号的产品类别的标识符");
    strcpy(dataInform.DeviceId.SerialNumber, "特定设备的标识符");
    
    //2.2.2 Inform 的 Event、eventNum、MaxEnvelopes、 CurrentTime、 RetryCount
    
    dataInform.eventNum = 2;
    
    strcpy(dataInform.Event[0].EventCode, "0 BOOTSTRAP");
    strcpy(dataInform.Event[0].CommandKey, "CommandKey_0");
    strcpy(dataInform.Event[1].EventCode, "1 BOOT");
    strcpy(dataInform.Event[1].CommandKey, "CommandKey_1");
    
    dataInform.MaxEnvelopes = 1;
    cwmprpc_str2dateTime("2024-11-19T17:51:00", &(dataInform.CurrentTime));
    dataInform.RetryCount = 0;

    //2.2.3 Inform 的 ParameterList，链表成员类型为 ParameterValueStruct
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

    //2.3、生成soap信封，并通过http 接口发送给 acs   
    soap_node_to_str(soap->root, buf2048, &pos, 2048);
    
    http_client_send_msg(session->client, (void *)buf2048, strlen(buf2048), "POST", "/cwmp");

    
    
    //3、开启msgQueue接收并等待可用信息 ，需要条件变量   
    pthread_mutex_lock(&(session->mutexMsgQueue));
    session->msgRecvEn = 1;
    while (queue_isEmpty(session->msgQueue)) {
        LOG_SHOW("等待消息队列非空...\n");
        
        // 阻塞等待条件变量
        pthread_cond_wait(&(session->condMsgQueueReady), &(session->mutexMsgQueue)); 
    }
    LOG_SHOW("开始处理消息队列表...\n");
    //sessioncpe_show_msgQUeue(session);

    //4、逐个取出队列的 msg，直到找到 InformResponse 消息
    InformResponseExit = 0;
    while(!queue_isEmpty(session->msgQueue))
    {
        queue_out_get_pointer(session->msgQueue, (void**)(&msgData));    
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
            nodeInformResponse = soap_node_get_son(
                                soap_node_get_son(
                                soap_node_get_son(
                                soapNodeRoot, 
                                "soap:Envelope"), 
                                "soap:Body"), 
                                "cwmp:InformResponse");
           if(nodeInformResponse != NULL)
           {
                InformResponseExit = 1;
                break;
           }

        }
        else
        {
            LOG_SHOW("-->msgData len:%d msg:\n【空】\n", msgData->len);
        }
        
        
    }

    pthread_mutex_unlock(&(session->mutexMsgQueue));

    if(InformResponseExit)
    {
        ret = RET_OK;   //收到了来自 acs 的 InformResponse，简单认为连接已经建立，后面应该要修改
    }
    
    return ret;
}




//会话线程
void *thread_sessioncpe(void *in)
{
    int ret;
    int outWhile;
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
                session->msgRecvEn = 1; //允许消息队列存入数据
                ret = __aux_http_auth(session);
                LOG_SHOW("sessioncpe_status_start ret:%d\n", ret);
                if(ret == 0)    //说明通过了一次握手
                {
                    session->status = sessioncpe_status_establish_connection;   //状态转移   
                }

            }break;
            case sessioncpe_status_establish_connection:
            {
                LOG_SHOW("sessioncpe_status_establish_connection 来了......\n");
                ret = __aux_establish_connection(session);

                if(RET_OK == ret)
                {
                    //session->status = sessioncpe_status_send;
                }
               
                
            
                outWhile = 1;
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
        if(outWhile)break;
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

        client->retHttpMsg = -2;  //无动作

        pthread_mutex_lock(&(session->mutexMsgQueue));
        if(session->msgRecvEn == 1)  //允许接收
        {
           
            if(queue_isFull(session->msgQueue))
            {
                client->retHttpMsg = -1;  
                
            }
            else
            {
                queue_in_set_pointer(session->msgQueue, 
                    (void *)sessioncpe_create_msg_set_data((void *)(payload->buf), payload->len));            
                client->retHttpMsg = 0;              
            }
            
        }
        else
        {
            client->retHttpMsg = -3;   //返回信息：msgQueue 不允许增加信息
        }

        if(!queue_isEmpty(session->msgQueue))
        {
            LOG_SHOW("开始发送条件变量....\n");
            pthread_cond_signal(&(session->condMsgQueueReady)); //发送条件变量
        }
            
        
        pthread_mutex_unlock(&(session->mutexMsgQueue));
        
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
    sessioncpe_start(session, ipv4, port, targetIpv4, targetPort);

    while(1)
    {
        ;
    }
    
}








