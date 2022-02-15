/* Author: Robbert van Renesse, August 2015
 *
 * This virtual block store module turns a partition of the underlying block
 * store into a block store of its own.
 *
 *		block_if partdisk_init(block_if below, block_no partsizes[])
 *			Sum of all entries in partsizes should equate to the number of
 *			blocks of 'below'. Each read and write method on the new block
 *			store returned is a call to 'below' with ino and offset adjusted.
 *			For example, if 'below' has 24 blocks total, and partsizes is
 *			[10, 4, 10], then writing to partdisk inode 0 offset 5 results in
 *			writing to 'below' inode 0 offset 5, (1, 1) to (0, 10+1=11), and
 *			(2, 7) to (0, 10+4+7=21).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct partdisk_state {
	block_if below;			// block store below
	unsigned int ninodes;	// number of partitions
	block_no *partsizes;	// sizes of partitions
};

static int partdisk_getninodes(block_if bi){
	struct partdisk_state *ps = bi->state;
	return ps->ninodes;
}

static int partdisk_getsize(block_if bi, unsigned int ino){
	struct partdisk_state *ps = bi->state;
	return ps->partsizes[ino];
}

// TODO currently does not support setting sizes after initialization
static int partdisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	fprintf(stderr, "partdisk_setsize: not supported\n");
	return -1;
}

static int partdisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct partdisk_state *ps = bi->state;

	unsigned int ninodes = (unsigned int) partdisk_getninodes(bi);
	if (ino >= ninodes) {
		fprintf(stderr, "partdisk_read: ino too large\n");
		return -1;
	}
	if (offset >= ps->partsizes[ino]) {
		fprintf(stderr, "partdisk_read: offset too large\n");
		return -1;
	}
	block_no noffset = offset;
	unsigned int i = 0;
	for (i = 0; i < ino; i++) {
		noffset += ps->partsizes[i];
	}
	return (*ps->below->read)(ps->below, 0, noffset, block);
}

static int partdisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct partdisk_state *ps = bi->state;

	unsigned int ninodes = (unsigned int) partdisk_getninodes(bi);
	if (ino >= ninodes) {
		fprintf(stderr, "partdisk_write: ino too large\n");
		return -1;
	}
	if (offset >= ps->partsizes[ino]) {
		fprintf(stderr, "partdisk_write: offset too large\n");
		return -1;
	}
	block_no noffset = offset;
	unsigned int i = 0;
	for (i = 0; i < ino; i++) {
		noffset += ps->partsizes[i];
	}
	return (*ps->below->write)(ps->below, 0, noffset, block);
}

static void partdisk_release(block_if bi){
	free(bi->state);
	free(bi);
}

static int partdisk_sync(block_if bi, unsigned int ino){
	struct partdisk_state *ps = bi->state;
	// TODO for now sync all data below
	// even though only a subset of the blocks need to be synched
	return (*ps->below->sync)(ps->below, 0);
}

block_if partdisk_init(block_if below, unsigned int ninodes, block_no partsizes[]){
	/* Create the block store state structure.
	 */
	struct partdisk_state *ps = new_alloc(struct partdisk_state);
	ps->below = below;
	ps->ninodes = ninodes;
	ps->partsizes = malloc(ninodes * sizeof(block_no));
	memcpy(ps->partsizes, partsizes, ninodes * sizeof(block_no));

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ps;
	bi->getninodes = partdisk_getninodes;
	bi->getsize = partdisk_getsize;
	bi->setsize = partdisk_setsize;
	bi->read = partdisk_read;
	bi->write = partdisk_write;
	bi->release = partdisk_release;
	bi->sync = partdisk_sync;
	return bi;
}
