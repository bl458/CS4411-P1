/* Interface to emulated TLB device.
 */

struct tlb_intf {
	void (*initialize)(unsigned int nentries);
	void (*flush)(void);
	void (*sync)(void);
	int (*map)(unsigned int tlb_index, page_no virt, void *phys,
									unsigned int prot);
	int (*get)(unsigned int tlb_index, page_no *virt,
									void **phys, unsigned int *prot);
	void (*unmap)(page_no virt);
	int (*get_entry)(page_no virt);
};

void tlb_setup(struct tlb_intf *ti);
