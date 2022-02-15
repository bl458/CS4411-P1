/* Author: Robbert van Renesse, August 2015
 *
 * This block store module simply forwards its method calls to an
 * underlying block store, but prints out invocations and returns
 * for debugging purposes:
 *
 *		block_if debugdisk_init(block_if below, char *descr);
 *			'below' is the underlying block store.  'descr' is included
 *			in print statements in case there are multiple debug modules
 *			active.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <egos/block_store.h>

struct debugdisk_state {
	block_if below;			// block store below
	const char *descr;		// description of underlying block store.
};

static int debugdisk_getninodes(block_if bi){
	struct debugdisk_state *ds = bi->state;

	return (*ds->below->getninodes)(ds->below);
}

static int debugdisk_getsize(block_if bi, unsigned int ino){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke getsize(%u)\n", ds->descr, ino);
	int nblocks = (*ds->below->getsize)(ds->below, ino);
	fprintf(stderr, "%s: getsize(%u) --> %d\n", ds->descr, ino, nblocks);
	return nblocks;
}

static int debugdisk_setsize(block_if bi, unsigned int ino, block_no nblocks){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke setsize(%u, %u)\n", ds->descr, ino, nblocks);
	int r = (*ds->below->setsize)(ds->below, ino, nblocks);
	fprintf(stderr, "%s: setsize(%u, %u) --> %d\n", ds->descr, ino, nblocks, r);
	return r;
}

static int debugdisk_read(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke read(ino = %u, offset = %u)\n", ds->descr, ino, offset);
	int r = (*ds->below->read)(ds->below, ino, offset, block);
	fprintf(stderr, "%s: read(ino = %u, offset = %u) --> %d\n", ds->descr, ino, offset, r);
	return r;
}

static int debugdisk_write(block_if bi, unsigned int ino, block_no offset, block_t *block){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke write(ino = %u, offset = %u)\n", ds->descr, ino, offset);
	int r = (*ds->below->write)(ds->below, ino, offset, block);
	fprintf(stderr, "%s: write(ino = %u, offset = %u) --> %d\n", ds->descr, ino, offset, r);
	return r;
}

static void debugdisk_release(block_if bi){
	struct debugdisk_state *ds = bi->state;
	fprintf(stderr, "%s: release()\n", ds->descr);
	free(bi->state);
	free(bi);
}

static int debugdisk_sync(block_if bi, unsigned int ino){
	struct debugdisk_state *ds = bi->state;
	return (*ds->below->sync)(ds->below, ino);
}

block_if debugdisk_init(block_if below, const char *descr){
	/* Create the block store state structure.
	 */
	struct debugdisk_state *ds = new_alloc(struct debugdisk_state);
	ds->below = below;
	ds->descr = descr;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ds;
	bi->getninodes = debugdisk_getninodes;
	bi->getsize = debugdisk_getsize;
	bi->setsize = debugdisk_setsize;
	bi->read = debugdisk_read;
	bi->write = debugdisk_write;
	bi->release = debugdisk_release;
	bi->sync = debugdisk_sync;
	return bi;
}
