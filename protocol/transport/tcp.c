﻿ 


//设想：本地管理（静态结构体变量），然后通过某个接口，汇总起来，
//把管理权限给到一个“超级管理员”…………

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "tcp.h"





/*============================================================
                        本地管理
==============================================================*/
typedef struct tcp_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    //tcp_server_t *tcp[];
    int tcpServerCnt;
    int tcpClientCnt;

    int initCode; 
}tcp_manager_t;

tcp_manager_t tcp_local_mg = {0};

//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(tcp_local_mg.poolId, x)
#define POOL_FREE(x) pool_user_free(x)
#define TCP_POOL_NAME "tcp"


//初始化
void tcp_init()
{
    if(tcp_local_mg.initCode == TCP_INIT_CODE)return ;  //一次调用
 
    strncpy(tcp_local_mg.poolName, TCP_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    tcp_local_mg.poolId = pool_apply_user_id(tcp_local_mg.poolName);

    tcp_local_mg.tcpServerCnt = 0;
    tcp_local_mg.tcpClientCnt = 0;

    pool_init();    //依赖 pool2.c
    //log_init();     //依赖 log.c
    ssl_mg_init();      //使用 ssl.c

    ssl_init();     //ssl初始化？？

    tcp_local_mg.initCode = TCP_INIT_CODE;
}



/*============================================================
                            user列表
==============================================================*/


//获取可用用户 index
int tcpUser_get_id(tcp_server_t *tcp)
{
    int i;
    for(i = 0; i < TCP_USER_NUM; i++)
    {
        if(tcp->user[i].used == 0)
        {
            tcp->user[i].used = 1;
            return i;
        }
    }
    return -1;  //调用者，可能会导致数组溢出
}

//新增成员
int tcpUser_add_member(tcp_server_t *tcp, char *ipv4, int port, int fd)
{
    int id = tcpUser_get_id(tcp);
    if(id >= 0)
    {
        strncpy(tcp->user[id].ipv4, ipv4, TCP_IPV4_NAME_MAX_LEN);
        tcp->user[id].port = port;
        tcp->user[id].used = 1;
        tcp->user[id].fd = fd;
        tcp->userCnt++;

        // 初始化自旋锁
        //pthread_spin_init(&(tcp->user[id].spinlock), 0); 

        //初始化互斥锁
        //tcp->user[id].mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&(tcp->user[id].mutex), NULL);

        
        
    }
    else
    {
        LOG_ALARM("[%s:%d] user full", ipv4, port);
    }

    return id;
}

//用户列表管理-释放
void tcpUser_release(tcp_server_t *tcp, int id)
{
    if(id >= 0 && id < TCP_USER_NUM)
    {
        tcp->user[id].used = 0;
        tcp->userCnt--;

        pthread_mutex_destroy(&(tcp->user[id].mutex));     
    }
}

//用户列表管理-获取用户数量
int tcpUser_cnt_get(tcp_server_t *tcp)
{
    int i, ret;
    ret = 0;
    for(i = 0; i < TCP_USER_NUM; i++)
    {
        if(tcp->user[i].used == 1)
        {
            ret++;
        }
    }
    return ret;
}

//用户列表管理-匹配
int tcpUser_match_fd(tcp_server_t *tcp, int fd)
{
    int i;
    for(i = 0; i < TCP_USER_NUM; i++)
    {
        if(tcp->user[i].fd == fd)
        {
            return i;
        }
    }
    return -1;
}




/*============================================================
                            对象
==============================================================*/

//tcp服务器对象创建
tcp_server_t *tcp_server_create(char *ipv4, int port)
{
    int i;
    
    tcp_server_t *tcp = (tcp_server_t *)POOL_MALLOC(sizeof(tcp_server_t));
    if(ipv4 != NULL)strncpy(tcp->ipv4, ipv4, TCP_IPV4_NAME_MAX_LEN);
    tcp->port = port;
    

    for(i = 0; i < TCP_USER_NUM; i++)
    {
        tcp->user[i].used = 0;
        tcp->user[i].fd = -1;
        tcp->user[i].disconnect = 0;    //失去连接 指示
    }
    //tcp->userCnt = 0;

    //ssl设置
    ssl_obj_t *serverSsl = ssl_create();
    ssl_server_config(serverSsl, SSL_CERT_FILE_PATH, SSL_KEY_FILE_PATH);
    tcp->ssl = serverSsl; 

    //初始化 互斥锁和条件变量
    //tcp->mutex = PTHREAD_MUTEX_INITIALIZER;
    //tcp->cond = PTHREAD_COND_INITIALIZER;

    //初始化信号量
    sem_init(&(tcp->semBufReady), 0, 0);
    sem_init(&(tcp->semUserConnect), 0, 0);

    
    tcp_local_mg.tcpServerCnt++;
    return tcp;
}

//tcp服务器对象摧毁
void tcp_server_destory(tcp_server_t *tcp)
{
    
    if(tcp != NULL)
    {
        tcp_local_mg.tcpServerCnt--;

        //1、成员释放
        
        //1.1 ssl对象释放
        ssl_destory(tcp->ssl);

        //1.2 互斥锁和条件变量
        //pthread_mutex_destroy(tcp->mutex);
        //pthread_cond_destroy(tcp->cond);
        
        //信号量
        sem_destroy(&(tcp->semBufReady));
        

        //1.3 释放线程 用户握手线程accept、用户数据收发线程等
        
        
        //1.4 关闭 服务器socket、关闭用户的连接
        
        
        //3、释放tcp对象
        POOL_FREE(tcp);

    }
}


//服务器客户端创建
tcp_client_t *tcp_client_create(char *ipv4, int port)
{
    //int i;
    
    tcp_client_t *client = (tcp_client_t *)POOL_MALLOC(sizeof(tcp_client_t));
    if(ipv4 != NULL)strncpy(client->ipv4, ipv4, TCP_IPV4_NAME_MAX_LEN);
    client->port = port;

     //ssl 的使用
    ssl_obj_t *clientSsl = ssl_create();
    ssl_client_config(clientSsl);
    client->ssl = clientSsl;

    //用于数据接收
    sem_init(&(client->semBufReady), 0, 0); //初始化信号量
    pthread_mutex_init(&(client->mutex), NULL);   //初始化互斥锁

    
    
    tcp_local_mg.tcpClientCnt++;
    return client;
}

//服务器客户端摧毁
void tcp_client_destory(tcp_client_t * client)
{
    tcp_local_mg.tcpClientCnt--;

    //if(client == NULL)return ;
    //成员释放
    if(client->ssl != NULL)ssl_destory(client->ssl);
    
    POOL_FREE(client);
}

/*============================================================
                            开启的线程
==============================================================*/

//接受线程
void *thread_tcp_accept(void *in)
{
    if(in == NULL){LOG_ALARM("thread accept");return NULL;}
    tcp_server_t *tcp = (tcp_server_t *)in;

    int fd, port, ret;
    //int userId;
    char ipv4[TCP_IPV4_NAME_MAX_LEN] = {0};

    
    //tcp->user[userId].used = 
    while(1)
    {
        fd = LINUX_itf_accept(tcp->fd, (char *)ipv4, TCP_IPV4_NAME_MAX_LEN, &port);

        //使用ssl
        tcp->ssl->set_fd(tcp->ssl->ssl, fd);    

        
        if(fd != -1)//代表成功
        {
            /*while(1)    //申请用户列表资源
            {
                userId = tcpUser_get_id(tcp);
                if(userId >= 0)
                {
                    strncpy(tcp->user[userId].ipv4, ipv4, TCP_IPV4_NAME_MAX_LEN);
                    tcp->user[userId].port = port;
                    tcp->user[userId].used = 1;
                    tcp->user[userId].fd = fd;
                    tcp->userCnt++;
                    
                    break;
                }  
                //ret = tcpUser_add_member(tcp, ipv4, port, fd);
                //if(ret >= 0)break;
                
                LOG_ALARM("userId");
                system("sleep 3");    
            }  */

            ret = tcpUser_add_member(tcp, ipv4, port, fd);
            
            if(ret < 0)
            {
                LOG_ALARM("apply user list faild\n");
            }   
            else
            {
                tcp->newUserId = ret;
            
                sem_post(&(tcp->semUserConnect));   //发送信号量
            }
            
        }
        else
        {
            LOG_ALARM("accept");
            system("sleep 3");
        }
    }
    
    return NULL;                           
}


//收发器线程
void *thread_tcp_transceiver(void *in)
{
//#define READ_BLOCK_SIZE 24   //每次读取的字节数，一般设计得比用户读取数据的缓存小
    if(in == NULL){LOG_ALARM("in");return NULL;}

    int i, ret;
    //char buf[READ_BLOCK_SIZE + 8] = {0};    // + 8 是为了 冗余
    //int len;

    
    tcp_server_t *tcp = (tcp_server_t *)in;
    if(tcp == NULL){LOG_ALARM("tcp");return NULL;}
    tcpUser_obj_t *user_p;

    //使用 poll 方法来并发处理用户的数据收发
    struct pollfd fds[TCP_USER_NUM] = {0};
   
    while (1) 
    {
        //将有效用户的文件描述符，添加到 集合中
        for(i = 0; i < TCP_USER_NUM; i++)
        {
            if(tcp->user[i].used == 1 && user_p->disconnect == 0)
            {
                fds[i].fd = tcp->user[i].fd;
                fds[i].events = POLLIN;
            }
            else
            {
                memset(&(fds[i]), 0, sizeof(struct pollfd));    //这一步可能有问题
            }
        }
        
        ret = poll(fds, TCP_USER_NUM, 2000); // 阻塞直到有事件发生或超时
        if (ret == -1) 
        {
            LOG_ALARM("poll");
            system("sleep 3");
            continue;
        }


        //读取的长度len大于0，说明有数据可读；等于0，代表断开连接，小于0，没有数据
        for(i = 0; i < TCP_USER_NUM; i++)
        {
            user_p = &(tcp->user[i]);

            if ((user_p->used == 1) && 
                    user_p->disconnect == 0 && 
                    (fds[i].revents & POLLIN))  //有数据进入
            {
                //memset(buf, '\0', strlen(buf));                 
                //len = read(fds[i].fd, buf, READ_BLOCK_SIZE); 

                //LOG_SHOW("---->POLL    IN  ready:%d\n", user_p->bufReady);
            
                pthread_mutex_lock(&(user_p->mutex));   //该锁用于保护 tcp user 缓存相关的资源
                
                if(user_p->bufReady == 1)
                {
                    pthread_mutex_unlock(&(user_p->mutex));
                    continue;
                }
                
                memset(user_p->buf, '\0', TCP_USER_BUF_SIZE + 1);   // + 1 位 
                user_p->bufLen = read(fds[i].fd, user_p->buf, TCP_USER_BUF_SIZE); 
                
                pthread_mutex_unlock(&(user_p->mutex));
                
                if(user_p->bufLen == 0)    
                {
                    if(user_p->disconnect != 1)
                    {
                        LOG_INFO("i=%d [%s-%d] 断开连接\n", i, user_p->ipv4, user_p->port);
                        user_p->disconnect = 1;     //断开连接标记
                    }
                    
                    
                    //tcpUser_release(tcp, i);
                }                
                else if(user_p->bufLen > 0)
                {                    
//                     LOG_SHOW("\n【用户id：%d ip地址：%s:%d 读取的字节：%dbyte】\n"
//                               "------------------------------------\n"
//                               "%s\n------------------------------------\n\n",
//                               i, user_p->ipv4, user_p->port, user_p->bufLen, user_p->buf); 
                               
                     pthread_mutex_lock(&(user_p->mutex));          
                     user_p->bufReady = 1;
                     //LOG_SHOW("   锁 里面 ---------------------\n");
                     pthread_mutex_unlock(&(user_p->mutex));
                     //LOG_SHOW("   锁 外面 ---------------------\n");
                     
                     sem_post(&(tcp->semBufReady)); //发出信号量

                     //LOG_SHOW("   激活信号量 ---------------------\n");
                }
  
            }  
        }
        
    }

    return NULL;
}


//客户端的收发器线程
void *thread_tcp_client_transceiver(void *in)
{
    if(in == NULL){LOG_ALARM("in");return NULL;}

    tcp_client_t *client = (tcp_client_t *)in;
    if(client == NULL){LOG_ALARM("client");return NULL;}    

    int ret;
//#define BUF_SIZE 512        
    //char buf[BUF_SIZE] = {0};  
    //int len;

#define POLL_SIZE 1    
    struct pollfd fds[POLL_SIZE] = {0};  //使用 poll 方法来并发处理用户的数据收发
    fds[0].fd = client->fd;
    fds[0].events = POLLIN;
    
    while (1) 
    {
       
        ret = poll(fds, POLL_SIZE, 2000); // 阻塞直到有事件发生或超时
        if (ret == -1) 
        {
            LOG_ALARM("客户端 poll 失败");
            system("sleep 3");
            continue;
        }

        //读取的长度len大于0，说明有数据可读；等于0，代表断开连接，小于0，没有数据        
        if (fds[0].revents & POLLIN)  //有数据进入
        {
            //LOG_SHOW("---------------->POLL   IN\n");
                
            pthread_mutex_lock(&(client->mutex));   //该锁用于保护 tcp user 缓存相关的资源
            
            if(client->bufReady == 1)
            {
                pthread_mutex_unlock(&(client->mutex));
                continue;
            }
            
            memset(client->buf, '\0', TCP_USER_BUF_SIZE + 1);   // + 1 位 
            client->bufLen = read(client->fd, client->buf, TCP_USER_BUF_SIZE); 
            
            pthread_mutex_unlock(&(client->mutex));
           
            if(client->bufLen == 0)    
            {
                LOG_INFO("断开连接 ..."); 
                close(client->fd);
                return NULL;
            }                
            else if(client->bufLen > 0)
            {                    
//                 LOG_SHOW("\n【客户端 ip地址：%s:%d 读取的字节：%dbyte】\n"
//                           "------------------------------------\n"
//                           "%s\n------------------------------------\n\n",
//                           client->ipv4, client->port, client->bufLen, client->buf);  
                           
                pthread_mutex_lock(&(client->mutex));          
                client->bufReady = 1;
                pthread_mutex_unlock(&(client->mutex));

                sem_post(&(client->semBufReady)); //发出信号量                
            }   
        }

    }

    return NULL;
}



/*============================================================
                            应用
==============================================================*/

//开启tcp服务器
int tcp_server_start(tcp_server_t *tcp)
{
    int ret;
    //绑定
    ret = LINUX_itf_bind(tcp->ipv4, tcp->port, &(tcp->fd));
    if(ret < 0){LOG_ALARM("bind faild"); return ret;};
    
    //监听
    ret = LINUX_itf_listen(tcp->fd);
    if(ret < 0){LOG_ALARM("listen ..."); return ret;};
    
    //等待连接（开启线程）
    /*int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);*/
    ret = pthread_create(&(tcp->accept), NULL, thread_tcp_accept, (void *)tcp);
    if(ret != 0){LOG_ERROR("thread_tcp_accept error"); return ret;};

    //开启 数据接收线程
    ret = pthread_create(&(tcp->transceiver), NULL, thread_tcp_transceiver, (void *)tcp);
    if(ret != 0){LOG_ERROR("thread_tcp_transceiver error"); return ret;};

    return 0;
}



//开启tcp客户端
int tcp_client_start(tcp_client_t *client, char *ipv4, int port)
{
    int ret;
    if(client == NULL){LOG_ERROR("clinet NULL"); return -1;};
    if(ipv4 == NULL){LOG_ALARM("clinet ipv4"); return -1;};
    
    //绑定
    ret = LINUX_itf_bind(client->ipv4, client->port, &(client->fd));
    if(ret < 0){LOG_ALARM("clinet bind"); return ret;};
    
    //连接
    client->ssl->set_fd(client->ssl->ssl, client->fd);  //ssl的使用   
    ret = LINUX_itf_connect(client->fd, ipv4, port);
    if(ret < 0){LOG_ALARM("clinet connect"); return ret;};

    LOG_INFO("connect to [%s:%d] OK", ipv4, port);

    //开启 数据接收线程
    ret = pthread_create(&(client->transceiver), NULL, thread_tcp_client_transceiver, (void *)client);
    if(ret != 0){LOG_ERROR("thread_tcp_client_transceiver error"); return ret;};
    
    return 0;
}

//读写数据
/*-----------------------------------------------------------
#include <unistd.h>

ssize_t write(int fd, const void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count)


返回值
成功：读取/写入的字节数
失败：-1
-----------------------------------------------------------*/
int tcp_server_send(tcp_server_t *tcp, int userId, char *msg, int size)
{
    if(userId < 0 || userId > TCP_USER_NUM)
    {
        LOG_ALARM("userId");
        return -1;
    }
    return write(tcp->user[userId].fd, msg, size);
}

int tcp_server_read(tcp_server_t *tcp, int userId, char *content, int size) //访问可能要考虑  资源竞争和互斥锁
{
    if(userId < 0 || userId > TCP_USER_NUM)
    {
        LOG_ALARM("userId");
        return -1;
    }
    return read(tcp->user[userId].fd, content, size);
}

int tcpUser_send(tcpUser_obj_t *user, void *data, int dataLen)
{
    if(user < 0 || data == NULL || dataLen <= 0)
    {
        LOG_ALARM("param");
        return -1;
    }
    return write(user->fd, data, dataLen);
}

int tcp_client_send(tcp_client_t *cl, char *msg, int size)  //这里有待修改，msg是字符串的话，长度参数可以不用
{
    return write(cl->fd, (const void *)msg, size);
}

int tcp_client_send_data(tcp_client_t *cl, void *data, int size)  
{
    return write(cl->fd, (const void *)data, size);
}


//接收数据
int tcp_client_read(tcp_client_t *cl, char *content, int size)
{
    return read(cl->fd, (void *)content, size);
}


/*============================================================
                            测试
==============================================================*/

//测试
/*
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
*/
void tcp_test(char *ipv4, int port)
{
    int i, ret;
    //int userId;
    char buf512[512] = {0};
    int len;
    //int firstMark;

    tcp_init();

    tcp_server_t *tcp = tcp_server_create(ipv4, port);

    if(0 != tcp_server_start(tcp)){
        LOG_ALARM("tcp_server_start 失败！");
        return;
    }
    
    struct pollfd fds[TCP_USER_NUM] = {0};
    
    pool_show();


    while (1) 
    {
        for(i = 0; i < TCP_USER_NUM; i++)
        {
            if(tcp->user[i].used == 1)
            {
                fds[i].fd = tcp->user[i].fd;
                fds[i].events = POLLIN;
            }
            else
            {
                memset(&(fds[i]), 0, sizeof(struct pollfd));
            }
        }
        
        ret = poll(fds, TCP_USER_NUM, 2000); // 阻塞直到有事件发生或超时
        if (ret == -1) 
        {
            LOG_ALARM("poll");
            system("sleep 3");
            continue;
        }
        
        for(i = 0; i < TCP_USER_NUM; i++)
        {
            if ((tcp->user[i].used == 1) && (fds[i].revents & POLLIN))  //有数据进入
            {
                memset(buf512, '\0', strlen(buf512));    
                len = read(fds[i].fd, buf512, 24);
                if(len == 0)  //读取的长度len大于0，说明有数据可读；等于0，代表断开连接，小于0，没有数据
                {
                    LOG_INFO("i=%d [%s-%d] 断开连接\n", i, tcp->user[i].ipv4, tcp->user[i].port);

                    
                    tcpUser_release(tcp, i);
                }
                else if(len > 0)
                {
                     LOG_SHOW("-----------------------i=%d [%s-%d]msg %dbyte--------------------\n%s\n",
                                        i, tcp->user[i].ipv4, tcp->user[i].port, len, buf512);
                }
                
            }
        }
        
    }

}



//测试
void tcp_client_test(char *ipv4, int port, char *targetIpv4, int targetPort)
{
    char buf521[512] = {0};

    tcp_init();

    tcp_client_t *client = tcp_client_create(ipv4, port);

   if(0 != tcp_client_start(client, targetIpv4, targetPort))return;  
    
    snprintf(buf521, 512, "this msg form is client [%s:%d]!", ipv4, port);
    tcp_client_send(client, buf521, strlen(buf521));


    //----------------------------------------------发送
    system("sleep 2");

    snprintf(buf521, 512, "this is second msg……!");
    tcp_client_send(client, buf521, strlen(buf521));

    system("sleep 2");

//        snprintf(buf521, 512, "GET /index.html HTTP/1.1\r\n"
//                                "Host: www.example.com\r\n\r\n");
//        tcp_client_send(client, buf521, strlen(buf521));

//        char sendStr[] =    "GET /index.html HTTP/1.1\r\n"
//                            "Accept-Language: en-US,en; q =0.8 ; new, old  =100\n"
//                            "Accept-Language1: en- US ,en =1232131 ; q =0.8 ; new, old  =100\n"
//                            "Authorization: Digest username=username, realm=realm-name, nonce=nonce-value,"
//                                "uri=request-uri, response=response-value, opaque=opaque-value, qop=qop-value,"
//                                "nc=nonce-count, cnonce=client-nonce\n"
//                            "\n";

//        char sendStr[] =    "GET /protected/resource HTTP/1.1\n"
//                            "Host: example.com\n"
//                            "Authorization: Basic dXNlck5hbWU6cGFzc3dvcmQ=,\n\n";

   char sendStr[] = "GET /protected/resource HTTP/1.1\n"
                    "Host: example.com\n"
                    "Authorization: Digest username=\"user\", "
                    "realm=\"Restricted Area\", "
                    "nonce=\"af8c7760297db973dfbbbe1d268cb6bd\", "
                    "uri=\"/protected/resource\", "
                    "response=\"6629fae49393a05397450978507c4ef1\", "
                    "opaque=\"8c85f23cbe747fe648bb2d130f2986a3\", "
                    "qop=auth, "
                    "nc=00000101, "
                    "cnonce=\"0a4f113b\" "
                    "\n\n";

    tcp_client_send(client, sendStr, strlen(sendStr));

    //system("sleep 2");



    //----------------------------------------------接收
//    int ret;
//#define BUF_SIZE 512        
//    char buf[BUF_SIZE] = {0};  
//    int len;
//
//#define POLL_SIZE 1    
//    struct pollfd fds[POLL_SIZE] = {0};  //使用 poll 方法来并发处理用户的数据收发
//    fds[0].fd = client->fd;
//    fds[0].events = POLLIN;
//    
//    while (1) 
//    {
//       
//        ret = poll(fds, POLL_SIZE, 2000); // 阻塞直到有事件发生或超时
//        if (ret == -1) 
//        {
//            LOG_ALARM("客户端 poll 失败");
//            system("sleep 3");
//            continue;
//        }
//
//
//        //读取的长度len大于0，说明有数据可读；等于0，代表断开连接，小于0，没有数据
//        
//        if (fds[0].revents & POLLIN)  //有数据进入
//        {
//            LOG_SHOW("---------------->POLL   IN\n");
//            len = read(client->fd, buf, BUF_SIZE);
//            if(len == 0)    
//            {
//                LOG_INFO("断开连接 ...");
//                break;
//            }                
//            else if(len > 0)
//            {                    
//                 LOG_SHOW("\n【客户端 ip地址：%s:%d 读取的字节：%dbyte】\n"
//                           "------------------------------------\n"
//                           "%s\n------------------------------------\n\n",
//                           client->ipv4, client->port, len, buf);  
//
//                 //break;
//            }
//   
//        }
//
//    }
//
//    //----------------------------------------------关闭连接
//    close(client->fd);      

    
    pthread_join(client->transceiver, NULL);
}



