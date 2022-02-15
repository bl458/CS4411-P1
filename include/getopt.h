#ifndef EGOS_GETOPT_H
#define EGOS_GETOPT_H

extern char *optarg, *optplace;
extern int optind, optopt, opterr, optreset;
int getopt(int argc, char * const argv[], const char *optstring);

#endif // EGOS_GETOPT_H
