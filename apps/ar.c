/*
 * @file 	myar.c
 * @author 	Rebecca Sagalyn
 */

#include <sys/types.h>
// #include <sys/stat.h>
#include <ar.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#define SARFMAG 2
#define BLKSIZE 4096
#define SARHDR 60   		/* size of ar_hdr */
#define SARFNAM 16
#define SARFDAT 12
#define SARFUID 6
#define SARFGID 6
#define SARFMOD 8
#define SARFSIZ 10
#define SARFMAG 2
#define TMPARCH "tempfile.a"
#define FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) /* default permissions */

#define EXIT_FAILURE 1

/* Quickly append named files to end of archive, and create archive if doesn't exist  */
void 	append(int fd, char**ar_list, int n);

/* Quickly append all regular files in current directory */
void	append_all(int fd, char*ar_name);

/* Delete names files from archive */
void 	delete(int *fd, char*arname, char**filename, int n);

void 	extract(int fd, char *filename);

void 	extract_all(int fd);

/* Print a verbose table of contents of the archive. */
void 	print_verbose(int fd);


void 	print_concise(int fd);

/* Return an ls-style string of permissions mask */
void 	ar_mode_string(mode_t mode, char* mode_string);

/* Returns fd to opened ar file if success
 * Returns -1 if not archive file, or can't open file/filedoesnt exist
 */
int 	open_ar_file(char *path, char key);

/* Prints a shell-style usage message when user makes invalid entry */
void usage();

/* Checks to see if specified file is in list of filenames */
int in_file_list(char *fname, char**list, int size);


void delete(int *fd_old, char*arname, char**file_list, int n) {
	int i;
	struct stat st;
	struct ar_hdr ar_head;
	char *pEnd;
	char buf[BLKSIZE];
	char member_name[SARFNAM];
	size_t mem_size;
	off_t pos_old_fd;
	size_t num_written;
	size_t to_write;
	int nread;
	int nout;
	int fd_new;
	int fd = *fd_old;


	// Stat old ar file to get permissions
	if (fstat(fd, &st) == -1) {
		printf("Error reading archive file.\n");
		exit(EXIT_FAILURE);
	}
	// unlink old file
	unlink(arname);

	// Create new archive file
	if ((fd_new = open(arname, O_RDWR | O_CREAT | O_TRUNC, st.st_mode)) == -1) {
		printf("Error creating archive file.\n");
		exit(EXIT_FAILURE);
	}

	// Create global header
	lseek(fd_new, 0, SEEK_SET);
	if (write(fd_new, ARMAG, SARMAG) == -1) {
		printf("Unable to write global header\n");
		exit(EXIT_FAILURE);
	}
	lseek(fd_new, SARMAG, SEEK_SET);

	// start reading at beginning of first file header
	pos_old_fd = lseek(fd, SARMAG, SEEK_SET);
	while (pos_old_fd < st.st_size) {
		// read file header into ar_head
		if (read(fd, &ar_head, SARHDR) == -1) {
			printf("Error reading file header\n");
			exit(EXIT_FAILURE);
		}

		// Make sure header is valid
		if (strncmp(ar_head.ar_fmag, ARFMAG, SARFMAG) != 0) {
			printf("Error in file header\n");
			// if bad file header, exit
			exit(EXIT_FAILURE);
		}
		// convert size to numeric
		mem_size = strtol(ar_head.ar_size, &pEnd, SARFSIZ);

		// store member filename from header without terminal '/'
		if ((ar_head.ar_name[1] != ' ') && (ar_head.ar_name[1] != '/')) {
			i = 0;
		memset(member_name, '\0', SARFNAM);
		while (ar_head.ar_name[i] != '/') {
			member_name[i] = ar_head.ar_name[i];
			i++;
		}
		}
		pos_old_fd = lseek(fd, 0, SEEK_CUR);

		// if filename not in arguemnt filename array, copy it
		if (!(in_file_list(member_name, file_list, n))) {

			// Move to even byte in new archive file
			if (lseek(fd_new, 0, SEEK_CUR) % 2 == 1) {
				write(fd_new, "\n", sizeof(char));
			}

			// Copy file header to new archive file
			if ((write(fd_new, &ar_head, SARHDR)) == -1) {
				printf("Error writing file header\n");
				exit(EXIT_FAILURE);
			}
			pos_old_fd = lseek(fd, 0, SEEK_CUR);
			num_written = 0;
			while (num_written < mem_size) {
				to_write =
						(BLKSIZE > (mem_size - num_written)) ?
								mem_size - num_written : BLKSIZE;
				// Read in block from old archive file
				if ((nread = read(fd, buf, to_write)) == -1) {
					printf("Error reading from archive file\n");
					exit(EXIT_FAILURE);
				}
				if ((nout = write(fd_new, buf, to_write)) == -1) {
					printf("Error writing to archive file\n");
					exit(EXIT_FAILURE);
				}
				num_written += nout;
			}

			pos_old_fd = lseek(fd, 0, SEEK_CUR);
		}

		// else, skip it
		else {
			lseek(fd, mem_size, SEEK_CUR);
		}
		pos_old_fd = lseek(fd, 0, SEEK_CUR);
		// Move to even byte in new archive file
		if ((lseek(fd, 0, SEEK_CUR) % 2) == 1) {
			lseek(fd, 0, SEEK_CUR);
		}
		pos_old_fd = lseek(fd, 0, SEEK_CUR);
	}

	// Update fd to point to new file
	*fd_old = fd_new;
}


void extract_all(int fd) {
	int i;
	int mem_fd;
	int num_read;
	struct utimbuf time_buf;
	struct ar_hdr ar_head;
	char buf[BLKSIZE];
	char arf_name[64];
	char *pEnd;
	char tmp[32];
	size_t ext_size;
	size_t to_write;
	mode_t mode;
	time_t scnds;
	uid_t uid;
	gid_t gid;
	off_t ext_pos;
	off_t ar_size = lseek(fd, 0, SEEK_END);
	off_t ar_pos = lseek(fd, SARMAG, SEEK_SET);

	// start at beginning of first file header
	while ((long)ar_pos < (long)ar_size) {
		// read header into ar_head
		if (read(fd, &ar_head, sizeof(struct ar_hdr)) != sizeof(struct ar_hdr))
			exit(EXIT_FAILURE);

		ar_pos = lseek(fd, 0, SEEK_CUR);

		// Make sure header is complete
		if (strncmp(ar_head.ar_fmag, ARFMAG, SARFMAG) != 0) {
			printf("Error reading member header.");
			// if not, corrupted file, exit
			exit(EXIT_FAILURE);
		}
		// convert size to numeric
		ext_size = strtol(ar_head.ar_size, &pEnd, SARFSIZ);

		// ORIG: if ((ar_head.ar_name[1] != ' ') && (ar_head.ar_name[1] != '/')) {
		if ((ar_head.ar_name[0] != ' ') && (ar_head.ar_name[0] != '/')) {
			// store filename from header without ending "/"
			i = 0;
			memset(arf_name, '\0', SARFNAM);
			while (ar_head.ar_name[i] != '/') {
				arf_name[i] = ar_head.ar_name[i];
				i++;
			}
			// create empty file
			mem_fd = open(arf_name, O_RDWR | O_TRUNC | O_CREAT, FILE_PERMS);
			if (mem_fd == -1) {
				exit(EXIT_FAILURE);
			}
			//Read from archive file, write to member file
			ext_pos = lseek(mem_fd, 0, SEEK_SET); // set position in new member file

			while ((long)ext_pos < (long)ext_size) {
				if (BLKSIZE > (ext_size - ext_pos)) // need to read portion of buffer
					to_write = ext_size - ext_pos;
				else
					to_write = BLKSIZE;

				if ((num_read = read(fd, buf, to_write)) == -1) {
					printf("Error reading archive file\n");
					exit(EXIT_FAILURE);
				}
				if (write(mem_fd, buf, to_write) == -1) {
					printf("Error writing to new file\n");
					exit(EXIT_FAILURE);
				}
				ext_pos = lseek(mem_fd, 0, SEEK_CUR); // update position
			}
			ar_pos = lseek(fd, 0, SEEK_CUR);

			// get user ID and convert to numeric
			memset(tmp, '\0', sizeof(tmp)); // clear buffer
			memcpy(tmp, ar_head.ar_uid, SARFUID);
			tmp[SARFUID] = '\0';
			uid = atoi(tmp);

			// get group ID and convert to numeric
			memcpy(tmp, ar_head.ar_gid, SARFGID);
			tmp[SARFGID] = '\0';
			gid = atoi(tmp);

			// get and convert mode to numeric
			memset(tmp, '\0', sizeof(tmp)); // clear buffer
			memcpy(tmp, ar_head.ar_mode, SARFMOD);
			mode = strtol(tmp, &pEnd, SARFMOD);

			// get decimal seconds since Epoch
			memset(tmp, '\0', sizeof(tmp));
			memcpy(tmp, ar_head.ar_date, SARFDAT);
			scnds = atoi(tmp);

			// close open fd
			if (close(mem_fd) == -1) {
				printf("Error closing open file descriptor\n");
				exit(EXIT_FAILURE);
			}

			// set file permissions on file
			if (chmod(arf_name, mode) == -1) {
				printf("Error: could not set file permissions for file '%s'\n", arf_name);
				exit(-1);
			}

			// add timestamps to file
			time_buf.modtime = scnds;
			time_buf.actime = scnds;
			if (utime(arf_name, &time_buf) == -1) {
				printf("Error: could not set file timestamps\n");
				exit(-1);
			}

			// if size is odd, move forward 1 more
			ar_pos = lseek(fd, (ext_size % 2), SEEK_CUR);

		} else
			// add size to position; if size is odd, move forward 1 more
			ar_pos = lseek(fd, (ext_size + (ext_size % 2)), SEEK_CUR);
	}
}



void extract(int fd, char *filename) {
	if (filename == NULL) { // extract all
		extract_all(fd);
	} else {
		int found = 0;
		int i;
		int mem_fd;
		int num_read;
		//struct stat st;
		struct ar_hdr ar_head;
		char *pEnd;
		char buf[BLKSIZE];
		char arf_name[SARFNAM];
		size_t member_size;
		mode_t mode;
		uid_t uid;
		gid_t gid;
		time_t scnds;
		char tmp[16];
		struct utimbuf time_buf;
		off_t ext_pos;
		size_t to_write;

		// start at beginning of first file header
		lseek(fd, SARMAG, SEEK_SET);
		while (!found) {
			// read header into ar_head
			if (read(fd, &ar_head, sizeof(struct ar_hdr)) == -1)
				exit(EXIT_FAILURE);

			// Make sure header is complete
			if (strncmp(ar_head.ar_fmag, ARFMAG, SARFMAG) != 0) {
				printf("Error reading member header.");
				// if not, corrupted file, exit
				exit(EXIT_FAILURE);
			}
			// convert size to numeric
			member_size = strtol(ar_head.ar_size, &pEnd, SARFSIZ);

			// store filename from header without ending "/"
			i = 0;
			memset(arf_name, '\0', SARFNAM);
			while (ar_head.ar_name[i] != '/') {
				arf_name[i] = ar_head.ar_name[i];
				i++;
			}

			// compare filenames
			if (memcmp(arf_name, filename, strlen(filename)) == 0) {
				// file found, jump to extraction
				found = 1;
				break;
			} // else continue searching

			// add size to position; if size is odd, move forward 1 more
			lseek(fd, (member_size + 1) & (~1), SEEK_CUR);
		}

		if (found == 1) { // extract it
			// create empty file
			mem_fd = open(filename, O_RDWR | O_TRUNC | O_CREAT, FILE_PERMS);
			if (mem_fd == -1) {
				exit(EXIT_FAILURE);
			}

			//Read from archive file, write to member file
			ext_pos = lseek(mem_fd, 0, SEEK_SET); // set position in new member file
			while ((long)ext_pos < (long)member_size) {
				if (BLKSIZE > (member_size - ext_pos)) // need to read portion of buffer
					to_write = member_size - ext_pos;
				else
					to_write = BLKSIZE;
				if ((num_read = read(fd, buf, to_write)) == -1) {
					printf("Error reading archive file\n");
					exit(EXIT_FAILURE);
				}
				if (write(mem_fd, buf, to_write) == -1) {
					printf("Error writing to new file\n");
					exit(EXIT_FAILURE);
				}
				ext_pos = lseek(mem_fd, 0, SEEK_CUR); // update position
			}
			// get user and group IDs and convert to numeric
			memset(tmp, '\0', sizeof(tmp)); // clear buffer
			memcpy(tmp, ar_head.ar_uid, SARFUID);
			tmp[SARFUID] = '\0';
			uid = atoi(tmp);
			memcpy(tmp, ar_head.ar_gid, SARFGID);
			tmp[SARFGID] = '\0';
			gid = atoi(tmp);

			// get and convert mode to numeric
			memset(tmp, '\0', sizeof(tmp)); // clear buffer
			memcpy(tmp, ar_head.ar_mode, SARFMOD);
			//mode = strtol(ar_head.ar_mode, &pEnd, SARFMOD);
			mode = strtol(tmp, &pEnd, SARFMOD);
			if (fchmod(mem_fd, mode) == -1) {
				printf("Error: could not set file permissions %d\n", mem_fd);
				exit(-1);
			}

			// get decimal seconds since Epoch
			memset(tmp, '\0', sizeof(tmp));
			memcpy(tmp, ar_head.ar_date, SARFDAT);
			scnds = atoi(tmp);

			// add timestamps to file
			time_buf.actime = time_buf.modtime = scnds;
			if (utime(filename, &time_buf) == -1) {
				close(mem_fd);
				exit(-1);
			}
			if (close(mem_fd) == -1)
				exit(EXIT_FAILURE);
		} else { // file wasn't found
			printf("no entry %s in archive\n", filename);
			exit(EXIT_FAILURE);
		}
	}
}



void append(int fd, char**ar_list, int n){
	struct stat st;
	struct ar_hdr ar_head;
	int mem_fd;
	char buf[BLKSIZE];
	char arf_name[SARFNAM+1];
	char arf_date[SARFDAT+1];
	char arf_uid[SARFUID+1];
	char arf_gid[SARFGID+1];
	char arf_mode[SARFMOD+1];
	char arf_size[SARFSIZ+1];
	int i;
	off_t mem_pos;
	size_t to_write;

	// Loop through list of files to append
	for (i=0; i<n; i++){

		// Does member file exist/is it accessible?
		if (access(ar_list[i], F_OK) == -1) {
			printf("ar: %s: No such file or directory", ar_list[i]);
			exit(EXIT_FAILURE);
		}

		// Is a named file or a sym link?
		if (stat(ar_list[i], &st) == -1) {
			printf("Can't stat file\n");
			exit(EXIT_FAILURE);
		}

		mem_fd = open(ar_list[i], O_RDONLY);
		if (mem_fd == -1) {
			printf("Cannot open file\n");
			exit(EXIT_FAILURE);
		}
		// Fill header with spaces
		char *member_name = basename(ar_list[i]);
		memset(&ar_head, ' ', SARHDR);

		// Filename
		memset(arf_name, ' ', SARFNAM + 1);
		memcpy(arf_name, member_name, strlen(member_name));
		arf_name[strlen(member_name)]='/';
		memcpy(ar_head.ar_name, arf_name, SARFNAM);

		// Convert st_mtime to string and copy to header
		sprintf(arf_date, "%-12lu", st.st_mtime);
		memcpy(ar_head.ar_date, arf_date, SARFDAT);

		// Convert uid and gid to string and copy to header
		sprintf(arf_uid, "%-6d", st.st_uid);
		memcpy(ar_head.ar_uid, arf_uid, SARFUID);
		sprintf(arf_gid, "%-6d", st.st_gid);
		memcpy(ar_head.ar_gid, arf_gid, SARFGID);

		sprintf(arf_mode, "%-8o", st.st_mode);				// mode
		memcpy(ar_head.ar_mode, arf_mode, SARFMOD);

		sprintf(arf_size, "%-10d", (int) st.st_size);		// size
		memcpy(ar_head.ar_size, arf_size, SARFSIZ);

		memcpy(ar_head.ar_fmag, ARFMAG, SARFMAG);			// fmag

		// seek to end of ar file to prepare to append new file header
		if (lseek(fd, 0, SEEK_END) == -1) {
			exit(EXIT_FAILURE);
		}

		// if odd byte, write new line
		if (lseek(fd, 0, SEEK_CUR) % 2 != 0)
			if (write(fd, "\n", 1)== -1) {
				printf("Error writing to file\n");
				exit(EXIT_FAILURE);
			}

		// write header to file
		if (write(fd, &ar_head, SARHDR) == -1)
		{
			printf("Error writing file header to archive file\n");
			exit(EXIT_FAILURE);
		}

		//Read from member file, write to archive file
		mem_pos = lseek(mem_fd, 0, SEEK_SET);
		while( mem_pos < st.st_size ){
			to_write = ((st.st_size-mem_pos) < BLKSIZE) ? (st.st_size-mem_pos) : BLKSIZE;
			if (read(mem_fd, buf, to_write) == -1) {
				printf("Error reading file \n");
				exit(EXIT_FAILURE);
			}
			if (write(fd, buf, to_write) == -1) {
				printf("Error writing file\n");
				exit(EXIT_FAILURE);
			}
			mem_pos = lseek(mem_fd, 0, SEEK_CUR);
		}

		// if odd byte, write new line
		if (lseek(fd, 0, SEEK_CUR) % 2 != 0)
			if (write(fd, "\n", 1)== -1) {
				printf("Error writing to file\n");
				exit(EXIT_FAILURE);
			}

		if (close(mem_fd) == -1) {
			printf("Error closing file\n");
			exit(EXIT_FAILURE);
		}
	}
}


void print_verbose(int fd) {
	struct ar_hdr ar_head;
	off_t ar_size = lseek(fd, 0, SEEK_END);
	off_t pos = lseek(fd, SARMAG, SEEK_SET);
	char *pEnd;
	char tmp[16];
	char *str_table;
	size_t num_read;
	int i = 0;
	size_t member_size;
	char mode_str[10];
	mode_t mode;
	uid_t uid;
	gid_t gid;
	time_t seconds;
	struct tm *timeinfo;
	char time_str[18];

	while (pos < ar_size) {
		memset(&ar_head, ' ', SARHDR);
		if (read(fd, &ar_head, SARHDR) == -1)
			exit(EXIT_FAILURE);

		// Make sure header is complete
		if (strncmp(ar_head.ar_fmag, ARFMAG, SARFMAG) != 0) {
			printf("Error reading member header.");
			exit(EXIT_FAILURE);
		}

		member_size = strtol(ar_head.ar_size, &pEnd, 10);
		if (ar_head.ar_name[0] != '/') {
			// get string representation of permissions
			mode = strtol(ar_head.ar_mode, &pEnd, 8);
			ar_mode_string(mode, mode_str);
			printf("%s ",mode_str);

			// get user and group IDs
			memcpy(tmp, ar_head.ar_uid, SARFUID);
			tmp[SARFUID] = '\0';
			uid = strtol(tmp, &pEnd, 10);
			memcpy(tmp, ar_head.ar_gid, SARFGID);
			tmp[SARFGID] = '\0';
			gid = strtol(tmp, &pEnd, 10);
			printf("%d/%d ", uid, gid);

			// get decimal seconds since Epoch
			memcpy(tmp, ar_head.ar_date, SARFDAT);
			tmp[SARFDAT] = '\0';
			seconds = strtol(tmp, &pEnd, 10);
			// Get printable time
			timeinfo = localtime(&seconds);
			strftime(time_str, 18, "%h %e %R %Y", timeinfo);
			printf("%6lu ", member_size);
			printf("%s ", time_str);

			// get file name (need to remove trailing slash)
			i = 0;
			while (ar_head.ar_name[i] != '/') {
				tmp[i] = ar_head.ar_name[i];
				i++;
			}
			tmp[i] = '\0';
			printf("%s\n", tmp);
		}

		else if (ar_head.ar_name[1] == '/') {
			// ar_name = // --> archive string table
			str_table = (char*) malloc((sizeof(char*) * member_size));
			pos = lseek(fd, 0, SEEK_CUR);
			num_read = read(fd, str_table, member_size);
			pos = lseek(fd, (-1 * member_size), SEEK_CUR);
			if ((int)num_read == -1) {
				perror("Error reading archive string table.");
				free(str_table);
				exit(EXIT_FAILURE);
			}
			free(str_table);
		}
		// if member_size is even, add it to position
		// else add member_size+1 to position
		pos = lseek(fd, (member_size + 1) & (~1), SEEK_CUR);
	}
}


void print_concise(int fd) {
	struct ar_hdr ar_head;
	off_t ar_size = lseek(fd, 0, SEEK_END);
	off_t pos = lseek(fd, SARMAG, SEEK_SET);
	size_t member_size;
	char *pEnd = NULL;
	char tmp[16];
	char *str_table = NULL;
	size_t num_read;
	int i = 0;
	int off;
	char* str;
	char ch;

	while (pos < ar_size) {
		memset(&ar_head, ' ', SARHDR);
		if (read(fd, &ar_head, SARHDR) == -1)
			exit(EXIT_FAILURE);

		// Make sure header is complete
		if (strncmp(ar_head.ar_fmag, ARFMAG, SARFMAG) != 0) {
			printf("Error reading member header.");
			exit(EXIT_FAILURE);
		}
		member_size = strtol(ar_head.ar_size, &pEnd, 10);
		if (ar_head.ar_name[0] != '/') {
			// need to remove trailing slash
			i = 0;
			while (ar_head.ar_name[i] != '/') {
				tmp[i] = ar_head.ar_name[i];
				i++;
			}
			tmp[i] = '\0';
			printf("%s\n", tmp);
		} else {
			// archive symbol table
			if (ar_head.ar_name[1] == ' ') {
			} else if (ar_head.ar_name[1] == '/') {
				// ar_name = // --> archive string table
				str_table = (char*) malloc((sizeof(char*) * member_size));
				pos = lseek(fd, 0, SEEK_CUR);
				num_read = read(fd, str_table, member_size);
				pos = lseek(fd, (-1 * member_size), SEEK_CUR);
				if ((int)num_read == -1) {
					perror("Error reading archive string table.");
					exit(EXIT_FAILURE);
				}
			}else {
			// ar_name = /[decimal]
			// look up in str_table --- not for this assignment
				str = ar_head.ar_name;
				str += 1;
				off = strtol(str, &pEnd, 10); // get offset into str_table
				pEnd = &str_table[off];
				i = 0;
				while (pEnd[i] != '/') {
					ch = pEnd[i];
					printf("%c", pEnd[i]);
					i++;
				}
				printf("\n");
			}
		}
		// if member_size is even, add it to position
		// else add member_size+1 to position
		pos = lseek(fd, (member_size + 1) & (~1), SEEK_CUR);
	}
}


int in_file_list(char *fname, char**list, int size) {
	int i;
	for (i = 0; i < size; i++) {
		if (memcmp(fname, list[i], strlen(fname))) {
			return 0;
		}
	}
	return 1;
}


int open_ar_file(char *path, char key) {
	int fd;
	int create_flag = 0;
	char hdr_in[SARMAG];

	// if need to create file, create it & set flag
	if ((key == 'q') && (access(path, F_OK) == -1)) {
		fd = open(path, O_RDWR | O_CREAT, FILE_PERMS);
		create_flag = 1;
	}
	// all others, error if can't be opened
	else
		fd = open(path, O_RDWR);

	// if file can't be opened, exit
	if (fd == -1) {
		printf("Error: %s\n", strerror(errno));
		return (-1);
	}

	// if new file, write header
	if (create_flag == 1) {
		lseek(fd, 0, SEEK_SET);
		if (write(fd, ARMAG, SARMAG) == -1) {
			printf("Unable to write global header\n");
			return (-1);
		}
	}
	// else, verify global header
	else {
		lseek(fd, 0, SEEK_SET);
		if (read(fd, hdr_in, SARMAG) == -1) {
			printf("Error reading global header\n");
			return (-1);
		}
		if (memcmp(hdr_in, ARMAG, SARMAG) == -1) {
			printf("Bad global header\n");
			return (-1);
		}
	}
	return fd;
}


void ar_mode_string(mode_t mode, char* mode_string) {
	mode_string[0] = (mode & S_IRUSR) ? 'r' : '-';
	mode_string[1] = (mode & S_IWUSR) ? 'w' : '-';
	mode_string[2] = (mode & S_IXUSR) ? 'x' : '-';
	mode_string[3] = (mode & S_IRGRP) ? 'r' : '-';
	mode_string[4] = (mode & S_IWGRP) ? 'w' : '-';
	mode_string[5] = (mode & S_IXGRP) ? 'x' : '-';
	mode_string[6] = (mode & S_IROTH) ? 'r' : '-';
	mode_string[7] = (mode & S_IWOTH) ? 'w' : '-';
	mode_string[8] = (mode & S_IXOTH) ? 'x' : '-';
	mode_string[9] = '\0';
}


void usage() {
	printf("Usage: ar [-]{qxtvd} [member-name] archive-file file...\n"
			" commands:\n"
			"  q            - quick append file(s) to the archive\n"
			"  x            - extract file(s) from the archive\n"
			"  t            - display contents of archive\n"
			"  v            - display verbose\n"
			"  d            - delete file(s) from the archive\n");
}


int main(int argc, char **argv)  {
	int fd;
    char c;
    char key;
    char *ar_name = NULL;


    // Process command line option arguments
	if (argc > 1 && *argv[1] != '-') {
		optplace = argv[1];
	}
    while ((c = getopt (argc, argv, "qxtvd")) != -1)
        switch (c) {
            case 'q':
                key = 'q';
                break;
            case 'x':
                key = 'x';
                break;
            case 't':
                key = 't';
                break;
            case 'v':
                key = 'v';
                break;
            case 'd':
                key = 'd';
                break;
            case 'w':
                key = 'w';
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
                break;
        }

    // Make sure an option was entered
    if (!key){
    	printf("No option given\n");
    	usage();
    	exit(-1);
    }

    // Get archive path
    if (optind < argc) {
        ar_name = (char*)malloc(sizeof(char) * (strlen(argv[optind]) + 1));
        assert(ar_name != NULL);
        strcpy(ar_name, argv[optind]);
        optind = optind + 1;
    }else { // Error - no archive file
        printf("no archive file given.\n");
        usage();
		exit(-1);
    }

    // Open archive fle
    fd = open_ar_file(ar_name, key);

    // Check key and call function
    do {
        switch(key) {
            case 'q':
                if (argv[optind] == NULL) {
                    printf("no file given to append.\n");
                	usage();
                	exit(EXIT_FAILURE);
                }
            	append(fd, &(argv[optind]), (argc-optind));
            	optind = argc;
                break;
            case 't':
                print_concise(fd);
                break;
            case 'v':
                print_verbose(fd);
                break;
            case 'x':
                extract(fd, argv[optind] ? argv[optind] : NULL);
                break;
            case 'd':
                if (argv[optind] == NULL) {
                    printf("no file given to delete.\n");
                	usage();
                	exit(EXIT_FAILURE);
                }
            	delete(&fd, ar_name, &(argv[optind]), (argc-optind));
            	optind = argc;
                break;
            case 'w':
                break;
        }
        optind++;
    } while (optind < argc);

    // Close file
    if (close(fd) == -1)
        printf("ERROR: %s\n", strerror(errno));
    return(0);
}
