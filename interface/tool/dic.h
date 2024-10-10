#ifndef _DIC_H_
#define _DIC_H_



/*============================================================
                        数据结构
==============================================================*/
typedef struct dic_data_t{
    void *key;
    int keySize;
    void *value;
    int valueSize;

    char used;  //1 ：确认初始化过，可以使用（因为要确保 void * 变量定义过，便来造成指针泄露）

}dic_data_t;


typedef struct dic_obj_t{
    int size;
    int dataCnt;
    link_obj_t *array;  //链表数组
    //link_obj_t keyLink;   //维护一张 key和映射index 的链表，用于快速遍历【弃用：因为要同步删除】
    int (*getIndex)(const void *, int);//回调函数：键值到索引的映射函数
}dic_obj_t;



/*============================================================
                        接口
==============================================================*/

void dic_init();
void dic_test();










//遍历
#define DIC_FOREACH_START(dic, iter) for(int i_conut = 0; i_conut < dic->size; i_conut++){\
                                        LINK_FOREACH(&((dic->array)[i_conut]), probe){\
                                        dic_data_t *iter = (dic_data_t *)(probe->data_p); 
            
#define DIC_FOREACH_END }}


#endif





