

#ifndef _SSL_H_
#define _SSL_H_

#include "linux.h"

/*==============================================================
                        数据结构
==============================================================*/


typedef struct ssl_obj_t{
    char isConfiged; 
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    SSL *ssl;  

    int (*set_fd)(SSL *, int );  
}ssl_obj_t;

#define SSL_MG_INIT_CODE 0x88

#define SSL_CERT_FILE_PATH "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest/space/ssl/server-cert.pem"
#define SSL_KEY_FILE_PATH "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest/space/ssl/server-key.pem"

/*==============================================================
                        接口
==============================================================*/

void ssl_mg_init();
ssl_obj_t *ssl_create();
void ssl_init();
void ssl_cleanup();
void ssl_server_config(ssl_obj_t *ssl, char *certFile, char *keyFile);
void ssl_client_config(ssl_obj_t *ssl);
void ssl_destory(ssl_obj_t *ssl);
void ssl_test();




#endif



