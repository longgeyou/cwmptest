


#ifndef _SOAP_H_
#define _SOAP_H_


#include "keyvalue.h"
#include "link.h"

#include "stack.h"
#include "log.h"


/*==============================================================
                        数据结构
==============================================================*/
#define SOAP_NAME_STRING_LEN 128
#define SOAP_VALUE_STRING_LEN 256
#define SOAP_ATTR_DEFAULT_SIZE 8
typedef struct soap_node_t{
    char name[SOAP_NAME_STRING_LEN];
    char nameEn;
    char value[SOAP_VALUE_STRING_LEN];
    char valueEn;
    keyvalue_obj_t *attr; //属性
    link_obj_t *son;    //子节点（链表）
}soap_node_t;

typedef struct soap_obj_t{
    soap_node_t *root;
    char en;
}soap_obj_t;


//基于soap封装的cwmp消息， header 元素结构体
#define SOAP_HEADER_ID_STRING_LENGTH 128
typedef struct soap_header_t{
    char ID[SOAP_HEADER_ID_STRING_LENGTH];
    char idEn;  //指示是否使用 ID
    char HoldRequests;
    char holdRequestsEn;    //指示是否使用 HoldRequests
}soap_header_t;



/*==============================================================
                        接口
==============================================================*/
void soap_mg_init();

void soap_test();
void soap_node_show(soap_node_t *node);



soap_node_t *soap_create_node(int attrSize);
soap_node_t *soap_create_node_default_size();
soap_node_t *soap_create_node_set_size(char *name, char *value, int attrSize);
soap_node_t *soap_create_node_set_value(char *name, char *value);
soap_obj_t *soap_create();
soap_obj_t *soap_create_set_value(char *name, char *value);
void soap_destroy_node(soap_node_t *node);
void soap_destroy(soap_obj_t *soap);

int soap_node_set_name_value(soap_node_t *node, char *name, char *value);
int soap_node_append_attr(soap_node_t *node, char *key, char *value);
int soap_node_append_son(soap_node_t *node, soap_node_t *son);

void soap_node_to_str(soap_node_t *node, char *buf, int *pos, int bufLen);
//void _attr_to_keyvalue(keyvalue_obj_t *keyvalue, char *str);
//void __name_attr_pro(soap_node_t *node, char *str);
soap_node_t *soap_str2node(char *str);
soap_node_t *soap_node_get_son(soap_node_t *node, char *name);




#endif

