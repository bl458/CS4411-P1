#ifdef TCC
#include "../tcc/tcc.c"
#else

#include <stdio.h>

int main(){
	fprintf(stderr, "no tcc compiler available\n");
	return 1;
}

#endif
