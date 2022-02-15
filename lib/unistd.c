#include <egos/syscall.h>
#include <egos/dir.h>
#include <egos/memchan.h>
#include <egos/gate.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <utime.h>
#include <assert.h>

#define U_NFDS	100		// max. # open files

struct u_fptr {
	int refcnt;
	int mode;
	FILE *file;
};

struct u_fd {
	bool open;
	struct u_fptr *ufp;
} u_fds[U_NFDS];

extern int errno;

int *__errno_location(void){
	return &errno;
}

static struct u_fptr *u_fptr(FILE *file, int mode){
	struct u_fptr *ufp = calloc(1, sizeof(*ufp));
	ufp->refcnt = 1;
	ufp->mode = mode;
	ufp->file = file;
	return ufp;
}

static void u_init(){
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		if (stdin != NULL) {
			u_fds[0].open = true;
			u_fds[0].ufp = u_fptr(stdin, O_RDONLY);
		}
		if (stdout != NULL) {
			u_fds[1].open = true;
			u_fds[1].ufp = u_fptr(stdout, O_WRONLY);
		}
		if (stderr != NULL) {
			u_fds[2].open = true;
			u_fds[2].ufp = u_fptr(stderr, O_WRONLY);
		}
	}
}

/* Find the first available file descriptor.
 */
static int u_findfd(){
	for (int fd = 0; fd < U_NFDS; fd++) {
		if (!u_fds[fd].open) {
			return fd;
		}
	}
	errno = ENFILE;
	return -1;
}

static int u_err(int dflt){
	if (errno == 0) {
		errno = dflt;
	}
	return -1;
}

static bool endswith(const char *s, const char *suffix){
    size_t len = strlen(s), suflen = strlen(suffix);
    return suflen > len ? false :
			strcmp(s + len - suflen, suffix) == 0;
}

int stat(const char *file, struct stat *st){
	bool is_dir = endswith(file, ".dir");
	fid_t fid;
	bool r = flookup(file, is_dir, 0, &fid);
	if (!r) {
		errno = ENOENT;
		return -1;
	}

	struct file_control_block fcb;
	r = file_stat(fid.server, fid.file_no, &fcb);
	if (!r) {
		errno = EIO;
		return -1;
	}

	st->st_dev = fcb.st_dev;
	st->st_ino = fcb.st_ino;
	st->st_mode = fcb.st_mode;
	st->st_nlink = 1;
	st->st_uid = fcb.st_uid;
	st->st_gid = fcb.st_uid;
	st->st_mtime = fcb.st_modtime;
	st->st_size = fcb.st_size;
	return 0;
}

/* Open the given file.
 */
int open(const char *path, int oflags, ...){
	/* Figure out the mode.
	 */
	char *mode;
	switch (oflags & O_MODE) {
	case O_RDONLY:		mode = "r";		break;
	case O_WRONLY:		mode = "w";		break;
	case O_RDWR:		mode = "w+";	break;
	default:
		errno = EINVAL;
		return -1;
	}

	u_init();
	int fd = u_findfd();
	if (fd < 0) {
		return -1;
	}

	errno = 0;
	FILE *file = fopen(path, mode);
	if (file == NULL) {
		return u_err(ENOENT);
	}

	struct u_fptr *ufp = u_fptr(file, oflags);

	u_fds[fd].open = true;
	u_fds[fd].ufp = ufp;
	return fd;
}

int fstat(int fd, struct stat *st){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;

	/* Get the file info.
	 */
	struct file_control_block fcb;
	if (ffstat(u_fds[fd].ufp->file, &fcb) < 0) {
		return -1;
	}

	st->st_dev = fcb.st_dev;
	st->st_ino = fcb.st_ino;
	st->st_mode = fcb.st_mode;
	st->st_nlink = 1;
	st->st_uid = fcb.st_uid;
	st->st_gid = fcb.st_uid;
	st->st_mtime = fcb.st_modtime;
	st->st_size = fcb.st_size;
	return 0;
}

ssize_t read(int fd, void *buf, size_t n){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	if (n == 0) {		// don't want to trigger EOF flag
		return 0;
	}
	size_t r = fread(buf, 1, n, u_fds[fd].ufp->file);
	if (r > 0) {
		return r;
	}
	if (feof(u_fds[fd].ufp->file)) {
		return 0;
	}
	return u_err(EIO);
}

ssize_t write(int fd, const void *buf, size_t n){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	size_t r = fwrite(buf, 1, n, u_fds[fd].ufp->file);
	if (r > 0) {
		return r;
	}
	return u_err(EIO);
}

off_t lseek(int fd, off_t offset, int whence){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}
	errno = 0;
	int r = fseek(u_fds[fd].ufp->file, offset, whence);
	if (r < 0) {
		return u_err(EIO);
	}
	offset = ftell(u_fds[fd].ufp->file);
	if (offset < 0) {
		return u_err(EIO);
	}
	return offset;
}

int dup(int fd){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}

	int fd2 = u_findfd();
	if (fd < 0) {
		return -1;
	}

	u_fds[fd2] = u_fds[fd];
	u_fds[fd2].ufp->refcnt++;
	return fd2;
}

int dup2(int fd, int fd2){
	u_init();
	if (fd == fd2) {
		return fd2;
	}
	if (u_fds[fd2].open) {
		int r = close(fd2);
		if (r < 0) {
			return u_err(EIO);
		}
	}

	u_fds[fd2] = u_fds[fd];
	u_fds[fd2].ufp->refcnt++;
	return fd2;
}

FILE *fdopen(int fd, const char *mode){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return NULL;
	}
	return u_fds[fd].ufp->file;
}

int close(int fd){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}
	fflush(u_fds[fd].ufp->file);		// to be on the safe side
	if (--u_fds[fd].ufp->refcnt == 0) {
		fclose(u_fds[fd].ufp->file);
		free(u_fds[fd].ufp);
	}
	u_fds[fd].open = false;
	return 0;
}

int unlink(const char *pathname){
	return remove(pathname);
}

static void print_parent(struct mem_chan *mc, fid_t me){
	/* Look up the file identifier of the parent directory.
	 */
	fid_t dir, parent;
	bool r = flookup("..", true, &dir, &parent);
	assert(r);
	if (fid_eq(parent, dir)) {
		return;
	}

	/* Find own entry in the parent directory.
	 */
	FILE *fp = fopen("...dir", "r");
	assert(fp != 0);
	for (;;) {
		struct dir_entry de;
		int sz = fread(&de, sizeof(de), 1, fp);
		if (sz == 0) {
			break;
		}
		assert(sz == 1);
		if (de.fid.server == 0) {
			de.fid.server = dir.server;
		}
		if (fid_eq(de.fid, me)) {
			GRASS_ENV->cwd = parent;
			print_parent(mc, parent);
			unsigned int n = strnlen(de.name, DIR_NAME_SIZE);
			if (n > 4) {			// strip .dir
				de.name[n - 4] = 0;
			}
			mc_printf(mc, "/%s", de.name);
		}
	}
}

char *getcwd(char *buf, size_t size){
	struct mem_chan *mc = mc_alloc();
	fid_t cwd = GRASS_ENV->cwd;

	print_parent(mc, cwd);

	if (mc->offset == 0) {
		mc_putc(mc, '/');
	}
	mc_putc(mc, 0);

	if (mc->offset < size) {
		size = mc->offset;
	}
	strncpy(buf, mc->buf, size);

	mc_free(mc);

	GRASS_ENV->cwd = cwd;
	return buf;
}

int gettimeofday(struct timeval *tv, struct timezone *tz){
	struct gate_time gt;

	bool r = gate_gettime(GRASS_ENV->servers[GPID_GATE], &gt);
	if (!r) {
		errno = EINVAL;
		return -1;
	}
	if (tv != 0) {
		tv->tv_sec = gt.seconds;
		tv->tv_usec = gt.useconds;
	}
	if (tz != 0) {
		tz->tz_minuteswest = gt.minuteswest;
		tz->tz_dsttime = gt.dsttime;
	}
	return 0;
}

time_t time(time_t *tloc){
	struct timeval tv;

	if (gettimeofday(&tv, 0) < 0) {
		return -1;
	}
	if (tloc != 0) {
		*tloc = tv.tv_usec / 1000000;
	}
	return tv.tv_usec / 1000000;
}

int execvp(const char *file, char *const argv[]){
	fprintf(stderr, "execvp: not implemented\n");
	errno = EINVAL;
	return -1;
}

int sigaction(int signum, const struct sigaction *act,
									 struct sigaction *oldact){
	fprintf(stderr, "sigaction: not implemented\n");
	errno = EINVAL;
	return -1;
}

int sigemptyset(sigset_t *set){
	fprintf(stderr, "sigemptyset: not implemented\n");
	errno = EINVAL;
	return -1;
}

int utime(const char *path, const struct utimbuf *times){
	fprintf(stderr, "utime: not implemented\n");
	// errno = EINVAL;
	// return -1;
	return 0;
}

/*
 * Blocks the current process for the requested number of seconds by attempting
 * to receive a MSG_EVENT message with a timeout. Since MSG_EVENT is only used to
 * notify processes of a child process's death, the sys_recv call will hopefully
 * block for its entire timeout.
 *
 * A more robust way to implement sleep() would be to create a new OS process, the
 * "sleep server," that processes can send an RPC to when they want to sleep. The
 * sleep server would keep track of the remaining time for each process's request
 * and only send a reply message to a process when its sleep time is up. This would
 * guarantee that the sending process's sys_rpc call blocks for exactly as long as
 * the desired sleep time.
 */
unsigned int sleep(unsigned int seconds) {
	struct msg_event dummy;
	gpid_t sender_id;
	unsigned int sender_uid;
	sys_recv(MSG_EVENT, seconds * 1000, &dummy, sizeof(dummy), &sender_id, &sender_uid);
	return 0;
}

static int do_chmod(fid_t fid, mode_t mode){
	gmode_t bits = 0;
	if (mode & S_IRUSR) bits |= P_FILE_OWNER_READ;
	if (mode & S_IWUSR) bits |= P_FILE_OWNER_WRITE;
	if (mode & S_IROTH) bits |= P_FILE_OTHER_READ;
	if (mode & S_IWOTH) bits |= P_FILE_OTHER_WRITE;

	bool r = file_chmod(fid.server, fid.file_no, bits);
	if (!r) {
		errno = EIO;
		return -1;
	}
	return 0;
}

int chmod(const char *path, mode_t mode){
	fid_t fid;
	bool r = flookup(path, false, 0, &fid);
	if (!r) {
		errno = ENOENT;
		return -1;
	}
	return do_chmod(fid, mode);
}

int fchmod(int fd, mode_t mode){
	u_init();
	if (!u_fds[fd].open) {
		errno = EINVAL;
		return -1;
	}
	return do_chmod(u_fds[fd].ufp->file->fid, mode);
}

int access(const char *path, int mode){
	if (mode == 0) {
		return 0;
	}

	struct stat st;
	if (stat(path, &st) < 0) {
		return -1;
	}

	// TODO.  Check permission bits.  Requires knowing uid.

	return 0;
}
