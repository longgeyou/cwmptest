

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
#include "list.h"
#include "keyvalue.h"







/*============================================================
                        本地管理
==============================================================*/
typedef struct kevalue_manager_t{
    int pool_id;
    char pool_name[POOL_USER_NAME_MAX_LEN];
    int instance_num;
}kevalue_manager_t;

kevalue_manager_t keyvalue_local_mg = {0};
    
//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(keyvalue_local_mg.pool_id, x)
#define POOL_FREE(x) pool_user_free(x)
#define KEYVALUE_POOL_NAME "keyvalue"
    
    
/*============================================================
                        xx
==============================================================*/
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void keyvalue_init()
{    
    strncpy(keyvalue_local_mg.pool_name, KEYVALUE_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    keyvalue_local_mg.pool_id = pool_apply_user_id(keyvalue_local_mg.pool_name); 
    keyvalue_local_mg.instance_num = 0;

    list_init();    //依赖于link.c，所以要初始化
}


//创建kevalue 对象，并初始化
keyvalue_obj_t *keyvalue_create(int size)
{
    int ret;

    keyvalue_obj_t *keyvalue = (keyvalue_obj_t *)POOL_MALLOC(sizeof(keyvalue_obj_t));
    if(keyvalue == NULL)return NULL;

    ret = list_obj_init(&(keyvalue->list), size);
    if(ret != RET_OK)
    {
        POOL_FREE(keyvalue);
        return NULL;
    }
    
    keyvalue->listEn = 1;   //有待考虑
    keyvalue_local_mg.instance_num++;

    return keyvalue; 
}

//设置 数据 的数值
int keyvalue_data_set(keyvalue_data_t *data, void *key, int keySize, void *value, int valueSize)
{
    if(data == NULL)return RET_FAILD;
    if(data->en != 1)return RET_FAILD;
    
    //关键字key
    if(key != NULL && keySize > 0)
    {
        memcpy(data->key, key, keySize);
        data->keySize = keySize;
    }
    else
    {
        return RET_FAILD;
    }

    //数值value
    if(value != NULL && valueSize > 0)
    {      
        memcpy(data->value, value, valueSize);
        data->valueSize = valueSize; 
    }
    else
    {
        data->value = NULL;
        data->valueSize = 0;  
    }
       
    return RET_OK;
}

//创建一个 数据，并且赋值
keyvalue_data_t *keyvalue_data_create(void *key, int keySize, void *value, int valueSize)
{
    if(key == NULL || keySize <= 0)return NULL;

    keyvalue_data_t *data = (keyvalue_data_t *)POOL_MALLOC(sizeof(keyvalue_data_t));
    if(data == NULL)return NULL;

    //关键字的创建
    data->key = POOL_MALLOC(keySize);
    if(data->key == NULL)
    {
        POOL_FREE(data);
        return NULL;
    }

    //数值value的创建
    data->value = POOL_MALLOC(valueSize);
    if(data->value == NULL)
    {
        POOL_FREE(data->key);
        POOL_FREE(data);
 
        return NULL;
    }

    //已经成功分配内存的标志
    data->en = 1;

    keyvalue_data_set(data, key, keySize, value, valueSize);
       
    return data;
}

//添加已有的 数据 
int keyvalue_append_data(keyvalue_obj_t *keyvalue, keyvalue_data_t *data_p)
{
    if(keyvalue == NULL)return RET_FAILD;
    if(data_p == NULL)return RET_FAILD;
    if(data_p->en != 1)return RET_FAILD;    //data_p没有分配内存，则不可用 
    
    list_obj_t *list = &(keyvalue->list);
    int ret = RET_FAILD;
    //1、键值对存入列表   
    //1.1 如果关键字key存在，则修改
    keyvalue_data_t *tmpData;
    int isExist = 0;
    
    LIST_FOREACH_START(list, probe)   //遍历列表
    {
        tmpData = (keyvalue_data_t *)(probe->data);
        if(data_p->key != NULL &&
            tmpData->key != NULL &&
            tmpData->en == 1 &&
            tmpData->keySize > 0 &&
            tmpData->keySize == data_p->keySize)    //可行性判断
        {
            if(memcmp(tmpData->key, data_p->key, data_p->keySize) == 0) //匹配成功
            {            
                if(data_p->value != NULL &&  data_p->valueSize > 0)
                memcpy(tmpData->value, data_p->value, data_p->valueSize);
                isExist = 1;               
                break;
            }
        }
        
    }LIST_FOREACH_END

    //1.2 如果关键字key不存在，则添加
    if(isExist == 0)
    {
        ret = list_append(list, (void *)data_p, sizeof(keyvalue_data_t));
    }
    
    return ret;
}


//通过给定的key 和 value，创建并添加 数据
int keyvalue_append(keyvalue_obj_t *keyvalue, void *key, int keySize, void *value, int valueSize)
{
    if(keyvalue == NULL)return RET_FAILD;
    int ret;
    keyvalue_data_t *data_p = keyvalue_data_create(key, keySize, value, valueSize);
    ret = keyvalue_append_data(keyvalue, data_p);
    
    return ret;
}

//key和value都是字符串的情况下，添加 数据
int keyvalue_strAppend(keyvalue_obj_t *keyvalue, char *key, char *value)
{  
    //printf("------------------- mark 1\n");   
    return keyvalue_append(keyvalue, key, strlen(key), value, strlen(value));
    //printf("------------------- mark 2\n");
}


//通过关键字key取得数值value
int keyvalue_get_value(keyvalue_obj_t *keyvalue, void *key, int keySize, void *value, int valueSize)
{
    if(keyvalue == NULL || key == NULL || keySize <= 0)return RET_FAILD;

    keyvalue_data_t *tmpData;
    list_obj_t *list = &(keyvalue->list);
    
    LIST_FOREACH_START(list, probe)   //遍历列表
    {
        tmpData = (keyvalue_data_t *)probe->data;
        if(tmpData->keySize == keySize)
        {
            if(memcmp(tmpData->key, key, keySize) == 0) //匹配成功
            {            
                if(tmpData->value != NULL && tmpData->valueSize > 0)
                    memcpy(value, tmpData->value, tmpData->valueSize);
                return RET_OK;
            }
        }
        
    }LIST_FOREACH_END
    return RET_FAILD;
}


//通过关键字key 删除list数据项，需要释放内存
int keyvalue_remove(keyvalue_obj_t *keyvalue, void *key, int keySize)
{
    if(keyvalue == NULL || key == NULL || keySize <= 0)return RET_FAILD;

    keyvalue_data_t *tmpData;
    list_obj_t *list = &(keyvalue->list);
    
    LIST_FOREACH_START(list, probe)   //遍历列表
    {
        tmpData = (keyvalue_data_t *)probe->data;
        if(tmpData->keySize == keySize)
        {
            if(memcmp(tmpData->key, key, keySize) == 0) //匹配成功
            {            
                list_remove_member(list, probe);
                POOL_FREE(tmpData);
                return RET_OK;
            }
        }
        
    }LIST_FOREACH_END
    return RET_FAILD;
}
//key和value都是字符串的情况下，删除 成员
void keyvalue_strRemove(keyvalue_obj_t *keyvalue, char *key)
{
    keyvalue_remove(keyvalue, key, strlen(key));
}



void __keyvalue_test_aux(keyvalue_obj_t  *keyvalue)
{
    printf("---------------- mark 1\n");
    if(keyvalue == NULL)return ;
    keyvalue_data_t *data;
    LIST_FOREACH_START(&(keyvalue->list), probe)
    {
        data = (keyvalue_data_t *)probe->data;
        
        printf("key:%s value:%s\n", (char *)data->key, (char *)data->value);
    } LIST_FOREACH_END
}

void keyvalue_test()
{
    keyvalue_obj_t *keyvalue = keyvalue_create(100);
    char *key[]={"userName",
                    "password",
                    "password2"};
    char *value[]={"along",
                    "123456",
                    "admin"};

    keyvalue_strAppend(keyvalue, key[0], value[0]);
    keyvalue_strAppend(keyvalue, key[1], value[1]);
    //keyvalue_strAppend(keyvalue, key[2], value[2]);

    keyvalue_strAppend(keyvalue, key[0], value[0]);
    keyvalue_strAppend(keyvalue, key[1], value[1]);
    //keyvalue_strAppend(keyvalue, key[2], value[2]);
    
    keyvalue_strRemove(keyvalue, key[2]);

    __keyvalue_test_aux(keyvalue);
    
    
    //pool_show();
}












