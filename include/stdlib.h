#ifndef _EGOS_STDLIB_H
#define _EGOS_STDLIB_H

#include <sys/types.h>
#include <egos/malloc.h>

int abs(int i);
long labs(long i);
char *getenv(const char *name);

int atoi(const char *s);
long atol(const char *str);
double atof(const char *str);

void srand(unsigned int seed);
int rand(void);
void abort(void);
void exit(int status);

void qsort(void *base, size_t nmemb, size_t size,
                  int (*compar)(const void *, const void *));

#endif // _EGOS_STDLIB_H
