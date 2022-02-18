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
  void *arg;
} * thread_t;

typedef struct sema
{
  // your code here
} * sema_t;

struct queue;

// Global variables
thread_t current_thread;
struct queue *run_queue;

//Helpers
void ctx_entry()
{
  void (*f)(void *arg) = current_thread->function;
  f(current_thread->arg);
  thread_exit();
}

// Thread functions
void thread_init()
{
  current_thread = (thread_t)malloc(sizeof(struct thread));
  run_queue = (struct queue *)malloc(sizeof(struct queue));
  current_thread->base = NULL;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize)

{
  current_thread->status = 1;
  queue_add(run_queue, current_thread);
  thread_t new_thread = (thread_t)malloc(sizeof(struct thread));
  new_thread->status = 0;
  new_thread->base = malloc(stacksize);
  new_thread->stack_ptr = (int)new_thread->base + stacksize;
  new_thread->function = f;
  new_thread->arg = arg;
  void *old_ptr = current_thread->stack_ptr;
  current_thread = new_thread;
  ctx_start(&old_ptr, new_thread->stack_ptr);
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
  ctx_switch(&current_thread->stack_ptr, next_thread->stack_ptr);
  current_thread = next_thread;
}

void thread_exit()
{
  if (queue_size(run_queue) == 0)
  {
    free(current_thread);
    free(run_queue);
    exit(0);
  }

  current_thread->status = 2;
  thread_t old_thread = current_thread;
  thread_t next_thread = queue_get(run_queue);
  next_thread->status = 0;
  ctx_switch(current_thread->stack_ptr, next_thread->stack_ptr);
  current_thread = next_thread;
  free(old_thread->base);
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
  thread_exit();
  return 0;
}
