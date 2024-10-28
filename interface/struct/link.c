
//单链表 link
//注：适用于体量不大的数据集；如果数据量比较大的话，为了提高搜索效率，可以考虑平衡二叉树存储方式；
//链表存储相较于顺序存储，优点是：1、适合频繁的插入、修改操作；2、长度可以随时改变一适应需求。
//约定：链表的头部不存储数据




#include <stdio.h>
#include <string.h>

#include "pool2.h"
#include "link.h"
//#include "log.h"



/*=============================================================
                        本地管理
==============================================================*/
#define LINK_INIT_CODE 0x8080

typedef struct link_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int linkCnt;

    int initCode; //是否初始的标志
}link_manager_t;

link_manager_t link_local_mg = {0};

//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(link_local_mg.poolId, x)
#define POOL_FREE(x) pool_user_free(x)
#define LINK_POOL_NAME "link"

//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1


//管理初始化（只能初始化一次）
void link_mg_init()
{    
    if(link_local_mg.initCode == LINK_INIT_CODE)return;   //防止重复初始化
    link_local_mg.initCode = LINK_INIT_CODE;
    
    strncpy(link_local_mg.poolName, LINK_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    link_local_mg.poolId = pool_apply_user_id(link_local_mg.poolName); 
    link_local_mg.linkCnt = 0;

}

/*==============================================================
                        对象
                        注：node->data 必须指向分配的动态内存
==============================================================*/
//仅仅创建节点，不分配动态内存给 数据指针
link_node_t *link_create_node()
{
    link_node_t *node = (link_node_t *)POOL_MALLOC(sizeof(link_node_t ));
    if(node == NULL)return NULL;    //分配失败
        
    node->next = NULL;
    node->data = NULL;
    node->en = 0;
    
    return node;   
}

//创建节点 数据指针设置为给定值（给定值 data 需要指向动态内存）
// 【出错记录】：新增的 node，没有给 node->next 赋值 NULL，导致遍历出错
link_node_t *link_create_node_set_pointer(void *data)
{
    //link_node_t *node = (link_node_t *)POOL_MALLOC(sizeof(link_node_t ));
    link_node_t *node = link_create_node();
    if(node == NULL)return NULL;    //分配失败
   
    node->data = data;
    if(data != NULL)node->en = 1;
        
    return node;   
}

//创建节点 分配动态内存给 数据指针
link_node_t *link_create_node_and_malloc(int size)
{
    //link_node_t *node = (link_node_t *)POOL_MALLOC(sizeof(link_node_t ));
    link_node_t *node = link_create_node();
    if(node == NULL)return NULL;    //分配失败
    
    node->data = (void *)POOL_MALLOC(size);
    if(node->data != NULL)node->en = 1;
        
    return node;   
}

//创建节点 分配动态内存给 数据指针，并且赋值给 数据指针
link_node_t *link_create_node_set_value(void *data, int dataLen)
{
    //link_node_t *node = (link_node_t *)POOL_MALLOC(sizeof(link_node_t ));
    link_node_t *node = link_create_node();
    if(node == NULL)return NULL;    //分配失败
    
    node->data = (void *)POOL_MALLOC(dataLen);
    if(node->data != NULL)  //data分配成功
    {
        node->en = 1;
        if(data != NULL && dataLen > 0)
            memcpy(node->data, data, dataLen);
    }

    
        
    return node;   
}


//仅仅销毁节点
void link_destory_node(link_node_t *node)
{
    if(node != NULL)POOL_FREE(node);
}

//销毁节点，同时销毁 数据指向的动态内存
void link_destory_node_and_data(link_node_t *node)
{
    if(node == NULL)return;
    if(node->data != NULL && node->en == 1)POOL_FREE(node->data);
    link_destory_node(node);
}

//创建链表对象
link_obj_t *link_create()
{
    link_obj_t *link = (link_obj_t *)POOL_MALLOC(sizeof(link_obj_t));

    if(link == NULL)return NULL;

    link->head = link_create_node_set_pointer(NULL);

    if(link->head == NULL)  //节点分配失败
    {
        POOL_FREE(link);
        return NULL;
    }

    link_local_mg.linkCnt++;    
    
    link->nodeNum = 0;
    
    return link;
}

//销毁链表， 同时销毁 节点、节点指向的存储数据的动态内存
//只销毁节点容器 link_node_t , 不销毁 node->data 指向的动态内存
void link_destory(link_obj_t *link)
{
    if(link == NULL)return;


    //销毁节点
    link_node_t *node = link->head;
    link_node_t *nodeNext;
    while(node != NULL)
    {
        nodeNext = node->next;
        
        //link_destory_node_and_data(node);
        link_destory_node(node);

        node = nodeNext;
        
    }

    //销毁链表
    POOL_FREE(link);
    
}

//清空链表节点，不销毁 node->data 指向的动态内存，不销毁链表头
void link_clear(link_obj_t *link)
{
    if(link == NULL)return;

    link->nodeNum = 0;
    //销毁节点
    if(link->head != NULL) 
    {
        link_node_t *node = link->head->next;
        link->head->next = NULL; //链表头的 next 必须指向空，不然又出错
        link_node_t *nodeNext;
        while(node != NULL)
        {
            nodeNext = node->next;
            
            //link_destory_node_and_data(node);
            link_destory_node(node);

            node = nodeNext;
            
        }
    }
    
}



/*==============================================================
                        应用
==============================================================*/

//链表在尾部添加 给定的节点
int link_append_node(link_obj_t *link, link_node_t *node)
{

    if(link == NULL || node == NULL)return RET_FAILD;
    
    link_node_t *probe = link->head;
    while(probe->next != NULL)
    {
        probe = probe->next; 
    }
    probe->next = node;
    node->next = NULL;

    link->nodeNum++;
    return RET_OK;
}

//仅仅创建节点，不分配动态内存给 数据指针
//然后把节点添加到链表尾部
int link_append_by_node(link_obj_t *link)
{
    if(link == NULL)return RET_FAILD;
    
    link_node_t *node = link_create_node();
    if(node == NULL)return RET_FAILD;

    return link_append_node(link, node);
}

//创建节点，分配动态内存给 数据指针
//然后把节点添加到链表尾部
int link_append_by_malloc(link_obj_t *link, int dataLen)
{
    if(link == NULL)return RET_FAILD;
    
    link_node_t *node = link_create_node_and_malloc(dataLen);
    if(node == NULL)return RET_FAILD;

    return link_append_node(link, node);
}


//创建节点 数据指针设置为给定值
//然后把节点添加到链表尾部
int link_append_by_set_pointer(link_obj_t *link, void *data)
{
    if(link == NULL)return RET_FAILD;
    
    link_node_t *node = link_create_node_set_pointer(data);
    if(node == NULL)return RET_FAILD;

    return link_append_node(link, node);
}

//创建节点，分配动态内存给 数据指针
//然后把节点添加到链表尾部
int link_append_by_set_value(link_obj_t *link, void *data, int dataLen)
{
    if(link == NULL)return RET_FAILD;
    
    link_node_t *node = link_create_node_set_value(data, dataLen);
    if(node == NULL)return RET_FAILD;

    return link_append_node(link, node);
}



//通过索引号来向 链表 插入 节点 (index 从0开始)
int link_insert(link_obj_t *link, link_node_t *node, int index)
{
    if(link == NULL || node == NULL || index < 0)return RET_FAILD;
    link_node_t *probe = link->head;
    int cnt = 0;
    while(probe != NULL)
    {
        if(cnt == index)
        {
            node->next = probe->next;
            probe->next = node;
            link->nodeNum++;
            return RET_OK;
        }
        probe = probe->next; 
        cnt++;
    }
    
    return RET_FAILD;
}

//通过索引号让 链表 删除 节点 (index 从0开始)
int link_remove_by_id(link_obj_t *link, int index)
{
    if(link == NULL || index < 0)return RET_FAILD;
    link_node_t *probe = link->head->next;
    link_node_t *last = link->head;
    int cnt = 0;
    while(probe != NULL)
    {
        if(cnt == index)
        {
           last->next = probe->next;
           
           //link_destory_node_and_data(probe);
           link_destory_node(probe);
           
           link->nodeNum--;
           return RET_OK;
        }
        last = probe;
        probe = probe->next; 
        cnt++;
    }
    return RET_FAILD;
}

//删除链表指定的某个节点（默认释放 节点 占用的动态内存, 但是不释放 node->data 指向的动态内存）
int link_remove_by_node(link_obj_t *link, link_node_t *node)
{
    if(link == NULL || node == NULL)return RET_FAILD;

    link_node_t *probe = link->head->next;
    link_node_t *last = link->head;
    
    while(probe != NULL)
    {
        if(probe == node)
        {
           last->next = probe->next;
           
           //link_destory_node_and_data(probe);
            link_destory_node(probe);
           
           link->nodeNum--;
           
           return RET_OK;
        }
        last = probe;
        probe = probe->next; 
    }
    return RET_FAILD;
}

//判断节点是否存在
int link_node_is_exist(link_obj_t *link, link_node_t *node)
{
     if(link == NULL || node == NULL)return RET_FAILD;
    link_node_t *probe = link->head->next;
    while(probe != NULL)
    {
        if(probe == node)
           return RET_EXIST;
        probe = probe->next; 
    }
    return RET_FAILD;
}

//获取节点数量
int link_size(link_obj_t *link)
{
    if(link == NULL)return -1;
    return link->nodeNum;
}

//节点内容覆盖
int link_node_alter(link_node_t *node, void *data, int dataLen)
{
    if(node == NULL )return RET_FAILD;
    if(data == NULL || dataLen <= 0)
    {
        node->data = NULL;
        return RET_OK;
    }

    if(node->en)memcpy(node->data, data, dataLen);

    return RET_OK;
}


//返回指定索引号的成员节点
link_node_t *link_get_node(link_obj_t *link, int index)
{
    if(link == NULL || index < 0 || index >= link->nodeNum)return NULL;
    int cnt;
    link_node_t *probe = link->head->next;
    cnt = 0;
    while(probe != NULL)
    {
        if(cnt == index)
            return probe;
            
        probe = probe->next; 
        cnt++;
    }
    return NULL;
}

/*==============================================================
                        测试
==============================================================*/

//遍历访问测试（这是本地测试，外调接口看头文件的 参数宏）
void __iter_link(link_obj_t *link)
{
    link_node_t *probe;

    for(probe = link->head->next; probe != NULL; probe = probe->next)
    {
        printf("%p\n", probe->data);
    }
}

//测试辅助
void link_test_aux(link_obj_t *link)
{
    int i = 0;
    printf("\n---------------------------------\n");
    LINK_FOREACH(link, probe)
    {
        printf("i:%d data:%d\n", i, *((int *)(probe->data)));
        i++;
    }
}



//测试
void link_test()
{
    printf("link_test start ...\n");

    int a[] = {1,3,7,5};
    link_obj_t *link = link_create();

    link_append_by_set_value(link, a + 0, sizeof(int));
    link_append_by_set_value(link, a + 1, sizeof(int));
    link_append_by_set_value(link, a + 2, sizeof(int));
    link_append_by_set_value(link, a + 3, sizeof(int));
    
    //__iter_link(link);
    link_test_aux(link);

    link_remove_by_node(link, link_get_node(link, 0));

    link_test_aux(link);
}








