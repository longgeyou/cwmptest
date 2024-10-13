


//ssl 层，主要是调用linux自带的接口，用法的话，可以询问通义千问、openai

/*

1、linux准备：
sudo apt-get install libssl-dev    ;安装
openssl version                    ;版本查询

2、头文件包含
#include <openssl/ssl.h>
#include <openssl/err.h>

3、编译时候需要链接
例如： gcc program.c -o program -lssl -lcrypto

4、TLS_server_method 和版本关系
TLS_server_method 是在 OpenSSL 1.1.0 版本中引入的，在 OpenSSL 1.0.2 
版本及之前，你无法使用这个函数。在 OpenSSL 1.0.2 中，常用的方法是：

    使用 SSLv23_server_method() 来代替 TLS_server_method，因为它支持所有版本的 TLS 并且向后兼容
*/





#include "ssl.h"
#include "pool2.h"



/*==============================================================
                        接口
==============================================================*/
/*
//const SSL_METHOD *TLS_server_method(void);          // 返回 TLS 服务器方法，用于创建 TLS 服务器端的 SSL 上下文
//const SSL_METHOD *TLS_client_method(void);          // 返回 TLS 客户端方法，用于创建 TLS 客户端的 SSL 上下文

const SSL_METHOD *method = SSLv23_server_method();

void SSL_load_error_strings(void);                  // 加载 OpenSSL 错误字符串，使错误信息更易读
void OpenSSL_add_ssl_algorithms(void);              // 初始化和注册 SSL/TLS 协议所需的加密算法
void EVP_cleanup(void);                             // 清理 OpenSSL 库中使用的加密算法和其他资源
SSL_CTX *SSL_CTX_new(const SSL_METHOD *method);     // 创建一个新的 SSL 上下文
void SSL_CTX_set_verify(SSL_CTX *ctx, int mode, int (*verify_callback)(int, X509_STORE_CTX *));  // 设置 SSL 上下文的证书验证模式和回调函数
int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type);  // 为 SSL 上下文设置证书文件
int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);   // 为 SSL 上下文设置私钥文件
void SSL_CTX_free(SSL_CTX *a);                      // 释放 SSL 上下文资源
SSL *SSL_new(SSL_CTX *ctx);                         // 创建一个新的 SSL 对象
int SSL_shutdown(SSL *ssl);                         // 关闭 SSL 连接
void SSL_free(SSL *ssl);                            // 释放 SSL 对象资源
int SSL_set_fd(SSL *ssl, int fd);                   // 为 SSL 对象设置文件描述符
*/


/*==============================================================
                        本地管理
==============================================================*/
typedef struct ssl_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int instanceNum;
    char initCode;

    
}ssl_manager_t;

ssl_manager_t ssl_local_mg = {0};
    
//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(ssl_local_mg.poolId, x)
#define POOL_FREE(x) pool_user_free(x)
#define SSL_POOL_NAME "ssl"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//本地管理初始化
void ssl_mg_init()
{    
    if(ssl_local_mg.initCode == SSL_MG_INIT_CODE)return;    //表示只初始化一次
    strncpy(ssl_local_mg.poolName, SSL_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    ssl_local_mg.poolId = pool_apply_user_id(ssl_local_mg.poolName); 
    ssl_local_mg.instanceNum = 0;
    
    ssl_local_mg.initCode = SSL_MG_INIT_CODE;
    

    pool_init();    //依赖于 pool.c
}


/*==============================================================
                        xx
==============================================================*/
//创建一个 ssl     实例对象（一般情况下，摒弃条件判断而返回错误码，调用者需注意参数输入）


ssl_obj_t *ssl_create()
{
    ssl_obj_t *ssl = (ssl_obj_t *)POOL_MALLOC(sizeof(ssl_obj_t));
    ssl->isConfiged = 0;

    ssl->set_fd = SSL_set_fd;
    ssl_local_mg.instanceNum++;
    return ssl;
}




//初始化
void ssl_init()
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();  
}

//清空
void ssl_cleanup() {
    EVP_cleanup();
}



//ssl配置为服务器  ，使用 cert文件 和 key文件
void ssl_server_config(ssl_obj_t *ssl, char *certFile, char *keyFile)
{
    ssl->method = SSLv23_server_method();  
    ssl->ctx = SSL_CTX_new(ssl->method);

    if (SSL_CTX_use_certificate_file(ssl->ctx, certFile, SSL_FILETYPE_PEM) <= 0) {
        printf("ssl_server_config_file::SSL_CTX_use_certificate_file error\n");
        return;
    }

    if (SSL_CTX_use_PrivateKey_file(ssl->ctx, keyFile, SSL_FILETYPE_PEM) <= 0 ) {
        printf("ssl_server_config_file::SSL_CTX_use_PrivateKey_file error\n");
        return;
    }

    ssl->ssl = SSL_new(ssl->ctx);
    ssl->isConfiged = 1;
}


//ssl配置为客户端 
void ssl_client_config(ssl_obj_t *ssl)
{
    ssl->method = SSLv23_client_method();  
    ssl->ctx = SSL_CTX_new(ssl->method);
     
    SSL_CTX_set_verify(ssl->ctx, SSL_VERIFY_NONE, NULL);
    ssl->ssl = SSL_new(ssl->ctx);
    ssl->isConfiged = 1;
}


//销毁 ssl
void ssl_destory(ssl_obj_t *ssl)
{
    SSL_shutdown(ssl->ssl);
    SSL_free(ssl->ssl);
    SSL_CTX_free(ssl->ctx);
    ssl_cleanup();
    
    ssl_local_mg.instanceNum--;
    
    POOL_FREE(ssl);
}

/*==============================================================
                        测试
==============================================================*/

void ssl_test()
{
    ssl_init();
    ssl_obj_t *serverSsl = ssl_create();
    ssl_server_config(serverSsl, SSL_CERT_FILE_PATH, SSL_KEY_FILE_PATH);
    
    ssl_obj_t *clientSsl = ssl_create();
    ssl_client_config(clientSsl);

    ssl_destory(serverSsl);
    ssl_destory(clientSsl);
}



