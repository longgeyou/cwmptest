


#ifndef _DIC_H_
#define _DIC_H_



/*============================================================
                        数据结构
==============================================================*/


typedef struct dic_data_t{
    void *key;
    char keyEn;
    int keyLen;
    void *value;
    char valueEn;
    int valueLen;
    //char en;  //确认初始化过，可以使用（因为要确保 void * 变量定义过，否则将造成指针泄露）
}dic_data_t;



//关于 链表数组，数组索引也是散列表的id值， 
//散列表存储方式，散列表id冲突处理用的是链地址法
//参考 《数据结构》的 查找->散列表 这个章节
typedef struct dic_obj_t{
    int size;    //键值对的容量
    int num;   //键值对的当前个数
    link_obj_t **array;  //链表指针 数组，注意这个数组存的是指针
     
}dic_obj_t;



/*============================================================
                        接口
==============================================================*/

void dic_mg_init();
void dic_test();





//遍历
#define DIC_FOREACH_START(dic, iter) for(int dic_conut = 0; dic_conut < dic->size; dic_conut++){\
                                        LINK_FOREACH((dic->array)[dic_conut], probe){\
                                        dic_data_t *iter = (dic_data_t *)(probe->data); 
            
#define DIC_FOREACH_END }}


#endif





