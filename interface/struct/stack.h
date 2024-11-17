


#ifndef _STACK_H_
#define _STACK_H_



/*==============================================================
                        数据结构
==============================================================*/
typedef struct stack_member_t{
    void *d;
    char en;
}stack_member_t;


typedef struct stack_obj_t{
    int top;    //栈顶，从0开始
    int size;
    stack_member_t **array;
}stack_obj_t;


/*==============================================================
                        接口
==============================================================*/
void stack_mg_init();
void stack_test();

stack_member_t *stack_create_member();
stack_member_t *stack_create_member_set_pointer(void *data);
stack_obj_t *stack_create(int size);
void stack_destroy_member(stack_member_t *member);
void stack_destroy_member_free_data(stack_member_t *member);
void stack_destroy(stack_obj_t *stack);

int stack_push_member(stack_obj_t *stack, stack_member_t *member);
int stack_push(stack_obj_t *stack, void *data);
stack_member_t *stack_pop_member(stack_obj_t *stack);
int stack_pop(stack_obj_t *stack, void **out);
int stack_isEmpty(stack_obj_t *stack);
int stack_isFull(stack_obj_t *stack);
stack_member_t *stack_top_member_pointer(stack_obj_t *stack);



#endif

