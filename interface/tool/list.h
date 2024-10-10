


#ifndef _LIST_H_
#define _LIST_H_



/*============================================================
                        数据结构
==============================================================*/
typedef struct list_member_t{ 
    void *data;
   // int id;
    char en;
}list_member_t;

typedef struct list_obj_t{
    int size;
    int maxSize;
    list_member_t *array;   
}list_obj_t;

/*============================================================
                        接口
==============================================================*/
void list_init();
list_obj_t *list_create(int maxSize);
//list_member_t list_create_member(void *data);
int list_member_init(list_member_t *member_p, void *data);
int list_append_member(list_obj_t *obj, list_member_t member);
int list_append(list_obj_t *obj, void *data);
int list_remove(list_obj_t *obj, int index);
list_member_t *list_index(list_obj_t *obj, int index);



void list_test();


#endif

