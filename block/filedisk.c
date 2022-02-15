/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse, August 2015
 *
 * This code implements a block store stored in a file.
 *
 *		block_store_t *filedisk_init(block_t *blocks, block_no nblocks)
 *			Create a new block store, stored in the array of blocks
 *			pointed to by 'blocks', which has nblocks blocks in it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <egos/block_store.h>

struct filedisk_state {
	FILE *fp;
	block_no nblocks;		// desired #blocks
	block_no current;		// current #blocks
};

static int filedisk_getninodes(block_if bi){
	return 1;
}

static int filedisk_getsize(block_store_t *this_bs, unsigned int ino){
	struct filedisk_state *rs = this_bs->state;

	if (ino != 0) {
		fprintf(stderr, "!!filedisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	return rs->nblocks;
}

static int filedisk_setsize(block_store_t *this_bs, unsigned int ino, block_no nblocks){
	struct filedisk_state *rs = this_bs->state;

	int before = rs->nblocks;
	rs->nblocks = nblocks;
	return before;
}

static int filedisk_read(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct filedisk_state *rs = this_bs->state;

	if (ino != 0) {
		fprintf(stderr, "!!filedisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	if (offset >= rs->nblocks) {
		fprintf(stderr, "filedisk_read: bad offset %u\n", offset);
		return -1;
	}

	if (offset >= rs->current) {
		memset(block, 0, BLOCK_SIZE);
	}
	else {
		fseek(rs->fp, offset * BLOCK_SIZE, SEEK_SET);
		int n = fread(block, BLOCK_SIZE, 1, rs->fp);
		assert(n == 1);
	}
	return 0;
}

static int filedisk_write(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
	struct filedisk_state *rs = this_bs->state;

	if (ino != 0) {
		fprintf(stderr, "!!filedisk_getsize: ino != 0 not supported\n");
		return -1;
	}

	if (offset >= rs->nblocks) {
		fprintf(stderr, "filedisk_write: bad offset\n");
		return -1;
	}
	if (offset >= rs->current) {
		rs->current = offset + 1;
	}
	fseek(rs->fp, offset * BLOCK_SIZE, SEEK_SET);
	int n = fwrite(block, BLOCK_SIZE, 1, rs->fp);
	assert(n == 1);
	return 0;
}

static void filedisk_release(block_store_t *this_bs){
	struct filedisk_state *rs = this_bs->state;

	fclose(rs->fp);
	free(rs);
	free(this_bs);
}

static int filedisk_sync(block_store_t *this_bs, unsigned int ino){
	struct filedisk_state *rs = this_bs->state;

	fflush(rs->fp);
	return 0;
}

block_store_t *filedisk_init(const char *file, block_no nblocks){
	struct filedisk_state *rs = new_alloc(struct filedisk_state);

	FILE *fp;

	/* Create the file and make it the right size.
	 */
	if ((fp = fopen(file, "w+")) == 0) {
		perror(file);
		return 0;
	}
	fseek(fp, (off_t) nblocks * BLOCK_SIZE - 1, SEEK_SET);
	fwrite("", 1, 1, fp);
	fflush(fp);

	rs->fp = fp;
	rs->nblocks = nblocks;		// desired #blocks
	rs->current = 0;			// current #blocks

	block_store_t *this_bs = new_alloc(block_store_t);
	this_bs->state = rs;
	this_bs->getninodes = filedisk_getninodes;
	this_bs->getsize = filedisk_getsize;
	this_bs->setsize = filedisk_setsize;
	this_bs->read = filedisk_read;
	this_bs->write = filedisk_write;
	this_bs->release = filedisk_release;
	this_bs->sync = filedisk_sync;
	return this_bs;
}
