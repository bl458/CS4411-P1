#ifndef _EGOS_STDINT_H
#define _EGOS_STDINT_H

/* Signed integers */
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
#ifdef x86_64
typedef long int64_t;
#else
typedef long long int64_t;
#endif

/* Unsigned integers */
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#ifdef x86_64
typedef unsigned long uint64_t;
#else
typedef unsigned long long uint64_t;
#endif

/* Pointers */
#ifdef x86_64
typedef long intptr_t;
typedef unsigned long uintptr_t;
#else
typedef int intptr_t;
typedef unsigned int uintptr_t;
#endif

/* Maximum width types */
#ifdef x86_64
typedef long intmax_t;
typedef unsigned long uintmax_t;
#else
typedef long long intmax_t;
typedef unsigned long long uintmax_t;
#endif 

/* Limits of integral types */
/* (copied from glibc's stdint.h) */

# ifdef x86_64
#  define __INT64_C(c)	c ## L
#  define __UINT64_C(c)	c ## UL
# else
#  define __INT64_C(c)	c ## LL
#  define __UINT64_C(c)	c ## ULL
# endif

/* Minimum of signed integral types.  */
# define INT8_MIN		(-128)
# define INT16_MIN		(-32767-1)
# define INT32_MIN		(-2147483647-1)
# define INT64_MIN		(-__INT64_C(9223372036854775807)-1)
/* Maximum of signed integral types.  */
# define INT8_MAX		(127)
# define INT16_MAX		(32767)
# define INT32_MAX		(2147483647)
# define INT64_MAX		(__INT64_C(9223372036854775807))

/* Maximum of unsigned integral types.  */
# define UINT8_MAX		(255)
# define UINT16_MAX		(65535)
# define UINT32_MAX		(4294967295U)
# define UINT64_MAX		(__UINT64_C(18446744073709551615))

/* Values to test for integral types holding `void *' pointer.  */
# ifdef x86_64
#  define INTPTR_MIN		(-9223372036854775807L-1)
#  define INTPTR_MAX		(9223372036854775807L)
#  define UINTPTR_MAX		(18446744073709551615UL)
# else
#  define INTPTR_MIN		(-2147483647-1)
#  define INTPTR_MAX		(2147483647)
#  define UINTPTR_MAX		(4294967295U)
# endif

/* Minimum for largest signed integral type.  */
# define INTMAX_MIN		(-__INT64_C(9223372036854775807)-1)
/* Maximum for largest signed integral type.  */
# define INTMAX_MAX		(__INT64_C(9223372036854775807))

/* Maximum for largest unsigned integral type.  */
# define UINTMAX_MAX		(__UINT64_C(18446744073709551615))


#endif