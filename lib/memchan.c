#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <earth/earth.h>
#include <egos/memchan.h>

#ifdef x86_32
typedef int32_t int_t;
typedef uint32_t uint_t;
#endif
#ifdef x86_64
typedef int64_t int_t;
typedef uint64_t uint_t;
#endif

/* mc_vprintf needs to know when to stop looking for numbers after the
 * decimal point in a floating-point value. Usually it can stop at the 
 * format string's specified precision value, but if none is specified,
 * we need to pick a default. */
#define DEFAULT_FLOAT_PRECISION 6

/* Allocate a "memory channel".
 */
struct mem_chan *mc_alloc(){
	return (struct mem_chan *) calloc(1, sizeof(struct mem_chan));
}

/* Release a memory channel.
 */
void mc_free(struct mem_chan *mc){
	free(mc->buf);
	free(mc);
}

/* Append the given buffer of the given size to the memory channel.
 */
void mc_put(struct mem_chan *mc, char *buf, unsigned int size){
	if (mc->offset + size > mc->size) {
		mc->size = mc->size * 2;
		if (mc->offset + size > mc->size) {
			mc->size = mc->offset + size;
		}
		mc->buf = realloc(mc->buf, mc->size);
	}
	memcpy(mc->buf + mc->offset, buf, size);
	mc->offset += size;
}

/* Append a character to the memory channel.
 */
void mc_putc(struct mem_chan *mc, char c){
	mc_put(mc, &c, 1);
}

/* Append a null-terminated string (sans null character).
  */
void mc_puts(struct mem_chan *mc, char *s){
	while (*s != 0) {
		mc_putc(mc, *s++);
	}
}

/* Helper function for printing an unsigned integer in a particular base.
 * caps is true is we should use upper case hex characters.
 */
static void mc_unsigned_long_long(struct mem_chan *mc, unsigned long long d, unsigned int base, bool caps){
	char buf[64], *p = &buf[64];
	char *chars = caps ? "0123456789ABCDEF" : "0123456789abcdef";

	if (base < 2 || base > 16) {
		mc_puts(mc, "<bad base>");
		return;
	}
	if (d == 0) {
		mc_putc(mc, '0');
		return;
	}
	*--p = 0;
	while (d != 0) {
		*--p = chars[d % base];
		d /= base;
	}
	mc_puts(mc, p);
}

/* Helper function for printing a signed integer in a particular base.
 * caps is true is we should use upper case hex characters.
 */
static void mc_signed_long_long(struct mem_chan *mc, long long d, unsigned int base, bool caps, bool opt_plus, bool opt_space){
	char buf[64], *p = &buf[64];
	char *chars = caps ? "0123456789ABCDEF" : "0123456789abcdef";

	if (opt_plus) {
		if (d >= 0) {
			mc_putc(mc, '+');
		}
	}
	else if (opt_space) {
		if (d > 0) {
			mc_putc(mc, ' ');
		}
	}
	if (base < 2 || base > 16) {
		mc_puts(mc, "<bad base>");
		return;
	}
	if (d == 0) {
		mc_putc(mc, '0');
		return;
	}
	if (d < 0) {
		mc_putc(mc, '-');
		d = -d;
	}
	*--p = 0;
	while (d != 0) {
		*--p = chars[d % base];
		d /= base;
	}
	mc_puts(mc, p);
}

/* Helper function for printing a signed floating-point value.
 * Caps is true if the values inf and nan should be capitalized.
 * Precision is the precision value parsed from the format string.
 */
static void mc_signed_double(struct mem_chan *mc, double value, int precision, bool caps, bool opt_plus, bool opt_space) {

	if (opt_plus) {
		if (value >= 0) {
			mc_putc(mc, '+');
		}
	}
	else if (opt_space) {
		if (value > 0) {
			mc_putc(mc, ' ');
		}
	}
	if (value == 0) {
		mc_putc(mc, '0');
		if(precision > 0) {
			mc_putc(mc, '.');
			while(precision > 0) {
				mc_putc(mc, '0');
				precision--;
			}
		}
		return;
	}
	if (isnan(value)) {
		mc_puts(mc, caps ? "NAN" : "nan");
		return;
	}
	if (value < 0) {
		mc_putc(mc, '-');
		value = -value;
	}
	if (!isfinite(value)) {
		mc_puts(mc, caps ? "INF" : "inf");
	}
	if (precision == -1) {
		precision = DEFAULT_FLOAT_PRECISION;
	}

	double int_part;
	double frac_part = modf(value, &int_part);
	unsigned long long int_value = (unsigned long long) int_part;
	if(precision == 0) {
		mc_unsigned_long_long(mc, int_value, 10, caps);
		return;
	}

	/* This is an inefficient and potentially inaccurate way to print the 
	 * digits after the decimal point in an IEEE 754 floating-point number. 
	 * It should be improved if we ever get the time.
	 */
	unsigned long long decimal_digits = 0;
	int leading_zeros = 0;
	bool all_nines = true;
	for(int d = 0; d < precision; ++d) {
		/* 0 <= frac_part < 1, so multiply by 10 to get the next decimal digit in the ones position */
		frac_part *= 10;
		unsigned int digit = (unsigned int) frac_part;
		decimal_digits = decimal_digits * 10 + digit;
		/* Until the first nonzero decimal digit is found, decimal_digits won't change */
		if(decimal_digits == 0 && digit == 0) {
			leading_zeros++;
		}
		/* After the first nonzero digit is found, check for all-nines digits */
		if(decimal_digits != 0 && digit != 9) {
			all_nines = false;
		}
		/* Subtract off this digit so frac_part is again < 1 */
		frac_part -= digit;
	}
	/* Determine if we need to round up based on the next digit. 
	 * This can result in trailing zeros if the decimal digits were all 9. */
	int trailing_zeros = 0;
	if ((unsigned int) (frac_part * 10) >= 5) {
		decimal_digits++;
		if(all_nines) {
			leading_zeros--;
		}
		if(leading_zeros < 0) {
			int_value++;
			decimal_digits = 0;
			trailing_zeros = precision;
		}
	}

	mc_unsigned_long_long(mc, int_value, 10, caps);
	mc_putc(mc, '.');
	for(int z = 0; z < leading_zeros; ++z) {
		mc_putc(mc, '0');
	}
	if(decimal_digits != 0) {
		mc_unsigned_long_long(mc, decimal_digits, 10, caps);
	}
	for(int z = 0; z < trailing_zeros; ++z) {
		mc_putc(mc, '0');
	}

}

/* Version of vprintf() that appends to a memory channel.
 */
void mc_vprintf(struct mem_chan *mc, const char *fmt, va_list ap){
	while (*fmt != 0) {
		if (*fmt != '%') {
			mc_putc(mc, *fmt++);
			continue;
		}
		bool opt_zero_padding = false;
		bool opt_left_adjust = false;
		bool opt_plus = false;
		bool opt_space = false;
		for (;;) {
			if (*++fmt == '0') {
				opt_zero_padding = true;
			}
			else if (*fmt == '-') {
				opt_left_adjust = true;
			}
			else if (*fmt == '+') {
				opt_plus = true;
			}
			else if (*fmt == ' ') {
				opt_space = true;
			}
			else {
				break;
			}
		}
		unsigned int min_field_width = 0;
		while (isdigit(*fmt)) {
			min_field_width *= 10;
			min_field_width += *fmt - '0';
			fmt++;
		}
		int precision;
		if (*fmt == '.') {
			if (*++fmt == '*') {
				precision = va_arg(ap, unsigned int);
				fmt++;
			}
			else {
				precision = 0;
				while (isdigit(*fmt)) {
					precision *= 10;
					precision += *fmt - '0';
					fmt++;
				}
			}
		}
		else {
			precision = -1;
		}
		int modifier = 0;
		if (strncmp(fmt, "hh", 2) == 0) {
			modifier = 'H';
			fmt += 2;
		}
		else if (strncmp(fmt, "ll", 2) == 0) {
			modifier = 'L';
			fmt += 2;
		}
		else if (*fmt == 'h' || *fmt == 'l' || *fmt == 'j' || *fmt == 't' || *fmt == 'z' || *fmt == 'L') {
			modifier = *fmt;
			fmt += 1;
		}
		if (*fmt == 0) {
			break;
		}

		/* Print into a separate buffer first.
		 */
		struct mem_chan *mc2 = mc_alloc();
		switch(*fmt) {
		case 'c':
			mc_putc(mc2, va_arg(ap, int));
			break;
		case 'd': case 'i':
			mc_signed_long_long(mc2, (long long) va_arg(ap, int), 10, false, opt_plus, opt_space);
			break;
		case 'D':
			mc_signed_long_long(mc2, (long long) va_arg(ap, long), 10, false, opt_plus, opt_space);
			break;
		case 's':
			mc_puts(mc2, va_arg(ap, char *));
			break;
		case 'u':
			mc_unsigned_long_long(mc2, (unsigned long long) va_arg(ap, unsigned int), 10, false);
			break;
		case 'U':
			mc_unsigned_long_long(mc2, (unsigned long long) va_arg(ap, unsigned long), 10, false);
			break;
		case 'o':
			mc_unsigned_long_long(mc2, (unsigned long long) va_arg(ap, unsigned int), 8, false);
			break;
		case 'O':
			mc_unsigned_long_long(mc2, (unsigned long long) va_arg(ap, unsigned long), 8, false);
			break;
		case 'x':
			mc_unsigned_long_long(mc2, (uint_t) va_arg(ap, unsigned int), 16, false);
			break;
		case 'X':
			mc_unsigned_long_long(mc2, (uint_t) va_arg(ap, unsigned long), 16, true);
			break;
		case 'p':
			mc_unsigned_long_long(mc2, (unsigned long long) va_arg(ap, void *), 16, false);
			break;
		case 'f':
			mc_signed_double(mc2, va_arg(ap, double), precision, false, opt_plus, opt_space);
			break;
		case 'F':
			mc_signed_double(mc2, va_arg(ap, double), precision, true, opt_plus, opt_space);
			break;
		default:
			mc_putc(mc2, *fmt);
		}
		
		/* Now copy into the main buffer, observing formatting options.
		 */

		if (precision < 0) {
			precision = mc2->offset;
		}
		else if ((int) mc2->offset < precision) {
			precision = mc2->offset;
		}

		if (!opt_left_adjust) {
			while ((int) min_field_width > precision) {
				mc_putc(mc, opt_zero_padding ? '0' : ' ');
				min_field_width--;
			}
		}

		/* For integral types, precision limits the size of the output here,
		 * but for floating-point types it doesn't.*/
		if (*fmt == 'f' || *fmt == 'F') {
			mc_put(mc, mc2->buf, mc2->offset);
		}
		else {
			mc_put(mc, mc2->buf, precision);
		}

		while ((int) min_field_width > precision) {
			mc_putc(mc, ' ');
			min_field_width--;
		}

		mc_free(mc2);
		fmt++;
	}
}

/* Version of printf() that adds to a memory channel.
 */
void mc_printf(struct mem_chan *mc, const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	mc_vprintf(mc, fmt, ap);
	va_end(ap);
}
