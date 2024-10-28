
//字典
//用散列表的方式进行存储；映射方式由调用者提供（回调函数）；映射冲突解决方法：链表地址法
//依赖于 link.c


#include <stdio.h>
#include <string.h>
#include "pool2.h"
#include "link.h"   //要用到链表
#include "dic.h"
#include "log.h"





/*==============================================================
                        本地管理
==============================================================*/
#define DIC_INIT_CODE 0x8080

typedef struct dic_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int dicCnt;
    
    int initCode;
}dic_manager_t;

dic_manager_t dic_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(dic_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define DIC_POOL_NAME "dic"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void dic_mg_init()
{    
    if(dic_local_mg.initCode == DIC_INIT_CODE)return ;   //防止重复初始化
    dic_local_mg.initCode = DIC_INIT_CODE;
    
    strncpy(dic_local_mg.poolName, DIC_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    dic_local_mg.poolId = pool_apply_user_id(dic_local_mg.poolName); 
    dic_local_mg.dicCnt = 0;

    link_mg_init();    //依赖于link.c，所以要初始化
}

/*==============================================================
                        对象
                        dic数据的 key 、value指针 应该指向分配好的动态内存
==============================================================*/
//创建字典数据（有初始化）
dic_data_t *dic_data_create()
{
    dic_data_t *data = (dic_data_t *)MALLOC(sizeof(dic_data_t));

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

//创建字典数据， 并给 key 、value指针 指定已经分配好的动态内存
dic_data_t *dic_data_create_set_pointer(void *key, int keyLen, void *value, int valueLen)
{
    dic_data_t *data = dic_data_create();
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


//创建字典数据，并给 key 、value指针分配动态内存
dic_data_t *dic_data_create_and_malloc(int keyLen, int valueLen)
{
    dic_data_t *data = dic_data_create();
    if(data == NULL)return NULL;

    if(keyLen > 0)
    {
        data->key = (void *)MALLOC(keyLen);
        if(data->key != NULL)
        {
            data->keyEn = 1;
            data->keyLen = keyLen;
        }
    }

    if(valueLen > 0)
    {
        data->value = (void *)MALLOC(valueLen);
        if(data->value != NULL)
        {
            data->valueLen = valueLen;
            data->valueEn = 1;
        }
    }

    return data;
}

//创建字典数据，给 key 、value指针分配动态内存，同时给key 、value指针 赋值
dic_data_t *dic_data_create_set_value(void *key, int keyLen, void *value, int valueLen)
{
    dic_data_t *data = dic_data_create();
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

//仅仅摧毁 字典数据
void dic_destory_data(dic_data_t *data)
{
    if(data == NULL)return ;
    FREE(data);
}

//摧毁 字典数据，同时对 key、value 指针指向的动态内存进行释放
void dic_destory_data_key_value(dic_data_t *data)
{
    if(data == NULL) return ;

    if(data->key != NULL && data->keyEn == 1)
    {
        FREE(data->key);
    }

    if(data->value != NULL && data->valueEn == 1)
    {
        FREE(data->value);
    }

    FREE(data);
}


//创建字典
dic_obj_t * dic_create(int size)
{
    int i;
    
    if(size <= 0)return NULL;
    
    dic_obj_t * dic = (dic_obj_t *)MALLOC(sizeof(dic_obj_t));
    if(dic == NULL)return NULL;
    
    //创建链表数组
    dic->array = (link_obj_t **)MALLOC(sizeof(link_obj_t *) * size);
    if(dic->array != NULL)
    {
        for(i = 0; i < size; i++)
        {
            dic->array[i] = link_create(); 
        }       
    }

    dic->size = size;
    dic->num = 0;
    
    dic_local_mg.dicCnt++;
    
    return dic; 
}

//摧毁字典
void dic_destory(dic_obj_t *dic)
{
    int i;
    if(dic == NULL)return ;

    //成员
    for(i = 0; i < dic->size; i++)
    {
        if(dic->array[i] != NULL)
        {
            //1、dic_data_t 数据释放
            LINK_FOREACH(dic->array[i], probe)     //遍历
            {   
                if(probe->data != NULL)
                    dic_destory_data_key_value(probe->data);
            }
            
            //2、链表容器释放
            link_destory(dic->array[i]);
        }
    }

    //3、链表指针 数组释放
    if(dic->array != NULL)FREE(dic->array);
    
    //释放自己
    FREE(dic);

    dic_local_mg.dicCnt--;
}

/*==============================================================
                        应用
==============================================================*/
//关键字 key 到 散列表索引的映射方式
#define DIC_KEY_HASH_FACTOR 131
int __key_to_index(const void *key, int keyLen, int scope)
{
    const unsigned char *p = (const unsigned char *)key;
    unsigned long long  totle;
    //int cnt;
    int i;

    totle = p[0];
    for(i = 1; i < keyLen; i++)
    {
        totle *= DIC_KEY_HASH_FACTOR;
        totle += p[i];
        
    }

     return (int)(totle % scope);
}

//字典添加 一个数据，这个数据是给定的指针，指向 已经分配好动态内存的 
//注意： 字典每一条数据的 key 值唯一，所以出现key已存在的情况下，则替换
int dic_append_set_poniter(dic_obj_t *dic, dic_data_t *data)
{
    int index;
    int exist;

    if(dic == NULL || data == NULL)return RET_FAILD;

    //查看容量
    if(dic->num >= dic->size)
    {
        LOG_ALARM("dic 容量");
        return RET_FAILD;
    }

    if(data->keyEn == 1 && data->key != NULL && data->keyLen > 0)
    {
        index = __key_to_index((const void *)(data->key), data->keyLen, dic->size);

        //1 如果关键字key存在，释放原来 数据，用新的 数据 替换
        exist = 0;
        LINK_FOREACH(dic->array[index], probe)
        {
            dic_data_t *it = (dic_data_t *)(probe->data);
            if(it != NULL && it->keyEn  == 1 )
            {
                if(it->keyLen == data->keyLen &&
                    memcmp(it->key, data->key, data->keyLen) == 0)
                {
                    dic_destory_data_key_value(it);
                    
                    probe->data = data;
                    exist = 1; 
                    break;
                }
            }
            
        }
        
        //2 如果关键字key不存在，则添加
        if(exist == 0)
        {
            link_append_by_set_pointer(dic->array[index], (void *)data);
            dic->num++;
        }
        return RET_OK;
    }

    return RET_FAILD;
}


//字典添加 一个数据，数据需要开辟动态空间，然后 赋值给 key、value
int dic_append_set_value(dic_obj_t *dic, void *key, int keyLen, void *value, int valueLen)
{   
    //int index;
    //int exist;

    if(dic == NULL)return RET_FAILD;

    //创建 数据
    dic_data_t *data = dic_data_create_set_value(key, keyLen, value, valueLen);
    if(data == NULL)return RET_FAILD;

    //查看容量
    if(dic->num >= dic->size)
    {
        LOG_ALARM("dic 容量");
        return RET_FAILD;
    }

    //添加或者替换
    return dic_append_set_poniter(dic, data);

}

//字典添加 一个数据，数据需要开辟动态空间，然后 赋值给 key、value
//key 和 value 默认为字符串
int dic_append_set_str(dic_obj_t *dic, char *key, char *value)
{   
    if(dic == NULL)return RET_FAILD;

    return dic_append_set_value(dic, key, strlen(key) + 1, value, strlen(value) + 1);
}


//字典通过 关键词key 来移除一个数据
int dic_remove(dic_obj_t *dic, void *key, int keyLen)
{
    //int exist;
    int index;

    if(dic == NULL || key == NULL || keyLen <= 0)return RET_FAILD;

    index = __key_to_index(key, keyLen, dic->size);
    
    LINK_FOREACH(dic->array[index], probe)
    {
        dic_data_t *it = (dic_data_t *)(probe->data);
        if(it->keyEn  == 1 && it != NULL)
        {
            if(it->keyLen == keyLen &&
                memcmp(it->key, key, keyLen) == 0)
            {
                dic_destory_data_key_value(it);
                //link_destory_node(probe);
                link_remove_by_node(dic->array[index], probe);
                
                dic->num--;
                return RET_OK;
            }
        }
        
    }

    return RET_FAILD;
}

//字典通过 关键词key 来移除一个数据
//关键词为字符串的情况
int dic_remove_by_str(dic_obj_t *dic, char *key)
{
    return dic_remove(dic, (void *)key, strlen(key) + 1);   //注意 字符串后面还有一个 '\0'
}


//字典通过 关键词key 来读取value的值
int dic_get_value(dic_obj_t *dic, const void *key, int keyLen, void *value, int valueLen)
{
    int index;
    int len;

    if(dic == NULL || key == NULL || keyLen <= 0)return RET_FAILD;

    index = __key_to_index(key, keyLen, dic->size);
    
    LINK_FOREACH(dic->array[index], probe)
    {
        dic_data_t *it = (dic_data_t *)(probe->data);
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
        
    }

    return RET_FAILD;

}


//字典通过 关键词key 来读取value的值
//key 和 value 都是字符串的情况
int dic_get_value_by_str(dic_obj_t *dic, char *key, char *value, int valueLen)
{
    return dic_get_value(dic, (const void *)key, strlen(key) +  1, (void *)value, valueLen);

}




/*==============================================================
                        测试
==============================================================*/

//本地测试：遍历所有键值对；对外接口看头文件的参数宏
void __iter_dic(dic_obj_t *dic)
{
    //注意：请确保dic已经被初始化，这里只作判断了
    if(dic == NULL)return;
    for(int i_conut = 0; i_conut < dic->size; i_conut++)
    {
        LINK_FOREACH((dic->array)[i_conut], probe)
        {
            dic_data_t *iter = (dic_data_t *)(probe->data);


            printf("---->key:%s value:%s\n", (char *)(iter->key), (char *)(iter->value));
        }
    }
}


//测试辅助
void __dic_test_aux(dic_obj_t *dic)
{
    LOG_SHOW("--------------------\n");
    DIC_FOREACH_START(dic, iter){
        LOG_SHOW("key:%s value:%s\n", (char *)(iter->key), (char *)(iter->value));

    }DIC_FOREACH_END;
    
}

//测试
void dic_test()
{
    int ret;
#define BUF_SIZE 256
    char buf[BUF_SIZE] = {0};

    LOG_SHOW("dic test start ...\n");
    
    dic_obj_t *dic = dic_create(100);
    
    char *key[]={"userName", "password", "password2"};
    char *value[]={"monkey", "admin", "123456"};

    dic_append_set_str(dic, key[0], value[0]); 
    dic_append_set_str(dic, key[1], value[1]); 
    dic_append_set_str(dic, key[2], value[2]); 

    dic_append_set_str(dic, key[0], value[0]); 
    dic_append_set_str(dic, key[1], value[1]); 
    dic_append_set_str(dic, key[2], value[2]);

    pool_show();
    
    dic_remove_by_str(dic, key[2]);

    pool_show();
    
    //__iter_dic(dic);
    __dic_test_aux(dic);


    ret = dic_get_value_by_str(dic, key[2], buf, BUF_SIZE);

    printf("---->ret:%d\n", ret);
    if(ret == RET_OK)printf("value:%s\n", buf);
    
    dic_destory(dic);
    
    pool_show();

}


