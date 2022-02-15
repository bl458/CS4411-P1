#include <stdarg.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int abs(int i){
	return i < 0 ? -i : i;
}

long labs(long i){
	return i < 0 ? -i : i;
}

#ifdef notdef
char *getenv(const char *name){
	extern char **environ;
	int i;
	const char *e, *p;

	if (environ == 0) {
		return 0;
	}
	for (i = 0; (e = environ[i]) != 0; i++) {
		p = name;
		while (*p == *e) {
			if (*p == 0) {
				break;
			}
			p++, e++;
		}
		if (*p == 0 && *e == '=') {
			return (char *) e + 1;
		}
	}
	return 0;
}
#else
char *getenv(const char *name){
	return NULL;
}
#endif

static int randstate;

void srand(unsigned int seed){
	randstate = (long) seed;
}

int rand(void){
	return ((randstate = randstate * 214013L + 2531011L) >> 16) & 0x7fff;
}

void abort(void){
	printf("ABORT\n");
	sys_exit(1);
	for (;;)
		;
}

void exit(int status){
	sys_exit(status);
	fprintf(stderr, "exit: returned???\n");
	for (;;);
}
