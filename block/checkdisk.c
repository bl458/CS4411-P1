/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse, August 2015
 *
 * This block store module simply forwards its method calls to an
 * underlying block store, but checks if reads correspond to prior
 * writes.
 *
 *		block_store_t *checkdisk_init(block_store_t *below, const char *descr);
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct block_list {
	struct block_list *next;
	unsigned int ino;
	block_no offset;
	block_t block;
};

struct checkdisk_state {
	block_store_t *below;			// block store below
	const char *descr;			// to disambiguate multiple instances
	struct block_list *bl;	// info about data written
};

static int checkdisk_getninodes(block_store_t *this_bs){
	struct checkdisk_state *cs = this_bs->state;

	return (*cs->below->getninodes)(cs->below);
}

static int checkdisk_getsize(block_store_t *this_bs, unsigned int ino){
	struct checkdisk_state *cs = this_bs->state;

	return (*cs->below->getsize)(cs->below, ino);
}

static int checkdisk_setsize(block_store_t *this_bs, unsigned int ino, block_no nblocks){
	struct checkdisk_state *cs = this_bs->state;

	/* See if I read or wrote any blocks beyond this boundary.  Remove.
	 */
	struct block_list **pbl, *bl;
	for (pbl = &cs->bl; (bl = *pbl) != 0;) {
		if (bl->ino == ino && bl->offset >= nblocks) {
			*pbl = bl->next;
			free(bl);
		}
		else {
			pbl = &bl->next;
		}
	}

	return (*cs->below->setsize)(cs->below, ino, nblocks);
}

static int checkdisk_read(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct checkdisk_state *cs = this_bs->state;

	if ((*cs->below->read)(cs->below, ino, offset, block) < 0) {
		return -1;
	}

	/* See if I read or wrote the block before.
	 */
	struct block_list *bl;
	for (bl = cs->bl; bl != 0; bl = bl->next) {
		if (bl->ino == ino && bl->offset == offset) {
			/* See if it's the same.
			 */
			if (memcmp(&bl->block, block, BLOCK_SIZE) != 0) {
				fprintf(stderr, "!!CHKDISK %s: checkdisk_read: corrupted\n\r", cs->descr);
				exit(1);
			}
			return 0;
		}
	}

	/* Add to the block list.
	 */
	bl = new_alloc(struct block_list);
	bl->ino = ino;
	bl->offset = offset;
	bl->block = *block;
	bl->next = cs->bl;
	cs->bl = bl;
	return 0;
}

static int checkdisk_write(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct checkdisk_state *cs = this_bs->state;

	int result = (*cs->below->write)(cs->below, ino, offset, block);
	if (result < 0) {
		return -1;
	}

	/* See if I read or wrote the block before.
	 */
	struct block_list *bl;
	for (bl = cs->bl; bl != 0; bl = bl->next) {
		if (bl->ino == ino && bl->offset == offset) {
			break;
		}
	}
	if (bl == 0) {
		/* Add to the block list.
		 */
		bl = new_alloc(struct block_list);
		bl->ino = ino;
		bl->offset = offset;
		bl->next = cs->bl;
		cs->bl = bl;
	}
	/* Update the block list entry.
	 */
	bl->block = *block;
	return result;
}

static void checkdisk_release(block_store_t *this_bs){
	struct checkdisk_state *cs = this_bs->state;
	struct block_list *bl;

	while ((bl = cs->bl) != 0) {
		cs->bl = bl->next;
		free(bl);
	}
	free(cs);
	free(this_bs);
}

static int checkdisk_sync(block_store_t *this_bs, unsigned int ino){
	struct checkdisk_state *cs = this_bs->state;
	return (*cs->below->sync)(cs->below, ino);
}

block_store_t *checkdisk_init(block_store_t *below, const char *descr){
	/* Create the block store state structure.
	 */
	struct checkdisk_state *cs = new_alloc(struct checkdisk_state);
	cs->below = below;
	cs->descr = descr;

	/* Return a block interface to this inode.
	 */
	block_store_t *this_bs = new_alloc(block_store_t);
	this_bs->state = cs;
	this_bs->getninodes = checkdisk_getninodes;
	this_bs->getsize = checkdisk_getsize;
	this_bs->setsize = checkdisk_setsize;
	this_bs->read = checkdisk_read;
	this_bs->write = checkdisk_write;
	this_bs->release = checkdisk_release;
	this_bs->sync = checkdisk_sync;
	return this_bs;
}
