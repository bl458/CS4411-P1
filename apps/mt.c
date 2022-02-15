/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <egos/context.h>
#include <egos/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**** THREADS AND SEMAPHORES ****/
struct thread {
  // your code here
};

struct sema {
  // your code here
};

// Global variables

// Thread functions
void thread_init() {}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize) {}

void thread_yield() {}

void thread_exit() {}

// Sema functions
void sema_init(struct sema *sema, unsigned int count) {}

void sema_dec(struct sema *sema) {}

void sema_inc(struct sema *sema) {}

bool sema_release(struct sema *sema) {}

/**** TEST SUITE ****/
int main(int argc, char **argv) {
  thread_init();
  // your code here
  thread_exit();
  return 0;
}
