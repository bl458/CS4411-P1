#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <earth/earth.h>
#include <earth/intf.h>

static FILE *log_fd;

static void panic(const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "Unrecoverable error: ");
	fprintf(stderr, fmt, ap);
	fprintf(stderr, "\n\r");
	va_end(ap);
	exit(1);
}

/* Add an entry to the log file.
 */
static void log_p(const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	fprintf(log_fd, "%lu,", earth.clock.now());
	vfprintf(log_fd, fmt, ap);
	fprintf(log_fd, "\n");
	va_end(ap);
	fflush(log_fd);
}

/* Print to standard output.  Also add to log file.
 */
static void log_print(const char *s, unsigned int size){
	fprintf(log_fd, "%lu,print %.*s\n", earth.clock.now(), size, s);
	fflush(log_fd);

	fwrite(s, 1, size, stdout);
	fflush(stdout);
}

static void log_init(void){
	printf("earth: log_init (means O.S. is running now!)\n\r");
	setbuf(stdout, NULL);			// simplifies debugging
	log_fd = fopen("storage/log.txt", "w");
	setlinebuf(log_fd);
}

void log_setup(struct log_intf *li){
	li->panic = panic;
	li->p = log_p;
	li->print = log_print;
	li->init = log_init;
}
