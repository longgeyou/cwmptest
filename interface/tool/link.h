
#ifndef _LINK_H_
#define _LINK_H_










/*============================================================
                        数据结构
==============================================================*/



typedef struct link_node_t{  //链表节点元素
    void *data_p; //存放数据的指针
    char data_used;   //用于指示data_p是否分配了动态内存
    int data_size;     //用于指示data_p内存的大小
    struct link_node_t *next;  
}link_node_t;

typedef struct link_obj_t{  //链表对象
    struct link_obj_t *me;
    link_node_t *head;
    int nodeNum;

    int (*find_callBack)(void *, const void *, int);   //回调函数，用于链表节点的查找
}link_obj_t;






/*============================================================
                        接口
==============================================================*/
void link_force_init();
void link_init();
int link_obj_init(link_obj_t *obj);
link_obj_t *link_create();
link_node_t *link_node_creat(void *data, int dataSize);
int link_appendNode(link_obj_t *obj, link_node_t *node);
int link_append(link_obj_t *obj, void *data, int dataSize);
int link_remove_byId(link_obj_t *obj, int id);
link_node_t *link_find(link_obj_t *obj, const void *key, int keySize); //查找
int link_remove_byNode(link_obj_t *obj, link_node_t **node_p);
int link_node_isExist(link_obj_t *obj, link_node_t *node);
int link_size(link_obj_t *obj);
void link_test();
int link_node_alter(link_node_t *node, void *data, int dataLen);    //节点内容修改




/*============================================================
                        特殊宏定义
==============================================================*/



//遍历访问宏，注意参数没有做判断，例如link和(link)->head可能为空，可能造成程序错误
#define LINK_FOREACH(link, probe) for(link_node_t *(probe) = (link)->head->next; \
                                        (probe) != NULL; (probe) = (probe)->next)



#endif 






