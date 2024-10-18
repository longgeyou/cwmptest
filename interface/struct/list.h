


#ifndef _LIST_H_
#define _LIST_H_



/*==============================================================
                        数据结构
==============================================================*/
typedef struct list_member_t{ 
    void *data;
    char en;    //指示 data 有没有分配 动态内存
    
}list_member_t;

typedef struct list_obj_t{
    int size;
    int num;
    list_member_t **array;   //用于存储 指向成员指针 的数组
}list_obj_t;




/*==============================================================
                        接口
==============================================================*/

void list_mg_init();

void list_test();



/*==============================================================
                        特殊宏定义
==============================================================*/




//遍历访问宏，注意参数没有做判断，例如list 可能为空，可能造成程序错误
#define LIST_FOREACH_START(list, probe)   for(int i_count = 0; i_count < list->size; i_count++){\
                                            if((list)->array[i_count] != NULL){\
                                            list_member_t *(probe) = list->array[i_count];
           
#define LIST_FOREACH_END }}



#endif

