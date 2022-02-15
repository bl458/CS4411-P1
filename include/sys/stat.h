#ifndef EGOS_SYS_STAT_H
#define EGOS_SYS_STAT_H

#include <sys/types.h>

struct stat {
	unsigned int	st_dev;    /* device (server in EGOS) */
	unsigned int	st_ino;    /* inode number */
	unsigned int	st_mode;   /* inode protection mode */
	unsigned int	st_nlink;  /* number of hard links to the file */
	unsigned int	st_uid;    /* user-id of owner */
	unsigned int	st_gid;    /* group-id of owner */
	time_t			st_mtime;  /* time of last data modification */
	off_t			st_size;   /* file size, in bytes */
};

#define S_IRWXU 0000700    /* RWX mask for owner */
#define S_IRUSR 0000400    /* R for owner */
#define S_IWUSR 0000200    /* W for owner */
#define S_IXUSR 0000100    /* X for owner */

#define S_IRWXG 0000070    /* RWX mask for group */
#define S_IRGRP 0000040    /* R for group */
#define S_IWGRP 0000020    /* W for group */
#define S_IXGRP 0000010    /* X for group */

#define S_IRWXO 0000007    /* RWX mask for other */
#define S_IROTH 0000004    /* R for other */
#define S_IWOTH 0000002    /* W for other */
#define S_IXOTH 0000001    /* X for other */

#define S_ISUID 0004000    /* set user id on execution */
#define S_ISGID 0002000    /* set group id on execution */
#define S_ISVTX 0001000    /* save swapped text even after use */

#endif // EGOS_SYS_STAT_H
