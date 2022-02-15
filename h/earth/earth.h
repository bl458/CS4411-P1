#ifndef _EGOS_EARTH_H
#define _EGOS_EARTH_H

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

/* Interface to underlying virtualize hardware.
 */

#define VIRT_PAGES	((address_t) 0x1000)			// size of virtual address space
#define VIRT_BASE	((address_t) 0x1000000000)		// base of this space
#define PAGESIZE	4096

#define KERN_PAGES	((address_t) 0x10000)			// size of virtual address space
#define KERN_BASE	((address_t) 0x2000000000)		// where the kernel is mapped

/* Handy.
 */
#define VIRT_TOP		(VIRT_BASE + VIRT_PAGES * PAGESIZE)
#define KERN_TOP		(KERN_BASE + KERN_PAGES * PAGESIZE)

#define BLOCK_SIZE      1024         // # bytes in a disk block

/* An "address_t" is an unsigned integer that is the size of a pointer.
 */
#ifdef x86_32
typedef uint32_t address_t;
#define PRIaddr PRIx32
#endif
#ifdef x86_64
typedef uint64_t address_t;
#define PRIaddr PRIx64
#endif

typedef uint32_t page_no;		// virtual page number
typedef uint32_t frame_no;		// physical frame number

/* Protection bits.  Do not necessarily correspond to PROT_READ etc.
 */
#define P_READ			0x1
#define P_WRITE			0x2
#define P_EXEC			0x4

/* Convenient memory allocation routines.  Usage:
 *
 *	new_alloc(<type>) allocates sizeof(<type>) bytes and returns a typed
 *		pointer to this area.  The area is set to all zeroes.
 *	new_alloc_ext(<type>, ext) allocates sizeof(<type>) + ext bytes and
 *		returns a typed pointer to this area.  The first sizeof(<type>)
 *		bytes are set to zero.
 */

#define new_alloc(t)		((t *) calloc(1, sizeof(t)))
#define new_alloc_ext(t,x)	((t *) calloc(1, sizeof(t) + x))

struct gate_time {
	unsigned long seconds;
	unsigned int useconds;
	int minuteswest;
	int dsttime;
};

#endif // _EGOS_EARTH_H
