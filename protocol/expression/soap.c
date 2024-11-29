


#include <stdio.h>
#include <string.h>

#include "soap.h"
#include "pool2.h"

#include "log.h"





/*==============================================================
                        本地管理
==============================================================*/
#define SOAP_INIT_CODE 0x8080

typedef struct soap_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    
    int initCode;
}soap_manager_t;

soap_manager_t soap_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(soap_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
#define DIC_POOL_NAME "soap"
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void soap_mg_init()
{    
    if(soap_local_mg.initCode == SOAP_INIT_CODE)return ;   //防止重复初始化
    soap_local_mg.initCode = SOAP_INIT_CODE;
    
    strncpy(soap_local_mg.poolName, DIC_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    soap_local_mg.poolId = pool_apply_user_id(soap_local_mg.poolName); 

    pool_init();    //依赖于pool2.c，所以要初始化
    keyvalue_mg_init();
    link_mg_init();
    stack_mg_init();
 
}

#define BUF64 64
#define BUF128 128
#define BUF256 256
#define BUF512 512
#define BUF1024 1024

/*==============================================================
                        对象
==============================================================*/
//创建一个soap节点
soap_node_t *soap_create_node(int attrSize)
{
    soap_node_t *node = (soap_node_t *)MALLOC(sizeof(soap_node_t));
    if(node == NULL)return NULL;

    memset(node->name, '\0', SOAP_NAME_STRING_LEN);
    memset(node->value, '\0', SOAP_VALUE_STRING_LEN);
    node->nameEn = 0;
    node->valueEn = 0;
 
    node->attr = keyvalue_create(attrSize);
    if(node->attr == NULL)
    {
        FREE(node);
        printf(" attr 分配内存失败\n");
        return NULL;
    }

    node->son = link_create();
    if(node->son == NULL)
    {
        FREE(node);
        keyvalue_destroy(node->attr);   //摧毁keyvalue 对象
        printf(" son 链表创建失败\n");
        return NULL;
    }
    
    return node;
}

//创建一个soap节点 属性最大个数为默认
soap_node_t *soap_create_node_default_size()
{
    return soap_create_node(SOAP_ATTR_DEFAULT_SIZE);
}


//创建一个soap节点，并指定 部分节点成员的数值
soap_node_t *soap_create_node_set_size(char *name, char *value, int attrSize)
{
    soap_node_t *node = soap_create_node(attrSize);
    if(node == NULL)return NULL;

    if(name != NULL)
    {
        strcpy(node->name, name);
        node->nameEn = 1;
    }
    else
    {
        node->nameEn = 0;
    }
    
    if(value != NULL)
    {
        strcpy(node->value, value);
        node->valueEn = 1;
    }
    else
    {
        node->valueEn = 0;
    }

    return node;
}

//创建一个soap节点，并指定 attrSize 为默认值
soap_node_t *soap_create_node_set_value(char *name, char *value)
{
    soap_node_t *node = soap_create_node(SOAP_ATTR_DEFAULT_SIZE);
    if(node == NULL)return NULL;

    if(name != NULL)
    {
        strcpy(node->name, name);
        node->nameEn = 1;
    }
    else
    {
        node->nameEn = 0;
    }
    
    if(value != NULL)
    {
        strcpy(node->value, value);
        node->valueEn = 1;
    }
    else
    {
        node->valueEn = 0;
    }

    return node;
}

//创建一个soap 对象, 但不初始化成员数值
soap_obj_t *soap_create()
{
    soap_obj_t *soap = (soap_obj_t *)MALLOC(sizeof(soap_obj_t));

    if(soap == NULL)return NULL;
    soap->en = 1;
    
    soap->root = soap_create_node_default_size();
    if(soap->root == NULL)
    {
        FREE(soap);
        return NULL;
    }
    
    return soap;
}

//创建一个soap 对象，并给定 name和value, attr的大小为默认
soap_obj_t *soap_create_set_value(char *name, char *value)
{
    soap_obj_t *soap = (soap_obj_t *)MALLOC(sizeof(soap_obj_t));

    if(soap == NULL)return NULL;
    soap->en = 1;
    
    soap->root = soap_create_node_set_value(name, value);
    if(soap->root == NULL)
    {
        FREE(soap);
        return NULL;
    }
    
    return soap;
}

//摧毁一个soap 节点
void soap_destroy_node(soap_node_t *node)
{
    if(node == NULL)return ;

    //释放 keyvalue对象
    keyvalue_destroy(node->attr);
    
    //递归调用，对树结构的每个节点进行摧毁
    LINK_FOREACH(node->son, probe){
        if(probe->data != NULL && probe->en == 1)
        {
            soap_destroy_node(probe->data);
        }
    }
    
    link_destory(node->son);    

    //释放自己
    FREE(node);
    
}

//摧毁一个soap对象
void soap_destroy(soap_obj_t *soap)
{
    if(soap == NULL)return ;
    //释放成员
    soap_destroy_node(soap->root);

    //释放自己
    FREE(soap);
}


//清空节点内容
void soap_node_clear(soap_node_t *node)
{
    if(node == NULL)return ;

    //递归调用，对每个子节点进行摧毁
    if(node->son != NULL)
    {
        LINK_FOREACH(node->son, probe){
            if(probe->data != NULL && probe->en == 1)
            {
                soap_destroy_node(probe->data);
            }
        }
    
        link_clear(node->son); 
    }

    //释放 keyvalue对象
    if(node->attr != NULL)
        keyvalue_clear(node->attr);
        
   //清空成员
   memset(node->name, '\0', SOAP_NAME_STRING_LEN);
   memset(node->value, '\0', SOAP_VALUE_STRING_LEN);
   node->nameEn = 0;
   node->valueEn = 0;
   
}

/*==============================================================
                        应用
==============================================================*/
//对soap 节点 name 和 value的赋值， 如果参数name为NULL，那么不修改name，value一样
int soap_node_set_name_value(soap_node_t *node, char *name, char *value)
{
    if(node == NULL)return RET_FAILD;

    if(name != NULL)strcpy(node->name, name);
    if(value != NULL)strcpy(node->value, value);

    return RET_OK;
}

//soap节点 添加属性值
int soap_node_append_attr(soap_node_t *node, char *key, char *value)
{
    if(node == NULL)return RET_FAILD;
    keyvalue_append_set_str(node->attr, key, value);
    return RET_OK;
}

//soap节点 添加子节点
int soap_node_append_son(soap_node_t *node, soap_node_t *son)
{
    if(node == NULL || son == NULL)return RET_FAILD;
    link_append_by_set_pointer(node->son, (void *)son);
    return RET_OK;
}



//soap 节点到字符串
void soap_node_to_str(soap_node_t *node, char *buf, int *pos, int bufLen)
{
    if(node == NULL || *pos >= bufLen)return ;

    *pos += snprintf(buf + *pos, bufLen - *pos, "<%s", node->name);
    
    KEYVALUE_FOREACH_START(node->attr, iter){
        if(iter->keyEn == 1 && iter->valueEn == 1)
            *pos += snprintf(buf + *pos, bufLen - *pos, " %s=%s", (char *)(iter->key), (char*)(iter->value));
    }KEYVALUE_FOREACH_END;
    
    if(node->son->head->next == NULL && node->valueEn != 1)
    {
        *pos += snprintf(buf + *pos, bufLen - *pos, "/>\n");
        return;
    }        
    else
    {
        *pos += snprintf(buf + *pos, bufLen - *pos,">\n");  
    }  
    
    if(node->valueEn == 1)
        *pos += snprintf(buf + *pos, bufLen - *pos, "%s\n", node->value);

    
    LINK_FOREACH(node->son, linkProbe)
    {
        if(linkProbe->en == 1)
        {
            soap_node_to_str((soap_node_t *)(linkProbe->data), buf , pos, bufLen);
        }       
    }
    
    *pos += snprintf(buf + *pos, bufLen - *pos, "</%s>\n", node->name);
}




//attr 属性转化为 keyvalue 存储， 例如 a=1
void _attr_to_keyvalue(keyvalue_obj_t *keyvalue, char *str)
{
    if(keyvalue == NULL || str == NULL)return ;
    char buf[BUF512 + 8];
    strcpy(buf, str);
    int len = strlen(buf);

    char *find;

    find = strstr(buf, "=");

    if(find == NULL)
    {
        keyvalue_append_set_str(keyvalue, buf, NULL);
    }
    else
    {
        *find = '\0';
        if(find == (buf + len - 1))
        {
            keyvalue_append_set_str(keyvalue, buf, NULL);
        }
        else
        {
             keyvalue_append_set_str(keyvalue, buf, find + 1);
        }
       
    }

    //keyvalue_show(keyvalue);      
}

//node 节点的成员 name、attr 的解析，例如 root a=1 b=2
void __name_attr_pro(soap_node_t *node, char *str)
{    
    if(node == NULL || str == NULL)return ;
    int mark;
    char buf[BUF512 + 8];
    strcpy(buf, str);


    char *token = strtok(buf, " ");   
    mark = 0;
    while(token) {
        //puts(token);
        if(mark == 0)
        {
            mark = 1;
            strcpy(node->name, token);
            node->nameEn = 1;
        }
        else
        {
            _attr_to_keyvalue(node->attr, token);
            //printf("-->%s\n", token);
        }
        
        token = strtok(NULL, " ");
    }

}



//字符串解析成 soap节点
#define STACK_SIZE 30
soap_node_t *soap_str2node(char *str)
{
    if(str == NULL)return NULL;
    int i;
    int j;
    int mark;
    int step;
    char buf[BUF1024 + 8];
    int bufCnt;
    int len;
    char bufName[SOAP_NAME_STRING_LEN];
    //char bufValue[];
    void *p;


    //soap_node_t *node = soap_create_node_set_value("root", "value");
    soap_node_t *root = soap_create_node_default_size();

    stack_obj_t *stack = stack_create(STACK_SIZE);  //辅助工具 栈
    stack_push(stack, (void *)root);

    soap_node_t *current;
    soap_node_t *newNode;

    len = strlen(str);
    step = 0;
    for(i = 0; i < len; i++)
    {
        //printf("-->step:%d char:%c\n", step, str[i]);
        switch(step)
        {
            case 0:{
                if(str[i] == '<')
                {
                    current = (soap_node_t *)((stack_top_member_pointer(stack))->d);
                    if(i + 1 < len && str[i + 1] == '/')    
                    {
                        if(stack_isEmpty(stack))
                        {
                            LOG_ALARM("格式问题 </ xxx> \n");
                        }
                        else        
                        {
                            strcpy(bufName, current->name);
                            stack_pop(stack, &p);   //出栈
                        }
                        
                        bufCnt = 0;
                        step = 3; 
                    }
                    else
                    {
                        
                        newNode = soap_create_node_default_size();
                        link_append_by_set_pointer(current->son, (void *)newNode);
                        stack_push(stack, (void *)newNode);
                        
                        step = 1;
                        bufCnt = 0;
                    }    
                }
                    
            }break;
            case 1:{    //处理 < > 里面的字符串
                if(str[i] == '>')
                {
                    current = (soap_node_t *)((stack_top_member_pointer(stack))->d);
                    
                    if(i - 1 > 0 && str[i - 1] == '/')   
                    {            
                        buf[bufCnt - 1] = '\0';
                        if(stack_isEmpty(stack))
                        {
                            printf("格式问题 xxx /> \n");
                        }
                        else        //出栈
                        {
                            stack_pop(stack, &p);
                        }
                        
                        step = 0; 
                    }
                    else
                    { 
                        buf[bufCnt] = '\0';
                        bufCnt = 0;
                        
                        step = 2;
                    } 

                    current->nameEn = 0;
                    //keyvalue_clear(current->attr);    //清空
                    __name_attr_pro(current, buf);
                    
                }
                else
                {
                    if(bufCnt < BUF1024)
                        buf[bufCnt++] = str[i];
                }
                
            }break;
            case 2:{    //处理    <> 和 </> 之间的数据，一般是 value ，当然中间有可能有子节点
                if(str[i] == '<')
                {
                    mark = 0;
                    for(j = 0; j < bufCnt; j++)
                    {
                        if(buf[j] != ' ' && buf[j] != '\n')   //找到除了空格和换行符的其他字符
                        {
                            mark = 1;
                            break;
                        }
                    }

                    current = (soap_node_t *)((stack_top_member_pointer(stack))->d);
                    if(mark == 1)
                    {
                        buf[bufCnt] = '\0';
                        strcpy(current->value, buf);   //获得节点的 value
                        current->valueEn = 1;
                    }
                    else
                    {
                        current->valueEn = 0;
                    }

                    if(i - 1 >= 0)i--;
                    step = 0;
                }
                else
                {
                    if(bufCnt < BUF1024)
                        buf[bufCnt++] = str[i];
                }
            }break;
            case 3:{    //  处理 </    xxx>
                if(str[i] == '>')
                {
                    buf[bufCnt] = '\0';
                    current = (soap_node_t *)((stack_top_member_pointer(stack))->d);
                    if(strcmp(buf, bufName) != 0)
                        LOG_ALARM("末尾标签不匹配 buf:%s current:%s", buf, bufName);
                    step = 0;
                }
                else if(str[i] != '/')
                {
                     buf[bufCnt++] = str[i];
                }

            }break;
        }

    }

    stack_destroy(stack);
    
    return root; 
}

//通过name匹配 得到子节点
soap_node_t *soap_node_get_son(soap_node_t *node, char *name)
{
    if(node == NULL || node->son == NULL || name == NULL)return NULL;

    soap_node_t *ret = NULL;
    LINK_FOREACH(node->son, probe){
        soap_node_t *nodeProbe = (soap_node_t *)(probe->data);
        if(strcmp(nodeProbe->name, name) == 0)
        {
            ret = nodeProbe;
            break;
        }
    };
    
    return ret;
}

//soap 节点复制
int soap_node_copy(soap_node_t *in, soap_node_t **outp)
{
    if(in == NULL)return RET_FAILD;

    soap_node_t *out;
    
    out = (soap_node_t *)MALLOC(sizeof(soap_node_t));
    memcpy(out, in, sizeof(soap_node_t));

    //子成员 attr
    if(in->attr != NULL)
    {
        out->attr = keyvalue_create(in->attr->list->size);
        KEYVALUE_FOREACH_START(in->attr, iter){
            keyvalue_data_t *data = iter;
            keyvalue_append_set_str(out->attr, data->key, data->value);        
        }KEYVALUE_FOREACH_END
    }

    //子成员 son
    if(in->son != NULL)
    {   
        out->son = link_create();
        LINK_DATA_FOREACH_START(in->son, probeData)
        {
            soap_node_t *data = (soap_node_t *)probeData;
            soap_node_t *nodeTmp;
            soap_node_copy(data, &nodeTmp);  //递归调用
            link_append_by_set_pointer(out->son, nodeTmp);
            
        }LINK_DATA_FOREACH_END

    }
    *outp = out;
    return RET_OK;
}



/*==============================================================
                        测试
==============================================================*/
void __test_show_node(soap_node_t *node)
{
    if(node == NULL)return ;
    printf("<%s", node->name);
    
    KEYVALUE_FOREACH_START(node->attr, iter){
        if(iter->keyEn == 1 && iter->valueEn == 1)printf(" %s=%s", (char *)(iter->key), (char*)(iter->value));
    }KEYVALUE_FOREACH_END;
    
    if(node->son->head->next == NULL && node->valueEn != 1)
    {
        printf("/>\n");
        return;
    }        
    else
    {
        printf(">\n");  
    }  
    
    if(node->valueEn == 1)printf("%s\n", node->value);

    
    LINK_FOREACH(node->son, linkProbe)
    {
        if(linkProbe->en == 1)
        {
            __test_show_node((soap_node_t *)(linkProbe->data));
        }       
    }
    
    printf("</%s>\n", node->name);
}

void __test_show_node_print2log(soap_node_t *node)
{
    if(node == NULL)return ;
    LOG_SHOW("<%s", node->name);
    
    KEYVALUE_FOREACH_START(node->attr, iter){
        if(iter->keyEn == 1 && iter->valueEn == 1)LOG_SHOW(" %s=%s", (char *)(iter->key), (char*)(iter->value));
    }KEYVALUE_FOREACH_END;
    
    if(node->son->head->next == NULL && node->valueEn != 1)
    {
        LOG_SHOW("/>\n");
        return;
    }        
    else
    {
        LOG_SHOW(">\n");  
    }  
    
    if(node->valueEn == 1)LOG_SHOW("%s\n", node->value);

    
    LINK_FOREACH(node->son, linkProbe)
    {
        if(linkProbe->en == 1)
        {
            __test_show_node((soap_node_t *)(linkProbe->data));
        }       
    }
    
    LOG_SHOW("</%s>\n", node->name);
}


void __test_show_soap(soap_obj_t *soap)
{
    if(soap == NULL)return ;
    printf("\n------------ soap show -------------\n");
    if(soap->en == 1)__test_show_node(soap->root);
}


//显示接口
void soap_node_show(soap_node_t *node)
{
    printf("\n---------------------soap node show---------------------\n");
    __test_show_node(node);
}
void soap_node_show_print2log(soap_node_t *node)
{
     LOG_SHOW("\n---------------------soap node show---------------------\n");
    __test_show_node_print2log(node);
}



void soap_test()
{
    //printf("soap test start ……\n");
    

    soap_obj_t *soap = soap_create_set_value("root", "value");
    pool_show();
    
    soap_node_append_attr(soap->root, "x", "10");
    soap_node_append_attr(soap->root, "y", "20");
    
    soap_node_t *node = soap_create_node_set_value("node", "node-value");
    soap_node_append_attr(node, "x", "10");
    soap_node_append_attr(node, "y", "20");    
    soap_node_append_son(soap->root, node);


    soap_node_t *subNode1 = soap_create_node_set_value("subNode1", "subNode1-value");
    soap_node_append_attr(subNode1, "x", "10");
    soap_node_append_attr(subNode1, "y", "20");
    soap_node_t *subNode2 = soap_create_node_set_value("subNode2", NULL);
    soap_node_append_attr(subNode2, "x", "10");
    soap_node_append_attr(subNode2, "y", "20");
    soap_node_append_son(node, subNode1);
    soap_node_append_son(node, subNode2);

    //__test_show_node(soap->root);
    
    //摧毁    
    //pool_show();

    //soap_destroy(soap);

    //pool_show();

    //节点到字符串
//    char buf[512];
//    char *p;
//    p = buf;
//    int pos;
//    soap_node_to_str(soap->root, p, &pos,10);
//
//    printf("pos:%d %s\n", pos, buf);


    //字符串到节点
    //char str[] = "<root a=1 b=2></>";
//    char str2[] = "<root x=10 y=20>\n"
//    "value\n"
//    "<node x=10 y=20>\n"
//    "node-value\n"
//    "<subNode1 x=10 y=20>\n"
//    "subNode1-value\n"
//    "</subNode1>\n"
//    "<subNode2 x=10 y=20/>\n"
//    "</node>\n"
//    "</root>"
//    "<root2 m=1 n=2/>";
//
//    
//    soap_node_t *nodePro = soap_str2node(str2);
//
//    __test_show_node(nodePro);
//    pool_show();
//
//    soap_destroy_node(nodePro);
//    pool_show();


    //测试清空 
//    pool_show();
//    soap_node_clear(soap->root);
//
//    pool_show();

    //测试复制
    soap_node_t *nodeCopy;
    soap_node_copy(soap->root, &nodeCopy);
    soap_destroy(soap);
    pool_show();
    __test_show_node(nodeCopy);
    soap_destroy_node(nodeCopy);
    
    pool_show();
}






