/* Author: Alice Chen, May 2019
 *
 * This block store module forwards calls to a block store below.
 *
 *		block_if mapdisk_init(block_if *below, unsigned int ino);
 *			'below' is the block store below.
 *			'ino' is the inode number of the part of the block store below.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <egos/block_store.h>

struct mapdisk_state {
	block_if below;
	unsigned int ino;
};

static int mapdisk_getninodes(block_if bi){
	return 1;
}

static int mapdisk_getsize(block_if bi, unsigned int ino){
	if (ino != 0) {
		fprintf(stderr, "!!mapdisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	struct mapdisk_state *ms = bi->state;
	return (*ms->below->getsize)(ms->below, ms->ino);
}

static int mapdisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	if (ino != 0) {
		fprintf(stderr, "!!mapdisk_setsize: ino != 0 not supported\n");
		return -1;
	}

	struct mapdisk_state *ms = bi->state;
	return (*ms->below->setsize)(ms->below, ms->ino, nblocks);
}

static int mapdisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	if (ino != 0) {
		fprintf(stderr, "!!mapdisk_read: ino != 0 not supported\n");
		return -1;
	}

	struct mapdisk_state *ms = bi->state;
	return (*ms->below->read)(ms->below, ms->ino, offset, block);
}

static int mapdisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	if (ino != 0) {
		fprintf(stderr, "!!mapdisk_write: ino != 0 not supported\n");
		return -1;
	}

	struct mapdisk_state *ms = bi->state;
	return (*ms->below->write)(ms->below, ms->ino, offset, block);
}

static void mapdisk_release(block_if bi){
	free(bi->state);
	free(bi);
}

static int mapdisk_sync(block_if bi, unsigned int ino){
	if (ino != 0) {
		fprintf(stderr, "!!mapdisk_sync: ino != 0 not supported\n");
		return -1;
	}

	struct mapdisk_state *ms = bi->state;
	return (*ms->below->sync)(ms->below, ms->ino);
}

block_if mapdisk_init(block_if below, unsigned int ino){
	/* Create the block store state structure.
	 */
	struct mapdisk_state *ms = new_alloc(struct mapdisk_state);
	ms->below = below;
	ms->ino = ino;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ms;
	bi->getninodes = mapdisk_getninodes;
	bi->getsize = mapdisk_getsize;
	bi->setsize = mapdisk_setsize;
	bi->read = mapdisk_read;
	bi->write = mapdisk_write;
	bi->release = mapdisk_release;
	bi->sync = mapdisk_sync;
	return bi;
}
