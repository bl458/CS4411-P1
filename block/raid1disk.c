/* Author: Robbert van Renesse, November 2015
 *
 * This block store module implements RAID1 (basic mirroring)
 *
 *		block_if raid1disk_init(block_if *below, unsigned int nbelow){
 *			'below' is an array of underlying block stores, all of which
 *			are assumed to be of the same size.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct raid1disk_state {
	block_if *below;		// block stores below
	unsigned int nbelow;	// #block stores
	char *broken;			// keeps track of which stores are broken
};

static int raid1disk_getninodes(block_if bi){
	struct raid1disk_state *rds = bi->state;
	unsigned int i;

	/* Try all underlying disks until one works.
	 */
	for (i = 0; i < rds->nbelow; i++) {
		if (!rds->broken[i]) {
			int ninodes = (*rds->below[i]->getninodes)(rds->below[i]);
			if (ninodes < 0) {
				rds->broken[i] = 1;
			}
			else {
				return ninodes;
			}
		}
	}
	return -1;
}

static int raid1disk_getsize(block_if bi, unsigned int ino){
	struct raid1disk_state *rds = bi->state;
	unsigned int i;

	/* Try all underlying disks until one works.
	 */
	for (i = 0; i < rds->nbelow; i++) {
		if (!rds->broken[i]) {
			int nblocks = (*rds->below[i]->getsize)(rds->below[i], ino);
			if (nblocks < 0) {
				rds->broken[i] = 1;
			}
			else {
				return nblocks;
			}
		}
	}
	return -1;
}

static int raid1disk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	struct raid1disk_state *rds = bi->state;
	int oldsize = -1;
	unsigned int i;

	/* Try to set the size for all underlying disks.  Keep track
	 * of failures.
	  */
	for (i = 0; i < rds->nbelow; i++) {
		int r = (*rds->below[i]->setsize)(rds->below[i], ino, nblocks);
		if (r < 0) {
			rds->broken[i] = 1;
		}
		else {
			oldsize = r;
		}
	}

	return oldsize;
}

static int raid1disk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct raid1disk_state *rds = bi->state;
	unsigned int i;

	/* Simply try all of them.  If reading fails it is not necessary
	 * to mark them as broken.
	 */
	for (i = 0; i < rds->nbelow; i++) {
		if (rds->broken[i]) {
			continue;
		}
		if ((*rds->below[i]->read)(rds->below[i], ino, offset, block) >= 0) {
			return 0;
		}
	}
	return -1;
}

static int raid1disk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct raid1disk_state *rds = bi->state;
	unsigned int i;
	int result = -1;

	/* Try to write all of the underlying stores.
	 */
	for (i = 0; i < rds->nbelow; i++) {
		if (rds->broken[i]) {
			continue;
		}
		if ((*rds->below[i]->write)(rds->below[i], ino, offset, block) < 0) {
			rds->broken[i] = 1;
		}
		else {
			result = 0;
		}
	}
	return result;
}

static void raid1disk_release(block_if bi){
	struct raid1disk_state *rds = bi->state;

	free(rds->broken);
	free(rds);
	free(bi);
}

static int raid1disk_sync(block_if bi, unsigned int ino){
	struct raid1disk_state *rds = bi->state;
	unsigned int i;
	int result = -1;

	for (i = 0; i < rds->nbelow; i++) {
		if (rds->broken[i]) {
			continue;
		}
		if ((*rds->below[i]->sync)(rds->below[i], ino) < 0) {
			rds->broken[i] = 1;
		}
		else {
			result = 0;
		}
	}
	return result;
}

block_if raid1disk_init(block_if *below, unsigned int nbelow){
	/* Create the block store state structure.
	 */
	struct raid1disk_state *rds = new_alloc(struct raid1disk_state);
	rds->below = below;
	rds->nbelow = nbelow;
	rds->broken = calloc(1, nbelow);

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = rds;
	bi->getninodes = raid1disk_getninodes;
	bi->getsize = raid1disk_getsize;
	bi->setsize = raid1disk_setsize;
	bi->read = raid1disk_read;
	bi->write = raid1disk_write;
	bi->release = raid1disk_release;
	bi->sync = raid1disk_sync;
	return bi;
}
