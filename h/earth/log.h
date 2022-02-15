struct log_intf {
	void (*panic)(const char *fmt, ...);
	void (*p)(const char *fmt, ...);
	void (*print)(const char *buf, unsigned int size);
	void (*init)(void);
};

void log_setup(struct log_intf *li);
