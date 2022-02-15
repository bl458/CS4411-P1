#include <stdbool.h>

struct dev_disk_intf {
	struct dev_disk *(*create)(char *file_name, unsigned int nblocks, bool sync);
	unsigned int (*getsize)(struct dev_disk *dd);
	void (*write)(struct dev_disk *dd, unsigned int offset, const char *data,
					void (*completion)(void *arg, bool success), void *arg);
	void (*read)(struct dev_disk *dd, unsigned int offset, char *data,
					void (*completion)(void *arg, bool success), void *arg);
};

void dev_disk_setup(struct dev_disk_intf *ddi);
