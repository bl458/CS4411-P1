#include <stdio.h>
#include <string.h>

#define TEST_UNISTD

#ifdef TEST_UNISTD
#include <unistd.h>
#endif

#define BUF_SIZE		1024

#ifdef TEST_UNISTD

void copy(int fd){
	char buf[BUF_SIZE];

	for (;;) {
		int n = read(fd, buf, BUF_SIZE);
		if (n < 0) {
			sys_exit(1);
		}
		if (n == 0) {
			break;
		}
		(void) write(1, buf, n);
	}
}

#else // TEST_UNISTD

void copy(FILE *fp){
	char buf[BUF_SIZE];

	for (;;) {
		int n = fread(buf, 1, BUF_SIZE, fp);
		if (n < 0) {
			sys_exit(1);
		}
		if (n == 0) {
			break;
		}
		(void) fwrite(buf, 1, n, stdout);
	}
}

#endif // TEST_UNISTD

int cat(char *file){
	if (strcmp(file, "-") == 0) {
#ifdef TEST_UNISTD
		copy(0);
#else
		copy(stdin);
#endif
		return 1;
	}

#ifdef TEST_UNISTD
	int fd;
	if ((fd = open(file, 0)) >= 0) {
		copy(fd);
		close(fd);
		return 1;
	}
#else // TEST_UNISTD
	FILE *fp;
	if ((fp = fopen(file, "r")) != 0) {
		copy(fp);
		fclose(fp);
		return 1;
	}
#endif // TEST_UNISTD
	else {
		fprintf(stderr, "cat: can't open '%s'\n", file);
		return 0;
	}
}

int main(int argc, char **argv){
	int i;
	int status = 0;

	if (argc < 2) {
#ifdef TEST_UNISTD
		copy(0);
#else
		copy(stdin);
#endif
	}
	for (i = 1; i < argc; i++) {
		if (!cat(argv[i])) {
			status = 1;
		}
	}
	return status;
}
