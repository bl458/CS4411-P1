#include <stdio.h>
#include <string.h>

int main(int argc, char **argv){
	int i, status = 0;
	bool silent = false;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-f") == 0) {
			silent = true;
		}
		else if (remove(argv[i]) != 0 && !silent) {
			fprintf(stderr, "remove %s failed\n", argv[i]);
			status = 1;
		}
	}
	return status;
}
