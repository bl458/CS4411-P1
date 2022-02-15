/* Interface to the UDP device.
 */

struct dev_udp_intf {
	struct dev_udp *(*create)(uint16_t port,
			void (*deliver)(void *, char *, unsigned int), void *arg);
	void (*send)(void *dest, char *buf, int unsigned size);
};

void dev_udp_setup(struct dev_udp_intf *dui);
