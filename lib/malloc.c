#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <earth/earth.h>
#include <stdlib.h>

#ifndef TLSF

#define ALIGN		(1 << 4)			// alignment = 16
#define ALMSK		(ALIGN - 1)			// mask

/* TODO.  Should replace this with a faster version.
 */

// #define UNIT_TEST	// define for testing only */

static char *base;		// start of list of blocks (free or allocated)

#ifdef UNIT_TEST

static char *brk;

#define SETBRK(b)		do { brk = (b); } while (0)
#define GETBRK()		brk

#else /* !UNIT_TEST */

#define SETBRK(b)		/* none */

#endif /* UNIT_TEST */

enum m_status { M_LAST, M_FREE, M_INUSE };

/* The heap is organized as follows.  It is a sequence of alternating
 * blocks and headers, although a block can be of size 0 and thus
 * headers can be next to one another.  The sequence starts and ends
 * with a header.  The last header has status M_LAST; all others are
 * either M_FREE or M_INUSE.  An M_FREE or M_INUSE header describes
 * the block that follows it.  There cannot be two consecutive free
 * blocks.  A header specifies both the size of the block (always a
 * multiple of 16) and the size of the block that precedes it (thus
 * the list can be traversed backwards).
 */
struct m_hdr {
	unsigned int size;		// size of this block
	unsigned int prev;		// size of preceding block
	unsigned int status;	// really enum m_status
	unsigned int align;		// to make header a multiple of 16 bytes
};
static bool initialized;

#define MAX_CACHE	(8192 / ALIGN)

void *M_alloc(size_t size){		// TODO capital M to prevent this from accidentally compiling
	/* Debugging */
	if (size < 0) { * (int *) 1 = 1; }
	if (size > 10000000) { * (int *) 5 = 5; }

	struct m_hdr *mh, *next, *other;

	/* The first time, set up an initial list with a free block.
	 */
	if (!initialized) {
		extern int *end_get(void);		// returns end of heap

		base = (char *) (end_get() + 1);

		/* Align to 16 bytes.
		 */
		base = (char *) (((address_t) base + ALMSK) & ~ (address_t) ALMSK);

		/* Create a free block of size PAGESIZE - sizeof(hdr), with
		 * headers at both ends.
		 */
		mh = (struct m_hdr *) base;
		SETBRK((char *) &mh[1] + PAGESIZE);
		mh->size = PAGESIZE - sizeof(*mh);
		mh->prev = 0;
		mh->status = M_FREE;
		next = (struct m_hdr *) ((char *) mh + PAGESIZE);
		next->size = 0;
		next->prev = PAGESIZE - sizeof(*mh);
		next->status = M_LAST;
		initialized = true;
	}

	/* The header plus the size should be a multiple of 16 bytes to deal
	 * with 128-bit floating point numbers.
	 */
	size = (size + ALMSK) & ~ALMSK;

	/* Loop through all chunks looking for a spot large enough.
	 */
	for (mh = (struct m_hdr *) base;;) {
		switch (mh->status) {
		case M_LAST:
			/* If the last block was free, skip back one.  It wasn't big
			 * enough but we still want to use it.
			 */
			if (mh != (struct m_hdr *) base) {
				other = ((struct m_hdr *) ((char *) mh - mh->prev)) - 1;
				if (other->status == M_FREE) {
					mh = other;
				}
			}
			/* TODO.  Can mh == base???  If so, combine with next if statement.
			 */
			else {
				* (int *) 2 = 2;
			}

			/* Allocate enough and a bit more (PAGESIZE) so we don't set
			 * the break too often.
			 */
			SETBRK((char *) &mh[1] + size + PAGESIZE);
			mh->size = size + PAGESIZE - sizeof(*mh);
			mh->status = M_FREE;
			next = (struct m_hdr *) ((char *) &mh[1] + mh->size);
#ifdef notdef
			if ((address_t) next >= (address_t) &next) {
				return 0;
			}
#endif
			next->size = 0;
			next->prev = mh->size;
			next->status = M_LAST;
			/* FALL THROUGH */

		case M_FREE:
			/* If it fits exactly, just grab it.
			 */
			if (mh->size == (unsigned int) size) {
				mh->status = M_INUSE;
				return mh + 1;
			}

			/* If it fits plus a new header, split it.
			 */
			if (mh->size >= size + sizeof(*mh)) {
				/* Keep track of the next header.
				 */
				next = (struct m_hdr *) ((char *) &mh[1] + mh->size);

				/* Insert a new header.
				 */
				other = (struct m_hdr *) ((char *) &mh[1] + size);
				other->size = mh->size - size - sizeof(*mh);
				other->prev = size;
				other->status = M_FREE;

				/* Update the following header.
				 */
				next->prev = other->size;

				/* Update the current header and return.
				 */
				mh->size = size;
				mh->status = M_INUSE;
				return mh + 1;
			}
			/* FALL THROUGH */

			/* If in use, or free and not big enough, skip to next.
			 */
		case M_INUSE:
			mh = (struct m_hdr *) ((char *) &mh[1] + mh->size);
			break;

		default:
			* (int *) 3 = 3;		// for debugging
		}
	}
}

void *m_calloc(size_t nitems, size_t size){
	size *= nitems;

	char *p = m_alloc(size);
	if (p != 0) {
		char *q = p;
		while (size-- > 0) {
			*q++ = 0;
		}
	}
	return p;
}

void m_free(void *ptr){
	if (ptr == 0) {
		return;
	}

	/* See if we can merge with the next block.
	 */
	struct m_hdr *mh = (struct m_hdr *) ptr - 1, *next, *other;
	next = (struct m_hdr *) ((char *) ptr + mh->size);
	if (next->status == M_FREE) {
		other = (struct m_hdr *) ((char *) &next[1] + next->size);
		mh->size += sizeof(*next) + next->size;
		other->prev = mh->size;
		next = other;
	}

	/* See if we can merge with the prior block, if any.
	 */
	if (mh != (struct m_hdr *) base) {
		other = (struct m_hdr *) ((char *) mh - mh->prev) - 1;
		if (other->status == M_FREE) {
			other->size += mh->size + sizeof(*mh);
			next->prev = other->size;
			return;
		}
	}

	mh->status = M_FREE;
}

/* TODO.  Something smarter.
 */
void *m_realloc(void *ptr, size_t size){
	/* See if it fits in the current block.
	 */
	if (ptr != 0 && size != 0) {
		struct m_hdr *mh = (struct m_hdr *) ptr - 1;

		if (size == ((mh->size + ALMSK) & ~ALMSK)) {
			return ptr;
		}
	}

	char *p = m_alloc(size), *q;

	if ((q = ptr) != 0) {
		struct m_hdr *mh = (struct m_hdr *) ptr - 1;
		if ((unsigned int) size > mh->size) {
			size = mh->size;
		}

		char *r = p;
		while (size > 0) {
			*r++ = *q++;
			size--;
		}

		m_free(ptr);
	}
	return p;
}

#endif // !TLSF

#ifdef UNIT_TEST

/* Check the integrity of the data structure.
 */
void m_check(char *descr, int print){
	struct m_hdr *mh = (struct m_hdr *) base;
	unsigned int prev;
	enum m_status last;

	if (print) {
		earth.log.printf("m_check %s: start (init=%d)\n", descr, initialized);
	}
	if (!initialized) {
		return;
	}
	if ((prev = mh->prev) != 0) {
		earth.log.printf("m_check %s: first header has nonzero prev\n", descr);
		return;
	}
	if (mh->status == M_LAST) {
		earth.log.printf("m_check %s: first header cannot be last one\n", descr);
		return;
	}
	for (;;) {
		if ((char *) &mh[1] > GETBRK()) {
			earth.log.printf("m_check %s: beyond break\n", descr);
		}
		if (print) {
			earth.log.printf("-> %p size=%d prev=%d status=%d\n",
					(address_t) mh, mh->size, mh->prev, mh->status);
		}
		switch (mh->status) {
		case M_LAST: case M_FREE: case M_INUSE:
			break;
		default:
			earth.log.printf("m_check %s: bad status\n", descr);
			return;
		}
		if (mh->prev != prev) {
			earth.log.printf("m_check %s: bad prev\n", descr);
			return;
		}
		if (mh->status == M_FREE && last == M_FREE) {
			earth.log.printf("m_check %s: consecutive free blocks\n", descr);
			return;
		}
		if (mh->status == M_LAST) {
			break;
		}
		prev = mh->size;
		mh = (struct m_hdr *) ((char *) &mh[1] + mh->size);
	}
	if ((char *) &mh[1] != GETBRK()) {
		earth.log.printf("m_check %s: last header not at break\n", descr);
	}
	if (print) {
		earth.log.printf("m_check %s: done (init=%d)\n", descr, initialized);
	}
}

int main(void){
	base = valloc(64 * PAGESIZE);
	brk = base + 100 * PAGESIZE;

	printf("hello world %p %p\n", (address_t) base, (address_t) brk);

	void *m0 = m_alloc(16);
	printf("m_alloc(16) --> %p\n", (address_t) m0);
	m_check("m0 = m_alloc(16)", 1);

	void *m1 = m_alloc(16);
	printf("m_alloc(16) --> %p\n", (address_t) m1);
	m_check("m1 = m_alloc(16)", 1);

	void *m2 = m_alloc(50 * PAGESIZE);
	printf("m_alloc(lots) --> %p\n", (address_t) m2);
	m_check("m2 = m_alloc(lots)", 1);

	void *m3 = m_alloc(16);
	printf("m_alloc(16) --> %p\n", (address_t) m3);
	m_check("m3 = m_alloc(16)", 1);

	m_free(m1);
	m_check("m_free(m1)", 1);

	m_free(m3);
	m_check("m_free(m3)", 1);

	m1 = m_alloc(64);
	printf("m_alloc(64) --> %p\n", (address_t) m1);
	m_check("m_alloc(64)", 1);

	return 0;
}

#endif /* UNIT_TEST */
