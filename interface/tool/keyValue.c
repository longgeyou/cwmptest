

/*key value键值对处理
    1、和字典dic很像，和本目录下的dic.c的区别是：dic.c 用的
        是散列表存储方式，地址冲突处理方案是 “链地址法”;
    2、键值对的实现有两种方案，一个是单个链表实现，特点是简
        单，且长度可变化；
    3、第二种方案是用线性表（参考list.c）存储法，特点也是简
        单，容量一般不可变化；
    4、决定：考虑先用线性表方式实现。
*/


#include <stdio.h>
#include <string.h>
#include "keyValue.h"
#include "list.h"


























