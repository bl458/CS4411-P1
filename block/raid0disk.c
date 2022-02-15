/* Author: Robbert van Renesse, November 2015
 *
 * This block store module implements RAID0.
 *
 *		block_if raid0disk_init(block_if *below, unsigned int nbelow){
 *			'below' is an array of underlying block stores, all of which
 *			are assumed to be of the same size.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct raid0disk_state {
	block_if *below;		// block stores below
	unsigned int nbelow;	// #block stores
};

static int raid0disk_getninodes(block_if bi){
	return 1;
}

static int raid0disk_getsize(block_if bi, unsigned int ino){
	if (ino != 0) {
		fprintf(stderr, "!!raid0disk_getsize: ino != 0 not supported\n");
		return -1;
	}

	struct raid0disk_state *rds = bi->state;
	int total = 0;
	unsigned int i;

	for (i = 0; i < rds->nbelow; i++) {
		int r = (*rds->below[0]->getsize)(rds->below[0], ino);
		if (r < 0) {
			return r;
		}
		total += r;
	}
	return total;
}

static int raid0disk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	fprintf(stderr, "raid0disk_setsize: not yet implemented\n");
	return -1;
}

static int raid0disk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	if (ino != 0) {
		fprintf(stderr, "!!raid0disk_read: ino != 0 not supported\n");
		return -1;
	}

	struct raid0disk_state *rds = bi->state;
	int i = offset % rds->nbelow;
	offset /= rds->nbelow;
	return (*rds->below[i]->read)(rds->below[i], ino, offset, block);
}

static int raid0disk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct raid0disk_state *rds = bi->state;

	if (ino != 0) {
		fprintf(stderr, "!!raid0disk_write: ino != 0 not supported\n");
		return -1;
	}

	int i = offset % rds->nbelow;
	offset /= rds->nbelow;
	return (*rds->below[i]->write)(rds->below[i], ino, offset, block);
}

static void raid0disk_release(block_if bi){
	free(bi->state);
	free(bi);
}

static int raid0disk_sync(block_if bi, unsigned int ino){
	if (ino != 0) {
		fprintf(stderr, "!!raid0disk_sync: ino != 0 not supported\n");
		return -1;
	}

	/* Sync all block stores below.
	 */
	struct raid0disk_state *rds = bi->state;
	for (unsigned int i = 0; i < rds->nbelow; i++) {
		if ((*rds->below[i]->sync)(rds->below[i], -1) < 0) {
			fprintf(stderr, "!!raid0disk_sync: sync error for %dth block store below\n", i);
			return -1;
		}
	}
	return 0;
}

block_if raid0disk_init(block_if *below, unsigned int nbelow){
	/* Create the block store state structure.
	 */
	struct raid0disk_state *rds = new_alloc(struct raid0disk_state);
	rds->below = below;
	rds->nbelow = nbelow;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = rds;
	bi->getninodes = raid0disk_getninodes;
	bi->getsize = raid0disk_getsize;
	bi->setsize = raid0disk_setsize;
	bi->read = raid0disk_read;
	bi->write = raid0disk_write;
	bi->release = raid0disk_release;
	bi->sync = raid0disk_sync;
	return bi;
}
