


#ifndef _LIST_H_
#define _LIST_H_



/*============================================================
                        数据结构
==============================================================*/
typedef struct list_member_t{ 
    void *data;
   // int id;
    char en;
    int dataSize;
}list_member_t;

typedef struct list_obj_t{
    int size;
    int maxSize;
    list_member_t *array;   
}list_obj_t;


#define LIST_INIT_CODE 0x8080


/*============================================================
                        接口
==============================================================*/
void list_init_force(); //强制初始化
void list_init();
int list_obj_init(list_obj_t *list, int size);
list_obj_t *list_create(int size);
//list_member_t list_create_member(void *data);
//int list_member_init(list_member_t *member_p, void *data);
int list_append_member(list_obj_t *obj, list_member_t member);
int list_append(list_obj_t *obj, void *data, int dataSize);
int list_remove(list_obj_t *obj, int index);
void list_remove_member(list_obj_t *obj, list_member_t *member_p);
list_member_t *list_index(list_obj_t *obj, int index);



void list_test();



/*============================================================
                        特殊宏定义
==============================================================*/



//遍历访问宏，注意参数没有做判断，例如list 可能为空，可能造成程序错误
#define LIST_FOREACH_START(list, probe) for(int i_count = 0; i_count < (list)->maxSize; i_count++){\
                                    list_member_t *(probe);\
                                    if(((list)->array)[i_count].en != 1)continue;\
                                    else{\
                                    probe = ((list)->array) + i_count;
#define LIST_FOREACH_END }}



#endif

