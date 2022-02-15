#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv){
	char buf[1024];

	getcwd(buf, 1024);
	buf[1023] = 0;
	printf("%s\n", buf);

	return 0;
}
