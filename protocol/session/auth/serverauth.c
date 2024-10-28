


//acs 配置
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "log.h"
#include "auth.h"

#include "global.h"
#include "md5.h"
#include "base64.h"
#include "strpro.h"




/*
服务器回应：
HTTP/1.1 401 Unauthorized
WWW-Authenticate: Digest realm="Restricted Area", \
qop="auth", \
nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", \
opaque="5ccc069c403ebaf9f0171e9517f40e41"
Content-Type: text/html
Content-Length: 137
*/


/* 
客户端回应：
GET /protected/resource HTTP/1.1
Host: example.com
Authorization: 
Digest username="user", \
realm="Restricted Area", \
nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", \
uri="/protected/resource", \
response="6629fae49393a05397450978507c4ef1", \
opaque="5ccc069c403ebaf9f0171e9517f40e41", \
qop=auth, \
nc=00000001, \
cnonce="0a4f113b"

*/

/*
机制：
    1、nonce：通常在每次服务器发送 `401 Unauthorized` 响应时生成一个新的 
    `nonce`。`nonce` 是一次性的，客户端在每次新的认证尝试中都需要使用新的 
    `nonce`。
    
    2、opaque`：`opaque` 通常在服务器启动时或某个特定的时间间隔内生成一次
    ，并在多次认证请求中保持不变。`opaque` 不需要每次都重新生成。

    3、在Digest认证中，uri 参数是 Authorization 头的一部分，用于标识客户端
    请求的具体资源。这个 uri 参数必须与请求行中的资源路径一致。
*/

/*==============================================================
                        xx
==============================================================*/
#define SERVER_HOME_DIR "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest"
#define SERVER_ACCOUNT_PATH "protocol/session/auth/account.txt"

int serverauth_parse_account(keyvalue_obj_t *keyvalue)
{
    if(keyvalue == NULL)return RET_FAILD;

#define PATH_SIZE 256
    char path[PATH_SIZE] = {0};
    snprintf(path, PATH_SIZE, "%s/%s", SERVER_HOME_DIR, SERVER_ACCOUNT_PATH);

    FILE *account = fopen(path, "r");
    if(account == NULL)return RET_FAILD;

#define BUF_SIZE 256
    char buf[BUF_SIZE + 8] = {0};
    char bufUserName[BUF_SIZE + 8] = {0};
    char bufPassword[BUF_SIZE + 8] = {0};
    char ch;
    int ret;
    int cnt;
    char *find;
    int pos;
    //int len;
    int breaMark;
    char *p;
    int end;

    cnt = 0;
    breaMark = 0;
    while(1)
    {
        ret = fread(&ch, 1, 1, account);
        if(ret <= 0)breaMark = 1; //读取完毕
        
        if(cnt < BUF_SIZE)
        {
            buf[cnt++] = ch;
        }            
        else if(cnt == BUF_SIZE)
        {
            LOG_ALARM("这一行太长了\n");
        }
            

        if(ch == '\n' ||  breaMark == 1)    //最后一行数据可能存在
        {
            if(cnt > 1 && cnt <= BUF_SIZE)
            {
                //buf[cnt] = '\0';
                find = strstr(buf, ":");
                if(find != NULL)
                {                   
                    pos = find - buf;
                    memcpy(bufUserName, buf, pos);  //可以考虑去掉两边空格...
                    bufUserName[pos] = '\0';
                    //LOG_SHOW("userName:%s\n", bufUserName);
                    
                     
                    memmove(buf, buf + pos + 1, cnt - pos - 1);

                    //去掉 \r\n
                    end = cnt - pos - 1;
                    buf[end] = '\0';
                    if(end >=  2)   //需要保证长度 
                    {
                        if(buf[end - 1] == '\n')
                        {

                             buf[end - 1] = '\0';
                             if(buf[end - 2] == '\r')
                             {
                                buf[end - 2] = '\0';
                             }
                        }
                    } 
                      

                    p = buf;    
                    strpro_clean_space(&p);     //去掉空格
                    strcpy(bufPassword, p);
                    //LOG_SHOW("password:%s\n", bufPassword);

                    keyvalue_append_set_str(keyvalue, bufUserName, bufPassword);
                    
                }
    
            }
            cnt = 0;
        }  

        if(breaMark == 1)break;
    }
    return RET_OK;

}


/*==============================================================
                        http 摘要认证（服务器）
==============================================================*/
//产生 nonce 随机值， 32 字节
void __generate_nonce(char **p) 
{
    static int seed = 100;
    srand(seed += 100);    //随机数种子
    
    char buffer[100];
    
    snprintf(buffer, sizeof(buffer), "%ld%ld", (long)time(NULL), (long)rand());
	buffer[16] = '\0';

	MD5_CTX context;
    unsigned char digest[16];
    unsigned int len = strlen (buffer);

    MD5Init (&context);
    MD5Update (&context, (unsigned char *)buffer, len);
    MD5Final (digest, &context);
	  
    for (int i = 0; i < 16; i++) 
        sprintf(*p + (i * 2), "%02x", (unsigned int)(digest[i]));
}

//产生 opaque 随机值， 32 字节 ，借用 nonce 生成法
void __generate_opaque(char **p) 
{
    __generate_nonce(p);
}


//初始化配置
int serverauth_obj_init(server_digest_auth_obj_t *auth, char *realm, char *qop)
{
    char *p;

    if(auth == NULL)return RET_FAILD;

    //nonce 随机值，设置为32个字节
    p = auth->nonce;
    __generate_nonce(&p);

    //nc：同一个随机序列 nonce，在每次信息交互后需要递增
    auth->nc = 0;

    //opaque 随机值，设置为32个字节
    p = auth->opaque;
    __generate_opaque(&p);
    
    //LOG_SHOW("nonce:%s\n", auth->nonce);
    //LOG_SHOW("opaque:%s\n", auth->opaque);
    
    //realm 设置
    if(realm == NULL)
        strcpy(auth->realm, "/");
    else
        snprintf(auth->realm, HTTP_AUTH_REALM_STRING_LEN, "%s", realm);

    //质量保护
    if(qop != NULL)
        strcpy(auth->qop, qop);

    //状态机
    auth->staus = digest_auth_status_start;
    
    return RET_OK;
}


/*==============================================================
                        http 客户端 摘要认证和基础认证
==============================================================*/
//产生 cnonce 随机值， 目标是 8 字节 ，借用 nonce 生成法
void __generate_cnonce(char **p) 
{
    char *ptmp;
    char buf[32 + 8] = {0};
    ptmp = buf;
    __generate_nonce(&ptmp);
    buf[9] = '\0';

    strcpy(*p, buf);
    
}
//基本认证配置
int clientauth_basic_cfg(client_auth_obj_t *auth, char *username, char *password)
{
    char *p;
    char buf[HTTP_AUTH_BASE64_STRING_LEN] = {0};
    
    if(auth == NULL)return RET_FAILD;

    if(username != NULL)
        strcpy(auth->username, username);
    if(password != NULL)
        strcpy(auth->password, password);    

    //设置 base64[username:password] 的值
    if(username != NULL && password != NULL)
    {
        snprintf(buf, HTTP_AUTH_BASE64_STRING_LEN, "%s:%s", username, password);
        p = auth->base64;
        base64_code(buf, &p);    
    }

    p = auth->cnonce;
    __generate_cnonce(&p);

    //置零 认证失败次数
    auth->authFaildCnt = 0;
        
    return RET_OK;
}

//客户端认证初始化
int clientauth_init(client_auth_obj_t *auth, char *username, char *password, int nc)
{
    if(auth == NULL)return RET_FAILD;
    auth->nc = nc;
    return clientauth_basic_cfg(auth, username, password);
}


//摘要认证配置
int clientauth_digest_cfg(client_auth_obj_t *auth, char *username, char *password, 
                            char *realm, char *qop, char *nonce, char *opaque, 
                            char *uri, char *response)
{
    clientauth_basic_cfg(auth, username, password);
    if(realm != NULL)
        strcpy(auth->realm, realm);
    if(qop != NULL)
        strcpy(auth->qop, qop);
    if(nonce != NULL)
        strcpy(auth->nonce, nonce); 
    if(opaque != NULL)
        strcpy(auth->opaque, opaque);        
    if(uri != NULL)
        strcpy(auth->uri, uri); 
    if(response != NULL)
        strcpy(auth->response, response);

                
    return RET_OK;
}

//更新 cnonce
int clientauth_updata(client_auth_obj_t *auth)
{
    char *p;
    
    if(auth == NULL)return RET_FAILD;  

    p = auth->cnonce;
    __generate_cnonce(&p);
         
    return RET_OK;
}

/*==============================================================
                        测试
==============================================================*/

void serverauth_test()
{
//    keyvalue_obj_t *keyvalue = keyvalue_create(100);
//    serverauth_parse_account(keyvalue);
//
//    keyvalue_show(keyvalue);

    server_digest_auth_obj_t auth;
    serverauth_obj_init(&auth, "/cwmp", "auth-int");

    char *p;    
    char str[64] = {0};
    p = str;
    __generate_cnonce(&p);
    printf("---->%s\n", str);

    __generate_cnonce(&p);
    printf("---->%s\n", str);

    __generate_cnonce(&p);
    printf("---->%s\n", str);

}

