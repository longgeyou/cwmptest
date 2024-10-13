



//平台依赖linux，接口调用




#include "linux_itf.h"

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  

#include <unistd.h>  
#include <arpa/inet.h>  


#include "log.h"



/*=============================================================
                        socket
==============================================================*/


/*----------------------------------------------
listen封装，简化为只输入 ipv4 以及端口

#include <sys/types.h>
#include <sys/socket.h>

int bind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen);
返回：成功返回fd，失败-1
----------------------------------------------*/
int LINUX_itf_bind(char *ipv4, int port, int *out)
{
    int server_fd;  
    struct sockaddr_in server_addr;  
    
    // 创建套接字  
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {  
        perror("socket");  
        return -1;  
    }  
  
    // 设置服务器地址信息  
    memset(&server_addr, 0, sizeof(server_addr));  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_addr.s_addr = inet_addr(ipv4);  
    server_addr.sin_port = htons(port);  

    *out = server_fd;
    return bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

}


/*----------------------------------------------
listen封装

#include <sys/socket.h>

int listen(int s, int backlog);
返回：成功0，失败-1
----------------------------------------------*/
int LINUX_itf_listen(int fd)
{
    return listen(fd, 5);
}

/*----------------------------------------------
accept封装

#include <sys/types.h>
#include <sys/socket.h>

int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
返回：成功fd，失败-1
----------------------------------------------*/
int LINUX_itf_accept(int fd, char *ipv4, int size, int *port)
{
    
    struct sockaddr_in addr;  
    socklen_t len = sizeof(addr);  
    //struct sockaddr addr;

    int ret;

    //LOG_SHOW("accept is comming ……\n");
    ret = accept(fd, (struct sockaddr *)&addr, &len); 
    if(ret != -1)
    {
        unsigned char *ip = (unsigned char *)(&(addr.sin_addr.s_addr));
        snprintf(ipv4, size, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

        //LOG_INFO("--------------------->%s 连接\n", ipv4);
        *port = ntohs(addr.sin_port);  
    }
    
    return ret;
}

/*----------------------------------------------
connect封装

#include <sys/types.h>          //See NOTES 
#include <sys/socket.h>

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

返回：成功0，失败-1
----------------------------------------------*/
int LINUX_itf_connect(int fd, char *ipv4, int port)
{
    //int ret;
    struct sockaddr_in addr;  
  
    // 设置服务器地址信息  
    memset(&addr, 0, sizeof(addr));  
    addr.sin_family = AF_INET;  
    addr.sin_addr.s_addr = inet_addr(ipv4);  
    addr.sin_port = htons(port);  
    
    return connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
}






/*=============================================================
                        ??
==============================================================*/













