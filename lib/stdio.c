#include <sys/errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <egos/file.h>
#include <egos/dir.h>
#include <egos/memchan.h>

FILE *stdin, *stdout, *stderr;

static char *errors[] = {
	/* */			"No error",
	/* EPERM */		"Operation not permitted",
	/* ENOENT */	"No such file or directory",
	/* ESRCH */		"No such process",
	/* EINTR */		"Interrupted system call",
	/* EIO */		"I/O error",
	/* ENXIO */		"No such device or address",
	/* E2BIG */		"Argument list too long",
	/* ENOEXEC */	"Exec format error",
	/* EBADF */		"Bad file number",
	/* ECHILD */	"No child processes",
	/* EAGAIN */	"Try again",
	/* ENOMEM */	"Out of memory",
	/* EACCES */	"Permission denied",
	/* EFAULT */	"Bad address",
	/* ENOTBLK */	"Block device required",
	/* EBUSY */		"Device or resource busy",
	/* EEXIST */	"File exists",
	/* EXDEV */		"Cross-device link",
	/* ENODEV */	"No such device",
	/* ENOTDIR */	"Not a directory",
	/* EISDIR */	"Is a directory",
	/* EINVAL */	"Invalid argument",
	/* ENFILE */	"File table overflow",
	/* EMFILE */	"Too many open files",
	/* ENOTTY */	"Not a typewriter",
	/* ETXTBSY */	"Text file busy",
	/* EFBIG */		"File too large",
	/* ENOSPC */	"No space left on device",
	/* ESPIPE */	"Illegal seek",
	/* EROFS */		"Read-only file system",
	/* EMLINK */	"Too many links",
	/* EPIPE */		"Broken pipe",
	/* EDOM */		"Numerical argument out of domain",
	/* ERANGE */	"Numerical result out of range"
};

/* Allocate a new FILE for the given fid.
 */
FILE *falloc(fid_t fid, unsigned int flags, int mode, const char *name){
	FILE *fp = (FILE *) calloc(1, sizeof(FILE));

	fp->fid = fid;
	fp->flags = flags;

	int n = strlen(name);
	fp->name = malloc(n + 1);
	strcpy(fp->name, name);

	fp->mode = mode;
	fp->buf = fp->internal;
	fp->size = BUFSIZ < FILE_MAX_MSG_SIZE ? BUFSIZ : FILE_MAX_MSG_SIZE;
	return fp;
}

/* Lookup the path name and return its directory and file identifier.
 * Returns false if unsuccessful.  However, *p_dir may still be set
 * to the parent directory if only the last component of the file
 * name could not be mapped.
 */
bool flookup(const char *restrict path, bool is_dir,
									fid_t *p_dir, fid_t *p_fid){
	/* See if this is an absolute or relative path.
	 */
	fid_t dir;
	if (*path == '/') {
		dir = GRASS_ENV->root;
		while (*path == '/') {
			path++;
		}
	}
	else {
		dir = GRASS_ENV->cwd;
	}

	/* Now walk the path until the final component.
	 */
	bool success = true;
	fid_t fid = dir;
	while (*path != 0) {
		/* See if there's more.
		 */
		char *e = strchr(path, '/'), *next;
		if (e == 0) {
			if (is_dir) {
				unsigned int n = strlen(path);
				next = malloc(n + 5);
				memcpy(next, path, n);
				strcpy(&next[n], ".dir");
			}
			else {
				next = (char *) path;
			}
		}
		else {
			next = malloc((e - path) + 5);
			memcpy(next, path, e - path);
			strcpy(&next[e - path], ".dir");
		}

		/* Take the next step.
		 */
		success = dir_lookup(GRASS_ENV->servers[GPID_DIR], dir, next, &fid);
		if (e != 0 || is_dir) {
			free(next);
		}
		if (e == 0) {
			if (p_dir != 0) {
				*p_dir = dir;
			}
			if (!success) {
				return false;
			}
			break;
		}

		/* Go on to the remainder of the path.
		 */
		if (!success) {
			return false;
		}
		path = e + 1;
		dir = fid;
	}
	if (p_dir != 0) {
		*p_dir = dir;
	}
	if (p_fid != 0) {
		*p_fid = fid;
	}
	return true;
}

int ffstat(FILE *stream, struct file_control_block *fs){
	return file_stat(stream->fid.server, stream->fid.file_no, fs) ? 0 : -1;
}

/* TODO.  Interpret mode.  Think about create, truncate, and append, and
 *		  set the position correctly in the FILE structure.
 */
FILE *fopen(const char *restrict path, const char *restrict mode){
	unsigned int flags = 0;
	bool create = false, truncate = false;

	switch (mode[0]) {
	case 'r':
		flags |= _FILE_READ;
		if (mode[1] == '+') {
			flags |= _FILE_WRITE;
		}
		break;
	case 'w':
		flags |= _FILE_WRITE;
		create = true;
		if (mode[1] == '+') {
			flags |= _FILE_READ;
		}
		else {
			truncate = true;
		}
		break;
	case 'a':
		flags |= _FILE_WRITE | _FILE_APPEND;
		create = true;
		if (mode[1] == '+') {
			flags |= _FILE_READ;
		}
		break;
	default:
		fprintf(stderr, "fopen: don't understand mode\n");
		return 0;
	}

	/* See if the file exists.  If not and the file isn't opened just for
	 * reading, try to create it.
	 */
	fid_t dir, fid;
	dir = fid_val(0, 0);
	bool success = flookup(path, false, &dir, &fid);
	if (!success) {
		if (!create || dir.server == 0) {
			return 0;
		}

		/* Find the last component in path.
		 */
		const char *last = rindex(path, '/');
		if (last == 0) {
			last = path;
		}
		else if (*++last == 0) {
			return 0;
		}

		/* Use the parent directory's file server as the file server.
		 */
		fid.server = dir.server;
		if (!file_create(dir.server, P_FILE_DEFAULT, &fid.file_no)) {
			return 0;
		}

		/* Insert into the directory.
		 */
		if (!dir_insert(GRASS_ENV->servers[GPID_DIR], dir, last, fid)) {
			return 0;
		}
	}
	if (truncate) {
		file_setsize(fid.server, fid.file_no, 0);
	}
	return falloc(fid, flags, _IOFBF, path);
}

FILE *freopen(const char *restrict filename, const char *restrict mode, FILE *restrict stream){
	fprintf(stderr, "freopen: not yet implemented\n");
	return 0;
}

size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream){
	fflush(stream);
/*
	if (strcmp(stream->name, "<stdin>") != 0) {
		printf("fread '%s' %d %d %d\n", stream->name, (int) stream->pos, (int) size, (int) nitems);
	}
*/

	assert(size > 0);
	assert(stream->in_offset <= stream->in_size);

	if (stream->flags & _FILE_ERROR) {
		return 0;
	}
	if (nitems == 0) {		// don't want to set the EOF flag
		return 0;
	}

	bool short_read = false;
	for (size_t total = 0;;) {
// printf("fread loop '%s' pos=%d sz=%d ni=%d of=%d bf=%d sr=%d fl=%x\n", stream->name, (int) stream->pos, (int) size, (int) nitems, (int) stream->in_offset, (int) stream->in_size, short_read, stream->flags);
		/* See if anything is buffered and can be returned right away.
		 */
		size_t buffered = stream->in_size - stream->in_offset;
		if (buffered >= size) {
			size_t ni = buffered / size;		// # items
			if (ni > nitems) {
				ni = nitems;
			}
			size_t n = ni * size;
			memcpy(ptr, &stream->in_buf[stream->in_offset], n);
			stream->in_offset += n;
			if (stream->in_offset == stream->in_size) {
				stream->in_offset = stream->in_size = 0;
			}
			stream->pos += n;
			ptr = (char *) ptr + n;
			nitems -= ni;
			total += ni;
		}

		/* Perhaps we're done.
		 */
		if (nitems == 0 || short_read || (stream->flags & _FILE_EOF)) {
			return total;
		}

		/* Move what's in the buffer to the beginning.
		 */
		if (buffered > 0 && stream->in_offset > 0) {
			memmove(stream->in_buf, &stream->in_buf[stream->in_offset],
															buffered);
			stream->in_offset = 0;
			stream->in_size = buffered;
		}
		assert(stream->in_offset == 0);

		/* Fill the buffer.
		 */
		unsigned int to_read = sizeof(stream->in_buf) - stream->in_size;
		bool r = file_read(stream->fid.server, stream->fid.file_no,
								stream->pos + stream->in_size,
								&stream->in_buf[stream->in_size], &to_read);
		if (!r) {
			printf("fread: file_read returned an error\n");
			stream->flags |= _FILE_ERROR;
			return 0;
		}
		if (to_read == 0) {
			stream->flags |= _FILE_EOF;
		}
		if (to_read < sizeof(stream->in_buf) - stream->in_size) {
			short_read = true;
		}
		stream->in_size += to_read;
	}
}

size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream){
	// printf("fwrite '%s' %d %d %d\n", stream->name, (int) stream->pos, (int) size, (int) nitems);

	if (stream->flags & _FILE_ERROR) {
		return 0;
	}

	const char *p = ptr;
	size_t n = size * nitems;

	while (n > 0) {
		// First copy as much as possible into the buffer
		assert(stream->index < stream->size);
		size_t chunk = n;
		if (chunk > stream->size - stream->index) {
			chunk = stream->size - stream->index;
		}
		assert(chunk <= FILE_MAX_MSG_SIZE);
		memcpy(&stream->buf[stream->index], p, chunk);
		stream->index += chunk;
		p += chunk;
		n -= chunk;

		// See if we should keep this buffered
		if (n == 0 && stream->index < stream->size) {
			switch (stream->mode) {
			case _IONBF:
				break;
			case _IOLBF:
				if (memchr(stream->buf, '\n', stream->size) == 0) {
					return nitems;
				}
				break;
			case _IOFBF:
				return nitems;
			default:
				assert(0);
			}
		}
		if (fflush(stream) != 0) {
			fprintf(stderr, "fwrite: fflush failed\n");
			return 0;
		}
	}
	return nitems;
}

int fclose(FILE *stream){
	fflush(stream);
	free(stream->name);
	free(stream);
	return 0;
}

int fgetc(FILE *stream){
	char c;
	int n = fread(&c, 1, 1, stream);

	if (n < 0) {
		fprintf(stderr, "fgetc: error on input\n");
		return EOF;
	}
	if (n == 0) {
		return EOF;
	}
	return c & 0xFF;
}

char *fgets(char *restrict str, int size, FILE *restrict stream){
	int c, i;

	for (i = 0; i < size - 1;) {
		if ((c = fgetc(stream)) == EOF) {
			if (i == 0) {
				return 0;
			}
			break;
		}
		str[i++] = c;
		if (c == '\n') {
			break;
		}
	}
	str[i] = 0;
	return str;
}

int fputs(const char *restrict s, FILE *restrict stream){
	int n = strlen(s);

	return fwrite(s, 1, n, stream);
}

int fputc(int c, FILE *stream){
	char x = c;
	return fwrite(&x, 1, 1, stream) == 1 ? c : EOF;
}

int fprintf(FILE *restrict stream, const char *fmt, ...){
	va_list ap;
	struct mem_chan *mc = mc_alloc();

	va_start(ap, fmt);
	mc_vprintf(mc, fmt, ap);
	va_end(ap);

	int size = fwrite(mc->buf, 1, mc->offset, stream);

	mc_free(mc);
	return size;
}

#ifdef notdef
int fgetpos(FILE *restrict stream, fpos_t *restrict pos){
	fflush(stream);
	pos->offset = stream->pos;
	return 0;
}

int fsetpos(FILE *stream, const fpos_t *pos){
	return fseek(stream, (long) *pos, SEEK_SET);
}
#endif

int fseek(FILE *stream, long offset, int whence){
	// printf("fseek '%s' %d %d %d\n", stream->name, (int) stream->pos, (int) offset, whence);

	switch (whence) {
	case SEEK_SET:
		if ((unsigned long) offset != stream->pos) {
			fflush(stream);								// flush output buffer
			stream->in_offset = stream->in_size = 0;	// toss input buffer
			stream->pos = offset;
		}
		stream->flags &= ~_FILE_EOF;
		return 0;
	case SEEK_CUR:
		if (offset != 0) {
			fflush(stream);								// flush output buffer
			stream->in_offset = stream->in_size = 0;	// toss input buffer
			stream->pos += offset;
		}
		stream->flags &= ~_FILE_EOF;
		return 0;
	case SEEK_END:
		{
			struct file_control_block stat;

			if (!file_stat(stream->fid.server, stream->fid.file_no, &stat)) {
				fprintf(stderr, "fseek: can't stat\n");
				return -1;
			}
			fflush(stream);								// flush output buffer
			stream->in_offset = stream->in_size = 0;	// toss input buffer
			stream->pos = stat.st_size + offset;
			stream->flags &= ~_FILE_EOF;
		}
		return 0;
	default:
		fprintf(stderr, "fseek: bad offset\n");
		fflush(stream);								// flush output buffer
		stream->in_offset = stream->in_size = 0;	// toss input buffer
		stream->flags |= _FILE_ERROR;
		return -1;
	}
}

long ftell(FILE *stream){
	fflush(stream);
	return (long) stream->pos;
}

int fflush(FILE *stream){
	if (stream->index > 0) {
		bool r = file_write(stream->fid.server, stream->fid.file_no,
	// TODO			(stream->flags & _FILE_APPEND) ? _FILE_APPEND_POS : stream->pos,
				stream->pos, stream->buf, stream->index);
		if (!r) {
			printf("fflush: file_write failed\n");
			stream->flags |= _FILE_ERROR;
			return EOF;
		}
		stream->pos += stream->index;
		stream->index = 0;
	}
	return 0;
}

int fflushsync(FILE *stream){
	if (stream == NULL) {
		return file_sync(GRASS_ENV->servers[GPID_FILE], (unsigned int) -1);
	}
	fflush(stream);
	if (stream->flags & _FILE_ERROR) {
		return 0;
	}
	bool r = file_sync(stream->fid.server, stream->fid.file_no);
	if (!r) {
		printf("fflushsync: file_sync fid.ino %d returned an error\n", stream->fid.file_no);
		stream->flags |= _FILE_ERROR;
	}
	return 0;
}

int feof(FILE *stream){
	return stream->flags & _FILE_EOF;
}

int ferror(FILE *stream){
	return stream->flags & _FILE_ERROR;
}

int getchar(void){
	return fgetc(stdin);
}

int putc(int c, FILE *stream){
	return fputc(c, stream);
}

int puts(const char *s){
	if (fputs(s, stdout) >= 0) {
		return putchar('\n');
	}
	return EOF;
}

int putchar(int c){
	return fputc(c, stdout);
}

int ungetc(int c, FILE *stream){
	fprintf(stderr, "ungetc: not yet implemented\n");
	return EOF;
}

int fileno(FILE *stream){
	fprintf(stderr, "fileno: not implemented\n");
	return -1;
}

void clearerr(FILE *stream){
	stream->flags &= ~(_FILE_EOF | _FILE_ERROR);
}

int setvbuf(FILE *restrict stream, char *restrict buf, int mode, size_t size){
	stream->mode = mode;
	if (mode == _IONBF || buf == 0 || size < BUFSIZ) {
		stream->buf = stream->internal;
		stream->size = BUFSIZ;
	}
	else {
		stream->buf = buf;
		stream->size = size;
	}
	if (stream->size > FILE_MAX_MSG_SIZE) {
		stream->size = FILE_MAX_MSG_SIZE;
	}
	stream->index = 0;
	return 0;
}

void setbuf(FILE *restrict stream, char *restrict buf){
	setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

char *strerror(int errnum){
	if ((unsigned long) errnum < sizeof(errors) / sizeof(errors[0])) {
		return errors[errnum];
	}

	static char buf[64];
	sprintf(buf, "Unknown error: %d", errnum);
	return buf;
}

void perror(const char *s){
	if (s != 0 && *s != 0) {
		fprintf(stderr, "%s: ", s);
	}
	fprintf(stderr, "%s\n", strerror(errno));
}

void rewind(FILE *stream){
	fseek(stream, 0, SEEK_SET);
}

int vfprintf(FILE *stream, const char *fmt, va_list ap){
	struct mem_chan *mc = mc_alloc();

	mc_vprintf(mc, fmt, ap);

	int size = fwrite(mc->buf, 1, mc->offset, stream);

	mc_free(mc);
	return size;
}

int remove(const char *path){
	/* See if the file exists.
	 *
	 * TODO.  Check if it's a directory.
	 */
	fid_t dir, fid;
	dir = fid_val(0, 0);
	bool success = flookup(path, false, &dir, &fid);
	if (!success) {
		return -1;
	}

	/* Delete the file.
	 */
	(void) file_delete(fid.server, fid.file_no);

	/* Remove the directory entry.
	 */
	const char *last = rindex(path, '/');
	if (last == 0) {
		last = path;
	}
	else {
		last++;
	}
	success = dir_remove(GRASS_ENV->servers[GPID_DIR], dir, last, fid);
	return success ? 0 : -1;
}

int rename(const char *old, const char *new){
	fprintf(stderr, "rename: not yet implemented\n");
	return -1;
}
 
FILE *tmpfile(void){
	fprintf(stderr, "tmpfile: not yet implemented\n");
	return 0;
}


void stdio_finit(void){
	stdin = falloc(GRASS_ENV->stdin, _FILE_READ, _IOFBF, "<stdin>");
	stdout = falloc(GRASS_ENV->stdout, _FILE_WRITE | _FILE_APPEND, _IOLBF, "<stdout>");
	stderr = falloc(GRASS_ENV->stderr, _FILE_WRITE | _FILE_APPEND, _IONBF, "<stderr>");
}

#ifdef notdef

int stat(const char *restrict path, struct file_control_block *restrict buf){
	fid_t fid;
	uint64_t size;

	if (grass_map(path, &fid) < 0) {
		fprintf(stderr, "stat: unknown file '%s'\n", path);
		return -1;
	}
	if (grass_getsize(fid, &size) < 0) {
		fprintf(stderr, "stat: can't get size of '%s'\n", path);
		return -1;
	}
	buf->st_dev = fid.fh_pid;
	buf->st_ino = fid.fh_ino;
	buf->st_size = size;
	return 0;
}

#endif
