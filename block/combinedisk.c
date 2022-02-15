/* Author: Alice Chen, April 2019
 *
 * This virtual block store module combines an array of underlying block
 * stores into a single block store.
 *
 *		block_if combinedisk_init(block_if *below, block_no partsizes[])
 *			Sum of all entries in partsizes should equate to the number of
 *			blocks of 'below'. Each read and write method on the new block
 *			store returned is a call to 'below' with ino and offset adjusted.
 *			For example, if 'below' has 24 blocks total, and partsizes is
 *			[10, 4, 10], then writing to combinedisk inode 0 offset 5 results in
 *			writing to 'below' inode 0 offset 5, (1, 1) to (0, 10+1=11), and
 *			(2, 7) to (0, 10+4+7=21).
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct combinedisk_state {
	block_if *below;				// block stores below
	unsigned int nbelow;			// #block stores below
	unsigned int *belowninodes;		// ninodes of block stores below
};

static int combinedisk_getninodes(block_if bi){
	struct combinedisk_state *cs = bi->state;
	unsigned int ninodes = 0;
	for (unsigned int i = 0; i < cs->nbelow; i++) {
		ninodes += cs->belowninodes[i];
	}
	return ninodes;
}

static int combinedisk_getsize(block_if bi, unsigned int ino){
	struct combinedisk_state *cs = bi->state;
	unsigned int nino = ino;
	unsigned int i = 0;
	while ((int) (nino - cs->belowninodes[i]) >= 0) {
		nino -= cs->belowninodes[i];
		i++;
	}
	assert(i <= cs->nbelow);
	return (*cs->below[i]->getsize)(cs->below[i], nino);
}

static int combinedisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	struct combinedisk_state *cs = bi->state;
	unsigned int nino = ino;
	unsigned int i = 0;
	while ((int) (nino - cs->belowninodes[i]) >= 0) {
		nino -= cs->belowninodes[i];
		i++;
	}
	assert(i <= cs->nbelow);
	return (*cs->below[i]->setsize)(cs->below[i], nino, nblocks);
}

static int combinedisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct combinedisk_state *cs = bi->state;
	unsigned int nino = ino;
	unsigned int i = 0;
	while ((int) (nino - cs->belowninodes[i]) >= 0) {
		nino -= cs->belowninodes[i];
		i++;
	}
	assert(i <= cs->nbelow);
	return (*cs->below[i]->read)(cs->below[i], nino, offset, block);
}

static int combinedisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct combinedisk_state *cs = bi->state;
	unsigned int nino = ino;
	unsigned int i = 0;
	while ((int) (nino - cs->belowninodes[i]) >= 0) {
		nino -= cs->belowninodes[i];
		i++;
	}
	assert(i <= cs->nbelow);
	return (*cs->below[i]->write)(cs->below[i], nino, offset, block);
}

static void combinedisk_release(block_if bi){
	free(bi->state);
	free(bi);
}

static int combinedisk_sync(block_if bi, unsigned int ino){
	struct combinedisk_state *cs = bi->state;
	unsigned int nino = ino;
	unsigned int i = 0;
	while ((int) (nino - cs->belowninodes[i]) >= 0) {
		nino -= cs->belowninodes[i];
		i++;
	}
	assert(i <= cs->nbelow);
	return (*cs->below[i]->sync)(cs->below[i], nino);
}

block_if combinedisk_init(block_if *below, unsigned int nbelow){
	/* Create the block store state structure.
	 */
	struct combinedisk_state *cs = new_alloc(struct combinedisk_state);
	cs->below = below;
	cs->nbelow = nbelow;
	cs->belowninodes = malloc(cs->nbelow * sizeof(unsigned int));
	for (unsigned int i = 0; i < cs->nbelow; i++) {
		cs->belowninodes[i] = (*cs->below[i]->getninodes)(cs->below[i]);
	}

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = cs;
	bi->getninodes = combinedisk_getninodes;
	bi->getsize = combinedisk_getsize;
	bi->setsize = combinedisk_setsize;
	bi->read = combinedisk_read;
	bi->write = combinedisk_write;
	bi->release = combinedisk_release;
	bi->sync = combinedisk_sync;
	return bi;
}
