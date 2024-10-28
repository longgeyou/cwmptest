

#ifndef _AUTH_H_
#define _AUTH_H_



#include "keyvalue.h"


#define RET_FAILD -1
#define RET_OK 0

/*==============================================================
                        服务器认证 数据结构
==============================================================*/



#define HTTP_AUTH_REALM_STRING_LEN 128
#define HTTP_AUTH_NONCE_STRING_LEN 32
#define HTTP_AUTH_OPAQUE_STRING_LEN 32
#define HTTP_AUTH_QOP_STRING_LEN 32
#define HTTP_AUTH_CNONCE_STRING_LEN 32
#define HTTP_AUTH_URI_STRING_LEN 128
#define HTTP_AUTH_RESPONSE_STRING_LEN 256
#define HTTP_AUTH_USERNAME_STRING_LEN 64
#define HTTP_AUTH_PASSWORD_STRING_LEN 64
#define HTTP_AUTH_BASE64_STRING_LEN 256



//typedef enum digest_auth_qop_e{ //qop类型
//    digest_auth_qop_auth,
//    digest_auth_qop_auth_int
//}digest_auth_qop_e;


typedef enum digest_auth_status_e{ //状态机
    digest_auth_status_start,   //未开启状态
    digest_auth_status_wait_reply   //已配置，并发送了认证要求，等待回应
}digest_auth_status_e;



//服务器认证对象
typedef struct server_digest_auth_obj_t{
    //配置
    //char username[HTTP_AUTH_NAME_STRING_LEN + 8];
    char realm[HTTP_AUTH_REALM_STRING_LEN + 8];
    char nonce[HTTP_AUTH_NONCE_STRING_LEN + 8];
    int nc; 
    char opaque[HTTP_AUTH_OPAQUE_STRING_LEN + 8];
    //digest_auth_qop_e qop;  
    char qop[HTTP_AUTH_QOP_STRING_LEN + 8]; 
    //char cnonce[HTTP_AUTH_CNONCE_STRING_LEN + 8];
    //char uri[HTTP_AUTH_URI_STRING_LEN + 8];
    //char response[HTTP_AUTH_RESPON_STRING_LEN + 8];

    //状态机
    digest_auth_status_e staus;
    
}server_digest_auth_obj_t;


//客户端认证对象
typedef struct client_auth_obj_t{
    //配置
    char username[HTTP_AUTH_USERNAME_STRING_LEN + 8];
    char password[HTTP_AUTH_PASSWORD_STRING_LEN + 8]; 
    char base64[HTTP_AUTH_BASE64_STRING_LEN + 8];
    
    char realm[HTTP_AUTH_REALM_STRING_LEN + 8];
    char nonce[HTTP_AUTH_NONCE_STRING_LEN + 8];
    char oldNonce[HTTP_AUTH_NONCE_STRING_LEN + 8];  //存储上一次的nonce，这个和nc自增计数相关
 
    char opaque[HTTP_AUTH_OPAQUE_STRING_LEN + 8];    
    char qop[HTTP_AUTH_QOP_STRING_LEN + 8];  
    char cnonce[HTTP_AUTH_CNONCE_STRING_LEN + 8];
    char uri[HTTP_AUTH_URI_STRING_LEN + 8];
    char response[HTTP_AUTH_RESPONSE_STRING_LEN + 8];  

    int nc; 
    
    //状态机
    digest_auth_status_e staus;

    //认证失败计数
    int authFaildCnt;
    
}client_auth_obj_t;



/*==============================================================
                        服务器认证 接口
==============================================================*/
void serverauth_test();
int serverauth_parse_account(keyvalue_obj_t *keyvalue);
int serverauth_obj_init(server_digest_auth_obj_t *auth, char *realm, char *qop);

int clientauth_basic_cfg(client_auth_obj_t *auth, char *username, char *password);
int clientauth_init(client_auth_obj_t *auth, char *username, char *password, int nc);

int clientauth_digest_cfg(client_auth_obj_t *auth, char *username, char *password, 
                            char *realm, char *qop, char *nonce, char *opaque, 
                            char *uri, char *response);

int clientauth_updata(client_auth_obj_t *auth);



#endif




