




//队列

#include <stdio.h>
#include <string.h>

#include "queue.h"
#include "pool2.h"


/*==============================================================
                        数据结构
==============================================================*/
typedef struct queue_member_t{
    void *d;
    char en;
}queue_member_t;

//top 代表队列尾，添加时向上增长；bottom代表队列头，取出时向上增长
//top == bottom 代表没有成员；top + 1 == bottom ，代表成员已满
typedef struct queue_obj_t{ 
    int top;    
    int bottom;     
    int size;   
    queue_member_t **array;
}queue_obj_t;

/*==============================================================
                        本地管理
==============================================================*/
#define QUEUE_INIT_CODE 0x8080

typedef struct queue_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    
    int initCode;
}queue_manager_t;

queue_manager_t queue_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(queue_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define QUEUE_POOL_NAME "queue"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void queue_mg_init()
{    
    if(queue_local_mg.initCode == QUEUE_INIT_CODE)return ;   //防止重复初始化
    queue_local_mg.initCode = QUEUE_INIT_CODE;
    
    strncpy(queue_local_mg.poolName, QUEUE_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    queue_local_mg.poolId = pool_apply_user_id(queue_local_mg.poolName); 

    pool_init();    //依赖于pool2.c，所以要初始化
}


/*==============================================================
                        对象
==============================================================*/
//创建一个队列成员
queue_member_t *queue_create_member()
{
    queue_member_t *member = (queue_member_t *)MALLOC(sizeof(queue_member_t));

    if(member == NULL)return NULL;
    member->en = 0;
    return member;
}
//创建一个队列成员， 并且给定数据指针
queue_member_t *queue_create_member_set_pointer(void *data)
{
    queue_member_t *member = queue_create_member();
    if(member == NULL)return NULL;

    member->d = data;
    member->en = 1;

    return member;
}

//创建一个队列 
queue_obj_t *queue_create(int size)
{
    if(size <= 0)return NULL;
    queue_obj_t *queue = (queue_obj_t *)MALLOC(sizeof(queue_obj_t));

    if(queue == NULL)return NULL;

    queue->array = (queue_member_t **)MALLOC(sizeof(queue_member_t ) *size);

    if(queue->array == NULL)
    {
        FREE(queue);
        return NULL;
    }

    queue->bottom = 0;
    queue->top = 0;
    queue->size = size;

    return queue;
}
//摧毁一个队列成员
void queue_destroy_member(queue_member_t *member)
{
    if(member == NULL)return ;

    //释放自己
    FREE(member);
}
//摧毁一个队列
void queue_destroy(queue_obj_t *queue)
{
    if(queue == NULL)return ;

    int i;
    int pos;
    //摧毁队列的所有成员
    for(i = 0; i < queue->size; i++)
    {
        pos = (queue->bottom + i) % queue->size;
        if(pos == queue->top)
            break;
        queue_destroy_member((queue->array)[pos]);
    }

    //释放队列成员 array
    if(queue->array != NULL)FREE(queue->array);
    
    //释放自己
    FREE(queue);
}

/*==============================================================
                        应用
==============================================================*/
//判断列表是否已满
int queue_isFull(queue_obj_t *queue)
{
    if(queue == NULL)return -1;
    if((queue->top + 1) % queue->size == queue->bottom)return 1;
    return 0;
}
//判断列表是否为空
int queue_isEmpty(queue_obj_t *queue)
{
    if(queue == NULL)return -1;
    if(queue->top == queue->bottom)return 1;
    return 0;
}

//队列添加成员（入队列）
int queue_in(queue_obj_t *queue, queue_member_t *member)
{
    if(queue == NULL)return RET_FAILD;

    if(queue_isFull(queue))return RET_FAILD;

    (queue->array)[queue->top] = member;    //如果member == NULL，不会报错
    queue->top = (queue->top + 1) % queue->size;

    return RET_OK;
}
//队列添加成员，指定数据指针，需要分配内存
int queue_in_set_pointer(queue_obj_t *queue, void *data)
{
    if(queue == NULL)return RET_FAILD;
    if(queue_isFull(queue))return RET_FAILD;

    queue_member_t *member = queue_create_member_set_pointer(data);

    if(member == NULL)return RET_FAILD;

    (queue->array)[queue->top] = member;    
    queue->top = (queue->top + 1) % queue->size;

    return RET_OK;
    
}

//从队列中取出成员（出队列）
int queue_out(queue_obj_t *queue, queue_member_t **memberP)
{
    if(queue == NULL)return RET_FAILD;
    if(queue_isEmpty(queue))return RET_FAILD;
    
    *memberP = (queue->array)[queue->bottom];

    queue->bottom = (queue->bottom + 1) % queue->size;

    return RET_OK;
}
//从队列中取出成员的数据指针，并且摧毁成员
int queue_out_get_pointer(queue_obj_t *queue, void **outP)
{
    if(queue == NULL)return RET_FAILD;
    if(queue_isEmpty(queue))return RET_FAILD;

    *outP = (queue->array)[queue->bottom]->d;
    queue_destroy_member((queue->array)[queue->bottom]);
    
    queue->bottom = (queue->bottom + 1) % queue->size;

    return RET_OK;
}

//获取队列当前成员个数
int queue_get_num(queue_obj_t *queue)
{
    if(queue_isEmpty(queue))return 0;
    if(queue_isFull(queue))return queue->size - 1;   //最多为 size - 1 个

    if(queue->top > queue->bottom)return (queue->top - queue->bottom);

    return (queue->top + queue->size - queue->bottom);
}

/*==============================================================
                        测试
==============================================================*/
void __queue_show(queue_obj_t *queue)
{
    printf("-----------queue show ----------\n");
    if(queue == NULL)printf("[void]\n");

    int i;
    int pos;
    for(i = 0; i < queue->size; i++)
    {
        pos = (queue->bottom + i) % queue->size;
        if(pos == queue->top)
            break;
        int *p = (int *)((queue->array)[pos]->d);
        printf("-->i:%d data:%d\n", i, *p);
    }
}


void queue_test()
{
    int num[] = {1,5,7,4};
    
    queue_obj_t *queue = queue_create(4);
    queue_in_set_pointer(queue, (void *)(num + 0));
    queue_in_set_pointer(queue, (void *)(num + 1));
    queue_in_set_pointer(queue, (void *)(num + 2));
    queue_in_set_pointer(queue, (void *)(num + 3));

    int *out;
    queue_out_get_pointer(queue, (void **)(&out));
    
    __queue_show(queue);

    printf("队列成员个数:%d\n", queue_get_num(queue));

    pool_show();

    queue_destroy(queue);

    pool_show();

    
}




