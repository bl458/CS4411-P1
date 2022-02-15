#include <earth/earth.h>
#include <earth/intf.h>

void _start(void){
	extern int main(int argc, char **argv, char **envp);

	(void) main(0, 0, 0);
}

int _print_output(const char *buf, unsigned int size){
	earth.log.print(buf, size);
	return (int) size;
}
