

#ifndef _HTTPHEAD_H_
#define _HTTPHEAD_H_


#include "link.h"
#include "keyvalue.h"



/*=============================================================
                        数据结构
==============================================================*/
/*
    http 头部结构
    类似于 Accept-Language: en-US,en;q=0.8 的一行数据，
    1、冒号前面是名字，后面是内容；
    2、内容用 分号 分割成多个子内容；
    3、子内容里面则是一个个用 逗号 成员，可以看为 键值对 的成员；
        键值对有的只有 key，没有value，有的两者都有
    例子里，可以理解为：
    --------------------------------------
    名字：Accept-Language
    内容：
        子内容1:
                键值对1：
                        key:空
                        value:en-US
                键值对2：
                        key:空
                        value:en
        子内容2:
                键值对1：
                        key:q
                        value:0.8
    --------------------------------------


    实现：
    1、数据结构
    --------------------------------------
    root（链表）
        每一行（名字       + 键值对列表）
    --------------------------------------
    

*/

#define LINE_NAME_STRING_SIZE 256


//每一行数据的结构
typedef struct httphead_line_t{
    char name[LINE_NAME_STRING_SIZE];
    keyvalue_obj_t *keyvalue;
}httphead_line_t;


//头部数据结构体（四层树，包括了根）
typedef struct httphead_head_t{
    link_obj_t *link;   //链表，链表节点的数据类型为 httphead_line_t 
}httphead_head_t;


/*=============================================================
                        接口
==============================================================*/

void httphead_mg_init();

void httphead_test();


httphead_line_t *httphead_create_line(int size);
void httphead_line_destroy(httphead_line_t *line);
httphead_head_t *httphead_create_head();
void httphead_destroy_head(httphead_head_t *head);
void httphead_clear_head(httphead_head_t *head);


int httphead_parse_line(httphead_line_t *line, const char *lineStr);
int httphead_parse_head(httphead_head_t *head, const char *headStr);

void httphead_show(httphead_head_t *head);
void httphead_show_line(httphead_line_t *line);



#endif


