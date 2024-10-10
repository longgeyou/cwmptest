
//单链表 link
//注：适用于体量不大的数据集；如果数据量比较大的话，为了提高搜索效率，可以考虑平衡二叉树存储方式；
//链表存储相较于顺序存储，优点是：1、适合频繁的插入、修改操作；2、长度可以随时改变一适应需求。
//约定：链表的头部不存储数据


#include "pool2.h"
#include "link.h"

#include <stdio.h>
#include <string.h>



/*============================================================
                        本地管理
==============================================================*/
typedef struct link_manager_t{
    int link_pool_id;
    char link_pool_name[POOL_USER_NAME_MAX_LEN];
    int link_num;

    int isInit; //是否初始的标志
}link_manager_t;

link_manager_t link_local_mg = {0};

//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(link_local_mg.link_pool_id, x)
#define POOL_FREE(x) pool_user_free(x)
#define LINK_POOL_NAME "link"


/*============================================================
                        xx
==============================================================*/
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1


#define LINK_INIT_CODE 0X10086
//强制初始化
void link_force_init()
{    
    strncpy(link_local_mg.link_pool_name, LINK_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    link_local_mg.link_pool_id = pool_apply_user_id(link_local_mg.link_pool_name); 
    link_local_mg.link_num = 0;

    link_local_mg.isInit = LINK_INIT_CODE;
}

//管理初始化（只能初始化一次）
void link_init()
{    
    if(link_local_mg.isInit == LINK_INIT_CODE)return;   //防止重复初始化
    link_force_init();
}


//初始化设置
int link_obj_init(link_obj_t *obj)
{
    if(obj == NULL)return RET_FAILD;
    obj->head = link_node_creat(NULL, 0);
    if(obj->head == NULL)return RET_FAILD;
    
    obj->head->next = NULL;
    link_local_mg.link_num++;
    
    obj->find_callBack = NULL;     //查找的回调函数
    obj->nodeNum = 0;

    return RET_OK;
}

//创建链表对象
link_obj_t *link_create()
{
    link_obj_t *obj = (link_obj_t *)POOL_MALLOC(sizeof(link_obj_t));
    if( RET_FAILD == link_obj_init(obj))    //分配失败
    {
        POOL_FREE(obj);
        return NULL;
    }

    return obj;
}
//创建节点
link_node_t *link_node_creat(void *data, int dataSize)
{
    link_node_t *node = (link_node_t *)POOL_MALLOC(sizeof(link_node_t ));
    if(node == NULL)return NULL;    //分配失败
    

    if(data == NULL)    //参数data是一个空指针
    {
        node->data_p = NULL;
        node->data_used = 1;
        node->data_size = 0; 
    }
    else
    {
        node->data_p = (void *)POOL_MALLOC(dataSize);
        if(node->data_p == NULL)
        {
            //printf("link_node_creat error\n");
            POOL_FREE(node);    //释放
            return NULL;    //分配失败
        }
        node->data_used = 1;
        node->data_size = dataSize;

        
        memcpy(node->data_p, data, dataSize);
        //printf("link_node_creat data: %d data_p:%d\n", *((int *)(data)), *((int *)(node->data_p)));
    }

    node->next = NULL;
    
    return node;
    
}

//增加节点
int link_appendNode(link_obj_t *obj, link_node_t *node)
{

    if(obj == NULL || node == NULL)return RET_FAILD;
    link_node_t *probe = obj->head;

    
    while(probe->next != NULL)
    {
        probe = probe->next; 
    }
    probe->next = node;
    node->next = NULL;

    //printf("==>%d\n", *((int *)(node->data_p)));
    obj->nodeNum++;
    return RET_OK;
}
//增加节点（只提供数据参数）
int link_append(link_obj_t *obj, void *data, int dataSize)
{
    link_node_t *node = link_node_creat(data, dataSize);
    if(node == NULL)return RET_FAILD;

    return link_appendNode(obj, node);
}

//插入(index 从0开始)
int link_insert(link_obj_t *obj, link_node_t *node, int index)
{
    if(obj == NULL || node == NULL)return RET_FAILD;
    link_node_t *probe = obj->head;
    int cnt = 0;
    while(probe != NULL)
    {
        if(cnt == index)
        {
            node->next = probe->next;
            probe->next = node;
            obj->nodeNum++;
            return RET_OK;
        }
        probe = probe->next; 
        cnt++;
    }
    
    return RET_FAILD;
}


//删(id号从0开始)
int link_remove_byId(link_obj_t *obj, int id)
{
    if(obj == NULL )return RET_FAILD;
    link_node_t *probe = obj->head->next;
    link_node_t *last = obj->head;
    int cnt = 0;
    while(probe != NULL)
    {
        if(cnt == id)
        {
           last->next = probe->next;
           POOL_FREE(probe); 
           obj->nodeNum--;
           return RET_OK;
        }
        last = probe;
        probe = probe->next; 
        cnt++;
    }
    return RET_FAILD;
}

//删(注，先搜索再删除；如果数据量大的话，会导致单链表搜索速度慢)
int link_remove_byNode(link_obj_t *obj, link_node_t **node_p)
{
    if(node_p == NULL)return RET_FAILD;
    if(*node_p == NULL)return RET_FAILD;
    
    link_node_t *node = *node_p;
    
    if(obj == NULL || node == NULL)return RET_FAILD;
    link_node_t *probe = obj->head->next;
    link_node_t *last = obj->head;
    
    while(probe != NULL)
    {
        if(probe == node)
        {
           last->next = probe->next;
           POOL_FREE(probe); 
           //printf("probe: %p\n", probe);
           *node_p = NULL;
           obj->nodeNum--;
           return RET_OK;
        }
        last = probe;
        probe = probe->next; 
    }
    return RET_FAILD;
}

//查找 （需要回调函数）
link_node_t *link_find(link_obj_t *obj, const void *key, int keySize)
{
    if(obj == NULL)return NULL;
    if(obj->find_callBack == NULL)return NULL;
    link_node_t *probe = obj->head->next;
    while(probe != NULL)
    {
        if(obj->find_callBack(probe, key, keySize) == 1)    //约定返回值为1代表成功
        {
            return probe;
        }
        probe = probe->next; 
    }
    return NULL;
}


//判断节点是否存在
int link_node_isExist(link_obj_t *obj, link_node_t *node)
{
     if(obj == NULL || node == NULL)return RET_FAILD;
    link_node_t *probe = obj->head->next;
    while(probe != NULL)
    {
        if(probe == node)
           return RET_EXIST;
        probe = probe->next; 
    }
    return RET_FAILD;
}

//获取节点数量
int link_size(link_obj_t *obj)
{
    if(obj == NULL)return -1;
    return obj->nodeNum;
}

//链表摧毁
void link_destory(link_obj_t **obj_p)
{
   if(obj_p == NULL || *obj_p)return ;
   link_obj_t *obj = *obj_p;
   while(obj->nodeNum != 0)
   {
        link_remove_byId(obj, obj->nodeNum);
   }
  POOL_FREE(obj);
  obj = NULL;
}

//遍历访问（这是本地测试，外调接口看头文件的 参数宏）
static void __iter_link(link_obj_t *link, link_node_t *probe)
{
    for(probe = link->head->next; probe != NULL; probe = probe->next)
    {
        printf("%p\n", probe->data_p);
    }
}

//节点内容修改
int link_node_alter(link_node_t *node, void *data, int dataLen)
{
    if(node == NULL)return RET_FAILD;
    if(dataLen <= 0)return RET_FAILD;

    if(data == NULL)dataLen = 0;
    node->data_used = 1;
    node->data_size = dataLen;
    memcpy(node->data_p, data, dataLen);

    return RET_OK;
}



//测试辅助
void link_test_aux(link_obj_t *obj)
{
    if(obj == NULL)return ;
    if(obj->head == NULL)
    {
        printf("  link_test_aux: obj->head = NULL !\n");
        return ;
    }
    
    link_node_t *probe = obj->head->next;
    while(probe != NULL)
    {
        //printf("-->%d\n", *((int *)(probe->data_p)));
        printf("-->used:%d dataSize:%d data:%d\n", probe->data_used, probe->data_size, *((int *)(probe->data_p)));
        probe = probe->next;
    }
   // printf("cnt=%d\n", link_size(obj));
}
//测试
void link_test()
{
    link_obj_t *obj = link_create();
    int a[] = {1,5,7,4};
    link_node_t *node_p;
    
    //link_node_creat(a, sizeof(int));
    node_p = link_node_creat(&(a[2]), sizeof(int));
    
    link_appendNode(obj, link_node_creat(&(a[0]), sizeof(int)));
    link_appendNode(obj, link_node_creat(&(a[1]), sizeof(int)));
    link_appendNode(obj, node_p);
    link_appendNode(obj, link_node_creat(&(a[3]), sizeof(int)));

    link_remove_byNode(obj, &node_p);
    //if(node_p == NULL)printf("---->node_p: NULL\n");
    
    link_insert(obj, link_node_creat(&(a[0]), sizeof(int)), 1);   
       
    link_test_aux(obj);
    printf("exist: %d\n", link_node_isExist(obj, node_p));


    link_node_t *p = NULL;
    __iter_link(obj, p);
}








