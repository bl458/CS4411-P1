/* This file contains everything to do with the interface to grass processes.
 */

#include <stdbool.h>
#include <signal.h>
#include <egos/syscall.h>
#include <egos/queue.h>
#include <egos/exec.h>

#define MAX_SEGMENTS		4
#define PG_DEV_SIZE			1024		// #pages on paging device

// Why does it have to be so large...?
#define KERNEL_STACK_SIZE	(64 * 1024)	// size of kernel stack of a process

/* A message consists of the source and a contents.
 */
struct message {
	gpid_t src;					// source process id
	unsigned int uid;			// source user id
	void *contents;				// contents of message
	unsigned int size;			// size in bytes
};

/* Page info.
 */
struct page_info {
	enum {
		PI_UNINIT,		// page not yet accessed
		PI_VALID,		// page mapped
	} status;
	union {
		unsigned int frame;		// if VALID (in memory)
	} u;
};

/* Message queue definition.
 */
struct msg_queue {
	bool waiting;					// true iff process is waiting for messages
	struct queue messages;			// list of messages that have arrived
};

/* One of these per process.
 */
struct process {
	gpid_t pid;					// process identifier
	char descr[16];				// for dumps
	void (*start)(void *);		// starting point
	void (*finish)(void *);		// ending point
	void *arg;					// argument to these functions

	unsigned int uid;			// user identifier of this process

	/* The owner (or "parent") of a process is notified (with a MSG_EVENT)
	 * when this process dies.
	 */
	gpid_t owner;

	/* Process states.
	 */
	enum {
		PROC_FREE,				// this slot is empty
		PROC_RUNNABLE,			// can be run (or is running)
		PROC_WAITING,			// waiting for message
		PROC_ZOMBIE				// dead but not cleaned up
	
	#ifdef HW_MLFQ
		int ticks_left;  
		int priority_level;
	#endif 
	} state;

	/* Message queues for synchronization between processes.
	 */
	struct msg_queue mboxes[MSG_NTYPES];

	/* Message buffer for receiving user processes.
	 */
	char *msgbuf;

	/* If the process is waiting for a response, this is the server.
	 */
	gpid_t server;

	/* If the process is waiting, it may have an alarm set.
	 */
	bool alarm_set;				// see if an alarm has been set
	unsigned long exptime;			// experiration time


	bool interruptable;		// can be interrupted with <ctrl>C

	/* Interrupt information.
	 */
	enum intr_type intr_type;	// type of last interrupt
	void *intr_arg;				// argument to last interrupt
	void (*intr_ip)();			// mostly used for debugging if anything

	/* Software page table.
	 */
	struct page_info pages[VIRT_PAGES];

	/* id and header of executable.
	 */
	fid_t executable;
	struct header {
		struct exec_header eh;
		struct exec_segment es[MAX_SEGMENTS];
	} hdr;

	/* Signal stack --- only needed for user processes.
	 */
	char sigstack[SIGNAL_STACK_SIZE];
	address_t sig_sp;			// saved stack pointer into this stack

	/* Kernel stack.
	 */
	char kernelstack[KERNEL_STACK_SIZE];
	address_t kernel_sp;

#ifndef NO_UCONTEXT
	ucontext_t ctx;
#endif

	address_t user_sp;			// user mode stack pointer
};

/* A global variable that identifies the current process.
 */
extern struct process *proc_current;

gpid_t proc_create(gpid_t owner, char *descr, void (*fun)(void *), void *arg);
gpid_t proc_create_uid(gpid_t owner, char *descr, void (*fun)(void *), void *arg, unsigned int uid);
void proc_yield(void);
void proc_kill(gpid_t killer, gpid_t pid, int status);
struct process *proc_find(gpid_t pid);
void proc_to_kernel(void);
void proc_got_interrupt(void);
void proc_dump(void);
void proc_initialize(void);
void proc_shutdown(void);
bool proc_recv(enum msg_type mtype, unsigned int max_time, void *contents,
					unsigned int *psize, gpid_t *psrc, unsigned int *puid);
bool proc_send(gpid_t src_pid, unsigned int src_uid, gpid_t dst_pid,
			enum msg_type mtype, const void *contents, unsigned int size);
void proc_pagefault(address_t virt, bool update);
void proc_term(struct process *p, int status);
void proc_syscall();

/* copy_user() is a routine used to copy data between kernel and user
 * space.  It is unsave to directly try to access user space data from
 * the kernel proc as the data may not be paged in or mapped.
 */
enum cu_dir { CU_TO_USER, CU_FROM_USER };
void copy_user(char *dst, const char *src, unsigned int size, enum cu_dir dir);

void sigstk_init(struct process *proc);

void fs_install(struct grass_env *ge);
fid_t file_load(gpid_t server, unsigned int uid, const char *src);

void shut_down();
