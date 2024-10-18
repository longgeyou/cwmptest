

#include <stdio.h>
#include <string.h>

#include "httphead.h"
#include "log.h"
#include "pool2.h"
#include "link.h"




/*=============================================================
                        数据结构
==============================================================*/
/*
    http 头部结构
    类似于 Accept-Language: en-US,en;q=0.8 的一行数据，
    1、冒号前面是名字，后面是内容；
    2、内容用 分号 分割成多个子内容；
    3、子内容里面则是一个个用 逗号 成员，可以看为 键值对 的成员；
        键值对有的只有 key，没有value，有的两者都有
    例子里，可以理解为：
    --------------------------------------
    名字：Accept-Language
    内容：
        子内容1:
                键值对1：
                        key:空
                        value:en-US
                键值对2：
                        key:空
                        value:en
        子内容2:
                键值对1：
                        key:q
                        value:0.8
    --------------------------------------

    可以理解为树形结构

*/

#define KEY_STRING_SIZE 32
#define VALUE_STRING_SIZE 256

//树节点结构
typedef struct httphead_node_t{
    char key[KEY_STRING_SIZE];
    char value[VALUE_STRING_SIZE];
    char keyEn;
    char valueEn;

    link_obj_t *son;   //子树（用链表表示）
}httphead_node_t;


//头部数据结构体（四层树，包括了根）
typedef struct httphead_head_t{
    httphead_node_t *root;   //树根
}httphead_head_t;


/*=============================================================
                        本地管理
==============================================================*/
#define HTTPHEAD_MG_INIT_CODE 0x8080

typedef struct httphead_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN];
    int initCode;
    
    int instanceNum;
}httphead_manager_t;

httphead_manager_t httphead_local_mg = {0};
    
//使用内存池
#define POOL_MALLOC(x) pool_user_malloc(httphead_local_mg.poolId, x)
#define POOL_FREE(x) pool_user_free(x)
#define HTTPHEAD_POOL_NAME "httphead"
    
    
//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1
    
//初始化
void httphead_mg_init()
{    
    if(httphead_local_mg.initCode == HTTPHEAD_MG_INIT_CODE) return; //一次初始化
    httphead_local_mg.initCode = HTTPHEAD_MG_INIT_CODE;
    
    strncpy(httphead_local_mg.poolName, HTTPHEAD_POOL_NAME, POOL_USER_NAME_MAX_LEN);
    httphead_local_mg.poolId = pool_apply_user_id(httphead_local_mg.poolName); 
    httphead_local_mg.instanceNum = 0;
   

    link_init();

}



/*=============================================================
                        对象
==============================================================*/

//动态创建 树节点
httphead_node_t *httphead_node_create()
{
    httphead_node_t *node = (httphead_node_t *)POOL_MALLOC(sizeof(httphead_node_t));
    if(node == NULL)
    {
        return NULL;
    }
    memset(node->key, '\0', KEY_STRING_SIZE);
    memset(node->value, '\0', VALUE_STRING_SIZE);
    node->keyEn = 0;
    node->valueEn = 0;
    
    node->son = NULL;
    
    return node;
}

//释放 数据节点
void httphead_node_release(httphead_node_t *node)
{
    if(node == NULL)return;


    if(node->son != NULL)
    {
        //遍历子节点，然后释放对应内存
        LINK_FOREACH(node->son, probe)
        {
            httphead_node_release((httphead_node_t *)(probe->data_p));    //递归
        }
    
    
        //子成员
        link_destory(node->son);
    }
    
    
    //自己
    POOL_FREE(node);        //注意node 指向的是动态分配内存，不然会出错
}


//创建头部数据
httphead_head_t *httphead_head_create()
{
    //http 头部（除了第一行）
    httphead_head_t *head = (httphead_head_t *)POOL_MALLOC(sizeof(httphead_head_t));
    if(head == NULL)
    {
        LOG_ALARM("head 分配内存失败");
        return NULL;
    }

    //创建树根
    head->root = httphead_node_create();
    if(head->root == NULL)
    {
        LOG_ALARM("root 分配内存失败");
        POOL_FREE(head);
        return NULL;
    }
    
    return head;
}


//释放 头部数据结构
void httphead_head_release(httphead_head_t *head)
{
    if(head == NULL)return;
    //子成员
    if(head->root != NULL)httphead_node_release(head->root);
    
    //自己
    POOL_FREE(head);        //注意head 指向的是动态分配内存，不然会出错
}


/*=============================================================
                        应用
==============================================================*/


//解析一行数据
int httphead_parse_line(httphead_head_t *head, const char *lineStr)
{
    char *find;
    int step;
    int pos;
    int len;
    
    
    
    if(head == NULL || lineStr == NULL)return RET_FAILD;
    
    httphead_node_t *line = httphead_node_create();     //一个节点
    if(line == NULL)return RET_FAILD;

    len = strlen(lineStr);
    char *buf = (char *)POOL_MALLOC(len + 8);
    if(buf == NULL)
    {
        httphead_node_release(line);
        return RET_FAILD;
    }
    strcpy(buf, lineStr);

    
    
    
    step = 0;
    //对一行数据的解析步骤
    //1、找到 冒号
    find = strstr(buf, ":");
    line->valueEn = 1;
    if(find == NULL)
    {
        strncpy(line->value, lineStr, VALUE_STRING_SIZE);

    }
    else
    {
        memcpy(line->value, buf, find - buf);   //名字
        line->value[find - buf] = '\0';
         
        pos = find - buf;
        memmove(buf, buf + pos + 1, len - pos - 1);   //内容
        buf[len - pos - 1] = '\0';
        step = 1;
    }

    //2、用 分号 分割内容
    const char delimA[] = ";";
    char *tokenA;
    
#define TMP_BUF_LEN 512    
    char bufB[TMP_BUF_LEN] = {0};
    const char delimB[] = ",";
    char *tokenB;

    char bufC[TMP_BUF_LEN] = {0};
    //const char delimC[] = "=";
    //char *tokenC;

    httphead_node_t *nodeA;     //分号
    httphead_node_t *nodeB;     //逗号
    httphead_node_t *nodeC;     //等于号
    if(step == 1)
    {
        
        tokenA = strtok(buf, delimA);
        while(tokenA != NULL)
        {
//            //step = 2;
//            nodeA = httphead_node_create(); //这里没有考虑 创建失败 的情况
//            link_append(line->son, (void *)nodeA , sizeof(httphead_node_t));
//            
//            
//            //3、用 逗号 去分割子内容
//            strncpy(bufB, tokenA, TMP_BUF_LEN);
//            tokenB = strtok(bufB, delimB);
//            while(tokenB != NULL)
//            {
//                nodeB = httphead_node_create(); //这里没有考虑 创建失败 的情况
//                link_append(nodeA->son, (void *)nodeB , sizeof(httphead_node_t));
//
//                //4、查找 等于号
//                strncpy(bufC, tokenB, TMP_BUF_LEN);
//                find = strstr(bufC, "=");
//                nodeC = httphead_node_create(); //这里没有考虑 创建失败 的情况
//                if(find == NULL)
//                {
//                    strncpy(nodeC->value, bufC, VALUE_STRING_SIZE);
//                    nodeC->valueEn = 1;
//                    
//                }
//                else
//                {
//                    find = '\0';    //'=' 变为 '\0'，分割为两个字符串
//                    strncpy(nodeC->key, bufC, KEY_STRING_SIZE);
//                    nodeC->keyEn = 1;
//                    strncpy(nodeC->value, find + 1, VALUE_STRING_SIZE);
//                    nodeC->valueEn = 1;
//                    
//                }
//                link_append(nodeB->son, (void *)nodeC , sizeof(httphead_node_t));
//                
//            
//                tokenB = strtok(NULL, delimB);
//            }

            LOG_SHOW("---->%s\n", tokenA);
            tokenA = strtok(NULL, delimA);
        }

    }

    link_append(head->root->son, (void *)line , sizeof(httphead_node_t));
    

    POOL_FREE(buf);

    return RET_OK;
}

//打印 一行 结构体的内容
void __show_line(httphead_node_t *line)
{
    LOG_SHOW("valueEn:%d value:【%s】\n", line->valueEn, line->value);
}




/*=============================================================
                        测试
==============================================================*/




void httphead_test()
{
    char lineStr[] = "Accept-Language: en-US,en;q=0.8";

    httphead_head_t *head = httphead_head_create();    

    httphead_parse_line(head, lineStr);

    link_node_t *node = link_get_node(head->root->son, 0);
    
    if(node == NULL)
        LOG_ALARM("node is NULL");
    httphead_node_t *line = (httphead_node_t *)(node->data_p);
    //__show_line(line);

    pool_show();
}






