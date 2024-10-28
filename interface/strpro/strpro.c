


//字符串处理

#include <stdio.h>
#include <string.h>
//#include <ctype.h>

#include "strpro.h"


//==============================================================
//清空字符串两边的空格
//容易出错点：参数 str_p 应该是指向【字符串指针】的指针，这样返回去【字符串指针】才会改变数值
void strpro_clean_space(char **str_p) {
    
    int len;
    int i;
    
    //char *str = *str_p;
    len = strlen(*str_p);
    while(**str_p == ' ')
        (*str_p)++;

    len = strlen(*str_p);
    for(i = 0; i < len; i++)
    {
        if((*str_p)[len - i - 1] == ' ')
            (*str_p)[len - i - 1] = '\0';
        else
            break;
    }

}


//去掉两边的双引号 （参考 去掉两边的空格）
void strpro_clean_by_ch(char **str_p, char ch)
{
    int len;
    int i;
    
    //char *str = *str_p;
    len = strlen(*str_p);
    while(**str_p == ch)
        (*str_p)++;

    len = strlen(*str_p);
    for(i = 0; i < len; i++)
    {
        if((*str_p)[len - i - 1] == ch)
            (*str_p)[len - i - 1] = '\0';
        else
            break;
    }
}


/*==============================================================
                        测试
==============================================================*/

void strpro_test()
{
//    int pos;
//    char *find;
//    printf("strpro test ...\n");
//
//    //char str[] = "1    d ";
//    char buf[512] = {0};
//
//    char lineStr[] = "Accept-Language: en- US,好的en;q=0.8; new, old=100";
//
//    find = strstr(lineStr, ":");
//
//    pos = find - lineStr;
//    memcpy(buf, lineStr + pos + 1, 5);
//    buf[5] = '\0';
//    
//    printf("【%s】\n", buf);
//    printf("【%s】\n",  strpro_clean_space(&buf));

}



