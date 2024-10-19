



#ifndef _KEYVALUE_H_
#define _KEYVALUE_H_



#include "list.h"


/*==============================================================
                        数据结构
==============================================================*/

typedef struct keyvalue_data_t{  
     void *key;
     int keyLen;
     char keyEn;
     void *value;
     int valueLen;
     char valueEn;
}keyvalue_data_t;

typedef struct keyvalue_obj_t{  
     list_obj_t *list;
}keyvalue_obj_t;


/*==============================================================
                        接口
==============================================================*/
void keyvalue_mg_init();
void keyvalue_test();

keyvalue_data_t *keyvalue_create_data();
keyvalue_data_t *keyvalue_create_data_set_pointer(void *key, int keyLen, void *value, int valueLen);
keyvalue_data_t *keyvalue_create_data_and_malloc(int keyLen, int valueLen);
keyvalue_data_t *keyvalue_create_data_set_value(void *key, int keyLen, void *value, int valueLen);
void keyvalue_destroy_data(keyvalue_data_t *data);
keyvalue_obj_t *keyvalue_create(int size);
void keyvalue_destroy(keyvalue_obj_t *keyvalue);

int keyvalue_append_data(keyvalue_obj_t *keyvalue, keyvalue_data_t *data);
int keyvalue_append_set_value(keyvalue_obj_t *keyvalue, void *key, int keyLen, void *value, int valueLen);
int keyvalue_append_set_str(keyvalue_obj_t *keyvalue, char *key, char *value);
int keyvalue_remove(keyvalue_obj_t *keyvalue, void *key, int keyLen);
int keyvalue_remove_by_str(keyvalue_obj_t *keyvalue, char *key);
int keyvalue_get_value(keyvalue_obj_t *keyvalue, const void *key, int keyLen, void *value, int valueLen);
int keyvalue_get_value_by_str(keyvalue_obj_t *keyvalue, char *key, char *value, int valueLen);



/*==============================================================
                       特殊宏
==============================================================*/
#define KEYVALUE_FOREACH_START(keyvalue, iter) LIST_FOREACH_START((keyvalue)->list, probe){\
                                               keyvalue_data_t *(iter) = probe->data;\
                                               if((iter) != NULL && probe->en == 1){
                                                                                                                                               
#define KEYVALUE_FOREACH_END }}LIST_FOREACH_END                                           



#endif









