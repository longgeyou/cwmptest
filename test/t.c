

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>


int main()
{
	char hex2str_map_g[]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	int i;
	
	
	char str2hex_map_g[103];	//'f' 为102
	/*for(i = 0; i < sizeof(hex2str_map_g); i++)
	{
		printf("i:%d ch:%c value:%d\n", i, hex2str_map_g[i], hex2str_map_g[i]);
		
	}*/
	
	for(i = 0; i < 103; i ++)
		str2hex_map_g[i] = -1;
	for(i = 0; i < sizeof(hex2str_map_g); i++)
	{
		//printf("i:%d ch:%c value:%d\n", i, hex2str_map_g[i], hex2str_map_g[i]);
		str2hex_map_g[hex2str_map_g[i]] = i;
	}
	for(i = 0; i < 103; i ++)
	{
		if((i + 1) % 10 == 0)printf("\n");
		printf("%d,", str2hex_map_g[i]);
		
	}
		
	return 0;
}



