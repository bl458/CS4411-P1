#include <inttypes.h>
#include <stdio.h>
#ifndef NO_UCONTEXT
#include <ucontext.h>
#endif
#include "process.h"
#include <assert.h>
#include <earth/earth.h>
#include <earth/intf.h>
#include <egos/context.h>
#include <egos/file.h>
#include <egos/malloc.h>
#include <egos/queue.h>
#include <egos/spawn.h>
#include <egos/syscall.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// # block on paging device
#define PG_DEV_BLOCKS ((PG_DEV_SIZE * PAGESIZE) / BLOCK_SIZE)

static bool initializing = true;
fid_t pgdev; // page device

static address_t kernel_sp;                  // kernel stack pointer
static address_t signal_sp;                  // signal stack pointer
static char signal_stack[SIGNAL_STACK_SIZE]; // stack for initial pgfault

void sigstk_init(struct process *proc) {
  memcpy(proc->sigstack, signal_stack, SIGNAL_STACK_SIZE);
  proc->sig_sp = signal_sp;
}

/* Loop of the main thread.
 */
static void proc_mainloop() {
  for (;;) {
    struct msg_event mev;

    int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0, 0);
    if (size < 0) {
      printf("proc_mainloop: exiting\n");
      return;
    }
    if (size == sizeof(mev) && mev.type == MEV_PROCDIED) {
      printf("Kernel main loop: process %u died\n\r", mev.pid);
    } else {
      printf("Kernel main loop: unexpected event\n\r");
    }
  }
}

/* Interrupt handler.
 */
static void intr_handler(struct intr_context *ic, void *arg) {
  /* The very first page fault is for initialization.  To distinguish
   * illegal accesses to address 1 from initialization, we use the
   * 'initializing' flag.
   */
  if (initializing && ic->type == INTR_PAGE_FAULT && arg == (void *)1) {
    initializing = false;

    /* Switch back to the kernel stack.
     */
    ctx_switch(&signal_sp, kernel_sp);

    /* We return here everytime a new user process is created.  We're now
     * running on the signal stack of the process.  Start the user process
     * by returning from interrupt with the ip (program counter) and stack
     * pointer updated.
     */
    ic->ip = (void (*)(void))proc_current->hdr.eh.eh_start;
    ic->sp = (void *)proc_current->user_sp;
  }

  /* In the normal case, save the interrupt info in the process structure
   * and switch to its kernel stack, after which the interrupt may be handled.
   */
  else {
    assert(proc_current->state == PROC_RUNNABLE);
    proc_current->intr_type = ic->type;
    proc_current->intr_arg = arg;
    proc_current->intr_ip = ic->ip;
    ctx_switch(&proc_current->sig_sp, proc_current->kernel_sp);
  }
}

/* Load a "grass" file with contents of a local file.
 */
static void fs_load(fid_t fid, const char *file) {
  char buf[BLOCK_SIZE];
  unsigned long offset = 0;
  for (;;) {
    int n = earth.dev_gate.read(file, offset, buf, BLOCK_SIZE);
    assert(n >= 0);
    if (n == 0) {
      break;
    }
    bool status = file_write(fid.server, fid.file_no, offset, buf, n);
    assert(status);
    offset += n;
  }
}

/* Create and load a file on the given server.
 */
fid_t file_load(gpid_t server, unsigned int uid, const char *src) {
  /* Allocate a file.
   */
  unsigned int file_no;
  bool r = file_create(server, P_FILE_DEFAULT, &file_no);
  assert(r);
  if (uid != 0) {
    r = file_chown(server, file_no, uid);
    assert(r);
  }

  /* Initialize the file.
   */
  fid_t fid = fid_val(server, file_no);
  fs_load(fid, src);

  return fid;
}

/* Spawn an executable with a string array of arguments to be passed as if
 * on the command line. (The arguments are optional; nargs can be 0).
 */
static gpid_t spawn_user_proc(struct grass_env *ge, const char *file, int nargs,
                              char *const *args) {
  /* Copy the file into the ram file server.
   */
  fid_t fid = file_load(ge->servers[GPID_FILE_RAM], 0, file);

  /* Create an argument block and spawn.
   */
  gpid_t pid;
  const int argc = nargs + 1;
  char *argv[argc], *argb;
  unsigned int size;
  argv[0] = (char *)file;
  for (int i = 1; i <= nargs; ++i) {
    argv[i] = args[i - 1];
  }
  (void)spawn_load_args(ge, argc, argv, &argb, &size);
  (void)spawn_exec(ge->servers[GPID_SPAWN], fid, argb, size, false, 0, &pid);
  m_free(argb);

  return pid;
}

/* It all begins here...
 */
void grass_main(void) {
  struct grass_env ge;
  memset(&ge, 0, sizeof(ge));

  earth.clock.initialize();            // initialize the clock
  earth.intr.initialize(intr_handler); // initialize interrupt handling

  printf("grass_main: virtual address space base = %p\n\r", VIRT_BASE);
  printf("grass_main: virtual address space top  = %p\n\r", VIRT_TOP);

  /* Cause a segmentation fault to create an initialized signal stack.
   */
  printf("grass_main: cause fault to initialize the signal stack\n\r");
  ctx_switch(&kernel_sp, 1); // 1 is an illegal address!

  /* At this point, the underlying kernel (Linux, MacOSX) thinks we're
   * executing on the signal stack with clock and I/O interrupts disabled.
   * We're actually back on the main stack after a context switch.
   */
  printf("grass_main: back from causing fault\n\r");

  /* Save the signal stack.  This one is going to be reused every time
   * a new process starts.
   */
  earth.intr.save(signal_stack, (void *)signal_sp);

  printf("grass_main: initialize the grass processes module\n\r");

  /* Initialize process management.
   */
  proc_initialize();

  /* Start the periodic timer.
   */
  earth.clock.start_timer(10);

  printf("grass_main: starting servers\n\r");

  ge.self = sys_getpid();
  ge.cwd = ge.root;

  /* Start a bunch of servers.
   */
  gpid_t tty_init(void);
  ge.servers[GPID_TTY] = tty_init();

  ge.stdin = fid_val(ge.servers[GPID_TTY], 0);
  ge.stdout = fid_val(ge.servers[GPID_TTY], 1);
  ge.stderr = fid_val(ge.servers[GPID_TTY], 2);
  ge.stdnull = fid_val(ge.servers[GPID_TTY], 3); // sort of like /dev/null

  gpid_t spawn_init(void);
  ge.servers[GPID_SPAWN] = spawn_init();

  gpid_t gate_init(void);
  ge.servers[GPID_GATE] = gate_init();

  gpid_t ramfile_init(gpid_t gate);
  ge.servers[GPID_FILE_RAM] = ramfile_init(ge.servers[GPID_GATE]);

  gpid_t disk_init(char *filename, unsigned int nblocks, bool sync);
  ge.servers[GPID_DISK_PAGE] =
      disk_init("storage/page.dev", PG_DEV_BLOCKS, false);
  ge.servers[GPID_DISK_FS] = disk_init("storage/fs.dev", 16 * 1024, false);

  // The -c argument to the block server determines which type of filesystem it
  // uses
  char *blocksvr_args[] = {"-c", "tree"};

  ge.servers[GPID_BLOCK] =
      spawn_user_proc(&ge, "bin/blocksvr.exe", 2, blocksvr_args);
  ge.servers[GPID_FILE_BLOCK] = spawn_user_proc(&ge, "bin/bfs.exe", 0, NULL);

  /* Set the default file server.
   */
  ge.servers[GPID_FILE] = ge.servers[GPID_FILE_BLOCK];
  //	ge.servers[GPID_FILE] = ge.servers[GPID_FILE_RAM];
  ge.root.server = ge.servers[GPID_FILE];

  ge.servers[GPID_DIR] = spawn_user_proc(&ge, "bin/dirsvr.exe", 0, NULL);

  ge.cwd = ge.root;
  spawn_user_proc(&ge, "bin/init.exe", 0, NULL);

  printf("grass_main: initialization completed\n\r");

  /* Start the main loop.
   */
  proc_mainloop();
}

/* For debugging purposes.
 */
uint64_t checksum(char *data, unsigned int size) {
  uint64_t sum = 0;

  while (size--) {
    sum = (sum << 1) | ((sum >> 63) & 0x1); // rotate
    sum ^= *data++;
  }
  return sum;
}

void shut_down() {
  printf("\n\rShutting down\n\r");
  proc_shutdown();
}

struct earth_intf earth;

int main(void) {
  /* Copy the ``earth'' interface through which this kernel communicates
   * with the Earth virtual machine monitor.
   *
   * TODO.  May want to do something similar with GRASS_ENV in user
   *		  space processes to avoid Tiny C Compiler bug manifesting
   *itself.
   */
  memcpy(&earth, (void *)(KERN_TOP - sizeof(earth)), sizeof(earth));
  earth.log.init(); // for logging and printing

  grass_main();
  return 0;
}
