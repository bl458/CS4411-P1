#ifndef _EGOS_FILE_H
#define _EGOS_FILE_H

#include <stdbool.h>
#include <earth/earth.h>
#include <egos/syscall.h>

#define FILE_MAX_MSG_SIZE		PAGESIZE

/* Flags.
 */
#define FILE_ECHO			(1L << 0)

/* permission bits for file access control
 */
#define P_FILE_OTHER_READ			0x1
#define P_FILE_OTHER_WRITE			0x2
#define P_FILE_OWNER_READ			0x4
#define P_FILE_OWNER_WRITE			0x8
#define P_FILE_DEFAULT				(P_FILE_OWNER_READ | P_FILE_OWNER_WRITE | P_FILE_OTHER_READ)
#define P_FILE_ALL					(P_FILE_OWNER_READ | P_FILE_OWNER_WRITE | P_FILE_OTHER_READ | P_FILE_OTHER_WRITE)

typedef unsigned short gmode_t;

/* This is analogous to an i-node for a file on a Linux filesystem, but it 
 * contains only the metadata about the file (permissions, etc.), not its 
 * block storage location. Each FCB corresponds to exactly one i-node on the
 * block storage system containing the data for that file.
 */
struct file_control_block {
    bool st_alloc;				// set when allocated
	gpid_t st_dev;				// server pid
	unsigned int st_ino;		// file number, still called "ino" for compatibility with Linux's struct stat
	unsigned int st_uid;		// owner uid
	gmode_t st_mode;			// permission bits
    unsigned long st_size;		// size of file in bytes
	time_t st_modtime;			// last data modification time
};

struct file_request {
	enum file_op {
		FILE_UNUSED,				// to simplify finding bugs
		FILE_CREATE,
		FILE_CHOWN,
		FILE_CHMOD,
		FILE_READ,
		FILE_WRITE,
		FILE_STAT,					// get status info
		FILE_SETSIZE,				// size is in field offset
		FILE_DELETE,
		FILE_SYNC,

		/* Special commands for tty server.
		 */
		FILE_SET_FLAGS,				// encoded in offset
	} type;							// type of request
	unsigned int file_no;			// file number
	unsigned long offset;			// offset
	unsigned int size;				// amount to read/write
	unsigned int uid;				// uid for chown
	gmode_t mode;					// mode for create and chmod
};

struct file_reply {
	enum file_status { FILE_OK, FILE_ERROR } status;
	unsigned int file_no;			// for FILE_CREATE only
	enum file_op op;				// operation type in request
	struct file_control_block fcb;	// information about file
};

bool file_exist(gpid_t svr, unsigned int file_no);
bool file_create(gpid_t svr, gmode_t mode, unsigned int *p_fileno);
bool file_chown(gpid_t svr, unsigned int file_no, unsigned int uid);
bool file_chmod(gpid_t svr, unsigned int file_no, gmode_t mode);
bool file_read(gpid_t svr, unsigned int file_no, unsigned long offset,
										void *addr, unsigned int *psize);
bool file_write(gpid_t svr, unsigned int file_no, unsigned long offset,
										const void *addr, unsigned int size);
bool file_stat(gpid_t svr, unsigned int file_no, struct file_control_block *pfcb);
bool file_setsize(gpid_t svr, unsigned int file_no, unsigned long size);
bool file_delete(gpid_t svr, unsigned int file_no);
bool file_set_flags(gpid_t svr, unsigned int file_no, unsigned long flags);
bool file_sync(gpid_t svr, unsigned int file_no);

#endif // _EGOS_FILE_H
