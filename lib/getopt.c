#include <stdio.h>
#include <string.h>
#include <getopt.h>

char *optarg;
int optind = 1;
int optopt;	
int	opterr = 1;
int optreset;
char *optplace;		// keeps track of where we are in scanning argument

int getopt(int argc, char * const argv[], const char *optstring){
	/* Point optplace to the next option.
	 */
	if (optreset || optplace == 0) {
		optreset = 0;
		if (optind >= argc || argv[optind][0] != '-') {
			optplace = 0;
			return -1;
		}
		optplace = argv[optind] + 1;
	}

	/* Get the option.
	 */
	optopt = *optplace++;

	/* The '-' option marks the end of option parsing.
	 */
	if (optopt == '-') {		// --
		optind++;
		optplace = 0;
		return -1;
	}

	/* See if the option is specified in optstring.
	 */
	char *index;
	if ((index = strchr(optstring, optopt)) == 0) {
		if (opterr) {
			fprintf(stderr, "%s: illegal option '%c'\n", argv[0], optopt);
		}
		if (*optplace == 0) {
			optplace = 0;
			optind++;
		}
		return '?';
	}

	/* See if the option takes an argument.
	 */
	if (index[1] == ':') {
		if (*optplace != 0) {
			optarg = optplace;
		}
		else if (argc <= ++optind) {
			optplace = 0;
			optarg = 0;
			if (*optstring == ':') {
				return ':';
			}
			if (opterr) {
				fprintf(stderr, "%s: option '%s' requires an argument\n", argv[0], optopt);
			}
			return '?';
		}
	 	else {
			optarg = argv[optind];
		}
		optplace = 0;
		optind++;
	}
	else {
		optarg = 0;
		if (*optplace == 0) {
			optplace = 0;
			optind++;
		}
	}
	return optopt;
}
