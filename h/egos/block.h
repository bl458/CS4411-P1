#ifndef _EGOS_BLOCK_H
#define _EGOS_BLOCK_H

#include <stdbool.h>
#include <egos/syscall.h>

/* This data structure is actually the header of block request message
 */
struct block_request {
    //gpid_t src;                     //  this is obsolete. the sys_recv will return src
    enum {
        BLOCK_UNUSED,               // to simplify finding bugs
        BLOCK_READ,
        BLOCK_WRITE,
        BLOCK_GETSIZE,
        BLOCK_SETSIZE,              // size is in field offset
        BLOCK_SYNC,
		BLOCK_GETNINODES
    } type;                         // type of request
    unsigned int ino;               // inode number
    unsigned int offset_nblock;     // offset in blocks (not bytes)
};

/* This data structure is actually the header of block reply message
 */
struct block_reply {
    enum block_status { BLOCK_OK, BLOCK_ERROR } status;
    unsigned int size_nblock;       // size of device in case of GETSIZE request
#define br_ninodes	size_nblock		// overloaded for getninodes
};

bool block_read(gpid_t svr, unsigned int ino, unsigned int offset, void *addr);
bool block_write(gpid_t svr, unsigned int ino, unsigned int offset, const void *addr);
bool block_getsize(gpid_t svr, unsigned int ino, unsigned int *psize_nblock);
bool block_setsize(gpid_t svr, unsigned int ino, unsigned int size_nblock);
bool block_sync(gpid_t svr, unsigned int ino);
bool block_getninodes(gpid_t svr, unsigned int *ninodes);

#endif // _EGOS_BLOCK_H
