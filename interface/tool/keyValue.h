



#ifndef _KEYVALUE_H
#define _KEYVALUE_H



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

void keyvalue_init();
void keyvalue_test();






#endif









