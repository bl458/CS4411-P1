/*
 * (C) 2019, Cornell University
 * All rights reserved.
 */

/* Author: Kenneth Fang (kwf37), Mena Wang (mw749), December 2019
 *
 * The superblock maintains the number of inode blocks and a pointer
 * to the free list structure.
 * 
 * The free list is stored in a linked list, where each node in the linked
 * list is filled with pointers to free blocks.
 * 
 *  The inode structure is as follows:
 *  Inode:
 *  * 12 direct pointers
 *  * 1 single indirect pointer
 *  * 1 double indirect pointer
 *  * 1 triple indirect pointer
 */

/* UNIX File System Layout
 * +-------------+-----------------+---------------+-----+-------------------+
 * | super block |   inode blocks  | data cluster1 | ... | last data cluster |
 * +-------------+-----------------+---------------+-----+-------------------+
 * |<- 1 block ->|<-n_inodeblocks->|
 */

typedef unsigned int datablock_no; // index of a datablock

#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(struct unixdisk_inode))
#define ENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(datablock_no))
#define ENTRIES_PER_FREE_BLOCK ((BLOCK_SIZE - sizeof(block_no) - sizeof(datablock_no)) / sizeof(datablock_no))
#define FAT_PER_BLOCK (BLOCK_SIZE / sizeof(struct unixdisk_fatentry))

/* Contents of the "superblock".  There is only one of these.
 */
struct unixdisk_superblock
{
    block_no n_inodeblocks;   // # blocks containing inodes
    datablock_no free_list;   // datablock index of the head of the free list
    datablock_no free_offset; // index of first empty slot in free list metadata block
};

/* An inode describes a file (= virtual block store). 
 * The Unix-style inodes normally contain metadata, but this
 * implementation will just have the direct and indirect block
 * entries. 0 represents an invalid/unused pointer.
 */
struct unixdisk_inode
{
    block_no nblocks; // total size (in blocks) of the file
    datablock_no direct_blocks[12];
    datablock_no single_indirect;
    datablock_no double_indirect;
    datablock_no triple_indirect;
};

/* An inode block is filled with inodes.
 */
struct unixdisk_inodeblock
{
    struct unixdisk_inode inodes[INODES_PER_BLOCK];
};

/* An indirect block is filled with entries.
 */
struct unixdisk_indirectblock
{
    datablock_no entries[ENTRIES_PER_BLOCK];
};

/* A free block has a pointer to the next free block and is filled with entries
 * to other free blocks.
 */
struct unixdisk_freeblock
{
    block_no nblocks;
    datablock_no next;
    datablock_no entries[ENTRIES_PER_FREE_BLOCK];
};

/* A convenient structure that's the union of all block types.  It should
 * have size BLOCK_SIZE, which may not be true for the elements.
 */
union unixdisk_block {
    block_t datablock;
    struct unixdisk_superblock superblock;
    struct unixdisk_inodeblock inodeblock;
    struct unixdisk_indirectblock indirectblock;
    struct unixdisk_freeblock freeblock;
};
