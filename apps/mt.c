/* Threads and semaphores in user space.
 */

#include <assert.h>
#include <egos/context.h>
#include <egos/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**** THREADS AND SEMAPHORES ****/
/* Types */
// Thread status
enum
{
  RUNNING,
  RUNNABLE,
  TERMINATED,
  BLOCKED
};

typedef struct thread
{
  int status;
  void *base;
  void *stack_ptr;
  void *function;
  void *arg;
} * thread_t;

typedef struct sema
{
  int count;
  struct queue *blocked_queue;
} * sema_t;

struct queue;

/* Global variables */
thread_t current_thread;
thread_t next_thread;
struct queue *run_queue;

/* Helpers */
void ctx_entry()
{
  void (*f)(void *arg) = current_thread->function;
  f(current_thread->arg);
  thread_exit();
}

/* Thread functions */
void thread_init()
{
  current_thread = (thread_t)malloc(sizeof(struct thread));
  next_thread = (thread_t)malloc(sizeof(struct thread));
  run_queue = (struct queue *)malloc(sizeof(struct queue));
  queue_init(run_queue);
  current_thread->base = NULL;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize)

{
  current_thread->status = RUNNABLE;
  queue_add(run_queue, current_thread);
  thread_t new_thread = (thread_t)malloc(sizeof(struct thread));
  new_thread->status = RUNNING;
  new_thread->base = malloc(stacksize);
  new_thread->stack_ptr = new_thread->base + stacksize;
  new_thread->function = f;
  new_thread->arg = arg;
  next_thread = new_thread;
  thread_t old_thread = current_thread;
  current_thread = next_thread;
  ctx_start(&old_thread->stack_ptr, current_thread->stack_ptr);
  if (current_thread->status == TERMINATED)
  {
    free(current_thread->base);
    free(current_thread);
  }

  current_thread = next_thread;
}

void thread_yield()
{
  if (queue_size(run_queue) == 0)
  {
    return;
  }

  // TODO should this be before or after the switch?
  if (current_thread->status != BLOCKED)
  {
    current_thread->status = RUNNABLE;
    queue_add(run_queue, current_thread);
  }

  thread_t popped_thread = queue_get(run_queue);
  next_thread = popped_thread;
  ctx_switch(&current_thread->stack_ptr, popped_thread->stack_ptr);
  if (current_thread->status == TERMINATED)
  {
    free(current_thread->base);
    free(current_thread);
  }

  current_thread = next_thread;
}

void thread_exit()
{
  if (queue_empty(run_queue))
  {
    free(current_thread);
    free(run_queue);
    exit(0);
  }

  current_thread->status = TERMINATED;
  thread_t popped_thread = queue_get(run_queue);
  popped_thread->status = RUNNING;
  next_thread = popped_thread;
  ctx_switch(current_thread->stack_ptr, popped_thread->stack_ptr);
}

/* Sema functions */
void sema_init(struct sema *sema, unsigned int count)
{
  sema->count = count;
  sema->blocked_queue = (struct queue *)malloc(sizeof(struct queue));
  queue_init(sema->blocked_queue);
}

void sema_dec(struct sema *sema)
{
  if (sema->count == 0)
  {
    queue_add(sema->blocked_queue, current_thread);
    current_thread->status = BLOCKED;
    thread_yield();
    return;
  }

  sema->count--;
}

void sema_inc(struct sema *sema)
{
  if (!queue_empty(sema->blocked_queue))
  {
    thread_t thread = queue_get(sema->blocked_queue);
    thread->status = RUNNABLE;
    queue_add(run_queue, thread);
    return;
  }

  sema->count++;
}

bool sema_release(struct sema *sema)
{
  if (!queue_empty(sema->blocked_queue))
  {
    return false;
  }

  free(sema->blocked_queue);
  free(sema);
  return true;
}

/**** TEST SUITE ****/
// int main(int argc, char **argv)
// {
//   thread_init();
//   printf("########### \n");
//   thread_exit();
//   return 0;
// }

#define NSLOTS 3
static struct sema s_empty, s_full, s_lock;
static unsigned int in, out;
static char *slots[NSLOTS];
static void producer(void *arg)
{
  printf("Entered producer 1\n");
  for (;;)
  {
    printf("Before Sema_dec1\n");
    // first make sure there’s an empty slot.
    sema_dec(&s_empty);
    printf("Before Sema_dec2\n");
    // now add an entry to the queue
    sema_dec(&s_lock);
    printf("Before Sema_dec2\n");
    slots[in++] = arg;
    if (in == NSLOTS)
      in = 0;
    printf("Before sema_inc1\n");
    sema_inc(&s_lock);
    // finally, signal consumers
    printf("Before sema_inc2\n");
    sema_inc(&s_full);
    printf("\n");
  }
}
static void consumer(void *arg)
{
  unsigned int i;
  for (i = 0; i < 5; i++)
  {
    // first make sure there’s something in the buffer
    sema_dec(&s_full);
    // now grab an entry to the queue
    sema_dec(&s_lock);
    void *x = slots[out++];
    printf("%s: got ’%s’\n", arg, x);
    if (out == NSLOTS)
      out = 0;
    sema_inc(&s_lock);
    // finally, signal producers
    sema_inc(&s_empty);
  }
}
int main(int argc, char **argv)
{
  thread_init();
  sema_init(&s_lock, 1);
  sema_init(&s_full, 0);
  sema_init(&s_empty, NSLOTS);
  thread_create(consumer, "consumer 1", 16 * 1024);
  printf("Before producer 1\n");
  producer("producer 1");
  printf("After producer 1\n");
  thread_exit();
  return 0;
}
