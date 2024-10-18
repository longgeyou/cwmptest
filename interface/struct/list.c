




//list 列表
//特点：成员有限的集合；修改比较方便，一个线性表，每一个成员有使能位

#include <stdio.h>
#include <string.h>
#include "pool2.h"
#include "list.h"



/*==============================================================
                        本地管理
==============================================================*/
#define LIST_INIT_CODE 0x8080

typedef struct list_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int listCnt;

    int initCode;
}list_manager_t;

list_manager_t list_local_mg = {0};
    
    //使用内存池
#define MALLOC(x) pool_user_malloc(list_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define LIST_POOL_NAME "list"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1

//强制初始化
void list_init_force()
{
   
}
    
//初始化
void list_mg_init()
{    
    if(list_local_mg.initCode == LIST_INIT_CODE)return ;
    list_local_mg.initCode = LIST_INIT_CODE;
    
     strncpy(list_local_mg.poolName, LIST_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    list_local_mg.poolId = pool_apply_user_id(list_local_mg.poolName); 
    list_local_mg.listCnt = 0;
    
}

/*=============================================================
                        对象
                        list_member_t 对象的 data 成员，应该指向 动态分配内存
=============================================================*/
//仅仅创建 成员 对象
list_member_t * list_create_member()
{
    list_member_t *member = (list_member_t *)MALLOC(sizeof(list_member_t));
    if(member != NULL)
        member->en = 0;
    
    
    return member;
}

//创建成员对象，并且给 member->data 成员分配动态内存
list_member_t * list_create_member_and_malloc(int size)
{
    list_member_t *member = list_create_member();
    if(member == NULL || size <= 0)return NULL;

    member->data = (void *)MALLOC(size);
    if(member->data != NULL)
        member->en = 1;
        
    return member;
}


//创建成员对象，给 member->data 成员分配动态内存
//并且给 data 赋值
list_member_t * list_create_member_set_value(void *data, int dataLen)
{
    list_member_t *member = list_create_member_and_malloc(dataLen);

    if(member != NULL && member->data != NULL &&
        member->en == 1 && dataLen > 0)     //条件判断
    {
        memcpy(member->data, data, dataLen);
    }
        
    return member;
}


//创建成员对象，给定 已经分配好的动态内存 的data指针
list_member_t *list_create_member_set_pointer(void *data)
{
    list_member_t *member = list_create_member();
    if(member != NULL)
    {
        member->en = 1;
        member->data = data;
    }

    return member;
}


//仅仅 成员摧毁
void list_destory_member(list_member_t *member)
{
    if(member != NULL)
        FREE(member);
}

//成员摧毁，并且释放 data 指向的数据
void list_destory_member_and_data(list_member_t *member)
{
    if(member == NULL)return ;
    // data 释放
    if(member->data != NULL && member->en == 1)
        FREE(member->data);
    //释放自己
    FREE(member);
}

//列表对象 创建
list_obj_t *list_create(int size)
{
    int i;
    list_obj_t *list = (list_obj_t *)MALLOC(sizeof(list_obj_t));
    if(list == NULL || size <= 0)return NULL;

    //指针数组
    list->array = (list_member_t **)MALLOC(sizeof(list_member_t *) * size);   
    
    if(list->array == NULL)
    {
        FREE(list);
        return NULL;
    }
  
    for(i = 0; i < size; i++)
    {
        list->array[i] = NULL;  //初始化
    }
    

    list->size = size;
    list->num = 0;
    
    return list;
}

//列表对象摧毁 释放member成员，但是不释放 member->data 
void list_destory(list_obj_t *list)
{
    int i;

    if(list == NULL)return ;
    
    //成员的释放
    if(list->array != NULL && list->size > 0)
    {
        for(i = 0; i < list->size; i++)
        {
            if(list->array[i] != NULL && list->array[i]->en == 1)
                list_destory_member(list->array[i]);
        }        
    }

    //释放自己
    FREE(list);
}

//列表对象摧毁，释放member、 member->data 
void list_destory_and_data(list_obj_t *list)
{
    int i;

    if(list == NULL)return ;
    
    //成员的释放
    if(list->array != NULL && list->size > 0)
    {
        for(i = 0; i < list->size; i++)
        {
            //1、释放member、 member->data 
            if(list->array[i] != NULL && list->array[i]->en == 1)
                list_destory_member_and_data(list->array[i]);
        }    

        //2、释放指针数组
        if(list->array != NULL)FREE(list->array);
    }

    //释放自己
    FREE(list);
}





/*=============================================================
                        应用
=============================================================*/
//找到可用成员在 指针数组 中的索引
int __get_useful_index(list_obj_t *list)
{
    int i;
    if(list == NULL || list->num >= list->size)return RET_FAILD;
    if(list->array == NULL)return RET_FAILD;
    
    for(i = 0; i < list->size; i++)
    {
        if(list->array[i] == NULL)
            return i;
    }
    return RET_FAILD;
}

//列表成员新增，给定已经存在 member
//如果成功，则返回 指针数组 的索引
int list_append_member(list_obj_t *list, list_member_t *member)
{
    int index;
    if(list == NULL)return RET_FAILD;
    if(list->array == NULL)return RET_FAILD;
    
    index = __get_useful_index(list);
    if(index >= 0 && index < list->size)
    {
        list->array[index] = member;
        if(list->array[index] != NULL )
        {
            list->num++;
            return RET_OK;
        }
    }
    
    return RET_FAILD;
}

//列表成员新增，仅仅 member 分配动态内存
//如果成功，则返回 指针数组 的索引
int list_append(list_obj_t *list)
{
    return list_append_member(list, list_create_member());
}

//列表成员新增，给 member、member->data 分配动态内存
//如果成功，则返回 指针数组 的索引
int list_append_member_and_malloc(list_obj_t *list, int dataLen)
{
    return list_append_member(list, list_create_member_and_malloc(dataLen));   
}

//列表成员新增，给 member、member->data 分配动态内存，并且对 member->data 赋值
//如果成功，则返回 指针数组 的索引
int list_append_member_set_value(list_obj_t *list, void *data, int dataLen)
{
    return list_append_member(list, list_create_member_set_value(data, dataLen));   
}

//列表成员新增，给 member 分配动态内存，把已经分配好动态内存的 data 指针给到 member->data 
//如果成功，则返回 指针数组 的索引
int list_append_member_set_poniter(list_obj_t *list, void *data)
{
    return list_append_member(list, list_create_member_set_pointer(data));   
}


//列表移除成员，释放 member，但是不释放 member->data（调用者释放）
int list_remove(list_obj_t *list, list_member_t *member)
{
    int i;

    if(list == NULL && member == NULL)return RET_FAILD;

    for(i = 0; i < list->size; i ++)
    {
        if(list->array[i] == member)
        {
            list_destory_member(member);
            list->array[i] = NULL;
            return RET_OK;
        }
            
    }
    return RET_FAILD;
}


//列表 通过成员序号 移除成员（序号从0开始）
//释放 member，但是不释放 member->data（调用者释放）
int list_remove_by_num(list_obj_t *list, int num)
{
    int i;
    int cnt;

    if(list == NULL)return RET_FAILD;

    cnt = 0;
    for(i = 0; i < list->size; i ++)
    {
        if(list->array[i] != NULL)
        {
            if(cnt == num)
            {
                list_destory_member(list->array[i]);
                list->array[i] = NULL;
                return RET_OK;
            }
            cnt++;
        }            
    }
    return RET_FAILD;
}

//列表 通过成员序号 移除成员（序号从0开始）
//释放 member、member->data
int list_remove_and_data_by_num(list_obj_t *list, int num)
{
    int i;
    int cnt;

    if(list == NULL)return RET_FAILD;

    cnt = 0;
    for(i = 0; i < list->size; i ++)
    {
        if(list->array[i] != NULL)
        {
            if(cnt == num)
            {
                list_destory_member_and_data(list->array[i]);
                list->array[i] = NULL;
                return RET_OK;
            }
            cnt++;
        }            
    }
    return RET_FAILD;
}



//列表 通过成员序号 查找 member（序号从0开始）
list_member_t *list_find_by_num(list_obj_t *list, int num)
{
    int i;
    int cnt;

    if(list == NULL)return NULL;

    cnt = 0;
    for(i = 0; i < list->size; i ++)
    {
        if(list->array[i] != NULL)
        {
            if(cnt == num)
            {
               
                return list->array[i];
            }
            cnt++;
        }                  
    }
    return NULL;
}


/*=============================================================
                        测试
=============================================================*/

//遍历测试
void __iter_test(list_obj_t *list)
{
    for(int i_count = 0; i_count < list->size; i_count++)
    {
        if(list->array[i_count] != NULL)
        {
            list_member_t *probe = list->array[i_count];
            printf("%d\n", *((int *)(probe->data)));
        }
    }
}


//测试辅助
void list_test_aux(list_obj_t *list)
{
    
    printf("---------list show---------\n");
    if(list == NULL)return;
    
    LIST_FOREACH_START(list, probe){
        printf("%d\n", *((int *)(probe->data)));
    }LIST_FOREACH_END;
    
    
    
}


//测试
void list_test()
{
    int a[] = {1,5,7,4};
    
    printf("list test start ...\n");
    

    list_obj_t *list = list_create(100);

    list_append_member_set_value(list, (void *)(a + 0), sizeof(int));
    list_append_member_set_value(list, (void *)(a + 1), sizeof(int));
    list_append_member_set_value(list, (void *)(a + 2), sizeof(int));
    list_append_member_set_value(list, (void *)(a + 3), sizeof(int));

    list_remove_and_data_by_num(list, 1);

    list_member_t *member = list_find_by_num(list, 2);
    if(member != NULL)printf("member:%p data:%d\n", member, *((int *)(member->data)));
    
    list_append_member_set_value(list, (void *)(a + 0), sizeof(int));
    list_append_member_set_value(list, (void *)(a + 1), sizeof(int));
    list_append_member_set_value(list, (void *)(a + 2), sizeof(int));
    list_append_member_set_value(list, (void *)(a + 3), sizeof(int));
    
    pool_show();
    
    //__iter_test(list);
    
    list_test_aux(list);

    
    
    list_destory_and_data(list);

    pool_show();

    

}












