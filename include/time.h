#ifndef EGOS_TIME_H
#define EGOS_TIME_H

#include <sys/types.h>

typedef long suseconds_t;

struct tm {
	int    tm_sec;   // seconds [0,61]
	int    tm_min;   // minutes [0,59]
	int    tm_hour;  // hour [0,23]
	int    tm_mday;  // day of month [1,31]
	int    tm_mon;   // month of year [0,11]
	int    tm_year;  // years since 1900
	int    tm_wday;  // day of week [0,6] (Sunday = 0)
	int    tm_yday;  // day of year [0,365]
	int    tm_isdst; // daylight savings flag
};

time_t time(time_t *tloc);
struct tm *localtime(const time_t *timep);
size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr);

#endif // EGOS_TIME_H
