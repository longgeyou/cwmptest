



#include "main.h"
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "pool2.h"
#include "tcp.h"
#include "link.h"
#include "list.h"
#include "dic.h"
#include "keyvalue.h"
#include "ssl.h"

#include "httphead.h"
#include "strpro.h"



void test(int);
int main(int argn, char *argv[])
{
    
	

	if(argn >= 2)
    {
	    test(atoi(argv[1]));
	}
	else
	{
	    test(0);
	}
	
	return 0;
}


//测试
void test(int port)
{
    printf("OK! my name is cpe\n");

    //int port;
    log_init("/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest", "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest/debug/log1.txt");
    pool_init();
    link_mg_init();
    list_mg_init();
    dic_mg_init();
    keyvalue_mg_init();
    ssl_mg_init();
    tcp_init();
    //httphead_mg_init();

       
    if(port > 0)
	    tcp_client_test("192.168.1.20", port, "192.168.1.20", 8080);  
    
	
	//link_test();
	//list_test();
	//dic_test();
	//keyvalue_test();
	//ssl_test();

    httphead_test();
    //strpro_test();
    
	//pool_show();
}





