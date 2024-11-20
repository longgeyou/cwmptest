


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



char hex2str_map_g[]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
char str2hex_map_g[]={
    -1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,0,
    1,2,3,4,5,6,7,8,9,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,10,11,
    12,13,14,15
};
//任意数据，看成是16进制数，然后转化为 字符串
int strpro_hex2str(void *hex, int hexLen, char *out, int outLen)
{
    if(hex == NULL || hexLen <= 0 || out == NULL || outLen <= 0)return -1;
    if(outLen  < (hexLen * 2 + 1) )return -2;   //outLen 长度不够
        
    int cnt;
    int i;
    char *seek = hex;

    cnt = 0;
    for(i = 0; i < hexLen; i++)
    {
        out[cnt++] = hex2str_map_g[seek[i] / 16];
        out[cnt++] = hex2str_map_g[seek[i] % 16];
    }
    out[cnt] = '\0';
    return 0;
}

//字符串转化为16 进制数
int strpro_str2hex(char *in, void *hex, int hexLen)
{
    if(hex == NULL || hexLen <= 0 || in == NULL || in <= 0)return -1;
    int inLen = strlen(in);
    if(hexLen * 2 < inLen)return -2;   //hexLen 长度不够
    if(inLen % 2 != 0)return -3;    // inLen 长度应该是偶数
        
    int cnt;
    int i;
    unsigned char *seek = (unsigned char *)hex;

    cnt = 0;
    for(i = 0; i < inLen; i += 2)
    {
        seek[cnt++] = str2hex_map_g[(int)(in[i])] * 16 + str2hex_map_g[(int)(in[i + 1])] ;
    }

    return 0;
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



    //任意数据 和 字符串的转换测试
    char buf128[128] = {0};
    int a = 0x12345678;
    strpro_hex2str((void *)(&a), sizeof(a), buf128, 128);

    int b;
    strpro_str2hex(buf128, (void *)(&b), sizeof(b));

    printf("result:%s b:%x\n", buf128, b);

}



