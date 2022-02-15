#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <earth/earth.h>
#include <egos/syscall.h>
#include <egos/file.h>
#include <egos/dir.h>

#define MAX_PATH_NAME	1024
#define NENTRIES		(PAGESIZE / DIR_ENTRY_SIZE)

/* See if the name in de->name is the same as in s for the given size.
 */
static bool name_cmp(struct dir_entry *de, char *s, unsigned int size){
	if (strnlen(de->name, DIR_NAME_SIZE) != size) {
		return false;
	}
	return strncmp(de->name, s, size) == 0;
}

static void dir_respond(gpid_t src, enum dir_status status, fid_t *fid){
	struct dir_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = status;
	if (fid != 0) {
		rep.fid = *fid;
	}
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a lookup request.
 */
static void dir_do_lookup(struct dir_request *req, gpid_t src, unsigned int uid,
										char *name, unsigned int size){
	if (size > DIR_NAME_SIZE) {
		dir_respond(src, DIR_ERROR, 0);
		return;
	}

	/* See if the directory is readable for the user.
	 */
	if (uid != 0) {
		struct file_control_block stat;
		bool status = file_stat(req->dir.server, req->dir.file_no, &stat);
		if (!status) {
			dir_respond(src, DIR_ERROR, 0);
			return;
		}

		if (stat.st_uid == uid) {
			if (!(stat.st_mode & P_FILE_OWNER_READ)) {
				dir_respond(src, DIR_ERROR, 0);
				return;
			}
		}
		else {
			if (!(stat.st_mode & P_FILE_OTHER_READ)) {
				dir_respond(src, DIR_ERROR, 0);
				return;
			}
		}
	}

	/* Read the directory one page at a time.
	 */
	char *buf = malloc(PAGESIZE);
	unsigned long offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool status = file_read(req->dir.server, req->dir.file_no,
					offset, buf, &n);
		if (!status) {
			dir_respond(src, DIR_ERROR, 0);			// file error
			free(buf);
			return;
		}
		if (n == 0) {
			dir_respond(src, DIR_ERROR, 0);			// not found
			free(buf);
			return;
		}
		assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];
			if (name_cmp(de, name, size)) {
				if (de->fid.server == 0) {
					de->fid.server = req->dir.server;
				}
				dir_respond(src, DIR_OK, &de->fid);
				free(buf);
				return;
			}
		}
	}
}

/* Respond to an insert request.
 */
static void dir_do_insert(struct dir_request *req, gpid_t src, unsigned int uid,
									char *name, unsigned int size){
	if (size > DIR_NAME_SIZE) {
		fprintf(stderr, "dir_do_insert: name too long\n");
		dir_respond(src, DIR_ERROR, 0);
		return;
	}

	/* See if the directory is writable for the user.
	 */
	if (uid != 0) {
		struct file_control_block stat;
		bool status = file_stat(req->dir.server, req->dir.file_no, &stat);
		if (!status) {
			fprintf(stderr, "dir_do_insert: directory not writable\n");
			dir_respond(src, DIR_ERROR, 0);
			return;
		}

		if (stat.st_uid == uid) {
			if (!(stat.st_mode & P_FILE_OWNER_WRITE)) {
				fprintf(stderr, "dir_do_insert: owner can't write\n");
				dir_respond(src, DIR_ERROR, 0);
				return;
			}
		}
		else {
			if (!(stat.st_mode & P_FILE_OTHER_WRITE)) {
				fprintf(stderr, "dir_do_insert: other can't write\n");
				dir_respond(src, DIR_ERROR, 0);
				return;
			}
		}
	}

	/* Read the directory one page at a time.
	 */
	char *buf = malloc(PAGESIZE);
	bool found_free_entry = false;
	unsigned long offset, free_offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool status = file_read(req->dir.server, req->dir.file_no,
					offset, buf, &n);
		if (!status) {
			free(buf);
			fprintf(stderr, "dir_do_insert: can't read directory\n");
			dir_respond(src, DIR_ERROR, 0);			// file error
			return;
		}
		if (n == 0) {
			break;
		}
		if (n % DIR_ENTRY_SIZE != 0) {
			fprintf(stderr, "dir_do_insert: svr=%u file_no=%u offset=%u n=%d src=%d name=%s\n",
						req->dir.server, req->dir.file_no, offset, (int) n, (int) src, name);
		}
		assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];
			if (de->name[0] == 0) {
				if (!found_free_entry) {
					free_offset = offset;
					found_free_entry = true;
				}
			}
			else if (name_cmp(de, name, size)) {
				free(buf);
				fprintf(stderr, "dir_do_insert: already exists\n");
				dir_respond(src, DIR_ERROR, 0);			// already exists
				return;
			}
		}
	}

	/* Add the new entry.
	 */
	if (found_free_entry) {
		offset = free_offset;
	}
	struct dir_entry nde;
	memset(&nde, 0, sizeof(nde));
	strncpy(nde.name, name, size);
	nde.fid = req->fid;
	bool wstatus = file_write(req->dir.server, req->dir.file_no,
											offset, &nde, sizeof(nde));
	free(buf);
	dir_respond(src, wstatus ? DIR_OK : DIR_ERROR, &req->fid);
}

/* Respond to a remove request.
 */
static void dir_do_remove(struct dir_request *req, gpid_t src, unsigned int uid,
										char *name, unsigned int size){
	if (size > DIR_NAME_SIZE) {
		dir_respond(src, DIR_ERROR, 0);
		return;
	}

	/* See if the directory is writable for the user.
	 */
	if (uid != 0) {
		struct file_control_block stat;
		bool status = file_stat(req->dir.server, req->dir.file_no, &stat);
		if (!status) {
			dir_respond(src, DIR_ERROR, 0);
			return;
		}

		if (stat.st_uid == uid) {
			if (!(stat.st_mode & P_FILE_OWNER_WRITE)) {
				dir_respond(src, DIR_ERROR, 0);
				return;
			}
		}
		else {
			if (!(stat.st_mode & P_FILE_OTHER_WRITE)) {
				dir_respond(src, DIR_ERROR, 0);
				return;
			}
		}
	}

	/* Read the directory one page at a time.
	 */
	char *buf = malloc(PAGESIZE);
	unsigned long offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool status = file_read(req->dir.server, req->dir.file_no,
					offset, buf, &n);
		if (!status) {
			free(buf);
			dir_respond(src, DIR_ERROR, 0);			// file error
			return;
		}
		if (n == 0) {
			/* Didn't find it, but pretend it went ok.
			 */
			free(buf);
			dir_respond(src, DIR_OK, &req->fid);
			return;
		}
		assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];
			if (de->name[0] != 0
						&& memcmp(&de->fid, &req->fid, sizeof(de->fid)) == 0
						&& name_cmp(de, name, size)) {
				memset(de, 0, sizeof(*de));
				bool wstatus = file_write(req->dir.server, req->dir.file_no,
											offset, de, sizeof(*de));
				free(buf);
				dir_respond(src, wstatus ? DIR_OK : DIR_ERROR, &req->fid);
				return;
			}
		}
	}
}

/* The directory server.
 */
static void dir_proc(){
	printf("DIRECTORY SERVER: pid=%u\n\r", sys_getpid());

	struct dir_request *req = calloc(1, sizeof(struct dir_request) + MAX_PATH_NAME);
	for (;;) {
		gpid_t src;
		unsigned int uid;

		int req_size = sys_recv(MSG_REQUEST, 0,
								req, sizeof(req) + MAX_PATH_NAME, &src, &uid);
		if (req_size < 0) {
			printf("directory server terminating\n\r");
			free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));
		switch (req->type) {
		case DIR_LOOKUP:
			dir_do_lookup(req, src, uid,
						(char *) &req[1], req_size - sizeof(*req));
			break;
		case DIR_INSERT:
			dir_do_insert(req, src, uid,
						(char *) &req[1], req_size - sizeof(*req));
			break;
		case DIR_REMOVE:
			dir_do_remove(req, src, uid,
						(char *) &req[1], req_size - sizeof(*req));
			break;
		default:
			assert(0);
		}
	}
}

int main(int argc, char **argv){
	assert(sizeof(struct dir_entry) == DIR_ENTRY_SIZE);
	dir_proc();
	return 0;
}
