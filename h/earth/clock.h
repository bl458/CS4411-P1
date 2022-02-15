/* Interface to the clock device.
 */

struct clock_intf {
	unsigned long (*now)(void);						// returns time in msec
	void (*start_timer)(unsigned int interval);		// in msec
	void (*initialize)();
};

void clock_setup(struct clock_intf *ci);
