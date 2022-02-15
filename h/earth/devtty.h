/* Interface to the terminal device.
 */

struct dev_tty_intf {
	struct dev_tty *(*create)(int fd,
				void (*deliver)(void *, char *, int), void *arg);
	void (*write)(unsigned int ino, const void *buf, unsigned int size);
};

void dev_tty_setup(struct dev_tty_intf *dti);
