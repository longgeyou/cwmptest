
//字典
//用散列表的方式进行存储；映射方式由调用者提供（回调函数）；映射冲突解决方法：链表地址法
//依赖于 link.c


#include <stdio.h>
#include <string.h>
#include "pool2.h"
#include "link.h"   //要用到链表
#include "dic.h"






/*============================================================
                        本地管理
==============================================================*/
typedef struct dic_manager_t{
    int dic_pool_id;
    char dic_pool_name[POOL_USER_NAME_MAX_LEN];
    int dic_num;
}dic_manager_t;

dic_manager_t dic_local_mg = {0};
    
//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(dic_local_mg.dic_pool_id, x)
#define POOL_FREE(x) pool_user_free(x)
#define DIC_POOL_NAME "dic"
    
    
/*============================================================
                        xx
==============================================================*/
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void dic_init()
{    
    strncpy(dic_local_mg.dic_pool_name, DIC_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    dic_local_mg.dic_pool_id = pool_apply_user_id(dic_local_mg.dic_pool_name); 
    dic_local_mg.dic_num = 0;

    link_init();    //依赖于link.c，所以要初始化
}


//查找的回调函数
int __find_callBack(void *iter, const void *key, int keySize)
{
    if(iter == NULL || key == NULL)return -1;
    link_node_t* node = (link_node_t *)iter;
    dic_data_t *data = node->data_p;

    if(keySize > data->keySize)return -1;   //数据长度不够

    
    if(memcmp((const void *)(data->key), key, keySize) == 0)  //默认key是字符串？？
    {
        return 1;   //约定1表示成功
    }
    return -1;
}



int __dic_test_getIndex(const void *key, int dicSize);
//创建字典
dic_obj_t * dic_create(int size, int (*getIndex)(const void *, int))
{
    if(size <= 0)return NULL;
    dic_obj_t * dic = (dic_obj_t *)POOL_MALLOC(sizeof(dic_obj_t));
    if(dic == NULL)return NULL;
    dic->size = size;
    dic->dataCnt = 0;

    //创建链表数组
    dic->array = (link_obj_t *)POOL_MALLOC(sizeof(link_obj_t) * size);
    int i;
    for(i = 0; i < size; i++)
    {
        link_obj_init(&((dic->array)[i])) ;  
        (dic->array)[i].find_callBack = __find_callBack;
    }

    //创建key链表（用于遍历访问）【弃用】
    //link_obj_init(&(dic->keyLink));

    if(getIndex == NULL)
       dic->getIndex = __dic_test_getIndex; //默认索引获取的回调函数
    else
        dic->getIndex = getIndex;   //回调函数

    dic_local_mg.dic_num++;
    
    return dic; 
}


//加入成员
int dic_append_data(dic_obj_t *obj, dic_data_t data)
{
    if(obj == NULL || obj->getIndex == NULL)return RET_FAILD;
    int index = obj->getIndex((void *)(data.key), obj->size);

    
    //1、键值对存入链表数组
    if(index < 0 || index > obj->size)return RET_FAILD;
    link_obj_t *link = (obj->array) + index;

    int exist = 0;
    //1.1 如果关键字key存在，则修改
    LINK_FOREACH(link, node)
    {
        dic_data_t * probe= (dic_data_t *)(node->data_p);
        if((node->data_used) == 1 && probe != NULL)
        {
            if(probe->keySize == data.keySize &&
                memcmp(probe->key, data.key, data.keySize) == 0)
            {
                link_node_alter(node, (void *)(&data), sizeof(dic_data_t));
                exist = 1; 
                break;
            }
        }
        
    }
    //1.2 如果关键字key不存在，则添加
    if(exist == 0)link_append(link, (void *)(&data), sizeof(dic_data_t));

    //键值key存入一条链表【弃用】
    //if(data.used == 1)link_append(&(obj->keyLink), data.key, data.keySize);
    //else printf("dic_append_data error\n");
    
    obj->dataCnt++;

    return RET_OK;
}
//增加成员（通过键值对）
int dic_append_keyValue(dic_obj_t *obj, void *key, int keySize, void *value, int valueSize)
{
    dic_data_t data = {key, keySize, value, valueSize, 1};

    
    return dic_append_data(obj, data);   
}
//增加成员（通过键值对，其中key和value都是字符串）
int dic_append(dic_obj_t *obj, char *key, char *value)
{
    if(key == NULL || value == NULL)return RET_FAILD;
    return dic_append_keyValue(obj, key, strlen(key), value, strlen(value));
}


//由关键字得到对应的数值（valueSize 指的是value的字节数）
int dic_get_value(dic_obj_t *obj, const void *key, int keySize, void *value, int valueSize)
{
    if(obj == NULL || key == NULL || value == NULL)return RET_FAILD;
    int index = obj->getIndex(key, obj->size);
    if(index < 0 || index > obj->size)return RET_FAILD;
    
    link_node_t *node = link_find((obj->array) + index, key, keySize);
    if(node == NULL)return RET_FAILD;

    dic_data_t *data = (dic_data_t *)(node->data_p);
    memcpy(value, data->value, valueSize);
    return RET_OK;
}

//key和value都是字符串的情况下，由key 得到对应的value
/*int dic_get_strValue(dic_obj_t *obj, const char *key, char *value, int valueSize)
{
    ;
}*/

//获取索引indx的回调函数测试（可作为默认回调函数）
#define DIC_KEY_MAX_LEN  32
int __dic_test_getIndex(const void *key, int dicSize)    //默认key为字符串，不然会出错
{
    const unsigned char *str = (const unsigned char *)key;
    int totle = 0;
    int cnt = 0;
    int mark = 0;
    while(1)
    {
        if(str[cnt] == '\0')
        {
            return totle % dicSize;    //  字符串的结尾
        }
        if(cnt >= DIC_KEY_MAX_LEN)return RET_FAILD;
        if(mark == 0)
        {
            totle += str[cnt] * 256;
            mark = 1;
        }
        else
        {
            totle += str[cnt];
            mark = 0;
        }
        cnt++;
    }
    return RET_FAILD;        
}



//通过key值来找到链表节点
/*link_node_t *dic_find_node(dic_obj_t *dic, void *key, int keySize)
{
    if(dic == NULL || key == NULL || keySize == 0)return NULL;

    int index = dic->getIndex(key, dic->size);
    if(index < 0 || index > dic->size)return NULL;
    
    return link_find((dic->array) + index, key, keySize);    
}*/
//删除键值对
int dic_remove_byKey(dic_obj_t *dic, void *key, int keySize)
{
    if(dic == NULL || key == NULL || keySize == 0)return RET_FAILD;

    int index = dic->getIndex(key, dic->size);
    if(index < 0 || index > dic->size)return RET_FAILD;

    link_node_t *node = link_find((dic->array) + index, key, keySize);
    return link_remove_byNode((dic->array) + index, &node);    
}

//key 是字符串的情况下
int dic_remove(dic_obj_t *dic, char *key)
{
    return dic_remove_byKey(dic, key, strlen(key));  //由于字符串末尾是'\0'，strlen并不包含，所以可能会出现问题
}



/*============================================================
                        测试
==============================================================*/

//本地测试：遍历所有键值对；对外接口看头文件的参数宏
void __iter_dic(dic_obj_t *dic)
{
    //注意：请确保dic已经被初始化，这里只作判断了
    if(dic == NULL)return;
    for(int i_conut = 0; i_conut < dic->size; i_conut++)
    {
        LINK_FOREACH(&((dic->array)[i_conut]), probe)
        {
            dic_data_t *iter = (dic_data_t *)(probe->data_p);


            printf("---->key:%s value:%s\n", (char *)(iter->key), (char *)(iter->value));
        }
    }
}



//测试
void dic_test()
{
    int ret;
    dic_obj_t *dic = dic_create(100, __dic_test_getIndex);
    char *key[]={"userName", "password", "password2"};
    char *value[]={"monkey", "admin", "123456"};
    
    //dic_append(dic, "userName", "monkey");
    //dic_append(dic, "password", "admin");
    //dic_append(dic, "password2", "123456");
    
    dic_append(dic, key[0], value[0]);
    //dic_append(dic, key[0], value[1]);
    dic_append(dic, key[1], value[1]);
    dic_append(dic, key[2], value[2]);

    dic_remove(dic, key[1]);
    

#define BUF_SIZE 32
    char buf32[BUF_SIZE] = {0};
    ret = dic_get_value(dic, key[0], strlen(key[0]) , buf32, BUF_SIZE);
    printf("ret:%d key:%s buf:%s \n", ret, key[0], buf32);
  
    //__iter_dic(dic);
    DIC_FOREACH_START(dic, iter)
    printf("key:%s value:%s\n", (char *)(iter->key), (char *)(iter->value));
    DIC_FOREACH_END

    pool_show();

    //printf("---------------------------->mark \n");

}


