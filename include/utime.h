#ifndef EGOS_UTIME_H
#define EGOS_UTIME_H

#include <sys/types.h>

struct utimbuf {
	time_t actime;
	time_t modtime;
};

#endif // EGOS_UTIME_H
