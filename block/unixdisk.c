/*
 * (C) 2019, Cornell University
 * All rights reserved.
 */

/* Author: Kenneth Fang (kwf37), Mena Wang (mw749), December 2019
 *
 * This code implements a set of virtualized block store on top of another
 *	block store.  Each virtualized block store is identified by a so-called
 *	"inode number", which indexes into an array of inodes. It is based off
 *  of the unix filesystem, but without file metadata. 
 * 
 *  The inode structure is as follows:
 *  Inode:
 *  * 12 direct pointers
 *  * 1 single indirect pointer
 *  * 1 double indirect pointer
 *  * 1 triple indirect pointer
 * 
 *  We were unable to test the triple indirect pointers because our test suite
 *  does not allocate enough data blocks in RAM for the triple indirect pointers
 *  to be used.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <egos/block_store.h>

#include "unixdisk.h"

#define EPB_2 (ENTRIES_PER_BLOCK * ENTRIES_PER_BLOCK)                     // Entries Per Block Squared
#define EPB_3 (ENTRIES_PER_BLOCK * ENTRIES_PER_BLOCK * ENTRIES_PER_BLOCK) // Entries Per Block Cubed

/* Temporary information about the file system and a particular inode.
 * Convenient for all operations. See "unixdisk.h" for field details.
 */
struct unixdisk_snapshot
{
    union unixdisk_block superblock;
    union unixdisk_block inodeblock;
    block_no inode_blockno;
    unsigned int inode_no;
    struct unixdisk_inode *inode;
};

struct unixdisk_state
{
    block_store_t *below; // block store below
    unsigned int ninodes; // number of inodes
};

static block_t null_block; // a block filled with null bytes

static int unixdisk_get_snapshot(struct unixdisk_snapshot *snapshot,
                                 block_store_t *below, unsigned int inode_no)
{
    snapshot->inode_no = inode_no;

    /* Get the super block
     */
    if ((*below->read)(below, 0, 0, (block_t *)&snapshot->superblock) < 0)
    {
        return -1;
    }

    /* Check the inode number.
     */
    if (inode_no >= snapshot->superblock.superblock.n_inodeblocks * INODES_PER_BLOCK)
    {
        fprintf(stderr, "!!UNIXDISK: inode number too large %u %u\n", inode_no, snapshot->superblock.superblock.n_inodeblocks);
        return -1;
    }

    /* Find the inode.
    */
    snapshot->inode_blockno = 1 + inode_no / INODES_PER_BLOCK;
    if ((*below->read)(below, 0, snapshot->inode_blockno, (block_t *)&snapshot->inodeblock) < 0)
    {
        return -1;
    }
    snapshot->inode = &(snapshot->inodeblock.inodeblock.inodes[inode_no % INODES_PER_BLOCK]);

    return 0;
}

/* Create a new UNIX file system on the block store below
 */
int unixdisk_create(block_store_t *below, unsigned int below_ino, unsigned int ninodes)
{
    /* Compute the number of inode blocks needed to store the inodes.
	 */
    unsigned int n_inodeblocks =
        (ninodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;

	/* Read the superblock to see if it's already initialized.
	 */
    union unixdisk_block to_write_super;
	if ((*below->read)(below, below_ino, 0, (block_t *) &to_write_super.superblock) < 0) {
		return -1;
	}
	if (to_write_super.superblock.n_inodeblocks != 0) {
		assert(to_write_super.superblock.n_inodeblocks >= n_inodeblocks);
		return 0;
	}

    /* Setup Inodes- They all start out empty */
    for (unsigned int i = 1; i <= n_inodeblocks; i++)
    {
        if ((*below->write)(below, below_ino, i, (block_t *)&null_block) < 0)
        {
            return -1;
        }
    }
    /* Get the size of the underlying disk */
    unsigned int n_blocks = (*below->getsize)(below, below_ino);

    /* Initialize free list */
    /* Note that because a free list utilizes metadata nodes, each
     * block can represent ENTRIES_PER_FREE_BLOCK + 1 free nodes */
    unsigned int n_datablocks = n_blocks - n_inodeblocks - 1;

    unsigned int i, j, next_entry;

    union unixdisk_block to_write_free;
    struct unixdisk_freeblock freeblock;
    // First initialize first block as the head of the blocks
    unsigned int n_remainder = n_datablocks % (ENTRIES_PER_FREE_BLOCK + 1);
    for (j = 0; j < n_remainder - 1; j++)
    {
        next_entry = j + 1;
        freeblock.entries[j] = next_entry;
    }
    // The following line sets next to 0 if there are no more free blocks needed,
    // or ENTRIES_PER_FREE_BLOCK + 1 if there are more datablocks that need to be
    // added to the free list (requiring another free list metadata block)
    freeblock.next = n_datablocks > n_remainder ? n_remainder : (unsigned int)-1;
    freeblock.nblocks = n_remainder - 1;
    to_write_free.freeblock = freeblock;
    if ((*below->write)(below, below_ino, n_inodeblocks + 1, (block_t *)&to_write_free) < 0)
    {
        return -1;
    }

    // Next Initialize rest of the blocks, which should be completely full
    // Determine indices of Metadata blocks in free list
    for (i = 0; i < n_datablocks / (ENTRIES_PER_FREE_BLOCK + 1); i++)
    {
        // Iterate over entries to put in the free block
        for (j = 0; j < ENTRIES_PER_FREE_BLOCK; j++)
        {
            next_entry = i * (ENTRIES_PER_FREE_BLOCK + 1) + j + 1 + n_remainder;
            freeblock.entries[j] = next_entry;
        }

        // Set freelist metadata fields
        if (i == n_datablocks / (ENTRIES_PER_FREE_BLOCK + 1) - 1)
        {
            // Reached end of list
            freeblock.next = (unsigned int)-1;
        }
        else
        {
            freeblock.next = (i + 1) * (ENTRIES_PER_FREE_BLOCK + 1) + n_remainder;
        }
        freeblock.nblocks = j;
        to_write_free.freeblock = freeblock;
        if ((*below->write)(below, below_ino, i * (ENTRIES_PER_FREE_BLOCK + 1) + n_remainder + 1 + n_inodeblocks, (block_t *)&to_write_free) < 0)
        {
            return -1;
        }
    }

    /* Initialize superblock */
    struct unixdisk_superblock superblock;
    superblock.n_inodeblocks = n_inodeblocks;
    superblock.free_list = 0;
    superblock.free_offset = n_remainder - 1;
    to_write_super.superblock = superblock;

    if ((*below->write)(below, below_ino, 0, (block_t *)&to_write_super) < 0)
    {
        return -1;
    }
    return 0;
}

/**
 * Helper function for traversing UNIX table
 * Returns the absolute block number of the data to read (not data block number)
 */
static block_no unixdisk_traverse(block_store_t *below, struct unixdisk_snapshot *snapshot, unsigned int offset)
{
    /* Determine which pointer to look from */
    if (offset < 12)
    {
        // Direct Blocks
        datablock_no to_read = snapshot->inode->direct_blocks[offset];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        return to_read;
    }
    else if (offset < 12 + ENTRIES_PER_BLOCK)
    {
        // Single Indirect Block
        union unixdisk_block read_data;

        datablock_no to_read = snapshot->inode->single_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&read_data) < 0)
        {
            return -1;
        }

        // First Indirect Block
        to_read = read_data.indirectblock.entries[offset - 12];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        return to_read;
    }
    else if (offset < 12 + ENTRIES_PER_BLOCK + EPB_2)
    {
        // Double Indirect Block
        union unixdisk_block read_data;
        unsigned int first_index, second_index;

        datablock_no to_read = snapshot->inode->double_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&read_data) < 0)
        {
            return -1;
        }
        // First indirect block
        first_index = (offset - 12 - ENTRIES_PER_BLOCK) / ENTRIES_PER_BLOCK;
        to_read = read_data.indirectblock.entries[first_index];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&read_data) < 0)
        {
            return -1;
        }
        // Second indirect block
        second_index = offset - 12 - ENTRIES_PER_BLOCK - first_index * ENTRIES_PER_BLOCK;
        to_read = read_data.indirectblock.entries[second_index];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        return to_read;
    }
    else
    {
        assert(offset < 12 + ENTRIES_PER_BLOCK + EPB_2 + EPB_3);
        // Triple Indirect Block
        union unixdisk_block read_data;
        unsigned int first_index, second_index, third_index;

        datablock_no to_read = snapshot->inode->double_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&read_data) < 0)
        {
            return -1;
        }
        // First indirect block
        first_index = (offset - 12 - ENTRIES_PER_BLOCK - EPB_2) / EPB_2;
        to_read = read_data.indirectblock.entries[first_index];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&read_data) < 0)
        {
            return -1;
        }
        // Second indirect block
        second_index = (offset - 12 - ENTRIES_PER_BLOCK - EPB_2 - first_index * EPB_2) / ENTRIES_PER_BLOCK;
        to_read = read_data.indirectblock.entries[second_index];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&read_data) < 0)
        {
            return -1;
        }
        // Third indirect block
        third_index = offset - 12 - ENTRIES_PER_BLOCK - EPB_2 - first_index * EPB_2 - second_index * ENTRIES_PER_BLOCK;
        to_read = read_data.indirectblock.entries[third_index];
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        return to_read;
    }
}
/*
 * Helper function that returns the datablock_no of the next free entry.
 * Modifes the superblock and the free list as a side effect, so that the
 * head is no longer in the free list after this function is called.
 * 
 * Returns (unsigned int)-1 on error
 */
static datablock_no unixdisk_popfree(block_store_t *below, struct unixdisk_snapshot *snapshot)
{
    // Check if free list is empty
    if (snapshot->superblock.superblock.free_list == (unsigned int)-1)
    {
        fprintf(stderr, "!!TDERR: disk out of space!\n");
        // panic("Out of space!");
        return -1;
    }

    // Compute next free datablock
    unsigned int free_list = snapshot->superblock.superblock.free_list + 1 + snapshot->superblock.superblock.n_inodeblocks;

    datablock_no next;
    union unixdisk_block free_head;
    if ((below->read)(below, 0, free_list, (block_t *)&free_head) < 0)
    {
        return -1;
    }

    if (snapshot->superblock.superblock.free_offset > 0)
    {
        // Head of free list has some pointers that can be allocated- take latest one out
        next = free_head.freeblock.entries[--snapshot->superblock.superblock.free_offset];
    }
    else
    {
        assert(snapshot->superblock.superblock.free_offset == 0);
        // Head of free list is empty- can allocate this block
        next = snapshot->superblock.superblock.free_list;
        snapshot->superblock.superblock.free_list = free_head.freeblock.next;
        snapshot->superblock.superblock.free_offset = ENTRIES_PER_FREE_BLOCK;
    }

    // Write superblock back
    if ((*below->write)(below, 0, 0, (block_t *)&snapshot->superblock) < 0)
    {
        return -1;
    }
    return next;
}

/*
 * Helper function that pushes a block onto the free list
 * 
 * Returns -1 on error, 0 on success
 */
static int unixdisk_pushfree(block_store_t *below, struct unixdisk_snapshot *snapshot, datablock_no block_no)
{
    unsigned int free_list = snapshot->superblock.superblock.free_list + 1 + snapshot->superblock.superblock.n_inodeblocks;

    union unixdisk_block free_head;
    if ((below->read)(below, 0, free_list, (block_t *)&free_head) < 0)
    {
        return -1;
    }

    if (snapshot->superblock.superblock.free_offset < ENTRIES_PER_FREE_BLOCK)
    {
        // Head of free list has some free slots- stick the block_no in
        free_head.freeblock.entries[snapshot->superblock.superblock.free_offset++] = block_no;
        // Write head of freelist back
        if ((*below->write)(below, 0, free_list, (block_t *)&free_head) < 0)
        {
            return -1;
        }
        // Write superblock back
        if ((*below->write)(below, 0, 0, (block_t *)&snapshot->superblock) < 0)
        {
            return -1;
        }
        return 0;
    }
    else
    {
        assert(snapshot->superblock.superblock.free_offset == ENTRIES_PER_FREE_BLOCK);
        // Head of free list is full- use block_no as new head
        free_head.freeblock.next = snapshot->superblock.superblock.free_list;
        snapshot->superblock.superblock.free_list = block_no;
        snapshot->superblock.superblock.free_offset = 0;
        // Write head of freelist back
        if ((*below->write)(below, 0, block_no + 1 + snapshot->superblock.superblock.n_inodeblocks, (block_t *)&free_head) < 0)
        {
            return -1;
        }
        // Write superblock back
        if ((*below->write)(below, 0, 0, (block_t *)&snapshot->superblock) < 0)
        {
            return -1;
        }
        return 0;
    }
}
static int unixdisk_alloc(block_store_t *below, struct unixdisk_snapshot *snapshot,
                          unsigned int offset)
{
    assert(offset >= snapshot->inode->nblocks);
    unsigned int i = snapshot->inode->nblocks; // Overall index of data

    unsigned int j, k;

    // Allocate in Direct Blocks
    while (i < 12 && i <= offset)
    {
        datablock_no next = unixdisk_popfree(below, snapshot);
        snapshot->inode->direct_blocks[i] = next;
        i++;
    }
    // Allocate in Single Indirect Block
    if (i < 12 + ENTRIES_PER_BLOCK && i <= offset) // Adding this if statement saves extraneous reads to indirect blocks
    {
        assert(i >= 12);
        // Check if we need to initialize indirect block
        if (snapshot->inode->nblocks <= 12)
        {
            snapshot->inode->single_indirect = unixdisk_popfree(below, snapshot);
        }
        // Allocate in Single Indirect Block
        union unixdisk_block indirectblock;
        datablock_no to_read = snapshot->inode->single_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&indirectblock) < 0)
        {
            return -1;
        }
        while (i < 12 + ENTRIES_PER_BLOCK && i <= offset)
        {
            datablock_no next = unixdisk_popfree(below, snapshot);
            indirectblock.indirectblock.entries[i - 12] = next;
            i++;
        }
        if ((below->write)(below, 0, to_read, (block_t *)&indirectblock) < 0)
        {
            return -1;
        }
    }
    if (i < 12 + ENTRIES_PER_BLOCK + EPB_2 && i <= offset)
    {
        assert(i >= 12 + ENTRIES_PER_BLOCK);
        // Check if we need to initialize indirect block
        if (snapshot->inode->nblocks <= 12 + ENTRIES_PER_BLOCK)
        {
            snapshot->inode->double_indirect = unixdisk_popfree(below, snapshot);
        }
        // Allocate in Double Indirect Block
        union unixdisk_block indirect1, indirect2;
        datablock_no to_read = snapshot->inode->double_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&indirect1) < 0)
        {
            return -1;
        }
        while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 && i <= offset)
        {
            j = (i - 12 - ENTRIES_PER_BLOCK) / ENTRIES_PER_BLOCK;
            // Check if we need to initialize indirect block
            if (snapshot->inode->nblocks <= 12 + ENTRIES_PER_BLOCK + j * ENTRIES_PER_BLOCK)
            {
                indirect1.indirectblock.entries[j] = unixdisk_popfree(below, snapshot);
            }
            datablock_no to_read = indirect1.indirectblock.entries[j];
            to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
            if ((below->read)(below, 0, to_read, (block_t *)&indirect2) < 0)
            {
                return -1;
            }
            while (i < 12 + ENTRIES_PER_BLOCK + (j + 1) * ENTRIES_PER_BLOCK && i <= offset)
            {
                datablock_no next = unixdisk_popfree(below, snapshot);
                indirect2.indirectblock.entries[i - 12 - ENTRIES_PER_BLOCK - j * ENTRIES_PER_BLOCK] = next;
                i++;
            }
            if ((below->write)(below, 0, to_read, (block_t *)&indirect2) < 0)
            {
                return -1;
            }
            j++;
        }
        if ((below->write)(below, 0, to_read, (block_t *)&indirect1) < 0)
        {
            return -1;
        }
    }
    if (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + EPB_3 && i <= offset)
    {
        assert(i >= 12 + ENTRIES_PER_BLOCK + EPB_2);
        // Check if we need to initialize indirect block
        if (snapshot->inode->nblocks <= 12 + ENTRIES_PER_BLOCK + EPB_2)
        {
            snapshot->inode->triple_indirect = unixdisk_popfree(below, snapshot);
        }
        // Allocate in Triple Indirect Block
        union unixdisk_block indirect1, indirect2, indirect3;
        datablock_no to_read = snapshot->inode->double_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&indirect1) < 0)
        {
            return -1;
        }
        while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + EPB_3 && i <= offset)
        {
            j = (i - 12 - ENTRIES_PER_BLOCK - EPB_2) / EPB_2;
            // Check if we need to initialize indirect block
            if (snapshot->inode->nblocks <= 12 + ENTRIES_PER_BLOCK + EPB_2 + j * EPB_2)
            {
                indirect1.indirectblock.entries[j] = unixdisk_popfree(below, snapshot);
            }
            datablock_no to_read = indirect1.indirectblock.entries[j];
            to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
            if ((below->read)(below, 0, to_read, (block_t *)&indirect2) < 0)
            {
                return -1;
            }
            while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + (j + 1) * EPB_2 && i <= offset)
            {
                k = (i - 12 - ENTRIES_PER_BLOCK - EPB_2 - j * EPB_2) / ENTRIES_PER_BLOCK;
                // Check if we need to initialize indirect block
                if (snapshot->inode->nblocks <= 12 + ENTRIES_PER_BLOCK + EPB_2 + j * EPB_2 + k * ENTRIES_PER_BLOCK)
                {
                    indirect2.indirectblock.entries[k] = unixdisk_popfree(below, snapshot);
                }
                datablock_no to_read = indirect2.indirectblock.entries[k];
                to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
                if ((below->read)(below, 0, to_read, (block_t *)&indirect3) < 0)
                {
                    return -1;
                }
                while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + j * EPB_2 + (k + 1) * ENTRIES_PER_BLOCK && i <= offset)
                {
                    datablock_no next = unixdisk_popfree(below, snapshot);
                    indirect3.indirectblock.entries[i - 12 - ENTRIES_PER_BLOCK - EPB_2 - j * EPB_2 - k * ENTRIES_PER_BLOCK] = next;
                    i++;
                }
                if ((below->write)(below, 0, to_read, (block_t *)&indirect3) < 0)
                {
                    return -1;
                }
                k++;
            }
            if ((below->write)(below, 0, to_read, (block_t *)&indirect2) < 0)
            {
                return -1;
            }
            j++;
        }
        if ((below->write)(below, 0, to_read, (block_t *)&indirect1) < 0)
        {
            return -1;
        }
    }
    // Lastly Write the new inode
    snapshot->inode->nblocks = offset + 1;
    if ((below->write)(below, 0, snapshot->inode_blockno, (block_t *)&snapshot->inodeblock) < 0)
    {
        return -1;
    }
    return 0;
}

static void unixdisk_free_file(struct unixdisk_snapshot *snapshot,
                               block_store_t *below)
{
    /* Your code goes here:
     */
    // First we need to free all the blocks.
    // The nested for loops exist to avoid re-reading blocks unnecessarily
    unsigned int i, j, k;

    i = 0;
    j = 0;
    k = 0;
    /* Free Direct Blocks */
    while (i < 12 && i < snapshot->inode->nblocks)
    {
        // Free Direct Blocks
        unixdisk_pushfree(below, snapshot, snapshot->inode->direct_blocks[i]);
        i++;
    }
    /* Free Single Indirect Blocks */
    if (i < ENTRIES_PER_BLOCK + 12 && i < snapshot->inode->nblocks)
    {
        // Read Single Indirect Block
        union unixdisk_block indirectblock;
        datablock_no to_read = snapshot->inode->single_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&indirectblock) < 0)
        {
            return;
        }
        // Free Single Indirect Block Pointers
        while (i < 12 + ENTRIES_PER_BLOCK && i < snapshot->inode->nblocks)
        {
            unixdisk_pushfree(below, snapshot, indirectblock.indirectblock.entries[i - 12]);
            i++;
        }
        // Free Single Indirect block
        unixdisk_pushfree(below, snapshot, snapshot->inode->single_indirect);
    }
    /* Free Double Indirect Blocks */
    if (i < EPB_2 + ENTRIES_PER_BLOCK + 12 && i < snapshot->inode->nblocks)
    {
        // Read Double Indirect Block
        union unixdisk_block indirect1, indirect2;
        datablock_no to_read = snapshot->inode->double_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&indirect1) < 0)
        {
            return;
        }
        while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 && i < snapshot->inode->nblocks)
        {
            j = (i - 12 - ENTRIES_PER_BLOCK) / ENTRIES_PER_BLOCK;

            // Read Single Indirect Block
            datablock_no to_read = indirect1.indirectblock.entries[j];
            to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
            if ((below->read)(below, 0, to_read, (block_t *)&indirect2) < 0)
            {
                return;
            }

            while (i < 12 + ENTRIES_PER_BLOCK + (j + 1) * ENTRIES_PER_BLOCK && i < snapshot->inode->nblocks)
            {
                // Free data blocks
                unixdisk_pushfree(below, snapshot, indirect2.indirectblock.entries[i - 12 - ENTRIES_PER_BLOCK - j * ENTRIES_PER_BLOCK]);
                i++;
            }
            // Free Single Indirect Block
            unixdisk_pushfree(below, snapshot, indirect1.indirectblock.entries[j]);
            j++;
        }
        // Free Double Indirect Block
        unixdisk_pushfree(below, snapshot, snapshot->inode->double_indirect);
    }
    /* Free Third Indirect Blocks */
    if (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + EPB_3 && i < snapshot->inode->nblocks)
    {
        assert(i >= 12 + ENTRIES_PER_BLOCK + EPB_2);
        // Read Triple Indirect Block
        union unixdisk_block indirect1, indirect2, indirect3;
        datablock_no to_read = snapshot->inode->double_indirect;
        to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
        if ((below->read)(below, 0, to_read, (block_t *)&indirect1) < 0)
        {
            return;
        }
        while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + EPB_3 && i < snapshot->inode->nblocks)
        {
            // Read Double Indirect Block
            j = (i - 12 - ENTRIES_PER_BLOCK - EPB_2) / EPB_2;
            datablock_no to_read = indirect1.indirectblock.entries[j];
            to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
            if ((below->read)(below, 0, to_read, (block_t *)&indirect2) < 0)
            {
                return;
            }
            while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + (j + 1) * EPB_2 && i < snapshot->inode->nblocks)
            {
                // Read Single Indirect Block
                k = (i - 12 - ENTRIES_PER_BLOCK - EPB_2 - j * EPB_2) / ENTRIES_PER_BLOCK;
                datablock_no to_read = indirect2.indirectblock.entries[k];
                to_read += 1 + snapshot->superblock.superblock.n_inodeblocks;
                if ((below->read)(below, 0, to_read, (block_t *)&indirect3) < 0)
                {
                    return;
                }
                // Free Data Blocks
                while (i < 12 + ENTRIES_PER_BLOCK + EPB_2 + j * EPB_2 + (k + 1) * ENTRIES_PER_BLOCK && i < snapshot->inode->nblocks)
                {
                    // Free Data Block
                    unixdisk_pushfree(below, snapshot, indirect3.indirectblock.entries[i - 12 - ENTRIES_PER_BLOCK - EPB_2 - j * EPB_2 - k * ENTRIES_PER_BLOCK]);
                    i++;
                }
                // Free Single Indirect Block
                unixdisk_pushfree(below, snapshot, indirect2.indirectblock.entries[k]);
                k++;
            }
            // Free Double Indirect Block
            unixdisk_pushfree(below, snapshot, indirect1.indirectblock.entries[j]);
            j++;
        }
        // Free Triple Indirect Block
        unixdisk_pushfree(below, snapshot, snapshot->inode->triple_indirect);
    }

    assert(i == snapshot->inode->nblocks);

    /* Done freeing all blocks, now need to update inode */
    /* Update Inode */
    snapshot->inode->nblocks = 0;
    if ((*below->write)(below, 0, snapshot->inode_blockno, (block_t *)&snapshot->inodeblock) < 0)
    {
        return;
    }
}

/* Write *block at the given block number 'offset'.
 */
static int unixdisk_write(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block)
{
    /* Your code goes here:
     */
    struct unixdisk_state *ts = this_bs->state;

    /* Get info from underlying file system.
	 */
    struct unixdisk_snapshot snapshot;
    if (unixdisk_get_snapshot(&snapshot, ts->below, ino) < 0)
    {
        return -1;
    }

    /* Check if offset is too big */
    if (offset >= snapshot.inode->nblocks)
    {

        /* Offset big- need to allocate new blocks */
        if (unixdisk_alloc(ts->below, &snapshot, offset) < 0)
        {
            return -1;
        }
    }
    /* Traverse to the relevant datablock */
    block_no to_write = unixdisk_traverse(ts->below, &snapshot, offset);
    if ((ts->below->write)(ts->below, 0, to_write, (block_t *)block) < 0)
    {
        return -1;
    }

    return 0;
}

/* Read a block at the given block number 'offset' and return in *block.
 */
static int unixdisk_read(block_store_t *this_bs, unsigned int ino, block_no offset, block_t *block)
{
    /* Your code goes here:
     */
    struct unixdisk_state *ts = this_bs->state;

    /* Get info from underlying file system.
	 */
    struct unixdisk_snapshot snapshot;
    if (unixdisk_get_snapshot(&snapshot, ts->below, ino) < 0)
    {
        return -1;
    }

    /* Check if offset is too big */
    if (offset >= snapshot.inode->nblocks)
    {
        fprintf(stderr, "!!TDERR: offset too large\n");
        return -1;
    }

    /* Traverse Pointers */
    block_no to_read = unixdisk_traverse(ts->below, &snapshot, offset);
    if ((ts->below->read)(ts->below, 0, to_read, (block_t *)block) < 0)
    {
        return -1;
    }
    return 0;
}

static int unixdisk_getninodes(block_store_t *this_bs)
{
    struct unixdisk_state *fs = this_bs->state;
    union unixdisk_block superblock;
    if((*fs->below->read)(fs->below, 0, 0, (block_t*) &superblock) < 0) {
        return -1;
    }
    return superblock.superblock.n_inodeblocks * INODES_PER_BLOCK;
}

/* Get size.
 */
static int unixdisk_getsize(block_store_t *this_bs, unsigned int ino)
{
    struct unixdisk_state *fs = this_bs->state;

    /* Get info from underlying file system.
     */
    struct unixdisk_snapshot snapshot;
    if (unixdisk_get_snapshot(&snapshot, fs->below, ino) < 0)
    {
        return -1;
    }

    return snapshot.inode->nblocks;
}

/* Set the size of the file 'this_bs' to 'nblocks'.
 */
static int unixdisk_setsize(block_store_t *this_bs, unsigned int ino, block_no nblocks)
{
    struct unixdisk_state *fs = this_bs->state;

    struct unixdisk_snapshot snapshot;
    unixdisk_get_snapshot(&snapshot, fs->below, ino);
    if (nblocks == snapshot.inode->nblocks)
    {
        return nblocks;
    }
    if (nblocks > 0)
    {
        fprintf(stderr, "!!UNIXDISK: nblocks > 0 not supported\n");
        return -1;
    }

    unixdisk_free_file(&snapshot, fs->below);
    return 0;
}

static void unixdisk_release(block_store_t *this_bs)
{
    free(this_bs->state);
    free(this_bs);
}

static int unixdisk_sync(block_if bi, unsigned int ino)
{
    struct unixdisk_state *fs = bi->state;
    return (*fs->below->sync)(fs->below, 0);
}

/* Create or open a new virtual block store at the given inode number.
 */
block_store_t *unixdisk_init(block_store_t *below, unsigned int below_ino)
{
    if(below_ino != 0) {
        fprintf(stderr, "!!UNIXDISK: below_ino != 0 not supported\n");
        return NULL;
    }
    /* Create the block store state structure.
     */
    struct unixdisk_state *fs = new_alloc(struct unixdisk_state);
    fs->below = below;

    /* Return a block interface to this inode.
     */
    block_store_t *this_bs = new_alloc(block_store_t);
    this_bs->state = fs;
    this_bs->getninodes = unixdisk_getninodes;
    this_bs->getsize = unixdisk_getsize;
    this_bs->setsize = unixdisk_setsize;
    this_bs->read = unixdisk_read;
    this_bs->write = unixdisk_write;
    this_bs->release = unixdisk_release;
    this_bs->sync = unixdisk_sync;
    return this_bs;
}
