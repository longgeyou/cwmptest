




#include <stdio.h>
#include <string.h>

#include "httpmsg.h"
#include "keyvalue.h"
#include "pool2.h"
#include "log.h"
//#include "base64.h"
#include "digcalc.h"







/*==============================================================
                        本地管理
==============================================================*/
#define HTTPMSG_INIT_CODE 0x8080
    
typedef struct httpmsg_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    
    int initCode;
}httpmsg_manager_t;

httpmsg_manager_t httpmsg_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(httpmsg_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define HTTPMSG_POOL_NAME "httpmsg"
    
//返回值 缓存大小
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1

#define BUF_SIZE_128 128
#define BUF_SIZE_256 256
#define BUF_SIZE_512 512
#define BUF_SIZE_1024 1024
#define BUF_SIZE_2048 2048

    
//初始化
void httpmsg_mg_init()
{    
    if(httpmsg_local_mg.initCode == HTTPMSG_INIT_CODE)return ;   //防止重复初始化
    httpmsg_local_mg.initCode = HTTPMSG_INIT_CODE;
    
    strncpy(httpmsg_local_mg.poolName, HTTPMSG_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    httpmsg_local_mg.poolId = pool_apply_user_id(httpmsg_local_mg.poolName);    
}



/*==============================================================
                        对象
==============================================================*/

//创建 httpClient_msg_t 对象
httpClient_msg_t *httpClient_create()
{
    httpClient_msg_t *msg = (httpClient_msg_t *)MALLOC(sizeof(httpClient_msg_t));
    if(msg == NULL)return NULL;
    msg->keyvalue = keyvalue_create(HTTP_CLIENT_MSG_KEYVALUE_SIZE);
    if(msg->keyvalue != NULL)msg->keyvalueEn = 1;
    
    return msg;
}


//摧毁 httpClient_msg_t 对象
void httpCLient_destroy(httpClient_msg_t *msg)
{
    if(msg == NULL)return ;

    //1、释放成员
    keyvalue_destroy(msg->keyvalue);
    
    //2、摧毁自己
    FREE(msg);
}


/*==============================================================
                        客户端：应用
==============================================================*/
//客户http报文 头部初始化  
//参数 msg 指向已经分配好动态内存的指针
//参数 authMode ， 0代表Basic， 其他 代表digest
int httpmsg_client_head_init(httpClient_msg_t *msg, char *method, char *uri, char *version, int authMode, client_auth_obj_t *auth)
{
    int len;
    int ret;
    char *p;
    
    if(msg == NULL)return RET_FAILD;

    p = msg->firstLine;
    ret = httpmsg_client_first_line(&p, HTTP_CLIENT_MSG_FIRST_LINE_SIZE, &len, method, uri, version);
    if(ret != RET_OK) LOG_ALARM("报文构造未成功 httpmsg_client_first_line");

    if(authMode == 0)  //基础认证
    {
        
        clientauth_updata(auth);    

        p = msg->auth;
        ret = httpmsg_client_basic_auth_append(&p, HTTP_CLIENT_MSG_AUTH_SIZE, &len, auth);
        if(ret != RET_OK) LOG_ALARM("报文构造未成功 httpmsg_client_basic_auth_append");
    }
    else  //摘要认证
    {
        clientauth_updata(auth);
        
        //------------------------------------------------------------------------------计算摘要值       
        LOG_SHOW("计算摘要认证 ...\n");
        //1.2.2 确定摘要认证所需的变量
        char * pszNonce = auth->nonce;
        char * pszCNonce = auth->cnonce;
        char * pszUser = auth->username;
        char * pszRealm = auth->realm;
        char * pszPass = auth->password;                                     
        char * pszAlg = "md5";


        if(strcmp(auth->oldNonce, pszNonce) == 0)
        {
            auth->nc++;  //nc需要自增，并告知服务器
        }
        else
        {
            strcpy(auth->oldNonce, pszNonce);
            auth->nc = 10;
        }
        char szNonceCount[9] = {0};  
        snprintf(szNonceCount, 9, "%08d", auth->nc % 9999999);
        
        char * pszMethod = method;   //测试用
        char * pszQop = auth->qop; 
        char * pszURI = uri;    //测试用
        HASHHEX HA1;
        HASHHEX HA2 = "";
        HASHHEX Response;

//        LOG_SHOW("计算摘要值所需参数：\n"
//                                "pszNonce:%s\n"
//                                "pszCNonce:%s\n"
//                                "pszUser:%s\n"
//                                "pszRealm:%s\n"
//                                "pszPass:%s\n"
//                                "pszAlg:%s\n"
//                                "szNonceCount:%s\n"
//                                "pszMethod:%s\n"
//                                "pszQop:%s\n"
//                                "pszURI:%s\n",
//                                pszNonce,
//                                pszCNonce,
//                                pszUser,
//                                pszRealm,
//                                pszPass,
//                                pszAlg,
//                                szNonceCount,
//                                pszMethod,
//                                pszQop,
//                                pszURI);
        
        DigestCalcHA1(pszAlg, pszUser, pszRealm, pszPass, pszNonce, pszCNonce, HA1);
        DigestCalcResponse(HA1, pszNonce, szNonceCount, pszCNonce, pszQop, pszMethod, pszURI, HA2, Response);
        LOG_SHOW("Response = %s\n", Response);
        //------------------------------------------------------------------------------计算摘要值 
        //存入 auth        
        strcpy(auth->uri, pszURI);
        strcpy(auth->response, Response);

        p = msg->auth;
        ret = httpmsg_client_digest_auth_append(&p, HTTP_CLIENT_MSG_AUTH_SIZE, &len, auth);
        
        if(ret != RET_OK) LOG_ALARM("报文构造未成功 httpmsg_client_digest_auth_append");     
    }  

    return RET_OK;
    
}

//客户http报文 发送
int httpmsg_client_send_msg(httpClient_msg_t *msg, tcp_client_t *client)
{
    int pos;
    char *p;
    char buf2048[BUF_SIZE_1024 * 2 + 8] = {0};

    if(msg == NULL || client == NULL)return RET_FAILD;

    tcp_client_send(client, msg->firstLine, strlen(msg->firstLine));
    tcp_client_send(client, msg->auth, strlen(msg->auth));

    pos = 0;
    p = buf2048;
    if(msg->keyvalue != NULL && msg->keyvalueEn == 1)
    {
        KEYVALUE_FOREACH_START(msg->keyvalue, iter)
        {
            keyvalue_data_t *data = iter;
            if(data->keyEn == 1 && data->key != NULL &&
                data->valueEn == 1 && data->value != NULL)
            {
                p += pos;
                pos += snprintf(p, (BUF_SIZE_1024 * 2) - pos, "%s: %s\n", (char *)data->key, (char *)data->value); //
                if(pos >= BUF_SIZE_1024)      //满 BUF_SIZE_1024 发送数据然后清空缓存
                {
                    tcp_client_send(client, buf2048, pos);
                    pos = 0;
                    p = buf2048;
                }
                
            }
        }KEYVALUE_FOREACH_END  

        if(pos > 0)
            tcp_client_send(client, buf2048, pos);
    }
    tcp_client_send(client, "\n", 1);   //空行代表 http头部 结束



    /////////////////////// 发送 负载 ......

    

    return RET_OK;;
}


/*==============================================================
                        认证
==============================================================*/

//服务器报文 添加第一行
int httpmsg_server_first_line(char **msg, int maxSize, int *usedLen, char *version, int code, char *reason)
{

    char buf[BUF_SIZE_128 + 8] = {0};
    
    if(msg == NULL) return RET_FAILD;
    if(*msg == NULL)return RET_FAILD;

    snprintf(buf, BUF_SIZE_128, "%s %d %s\n", version, code, reason);
    *usedLen = strlen(buf);

    if(*usedLen  > maxSize)return RET_FAILD;
    
    strcpy(*msg, buf);
    

    return RET_OK;
}

//服务器报文 添加基础认证
int httpmsg_server_basic_auth_append(char **msg, int maxSize, int *usedLen, char *realm)
{
    char buf[BUF_SIZE_256 + 8] = {0};
    
    if(msg == NULL || realm == NULL) return RET_FAILD;
    if(*msg == NULL)return RET_FAILD;

    snprintf(buf, BUF_SIZE_256, "WWW-Authenticate: Basic realm=%s\n", realm);
    *usedLen = strlen(buf);

    if(*usedLen  > maxSize)return RET_FAILD;
    
    strcpy(*msg, buf);
    

    return RET_OK;
}

//服务器报文 添加基础认证
int httpmsg_server_digest_auth_append(char **msg, int maxSize, int *usedLen, server_digest_auth_obj_t *auth)
{
#define BUF_SIZE_B 128
    char buf[BUF_SIZE_1024 + 8] = {0};
    
    if(msg == NULL || auth == NULL) return RET_FAILD;
    if(*msg == NULL)return RET_FAILD;

    snprintf(buf, BUF_SIZE_1024, "WWW-Authenticate: Digest realm=%s, qop=%s, nonce=%s, opaque=%s\n" ,
                                auth->realm, auth->qop, auth->nonce, auth->opaque);
    *usedLen = strlen(buf);

    if(*usedLen  > maxSize)return RET_FAILD;
    
    strcpy(*msg, buf);
    

    return RET_OK;
}


//客户端报文 添加第一行
int httpmsg_client_first_line(char **msg, int maxSize, int *usedLen, char *method, char *uri, char *version)
{
#define BUF_SIZE_A 128
    char buf[BUF_SIZE_A + 8] = {0};
    
    if(msg == NULL) return RET_FAILD;
    if(*msg == NULL)return RET_FAILD;

    snprintf(buf, BUF_SIZE_A, "%s %s %s\n", method, uri, version);
    *usedLen = strlen(buf);

    if(*usedLen  > maxSize)return RET_FAILD;
    
    strcpy(*msg, buf);
    

    return RET_OK;
}

//请求报文 添加基础认证
int httpmsg_client_basic_auth_append(char **msg, int maxSize, int *usedLen, client_auth_obj_t *auth)
{
#define BUF_SIZE_B 128
    char buf[BUF_SIZE_B + 8] = {0};
    
    if(msg == NULL || auth == NULL) return RET_FAILD;
    if(*msg == NULL)return RET_FAILD;

    snprintf(buf, BUF_SIZE_B, "Authorization: Basic %s\n", auth->base64);
    *usedLen = strlen(buf);

    if(*usedLen  > maxSize)return RET_FAILD;
    
    strcpy(*msg, buf);
    

    return RET_OK;
}

//请求报文 添加摘要认证
int httpmsg_client_digest_auth_append(char **msg, int maxSize, int *usedLen, client_auth_obj_t *auth)
{
#define BUF_SIZE_C 2048    //斟酌设置
    char buf[BUF_SIZE_C + 8] = {0};
    char str[] = "Authorization: Digest ";
    int pos;
    char *p;
    int ret;
    
    if(msg == NULL || auth == NULL) return RET_FAILD;
    if(*msg == NULL)return RET_FAILD;

    pos = 0;
    p = buf;
    ret = snprintf(p + pos, BUF_SIZE_C, "%s", str);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "username=\"%s\", ", auth->username);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "realm=%s, ", auth->realm);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "nonce=%s, ", auth->nonce);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "uri=\"%s\", ", auth->uri);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "response=\"%s\", ", auth->response);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "opaque=%s, ", auth->opaque);
    pos += ret;
    ret = snprintf(p + pos, BUF_SIZE_C, "qop=%s, ", auth->qop);
    pos += ret;    
    ret = snprintf(p + pos, BUF_SIZE_C, "nc=%d, ", auth->nc);
    pos += ret;    
    ret = snprintf(p + pos, BUF_SIZE_C, "cnonce=\"%s\"\n", auth->cnonce);
    pos += ret;
    
    
    *usedLen = strlen(buf);

    if(*usedLen  > maxSize)return RET_FAILD;
    
    strcpy(*msg, buf);
    

    return RET_OK;    
}



/*==============================================================
                        特定的消息
==============================================================*/
//cpe 给 acs 发送 hello 信息（用于探测是否要摘要认证）
void httpmsg_client_say_hello(char *out, int len)
{
    char buf1024[1024] = {0};
    char header[] =    "GET /cwmp HTTP/1.1\n"
                        "Host: example.com\n"
                        "Content-Type: text/xml; charset=\"UTF-8\"\n"
                        "Content-Length: %d\n\n";

    char content[] =    "<html>\n"
                        "<head>\n"
                        "<title>Unauthorized</title>\n"
                        "</head>\n"
                        "<body>\n"
                        "<h1>hello</h1>\n"
                        "<p>hello cwmp</p>\n"
                        "</body>\n"
                        "</html>";

    snprintf(buf1024, 1024, header, strlen(content));                    
    snprintf(out, len, "%s%s", buf1024, content);
    
}

//简单的http回复 （可用于摘要认证回复）
void httpmsg_server_response_hello(char *out, int len)
{
    char buf1024[1024] = {0};
    char header[] =    "HTTP/1.1 200 OK\n"
                        "Host: cwmp.example.com\n"
                        "Content-Type: text/xml; charset=\"UTF-8\"\n"
                        "Content-Length: %d\n\n";

    char content[] =    "hello visitor!";

    snprintf(buf1024, 1024, header, strlen(content));                    
    snprintf(out, len, "%s%s", buf1024, content);  
}




/*==============================================================
                        测试
==============================================================*/

void httpmsg_test()
{

    LOG_SHOW("this is httpmsg test\n");
}


