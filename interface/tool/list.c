

//list 列表
//特点：成员有限的集合；修改比较方便，一个线性表，每一个成员有使能位


#include <stdio.h>
#include <string.h>
#include "pool2.h"
#include "list.h"







/*============================================================
                        本地管理
==============================================================*/
typedef struct list_manager_t{
    int list_pool_id;
    char list_pool_name[POOL_USER_NAME_MAX_LEN];
    int list_num;

    int isInit;
}list_manager_t;

list_manager_t list_local_mg = {0};
    
    //使用内存池
#define POOL_MALLOC(x) pool_user_malloc(list_local_mg.list_pool_id, x)
#define POOL_FREE(x) pool_user_free(x)
#define LIST_POOL_NAME "list"
    
    
/*============================================================
                        xx
==============================================================*/
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1

//强制初始化
void list_init_force()
{
    strncpy(list_local_mg.list_pool_name, LIST_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    list_local_mg.list_pool_id = pool_apply_user_id(list_local_mg.list_pool_name); 
    list_local_mg.list_num = 0;
    list_local_mg.isInit = LIST_INIT_CODE;
}
    
//初始化
void list_init()
{    
    if(list_local_mg.isInit == LIST_INIT_CODE)return ;
    return list_init_force(); 
}


//list对象初始化
int list_obj_init(list_obj_t *list, int size)
{
    if(list == NULL)return RET_FAILD;

    int i;
    list->maxSize = size;
    list->size = 0;
    list->array = (list_member_t *)POOL_MALLOC(sizeof(list_member_t) * size);
    if(list->array == NULL)
    {
        POOL_FREE(list);
        return RET_FAILD; 
    }
    for(i = 0 ; i < size; i++)
    {
        (list->array)[i].en = 0;
        (list->array)[i].data = NULL;
    }
    list_local_mg.list_num++;
    
    return RET_OK;

}

//创建list对象
list_obj_t *list_create(int size)
{
    list_obj_t *list = (list_obj_t *)POOL_MALLOC(sizeof(list_obj_t));   
    //if(list == NULL)return NULL;
    if(RET_OK != list_obj_init(list, size))return NULL;

    return list;
}

//member赋值，注意member_p已经被分配了内容
/*int list_member_init(list_member_t *member_p, void *data, int dataSize)
{
    if(member_p == NULL)return RET_FAILD;

    member_p->data = data;
    
    member_p->en = 1; 
    return RET_OK;
}*/

//找到可用索引(用线性搜索法)
static int __find_useful(list_obj_t *obj)
{
    if(obj == NULL)return RET_FAILD;
    int i;
    for(i = 0; i < obj->maxSize; i++)
    {
        if((obj->array)[i].en == 0)
        {
            return i;
        }
    }
    return RET_FAILD;
}


//增加成员, 
int list_append(list_obj_t *obj, void *data, int dataSize)
{
    if(obj == NULL )return RET_FAILD;
    
    int index = __find_useful(obj);
    list_member_t *member_p = obj->array + index;
    if(index >= 0 || index < obj->maxSize)
    {
        //memcpy(member_p->data, data, dataSize);   //弃用
        //注意 member_p->data 只是指针，所以列表是指针数组，没有分配存储数据的内存空间
        member_p->data = data;
        member_p->dataSize = dataSize;
        member_p->en = 1;
        obj->size++;
        return RET_OK;
    }
    return RET_FAILD;    
}

//通过已有成员添加
int list_append_member(list_obj_t *obj, list_member_t member)
{
    return list_append(obj, member.data, member.dataSize);     
}

//删除成员
int list_remove(list_obj_t *obj, int index)
{
    if(obj == NULL)return RET_FAILD;
    if(index < 0 || index >= obj->maxSize)return RET_FAILD;
    (obj->array)[index].en = 0;
    obj->size--;
    return RET_OK; 
}

//删除成员时候，对成员的操作，member_p->data 由调用者释放内存
void list_remove_member(list_obj_t *obj, list_member_t *member_p)
{
    //不作判断了，调用的时候自己注意
    obj->size--;
    member_p->en = 0;
}

//可用成员访问
list_member_t *list_index(list_obj_t *obj, int index)
{
    if(obj == NULL)return NULL;
    if(index < 0 || index >= obj->size)return NULL;
    int i, cnt;
    cnt = 0;
    for(i = 0; i < obj->maxSize; i++)
    {
        if(cnt == index)
        {
            return (obj->array) + i; 
        }
        if((obj->array)[i].en == 1)
            cnt++;
    }
    return NULL;
}



//测试辅助
void list_test_aux(list_obj_t *obj)
{
    if(obj == NULL)return;
    int i;
    printf("---------list obj show---------\n");
    printf("maxSize=%d size=%d\n", obj->maxSize, obj->size);
    /*for(i = 0; i < obj->maxSize; i++)
    {
        if((obj->array)[i].en == 1 && (obj->array)[i].data != NULL)
        {
            printf("--->%d\n", *((int *)((obj->array)[i].data)));
        }
    }*/

   list_member_t *probe;
    for(probe = list_index(obj, 0), i = 0; probe != NULL; i++)
    {
        printf("--->%d\n", *((int *)(probe->data)));  
        probe = list_index(obj, i);
    }
    
}
//测试
void list_test()
{
    list_obj_t *obj = list_create(32);
    int array[] = {1,7,3,5};
    list_append(obj, &(array[0]), sizeof(int));
    list_append(obj, &(array[1]), sizeof(int));
    list_append(obj, &(array[2]), sizeof(int));
    list_append(obj, &(array[3]), sizeof(int));

    list_remove(obj, 2);

    list_test_aux(obj);

}












