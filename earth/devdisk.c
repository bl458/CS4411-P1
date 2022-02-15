#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <earth/earth.h>
#include <earth/intf.h>

struct dev_disk {
	int fd;
	unsigned int nblocks;
	bool sync;
};

struct dd_event {
	struct dev_disk *dd;
	void (*completion)(void *arg, bool success);
	void *arg;
	bool success;
};

/* Create a "disk device", simulated on a file.
 */
static struct dev_disk *dev_disk_create(char *file_name, unsigned int nblocks, bool sync){
	struct dev_disk *dd = calloc(1, sizeof(struct dev_disk));

	/* Open the disk.  Create if non-existent.
	 */
	dd->fd = open(file_name, O_RDWR, 0600);
	if (dd->fd < 0) {
		dd->fd = open(file_name, O_RDWR | O_CREAT, 0600);

		if (dd->fd < 0) {
			perror(file_name);
			free(dd);
			return 0;
		}
	}

	/* See how large the file is.
	 */
	off_t size = lseek(dd->fd, 0, SEEK_END);
	if (size < 0) {
		perror(file_name);
		free(dd);
		return 0;
	}
	if (size > 0) {
		dd->nblocks = size / BLOCK_SIZE;
		printf("earth: dev_disk_create %s: existing: %u blocks\n\r", file_name, dd->nblocks);
		assert(dd->nblocks >= nblocks);
	}
	else {
		/* Make the device the right size.
		 */
		dd->nblocks = nblocks;
		printf("earth: dev_disk_create %s: new: %u blocks\n\r", file_name, dd->nblocks);
		size = lseek(dd->fd, (off_t) nblocks * BLOCK_SIZE - 1, SEEK_SET);
		assert(size > 0);
		(void) write(dd->fd, "", 1);
	}
	dd->sync = sync;
	return dd;
}

/* Simulated disk completion event.
 */
static void dev_disk_complete(void *arg){
	struct dd_event *ddev = arg;

	(*ddev->completion)(ddev->arg, ddev->success);
	free(ddev);
}

/* Simulate a disk operation completion event.
 */
static void dev_disk_make_event(struct dev_disk *dd,
			void (*completion)(void *arg, bool success), void *arg, bool success){
	struct dd_event *ddev = calloc(1, sizeof(struct dd_event));

	ddev->dd = dd;
	ddev->completion = completion;
	ddev->arg = arg;
	ddev->success = success;
	earth.intr.sched_event(dev_disk_complete, ddev);
}

/* Write a block.  Invoke completion() when done.
 */
static void dev_disk_write(struct dev_disk *dd, unsigned int offset, const char *data,
				void (*completion)(void *arg, bool success), void *arg){
	bool success;

	if (offset >= dd->nblocks) {
		fprintf(stderr, "dev_disk_write: offset too large\n");
		success = false;
	}
	else {
		lseek(dd->fd, (off_t) offset * BLOCK_SIZE, SEEK_SET);

		int n = write(dd->fd, data, BLOCK_SIZE);
		if (n < 0) {
			perror("dev_disk_write");
			success = false;
		}
		else if (n != BLOCK_SIZE) {
			fprintf(stderr, "disk_write: wrote only %d bytes\n", n);
			success = false;
		}
		else {
			if (dd->sync) {
				fsync(dd->fd);
			}
			success = true;
		}
	}

	dev_disk_make_event(dd, completion, arg, success);
}

static unsigned int dev_disk_getsize(struct dev_disk *dd){
	return dd->nblocks;
}

/* Read a block.  Invoke completion() when done.
 */
static void dev_disk_read(struct dev_disk *dd, unsigned int offset, char *data,
				void (*completion)(void *arg, bool success), void *arg){
	bool success;

	if (offset >= dd->nblocks) {
		fprintf(stderr, "dev_disk_read: offset too large\n");
		success = false;
	}
	else {
		lseek(dd->fd, (off_t) offset * BLOCK_SIZE, SEEK_SET);

		int n = read(dd->fd, data, BLOCK_SIZE);
		if (n < 0) {
			perror("dev_disk_read");
			success = false;
		}
		else {
			if (n < BLOCK_SIZE) {
				memset((char *) data + n, 0, BLOCK_SIZE - n);
			}
			success = true;
		}
	}

	dev_disk_make_event(dd, completion, arg, success);
}

void dev_disk_setup(struct dev_disk_intf *ddi){
	ddi->create = dev_disk_create;
	ddi->getsize = dev_disk_getsize;
	ddi->read = dev_disk_read;
	ddi->write = dev_disk_write;
};
