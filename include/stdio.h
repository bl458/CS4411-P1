#ifndef _EGOS_STDIO_H
#define _EGOS_STDIO_H

#include <sys/types.h>
#include <sys/errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <fcntl.h>

#undef errno

#ifndef NULL
#define NULL		((void *) 0)
#endif
#define	EOF			(-1)

#include <egos/print.h>
#include <egos/file.h>

typedef struct {
	uint64_t offset;
} fpos_t;

/* Flags in FILE
 */
#define _FILE_READ		(1 << 0)	// file is readable
#define _FILE_WRITE		(1 << 1)	// file is writable
#define _FILE_APPEND	(1 << 2)	// all writes go to end
#define _FILE_EOF		(1 << 3)	// input stream got to EOF
#define _FILE_ERROR		(1 << 4)	// stream experienced an error

/* Buffer modes.
 */
#define _IONBF		0	// unbuffered
#define _IOLBF		1	// line buffered
#define _IOFBF		2	// block buffered

#define BUFSIZ		1024

struct FILE {
	fid_t fid;					// file identifier
	uint64_t pos;				// file offset
	unsigned int flags;			// see above
	char *name;					// for debugging purposes

	// Output buffering
	enum { _BUF_NOBUF, _BUF_LINEBUF, _BUF_BLKBUF } mode;
	char *buf;
	char internal[BUFSIZ];
	size_t size, index;

	// Input buffering
	char in_buf[BUFSIZ];
	size_t in_offset, in_size;
};
typedef struct FILE FILE;

extern FILE *stdin, *stdout, *stderr;

FILE *fopen(const char *restrict filename, const char *restrict mode);
FILE *freopen(const char *restrict filename, const char *restrict mode, FILE *restrict stream);
size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
int fflush(FILE *stream);
int fflushsync(FILE *stream);
int fgetpos(FILE *restrict stream, fpos_t *restrict pos);
int fsetpos(FILE *stream, const fpos_t *pos);
long ftell(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
int fclose(FILE *stream);

int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
char *fgets(char *restrict str, int size, FILE *restrict stream);
int fputs(const char *restrict s, FILE *restrict stream);
int feof(FILE *stream);
int ferror(FILE *stream);

int getchar(void);
int putchar(int c);
int putc(int c, FILE *stream);
int puts(const char *s);
int ungetc(int c, FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);
void setbuf(FILE *restrict stream, char *restrict buf);
int setvbuf(FILE *restrict stream, char *restrict buf, int type, size_t size);
void rewind(FILE *stream);
int remove(const char *path);
int rename(const char *old, const char *new);
FILE *tmpfile(void);
FILE *fdopen(int fildes, const char *mode);

void stdio_finit(void);

int fprintf(FILE *restrict stream, const char *fmt, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int fscanf(FILE *restrict stream, const char *restrict format, ...);

int vsscanf(const char *s, const char *fmt, va_list ap);
int sscanf(const char *s, const char *fmt, ...);

void perror(const char *s);
char *strerror(int errnum);

bool flookup(const char *restrict path, bool is_dir, fid_t *p_dir, fid_t *p_fid);
int ffstat(FILE *stream, struct file_control_block *fs);

#endif // _EGOS_STDIO_H
