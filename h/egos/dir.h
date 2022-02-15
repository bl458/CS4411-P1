#ifndef _EGOS_DIR_H
#define _EGOS_DIR_H

#include <stdbool.h>
#include <egos/syscall.h>
#include <egos/file.h>

#define DIR_ENTRY_SIZE		32
#define DIR_NAME_SIZE		(DIR_ENTRY_SIZE - sizeof(fid_t))

/* Entry in the directory.
 *
 * Note: if fid.server == 0, the directory server will automatically overwrite
 *		 it with the pid of the file server on LOOKUP requests.
 */
struct dir_entry {
	fid_t fid;
	char name[DIR_NAME_SIZE];
};

struct dir_request {
	enum {
		DIR_UNUSED,
		DIR_LOOKUP,
		DIR_INSERT,
		DIR_REMOVE
	} type;	// type of request
	fid_t dir;							// identifies directory
	fid_t fid;							// to be inserted
	unsigned int size;					// size of pathname that follows
};

struct dir_reply {
	enum dir_status { DIR_OK, DIR_ERROR } status;
	fid_t fid;							// result of LOOKUP
};

/* Interface to directory service.
 */
bool dir_lookup(gpid_t svr, fid_t dir, const char *path, fid_t *pfid);
bool dir_insert(gpid_t svr, fid_t dir, const char *path, fid_t fid);
bool dir_remove(gpid_t svr, fid_t dir, const char *path, fid_t fid);
bool dir_create(gpid_t svr, fid_t dir, const char *path, fid_t *p_fid);
bool dir_create2(gpid_t dirsvr, gpid_t filesvr, gmode_t mode, fid_t dir, const char *path, fid_t *p_fid);

#endif // _EGOS_DIR_H
