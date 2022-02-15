#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <assert.h>
#include <earth/earth.h>
#include <egos/syscall.h>
#include <egos/block.h>
#include <egos/block_store.h>

#define DISK_SIZE		(16 * 1024)     // size of "physical" disk in blocks
#define NCACHE_BLOCKS	20				// size of cache
#define MAX_STACK_SIZE	100				// probably enough...
#define NINODES			256

/* State of the block server.
 */
struct block_server_state {
	block_store_t *stack[MAX_STACK_SIZE];
	block_store_t **sp;
};

// these helper functions are declared here and defined later
static void block_do_read(struct block_server_state *bss, struct block_request *req, gpid_t src);
static void block_do_write(struct block_server_state *bss, struct block_request *req, void *data, unsigned int nblock, gpid_t src);
static void block_do_sync(struct block_server_state *bss, struct block_request *req, gpid_t src);
static void block_do_getsize(struct block_server_state *bss, struct block_request *req, gpid_t src);
static void block_do_setsize(struct block_server_state *bss, struct block_request *req, gpid_t src);
static void block_do_getninodes(struct block_server_state *bss, struct block_request *req, gpid_t src);

#ifdef notdef
static void block_cleanup(void *arg){
	struct block_server_state *bss = arg;

	printf("block server: cleaning up\n\r");

	while (bss->sp > bss->stack) {
		block_store_t *bs = *bss->sp;
		(*bs->release)(bs);
		bss->sp--;
	}

	free(bss);
}
#endif

static void block_proc(void *arg){
	struct block_server_state *bss = arg;

	printf("BLOCK SERVER (layered block storage): pid=%u\n\r", sys_getpid());

	struct block_request *req = new_alloc_ext(struct block_request, PAGESIZE);
	for (;;) {
		gpid_t src;
		int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(req) + PAGESIZE, &src, 0);
		if (req_size < 0) {
			printf("block server shutting down\n\r");
			free(bss);
			free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));

		switch (req->type) {
			case BLOCK_READ:
				//fprintf(stderr, "!!DEBUG: calling block read\n");
				block_do_read(bss, req, src);
				break;
			case BLOCK_WRITE:
				//fprintf(stderr, "!!DEBUG: calling block write: %u %u %u\n", req_size, sizeof(*req), BLOCK_SIZE);
				block_do_write(bss, req, &req[1], (req_size - sizeof(*req)) / BLOCK_SIZE, src);
				break;
			case BLOCK_SYNC:
				//fprintf(stderr, "!!DEBUG: calling blkfile sync\n");
				block_do_sync(bss, req, src);
				break;
			case BLOCK_GETSIZE:
				//fprintf(stderr, "!!DEBUG: calling block getsize\n");
				block_do_getsize(bss, req, src);
				break;
			case BLOCK_SETSIZE:
				//fprintf(stderr, "!!DEBUG: calling block setsize\n");
				block_do_setsize(bss, req, src);
				break;
			case BLOCK_GETNINODES:
				//fprintf(stderr, "!!DEBUG: calling block getninodes\n");
				block_do_getninodes(bss, req, src);
				break;
			default:
				assert(0);
		}
	}
}

// Size of paging partition on disk
#define PG_DEV_BLOCKS		((PG_DEV_SIZE * PAGESIZE) / BLOCK_SIZE)

// The inode number to hard-code as the inode for all requests to the bottom 
// device in the block storage stack
#define BOTTOM_INODE 		0

/* Create a new block device.  fsconf is the file system configuration,
 * which is currently either "tree", "fat", or "unix".
 */
void block_init(block_store_t *bot, char *fsconf){
	struct block_server_state *bss = new_alloc(struct block_server_state);
	bss->sp = bss->stack;

	*bss->sp = bot;

	/* Create cache layer.
	 */
	block_t *cache = malloc(NCACHE_BLOCKS * BLOCK_SIZE);
	bss->sp++;
	*bss->sp = clockdisk_init(bss->sp[-1], cache, NCACHE_BLOCKS);
	// *bss->sp = wtclockdisk_init(bss->sp[-1], cache, NCACHE_BLOCKS);

	/* Check layer.
	 */
	// bss->sp++;
	// *bss->sp = checkdisk_init(bss->sp[-1], "above cache");

	/* Virtualize the store, creating a collection of NINODES virtual stores
	 * on top of a single inode of the bottom block store (which probably only
	 * has one inode, inode 0).
	 */
	if (strcmp(fsconf, "tree") == 0) {
		if (treedisk_create(*bss->sp, BOTTOM_INODE, NINODES) < 0) {
			fprintf(stderr, "block_init: can't create treedisk file system\n");
			exit(1);
		}
		bss->sp++;
		*bss->sp = treedisk_init(bss->sp[-1], BOTTOM_INODE);
	}
#ifdef HW_FS
	else if (strcmp(fsconf, "unix") == 0) {
		if (unixdisk_create(*bss->sp, BOTTOM_INODE, NINODES) < 0) {
			fprintf(stderr, "block_init: can't create unixdisk file system\n");
			exit(1);
		}
		bss->sp++;
		*bss->sp = unixdisk_init(bss->sp[-1], BOTTOM_INODE);
	}
	else if (strcmp(fsconf, "fat") == 0) {
		if (fatdisk_create(*bss->sp, BOTTOM_INODE, NINODES) < 0) {
			fprintf(stderr, "block_init: can't create fatdisk file system\n");
			exit(1);
		}
		bss->sp++;
		*bss->sp = fatdisk_init(bss->sp[-1], BOTTOM_INODE);
	}
#endif //HW_FS
	else {
		fprintf(stderr, "block_init: unknown fs configuration '%s'\n", fsconf);
		exit(1);
	}

	/* Check layer.
	 */
	// bss->sp++;
	// *bss->sp = checkdisk_init(bss->sp[-1], "above file system");

	// bss->sp++;
	// *bss->sp = debugdisk_init(bss->sp[-1], "above file system");

	block_proc(bss);
}

static void usage(char *name){
	fprintf(stderr, "Usage: %s [-r #blocks | -s server] [-c file-sys-conf]\n", name);
	exit(1);
}

int main(int argc, char **argv){
	block_store_t *bottom = 0;
	char *fsconf = "tree", c;

    while ((c = getopt(argc, argv, "c:r:s:")) != -1) {
		switch (c) {
		case 'c':
			fsconf = optarg;
			break;
		case 'r':
			if (bottom == 0) {
				int n = atoi(optarg);
				block_t *blocks = malloc(n * BLOCK_SIZE);
				bottom = ramdisk_init(blocks, n);
			}
			else {
				usage(argv[0]);
			}
			break;
		case 's':
			if (bottom == 0) {
				gpid_t store = atoi(optarg);
				bottom = protdisk_init(store, 0);
			}
			else {
				usage(argv[0]);
			}
			break;
		default:
			usage(argv[0]);
		}
	}
	if (argc != optind) {
		usage(argv[0]);
	}

	/* Default bottom layer is file system disk.
	 */
	if (bottom == 0) {
		bottom = protdisk_init(GRASS_ENV->servers[GPID_DISK_FS], 0);
	}

	block_init(bottom, fsconf);
	return 0;
}

static void block_respond(struct block_request *req, enum block_status status,
				void *data, unsigned int nblock, gpid_t src){
	struct block_reply *rep = new_alloc_ext(struct block_reply, nblock * BLOCK_SIZE);
	rep->status = status;
	memcpy(&rep[1], data, nblock * BLOCK_SIZE);
	sys_send(src, MSG_REPLY, rep, sizeof(*rep) + nblock * BLOCK_SIZE);
	free(rep);
}

/* Respond to a read block request.
 */
static void block_do_read(struct block_server_state *bss, struct block_request *req, gpid_t src){
	// req->file_no
	// req->offset_nblock

	/* Allocate room for the reply.
	 */
	struct block_reply *rep = new_alloc_ext(struct block_reply, BLOCK_SIZE);

	/* Read the block from block store
	 */
	int result;
	block_t *buffer = (block_t *) &rep[1];
	block_store_t *bs = *bss->sp;
	result = (*bs->read)(bs, req->ino, req->offset_nblock, buffer);
	if (result < 0) {
		printf("block_do_read: bad offset: %u in inode %u\n", req->offset_nblock, req->ino);
		block_respond(req, BLOCK_ERROR, 0, 0, src);
	}
	else {
		rep->status = BLOCK_OK;
		rep->size_nblock = 1;
		sys_send(src, MSG_REPLY, rep, sizeof(*rep) + BLOCK_SIZE);
	}
	free(rep);
}

/* Respond to a write block request.
 */
static void block_do_write(struct block_server_state *bss, struct block_request *req, void *data, unsigned int nblock, gpid_t src){
	// req->file_no
	// req->offset_nblock

	if (nblock != 1) {
		printf("block_do_write: size mismatch %u %u\n", 1, nblock);
		block_respond(req, BLOCK_ERROR, 0, 0, src);
		return;
	}

	int result;
	block_t *buffer = (block_t *) data;
	block_store_t *bs = *bss->sp;
	result = (*bs->write)(bs, req->ino, req->offset_nblock, buffer);
	if (result < 0) {
		printf("block_do_write: bad offset: %u in inode %u\n", req->offset_nblock, req->ino);
		block_respond(req, BLOCK_ERROR, 0, 0, src);
		return;
	}

	block_respond(req, BLOCK_OK, 0, 0, src);
}

/* Respond to a write block request.
 */
static void block_do_sync(struct block_server_state *bss, struct block_request *req, gpid_t src){
	if (req->ino != (unsigned int) -1) {
		printf("block_do_sync: bad inode %u\n", req->ino);
		block_respond(req, BLOCK_ERROR, 0, 0, src);
		return;
	}

	int result;
	block_store_t *bs = *bss->sp;
	result = (*bs->sync)(bs, req->ino);

	if (result < 0) {
		printf("block_do_sync: sync error\n");
		block_respond(req, BLOCK_ERROR, 0, 0, src);
		return;
	}

	block_respond(req, BLOCK_OK, 0, 0, src);
}

/* Respond to a getsize block request.
 */
static void block_do_getsize(struct block_server_state *bss, struct block_request *req, gpid_t src){
	// req->file_no
	// req->offset_nblock

	/* Get size of block store.
	 */
	block_store_t *bs = *bss->sp;
	unsigned int size = (*bs->getsize)(bs, req->ino);

	/* Send size of block store.
	 */
	struct block_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = BLOCK_OK;
	rep.size_nblock = size;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize block request.
 */
static void block_do_setsize(struct block_server_state *bss, struct block_request *req, gpid_t src){

	block_store_t *bs = *bss->sp;
	int result = (*bs->setsize)(bs, req->ino, req->offset_nblock);

	if (result < 0) {
		printf("block_do_setsize: bad size file_no: %u, size: %u\n", req->ino, req->offset_nblock);
		block_respond(req, BLOCK_ERROR, 0, 0, src);
		return;
	}

	block_respond(req, BLOCK_OK, 0, 0, src);
}

/* Respond to a getninodes block request.
 */
static void block_do_getninodes(struct block_server_state *bss, struct block_request *req, gpid_t src){
	block_store_t *bs = *bss->sp;
	unsigned int ninodes = (*bs->getninodes)(bs);

	/* Send size of block store.
	 */
	struct block_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = BLOCK_OK;
	rep.br_ninodes = ninodes;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}
