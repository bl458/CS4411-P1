#ifndef _EGOS_INTTYPES_H
#define _EGOS_INTTYPES_H

#include <stdint.h>

#ifdef x86_64
#  define __PRI64_PREFIX	"l"
#  define __PRIPTR_PREFIX	"l"
#else 
#  define __PRI64_PREFIX	"ll"
#  define __PRIPTR_PREFIX
#endif 

/* Macros for printing format specifiers.  */
/* Mostly copied from glibc's inttypes.h, except for the "fast"
 * and "least" types because we don't bother supporting them. */

/* Decimal notation.  */
# define PRId8		"d"
# define PRId16		"d"
# define PRId32		"d"
# define PRId64		__PRI64_PREFIX "d"

# define PRIi8		"i"
# define PRIi16		"i"
# define PRIi32		"i"
# define PRIi64		__PRI64_PREFIX "i"

/* Octal notation.  */
# define PRIo8		"o"
# define PRIo16		"o"
# define PRIo32		"o"
# define PRIo64		__PRI64_PREFIX "o"

/* Unsigned integers.  */
# define PRIu8		"u"
# define PRIu16		"u"
# define PRIu32		"u"
# define PRIu64		__PRI64_PREFIX "u"

/* lowercase hexadecimal notation.  */
# define PRIx8		"x"
# define PRIx16		"x"
# define PRIx32		"x"
# define PRIx64		__PRI64_PREFIX "x"

/* UPPERCASE hexadecimal notation.  */
# define PRIX8		"X"
# define PRIX16		"X"
# define PRIX32		"X"
# define PRIX64		__PRI64_PREFIX "X"

/* Macros for printing `intmax_t' and `uintmax_t'.  */
# define PRIdMAX	__PRI64_PREFIX "d"
# define PRIiMAX	__PRI64_PREFIX "i"
# define PRIoMAX	__PRI64_PREFIX "o"
# define PRIuMAX	__PRI64_PREFIX "u"
# define PRIxMAX	__PRI64_PREFIX "x"
# define PRIXMAX	__PRI64_PREFIX "X"


/* Macros for printing `intptr_t' and `uintptr_t'.  */
# define PRIdPTR	__PRIPTR_PREFIX "d"
# define PRIiPTR	__PRIPTR_PREFIX "i"
# define PRIoPTR	__PRIPTR_PREFIX "o"
# define PRIuPTR	__PRIPTR_PREFIX "u"
# define PRIxPTR	__PRIPTR_PREFIX "x"
# define PRIXPTR	__PRIPTR_PREFIX "X"

/* Macros for scanning format specifiers.  */

/* Signed decimal notation.  */
# define SCNd8		"hhd"
# define SCNd16		"hd"
# define SCNd32		"d"
# define SCNd64		__PRI64_PREFIX "d"

/* Signed decimal notation.  */
# define SCNi8		"hhi"
# define SCNi16		"hi"
# define SCNi32		"i"
# define SCNi64		__PRI64_PREFIX "i"

/* Unsigned decimal notation.  */
# define SCNu8		"hhu"
# define SCNu16		"hu"
# define SCNu32		"u"
# define SCNu64		__PRI64_PREFIX "u"

/* Octal notation.  */
# define SCNo8		"hho"
# define SCNo16		"ho"
# define SCNo32		"o"
# define SCNo64		__PRI64_PREFIX "o"

/* Hexadecimal notation.  */
# define SCNx8		"hhx"
# define SCNx16		"hx"
# define SCNx32		"x"
# define SCNx64		__PRI64_PREFIX "x"

/* Macros for scanning `intmax_t' and `uintmax_t'.  */
# define SCNdMAX	__PRI64_PREFIX "d"
# define SCNiMAX	__PRI64_PREFIX "i"
# define SCNoMAX	__PRI64_PREFIX "o"
# define SCNuMAX	__PRI64_PREFIX "u"
# define SCNxMAX	__PRI64_PREFIX "x"

/* Macros for scaning `intptr_t' and `uintptr_t'.  */
# define SCNdPTR	__PRIPTR_PREFIX "d"
# define SCNiPTR	__PRIPTR_PREFIX "i"
# define SCNoPTR	__PRIPTR_PREFIX "o"
# define SCNuPTR	__PRIPTR_PREFIX "u"
# define SCNxPTR	__PRIPTR_PREFIX "x"

#endif