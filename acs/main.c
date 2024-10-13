




#include "main.h"
#include <stdio.h>




#include "log.h"
#include "pool2.h"
#include "tcp.h"
#include "link.h"
#include "list.h"
#include "dic.h"
#include "keyvalue.h"
#include "ssl.h"


void test();
int main()
{
	printf("OK,my name is acs!\n");

	test();
    

	
	return 0;
}


//测试
void test()
{
    //int port;
    log_init("/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest", "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest/debug/log2.txt");
    pool_init();
    link_init();
    list_init();
    dic_init();
    keyvalue_init();
    ssl_mg_init();
    tcp_init();
	
	//link_test();
	//list_test();
	//dic_test();
	//keyvalue_test();
	//ssl_test();
    tcp_test("192.168.1.20", 8080);

	pool_show();
}



