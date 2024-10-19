

/*key value键值对处理
    1、和字典dic很像，和本目录下的dic.c的区别是：dic.c 用的
        是散列表存储方式，地址冲突处理方案是 “链地址法”;
    2、键值对的实现有两种方案，一个是单个链表实现，特点是简
        单，且长度可变化；
    3、第二种方案是用线性表（参考list.c）存储法，特点也是简
        单，容量一般不可变化；
    4、决定：考虑先用线性表方式实现。
*/


#include <stdio.h>
#include <string.h>
#include "pool2.h"
#include "keyvalue.h"
#include "log.h"




/*==============================================================
                        本地管理
==============================================================*/
#define KEYVALUE_INIT_CODE 0x88

typedef struct kevalue_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int keyvalueCnt;

    int initCode;
}kevalue_manager_t;

kevalue_manager_t keyvalue_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(keyvalue_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define KEYVALUE_POOL_NAME "keyvalue"
    
   
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void keyvalue_mg_init()
{    
    if(keyvalue_local_mg.initCode == KEYVALUE_INIT_CODE)return;
    keyvalue_local_mg.initCode = KEYVALUE_INIT_CODE;

    strncpy(keyvalue_local_mg.poolName, KEYVALUE_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    keyvalue_local_mg.poolId = pool_apply_user_id(keyvalue_local_mg.poolName); 
    keyvalue_local_mg.keyvalueCnt = 0;

    list_mg_init();    //依赖于link.c，所以要初始化
}


/*==============================================================
                        对象
==============================================================*/

//创建一个 数据项，仅仅对 data 分配内存 
keyvalue_data_t *keyvalue_create_data()
{
    keyvalue_data_t *data = (keyvalue_data_t *)MALLOC(sizeof(keyvalue_data_t));

    if(data != NULL)
    {
        data->key = NULL;
        data->keyEn = 0;
        data->keyLen = 0;

        data->value = NULL;
        data->valueEn = 0;
        data->valueLen = 0;
    }

    return data;
}

//创建一个数据项， 给 data 分配内存， 
//把指定的指针赋值给 data->key、data->value （调用之前 已经分配好了内存）
keyvalue_data_t *keyvalue_create_data_set_pointer(void *key, int keyLen, void *value, int valueLen)
{
    if(key == NULL || keyLen <= 0)return NULL;
    
    keyvalue_data_t *data = (keyvalue_data_t *)MALLOC(sizeof(keyvalue_data_t));
    if(data == NULL)return NULL;

    if(key != NULL)
    {   
        data->key = key;
        data->keyEn = 1;
        data->keyLen = keyLen;
    }
        
    if(value != NULL)
    {
        data->value = value;
        data->valueEn = 1;
        data->valueLen = valueLen;
    }

    return data;
}


//创建一个数据项，对 data、data->key、data->value 分配内存
keyvalue_data_t *keyvalue_create_data_and_malloc(int keyLen, int valueLen)
{
    keyvalue_data_t *data = (keyvalue_data_t *)MALLOC(sizeof(keyvalue_data_t));
    if(data == NULL)return NULL;

    data->key = (void *)MALLOC(keyLen);
    if(data->key != NULL)   
    {
        data->keyEn = 1;
        data->keyLen = keyLen;
    }
       
    data->value = (void *)MALLOC(valueLen);
    if(data->value != NULL)
    {
        data->valueEn = 1;
        data->valueLen = valueLen;
    }

    return data;
}

//创建一个数据项，对 data、data->key、data->value 分配内存，并对 data->key、data->value 赋值
keyvalue_data_t *keyvalue_create_data_set_value(void *key, int keyLen, void *value, int valueLen)
{
    keyvalue_data_t *data = (keyvalue_data_t *)MALLOC(sizeof(keyvalue_data_t));
    if(data == NULL)return NULL;

    if(key != NULL && keyLen > 0)
    {
         data->key = (void *)MALLOC(keyLen);
        if(data->key != NULL)   
        {
            memcpy(data->key, key, keyLen);
            data->keyEn = 1;
            data->keyLen = keyLen;
        }

    }
   
    if(value != NULL && valueLen > 0)
    {
        data->value = (void *)MALLOC(valueLen);
        if(data->value != NULL)
        {
            memcpy(data->value, value, valueLen);
            data->valueEn = 1;
            data->valueLen = valueLen;
        }
    }
    

    return data;
}

//摧毁一个 数据项
void keyvalue_destroy_data(keyvalue_data_t *data)
{
    if(data == NULL)return ;

    //释放成员 key、value
    if(data->key != NULL)FREE(data->key);
    if(data->value != NULL)FREE(data->value);

    //释放自己
    FREE(data);  
}

//创建一个 keyvalue 对象
keyvalue_obj_t *keyvalue_create(int size)
{
    keyvalue_obj_t *keyvalue = (keyvalue_obj_t *)MALLOC(sizeof(keyvalue_obj_t));
    if(keyvalue == NULL)return NULL;

    keyvalue->list = list_create(size);
    if(keyvalue->list == NULL)
    {
        FREE(keyvalue);
        return NULL;
    }

    keyvalue_local_mg.keyvalueCnt++;
    return keyvalue;
}

//摧毁一个 keyvalue 对象
void keyvalue_destroy(keyvalue_obj_t *keyvalue)
{
    if(keyvalue == NULL)return ;

    //1、遍历 list 成员，释放 data
    LIST_FOREACH_START(keyvalue->list, probe)
    {
        if(probe->en == 1)
            keyvalue_destroy_data(probe->data);
    }LIST_FOREACH_END;

    //2、释放列表
    list_destory(keyvalue->list);    
    
    //3、释放自己
    FREE(keyvalue);
    
    keyvalue_local_mg.keyvalueCnt--;
}


/*==============================================================
                        应用
==============================================================*/

//键值对添加 一个数据项，这个数据项是给定的指针，指向 已经分配好动态内存的 
//注意： 字典每一条数据的 key 值唯一，如果出现key已存在的情况，则替换
int keyvalue_append_data(keyvalue_obj_t *keyvalue, keyvalue_data_t *data)
{
    //int index;
    int exist;
    
    if(keyvalue == NULL || data == NULL)return RET_FAILD;
    
    list_obj_t *list = keyvalue->list;
    if(list == NULL)return RET_FAILD;
    //查看容量
    if(list->num >= list->size)
    {
       LOG_ALARM("dic 容量");
       return RET_FAILD;
    }

    if(data->keyEn == 1 && data->key != NULL && data->keyLen > 0)
    {      
       //1 如果关键字key存在，释放原来 数据，用新的 数据 替换
       exist = 0;

        LIST_FOREACH_START(list, probe){
           keyvalue_data_t *it = (keyvalue_data_t *)(probe->data);
           if(it != NULL && it->keyEn  == 1)
           {
               if(it->keyLen == data->keyLen &&
                   memcmp(it->key, data->key, data->keyLen) == 0)
               {
                   keyvalue_destroy_data(it);
                   
                   probe->data = data;
                   exist = 1; 
                   break;
               }
           }

        }LIST_FOREACH_END;
       
       //2 如果关键字key不存在，则添加
       if(exist == 0)
       {
           list_append_member_set_poniter(list, (void *)data);
           list->num++;
       }
       return RET_OK;
    }

    return RET_FAILD;
}


//键值对添加 一个数据，数据需要开辟动态空间，然后 赋值给 key、value
int keyvalue_append_set_value(keyvalue_obj_t *keyvalue, void *key, int keyLen, void *value, int valueLen)
{   

    if(keyvalue == NULL)return RET_FAILD;

    list_obj_t *list = keyvalue->list;
    if(list == NULL)return RET_FAILD;
    

    //创建 数据
    keyvalue_data_t *data = keyvalue_create_data_set_value(key, keyLen, value, valueLen); 
    if(data == NULL)return RET_FAILD;

    //查看容量
    if(list->num >= list->size)
    {
        LOG_ALARM("dic 容量");
        return RET_FAILD;
    }

    //添加或者替换
    return keyvalue_append_data(keyvalue, data);
}

//键值对添加 一个数据，数据需要开辟动态空间，然后 赋值给 key、value
//key 和 value 默认为字符串，注意 key不能为NULL
int keyvalue_append_set_str(keyvalue_obj_t *keyvalue, char *key, char *value)
{   
    if(keyvalue == NULL || key == NULL )return RET_FAILD;
    int lenA, lenB;
    lenA = strlen(key) + 1;

    lenB = 0;
    if(value != NULL)lenB = strlen(value) + 1;
    
    return keyvalue_append_set_value(keyvalue, key, lenA, value, lenB);
}

//键值对通过 关键词key 来移除一个数据
int keyvalue_remove(keyvalue_obj_t *keyvalue, void *key, int keyLen)
{
    

    if(keyvalue == NULL || key == NULL || keyLen <= 0)return RET_FAILD;

    list_obj_t *list = keyvalue->list;
    if(list == NULL)return RET_FAILD;

    LIST_FOREACH_START(list, probe)
    {
        keyvalue_data_t *it = (keyvalue_data_t *)(probe->data);
        if(it->keyEn  == 1 && it != NULL)
        {
            if(it->keyLen == keyLen &&
                memcmp(it->key, key, keyLen) == 0)
            {
                keyvalue_destroy_data(it);
               
                list_remove(list, probe);
                
                list->num--;
                return RET_OK;
            }
        }
        
    }LIST_FOREACH_END;

    return RET_FAILD;
}

//键值对通过 关键词key 来移除一个数据
//关键词为字符串的情况
int keyvalue_remove_by_str(keyvalue_obj_t *keyvalue, char *key)
{
    return keyvalue_remove(keyvalue, (void *)key, strlen(key) + 1);   //注意 字符串后面还有一个 '\0'
}

//键值对通过 关键词key 来读取value的值
int keyvalue_get_value(keyvalue_obj_t *keyvalue, const void *key, int keyLen, void *value, int valueLen)
{
    //int index;
    int len;

    if(keyvalue == NULL || key == NULL || keyLen <= 0)return RET_FAILD;

    list_obj_t *list = keyvalue->list;
    if(list == NULL)return RET_FAILD;

    LIST_FOREACH_START(list, probe)
    {
        keyvalue_data_t *it = (keyvalue_data_t *)(probe->data);

        if(it->keyEn  == 1 && it != NULL)
        {
            if(it->keyLen == keyLen &&
                memcmp(it->key, key, keyLen) == 0)
            {
                if(value != NULL && valueLen > 0 && it->valueEn == 1)
                {
                    len = valueLen;
                    if(len > it->valueLen)len = it->valueLen;  //注意复制字节的长度，不要超过 数据的大小
                    memcpy(value, it->value, len);                    
                    return RET_OK;
                }
            }
        }
        
    }LIST_FOREACH_END;

    return RET_FAILD;

}

//键值对通过 关键词key 来读取value的值
//key 和 value 都是字符串的情况
int keyvalue_get_value_by_str(keyvalue_obj_t *keyvalue, char *key, char *value, int valueLen)
{
    return keyvalue_get_value(keyvalue, (const void *)key, strlen(key) +  1, (void *)value, valueLen);

}


/*==============================================================
                        测试
==============================================================*/
//遍历测试
void __iter_keyvalue(keyvalue_obj_t *keyvalue)
{
    if(keyvalue == NULL)return ;

    
    LIST_FOREACH_START(keyvalue->list, probe){
        keyvalue_data_t *iter = probe->data;
        if(iter != NULL && probe->en == 1)
        {
            if(iter->key != NULL && iter->keyEn == 1 && 
                iter->value != NULL && iter->valueEn)
                printf("key:%s keyLen:%d value:%s valueLen:%d\n", 
                        (char *)(iter->key), iter->keyLen, (char *)(iter->value), iter->valueLen);
        }
    }LIST_FOREACH_END;
    
}

void __keyvalue_test_aux(keyvalue_obj_t  *keyvalue)
{
    printf("---------------- keyvalue show ----------------\n");
    if(keyvalue == NULL)return ;
    KEYVALUE_FOREACH_START(keyvalue, iter){
        if(iter->key != NULL && iter->keyEn == 1 && 
                iter->value != NULL && iter->valueEn)
                printf("key:%s keyLen:%d keyEn:%d value:%s valueLen:%d valueEn:%d\n", 
                        (char *)(iter->key), iter->keyLen, iter->keyEn,
                        (char *)(iter->value), iter->valueLen, iter->valueEn);
    } KEYVALUE_FOREACH_END;
}

void keyvalue_test()
{
    printf("keyvalue  test start ...\n");
    
    keyvalue_obj_t *keyvalue = keyvalue_create(100);
    char *key[]={"userName",
                    "password",
                    "password2"};
    char *value[]={"along",
                    "123456",
                    "admin"};
    //pool_show();
    
    keyvalue_append_set_str(keyvalue, key[0], value[0]);
    keyvalue_append_set_str(keyvalue, key[1], value[1]);
    keyvalue_append_set_str(keyvalue, key[2], value[2]);

    //keyvalue_remove_by_str(keyvalue, key[0]);
     
    pool_show();
    
    //__iter_keyvalue(keyvalue);
    __keyvalue_test_aux(keyvalue);

   

//    keyvalue_append_set_str(keyvalue, key[0], value[0]);
//    keyvalue_append_set_str(keyvalue, key[1], value[1]);
//    keyvalue_append_set_str(keyvalue, key[2], value[2]);
    
    __keyvalue_test_aux(keyvalue);

    int ret;
#define BUF_SIZE 256
    char buf[BUF_SIZE] = {0};
    ret = keyvalue_get_value_by_str(keyvalue, key[1], buf, BUF_SIZE);

    if(ret == RET_OK)printf("--->value:%s\n", buf);

    keyvalue_destroy(keyvalue);

    pool_show();
}












