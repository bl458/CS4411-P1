#include <stdio.h>
#include <string.h>
#include <egos/gate.h>

#define BUF_SIZE		1024

void cat(char *src, char *dst){
	char buf[BUF_SIZE];
	unsigned long pos = 0;
	FILE *fp;

	if ((fp = fopen(src, "r")) == 0) {
		fprintf(stderr, "push: can't open '%s'\n", src);
		return;
	}

	for (;;) {
		int n = fread(buf, 1, sizeof(buf), fp);
		if (n == 0) {
			break;
		}
		bool r = gate_push(GRASS_ENV->servers[GPID_GATE], dst, pos, buf, n);
		if (!r) {
			fprintf(stderr, "push: gate_push to remote file %s failed\n", dst);
			exit(1);
		}
		pos += n;
	}
	fclose(fp);
}

int main(int argc, char **argv){
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s local-file [remote-file]\n", argv[0]);
		return 1;
	}
	cat(argv[1], argc == 2 ? argv[1] : argv[2]);
	return 0;
}
