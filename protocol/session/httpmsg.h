
#ifndef _HTTPMSG_H_
#define _HTTPMSG_H_

#include "auth.h"
#include "http.h"
#include "tcp.h"





/*==============================================================
                        数据结构 
==============================================================*/
#define HTTP_CLIENT_MSG_FIRST_LINE_SIZE 64
#define HTTP_CLIENT_MSG_AUTH_SIZE 1024      //注意留下足够空间
#define HTTP_CLIENT_MSG_KEYVALUE_SIZE 100   //头部信息 子项的个数
typedef struct httpClient_msg_t{
    char firstLine[HTTP_CLIENT_MSG_FIRST_LINE_SIZE + 8];
    char auth[HTTP_CLIENT_MSG_AUTH_SIZE + 8];

    keyvalue_obj_t *keyvalue;   //头部信息 子项的个数
    char keyvalueEn;

    void *payload;  //负载
    int payloadLen;
}httpClient_msg_t;




/*==============================================================
                        接口
==============================================================*/  
httpClient_msg_t *httpClient_create();
void httpCLient_destroy(httpClient_msg_t *msg);
int httpmsg_client_head_init(httpClient_msg_t *msg, char *method, char *uri, 
                                char *version, int authMode, client_auth_obj_t *auth);
int httpmsg_client_send_msg(httpClient_msg_t *msg, tcp_client_t *client);


void httpmsg_mg_init();
void httpmsg_test();


int httpmsg_server_first_line(char **msg, int maxSize, int *usedLen, char *version, char *code, char *reason);
int httpmsg_server_basic_auth_append(char **msg, int maxSize, int *usedLen, char *realm);
int httpmsg_server_digest_auth_append(char **msg, int maxSize, int *usedLen, server_digest_auth_obj_t *auth);
int httpmsg_client_first_line(char **msg, int maxSize, int *usedLen, char *method, char *uri, char *version);
int httpmsg_client_basic_auth_append(char **msg, int maxSize, int *usedLen, client_auth_obj_t *auth);
int httpmsg_client_digest_auth_append(char **msg, int maxSize, int *usedLen, client_auth_obj_t *auth);


#endif
