/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse, August 2015
 *
 * This code implements a block store stored in memory
 * file system:
 *
 *		block_store_t *ramdisk_init(block_t *blocks, block_no nblocks)
 *			Create a new block store, stored in the array of blocks
 *			pointed to by 'blocks', which has nblocks blocks in it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct ramdisk_state {
	block_t *blocks;
	block_no nblocks;
};

static int ramdisk_getninodes(block_if bi){
	return 1;
}

static int ramdisk_getsize(block_store_t *this_bs, unsigned int ino){
	struct ramdisk_state *rs = this_bs->state;

	if (ino != 0) {
		fprintf(stderr, "!!ramdisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	return rs->nblocks;
}

static int ramdisk_setsize(block_store_t *this_bs, unsigned int ino, block_no nblocks){
	struct ramdisk_state *rs = this_bs->state;

	int before = rs->nblocks;
	rs->nblocks = nblocks;
	return before;
}

static int ramdisk_read(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct ramdisk_state *rs = this_bs->state;

	if (ino != 0) {
		fprintf(stderr, "!!ramdisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	if (offset >= rs->nblocks) {
		fprintf(stderr, "ramdisk_read: bad offset %u\n", offset);
		return -1;
	}
	memcpy(block, &rs->blocks[offset], BLOCK_SIZE);
	return 0;
}

static int ramdisk_write(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct ramdisk_state *rs = this_bs->state;

	if (ino != 0) {
		fprintf(stderr, "!!ramdisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	if (offset >= rs->nblocks) {
		fprintf(stderr, "ramdisk_write: bad offset\n");
		return -1;
	}
	memcpy(&rs->blocks[offset], block, BLOCK_SIZE);
	return 0;
}

static void ramdisk_release(block_store_t *this_bs){
	free(this_bs->state);
	free(this_bs);
}

static int ramdisk_sync(block_store_t *this_bs, unsigned int ino){
	// do nothing as ram is not write-back and at bottom
	return 0;
}

block_store_t *ramdisk_init(block_t *blocks, block_no nblocks){
	struct ramdisk_state *rs = new_alloc(struct ramdisk_state);

	rs->blocks = blocks;
	rs->nblocks = nblocks;

	block_store_t *this_bs = new_alloc(block_store_t);
	this_bs->state = rs;
	this_bs->getninodes = ramdisk_getninodes;
	this_bs->getsize = ramdisk_getsize;
	this_bs->setsize = ramdisk_setsize;
	this_bs->read = ramdisk_read;
	this_bs->write = ramdisk_write;
	this_bs->release = ramdisk_release;
	this_bs->sync = ramdisk_sync;
	return this_bs;
}
