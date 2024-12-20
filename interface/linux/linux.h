




#ifndef __LINUX_H
#define __LINU_H



#include <poll.h>


/*==============================================================
                        socket
==============================================================*/
#include <unistd.h>  
#include <arpa/inet.h>  
#include <unistd.h>



/*==============================================================
                        ssl
==============================================================*/

#include <openssl/ssl.h>
#include <openssl/err.h>
/*----------------------------------------------
SSL_load_error_strings
OpenSSL_add_ssl_algorithms
EVP_cleanup
TLS_server_method
TLS_client_method
SSL_CTX_new
SSL_CTX_set_verify
SSL_CTX_use_certificate_file
SSL_CTX_use_PrivateKey_file
SSL_CTX_free
SSL_new
SSL_shutdown
SSL_free
SSL_set_fd
----------------------------------------------*/

/*==============================================================
                        线程
==============================================================*/
#include <pthread.h>
#include <semaphore.h>  //信号量




#endif







