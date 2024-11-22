


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



/*==============================================================
                        数据结构
==============================================================*/
//消息队列的数据元，用于存储 soap 信封
typedef struct sessionacs_msgdata_t{
    void *d;    //数据指针
    int len;    //长度
}sessionacs_msgdata_t;

//会话状态机
typedef enum sessionacs_status_e{
    sessionacs_status_start,    //开始
    sessionacs_status_establish_connection,     //建立连接
    sessionacs_status_send,     //发送状态
    sessionacs_status_wait_recv,    //等待接收状态
    sessionacs_status_disconnect,   //会话完成，请求断开状态
    sessionacs_status_end   //结束
}sessionacs_status_e;

//acs 会话对象的 子成员
#define SESSION_ACS_MEMBER_MSG_QUEUE_SIZE 10
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

    int outWhile = 0;
    sessionacs_msgdata_t *msgData;
    //soap_obj_t *soap; 
    soap_node_t *soapNodeRoot;
    //soap_node_t *soapNodeEnvelope;
    //soap_node_t *soapNodeBody;
    soap_node_t *soapNodeInform;
    char *strp;
    int informExit;
    char buf1024[1024 + 8] = {0};
    
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
                //等待msgQueue有可用信息 ，需要条件变量
                   
                pthread_mutex_lock(&(member->mutexMsgQueue));
                while (queue_isEmpty(member->msgQueue)) {
                    LOG_SHOW("等待消息队列非空...\n");
                    
                    // 阻塞等待条件变量
                    pthread_cond_wait(&(member->condMsgQueueReady), &(member->mutexMsgQueue)); 
                }
                LOG_SHOW("开始处理消息队列表...\n");
                //sessionacs_show_msgQUeue(member);

                while(!queue_isEmpty(member->msgQueue))
                {                     
                    queue_out_get_pointer(member->msgQueue, (void**)(&msgData));    
                    strp = (char *)(msgData->d);
                    strp[msgData->len] = '\0';
                    LOG_SHOW("-->msgData len:%d msg:\n【%s】\n", msgData->len, strp);


                    //开始解析成 soap 节点，这里简单规定，第一个字节点应该是 "soap:Envelope"
                    //里面包含的 Inform 方法 用于acs的初始化连接；其他的话不处理，
                    //一律回复 HTTP/1.1 200 OK；后面需要优化；对于htpp 的错误码响应的实现，但
                    //是要重构 http ，以后再说吧
                    soapNodeRoot = soap_str2node(strp);
                    soap_node_show(soapNodeRoot);

                    informExit = 0;
                    //这里只考虑 soap消息的第一个 soap 信封（1.4 新版本只支持一个信封， 老版本
                    //则需要协商个数）
                    soapNodeInform = soap_node_get_son(
                                        soap_node_get_son(
                                        soap_node_get_son(
                                        soapNodeRoot, 
                                        "soap:Envelope"), 
                                        "soap:Body"), 
                                        "soap:Envelope");
                   if(soapNodeInform != NULL)
                   {
                        informExit = 1;
                        break;
                   }
 
                }
                
                pthread_mutex_unlock(&(member->mutexMsgQueue));
                
                if(informExit == 0) //发送回复
                {
                    httpmsg_server_response_hello(buf1024, 1024);
                    LOG_SHOW("开始发送 http 的回复信息\n");
                    httpUser_send_str(member->session->http, member->user, buf1024);
                }
                else
                {
                    if(member->user->id >= 0 && member->user->id < HTTP_USER_NUM)
                    {
                        tcpUser_obj_t *tcpUser = &(member->session->http->tcp[member->user->id]);
                        LOG_SHOW("收到来自 %s:%d 的 Inform 消息，开始握手.....\n", tcpUser->ipv4, tcpUser->port);

                    }
                    
                    
                }

               
                //outWhile = 1;
           }break;
           case sessionacs_status_send:{


           }break;
           case sessionacs_status_wait_recv:{


           }break;
           case sessionacs_status_disconnect:{


           }break;
           case sessionacs_status_end:{


           }break;
           default:{    //其他的情况
    
                LOG_ALARM("thread_sessionacs switch default ......");
           }break;
         
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








