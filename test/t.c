

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>


int main()
{
	//printf("%08d\n", 100010001);
	char str[] = "root a=1 ";
	
	char bufa[256];
	char bufb[256];
	char bufc[256];
	
	sscanf(str, "%s%s%s", bufa, bufb, bufc);
	
	printf("%s %s %s\n", bufa, bufb, bufc);
	return 0;
}



