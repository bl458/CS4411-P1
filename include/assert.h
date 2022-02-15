#ifndef _EGOS_ASSERT_H
#define _EGOS_ASSERT_H

#include <stdio.h>

#ifdef NDEBUG
#define assert(expr)	do while (0)
#else
// #define assert(expr)	do if (!(expr)) { fprintf(stderr, "assertion \"%s\" failed: file \"%s\", line %d\n", #expr, __FILE__, __LINE__); sys_exit(-4); } while (0)
#define assert(expr)	do if (!(expr)) { printf("assertion \"%s\" failed: file \"%s\", line %d\n", #expr, __FILE__, __LINE__); sys_exit(-4); } while (0)
#endif // NDEBUG

#endif // _EGOS_ASSERT_H
