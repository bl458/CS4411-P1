#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <egos/block_store.h>

#ifdef HW_FS
#include "fatdisk.h"

/* Temporary information about the file system and a particular inode.
 * Convenient for all operations. See "fatdisk.h" for field details.
 */
struct fatdisk_snapshot {
    union fatdisk_block superblock;
    union fatdisk_block inodeblock;
    block_no inode_blockno;
    unsigned int inode_no;
    struct fatdisk_inode *inode;
};

struct fatdisk_state {
    block_store_t *below;   // block store below
    unsigned int below_ino; // inode number to use for the block store below
    unsigned int ninodes;   // number of inodes
};


static void panic(char *s){
	fprintf(stderr, "Panic: %s\n", s);
	exit(1);
}

static int fatdisk_get_snapshot(struct fatdisk_snapshot *snapshot,
                                struct fatdisk_state* fs, unsigned int inode_no) {
    snapshot->inode_no = inode_no;

    /* Get the super block.
     */
    if ((*fs->below->read)(fs->below, fs->below_ino, 0, (block_t *) &snapshot->superblock) < 0) {
        return -1;
    }

    /* Check the inode number.
     */
    if (inode_no >= snapshot->superblock.superblock.n_inodeblocks * INODES_PER_BLOCK) {
        fprintf(stderr, "!!FATDISK: inode number too large %u %u\n", inode_no, snapshot->superblock.superblock.n_inodeblocks);
        return -1;
    }

    /* Find the inode.
    */
    snapshot->inode_blockno = 1 + inode_no / INODES_PER_BLOCK;
    if ((*fs->below->read)(fs->below, fs->below_ino, snapshot->inode_blockno, (block_t *) &snapshot->inodeblock) < 0) {
        return -1;
    }
    snapshot->inode = &(snapshot->inodeblock.inodeblock.inodes[inode_no % INODES_PER_BLOCK]);

    return 0;
}

/* Create a new FAT file system on the specified inode of the block store below
 */
int fatdisk_create(block_store_t *below, unsigned int below_ino, unsigned int ninodes) {
    union fatdisk_block f_block_check;
    if ((*below->read)(below, below_ino, 0, &f_block_check.datablock) == -1) {
        return -1; 
    }
    if (f_block_check.superblock.n_inodeblocks != 0) {
        return 0;
    }
        
    int total_blocks = (*below->getsize)(below, below_ino);
    int num_inodeblocks;
    if(ninodes%INODES_PER_BLOCK == 0){
        num_inodeblocks = ninodes/INODES_PER_BLOCK;
    }
    else{
        num_inodeblocks = ninodes/INODES_PER_BLOCK + 1;
    }
    
    int remainder = total_blocks - num_inodeblocks - 1;
    int full_fatblocks = remainder/(1 + FAT_PER_BLOCK);
    remainder -= full_fatblocks;
    int num_fatblocks;
    int num_fatentries;
    if(remainder > 0){
        num_fatblocks = full_fatblocks + 1;
        num_fatentries = full_fatblocks * FAT_PER_BLOCK + remainder - 1;
    }
    else{
        num_fatblocks = full_fatblocks;
        num_fatentries = full_fatblocks * FAT_PER_BLOCK;
    }

    union fatdisk_block f_block_inode;
    for (int i=0; i<INODES_PER_BLOCK; i++) {
        f_block_inode.inodeblock.inodes[i].head = -1; 
        f_block_inode.inodeblock.inodes[i].nblocks = 0; 
    }

    int offset = 1; 
    for(int i=0; i<num_inodeblocks; i++) {
        if ((*below->write)(below, below_ino, offset, &f_block_inode.datablock) == -1) {
            return -1; 
        }
        offset += 1; 
    }

    offset = 1 + n_inodeblocks;
    for(int i=0; i<num_fatblocks; i++) {
        union fatdisk_block f_block_fat;
        for(int j=0; j<FAT_PER_BLOCK; j++){
            if (i * FAT_PER_BLOCK + j >= num_fatentries) {
                //TODO ask 
                f_block_fat.fatblock.entries[j].next = -1;
            }
            else {
                f_block_fat.fatblock.entries[j].next = i * FAT_PER_BLOCK + j + 1;  
            }
        }

        if ((*below->write)(below, below_ino, offset, &f_block_inode.datablock) == -1) {
            return -1; 
        }

        offset += 1; 
    }

    return 0;
}


static void fatdisk_free_file(struct fatdisk_snapshot *snapshot,
                              struct fatdisk_state *fs) {
    /* Your code goes here:
     */
}


/* Write *block at the given block number 'offset'.
 */
static int fatdisk_write(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block) {
    /* Your code goes here:
     */
    return 0;
}

/* Read a block at the given block number 'offset' and return in *block.
 */
static int fatdisk_read(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block){
    /* Your code goes here:
     */
    return 0;
}

static int fatdisk_getninodes(block_store_t *this_bs){
    struct fatdisk_state *fs = this_bs->state;
    union fatdisk_block superblock;
	if ((*fs->below->read)(fs->below, fs->below_ino, 0, (block_t *) &superblock) < 0) {
        return -1;
    }
    return superblock.superblock.n_inodeblocks * INODES_PER_BLOCK;
}

/* Get size.
 */
static int fatdisk_getsize(block_store_t *this_bs, unsigned int ino){
    struct fatdisk_state *fs = this_bs->state;

    /* Get info from underlying file system.
     */
    struct fatdisk_snapshot snapshot;
    if (fatdisk_get_snapshot(&snapshot, fs, ino) < 0) {
        return -1;
    }

    return snapshot.inode->nblocks;
}

/* Set the size of the file 'this_bs' to 'nblocks'.
 */
static int fatdisk_setsize(block_store_t *this_bs, unsigned int ino, block_no nblocks){
    struct fatdisk_state *fs = this_bs->state;

    struct fatdisk_snapshot snapshot;
    fatdisk_get_snapshot(&snapshot, fs, ino);
    if (nblocks == snapshot.inode->nblocks) {
        return nblocks;
    }
    if (nblocks > 0) {
        fprintf(stderr, "!!FATDISK: nblocks > 0 not supported\n");
        return -1;
    }

    fatdisk_free_file(&snapshot, fs);
    return 0;
}

static void fatdisk_release(block_store_t *this_bs){
    free(this_bs->state);
    free(this_bs);
}

static int fatdisk_sync(block_if bi, unsigned int ino){
    struct fatdisk_state *fs = bi->state;
    return (*fs->below->sync)(fs->below, fs->below_ino);
}

/* Open a new virtual block store at the given inode number of block store "below".
 */
block_store_t *fatdisk_init(block_store_t *below, unsigned int below_ino){
    /* Create the block store state structure.
     */
    struct fatdisk_state *fs = new_alloc(struct fatdisk_state);
    fs->below = below;
    fs->below_ino = below_ino;

    /* Return a block interface to this block store.
     */
    block_store_t *this_bs = new_alloc(block_store_t);
    this_bs->state = fs;
    this_bs->getninodes = fatdisk_getninodes;
    this_bs->getsize = fatdisk_getsize;
    this_bs->setsize = fatdisk_setsize;
    this_bs->read = fatdisk_read;
    this_bs->write = fatdisk_write;
    this_bs->release = fatdisk_release;
    this_bs->sync = fatdisk_sync;
    return this_bs;
}

#endif //HW_FS
