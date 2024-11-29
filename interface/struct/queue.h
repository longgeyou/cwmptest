
#ifndef _QUEUE_H_
#define _QUEUE_H_




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
                        接口
==============================================================*/
void queue_mg_init();

void queue_test();

queue_member_t *queue_create_member();
queue_member_t *queue_create_member_set_pointer(void *data);
queue_member_t *queue_create_member_and_malloc(int size);
queue_obj_t *queue_create(int size);
void queue_destroy_member(queue_member_t *member);
void queue_destroy_member_and_data(queue_member_t *member);
void queue_destroy(queue_obj_t *queue);
void queue_destroy_and_data(queue_obj_t *queue);
void queue_clear(queue_obj_t *queue);


int queue_isFull(queue_obj_t *queue);
int queue_isEmpty(queue_obj_t *queue);
int queue_in(queue_obj_t *queue, queue_member_t *member);
int queue_in_set_pointer(queue_obj_t *queue, void *data);
int queue_out(queue_obj_t *queue, queue_member_t **memberP);
int queue_out_get_pointer(queue_obj_t *queue, void **outP);
int queue_get_head_pointer(queue_obj_t *queue, void **outP);
int queue_get_num(queue_obj_t *queue);






#define QUEUE_FOEWACH_START(queue, probe) for(int queue_i = 0; queue_i < (queue)->size; queue_i++){\
                                    int queue_tmp = ((queue)->bottom + queue_i) % (queue)->size;\
                                    if(queue_tmp == (queue)->top) break;\
                                    queue_member_t *probe = ((queue)->array)[queue_tmp];
                   
#define QUEUE_FOEWACH_END }



#endif 






