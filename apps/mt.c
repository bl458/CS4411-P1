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
  void *function;
} * thread_t;

typedef struct sema
{
  // your code here
} * sema_t;

struct queue;

// Global variables
thread_t init_thread;
thread_t current_thread;
struct queue *run_queue;

// Thread functions
void thread_init()
{
  current_thread = (thread_t)malloc(sizeof(struct thread));
  run_queue = (struct queue *)malloc(sizeof(struct queue));
  current_thread->base = NULL;
  current_thread->stack_ptr = &current_thread;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize)

{
  current_thread->status = 1;
  queue_add(run_queue, current_thread);
  thread_t new_thread = (thread_t)malloc(sizeof(struct thread));
  new_thread->function = f;
  new_thread->stack_ptr = malloc(stacksize);
  new_thread->status = 0;
  // TODO what is old and new sp?
  void *old_ptr = current_thread->stack_ptr;
  ctx_start(old_ptr, new_thread->stack_ptr);
  // TODO what is top and bottom?
  // TODO how do we execute f with arg?
  // TODO ctx_entry?
  current_thread = new_thread;
  f(arg);
}

void thread_yield()
{
  if (queue_size(run_queue) == 0)
  {
    return;
  }

  current_thread->status = 1;
  queue_add(run_queue, current_thread);
  thread_t next_thread = queue_get(run_queue);
  next_thread->status = 0;
  ctx_switch(current_thread->stack_ptr, next_thread->stack_ptr);
  current_thread = next_thread;
}

void thread_exit()
{
  if (queue_size(run_queue) == 0)
  {
    exit(0);
  }

  current_thread->status = 2;
  thread_t old_thread = current_thread;
  thread_t next_thread = queue_get(run_queue);
  next_thread->status = 0;
  ctx_switch(current_thread->stack_ptr, next_thread->stack_ptr);
  current_thread = next_thread;
  // TODO how do we clean up the old thread?
  free(old_thread);
}

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
