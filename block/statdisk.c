/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse, August 2015
 *
 * This block store module simply forwards its method calls to an
 * underlying block store, but keeps track of statistics.
 *
 *		block_store_t *statdisk_init(block_store_t *below){
 *			'below' is the underlying block store.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct statdisk_state {
	block_store_t *below;	// block store below
	unsigned int ngetsize;	// #getsize operations
	unsigned int nsetsize;	// #setsize operations
	unsigned int nread;		// #read operations
	unsigned int nwrite;	// #write operations
	unsigned int nsync;		// #sync operations
};

static int statdisk_getninodes(block_store_t *this_bs){
	struct statdisk_state *sds = this_bs->state;

	return (*sds->below->getninodes)(sds->below);
}

static int statdisk_getsize(block_store_t *this_bs, unsigned int ino){
	struct statdisk_state *sds = this_bs->state;

	sds->ngetsize++;
	return (*sds->below->getsize)(sds->below, ino);
}

static int statdisk_setsize(block_store_t *this_bs, unsigned int ino, block_no nblocks){
	struct statdisk_state *sds = this_bs->state;

	sds->nsetsize++;
	return (*sds->below->setsize)(sds->below, ino, nblocks);
}

static int statdisk_read(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct statdisk_state *sds = this_bs->state;
	sds->nread++;
	return (*sds->below->read)(sds->below, ino, offset, block);
}

static int statdisk_write(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct statdisk_state *sds = this_bs->state;
	sds->nwrite++;
	return (*sds->below->write)(sds->below, ino, offset, block);
}

static void statdisk_release(block_store_t *this_bs){
	free(this_bs->state);
	free(this_bs);
}

static int statdisk_sync(block_store_t *this_bs, unsigned int ino){
	struct statdisk_state *sds = this_bs->state;
	sds->nsync++;
	return (*sds->below->sync)(sds->below, ino);
}

void statdisk_dump_stats(block_store_t *this_bs){
	struct statdisk_state *sds = this_bs->state;

	printf("!$STAT: #getsize:  %u\n", sds->ngetsize);
	printf("!$STAT: #setsize:  %u\n", sds->nsetsize);
	printf("!$STAT: #read:     %u\n", sds->nread);
	printf("!$STAT: #write:    %u\n", sds->nwrite);
	printf("!$STAT: #sync:     %u\n", sds->nsync);
}

block_store_t *statdisk_init(block_store_t *below){
	/* Create the block store state structure.
	 */
	struct statdisk_state *sds = new_alloc(struct statdisk_state);
	sds->below = below;

	/* Return a block interface to this inode.
	 */
	block_store_t *this_bs = new_alloc(block_store_t);
	this_bs->state = sds;
	this_bs->getninodes = statdisk_getninodes;
	this_bs->getsize = statdisk_getsize;
	this_bs->setsize = statdisk_setsize;
	this_bs->read = statdisk_read;
	this_bs->write = statdisk_write;
	this_bs->release = statdisk_release;
	this_bs->sync = statdisk_sync;
	return this_bs;
}
