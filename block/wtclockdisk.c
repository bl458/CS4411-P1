/* Author: Robbert van Renesse, August 2015
 *
 * This is the write-through version of clockdisk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>


/* State contains the pointer to the block module below as well as caching
 * information and caching statistics.
 */

struct block_info {
	int use_bit;
	unsigned int ino;
	unsigned int offset; 
};


struct wtclockdisk_state {
	block_if below;				// block store below
	block_t *blocks;			// memory for caching blocks
	block_no nblocks;			// size of cache (not size of block store!)
	block_info metadatas[nblocks];
	int clock_hand;
	
	/* Stats.
	 */
	unsigned int read_hit, read_miss, write_hit, write_miss;
};


static int wtclockdisk_getninodes(block_store_t *this_bs){
	struct wtclockdisk_state *cs = this_bs->state;
	return (*cs->below->getninodes)(cs->below);
}

static int wtclockdisk_getsize(block_if bi, unsigned int ino){
	struct wtclockdisk_state *cs = bi->state;
	return (*cs->below->getsize)(cs->below, ino);
}

static int wtclockdisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	struct wtclockdisk_state *cs = bi->state;

	return (*cs->below->setsize)(cs->below, ino, nblocks);
}

static int wtclockdisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct wtclockdisk_state *cs = bi->state;
	int i = 0; 
	while (i < nblocks) {
		if (metadatas[i]->ino == ino && metadatas[i]->offset == offset){
			break;
		}

		i += 1; 
	}
	
	if (i == nblocks) {
		read_miss += 1;
		if ((*cs->below->read)(cs->below, ino, offset, block) == -1) {
			return -1; 
		}

		cache_update(cs, ino, offset, block); 
	}
	else {
		read_hit += 1; 
		memcpy(block, blocks->bytes[i], BLOCK_SIZE);
	}
	
	return 0;
}

static int wtclockdisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	/* Your code should replace this naive implementation
	 */
	
	struct wtclockdisk_state *cs = bi->state;

	return (*cs->below->write)(cs->below, ino, offset, block);
}

static void wtclockdisk_release(block_if bi){
	struct wtclockdisk_state *cs = bi->state;
	free(cs);
	free(bi);
}

static int wtclockdisk_sync(block_if bi, unsigned int ino){
	struct wtclockdisk_state *cs = bi->state;
	return (*cs->below->sync)(cs->below, ino);
}

void wtclockdisk_dump_stats(block_if bi){
	struct wtclockdisk_state *cs = bi->state;

	printf("!$WTCLOCK: #read hits:    %u\n", cs->read_hit);
	printf("!$WTCLOCK: #read misses:  %u\n", cs->read_miss);
	printf("!$WTCLOCK: #write hits:   %u\n", cs->write_hit);
	printf("!$WTCLOCK: #write misses: %u\n", cs->write_miss);
}

static void cache_update(wtclockdisk_state *cs, unsigned int ino, block_no offset, block_t *block) {	
	//Update the slot *cs->metadatas, clock_hand  
	while (true) {
		if (metadatas[clock_hand]->use_bit == 0) {
			break; 
		}

		metadatas[clock_hand]->use_bit = 0;
		if (clock_hand == nblocks - 1) {
			clock_hand = 0;
		}
		else {
			clock_hand += 1;
		}
	}

	// Edit acutal cache memory 
	// Todo ask if this is correct 
	memcpy(&cs->blocks->bytes[clock_hand], block, BLOCK_SIZE);

	// Edit slot metadatas[clock_hand]
	metadatas[clock_hand]->use_bit = 1; 
	metadatas[clock_hand]->ino = ino; 
	metadatas[clock_hand]->offset = offset; 
}

/* Create a new block store module on top of the specified module below.
 * blocks points to a chunk of memory of nblocks blocks that can be used
 * for caching.
 */
block_if wtclockdisk_init(block_if below, block_t *blocks, block_no nblocks){
	/* Create the block store state structure.
	 */
	struct wtclockdisk_state *cs = new_alloc(struct wtclockdisk_state);
	cs->below = below;
	cs->blocks = blocks;
	cs->nblocks = nblocks;
	cs->read_hit = 0;
	cs->read_miss = 0;
	cs->write_hit = 0;
	cs->write_miss = 0;
	// Todo: how to initialize metadatas?
	for (int i=0; i < nblocks; i++) {
		cs->metadatas[i] = malloc(sizeof(struct block_info));
	}
	cs->clock_hand = 0;
	
	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = cs;
	bi->getninodes = wtclockdisk_getninodes;
	bi->getsize = wtclockdisk_getsize;
	bi->setsize = wtclockdisk_setsize;
	bi->read = wtclockdisk_read;
	bi->write = wtclockdisk_write;
	bi->release = wtclockdisk_release;
	bi->sync = wtclockdisk_sync;
	return bi;
}
