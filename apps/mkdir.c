#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/dir.h>

static void mkdir(char *name){
	/* Append ".dir".
	 */
	char *path;
	asprintf(&path, "%s.dir", name);

	/* Make sure it doesn't already exist, and find the parent dir.
	 */
	fid_t dir, fid;
	dir = fid_val(0, 0);
	bool success = flookup(path, false, &dir, &fid);
	if (success) {
		fprintf(stderr, "mkdir: '%s' already exists\n", path);
		return;
	}
	if (dir.server == 0) {
		fprintf(stderr, "mkdir: no such parent directory\n", path);
		return;
	}

	/* Find the last component in path.
	 */
	char *last = rindex(path, '/');
	if (last == 0) {
		last = path;
	}
	else {
		last++;
	}

	/* Finally create the directory.
	 */
	success = dir_create(GRASS_ENV->servers[GPID_DIR], dir, last, 0);
	if (!success) {
		fprintf(stderr, "mkdir: can't create '%s'\n", path);
	}
}

int main(int argc, char **argv){
	int i;

	for (i = 1; i < argc; i++) {
		mkdir(argv[i]);
	}
	return 0;
}
