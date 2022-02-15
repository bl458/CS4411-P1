#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define	ALIGN	sizeof(int)
#define	ALMSK	(ALIGN - 1)

void *memchr(const void *s, int c, size_t n){
	const unsigned char *u = s;

	while (n-- > 0) {
		if (*u == c) {
			return (void *) u;
		}
		u++;
	}
	return 0;
}

int memcmp(const void *s1, const void *s2, size_t n){
	const unsigned char *u1 = s1, *u2 = s2;

	while (n-- > 0) {
		if (*u1 != *u2) {
			return (int) *u1 - (int) *u2;
		}
		u1++; u2++;
	}
	return 0;
}

void *memset(void *b, int c, size_t n){
	char *d = b;

	/* We only optimize the common case when c == 0.
	 */
	if (c != 0) {
		while (n-- > 0) {
			*d++ = c & 0xFF;
		}
		return b;
	}

	/* Three phases:
	 *	Phase 1: align on integer alignment
	 *	Phase 2: fast copy one integer at a time.
	 *	Phase 3: clear last few remaining bytes if any.
	 */

	size_t cnt = (address_t) d & ALMSK;
	while (((address_t) d & ALMSK) != 0 && cnt-- > 0) {
		*d++ = 0;
		n--;
	}

	cnt = n / ALIGN;
	while (cnt-- > 0) {
		* (int *) d = 0;
		d += ALIGN;
	}

	cnt = n & ALMSK;
	while (cnt-- > 0) {
		*d++ = 0;
	}

	return b;
}

void *memmove(void *dst, const void *src, size_t n){
	if (n == 0 || dst == src) {
		return dst;
	}

	/* Three phases:
	 *	Phase 1: try to align to s and d on integer boundaries.
	 *	Phase 2: fast copy one integer at a time.
	 *	Phase 3: copy the last few remaining bytes if any.
	 * Copy backwards if the source address is before the destination.
	 * Copy forwards otherwise (because ranges may overlap).
	 */
	char *d = dst;
	const char *s = src;
	if ((address_t) s < (address_t) d) {
		s += n;
		d += n;
		if (((address_t) s & ALMSK) != 0 || ((address_t) d & ALMSK) != 0) {
			size_t cnt;

			if (((address_t) s & ALMSK) != ((address_t) d & ALMSK) || n <= ALIGN) {
				cnt = n;
				n = 0;
			}
			else {
				cnt = (address_t) s & ALMSK;
				n -= cnt;
			}
			do *--d = *--s; while (--cnt > 0);
		}

		while (n >= ALIGN) {
			n -= ALIGN;
			s -= ALIGN;
			d -= ALIGN;
			* (int *) d = * (int *) s;
		}

		while (n > 0) {
			*--d = *--s;
			--n;
		}
	}
	else {
		if (((address_t) s & ALMSK) != 0 || ((address_t) d & ALMSK) != 0) {
			size_t cnt;

			if (((address_t) s & ALMSK) != ((address_t) d & ALMSK) || n < ALIGN) {
				cnt = n;
				n = 0;
			}
			else {
				cnt = ALIGN - ((address_t) s & ALMSK);
				n -= cnt;
			}
			do *d++ = *s++; while (--cnt > 0);
		}

		while (n >= ALIGN) {
			* (int *) d = * (int *) s;
			s += ALIGN;
			d += ALIGN;
			n -= ALIGN;
		}

		while (n > 0) {
			*d++ = *s++;
			n--;
		}
	}
	return dst;
}

void *memcpy(void *dst, const void *src, size_t n){
	return memmove(dst, src, n);
}

char *strcat(char *s1, const char *s2){
	char *orig = s1;

	while (*s1 != 0) {
		s1++;
	}
	while ((*s1++ = *s2++) != 0)
		;
	return orig;
}

char *strncat(char *s1, const char *s2, size_t n){
	char *orig = s1;

	while (*s1 != 0) {
		s1++;
	}
	while (n--) {
		*s1++ = *s2;
		if (*s2++ == 0) {
			return orig;
		}
	}
	*s1 = 0;
	return orig;
}

char *strcpy(char *dst, const char *src){
	char *d = dst;

	while ((*dst++ = *src++) != 0)
		;
	return d;
}

char *strncpy(char *dst, const char *src, size_t n){
	char *d = dst;

	while (n-- > 0) {
		if ((*dst++ = *src++) == 0) {
			while (n-- > 0) {
				*dst++ = 0;
			}
			break;
		}
	}
	return d;
}

int strcmp(const char *s1, const char *s2){
	const unsigned char *u1 = (unsigned char *) s1, *u2 = (unsigned char *) s2;

	while (*u1 != 0 && *u2 != 0) {
		if (*u1 != *u2) {
			return (int) *u1 - (int) *u2;
		}
		u1++; u2++;
	}
	if (*u1 == 0 && *u2 == 0) {
		return 0;
	}
	return *u1 == 0 ? -1 : 1;
}

int strncmp(const char *s1, const char *s2, size_t n){
	const unsigned char *u1 = (unsigned char *) s1, *u2 = (unsigned char *) s2;

	while (n > 0) {
		if (*u1 == 0 && *u2 == 0) {
			return 0;
		}
		if (*u1 == 0) {
			return 1;
		}
		if (*u2 == 0) {
			return -1;
		}
		if (*u1 < *u2) {
			return -1;
		}
		if (*u1 > *u2) {
			return 1;
		}
		u1++; u2++; n--;
	}
	return 0;
}

size_t strlen(const char *s){
	size_t n = 0;

	while (*s++ != 0) {
		n++;
	}
	return n;
}

size_t strnlen(const char *s, size_t maxlen) {
	size_t n = 0;

	while (n < maxlen && *s++ != 0) {
		n++;
	}
	return n;
}

int atoi(const char *s){
	bool minus = false;

	/* Skip blanks.
	 */
	while (isspace(*s)) {
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
	while (isdigit(*s)) {
		total *= 10;
		total += *s - '0';
		s++;
	}
	return minus ? -total : total;
}

long atol(const char *s){
	bool minus = false;

	/* Skip blanks.
	 */
	while (isspace(*s)) {
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

	long total = 0;
	while (isdigit(*s)) {
		total *= 10;
		total += *s - '0';
		s++;
	}
	return minus ? -total : total;
}

double atof(const char *str){
	return strtod(str, 0);
}

long strtol(const char *s, char **endptr, int base){
	long sign = 1, n = 0;

	while (isspace(*s)) {
		s++;
	}
	if (*s == '+') {
		s++;
	}
	else if (*s == '-') {
		s++;
		sign = -1;
	}

	if (base == 0 || base == 16) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			s += 2;
			base = 16;
		}
	}
	if (base == 0 || base == 8) {
		if (s[0] == '0') {
			s += 1;
			base = 8;
		}
	}
	if (base == 0) {
		base = 10;
	}

	while (base > 10 ? isalnum(*s) : isdigit(*s)) {
		n *= base;
		if (isdigit(*s)) {
			n += *s - '0';
		}
		else if (islower(*s)) {
			n += *s - 'a' + 10;
		}
		else if (isupper(*s)) {
			n += *s - 'A' + 10;
		}
		s++;
	}
	if (endptr != 0) {
		*endptr = (char *) s;
	}
	return sign * n;
}

unsigned long int strtoul(const char *s, char **endptr, int base){
	bool minus = false;

	/* Skip blanks.
	 */
	while (isspace(*s)) {
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

	if (base == 0 || base == 16) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			s += 2;
			base = 16;
		}
	}
	if (base == 0 || base == 8) {
		if (s[0] == '0') {
			s += 1;
			base = 8;
		}
	}
	if (base == 0) {
		base = 10;
	}

	unsigned long total = 0;
	while (base > 10 ? isalnum(*s) : isdigit(*s)) {
		total *= base;
		if (isdigit(*s)) {
			total += *s - '0';
		}
		else if (islower(*s)) {
			total += *s - 'a' + 10;
		}
		else if (isupper(*s)) {
			total += *s - 'A' + 10;
		}
		s++;
	}

	if (endptr != 0) {
		*endptr = (char *) s;
	}

	if (minus) {
		return (unsigned long) -total;
	}
	return total;
}

unsigned long long int strtoull(const char *s, char **endptr, int base){
	bool minus = false;

	/* Skip blanks.
	 */
	while (isspace(*s)) {
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

	if (base == 0 || base == 16) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			s += 2;
			base = 16;
		}
	}
	if (base == 0 || base == 8) {
		if (s[0] == '0') {
			s += 1;
			base = 8;
		}
	}
	if (base == 0) {
		base = 10;
	}

	unsigned long long total = 0;
	while (base > 10 ? isalnum(*s) : isdigit(*s)) {
		total *= base;
		if (isdigit(*s)) {
			total += *s - '0';
		}
		else if (islower(*s)) {
			total += *s - 'a' + 10;
		}
		else if (isupper(*s)) {
			total += *s - 'A' + 10;
		}
		s++;
	}

	if (endptr != 0) {
		*endptr = (char *) s;
	}

	if (minus) {
		return (unsigned long long) -total;
	}
	return total;
}

long double strtold(const char *s, char **endptr){
	bool minus = false;

	/* Skip blanks.
	 */
	while (isspace(*s)) {
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

	int base = 10;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		base = 16;
	}
	else if (s[0] == '0') {
		s += 1;
		base = 8;
	}

	long double total = 0;
	while (base > 10 ? isalnum(*s) : isdigit(*s)) {
		total *= base;
		if (isdigit(*s)) {
			total += *s - '0';
		}
		else if (islower(*s)) {
			total += *s - 'a' + 10;
		}
		else if (isupper(*s)) {
			total += *s - 'A' + 10;
		}
		s++;
	}

	if (*s == '.') {
		s++;
		long double div = 10;
		while (base > 10 ? isalnum(*s) : isdigit(*s)) {
			if (isdigit(*s)) {
				total += (*s - '0') / div;
			}
			else if (islower(*s)) {
				total += (*s - 'a' + 10) / div;
			}
			else if (isupper(*s)) {
				total += (*s - 'A' + 10) / div;
			}
			div *= 10;
			s++;
		}
	}

	if (*s == 'e' || *s == 'E' || *s == 'p' || *s == 'P') {
		unsigned eb = (*s == 'e' || *s == 'E') ? 10 : 2;
		char *e;
		long exp = strtol(s + 1, &e, 0);
		s = e;
		if (exp > 0) {
			while (exp > 0) {
				total *= eb;
				exp--;
			}
		}
		else if (exp < 0) {
			while (exp < 0) {
				total /= eb;
				exp++;
			}
		}
		else {
			total = 1;
		}
	}

	if (endptr != 0) {
		*endptr = (char *) s;
	}

	return minus ? -total : total;
}

float strtof(const char *s, char **endptr){
	return (float) strtold(s, endptr);
}

double strtod(const char *s, char **endptr){
	return (double) strtold(s, endptr);
}

char *strchr(const char *s, int c){
	for (;;) {
		if (*s == c) {
			return (char *) s;
		}
		if (*s == 0) {
			return 0;
		}
		s++;
	}
}

char *strrchr(const char *s, int c){
	char *found = 0;

	for (;;) {
		if (*s == c) {
			found = (char *) s;
		}
		if (*s == 0) {
			return found;
		}
		s++;
	}
}

char *strstr(const char *s1, const char *s2){
	int n = strlen(s2);

	while (*s1 != 0) {
		if (strncmp(s1, s2, n) == 0) {
			return (char *) s1;
		}
		s1++;
	}
	return 0;
}

char *index(const char *s, int c){
	return strchr(s, c);
}

char *rindex(const char *s, int c){
	return strrchr(s, c);
}
