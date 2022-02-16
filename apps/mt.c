/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <egos/context.h>
#include <egos/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**** THREADS AND SEMAPHORES ****/
typedef struct thread
{
  /* Thread status 
     0 = Running, 1 = Runnable, 2 = Terminated
  */
  int *status;
  void *base;
  void *stack_ptr;
} * thread_t;

typedef struct sema
{
  // your code here
} * sema_t;

struct queue;

// Global variables
thread_t init_thread;
struct queue *run_queue;

// Thread functions
void thread_init()
{
  init_thread = (thread_t)malloc(sizeof(struct thread));
  run_queue = (struct queue *)malloc(sizeof(struct queue));
  init_thread->base = NULL;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize)
{
}

void thread_yield() {}

void thread_exit() {}

// Sema functions
void sema_init(struct sema *sema, unsigned int count) {}

void sema_dec(struct sema *sema) {}

void sema_inc(struct sema *sema) {}

bool sema_release(struct sema *sema) {}

/**** TEST SUITE ****/
int main(int argc, char **argv)
{
  thread_init();
  printf("########### \n");
  // your code here
  thread_exit();
  return 0;
}
