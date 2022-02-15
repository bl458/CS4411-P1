#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <earth/earth.h>
#include <egos/syscall.h>
#include <egos/queue.h>
#include <egos/file.h>
#include <egos/block.h>
#include <egos/gate.h>

struct file_server_state {
	gpid_t block_svr;

	/* Used to compute modification times in File Control Blocks.
	 */
	unsigned long global_time;
	unsigned int start_time;

	/* This file server maintains one File Control Block (FCB) for each
	 * underlying i-node.  They are kept as an array in i-node 0.
	 * fcb_cache is an in-memory cache of all these FCBs.
	 */
	struct file_control_block *fcb_cache;
	unsigned int num_fcbs;		// corresponds to underlying #inodes
};

// file_control_block is 16 bytes
#define STATS_PER_BLOCK      (BLOCK_SIZE / sizeof(struct file_control_block))

// these helper functions are declared here and defined later
static void flush_fcb_cache(struct file_server_state *, unsigned int file_no);
static void flush_stat_cache_all(struct file_server_state *);
static bool load_fcb_cache(struct file_server_state *);
static void blkfile_do_create(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_delete(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_chown(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_chmod(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_read(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_write(struct file_server_state *, struct file_request *req, void *data, unsigned int size, gpid_t src, unsigned int uid);
static void blkfile_do_sync(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_stat(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_do_setsize(struct file_server_state *, struct file_request *req, gpid_t src, unsigned int uid);
static void blkfile_respond(struct file_request *req, enum file_status status,
                void *data, unsigned int size, gpid_t src);
static bool blkfile_read_allowed(struct file_control_block *stat, unsigned int uid);
static bool blkfile_write_allowed(struct file_control_block *stat, unsigned int uid);

/* Reads several contiguous blocks from a block server, starting at a specific
 * inode number and offset. The data will be placed in the buffer pointed to by 
 * addr, assuming it is at least *p_nblocks * BLOCK_SIZE long. Updates *p_nblocks
 * to equal the number of blocks actually read.
 */
bool multiblock_read(gpid_t svr, unsigned int ino, unsigned int offset, void *addr, unsigned int *p_nblocks){
	unsigned int nblocks = *p_nblocks;
	unsigned int i;

	for (i = 0; i < nblocks; i++) {
// printf("BFS R %u %u\n", ino, offset);
		bool r = block_read(svr, ino, offset, addr);
		if (!r) {
			if (i == 0) {
				return false;
			}
			else {
				break;
			}
		}
		offset++;
		addr = (char *) addr + BLOCK_SIZE;
	}
	*p_nblocks = i;
	return true;
}

/* Writes several contiguous blocks to a block server, starting at the given 
 * inode number and offset. The data will be read from the buffer pointed to by
 * addr, assuming it is at least nblocks * BLOCK_SIZE long.
 */
bool multiblock_write(gpid_t svr, unsigned int ino, unsigned int offset, const void *addr, unsigned int nblocks){
	unsigned int i;

	for (i = 0; i < nblocks; i++) {
// printf("BFS W %u %u\n", ino, offset);
		bool r = block_write(svr, ino, offset, addr);
		if (!r) {
			return false;
		}
		offset++;
		addr = (char *) addr + BLOCK_SIZE;
	}
	return true;
}

/* A file server based on block server. Each file corresponds to an inode in the block server
 */
static void blkfile_proc(void *arg){
    printf("BLOCK FILE SERVER (BFS): pid=%u\n\r", sys_getpid());

	struct file_server_state *fss = arg;

    // initialize blkfile internal states
    if (!load_fcb_cache(fss)) {
        memset(fss->fcb_cache, 0, fss->num_fcbs * sizeof(*fss->fcb_cache));
        fss->fcb_cache[0].st_alloc = true;
		// TODO: should the following round up?
        fss->fcb_cache[0].st_size = fss->num_fcbs * sizeof(*fss->fcb_cache) / STATS_PER_BLOCK;
        flush_stat_cache_all(fss);
    }

    struct file_request *req =
		calloc(1, sizeof(struct file_request) + FILE_MAX_MSG_SIZE);
    for (;;) {
        gpid_t src;
		unsigned int uid;
        int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(*req) + FILE_MAX_MSG_SIZE, &src, &uid);
		if (req_size < 0) {
			printf("block file server terminated\n\r");
			free(req);
			free(fss);
			break;
		}

        assert(req_size >= (int) sizeof(*req));
        switch (req->type) {
        case FILE_CREATE:
            //fprintf(stderr, "!!DEBUG: calling blkfile create\n");
            blkfile_do_create(fss, req, src, uid);
            break;
        case FILE_DELETE:
            blkfile_do_delete(fss, req, src, uid);
            break;
        case FILE_CHOWN:
            //fprintf(stderr, "!!DEBUG: calling blkfile chown\n");
            blkfile_do_chown(fss, req, src, uid);
            break;
        case FILE_CHMOD:
            //fprintf(stderr, "!!DEBUG: calling blkfile chmod\n");
            blkfile_do_chmod(fss, req, src, uid);
            break;
        case FILE_READ:
            //fprintf(stderr, "!!DEBUG: calling blkfile read\n");
            blkfile_do_read(fss, req, src, uid);
            break;
        case FILE_WRITE:
            //fprintf(stderr, "!!DEBUG: calling blkfile write\n");
            blkfile_do_write(fss, req, &req[1], req_size - sizeof(*req), src, uid);
            break;
        case FILE_SYNC:
            //fprintf(stderr, "!!DEBUG: calling blkfile sync\n");
            blkfile_do_sync(fss, req, src, uid);
            break;
        case FILE_STAT:
            //fprintf(stderr, "!!DEBUG: calling blkfile stat\n");
            blkfile_do_stat(fss, req, src, uid);
            break;
        case FILE_SETSIZE:
            //fprintf(stderr, "!!DEBUG: calling blkfile create\n");
            blkfile_do_setsize(fss, req, src, uid);
            break;
        default:
            assert(0);
        }
    }
}

int main(int argc, char **argv){
	gpid_t block_server = argc > 1 ? (gpid_t) atoi(argv[1]) : GRASS_ENV->servers[GPID_BLOCK];

	/* See how many inodes the underlying block server has.
	 * Note that this file server maintain one file control block for
	 * each underlying i-node.
	 */
	unsigned int ninodes;
	bool r = block_getninodes(block_server, &ninodes);
	printf("BFS: %u inodes\n", ninodes);

	/* Get the current global time.
	 */
	struct gate_time gt;
	r = gate_gettime(GRASS_ENV->servers[GPID_GATE], &gt);
	assert(r);

	struct file_server_state *fss = calloc(1, sizeof(struct file_server_state));
    fss->block_svr = block_server;
	fss->global_time = gt.seconds;
	fss->start_time = sys_gettime();
	fss->num_fcbs = ninodes;
	fss->fcb_cache = calloc(ninodes, sizeof(*fss->fcb_cache));

    blkfile_proc(fss);
	return 0;
}

/* Respond to a create request.
 */
static void blkfile_do_create(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid) {
    unsigned int file_no;
    for(file_no = 1; file_no < fss->num_fcbs; file_no++) {
        if (!fss->fcb_cache[file_no].st_alloc) {
            break;
        }
    }

    if (file_no == fss->num_fcbs) {
        printf("blkfile_do_create: out of files\n");
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    fss->fcb_cache[file_no].st_alloc = true;
    fss->fcb_cache[file_no].st_dev = sys_getpid();
    fss->fcb_cache[file_no].st_ino = file_no;
    fss->fcb_cache[file_no].st_mode = req->mode;
    fss->fcb_cache[file_no].st_uid = uid;
	fss->fcb_cache[file_no].st_modtime = fss->global_time +
						(sys_gettime() - fss->start_time) / 1000;
    flush_fcb_cache(fss, file_no);

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    rep.file_no = file_no;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a delete request.
 */
static void blkfile_do_delete(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid) {
    if (req->file_no >= fss->num_fcbs) {
        printf("blkfile_do_delete: invalid req->file_no\n");
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // check permission
    if (!blkfile_write_allowed(&fss->fcb_cache[req->file_no], uid)) {
        printf("blkfile_do_delete: permission denied: %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;

    if (!fss->fcb_cache[req->file_no].st_alloc) {
        // already deleted
        sys_send(src, MSG_REPLY, &rep, sizeof(rep));
    } else {
        // delete the file
        fss->fcb_cache[req->file_no].st_alloc = false;
        flush_fcb_cache(fss, req->file_no);

        if (!block_setsize(fss->block_svr, req->file_no, 0)) {
            printf("blkfile_do_delete: bad size %u\n", req->file_no);
            blkfile_respond(req, FILE_ERROR, 0, 0, src);
        }
        sys_send(src, MSG_REPLY, &rep, sizeof(rep));
    }

}

/* Respond to a chown request.
 */
static void blkfile_do_chown(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid) {
    unsigned int file_no = req->file_no;
    if (file_no >= fss->num_fcbs || !fss->fcb_cache[file_no].st_alloc) {
        printf("blkfile_do_chown: bad inode: %u\n", req->file_no); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    if (uid != 0) {
        printf("blkfile_do_chown: permission denied for uid %u\n", uid); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    fss->fcb_cache[file_no].st_uid = req->uid;
    flush_fcb_cache(fss, file_no);

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a chmod request.
 */
static void blkfile_do_chmod(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid) {
    unsigned int file_no = req->file_no;
    if (file_no >= fss->num_fcbs || !fss->fcb_cache[file_no].st_alloc) {
        printf("blkfile_do_chmod: bad inode: %u\n", req->file_no); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    if (uid != 0 && uid != fss->fcb_cache[file_no].st_uid) {
        printf("blkfile_do_chmod: permission denied for uid %u\n", uid); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    fss->fcb_cache[file_no].st_mode = req->mode;
    flush_fcb_cache(fss, file_no);

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}


/* Respond to a read request.
 */
static void blkfile_do_read(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid){

    if (req->file_no >= fss->num_fcbs || !fss->fcb_cache[req->file_no].st_alloc) {
        if (req->size == 0 && req->file_no == 1) {
            // initializing dir server for new disk
            // print nothing
        } else {
            printf("blkfile_do_read: bad inode: %u\n", req->file_no);
        } 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    // check permission
    if (!blkfile_read_allowed(&fss->fcb_cache[req->file_no], uid)) {
        printf("blkfile_do_read: permission denied: %u, %u\n", req->file_no, fss->fcb_cache[req->file_no].st_mode);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct file_reply *rep = calloc(1, sizeof(struct file_reply) + req->size);
    char *contents = NULL;

    if (req->size == 0) {
        // the request is just checking whether the file exists
        rep->status = FILE_OK;
		rep->op = FILE_READ;
        rep->fcb.st_size = 0;
        sys_send(src, MSG_REPLY, rep, sizeof(*rep));

        free(rep);
        return;
    }

    unsigned int n;
    if (req->offset < fss->fcb_cache[req->file_no].st_size) {
        n = fss->fcb_cache[req->file_no].st_size - req->offset;
        if (n > req->size) {
            n = req->size;
        }

        // Read file parameters
        unsigned int start_block_no = req->offset / BLOCK_SIZE;
        unsigned int end_block_no = (req->offset + n - 1) / BLOCK_SIZE;
        unsigned int psize_nblock = end_block_no - start_block_no + 1;

        // Call block server, using the file number as the inode number
        contents = malloc(psize_nblock * BLOCK_SIZE);
        if (!multiblock_read(fss->block_svr, req->file_no, start_block_no, contents, &psize_nblock) 
          || psize_nblock != end_block_no - start_block_no + 1) {
            free(contents);
            free(rep);
            printf("blkfile_do_read: block server read error: %d %d\n", psize_nblock, end_block_no - start_block_no + 1);
            blkfile_respond(req, FILE_ERROR, 0, 0, src);
            return;
        }

        memcpy(&rep[1], &contents[req->offset % BLOCK_SIZE], n);
    }
    else {
        n = 0;
    }

    rep->status = FILE_OK;
	rep->op = FILE_READ;
    rep->fcb.st_size = n;
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + n);

    free(rep);
    if (contents != NULL)
        free(contents);
}

static int blkfile_put(struct file_server_state *fss, unsigned int file_no, unsigned long offset, void* data, unsigned int size) {

    // Get file size and nblock
    unsigned long file_size = fss->fcb_cache[file_no].st_size;

    // File read parameter
    unsigned long start_block_no = offset / BLOCK_SIZE;
    unsigned long final_size;
    unsigned long end_block_no;
    unsigned int psize_nblock; // how many to read from file

    if (offset + size > file_size) {
        final_size = offset + size;
        end_block_no = final_size / BLOCK_SIZE;
        psize_nblock = (file_size / BLOCK_SIZE) - start_block_no + 1;
    } else {
        final_size = file_size;
        end_block_no = (offset + size) / BLOCK_SIZE;
        psize_nblock = end_block_no - start_block_no + 1;
    }

    // Get file content
    char *contents = malloc((end_block_no - start_block_no + 1) * BLOCK_SIZE);
    memset(contents, 0, (end_block_no - start_block_no + 1) * BLOCK_SIZE);

    if (start_block_no > file_size / BLOCK_SIZE) {
        // Write directly to block server, using file_no as the inode number
        memcpy(&contents[offset % BLOCK_SIZE], data, size);
        if (!multiblock_write(fss->block_svr, file_no, start_block_no, contents, end_block_no - start_block_no + 1)) {
            free(contents);
            return -1;
        }
    } else {
        // Read file first
        if (file_size != 0 && 
            (!multiblock_read(fss->block_svr, file_no, start_block_no, contents, &psize_nblock))) {
            free(contents);
            return -1;
        }

        // Modify file content
        memcpy(&contents[offset % BLOCK_SIZE], data, size);

        // Write back to block store
        if (!multiblock_write(fss->block_svr, file_no, start_block_no, contents, end_block_no - start_block_no + 1)) {
            free(contents);
            return -1;
        }
    }

    free(contents);
    fss->fcb_cache[file_no].st_size = final_size;
	fss->fcb_cache[file_no].st_modtime = fss->global_time +
						(sys_gettime() - fss->start_time) / 1000;
    flush_fcb_cache(fss, file_no);
    return 0;
}

/* Respond to a write request.
 */
static void blkfile_do_write(struct file_server_state *fss, struct file_request *req, void *data, unsigned int size, gpid_t src, unsigned int uid){
    if (req->file_no >= fss->num_fcbs || !fss->fcb_cache[req->file_no].st_alloc) {
        printf("blkfile_do_write: bad inode %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    if (req->size != size) {
        printf("blkfile_do_write: size mismatch %u %u\n", req->size, size);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // check permission
    if (!blkfile_write_allowed(&fss->fcb_cache[req->file_no], uid)) {
        printf("blkfile_do_write: permission denied: %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    if (blkfile_put(fss, req->file_no, req->offset, data, size) == 0) {
        blkfile_respond(req, FILE_OK, 0, 0, src);
    } else {
        printf("blkfile_do_write: write error, offset: %lu, size: %u\n", req->offset, req->size);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
    }
}

/* Respond to a sync request.
 */
static void blkfile_do_sync(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid){
    if (req->file_no >= fss->num_fcbs && req->file_no != (unsigned int) -1) {
        printf("blkfile_do_sync: bad inode %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    bool r = block_sync(fss->block_svr, req->file_no);
    if (!r) {
        printf("blkfile_do_sync: sync error\n");
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

	blkfile_respond(req, FILE_OK, 0, 0, src);
}

/* Respond to a stat request.
 */
static void blkfile_do_stat(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid){
    if (req->file_no >= fss->num_fcbs || !fss->fcb_cache[req->file_no].st_alloc) {
        printf("blkfile_do_stat: bad inode %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    // check permission
    if (!blkfile_read_allowed(&fss->fcb_cache[req->file_no], uid)) {
        // printf("blkfile_do_stat: permission denied: %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    /* Send size of file.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
	rep.op = FILE_STAT;
    rep.fcb = fss->fcb_cache[req->file_no];
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize request.
 */
static void blkfile_do_setsize(struct file_server_state *fss, struct file_request *req, gpid_t src, unsigned int uid){
    if (req->file_no >= fss->num_fcbs || !fss->fcb_cache[req->file_no].st_alloc) {
        printf("blkfile_do_setsize: bad file number %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    if (req->offset > fss->fcb_cache[req->file_no].st_size) {
        printf("blkfile_do_setsize: bad size %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // check permission
    if (!blkfile_write_allowed(&fss->fcb_cache[req->file_no], uid)) {
        printf("blkfile_do_setsize: permission denied: %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // call block server set size
    unsigned int final_nblock = (req->offset == 0? 0 : 1 + (req->offset / BLOCK_SIZE));
    if (!block_setsize(fss->block_svr, req->file_no, final_nblock)) {
        printf("blkfile_do_setsize: bad size %u\n", req->file_no);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
    }

    fss->fcb_cache[req->file_no].st_size = req->offset;
	fss->fcb_cache[req->file_no].st_modtime = fss->global_time +
						(sys_gettime() - fss->start_time) / 1000;
    flush_fcb_cache(fss, req->file_no);
    blkfile_respond(req, FILE_OK, 0, 0, src);
}

static void blkfile_respond(struct file_request *req, enum file_status status,
                void *data, unsigned int size, gpid_t src){
    struct file_reply *rep = calloc(1, sizeof(struct file_reply) + size);
    rep->status = status;
    memcpy(&rep[1], data, size);
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + size);
    free(rep);
}

static void flush_stat_cache_all(struct file_server_state *fss) {
    unsigned int size = fss->num_fcbs * sizeof(*fss->fcb_cache);
    unsigned int end_block_no = (size - 1) / BLOCK_SIZE;

    if (!multiblock_write(fss->block_svr, 0, 0, fss->fcb_cache, end_block_no + 1)) {
        printf("flush_stat_cache_all: error\n");
    }
}

static void flush_fcb_cache(struct file_server_state *fss, unsigned int file_no) {
	char *blocks = (char *) fss->fcb_cache;
	unsigned int start_block_no =
		((char *) &fss->fcb_cache[file_no] - blocks) / BLOCK_SIZE;
    unsigned int end_block_no =
		((char *) &fss->fcb_cache[file_no + 1] - 1 - blocks) / BLOCK_SIZE;

    if (!multiblock_write(fss->block_svr, 0, start_block_no, blocks + (start_block_no * BLOCK_SIZE), end_block_no + 1)) {
        printf("flush_fcb_cache: error\n");
    }
}

static bool load_fcb_cache(struct file_server_state *fss) {
    unsigned int size = fss->num_fcbs * sizeof(*fss->fcb_cache);
    unsigned int psize_nblock = 0;

    bool r = block_getsize(fss->block_svr, 0, &psize_nblock);
	assert(r);
    if (psize_nblock == 0) {
        printf("BFS: Init new disk: installing FCB array at inode 0\n\r");
        return false;
    }
	else if (psize_nblock != (size - 1) / BLOCK_SIZE + 1) {
        printf("BFS: load_fcb_cache: error, size of inode 0 was %d but expected %d. Reinstalling FCB array in inode 0\n", psize_nblock, (size - 1) / BLOCK_SIZE + 1);
        return false;
    }
	else {
        printf("BFS: existing file system: %u FCBs\n", fss->num_fcbs);
	}

    // psize_nblock = (size - 1) / BLOCK_SIZE + 1;
    multiblock_read(fss->block_svr, 0, 0, fss->fcb_cache, &psize_nblock);
    return true;
}

static bool blkfile_read_allowed(struct file_control_block *stat, unsigned int uid) {
    //printf("%u is trying to read file owned by %u, other: %u\n", uid, stat->st_uid, stat->st_mode & P_FILE_OTHER_READ);

    if (uid == 0) {
        // read from root user
        return true;
    } else if (uid == stat->st_uid) {
        // read from owner
        return stat->st_mode & P_FILE_OWNER_READ;
    } else {
        // read from other
        return stat->st_mode & P_FILE_OTHER_READ;
    }
}

static bool blkfile_write_allowed(struct file_control_block *stat, unsigned int uid) {
    if (uid == 0) {
        // write from root user
        return true;
    } else if (uid == stat->st_uid) {
        // write from owner
        return stat->st_mode & P_FILE_OWNER_WRITE;
    } else {
        // write from other
        return stat->st_mode & P_FILE_OTHER_WRITE;
    }
}

#define BUF_SIZE    1024

/* Load a "grass" file with contents of a local file.
 */
void blkfile_load(fid_t fid, char *file){
    FILE *fp = fopen(file, "r");
	if (fp == 0) {
		fprintf(stderr, "blkfile_load: can't open '%s'\n", file);
		assert(0);
	}

    char buf[BUF_SIZE];

    unsigned long offset = 0;
    while (!feof(fp)) {
        assert(!ferror(fp));
        size_t n = fread(buf, 1, BUF_SIZE, fp);
        bool status = file_write(fid.server, fid.file_no, offset, buf, n);
        assert(status);
        offset += n;
    }
}
