



#include "main.h"
#include <stdio.h>
#include <stdlib.h>



#include "log.h"
#include "pool2.h"
#include "tcp.h"


int main(int argn, char *argv[])
{
    printf("OK! my name is cpe\n");

    int port;
    log_init("/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest", "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest/debug/log2.txt");
    pool_init();

    //log_test();
    //pool_test();
    
    if(argn >= 2)
    {
        port = atoi(argv[1]);
	    tcp_client_test("192.168.1.20", port, "192.168.1.20", 8080);  
    }
	pool_show();
	
	return 0;
}


