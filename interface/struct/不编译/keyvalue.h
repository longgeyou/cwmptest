



#ifndef _KEYVALUE_H_
#define _KEYVALUE_H_



#include "list.h"


/*============================================================
                        数据结构
==============================================================*/

typedef struct keyvalue_data_t{  
     void *key;
     int keySize;
     void *value;
     int valueSize;
     char en;
}keyvalue_data_t;

typedef struct keyvalue_obj_t{  
     list_obj_t list;
     int listEn;
}keyvalue_obj_t;


/*============================================================
                        接口
==============================================================*/

void keyvalue_data_destroy(keyvalue_data_t *data);
void keyvalue_obj_destory(keyvalue_obj_t *keyvalue);


void keyvalue_init();
void keyvalue_test();


//创建kevalue 对象，并初始化
keyvalue_obj_t *keyvalue_create(int size);
//设置 数据 的数值
int keyvalue_data_set(keyvalue_data_t *data, void *key, int keySize, void *value, int valueSize);
//创建一个 数据，并且赋值
keyvalue_data_t *keyvalue_data_create(void *key, int keySize, void *value, int valueSize);
//添加已有的 数据 
int keyvalue_append_data(keyvalue_obj_t *keyvalue, keyvalue_data_t *data_p);
//通过给定的key 和 value，创建并添加 数据
int keyvalue_append(keyvalue_obj_t *keyvalue, void *key, int keySize, void *value, int valueSize);
//key和value都是字符串的情况下，添加 数据
int keyvalue_strAppend(keyvalue_obj_t *keyvalue, char *key, char *value);
//通过关键字key取得数值value
int keyvalue_get_value(keyvalue_obj_t *keyvalue, void *key, int keySize, void *value, int valueSize);
//通过关键字key 删除list数据项，需要释放内存
int keyvalue_remove(keyvalue_obj_t *keyvalue, void *key, int keySize);
//key和value都是字符串的情况下，删除 成员
void keyvalue_strRemove(keyvalue_obj_t *keyvalue, char *key);





#endif









