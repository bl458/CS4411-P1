#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/dir.h>
#include <egos/spawn.h>

/* Execute the given executable by file name and return a process id.
 */
bool spawn_fexec(const char *exec, const char *argb, int total, bool interruptable,
								unsigned int uid, gpid_t *ppid){
	fid_t dir, fid;

	/* Find the inode of the executable.
	 */
	if (!flookup(exec, false, &dir, &fid)) {
		printf("spawn_fexec: can't find '%s'\n", exec);
		return false;
	}

	return spawn_exec(GRASS_ENV->servers[GPID_SPAWN], fid, argb, total,
							interruptable, uid, ppid);
}

bool spawn_vexec(const char *exec, int argc, char *const *argv, bool interruptable,
												unsigned int uid, gpid_t *ppid){
	char *argb;
	unsigned int size;

	if (!spawn_load_args(GRASS_ENV, argc, argv, &argb, &size)) {
		return false;
	}
	bool status = spawn_fexec(exec, argb, size, interruptable, uid, ppid);
	free(argb);
	return status;
}
