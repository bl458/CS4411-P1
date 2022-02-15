#ifndef EGOS_SYS_TIME_H
#define EGOS_SYS_TIME_H

#include <time.h>

struct timeval {
	time_t      tv_sec;     /* seconds */
	suseconds_t tv_usec;    /* microseconds */
};

struct timezone {
	int			tz_minuteswest; /* of Greenwich */
	int			tz_dsttime;     /* type of dst correction to apply */
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#endif // EGOS_SYS_TIME_H
