struct dev_gate_intf {
	int (*read)(const char *file, unsigned long offset, void *dst, unsigned int size);
	int (*write)(const char *file, unsigned long offset, void *dst, unsigned int size);
	void (*gettime)(struct gate_time *gt);
	void (*exit)(int status);
};

void dev_gate_setup(struct dev_gate_intf *dgi);
