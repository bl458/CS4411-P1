#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <egos/file.h>
#include <egos/memchan.h>

int printf(const char *fmt, ...){
	va_list ap;
	struct mem_chan *mc = mc_alloc();

	va_start(ap, fmt);
	mc_vprintf(mc, fmt, ap);
	va_end(ap);

	int _print_output(const char *buf, unsigned int size);
	int size = _print_output(mc->buf, mc->offset);

	mc_free(mc);
	return size;
}

int vsnprintf(char * restrict str, size_t size, const char * restrict fmt, va_list ap){
	struct mem_chan *mc = mc_alloc();

	mc_vprintf(mc, fmt, ap);

	int total = mc->offset;
	if (size > 0) {
		size--;			// leaf room for null byte
		if (size > mc->offset) {
			size = mc->offset;
		}
		memcpy(str, mc->buf, size);
		str[size] = 0;
	}

	mc_free(mc);
	return total;
}

int vsprintf(char * restrict str, const char * restrict fmt, va_list ap){
	struct mem_chan *mc = mc_alloc();

	mc_vprintf(mc, fmt, ap);

	int size = mc->offset;
	mc_putc(mc, 0);
	strcpy(str, mc->buf);

	mc_free(mc);
	return size;
}

int sprintf(char * restrict str, const char * restrict fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	int n = vsprintf(str, fmt, ap);
	va_end(ap);
	return n;
}

int snprintf(char * restrict str, size_t size, const char * restrict fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return n;
}


int vasprintf(char **strp, const char *fmt, va_list ap){
	struct mem_chan *mc = mc_alloc();

	mc_vprintf(mc, fmt, ap);
	int size = mc->offset;

	mc_putc(mc, 0);

	*strp = malloc(mc->offset);
	strcpy(*strp, mc->buf);

	mc_free(mc);
	return size;
}

int asprintf(char **strp, const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	int size = vasprintf(strp, fmt, ap);
	va_end(ap);

	return size;
}
