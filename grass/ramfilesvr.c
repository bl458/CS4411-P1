/* This implements the ram file server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <earth/earth.h>
#include <egos/malloc.h>
#include <egos/syscall.h>
#include <egos/file.h>
#include <egos/gate.h>
#include <earth/intf.h>
#include "process.h"

#define MAX_FILES	100
#define BUF_SIZE	1024

/* Contents of a file.
 */
struct file {
	struct file_control_block fcb;
	char *contents;			// contents of file
};

struct ramfile_state {
	gpid_t gate;
	unsigned long start_time;		// local starting time
	struct file files[MAX_FILES];
};

static void ramfile_respond(gpid_t src, enum file_status status,
				void *data, unsigned int size){
	struct file_reply *rep = new_alloc_ext(struct file_reply, size);
	rep->status = status;
	memcpy(&rep[1], data, size);
	sys_send(src, MSG_REPLY, rep, sizeof(*rep) + size);
	m_free(rep);
}

static bool ramfile_read_allowed(struct file *file, unsigned int uid) {
	if (uid == 0) {
		// read from root user
		return true;
	} else if (uid == file->fcb.st_uid) {
		// read from owner
		return file->fcb.st_mode & P_FILE_OWNER_READ;
	} else {
		// read from other
		return file->fcb.st_mode & P_FILE_OTHER_READ;
	}
}

static bool ramfile_write_allowed(struct file *file, unsigned int uid) {
	if (uid == 0) {
		// write from root user
		return true;
	} else if (uid == file->fcb.st_uid) {
		// write from owner
		return file->fcb.st_mode & P_FILE_OWNER_WRITE;
	} else {
		// write from other
		return file->fcb.st_mode & P_FILE_OTHER_WRITE;
	}
}


/* Respond to a create request.
 */
static void ramfile_do_create(struct ramfile_state *rs, struct file_request *req, gpid_t src, unsigned int uid){
	/* See if there's a free file.
	 */
	unsigned int file_no;

	for (file_no = 1; file_no < MAX_FILES; file_no++) {
		if (!rs->files[file_no].fcb.st_alloc) {
			break;
		}
	}
	if (file_no == MAX_FILES) {
		printf("ramfile_do_create: out of files\n\r");
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	rs->files[file_no].fcb.st_alloc = true;
	rs->files[file_no].fcb.st_dev = sys_getpid();
	rs->files[file_no].fcb.st_ino = file_no;
	rs->files[file_no].fcb.st_mode = req->mode;
	rs->files[file_no].fcb.st_uid = uid;
	rs->files[file_no].fcb.st_modtime = rs->files[0].fcb.st_modtime +
						(sys_gettime() - rs->start_time) / 1000;

	/* Send response.
	 */
	struct file_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = FILE_OK;
	rep.file_no = file_no;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a read request.
 */
static void ramfile_do_read(struct ramfile_state *rs, struct file_request *req, gpid_t src, unsigned int uid){
	if (!rs->files[req->file_no].fcb.st_alloc || req->file_no >= MAX_FILES) {
		printf("ramfile_do_read: bad file number: %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	struct file *file = &rs->files[req->file_no];
	unsigned int n;

	// check permission
	if (!ramfile_read_allowed(file, uid)) {
		printf("ramfile_do_read: permission denied: %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	/* Allocate room for the reply.
	 */
	struct file_reply *rep = new_alloc_ext(struct file_reply, req->size);

	// try reading
	if (req->offset < file->fcb.st_size) {
		n = file->fcb.st_size - req->offset;
		if (n > req->size) {
			n = req->size;
		}
		memcpy(&rep[1], &file->contents[req->offset], n);
	}
	else {
		n = 0;
	}

	rep->status = FILE_OK;
	rep->op = FILE_READ;
	rep->fcb.st_size = n;
	sys_send(src, MSG_REPLY, rep, sizeof(*rep) + n);
	m_free(rep);
}

/* Code for adding content to a file.
 */
static void ramfile_put(struct ramfile_state *rs, unsigned int file_no,
				unsigned long offset, char *data, unsigned int size){
	struct file *file = &rs->files[file_no];
	if (offset + size > file->fcb.st_size) {
		/* TODO.  Should zero-fill if creating a hole.  Or just not allow
		 *		  creating holes.
		 */
		file->contents = m_realloc(file->contents, offset + size);
		file->fcb.st_size = offset + size;
	}
	memcpy(&file->contents[offset], data, size);
	file->fcb.st_modtime = rs->files[0].fcb.st_modtime +
						(sys_gettime() - rs->start_time) / 1000;
}

/* Respond to a write request.
 */
static void ramfile_do_write(struct ramfile_state *rs, struct file_request *req,
			gpid_t src, unsigned int uid, void *data, unsigned int size){
	if (req->file_no >= MAX_FILES || !rs->files[req->file_no].fcb.st_alloc) {
		printf("ramfile_do_write: bad inode %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	if (req->size != size) {
		printf("ramfile_do_write: size mismatch %u %u\n\r", req->size, size);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	// check permission
	if (!ramfile_write_allowed(&rs->files[req->file_no], uid)) {
		printf("ramfile_do_write: permission denied: %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	ramfile_put(rs, req->file_no, req->offset, data, size);
	ramfile_respond(src, FILE_OK, 0, 0);
}

/* Respond to a stat request.
 */
static void ramfile_do_stat(struct ramfile_state *rs, struct file_request *req,
			gpid_t src, unsigned int uid){
	if (!rs->files[req->file_no].fcb.st_alloc || req->file_no >= MAX_FILES) {
		printf("ramfile_do_stat: bad inode %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	// check permission
	if (!ramfile_read_allowed(&rs->files[req->file_no], uid)) {
		// printf("ramfile_do_stat: permission denied: %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	/* Send size of file.
	 */
	struct file_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = FILE_OK;
	rep.op = FILE_STAT;
	rep.fcb = rs->files[req->file_no].fcb;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize request.
 */
static void ramfile_do_setsize(struct ramfile_state *rs, struct file_request *req,
			gpid_t src, unsigned int uid){
	if (req->file_no >= MAX_FILES || !rs->files[req->file_no].fcb.st_alloc) {
		printf("ramfile_do_setsize: bad inode %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	if (req->offset > rs->files[req->file_no].fcb.st_size) {
		printf("ramfile_do_setsize: bad size %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	// check permission
	if (!ramfile_write_allowed(&rs->files[req->file_no], uid)) {
		printf("ramfile_do_setsize: permission denied: %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	struct file *f = &rs->files[req->file_no];
	f->fcb.st_size = req->offset;
	f->contents = m_realloc(f->contents, f->fcb.st_size);
	f->fcb.st_modtime = sys_gettime();
	ramfile_respond(src, FILE_OK, 0, 0);
}

/* Respond to a delete request.
 */
static void ramfile_do_delete(struct ramfile_state *rs, struct file_request *req,
			gpid_t src, unsigned int uid){
	if (req->file_no >= MAX_FILES || !rs->files[req->file_no].fcb.st_alloc) {
		printf("ramfile_do_delete: bad inode %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	if (!ramfile_write_allowed(&rs->files[req->file_no], uid)) {
		printf("ramfile_do_delete: permission denied: %u\n\r", req->file_no);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	struct file *f = &rs->files[req->file_no];
	f->fcb.st_alloc = 0;
	f->fcb.st_size = 0;
	m_free(f->contents);
	f->contents = 0;
	ramfile_respond(src, FILE_OK, 0, 0);
}

/* Respond to a chown request.
 */
static void ramfile_do_chown(struct ramfile_state *rs, struct file_request *req,
			gpid_t src, unsigned int uid) {
    if (req->file_no >= MAX_FILES || !rs->files[req->file_no].fcb.st_alloc) {
        printf("ramfile_do_chown: bad inode: %u\n\r", req->file_no); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }
    if (uid != 0) {
        printf("ramfile_do_chown: permission denied for uid %u\n\r", uid); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }

    rs->files[req->file_no].fcb.st_uid = req->uid;

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a chmod request.
 */
static void ramfile_do_chmod(struct ramfile_state *rs, struct file_request *req,
			gpid_t src, unsigned int uid) {
    if (req->file_no >= MAX_FILES || !rs->files[req->file_no].fcb.st_alloc) {
        printf("ramfile_do_chown: bad inode: %u\n\r", req->file_no); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }
    if (uid != 0 && uid != rs->files[req->file_no].fcb.st_uid) {
        printf("ramfile_do_chmod: permission denied for uid %u\n\r", uid); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }

    rs->files[req->file_no].fcb.st_mode = req->mode;

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

static void ramfile_cleanup(void *arg){
	struct ramfile_state *rs = arg;
	unsigned int i;

	printf("ram file server: cleaning up\n\r");

	for (i = 0; i < MAX_FILES; i++) {
		struct file *f = &rs->files[i];

		if (f->fcb.st_alloc) {
			m_free(f->contents);
		}
	}
	m_free(rs);
}

/* A simple in-memory file server.  All files are simply contiguous
 * memory regions.
 */
static void ramfile_proc(void *arg){
	struct ramfile_state *rs = arg;

	printf("RAM FILE SERVER: pid=%u\n\r", sys_getpid());

	/* Get the current external time and the local time;
	 */
	struct gate_time gt;
	bool r = gate_gettime(rs->gate, &gt);
	assert(r);

	rs->files[0].fcb.st_alloc = true;
	rs->files[0].fcb.st_modtime = gt.seconds;
	rs->start_time = sys_gettime();

	proc_current->finish = ramfile_cleanup;

	struct file_request *req = new_alloc_ext(struct file_request, PAGESIZE);
	for (;;) {
		gpid_t src;
		unsigned int uid;
		int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(*req) + PAGESIZE, &src, &uid);
		if (req_size < 0) {
			printf("ram file server shutting down\n\r");
			m_free(rs);
			m_free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));
		switch (req->type) {
		case FILE_CREATE:
			ramfile_do_create(rs, req, src, uid);
			break;
		case FILE_CHOWN:
            ramfile_do_chown(rs, req, src, uid);
            break;
        case FILE_CHMOD:
            ramfile_do_chmod(rs, req, src, uid);
            break;
		case FILE_READ:
			ramfile_do_read(rs, req, src, uid);
			break;
		case FILE_WRITE:
			ramfile_do_write(rs, req, src, uid, &req[1], req_size - sizeof(*req));
			break;
		case FILE_STAT:
			ramfile_do_stat(rs, req, src, uid);
			break;
		case FILE_SETSIZE:
			ramfile_do_setsize(rs, req, src, uid);
			break;
		case FILE_DELETE:
			ramfile_do_delete(rs, req, src, uid);
			break;
		default:
			printf("ram file server: bad request type: %u\n\r", req->type);
			ramfile_respond(src, FILE_ERROR, 0, 0);
		}
	}
}

gpid_t ramfile_init(gpid_t gate){
	struct ramfile_state *rs = new_alloc(struct ramfile_state);
	rs->gate = gate;

	return proc_create(1, "ramfile", ramfile_proc, rs);
}
