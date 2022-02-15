#include <libgen.h>

char *basename(const char *path){
	const char *result = path;

	while (*path != 0) {
		if (*path++ == '/') {
			result = path;
		}
	}
	return (char *) result;
}
