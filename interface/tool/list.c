

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
    
//初始化
void list_init()
{    
    strncpy(list_local_mg.list_pool_name, LIST_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    list_local_mg.list_pool_id = pool_apply_user_id(list_local_mg.list_pool_name); 
    list_local_mg.list_num = 0;
}

//创建list对象
list_obj_t *list_create(int maxSize)
{
    list_obj_t *list = (list_obj_t *)POOL_MALLOC(sizeof(list_obj_t));   
    if(list == NULL)return NULL;

    int i;
    list->maxSize = maxSize;
    list->size = 0;
    list->array = (list_member_t *)POOL_MALLOC(sizeof(list_member_t) * maxSize);
    if(list->array == NULL)
    {
        POOL_FREE(list);
        return NULL; 
    }
    for(i = 0 ; i < maxSize; i++)
    {
        (list->array)[i].en = 0;
        (list->array)[i].data = NULL;
    }
    list_local_mg.list_num++;
    return list;
}

//创建member，并赋值
//list_member_t list_create_member(void *data)
int list_member_init(list_member_t *member_p, void *data)
{
    if(member_p == NULL)return RET_FAILD;
    member_p->data = data;
    member_p->en = 1; 
    return RET_OK;
}

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

//通过已有成员添加
int list_append_member(list_obj_t *obj, list_member_t member)
{
    if(obj == NULL )return RET_FAILD;
    member.en = 1;
    int index = __find_useful(obj);
    if(index >= 0 || index < obj->maxSize)
    {
          (obj->array)[index] = member;
          obj->size++;
          return RET_OK;
    }
    return RET_FAILD;     
}
//增加成员
int list_append(list_obj_t *obj, void *data)
{
    list_member_t member;
    list_member_init(&member, data);
   return list_append_member(obj, member);
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
    list_append(obj, &(array[0]));
    list_append(obj, &(array[1]));
    list_append(obj, &(array[2]));
    list_append(obj, &(array[3]));

    list_remove(obj, 2);

    list_test_aux(obj);

}












