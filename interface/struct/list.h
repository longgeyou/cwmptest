


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



list_member_t * list_create_member();
list_member_t * list_create_member_and_malloc(int size);
list_member_t * list_create_member_set_value(void *data, int dataLen);
list_member_t *list_create_member_set_pointer(void *data);
void list_destory_member(list_member_t *member);
void list_destory_member_and_data(list_member_t *member);
list_obj_t *list_create(int size);
void list_destory(list_obj_t *list);
void list_destory_and_data(list_obj_t *list);

//int __get_useful_index(list_obj_t *list);
int list_append_member(list_obj_t *list, list_member_t *member);
int list_append(list_obj_t *list);
int list_append_member_and_malloc(list_obj_t *list, int dataLen);
int list_append_member_set_value(list_obj_t *list, void *data, int dataLen);
int list_append_member_set_poniter(list_obj_t *list, void *data);
int list_remove(list_obj_t *list, list_member_t *member);
int list_remove_by_num(list_obj_t *list, int num);
int list_remove_and_data_by_num(list_obj_t *list, int num);
list_member_t *list_find_by_num(list_obj_t *list, int num);



/*==============================================================
                        特殊宏定义
==============================================================*/




//遍历访问宏，注意参数没有做判断，例如list 可能为空，可能造成程序错误
#define LIST_FOREACH_START(list, probe)   for(int list_count = 0; list_count < (list)->size; list_count++){\
                                            list_member_t *(probe) = (list)->array[list_count];\
                                            if(probe != NULL && probe->en == 1){
                                            
           
#define LIST_FOREACH_END }}



#endif

