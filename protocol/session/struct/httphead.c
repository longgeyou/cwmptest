 

#include <stdio.h>
#include <string.h>

#include "httphead.h"
#include "log.h"
#include "pool2.h"
#include "link.h"
#include "keyvalue.h"
#include "strpro.h"




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


    实现：
    1、数据结构
    --------------------------------------
    root（链表）
        每一行（名字       + 键值对列表）
    --------------------------------------
    

*/

#define LINE_NAME_STRING_SIZE 256


//每一行数据的结构
typedef struct httphead_line_t{
    char name[LINE_NAME_STRING_SIZE];
    keyvalue_obj_t *keyvalue;
}httphead_line_t;


//头部数据结构体（四层树，包括了根）
typedef struct httphead_head_t{
    link_obj_t *link;   //链表，链表节点的数据类型为 httphead_line_t 
}httphead_head_t;


/*==============================================================
                        本地管理
==============================================================*/
#define HTTPHEAD_MG_INIT_CODE 0x8080

typedef struct httphead_manager_t{
    int poolId;
    char poolName[POOL_USER_NAME_MAX_LEN + 8];
    int initCode;
    
    int httpheadCnt;
}httphead_manager_t;

httphead_manager_t httphead_local_mg = {0};
    
//使用内存池
#define MALLOC(x) pool_user_malloc(httphead_local_mg.poolId, x)
#define FREE(x) pool_user_free(x)
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
    httphead_local_mg.httpheadCnt = 0;
   

    link_mg_init();
    keyvalue_mg_init();

}

/*==============================================================
                        对象
==============================================================*/

//创建 一行的数据结构
httphead_line_t *httphead_create_line(int size)
{
    httphead_line_t *line = (httphead_line_t *)MALLOC(sizeof(httphead_line_t));
    if(line == NULL)return NULL;

    line->keyvalue = keyvalue_create(size);
    if(line->keyvalue == NULL)
    {
        FREE(line);
        return NULL;
    }

    
    memset(line->name, '\0', LINE_NAME_STRING_SIZE);
    
    return line;
}

//释放 数据节点
void httphead_line_destroy(httphead_line_t *line)
{
    if(line == NULL)return;

    
    if(line->keyvalue != NULL)
    {
        //键值对列表的释放
        keyvalue_destroy(line->keyvalue);
    }
        
    //自己
    FREE(line);        //注意node 指向的是动态分配内存，不然会出错
}


//创建头部数据
httphead_head_t *httphead_create_head()
{
    //http 头部（除了第一行）
    httphead_head_t *head = (httphead_head_t *)MALLOC(sizeof(httphead_head_t));
    if(head == NULL)
    {
        LOG_ALARM("head 分配内存失败");
        return NULL;
    }

    //创建链表
    head->link = link_create();
    if(head->link == NULL)
    {
        LOG_ALARM("link 创建失败");
        FREE(head);
        return NULL;
    }
    
    return head;
}


//释放 头部数据结构
void httphead_destroy_head(httphead_head_t *head)
{
    if(head == NULL)return;
    //遍历
    if(head->link != NULL)
    {
        LINK_FOREACH(head->link, probe){
            //1、释放链表节点的 数据
            httphead_line_destroy(probe->data);
        }

        //2、释放链表
            link_destory(head->link );
    }
    
    
    
    //3、自己
    FREE(head);        //注意head 指向的是动态分配内存，不然会出错
}


/*==============================================================
                        对象操作
==============================================================*/








/*==============================================================
                        应用
==============================================================*/

//解析一行数据
int httphead_parse_line(httphead_line_t *line, const char *lineStr)
{
    char *findA, *findB, *findC, *findD;
    char *strA, *strB;
    int step;
    int pos;
    int lenA, lenB, lenD;


//BUF_SIZE 大小要求：应该可以容纳 key、value 字符串
//或许可以考虑分配动态内存
#define BUF_SIZE 512        
    char localBufA[BUF_SIZE] = {0};
    char localBufB[BUF_SIZE] = {0};
    
    

    //参数检测
    if(line == NULL || lineStr == NULL)return RET_FAILD;

    //给buf分配动态内存 
    lenA = strlen(lineStr);
    char *buf = (char *)MALLOC(lenA + 8);
    if(buf == NULL)return RET_FAILD; 
    strcpy(buf, lineStr);

    char *bufTmp = (char *)MALLOC(lenA + 8);
    if(bufTmp == NULL)
    {   
        FREE(buf);
        return RET_FAILD; 
    }

   
    step = 0;
    //1、找到 冒号
    findA = strstr(buf, ":");
    if(findA == NULL)
    {
        strA = localBufA;
        strpro_clean_space(&strA);
        //LOG_SHOW("【A1】%s\n", strA);

        strncpy(line->name, strA, LINE_NAME_STRING_SIZE);   //存入 linde 的名字中
        
    }
    else
    {
        memcpy(localBufA, buf, findA - buf);
        localBufA[findA - buf] = '\0';
        
        strA = localBufA;
        strpro_clean_space(&strA);
        //LOG_SHOW("【A2】%s\n", strA);
        strncpy(line->name, strA, LINE_NAME_STRING_SIZE);   //存入 linde 的名字中
        
         
        pos = findA - buf + 1;
        memmove(buf, buf + pos, lenA - pos);   //内容
        buf[lenA - pos] = '\0';
        //LOG_SHOW("【A3】%s\n", buf);
        
        step = 1;
    }

    if(step == 1)
    {
        while(1)
        {
            //LOG_SHOW("-------------------\n");
            //找到 , 或者 ;
            lenB = strlen(buf);
            findB = strstr(buf, ",");
            findC = strstr(buf, ";");
            if(findB == NULL)
                findB = findC;
            else if(findC != NULL)
                 if(findC - findB < 0)findB = findC;
            
            if(findB != NULL)
            {
                memcpy(localBufA, buf, findB - buf);
                localBufA[findB - buf] = '\0';
                //LOG_SHOW("【B1】%s\n", localBufA);
                strcpy(bufTmp, localBufA);                
            }
            else
            {
                strcpy(bufTmp, buf); 
                step = 2;
            }
            
            //找到 =
            lenD = strlen(bufTmp);
            findD = strstr(bufTmp, "=");
            if(findD != NULL)
            {
                pos = findD - bufTmp;
                memcpy(localBufA, bufTmp, pos);
                localBufA[pos] = '\0';

                strA = localBufA;
                strpro_clean_space(&strA);
                //LOG_SHOW("【D1】【%s】【%s】\n", strA, localBufA);                
                 
                

                pos = findD - bufTmp + 1;
                memcpy(localBufB, bufTmp + pos, lenD - pos);
                localBufB[lenD - pos] = '\0';
                
                strB = localBufB;
                strpro_clean_space(&strB);
                //LOG_SHOW("【D2】【%s】\n", strB); 

                //添加到line的 键值对列表 中
                keyvalue_append_set_str(line->keyvalue, strA, strB);
            }
            else
            {           
                strA = localBufA;
                strpro_clean_space(&strA);
                //LOG_SHOW("【D3】【%s】\n", strA); 

                //添加到line的 键值对列表 中
                keyvalue_append_set_str(line->keyvalue, strA, NULL);
            }

            if(step == 2)break;
            
            pos = findB - buf + 1;
            memmove(buf, buf + pos, lenB - pos);   //内容
            buf[lenB - pos] = '\0';
            //LOG_SHOW("【B2】%s\n", buf);
            
            
        }

    }
    
    FREE(buf);
    FREE(bufTmp);

    return RET_OK;
}

//解析 http头部数据（除了第一行）
int httphead_parse_head(httphead_head_t *head, const char *headStr)
{
    char *find;
    int len;
    int bufLen;
    httphead_line_t *line;
    
    if(head == NULL || headStr == NULL)return RET_FAILD;

    //给buf分配动态内存 
    len = strlen(headStr);
    char *buf = (char *)MALLOC(len + 8);
    if(buf == NULL)return RET_FAILD; 
    strcpy(buf, headStr);

    char *bufTmp = (char *)MALLOC(len + 8);
    if(bufTmp== NULL)
    {
        FREE(buf);
        return RET_FAILD;  
    }
    
    //1、找到 换行符
    while(1)
    {
        len = strlen(buf);
        find = strstr(buf, "\n");
        bufLen = find - buf;
        if(find != NULL && bufLen > 0)
        {                   
            if((bufLen == 1 && buf[0] == '\n') || 
                    (bufLen == 2 && buf[1] == '\n'))    
            {
                break;  //http 头部和负荷之间有一个空行
            }
            else
            {
                memcpy(bufTmp, buf, bufLen);
                bufTmp[bufLen] = '\0';
                
                //LOG_SHOW("【L1】【%s】\n", bufTmp);
#define LINE_KEYVALUE_SIZE 10                
                line = httphead_create_line(LINE_KEYVALUE_SIZE);                  
                httphead_parse_line(line, bufTmp);  //解析
                link_append_by_set_pointer(head->link, (void *)line); //添加
                
                memmove(buf, buf + bufLen + 1, len - bufLen - 1);
                buf[len - bufLen - 1] = '\0';

                //LOG_SHOW("【L2】【%s】\n", buf);
            }   
        }
        else
        {
            break;  //到达了结尾
        }
        
    }


    FREE(buf);
    FREE(bufTmp);
    return RET_OK;
}

/*=============================================================
                        测试
==============================================================*/
//测试辅助：显示 httphead_line_t 对象内容
void __test_aux_show_line(httphead_line_t *line)
{
    if(line == NULL)return ;
    if(line->keyvalue == NULL)return ;

    LOG_SHOW("--------------show line--------------\n");
    LOG_SHOW("name:%s\n", line->name);
    KEYVALUE_FOREACH_START(line->keyvalue, iter){
        if(iter->keyEn == 1)
        {
            LOG_SHOW("key:%s ", iter->key);
            if(iter->valueEn == 1)
            {
                LOG_SHOW("value:%s ", iter->value);
            }
            LOG_SHOW("\n");
        }
        
    }KEYVALUE_FOREACH_END;    
}

//测试辅助：显示 httphead_head_t 内容
void __test_aux_show_head(httphead_head_t *head)
{
    if(head == NULL)return ;
    if(head->link == NULL)return ;

    LOG_SHOW("============================show head\n");
    LOG_SHOW("num:%d\n", head->link->nodeNum);
    LINK_FOREACH(head->link, probe){
        __test_aux_show_line(probe->data);
    }    
}



void httphead_test()
{
    printf("httphead test ...\n");

//    httphead_line_t *line = httphead_create_line(100);    
//    char lineStr[] = "Accept-Language: en-US,en; q =0.8 ; new, old  =100";
//    httphead_parse_line(line, lineStr);
//    __test_aux_show_line(line);
//
//    httphead_line_t *line2 = httphead_create_line(100);
//    char lineStr2[] = "Accept-Language1: en- US ,en =1232131 ; q =0.8 ; new, old  =100";
//    httphead_parse_line(line2, lineStr2);
//    __test_aux_show_line(line2);
//    
//
//    httphead_head_t *head = httphead_create_head();
//    link_append_by_set_pointer(head->link, (void *)line); 
//    link_append_by_set_pointer(head->link, (void *)line2); 
//    __test_aux_show_head(head);
//     
//    pool_show();
//
//    httphead_destroy_head(head);
//    pool_show();


    char headStr[] = "Accept-Language: en-US,en; q =0.8 ; new, old  =100\n"
                        "Accept-Language1: en- US ,en =1232131 ; q =0.8 ; new, old  =100\n\n";
    httphead_head_t *head = httphead_create_head();                    
    httphead_parse_head(head, headStr);  

    __test_aux_show_head(head);
}






