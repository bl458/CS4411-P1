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
enum { RUNNING, RUNNABLE, TERMINATED, BLOCKED };

typedef struct thread {
  int status;
  void *base;
  void *stack_ptr;
  void *function;
  void *arg;
} * thread_t;

typedef struct sema {
  int count;
  struct queue *blocked_queue;
} * sema_t;

struct queue;

typedef address_t;

/* Global variables */
thread_t current_thread;
thread_t zombie_thread;
struct queue *run_queue;
int num_blocked_threads;

void thread_exit();

/* Helpers */
void ctx_entry() {
  void (*f)(void *arg) = current_thread->function;
  f(current_thread->arg);
  thread_exit();
}

/* Thread functions */
void thread_release(thread_t thread) {
  if (thread->base != NULL) {
    free(thread->base);
  }

  free(thread);
}

void thread_init() {
  current_thread = (thread_t)malloc(sizeof(struct thread));
  zombie_thread = NULL;
  run_queue = (struct queue *)malloc(sizeof(struct queue));
  num_blocked_threads = 0;
  queue_init(run_queue);
  current_thread->base = NULL;
  current_thread->arg = 0;
}

void thread_create(void (*f)(void *arg), void *arg, unsigned int stacksize) {
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
  // printf("thread_create with old: %p, new: %p\n\n", old_thread->arg,
  // new_thread->arg);
  ctx_start((address_t *)&old_thread->stack_ptr,
            (address_t)new_thread->stack_ptr);

  if (zombie_thread != NULL) {
    thread_release(zombie_thread);
    zombie_thread = NULL;
  }
}

void thread_yield() {
  if (queue_size(run_queue) == 0) {
    if (current_thread->status == BLOCKED) {
      exit(0);
    }

    return;
  }

  thread_t old_thread = current_thread;
  thread_t popped_thread = queue_get(run_queue);

  if (old_thread->status != BLOCKED) {
    old_thread->status = RUNNABLE;
    queue_add(run_queue, old_thread);
  }
  popped_thread->status = RUNNING;

  current_thread = popped_thread;

  ctx_switch((address_t *)&old_thread->stack_ptr,
             (address_t)popped_thread->stack_ptr);

  if (zombie_thread != NULL) {
    thread_release(zombie_thread);
    zombie_thread = NULL;
  }
}

void thread_exit() {
  // printf("thread_exit with old: %p\n", current_thread->arg);
  if (queue_empty(run_queue)) {
    free(run_queue);
    if (num_blocked_threads > 0) {
      printf("Thread_exit called with blocked threads remaining\n");
      exit(1);
    }

    exit(0);
  }

  // printf("queue_get in thread_exit\n");
  thread_t popped_thread = queue_get(run_queue);
  zombie_thread = current_thread;
  zombie_thread->status = TERMINATED;
  popped_thread->status = RUNNING;
  // printf("thread_exit with old: %p, new: %p\n\n", zombie_thread->arg,
  // popped_thread->arg);
  current_thread = popped_thread;
  ctx_switch((address_t *)&zombie_thread->stack_ptr,
             (address_t)popped_thread->stack_ptr);
}

/* Sema functions */
void sema_init(struct sema *sema, unsigned int count) {
  sema->count = count;
  sema->blocked_queue = (struct queue *)malloc(sizeof(struct queue));
  queue_init(sema->blocked_queue);
}

void sema_dec(struct sema *sema) {
  if (sema->count == 0) {
    queue_add(sema->blocked_queue, current_thread);
    current_thread->status = BLOCKED;
    num_blocked_threads++;
    thread_yield();
    return;
  }

  sema->count--;
}

void sema_inc(struct sema *sema) {
  if (!queue_empty(sema->blocked_queue)) {
    // printf("queue_get in sema_inc\n");
    thread_t thread = queue_get(sema->blocked_queue);
    thread->status = RUNNABLE;
    num_blocked_threads--;
    queue_add(run_queue, thread);
    return;
  }

  sema->count++;
}

bool sema_release(struct sema *sema) {
  if (!queue_empty(sema->blocked_queue)) {
    return false;
  }

  free(sema->blocked_queue);
  // free(sema);
  return true;
}

/**** TEST SUITE ****/
/* Producer & Consumer implementation */
#define NSLOTS 3
static struct sema s_empty, s_full, s_lock;
static unsigned int in, out;
static char *slots[NSLOTS];
static void producer(void *arg) {
  unsigned int i;
  for (;;) {
    sema_dec(&s_empty);
    sema_dec(&s_lock);
    slots[in++] = arg;
    if (in == NSLOTS)
      in = 0;
    sema_inc(&s_lock);
    sema_inc(&s_full);
  }
}

static void consumer(void *arg) {
  unsigned int i;
  for (i = 0; i < 5; i++) {
    sema_dec(&s_full);
    sema_dec(&s_lock);
    void *x = slots[out++];
    printf("%s: got ’%s’\n", arg, x);
    if (out == NSLOTS)
      out = 0;
    sema_inc(&s_lock);
    sema_inc(&s_empty);
  }
}

void test_producer1() {
  printf("Producer, consumer1\n");
  sema_init(&s_lock, 1);
  sema_init(&s_full, 0);
  sema_init(&s_empty, NSLOTS);
  thread_create(producer, "producer 1", 16 * 1024);
  thread_create(consumer, "consumer 1", 16 * 1024);
  // consumer("consumer 1");
  printf("\n");

  sema_release(&s_empty);
  sema_release(&s_full);
  sema_release(&s_lock);
}

void test_producer2() {
  printf("Producer, consumer2\n");
  sema_init(&s_lock, 1);
  sema_init(&s_full, 0);
  sema_init(&s_empty, NSLOTS);
  thread_create(producer, "producer 1", 16 * 1024);
  thread_create(consumer, "consumer 1", 16 * 1024);
  // thread_create(consumer, "consumer 2", 16 * 1024);

  // consumer("consumer 1");
  consumer("consumer 2");
  printf("\n");

  sema_release(&s_empty);
  sema_release(&s_full);
  sema_release(&s_lock);
}

void test_producer3() {
  printf("Producer, consumer3\n");
  sema_init(&s_lock, 1);
  sema_init(&s_full, 0);
  sema_init(&s_empty, NSLOTS);
  thread_create(producer, "producer 1", 16 * 1024);
  // thread_create(producer, "producer 2", 16 * 1024);

  thread_create(consumer, "consumer 1", 16 * 1024);

  // consumer("consumer 1");
  producer("producer 2");
  printf("\n");

  sema_release(&s_empty);
  sema_release(&s_full);
  sema_release(&s_lock);
}

void test_producer4() {
  printf("Producer, consumer4\n");
  sema_init(&s_lock, 1);
  sema_init(&s_full, 0);
  sema_init(&s_empty, NSLOTS);
  thread_create(producer, "producer 1", 16 * 1024);
  // thread_create(producer, "producer 2", 16 * 1024);
  thread_create(consumer, "consumer 1", 16 * 1024);
  thread_create(consumer, "consumer 2", 16 * 1024);

  // consumer("consumer 1");
  // consumer("consumer 2");
  producer("producer 2");
  printf("\n");

  sema_release(&s_empty);
  sema_release(&s_full);
  sema_release(&s_lock);
}

/* Sleeping Barber Implementation */
static struct sema s_barber_ready, s_cust_waiting, s_seats;
static unsigned int free_seats = 4;

static void barber(void *arg) {
  while (true) {
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
  } else {
    // printf("%s, freeing seats when no more waiting space\n", arg);
    sema_inc(&s_seats);
    printf("%s left because no more waiting space\n", arg);
  }
}

// Releases the threads in blocked queue if there are threads blocked
// Only for preventing memory leak in testing
bool sema_release_robust(struct sema *sema) {
  while (queue_size(sema->blocked_queue) > 0) {
    thread_release(queue_get(sema->blocked_queue));
    num_blocked_threads--;
  }

  return sema_release(sema);
}

//# barber = 1, # customer = 1
void test_sleeping_barber1() {
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);

  while (queue_size(run_queue) > 0) {
    thread_yield();
  }

  if (current_thread->arg != 0) {
    printf("Current_thread was %p\n. Expected main", current_thread->arg);
    exit(1);
  }

  if (num_blocked_threads != 1) {
    printf("Number of blocked threads was %d\n. Expected 1",
           current_thread->arg);
    exit(1);
  }

  sema_release_robust(&s_barber_ready);
  sema_release_robust(&s_cust_waiting);
  sema_release_robust(&s_seats);
}

//# barber = 1, # customer = 2
void test_sleeping_barber2() {
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);
  thread_create(customer, "customer 2", 16 * 1024);

  while (queue_size(run_queue) > 0) {
    thread_yield();
  }

  // printf("Current_thread: %p\n", current_thread->arg);
  // printf("Number of blocked threads: %d\n", num_blocked_threads);

  printf("Released: %d\n", sema_release_robust(&s_barber_ready));
  printf("Released: %d\n", sema_release_robust(&s_cust_waiting));
  printf("Released: %d\n", sema_release_robust(&s_seats));

  // printf("Current_thread: %p\n", current_thread->arg);
  // printf("Number of blocked threads: %d\n", num_blocked_threads);
  // printf("All customers have cut their hair for test2\n\n");
}

//# barber = 1, # customers = # free seats
void test_sleeping_barber3() {
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(barber, "barber 1", 16 * 1024);
  thread_create(customer, "customer 1", 16 * 1024);
  thread_create(customer, "customer 2", 16 * 1024);
  thread_create(customer, "customer 3", 16 * 1024);
  thread_create(customer, "customer 4", 16 * 1024);

  while (queue_size(run_queue) > 0) {
    thread_yield();
  }

  // printf("Current_thread: %p\n", current_thread->arg);
  // printf("Number of blocked threads: %d\n", num_blocked_threads);

  printf("Released: %d\n", sema_release_robust(&s_barber_ready));
  printf("Released: %d\n", sema_release_robust(&s_cust_waiting));
  printf("Released: %d\n", sema_release_robust(&s_seats));

  // printf("Current_thread: %p\n", current_thread->arg);
  // printf("Number of blocked threads: %d\n", num_blocked_threads);
  // printf("All customers have cut their hair for test3\n\n");
}

//# barber = 1, # customers > # free seats = 4
void test_sleeping_barber4() {
  sema_init(&s_barber_ready, 1);
  sema_init(&s_cust_waiting, 0);
  sema_init(&s_seats, 1);
  thread_create(customer, "customer 1", 16 * 1024);
  thread_create(customer, "customer 2", 16 * 1024);
  thread_create(customer, "customer 3", 16 * 1024);
  thread_create(customer, "customer 4", 16 * 1024);
  thread_create(customer, "customer 5", 16 * 1024);
  thread_create(customer, "customer 6", 16 * 1024);
  thread_create(barber, "barber 1", 16 * 1024);

  while (queue_size(run_queue) > 0) {
    thread_yield();
  }

  // printf("Current_thread: %p\n", current_thread->arg);
  // printf("Number of blocked threads: %d\n", num_blocked_threads);

  printf("Released: %d\n", sema_release_robust(&s_barber_ready));
  printf("Released: %d\n", sema_release_robust(&s_cust_waiting));
  printf("Released: %d\n", sema_release_robust(&s_seats));

  // printf("Current_thread: %p\n", current_thread->arg);
  // printf("Number of blocked threads: %d\n", num_blocked_threads);
  // printf("All customers have cut their hair for test4\n\n");
}

void test_sleeping_barber() {
  test_sleeping_barber1();
  // test_sleeping_barber2();
  // test_sleeping_barber3();
  // test_sleeping_barber4();
}

int main(int argc, char **argv) {
  thread_init();
  // test_producer1();
  // test_producer2();
  // test_producer3();
  // test_producer4();
  test_sleeping_barber();
  thread_exit();
  return 0;
}
