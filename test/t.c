

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>


int main()
{
	#define A 1
	#undef A
	#define A 2
	
	printf("OK %d\n", A);
	return 0;
}



