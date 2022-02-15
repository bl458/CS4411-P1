#ifndef _EGOS_SPAWN_H
#define _EGOS_SPAWN_H

#include <stdbool.h>
#include <egos/syscall.h>

struct spawn_request {
	enum {
		SPAWN_UNUSED,				// simplifies finding bugs
		SPAWN_EXEC,
		SPAWN_KILL,
		SPAWN_GETUID,
		SPAWN_SET_DESCR,
		SPAWN_SHUTDOWN,
	} type;							// type of request

	union {
		struct {
			fid_t executable;		// identifies executable
			unsigned int size;		// size of args/env that follows request
			bool interruptable;	// foreground process
			unsigned int uid;		// requested uid
		} exec;
		struct {
			gpid_t pid;
			int status;
		} kill;
		struct {
			gpid_t pid;
		} set_descr;
		struct {
			gpid_t pid;
		} getuid;
	} u;
};

struct spawn_reply {
	enum spawn_status { SPAWN_OK, SPAWN_ERROR } status;
	union {
		gpid_t pid;					// process id of process
		unsigned int uid;			// user id of process
	} u;
};

bool spawn_exec(gpid_t svr, fid_t executable, const char *args, unsigned int size,
						bool interruptable, unsigned int uid, gpid_t *ppid);
bool spawn_kill(gpid_t svr, gpid_t pid, int status);
bool spawn_shutdown(gpid_t svr);
bool spawn_getuid(gpid_t svr, gpid_t pid, unsigned int *p_uid);
bool spawn_set_descr(gpid_t svr, gpid_t pid, char *descr);
bool spawn_load_args(const struct grass_env *ge_init,
							int argc, char *const *argv,
							char **p_argb, unsigned int *p_size);

/* TODO.  This doesn't really belong here since it is user-space only.
 * Maybe lib/spawn.h
 */
bool spawn_fexec(const char *exec, const char *argb, int total, bool interruptable,
										unsigned int uid, gpid_t *ppid);
bool spawn_vexec(const char *exec, int argc, char *const *argv, bool interruptable,
										unsigned int uid, gpid_t *ppid);

#endif // _EGOS_SPAWN_H
