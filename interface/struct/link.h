
#ifndef _LINK_H_
#define _LINK_H_










/*=============================================================
                        数据结构
==============================================================*/



typedef struct link_node_t{  //链表节点元素
    void *data; //存放数据的指针
    char en;   //用于指示data_p是否分配了动态内存
    //int data_size;     //用于指示data_p内存的大小
    struct link_node_t *next;  
}link_node_t;

typedef struct link_obj_t{  //链表对象
    //struct link_obj_t *me;
    link_node_t *head;
    int nodeNum;

    //int (*find_callBack)(void *, const void *, int);   //回调函数，用于链表节点的查找
}link_obj_t;






/*=============================================================
                        接口
==============================================================*/
void link_mg_init();

void link_test();

link_node_t *link_create_node();
link_node_t *link_create_node_set_pointer(void *data);
link_node_t *link_create_node_and_malloc(int size);
link_node_t *link_create_node_set_value(void *data, int dataLen);
void link_destory_node(link_node_t *node);
void link_destory_node_and_data(link_node_t *node);
link_obj_t *link_create();
void link_destory(link_obj_t *link);
void link_destroy_and_data(link_obj_t *link);
void link_clear(link_obj_t *link);


int link_append_node(link_obj_t *link, link_node_t *node);
int link_append_by_node(link_obj_t *link);
void *link_append_by_malloc(link_obj_t *link, int dataLen);
int link_append_by_set_pointer(link_obj_t *link, void *data);
int link_append_by_set_value(link_obj_t *link, void *data, int dataLen);
int link_insert(link_obj_t *link, link_node_t *node, int index);
int link_remove_by_id(link_obj_t *link, int index);
int link_remove_by_node(link_obj_t *link, link_node_t *node);
int link_node_is_exist(link_obj_t *link, link_node_t *node);
int link_size(link_obj_t *link);
int link_node_alter(link_node_t *node, void *data, int dataLen);
link_node_t *link_get_node(link_obj_t *link, int index);



/*=============================================================
                        特殊宏定义
==============================================================*/



//遍历访问宏，注意参数没有做判断，例如link和(link)->head可能为空，可能造成程序错误
#define LINK_FOREACH(link, probe) for(link_node_t *(probe) = (link)->head->next; \
                                        (probe) != NULL; (probe) = (probe)->next)

//遍历访问数据
#define LINK_DATA_FOREACH_START(link, probeData) for(link_node_t *(linkprobe) = (link)->head->next; \
                                        (linkprobe) != NULL; (linkprobe) = (linkprobe)->next){   \
                                        if((linkprobe)->en == 1 && (linkprobe)->data != NULL){  \
                                        void *probeData = (linkprobe)->data;    
                                        
#define LINK_DATA_FOREACH_END   }}

//遍历访问，多了参数，表示 上一个
#define LINK_FOREACH_LAST(link, probe, last) for(link_node_t *(probe) = (link)->head->next, (last) = (link)->head; \
                                        (probe) != NULL; (last) = (probe), (probe) = (probe)->next)


#endif 






