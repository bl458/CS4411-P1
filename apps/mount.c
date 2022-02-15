#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <earth/earth.h>
#include <egos/dir.h>

int main(int argc, char **argv){
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "Usage: %s name filesvr [inode]\n", argv[0]);
		return 1;
	}
	char *path;
	asprintf(&path, "%s.dir", argv[1]);
	gpid_t server = atoi(argv[2]);
	unsigned int ino = argc == 4 ? atoi(argv[3]) : 0;

	/* Make sure it doesn't already exist, and find the parent dir.
	 */
	fid_t dir, fid;
	dir = fid_val(0, 0);
	bool success = flookup(path, false, &dir, &fid);
	if (success) {
		fprintf(stderr, "%s: '%s' already exists\n", argv[0], path);
		return 1;
	}
	if (dir.server == 0) {
		fprintf(stderr, "%s: no such parent directory\n", argv[0], path);
		return 1;
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

	if (ino == 0) {
		bool r = dir_create2(GRASS_ENV->servers[GPID_DIR], server,
			P_FILE_ALL, dir, last, 0);
		if (!r) {
			fprintf(stderr, "%s: dir_create2 '%s' %u failed\n", argv[0], path, server);
			return 1;
		}
	}
	else {
		/* Insert the new file in the directory.
		 */
		fid.server = server;
		fid.file_no = ino;
		bool r = dir_insert(GRASS_ENV->servers[GPID_DIR], dir, last, fid);
		if (!r) {
			fprintf(stderr, "%s: dir_insert %s failed\n", argv[0], path);
			return 1;
		}

		/* Update the .. entry.
		 */
		struct dir_entry de;
		strcpy(de.name, "...dir");
		de.fid = dir;
		r = file_write(server, fid.file_no, sizeof(de), &de, sizeof(de));
		assert(r);
	}
	return 0;
}
