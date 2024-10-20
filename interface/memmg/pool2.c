﻿


/*
内存池管理，监控
给出的接口，
pool_malloc代替malloc
pool_free代替free
*/


#include "pool2.h"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>






//内存池的最小单元，块对象
typedef struct __block_obj{
    void *handle;   //句柄
    int size;   //块大小（字节）
    int id;   //属于哪个用户id
    char en;    //使用标记
}block_obj_t;

//管理
#define POOL_BLOCK_MAX_NUM (1024*32)     //有必要增加个数
#define POOL_TOTLE_BYTE (1024*1024*4)     //内存池总量4M
//#define POOL_USER_MAX_NUM 256     //头文件中定义
typedef struct __pool_mg{
    block_obj_t block[POOL_BLOCK_MAX_NUM];
    pool_user_t user[POOL_USER_MAX_NUM];
    int mallocCnt;  //malloc次数统计(成功次数)
    int freeCnt;    //free次数统计
    int mallocFaildCnt[POOL_USER_MAX_NUM]; //内存分配失败次数统计
    
    int initCode;  //已经被初始化 标志
}pool_mg_t;


#define POOL_INIT_CODE 0x8080


static pool_mg_t pool_local_mg = {0};


//初始化
void pool_init()
{
    if(pool_local_mg.initCode == POOL_INIT_CODE)return ; // 只初始化一次
    pool_local_mg.initCode = POOL_INIT_CODE;
    
    int i;
    block_obj_t *b = pool_local_mg.block;
    pool_user_t *u = pool_local_mg.user;
    
    
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {
        b[i].en = 0;
    }

    for(i = 0; i < POOL_USER_MAX_NUM; i++)
    {
        u[i].en = 0;

        pool_local_mg.mallocFaildCnt[i] = 0;
    }

    //默认用户
    u[DEAULT_USER_ID].en = 1;
    strncpy(u[DEAULT_USER_ID].name, "defalut", POOL_USER_NAME_MAX_LEN);

    pool_local_mg.mallocCnt = 0;
    pool_local_mg.freeCnt = 0;

    
}
//取得内存池的使用量
static int __get_pool_used_byte()
{
    int i, cnt;
    cnt = 0;

    block_obj_t *b = pool_local_mg.block;
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {   
        if(b[i].en == 1 && b[i].handle != NULL)
        {
            cnt += b[i].size;
        }
        
    }
    return cnt;
}

//块的个数统计
static int __get_pool_block_num()
{
    int i, cnt;
    cnt = 0;

    block_obj_t *b = pool_local_mg.block;
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {   
        if(b[i].en == 1)
        {
            cnt ++;
        }
        
    }
    return cnt;
}


//申请新的内存块
static void *__apply_new_block(int id, int size)
{
    int i;
    if((__get_pool_used_byte() + size) >= POOL_TOTLE_BYTE)  //内存池溢出
    {
        return NULL;
    }

    if(id < 0 || id >= POOL_USER_MAX_NUM)   //用户id无效
    {
        return NULL;
    }
    
    void *handle = malloc(size);
    if(handle == NULL)
    {
        return NULL;
    }
    block_obj_t *b = pool_local_mg.block;
   
    
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {
        if(b[i].en == 0)
        {
            b[i].en = 1;
            b[i].handle = handle;
            b[i].id = id;
            b[i].size = size;
            pool_local_mg.mallocCnt++;
            return handle;
        }
    }

   return NULL;
}

//释放区块
static void __free_block(void *handle)
{
    if(handle == NULL)return ;
    int i;
    block_obj_t *b = pool_local_mg.block;
   
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {
        if(b[i].handle == handle)
        {
            b[i].en = 0;
            free(handle);
            handle = NULL;   //有待考虑
            pool_local_mg.freeCnt++;
            return ;
        }
    }
}



//统计结果，内存池的总量，内存池使用占比，内存池剩余字节数
//分配块的个数，使用者的使用字节数及其占比
//提供外部调用接口
void pool_statistic(pool_stat_t *stat)
{
    int i;
    
    if(stat == NULL)
        return;
    int usedByte = __get_pool_used_byte();
    stat->totle = POOL_TOTLE_BYTE;
    stat->used = usedByte;
    stat->usedRate = (long long)usedByte * 1000 / POOL_TOTLE_BYTE;   
    stat->residue = POOL_TOTLE_BYTE - usedByte;
    stat->residueRate = (long long)(stat->residue) * 1000 / POOL_TOTLE_BYTE;   
    stat->blockCnt = __get_pool_block_num();
    stat->mallocCnt = pool_local_mg.mallocCnt;
    stat->freeCnt = pool_local_mg.freeCnt;
    
    block_obj_t *b = pool_local_mg.block;
    pool_user_t *u = pool_local_mg.user;
    
    for(i = 0; i < POOL_USER_MAX_NUM; i++)
    {  
        stat->mallocFaildCnt[i] = pool_local_mg.mallocFaildCnt[i]; //内存失败分配次数
    }
    
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {   
        if(b[i].en == 1 && b[i].handle != NULL && b[i].id < POOL_USER_MAX_NUM) //用户id号是用户数组的序号
        {
            if(u[b[i].id].en == 1)
            {
                stat->userUsed[b[i].id] += b[i].size;
                //stat->userEn[i] = 1;
                
            }
        }
        
    }

    for(i = 0; i < POOL_USER_MAX_NUM; i++)
    {   
    
        if(b[i].en == 1 && b[i].id < POOL_USER_MAX_NUM) //用户id号是用户数组的序号
        {
            stat->userUsedRate[b[i].id] = (long long)(stat->userUsed[b[i].id]) * 1000 / POOL_TOTLE_BYTE;   //千分比
        }
        
    }    
}



//不记用户名方式请求内存 和释放内存？
void *pool_malloc(int size)
{
    return __apply_new_block(DEAULT_USER_ID, size);    //最后一个id号POOL_USER_MAX_NUM - 1
}
void pool_free(void *handle)
{
    __free_block(handle);
}



//用户申请id
int pool_apply_user_id(char *name)
{
    int i;
    pool_user_t *u = pool_local_mg.user;

    if(name == NULL)
    {
        u[DEAULT_USER_ID].en = 1;
        return DEAULT_USER_ID;
    }
                
     for(i = 0; i < POOL_USER_MAX_NUM; i++)
    {   
        if(u[i].en == 0) //用户id号是用户数组的序号
        {
            u[i].en = 1;
            
            strncpy(u[i].name, name, POOL_USER_NAME_MAX_LEN);
            return i;
        }
        
    }   
    return DEAULT_USER_ID;
}

//用户通过ID号来释放内存
void pool_free_by_userId(int userId)
{
    int i;
    pool_local_mg.user[userId].en = 0;

    block_obj_t *b = pool_local_mg.block;
    for(i = 0; i < POOL_BLOCK_MAX_NUM; i++)
    {   
        if( b[i].handle != NULL && b[i].id == userId) 
        {
            b[i].en = 0;
            free(b[i].handle);
        }        
    }
}




//用户malloc接口
void *pool_user_malloc(int id, int size)
{
    void *ret = __apply_new_block(id, size);

    if(ret == NULL)
    {
        pool_local_mg.mallocFaildCnt[id]++;
    }

    return ret;
}

//用户free接口
void pool_user_free(void *handle)
{
    __free_block(handle);
}




//===========================================
//给一个访问用户数组的接口
pool_user_t *get_pool_user_handle()
{
    return (pool_user_t *)(pool_local_mg.user);
}


//统计测试
void pool_show()
{
    int i;
    pool_user_t *user = get_pool_user_handle();
    
    pool_stat_t stat = {0};
    pool_statistic(&stat);
    printf("====================pool show===================\n");
    //printf("")
    printf("内存池的总量=%d 字节\n"
            "内存池已经使用=%d 字节\n"
            "内存池使用占比=%d 千分比\n"
            "内存池剩余字节数=%d 字节\n"
            "内存池剩占比=%d 千分比\n"
            "总共块的个数：%d\n"
            "分配块的个数：%d\n"
            "请求内存总次数：%d\n"
            "释放内存总次数:%d\n",
            
            stat.totle,
            stat.used,
            stat.usedRate,
            stat.residue,
            stat.residueRate,
            POOL_BLOCK_MAX_NUM,
            stat.blockCnt,
            stat.mallocCnt,
            stat.freeCnt);
    for(i = 0; i < POOL_USER_MAX_NUM; i++)
    {  
        //if(stat.userEn[i] == 1)
        if(user[i].en == 1)
        {
            printf("用户%d\t[%s]\t\t使用字节：%d字节\t        占比：%d 千分比\t内存分配失败次数:%d\n",
                    i,
                    user[i].name,
                    stat.userUsed[i],
                    stat.userUsedRate[i],
                    stat.mallocFaildCnt[i]);
        }
    }
    printf("====================== end =====================\n"); 
}





void pool_test()
{
    pool_init();
    int id = pool_apply_user_id("main.c");
    void *ptr = pool_user_malloc(id, 1024 *100);
    void *ptr2 = pool_user_malloc(id, 421);
    id = pool_apply_user_id("main2.c");
    ptr = pool_user_malloc(id, 1024 * 200);
    ptr2 = pool_user_malloc(id, 421);
    id = pool_apply_user_id(NULL);
    ptr = pool_user_malloc(id, 1024 * 200);
    ptr2 = pool_user_malloc(id, 421);

    pool_user_free(ptr);
    pool_user_free(ptr2);
    pool_show();
}








