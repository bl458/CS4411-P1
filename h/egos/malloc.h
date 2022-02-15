#ifndef _EGOS_MALLOC_H
#define _EGOS_MALLOC_H

#include <sys/types.h>
#include <earth/earth.h>

#define m_alloc			malloc
#define m_calloc		calloc
#define m_free			free
#define m_realloc		realloc

#ifdef TLSF

#define tlsf_malloc		malloc
#define tlsf_calloc		calloc
#define tlsf_free		free
#define tlsf_realloc	realloc

extern void *tlsf_malloc(size_t size);
extern void tlsf_free(void *ptr);
extern void *tlsf_realloc(void *ptr, size_t size);
extern void *tlsf_calloc(size_t nelem, size_t elem_size);

#else // !TLSF

void *m_alloc(size_t size);
void *m_calloc(size_t nitems, size_t size);
void m_free(void *ptr);
void *m_realloc(void *ptr, size_t size);

#endif // TLSF

#endif // _EGOS_MALLOC_H
