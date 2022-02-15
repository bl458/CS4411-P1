/* mkfs is used to create a new file system on a disk.
 *
 * The type of file system is specified with the -c option
 * (the default is TreeDisk).
 *
 * mkfs is a little like tar: it traverses each of the provided
 * arguments, but instead of creating a tarball it creates a
 * file system.
 *
 * You can specify the disk size in blocks with the -d option.
 * You can specify the default uid (file owner) with the -u option.
 *
 * A directory that contains a file .mkfs-skip is not included.
 * A directory containing a file .mkfs-uid with a decimal integer
 * in it override the default uid.
 */

#define _GNU_SOURCE

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include <assert.h>
#include <egos/block_store.h>
#include <egos/file.h>
#include <egos/dir.h>

#define DISK_SIZE			(256 * 1024)
#define CACHE_SIZE			256
#define NINODES				4096			// number of inodes to create
#define MAX_DIR_ENTRIES		1024			// max # entries per directory

unsigned int ino = 1;

struct file_control_block stats[NINODES];

void trav(block_store_t *bs, fid_t parent, char *path, char *name,
					unsigned int uid, int depth, struct dir_entry *de);

unsigned int dir_trav(block_store_t *bs, fid_t parent, char *path, time_t mtime, unsigned int uid, int depth){
	DIR *dir;
	struct dirent *d;
	unsigned int my_ino = ino++;
	struct dir_entry entries[MAX_DIR_ENTRIES];
	unsigned int index = 2;

#ifdef VERBOSE
	for (int i = 0; i < depth; i++) {
		printf("    ");
	}
#endif

	char *skip;
	asprintf(&skip, "%s/.mkfs-skip", path);
	if (access(skip, F_OK) == 0) {
		printf("%s.dir: skipping\n", path);
		free(skip);
		return 0;
	}
	else {
		free(skip);
#ifdef VERBOSE
		printf("%s.dir: ino=%u\n", path, my_ino);
#endif
	}

	char *uidfile;
	asprintf(&uidfile, "%s/.mkfs-uid", path);
	FILE *fp = fopen(uidfile, "r");
	if (fp != 0) {
		fscanf(fp, "%u", &uid);
		fclose(fp);
	}
	free(uidfile);

	memset(entries, 0, sizeof(entries));
	strcpy(entries[0].name, "..dir");
	entries[0].fid.server = parent.server;
	entries[0].fid.file_no = my_ino;
	strcpy(entries[1].name, "...dir");
	entries[1].fid = parent;

	if ((dir = opendir(path)) == 0) {
		perror(path);
		exit(1);
	}
	while ((d = readdir(dir)) != 0) {
		if (d->d_name[0] != '.') {
			char *full;
			asprintf(&full, "%s/%s", path, d->d_name);
			trav(bs, entries[0].fid, full, d->d_name, uid, depth + 1, &entries[index]);
			free(full);
			if (entries[index].fid.file_no != 0) {		// ignore skipped entries
				index++;
			}
		}
	}
	closedir(dir);

	struct file_control_block *st = &stats[my_ino];
	st->st_alloc = true;
	st->st_dev = 0;
	st->st_ino = my_ino;
	st->st_uid = uid;
	st->st_mode = P_FILE_OTHER_READ | P_FILE_OWNER_READ | P_FILE_OWNER_WRITE;
	st->st_size = (char *) &entries[index] - (char *) entries;
	st->st_modtime = mtime;

	unsigned int nblocks = (st->st_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	block_t *block = (block_t *) entries;
	for (unsigned int i = 0; i < nblocks; i++) {
		int r = (*bs->write)(bs, my_ino, i, block++);
		assert(r == 0);
	}

	return my_ino;
}

unsigned int file_trav(block_store_t *bs, fid_t parent, char *path, time_t mtime, unsigned int uid, int depth){
	unsigned int my_ino = ino++;

	FILE *fp = fopen(path, "r");
	if (fp == 0) {
		fprintf(stderr, "can't open '%s'\n", path);
		exit(1);
	}
	unsigned int total = 0;
	int last_read = BLOCK_SIZE;
	block_no offset = 0;
	for (;;) {
		block_t block;
		int n = fread(&block, 1, BLOCK_SIZE, fp);
		if (n == 0) {
			if (ferror(fp)) {
				perror(path);
				exit(1);
			}
			break;
		}
		assert(n == BLOCK_SIZE || last_read == BLOCK_SIZE);
		last_read = n;
		int r = (*bs->write)(bs, my_ino, offset++, &block);
		assert(r == 0);
		total += n;
	}
#ifdef VERBOSE
	for (int i = 0; i < depth; i++) {
		printf("    ");
	}
	printf("%s: file ino=%u size=%u\n", path, my_ino, total);
#endif
	fclose(fp);

	struct file_control_block *st = &stats[my_ino];
	st->st_alloc = true;
	st->st_dev = 0;
	st->st_ino = my_ino;
	st->st_uid = uid;
	st->st_mode = P_FILE_OTHER_READ | P_FILE_OWNER_READ | P_FILE_OWNER_WRITE;
	st->st_size = total;
	st->st_modtime = mtime;

	return my_ino;
}

void trav(block_store_t *bs, fid_t parent, char *path, char *name,
						unsigned int uid, int depth, struct dir_entry *de){
	struct stat st;

	if (access(path, R_OK) < 0) {
		perror(path);
		exit(1);
	}
	if (stat(path, &st) < 0) {
		perror(path);
		exit(1);
	}
	if (S_ISDIR(st.st_mode)) {
		sprintf(de->name, "%s.dir", name);
		de->fid.server = parent.server;
		de->fid.file_no = dir_trav(bs, parent, path, st.st_mtime, uid, depth);
	}
	else if (S_ISREG(st.st_mode)) {
		strcpy(de->name, name);
		de->fid.server = parent.server;
		de->fid.file_no = file_trav(bs, parent, path, st.st_mtime, uid, depth);
	}
	else {
		fprintf(stderr, "skipping weird file %s\n", path);
	}
}

block_t blocks[CACHE_SIZE];

static void usage(char *name){
	fprintf(stderr, "Usage: %s [-d disk-size] [-u uid] [-c file-sys-conf] path1 ...\n", name);
	exit(1);
}

#define BOTTOM_INODE 		0

int main(int argc, char **argv){
	unsigned int uid = 0, disksize = DISK_SIZE;
	char *fsconf = "tree", *file_name = "storage/fs.dev";

	char c;
    while ((c = getopt(argc, argv, "c:d:f:u:")) != -1) {
		switch (c) {
		case 'c':
			fsconf = optarg;
			break;
		case 'd':
			disksize = atoi(optarg);
			break;
		case 'f':
			file_name = optarg;
			break;
		case 'u':
			uid = atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}
	if (optind >= argc) {
		usage(argv[0]);
	}

	/* Create the file system.
	 */
	block_store_t *file = filedisk_init(file_name, disksize);
	assert(file != 0);
	block_store_t *cache = clockdisk_init(file, blocks, CACHE_SIZE);
	assert(cache != 0);

	block_store_t *bs = 0;
	if (strcmp(fsconf, "tree") == 0) {
		if (treedisk_create(cache, BOTTOM_INODE, NINODES) < 0) {
			fprintf(stderr, "main: can't create treedisk file system\n");
			exit(1);
		}
		bs = treedisk_init(cache, BOTTOM_INODE);
	}
#ifdef HW_FS
	else if (strcmp(fsconf, "unix") == 0) {
		if (unixdisk_create(cache, BOTTOM_INODE, NINODES) < 0) {
			fprintf(stderr, "main: can't create unixdisk file system\n");
			exit(1);
		}
		bs = unixdisk_init(cache, BOTTOM_INODE);
	}
	else if (strcmp(fsconf, "fat") == 0) {
		if (fatdisk_create(cache, BOTTOM_INODE, NINODES) < 0) {
			fprintf(stderr, "main: can't create fatdisk file system\n");
			exit(1);
		}
		bs = fatdisk_init(cache, BOTTOM_INODE);
	}
#endif //HW_FS
	else {
		fprintf(stderr, "main: unknown fs configuration '%s'\n", fsconf);
		exit(1);
	}
	assert(bs != 0);

	fid_t fid;
	fid.server = 0;
	fid.file_no = 0;
	struct dir_entry de;
	for (int i = optind; i < argc; i++) {
		trav(bs, fid, argv[i], argv[i], uid, 0, &de);
	}

	struct file_control_block *st = &stats[0];
	st->st_alloc = true;
	st->st_dev = 0;
	st->st_ino = 0;
	st->st_uid = uid;
	st->st_mode = P_FILE_OTHER_READ | P_FILE_OWNER_READ | P_FILE_OWNER_WRITE;
	st->st_size = NINODES * sizeof(*st);
	time(&st->st_modtime);			// TODO

	unsigned int nblocks = (st->st_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
	block_t *block = (block_t *) stats;
	for (unsigned int i = 0; i < nblocks; i++) {
		int r = (*bs->write)(bs, 0, i, block++);
		assert(r == 0);
	}

	(*bs->sync)(bs, (unsigned int) -1);
	(*bs->release)(bs);

	return 0;
}
