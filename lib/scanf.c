#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

static int scan_d(const char **ps, int *result){
	const char *s = *ps;
	bool minus = false;

	/* Skip blanks.
	 */
	while (*s == ' ' || *s == '\t') {
		s++;
	}

	/* Parse optional sign.
	 */
	if (*s == '-') {
		minus = true;
		s++;
	}
	else if (*s == '+') {
		s++;
	}

	int total = 0;
	while ('0' <= *s && *s <= '9') {
		total *= 10;
		total += *s - '0';
		s++;
	}
	*result = minus ? -total : total;
	*ps = s;
	return 1;
}

int vsscanf(const char *s, const char *fmt, va_list ap){
	int count = 0;
	while (*fmt != 0 && *s != 0) {
		if (*fmt != '%' && *fmt == *s) {
			fmt++;
			s++;
			continue;
		}
		if (*++fmt == 0) {
			break;
		}
		switch(*fmt) {
		case 'd':
			if (scan_d(&s, (int *) va_arg(ap, int *))) {
				count++;
			}
			break;
		default:
			assert(0);
		}
		fmt++;
	}
	return count;
}

int sscanf(const char *s, const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	int r = vsscanf(s, fmt, ap);
	va_end(ap);
	return r;
}
