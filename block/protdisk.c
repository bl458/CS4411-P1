/* Author: Robbert van Renesse, July 2018
 *
 * This block store module forwards API calls to a remote block server.
 * ("prot" is short for "protocol")
 *
 *		block_if protdisk_init(gpid_t below, unsigned int ino);
 *			'below' is the process identifier of the remote block store.
 *			'ino' is the inode number of the remote disk
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <egos/block.h>
#include <egos/block_store.h>

struct protdisk_state {
	gpid_t below;			// process id of remote block store
	unsigned int ino;		// inode number of remote disk
};

static int protdisk_getninodes(block_if bi){
	struct protdisk_state *ps = bi->state;

	unsigned int ninodes;
	bool r = block_getninodes(ps->below, &ninodes);
	assert(r);
	return ninodes;
}

static int protdisk_getsize(block_if bi, unsigned int ino){
	struct protdisk_state *ps = bi->state;

	if (ino != 0) {
		fprintf(stderr, "!!PROTDISK: ino != 0 not supported\n");
		return -1;
	}

	unsigned int nblocks;
	bool r = block_getsize(ps->below, ps->ino, &nblocks);
	assert(r);
	return nblocks;
}

static int protdisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	// struct protdisk_state *ps = bi->state;
	// TODO

	assert(0);
	return -1;
}

static int protdisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct protdisk_state *ps = bi->state;

	if (ino != 0) {
		fprintf(stderr, "!!PROTDISK: ino != 0 not supported\n");
		return -1;
	}

	bool r = block_read(ps->below, ps->ino, offset, block);
	return r ? 0 : -1;
}

static int protdisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct protdisk_state *ps = bi->state;

	if (ino != 0) {
		fprintf(stderr, "!!PROTDISK: ino != 0 not supported\n");
		return -1;
	}

	bool r = block_write(ps->below, ps->ino, offset, block);
	return r ? 0 : -1;
}

static void protdisk_release(block_if bi){
	free(bi->state);
	free(bi);
}

static int protdisk_sync(block_if bi, unsigned int ino){
	struct protdisk_state *ps = bi->state;

	if (ino != 0) {
		fprintf(stderr, "!!PROTDISK: ino != 0 not supported\n");
		return -1;
	}

	bool r = block_sync(ps->below, ps->ino);
	return r ? 0 : -1;
}

// TODO.  Maybe get rid of ino?
block_if protdisk_init(gpid_t below, unsigned int ino){
	/* Create the block store state structure.
	 */
	struct protdisk_state *ps = new_alloc(struct protdisk_state);
	ps->below = below;
	ps->ino = ino;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ps;
	bi->getninodes = protdisk_getninodes;
	bi->getsize = protdisk_getsize;
	bi->setsize = protdisk_setsize;
	bi->read = protdisk_read;
	bi->write = protdisk_write;
	bi->release = protdisk_release;
	bi->sync = protdisk_sync;
	return bi;
}
