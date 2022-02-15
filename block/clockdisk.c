/* Author: Robbert van Renesse, August 2015
 *
 * This block store module mirrors the underlying block store but contains
 * a write-through cache.  The caching strategy is CLOCK, approximating LRU.
 * The interface is as follows:
 *
 *		block_if clockdisk_init(block_if below,
 *									block_t *blocks, block_no nblocks)
 *			'below' is the underlying block store.  'blocks' points to
 *			a chunk of memory wth 'nblocks' blocks for caching.
 *
 *		void clockdisk_dump_stats(block_if bi)
 *			Prints the cache statistics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>


/* State contains the pointer to the block module below as well as caching
 * information and caching statistics.
 */
struct clockdisk_state {
	block_if below;				// block store below
	block_t *blocks;			// memory for caching blocks
	block_no nblocks;			// size of cache (not size of block store!)


	/* Stats.
	 */
	unsigned int read_hit, read_miss, write_hit, write_miss;
};


static int clockdisk_getninodes(block_store_t *this_bs){
	struct clockdisk_state *cs = this_bs->state;
	return (*cs->below->getninodes)(cs->below);
}

static int clockdisk_getsize(block_if bi, unsigned int ino){
	struct clockdisk_state *cs = bi->state;
	return (*cs->below->getsize)(cs->below, ino);
}

static int clockdisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	struct clockdisk_state *cs = bi->state;

	return (*cs->below->setsize)(cs->below, ino, nblocks);
}

static int clockdisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	/* Your code should replace this naive implementation
	 */
	struct clockdisk_state *cs = bi->state;

	int r = (*cs->below->read)(cs->below, ino, offset, block);
	return r;
}

static int clockdisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	/* Your code should replace this naive implementation
	 */
	
	struct clockdisk_state *cs = bi->state;

	return (*cs->below->write)(cs->below, ino, offset, block);
}

static int clockdisk_sync(block_if bi, unsigned int ino){
	/* Your code goes here:
	 */
	return 0;
}

static void clockdisk_release(block_if bi){
	struct clockdisk_state *cs = bi->state;
	free(cs);
	free(bi);
}

void clockdisk_dump_stats(block_if bi){
	struct clockdisk_state *cs = bi->state;

	printf("!$CLOCK: #read hits:    %u\n", cs->read_hit);
	printf("!$CLOCK: #read misses:  %u\n", cs->read_miss);
	printf("!$CLOCK: #write hits:   %u\n", cs->write_hit);
	printf("!$CLOCK: #write misses: %u\n", cs->write_miss);
}

/* Create a new block store module on top of the specified module below.
 * blocks points to a chunk of memory of nblocks blocks that can be used
 * for caching.
 */
block_if clockdisk_init(block_if below, block_t *blocks, block_no nblocks){
	/* Create the block store state structure.
	 */
	struct clockdisk_state *cs = new_alloc(struct clockdisk_state);
	cs->below = below;
	cs->blocks = blocks;
	cs->nblocks = nblocks;
	cs->read_hit = 0;
	cs->read_miss = 0;
	cs->write_hit = 0;
	cs->write_miss = 0;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = cs;
	bi->getninodes = clockdisk_getninodes;
	bi->getsize = clockdisk_getsize;
	bi->setsize = clockdisk_setsize;
	bi->read = clockdisk_read;
	bi->write = clockdisk_write;
	bi->release = clockdisk_release;
	bi->sync = clockdisk_sync;
	return bi;
}
