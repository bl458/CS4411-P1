#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <earth/earth.h>
#include <earth/clock.h>

static struct timeval clock_start;

/* Get the time in milliseconds since the kernel booted.
 */
static unsigned long clock_now(void){
	struct timeval tv;

	gettimeofday(&tv, 0);
	tv.tv_sec -= clock_start.tv_sec;
	if (tv.tv_usec < clock_start.tv_usec) {
		tv.tv_usec += 1000000;
		tv.tv_sec--;
	}
	return 1000 * tv.tv_sec + tv.tv_usec / 1000;
}

/* Start a periodic timer.  Interval in milliseconds.
 */
static void clock_start_timer(unsigned int interval){
	struct itimerval it;

	it.it_value.tv_sec = interval / 1000;
	it.it_value.tv_usec = (interval % 1000) * 1000;
	it.it_interval = it.it_value;
	if (setitimer(ITIMER_REAL, &it, 0) != 0) {
		perror("clock_start_timer: setitimer");
		exit(1);
	}
}

static void clock_initialize(){
	gettimeofday(&clock_start, 0);
}

void clock_setup(struct clock_intf *ci){
	ci->now = clock_now;
	ci->start_timer = clock_start_timer;
	ci->initialize = clock_initialize;
}
