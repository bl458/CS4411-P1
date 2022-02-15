#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <earth/earth.h>
#include <earth/intf.h>

/* Read from a local file.
 */
static int dev_gate_read(const char *file, unsigned long offset,
								void *dst, unsigned int size){
	FILE *fp = fopen(file, "r");
	if (fp == 0) {
		perror("dev_gate_read");
		fprintf(stderr, "dev_gate_read: can't open '%s'\n", file);
		return -1;
	}
	if (fseek(fp, offset, SEEK_SET) != 0) {
		fprintf(stderr, "dev_gate_read: can't seek to %lu on '%s'\n", offset, file);
		fclose(fp);
		return -1;
	}
	size_t n = fread(dst, 1, size, fp);
	if (n == 0 && ferror(fp)) {
		fprintf(stderr, "dev_gate_read: can't read from '%s' at offset %lu\n", file, offset);
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return n;
}

/* Write to a local file.  If offset == 0, then truncate file first.
 *
 * TODO.  Truncate should be controlled by a flag.
 */
static int dev_gate_write(const char *file, unsigned long offset,
								void *dst, unsigned int size){
	FILE *fp = fopen(file, offset == 0 ? "w" : "a");
	if (fp == 0) {
		perror("dev_gate_write");
		fprintf(stderr, "dev_gate_write: can't open '%s'\n", file);
		return -1;
	}
	if (fseek(fp, offset, SEEK_SET) != 0) {
		fprintf(stderr, "dev_gate_write: can't seek to %lu on '%s'\n", offset, file);
		fclose(fp);
		return -1;
	}
	size_t n = fwrite(dst, 1, size, fp);
	if (n == 0 && ferror(fp)) {
		fprintf(stderr, "dev_gate_write: can't write from '%s' at offset %lu\n", file, offset);
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return n;
}

static void dev_gate_gettime(struct gate_time *gt){
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	gt->seconds = tv.tv_sec;
	gt->useconds = tv.tv_usec;
	gt->minuteswest = tz.tz_minuteswest;
	gt->dsttime = tz.tz_dsttime;
}

static void dev_gate_exit(int status){
	printf("earth: exit with status %d\n\r", status);
	exit(status);
}

void dev_gate_setup(struct dev_gate_intf *dgi){
	dgi->read = dev_gate_read;
	dgi->write = dev_gate_write;
	dgi->gettime = dev_gate_gettime;
	dgi->exit = dev_gate_exit;
};
