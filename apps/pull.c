#include <stdio.h>
#include <string.h>
#include <egos/gate.h>

#define BUF_SIZE		1024

void cat(char *src, char *dst){
	char buf[BUF_SIZE];
	unsigned long pos = 0;
	FILE *fp = dst == 0 ? stdout : 0;

	for (;;) {
		unsigned int size = BUF_SIZE;
		bool r = gate_pull(GRASS_ENV->servers[GPID_GATE], src, pos, buf, &size);
		if (!r) {
			fprintf(stderr, "pull: gate_pull from remote file %s failed\n", src);
			exit(1);
		}
		if (fp == 0 && (fp = fopen(dst, "w")) == 0) {
			fprintf(stderr, "%s: can't open local file '%s'\n", dst);
			exit(1);
		}
		if (size == 0) {
			break;
		}
		(void) fwrite(buf, 1, size, fp);
		pos += size;
	}
	fclose(fp);
}

int main(int argc, char **argv){
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s remote-file [local-file]\n", argv[0]);
		return 1;
	}
	cat(argv[1], argv[2]);
	return 0;
}
