




#include "main.h"
#include <stdio.h>



#include "log.h"
#include "pool2.h"
#include "tcp.h"


int main()
{
	printf("OK,my name is acs!\n");

	log_init("/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest", "/mnt/hgfs/share/PRO/CWMP_TEST/cwmptest/debug/log.txt");
    pool_init();
    //log_test();
    //pool_test();
    tcp_test("192.168.1.20", 8080);
   
    
	
	return 0;
}


