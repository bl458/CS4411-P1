#ifndef _EGOS_UNISTD_H
#define _EGOS_UNISTD_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <getopt.h>

#define O_MODE		0x3
#define	O_RDONLY	0
#define	O_WRONLY	1
#define	O_RDWR		2

#define O_CREAT		(1 << 8)
#define O_TRUNC		(1 << 9)

#define R_OK		(1 << 0)
#define W_OK		(1 << 1)
#define X_OK		(1 << 2)
#define F_OK		(1 << 3)

int open(const char *path, int oflag, ...);
int fstat(int fildes, struct stat *st);
ssize_t read(int fildes, void *buf, size_t nbyte);
ssize_t write(int fildes, const void *buf, size_t nbyte);
off_t lseek(int fildes, off_t offset, int whence);
int dup(int fildes);
int dup2(int fildes, int fildes2);
int close(int fildes);
int unlink(const char *pathname);
char *getcwd(char *buf, size_t size);
int execvp(const char *file, char *const argv[]);
int stat(const char *file, struct stat *st);
int utime(const char *path, const struct utimbuf *times);
int chmod(const char *path, mode_t mode);
int fchmod(int fildes, mode_t mode);
int access(const char *path, int mode);
unsigned int sleep(unsigned int seconds);

#endif // _EGOS_UNISTD_H
