#include <stdlib.h>

/* Exchange p and q in place.
 */
static void exch(char *p, char *q, size_t width){
	if (p == q) return;
	for (size_t i = 0; i < width; i++) p[i] ^= q[i];
	for (size_t i = 0; i < width; i++) q[i] ^= p[i];
	for (size_t i = 0; i < width; i++) p[i] ^= q[i];
}

void qsort(void *base, size_t nel, size_t width,
					 int (*compar)(const void *, const void *)){
	if (nel <= 1) {
		return;
	}

	/* First split the elements based on the pivot (the first element).
	 */
	char *pivot = base, *p = pivot;
	for (size_t i = 1; i < nel; i++) {
		p += width;
		int cmp = (*compar)(pivot, p);
		if (cmp >= 0) {
			exch(pivot, pivot + width, width);
			if (p != pivot + width) {
				exch(pivot, p, width);
			}
			pivot += width;
		}
	}

	/* Now recursively sort both parts
	 */
	int n = (pivot - (char *) base) / width;
	qsort(base, n, width, compar);
	if (nel - n > 1) {
		qsort(pivot + width, nel - n - 1, width, compar);
	}
}
