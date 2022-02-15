#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <assert.h>
#include <earth/earth.h>
#include <earth/intf.h>
#include <egos/malloc.h>
#include <egos/block.h>
#include "process.h"

/* State of the block server.
 */
struct disk_server_state {
	char *filename;
	struct dev_disk *dd;
};

struct disk_request {
	gpid_t pid, src;
	struct block_reply *rep;
};

static void disk_respond(struct block_request *req, enum block_status status,
                void *data, unsigned int nblock, gpid_t src){
    struct block_reply *rep = new_alloc_ext(struct block_reply, nblock * BLOCK_SIZE);
    rep->status = status;
    memcpy(&rep[1], data, nblock * BLOCK_SIZE);
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + nblock * BLOCK_SIZE);
	m_free(rep);
}

/* This is an interrupt handler, invoked when the read has completed.
 */
static void disk_read_complete(void *arg, bool success){
	struct disk_request *dr = arg;

	dr->rep->size_nblock = 1;
	if (success) {
		dr->rep->status = BLOCK_OK;
		proc_send(dr->pid, 0, dr->src, MSG_REPLY, dr->rep, sizeof(*dr->rep) + BLOCK_SIZE);
	}
	else {
		dr->rep->status = BLOCK_ERROR;
		proc_send(dr->pid, 0, dr->src, MSG_REPLY, dr->rep, sizeof(*dr->rep));
	}
	m_free(dr->rep);
	m_free(dr);
}

/* Respond to a read block request.
 */
static void disk_do_read(struct disk_server_state *dss, struct block_request *req, gpid_t src){
    if (req->ino != 0) {
        printf("disk_do_read %s: bad inode: %u\n\r", dss->filename, req->ino);
        disk_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct block_reply *rep = new_alloc_ext(struct block_reply, BLOCK_SIZE);

	/* Schedule the disk read operation.
	 */
	struct disk_request *dr = new_alloc(struct disk_request);
	dr->pid = sys_getpid();
	dr->src = src;
	dr->rep = rep;
	earth.dev_disk.read(dss->dd, req->offset_nblock, (char *) &rep[1], disk_read_complete, dr);
}

/* This is an interrupt handler, invoked when the write has completed.
 */
static void disk_write_complete(void *arg, bool success){
	struct disk_request *dr = arg;

	dr->rep->status = success ? BLOCK_OK : BLOCK_ERROR;
	dr->rep->size_nblock = 1;
	proc_send(dr->pid, 0, dr->src, MSG_REPLY, dr->rep, sizeof(*dr->rep));
	m_free(dr->rep);
	m_free(dr);
}

/* Respond to a write block request.
 */
static void disk_do_write(struct disk_server_state *dss, struct block_request *req,
														unsigned int size, gpid_t src){
	assert(size == BLOCK_SIZE);
    if (req->ino != 0) {
        printf("disk_do_write %s: bad inode: %u\n\r", dss->filename, req->ino);
        disk_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct block_reply *rep = new_alloc(struct block_reply);

	/* Schedule the disk read operation.
	 */
	struct disk_request *dr = new_alloc(struct disk_request);
	dr->pid = sys_getpid();
	dr->src = src;
	dr->rep = rep;
	earth.dev_disk.write(dss->dd, req->offset_nblock, (char *) &req[1], disk_write_complete, dr);
}

/* Respond to a getsize block request.
 */
static void disk_do_getsize(struct disk_server_state *dss, struct block_request *req, gpid_t src){
    if (req->ino != 0) {
        printf("disk_do_getsize: bad inode %u\n\r", req->ino);
        disk_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Send size of block store.
     */
    struct block_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = BLOCK_OK;
    rep.size_nblock = earth.dev_disk.getsize(dss->dd);
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize block request.
 */
static void disk_do_setsize(struct disk_server_state *dss, struct block_request *req, gpid_t src){
	printf("disk_do_setsize: not supported\n\r");
	disk_respond(req, BLOCK_ERROR, 0, 0, src);
}

/* Respond to a sync request
 */
static void disk_do_sync(struct disk_server_state *dss, struct block_request *req, gpid_t src){
	disk_respond(req, BLOCK_OK, 0, 0, src);
}

static void disk_proc(void *arg){
	struct disk_server_state *dss = arg;

	printf("DISK SERVER (data stored on %s): pid=%u\n\r", dss->filename, sys_getpid());

	snprintf(proc_current->descr, sizeof(proc_current->descr), "K %s", basename(dss->filename));

    struct block_request *req = new_alloc_ext(struct block_request, BLOCK_SIZE);
    for (;;) {
        gpid_t src;
		unsigned int uid;
        int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(*req) + BLOCK_SIZE, &src, &uid);
		if (req_size < 0) {
			printf("disk server shutting down\n\r");
			// m_free(dss);			-- events may still come in
			m_free(req);
			break;
		}
#ifdef notdef
		/* Unfortunately, this can happen while paging.
		 */
		if (uid != 0) {
			printf("disk_proc: bad uid: %u\n\r", uid);
			disk_respond(req, BLOCK_ERROR, 0, 0, src);
			continue;
		}
#endif

        assert(req_size >= (int) sizeof(*req));

        switch (req->type) {
            case BLOCK_READ:
                disk_do_read(dss, req, src);
                break;
            case BLOCK_WRITE:
                disk_do_write(dss, req, req_size - sizeof(*req), src);
                break;
            case BLOCK_GETSIZE:
                disk_do_getsize(dss, req, src);
                break;
            case BLOCK_SETSIZE:
                disk_do_setsize(dss, req, src);
                break;
            case BLOCK_SYNC:
                disk_do_sync(dss, req, src);
                break;
			default:
				printf("disk_proc: bad request: %u\n\r", req->type);
				disk_respond(req, BLOCK_ERROR, 0, 0, src);
		}
    }
}

/* Create a disk device.
 */
gpid_t disk_init(char *filename, unsigned int nblocks, bool sync){
	struct disk_server_state *dss = new_alloc(struct disk_server_state);
	dss->filename = filename;
	dss->dd = earth.dev_disk.create(filename, nblocks, sync);
	return proc_create(1, "disk", disk_proc, dss);
}
