




#include <stdio.h>
#include <string.h>
#include "stack.h"

#include "pool2.h"




/*==============================================================
                        本地管理
==============================================================*/
#define STACK_INIT_CODE 0x8080

typedef struct stack_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    
    int initCode;
}stack_manager_t;

stack_manager_t stack_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(stack_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define DIC_POOL_NAME "stack"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void stack_mg_init()
{    
    if(stack_local_mg.initCode == STACK_INIT_CODE)return ;   //防止重复初始化
    stack_local_mg.initCode = STACK_INIT_CODE;
    
    strncpy(stack_local_mg.poolName, DIC_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    stack_local_mg.poolId = pool_apply_user_id(stack_local_mg.poolName); 

    pool_init();    //依赖于pool2.c，所以要初始化
}


/*==============================================================
                        对象
==============================================================*/
//创建一个成员
stack_member_t *stack_create_member()
{
    stack_member_t *member = MALLOC(sizeof(stack_member_t));

    if(member == NULL)return NULL;
    member->d = NULL;
    member->en = 0;

    return member;
}

//创建一个成员并指定 数据指针
stack_member_t *stack_create_member_set_pointer(void *data)
{
    stack_member_t *member = stack_create_member();
    if(member == NULL)return NULL;
    member->d = data;
    member->en = 1;
    
    return member;
}

//创建一个栈对象
stack_obj_t *stack_create(int size)
{
    if(size <= 0 )return NULL;

    stack_obj_t *stack = MALLOC(sizeof(stack_obj_t));
    if(stack == NULL)return NULL;

    stack->size = size;
    stack->top = 0;

    stack->array = (stack_member_t **)MALLOC(sizeof(stack_member_t *) * size);
    if(stack->array == NULL)
    {
        FREE(stack);
        return NULL;
    }
    
    return stack;
}


//销毁一个数据成员
void stack_destroy_member(stack_member_t *member)
{
    if(member == NULL)return ;

    //释放自己
    FREE(member);
}

//销毁一个数据成员，并释放 成员里的数据指针指向的动态内存
void stack_destroy_member_free_data(stack_member_t *member)
{
    if(member == NULL)return;

    //释放member 数据指针指向的动态内存
    if(member->d != NULL && member->en == 1)FREE(member->d);

    //释放自己
    FREE(member);
}

//销毁栈
void stack_destroy(stack_obj_t *stack)
{   
    int i;
    if(stack == NULL)return;

    //释放 成员
    for(i = 0; i < stack->top && i < stack->size; i++)
    {
        stack_destroy_member((stack->array)[i]);
    }

    //释放指针数组
    if(stack->array != NULL)FREE(stack->array);

    //释放自己
    FREE(stack);
    
}



/*==============================================================
                        应用
==============================================================*/
//栈压入成员 序号从0开始
int stack_push_member(stack_obj_t *stack, stack_member_t *member)
{
    if(stack == NULL)return RET_FAILD;

    if(stack->top >= 0 && stack->top < (stack->size - 1))
    {
        (stack->array)[stack->top] = member;    //这里可以允许 member为NULL
        stack->top++;
        return RET_OK;
    }

    return RET_FAILD;
}

//栈压入 数据，会新建成员
int stack_push(stack_obj_t *stack, void *data)
{
    if(stack == NULL)return RET_FAILD;
    stack_member_t *member;
    
    member = stack_create_member_set_pointer(data);
    if(member != NULL && member->en == 1)
    {
        return stack_push_member(stack, member);
    }
    
    return RET_FAILD;
}


//栈取出成员
stack_member_t *stack_pop_member(stack_obj_t *stack)
{
    if(stack == NULL)return NULL;
    if(stack->top <= 0 || stack->top >= stack->size)return NULL;
    
    stack->top--;
    return (stack->array)[stack->top];   
}


//栈取出数据并释放成员内存
int stack_pop(stack_obj_t *stack, void **out)
{
    stack_member_t *member = stack_pop_member(stack);
    if(member == NULL)
    {
        *out = NULL;
        return RET_FAILD;  
    }

    *out = member->d;
    
    stack_destroy_member(member);
    return RET_OK;
}

//空栈判断
int stack_isEmpty(stack_obj_t *stack)
{
    if(stack == NULL)return -1;

    if(stack->top == 0)return 1;
    return 0;
}

//满栈判断
int stack_isFull(stack_obj_t *stack)
{
    if(stack == NULL)return -1;

    if(stack->top == (stack->size - 1))return 1;
    return 0;
}


//取得栈顶元素的指针
stack_member_t *stack_top_member_pointer(stack_obj_t *stack)
{
    if(stack == NULL)return NULL;
    if(stack->top <= 0)return NULL;
    return (stack->array)[stack->top - 1];
}


/*==============================================================
                        测试
==============================================================*/
void __test_aux(stack_obj_t *stack)
{
    if(stack == NULL)
    {
        printf("stack is NULL\n");
        return ;
    }

    void *p;
    p = NULL;
    int ret;
    while(stack_isEmpty(stack) != 1)
    {
        ret = stack_pop(stack, &p);
        if(ret == RET_OK && p != NULL)
        {
            printf("-->value:%d top:%d\n", *((int *)p), stack->top);
        }
        else
        {
            printf("-->faild top:%d\n", stack->top);
        }
    }
}



void stack_test()
{

    //printf("this is stack test!\n");
    int num[] = {1,4,7,3};
    //int ret;
        
    stack_obj_t *stack = stack_create(10);

    
    stack_push(stack, (void *)(num + 0));
    stack_push(stack, (void *)(num + 1));
    stack_push(stack, (void *)(num + 2));
    stack_push(stack, (void *)(num + 3));
    //printf("ret:%d top:%d size:%d\n", ret, stack->top, stack->size);

    pool_show();
    __test_aux(stack);

    stack_destroy(stack);
    pool_show(); 
}






