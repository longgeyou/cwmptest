


//base64 的计算
/*
Table 1: The Base64 Alphabet

     Value Encoding  Value Encoding  Value Encoding  Value Encoding
         0 A            17 R            34 i            51 z
         1 B            18 S            35 j            52 0
         2 C            19 T            36 k            53 1
         3 D            20 U            37 l            54 2
         4 E            21 V            38 m            55 3
         5 F            22 W            39 n            56 4
         6 G            23 X            40 o            57 5
         7 H            24 Y            41 p            58 6
         8 I            25 Z            42 q            59 7
         9 J            26 a            43 r            60 8
        10 K            27 b            44 s            61 9
        11 L            28 c            45 t            62 +
        12 M            29 d            46 u            63 /
        13 N            30 e            47 v
        14 O            31 f            48 w         (pad) =
        15 P            32 g            49 x
        16 Q            33 h            50 y
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RET_OK 0
#define RET_FAILD -1


static const char __base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

//base64 编码
//注意 要保证字符数组 *out 的长度足够长
int base64_code(const char *in, char **out)
{
    int pos;    //从0开始
    int tmp;
    int result;
    int cnt;
    //int i;
    
    int inLen = strlen(in);
    char *buf = (char *)malloc(inLen + 8);
    if(buf == NULL)return -1;
	memcpy(buf, in, strlen(in));
    buf[inLen] = 0;
    
    result = 0;
    cnt = 0;
    pos = 0;
    while(1)
    {
        result = result * 2;

        if((buf[pos / 8] & (0x1 << (7 - (pos % 8)))) != 0)
            result += 1;
        if((pos + 1) % 6 == 0)
        {
            (*out)[cnt++] = __base64_chars[result];
            result = 0;
			
            if(pos / 8 > (inLen - 1))break;
        }
        pos++;
    }
    tmp = inLen % 3;
	
    if(tmp == 2)
    {
        (*out)[cnt++] = '=';       
    }        
    else if(tmp == 1)
    {
        (*out)[cnt++] = '=';
        (*out)[cnt++] = '=';
    }
	(*out)[cnt] = '\0';
    free(buf); 
    return 0;
}


//base64 解码
int base64_decode(const char *in, char **out)
{
	//int mark;
	int inLen;
	char *buf;	
	int outSize;
	int tmp;
	int pos;
	int i;
	int result;
	int cnt;
	
	inLen = strlen(in);
	if(inLen <= 0 && inLen % 4 != 0)return RET_FAILD;	//参数判断
	
	buf = (char *)malloc(inLen + 8);
	memcpy(buf, in, inLen);
#define MAP_LEN 256	
#define BASE64_CHAR_NUM 64
	//解码映射表
	char __base64_map[MAP_LEN + 8] = {0};
	for(i = 0; i < BASE64_CHAR_NUM; i++)
		__base64_map[(unsigned char)(__base64_chars[i])] = i;

	//计算结果的字节数
	tmp = inLen / 4 * 3;
	outSize = tmp;
	if(inLen >= 1 && in[inLen - 1] == '=')
	{
        outSize = tmp - 1;	
	}
	
	if(inLen >= 2 && in[inLen - 2] == '=')
	{
        outSize = tmp - 2;
	}
			
	
	pos = 0;
	result = 0;
	cnt = 0;
	while(1)
	{
		result *= 2;
		
		if((__base64_map[(unsigned char)(buf[pos / 6])] & (0x1 << (5 - (pos % 6)))) != 0)
			result += 1;
		if((pos + 1) % 8 == 0)
		{
			(*out)[cnt++] = result;

			//printf("-->cnt:%d result:%c outSize:%d pos:%d\n", cnt, result, outSize, pos);
			
			result = 0;
			if(cnt >=  outSize)
				break;
		}
		pos++;
	}
	(*out)[cnt] = '\0';
	free(buf);
	return 0;
}



//void  main()
void  base64_test()
{
    printf("base64 test ...\n");
#define BUF_SIZE 256
    char str[] = "hello=ffsd";
    char buf[BUF_SIZE] = {0};
	char *p = buf;
	
	char buf2[BUF_SIZE] = {0};
	char *p2 = buf2;

    base64_code(str, &p);
	
	base64_decode(buf, &p2);
	
    printf("str:%s buf:%s buf2:%s\n", str, buf, buf2);
    

    char bufTmp[] = "YWRtaW46MTIzNDU2A";
    p2 = buf2;
    base64_decode(bufTmp, &p2);
    printf("解码：%s\n", buf2);
}





