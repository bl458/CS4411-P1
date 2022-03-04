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
thread_t zombie_thread;
struct queue *run_queue;
int num_blocked_threads;

void thread_exit();

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
  zombie_thread = NULL;
  run_queue = (struct queue *)malloc(sizeof(struct queue));
  num_blocked_threads = 0;
  queue_init(run_queue);
  current_thread->base = NULL;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize)
{
  thread_t old_thread = current_thread;
  old_thread->status = RUNNABLE;
  queue_add(run_queue, old_thread);
  thread_t new_thread = (thread_t)malloc(sizeof(struct thread));
  new_thread->status = RUNNING;
  new_thread->base = malloc(stacksize);
  new_thread->stack_ptr = new_thread->base + stacksize;
  new_thread->function = f;
  new_thread->arg = arg;
  current_thread = new_thread;
  // printf("thread_create with old: %p, new: %p\n\n", old_thread->arg, new_thread->arg);
  ctx_start((address_t*)&old_thread->stack_ptr, (address_t)new_thread->stack_ptr);
  if (zombie_thread != NULL)
  {
    free(zombie_thread->base);
    free(zombie_thread);
    zombie_thread = NULL;
  }
}

void thread_yield()
{
  if (queue_size(run_queue) == 0)
  {
    if (current_thread->status == BLOCKED) {
      // printf("thread_yield run_queue == 0 and %p was blocked\n", current_thread->arg);
      exit(0);
    }

    // printf("thread_yield run_queue == 0 returned\n");
    return;
  }

  // printf("thread_yield: run_queue was not empty\n");
  // printf("%p\n\n", current_thread->arg);
  if (current_thread->status != BLOCKED)
  {
    current_thread->status = RUNNABLE;
    queue_add(run_queue, current_thread);
  }

  thread_t popped_thread = queue_get(run_queue);
  if (popped_thread == NULL) {
    exit(0);
  } 

  popped_thread->status = RUNNING;
  // printf("queue_get in thread_yield: %p\n", popped_thread->arg);
  // printf("run queue_size in thread_yield: %d\n", queue_size(run_queue));
  thread_t old_thread = current_thread;
  old_thread->status = RUNNABLE;
  current_thread = popped_thread;
  // printf("thread_yield is called old: %p, new: %p\n\n", old_thread->arg, popped_thread->arg);
  ctx_switch((address_t*)&old_thread->stack_ptr, (address_t)popped_thread->stack_ptr);
  if (zombie_thread != NULL)
  {
    free(zombie_thread->base);
    free(zombie_thread);
    zombie_thread = NULL;
  }
}

void thread_exit()
{
  // printf("thread_exit with old: %p\n", current_thread->arg);
  if (queue_empty(run_queue))
  {
    free(current_thread);
    free(run_queue);
    if (num_blocked_threads > 0) {
      printf("Thread_exit called with a blocked thread remaining\n");
      exit(1);
    }

    exit(0);
  }

  // printf("queue_get in thread_exit\n");
  thread_t popped_thread = queue_get(run_queue);
  zombie_thread = current_thread; 
  zombie_thread->status = TERMINATED;
  popped_thread->status = RUNNING;
  // printf("thread_exit with old: %p, new: %p\n\n", zombie_thread->arg, popped_thread->arg);
  current_thread = popped_thread;
  ctx_switch((address_t*)&zombie_thread->stack_ptr, (address_t)popped_thread->stack_ptr);
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
    num_blocked_threads++;
    thread_yield();
    return;
  }

  sema->count--;
}

void sema_inc(struct sema *sema)
{
  if (!queue_empty(sema->blocked_queue))
  {
    // printf("queue_get in sema_inc\n");
    thread_t thread = queue_get(sema->blocked_queue);
    thread->status = RUNNABLE;
    num_blocked_threads--;
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
// static void test_code(void *arg){
//      int i;
//      for (i = 0; i < 10; i++) {
//          printf("%s here: %d\n", arg, i);
//          thread_yield();
// }
//      printf("%s done\n", arg);
//  }
//  int main(int argc, char **argv){
//      thread_init();
//      thread_create(test_code, "thread 1", 16 * 1024);
//      thread_create(test_code, "thread 2", 16 * 1024);
//      test_code("main thread");
//      thread_exit();
// return 0; }


#define NSLOTS 3
static struct sema s_empty, s_full, s_lock;
static unsigned int in, out;
static char *slots[NSLOTS];
static void producer(void *arg)
{
  for (;;)
  {
    // first make sure there’s an empty slot.
    sema_dec(&s_empty);
    // now add an entry to the queue
    sema_dec(&s_lock);
    slots[in++] = arg;
    if (in == NSLOTS)
      in = 0;
    sema_inc(&s_lock);
    // finally, signal consumers
    sema_inc(&s_full);
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

void test_producer() {
  thread_init();
  sema_init(&s_lock, 1);
  sema_init(&s_full, 0);
  sema_init(&s_empty, NSLOTS);
  thread_create(consumer, "consumer 1", 16 * 1024);
  producer("producer 1");
  thread_exit();
}

/* Sleeping Barber Implementation */
static struct sema s_barber_ready, s_cust_waiting, s_seats;
static unsigned int free_seats = 4;

static void barber(void *arg) {
  for(int i= 0; i < 3; i++) {
    // printf("%s, waiting for customer\n", arg);
    sema_dec(&s_cust_waiting);

    // printf("%s, locking seats\n", arg);
    sema_dec(&s_seats);

    free_seats++;

    // printf("%s, freeing seats\n", arg);
    sema_inc(&s_seats);

    // printf("%s, finishing cut\n", arg);
    sema_inc(&s_barber_ready);
  }
}

static void customer(void *arg) {
  // printf("%s, locking seats\n", arg);
  sema_dec(&s_seats);
  if (free_seats > 0) {
    free_seats--;

    // printf("%s, freeing seats in if\n", arg);
    sema_inc(&s_seats);

    // printf("%s, entering waiting room\n", arg);
    sema_inc(&s_cust_waiting);

    // printf("%s waiting for barber\n", arg);
    sema_dec(&s_barber_ready);
  }
  else {
    // printf("%s, freeing seats when no more waiting space\n", arg);
    sema_inc(&s_seats);
  }
}

//# barber = 1, # customer = 1
void test_sleeping_barber1() {
  thread_init();
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);

  while(queue_size(run_queue) > 0) {
    thread_yield();
  }

  printf("Current_thread: %p\n", current_thread->arg);
  printf("Number of blocked threads: %d\n", num_blocked_threads);
  printf("All customers have cut their hair for test1\n\n");
  // thread_exit();
}

//# barber = 1, # customer = 2
void test_sleeping_barber2() {
  thread_init();
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);
  thread_create(customer, "customer 2", 16 * 1024);

  while(queue_size(run_queue) > 0) {
    thread_yield();
  }

  //Expected: current thread: 0, 
  printf("Current_thread: %p\n", current_thread->arg);
  printf("Number of blocked threads: %d\n", num_blocked_threads);
  printf("All customers have cut their hair for test2\n\n");
  // thread_exit();
}

//# barber = 1, # customers = # free seats 
void test_sleeping_barber3() {
  thread_init();
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);
  thread_create(customer, "customer 2", 16 * 1024);
  thread_create(customer, "customer 3", 16 * 1024);
  thread_create(customer, "customer 4", 16 * 1024);

  while(queue_size(run_queue) > 0) {
    thread_yield();
  }

  printf("Current_thread: %p\n", current_thread->arg);
  printf("Number of blocked threads: %d\n", num_blocked_threads);
  printf("All customers have cut their hair for test3\n\n");
  // thread_exit();
}

//# barber = 1, # customers > # free seats 
void test_sleeping_barber4() {
  thread_init();
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);
  thread_create(customer, "customer 2", 16 * 1024);
  thread_create(customer, "customer 3", 16 * 1024);
  thread_create(customer, "customer 4", 16 * 1024);
  thread_create(customer, "customer 5", 16 * 1024);

  while(queue_size(run_queue) > 0) {
    thread_yield();
  }

  printf("Current_thread: %p\n", current_thread->arg);
  printf("Number of blocked threads: %d\n", num_blocked_threads);
  printf("All customers have cut their hair for test4\n\n");
  // thread_exit();
}

void test_sleeping_barber() {
  test_sleeping_barber1();
  test_sleeping_barber2();
  test_sleeping_barber3();
  test_sleeping_barber4();
}

int main(int argc, char **argv)
{
  // test_producer();
  test_sleeping_barber();
  thread_exit();
  return 0;
}

