﻿
#ifndef _HTTP_H_
#define _HTTP_H_



/*=============================================================
                        数据结构
==============================================================*/




/*==============================================================
                        接口
==============================================================*/
void http_mg_init();
void http_test();
void http_other_test();


void http_client_test(char *ipv4, int port, char *targetIpv4, int targetPort);



#endif
