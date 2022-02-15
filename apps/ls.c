#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <egos/file.h>
#include <egos/dir.h>

char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void ls(char *name, fid_t fid, bool long_list){
	if (long_list) {
		printf("S:I\tSIZE\tMODE\tUID\tTIME\t\tNAME\n");
		printf("====================================================\n");
	}
	char *buf = malloc(PAGESIZE);
	unsigned long offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool status = file_read(fid.server, fid.file_no, offset, buf, &n);
		if (!status) {
			fprintf(stderr, "ls: error reading '%s'\n", name);
			free(buf);
			return;
		}
		if (n == 0) {
			free(buf);
			return;
		}
		// assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];
			de->name[DIR_NAME_SIZE - 1] = 0;
			if (de->fid.server == 0) {
				de->fid.server = fid.server;
			}

			if (de->name[0] != 0) {
				if (long_list) {
					printf("%u:%u\t", de->fid.server, de->fid.file_no);

					struct file_control_block stat;
					status = file_stat(de->fid.server, de->fid.file_no, &stat);
					if (status) {
						printf("%u\t", (unsigned int) stat.st_size);
						putchar((stat.st_mode & P_FILE_OWNER_READ) ? 'r' : '-');
						putchar((stat.st_mode & P_FILE_OWNER_WRITE) ? 'w' : '-');
						putchar((stat.st_mode & P_FILE_OTHER_READ) ? 'r' : '-');
						putchar((stat.st_mode & P_FILE_OTHER_WRITE) ? 'w' : '-');
						printf("\t%d", stat.st_uid);
					}
					else {
						printf("?\t?\t?");
					}

					printf("\t%lu", stat.st_modtime);
					// time_t t = time;
					// struct tm* info;
					// info = localtime(&t);
					// _secs_to_tm(time, info);
					// printf("\t%s %d %d:%d", months[info->tm_mon - 1], info->tm_mday, info->tm_hour, info->tm_min);

					printf("\t%s\n", de->name);
				}
				else {
					printf("%s\n", de->name);
				}
			}
		}
	}
}

int main(int argc, char **argv){
	int i;
	fid_t fid;

	bool long_list = false, something = false;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-l") == 0) {
			long_list = true;
			continue;
		}
		fid_t dir;
		if (!flookup(argv[i], true, &dir, &fid)) {
			printf("%s: unknown directory '%s'\n", argv[0], argv[i]);
		}
		else {
			ls(argv[i], fid, long_list);
		}
		something = true;
	}

	if (!something) {
		ls(".", GRASS_ENV->cwd, long_list);
	}

	return 0;
}
