/*
 * binary_search.c
 * Teaches: iterative binary search on a sorted int array, with safe
 * midpoint computation and an unambiguous bounds invariant.
 * Intermediate sample.
 * Build: gcc -Wall -o binary_search binary_search.c
 * Run:   ./binary_search
 */

#include <stdio.h>
#include <stddef.h>   /* for size_t */

/* Returns the index of `target` in the sorted array `data` of length `n`,
 * or -1 if it isn't present.
 *
 * Uses the half-open interval [lo, hi): valid candidates are at indices
 * lo through hi-1. This convention matches the iterator end-pointer
 * idiom in C++/STL and avoids the off-by-one bugs that the closed-form
 * [lo, hi] is prone to. */
static long binary_search(const int *data, size_t n, int target)
{
    size_t lo = 0;
    size_t hi = n;   /* one past the last valid index */

    while (lo < hi) {
        /* `lo + (hi - lo) / 2` is the overflow-safe midpoint. The
         * naive `(lo + hi) / 2` can overflow for very large arrays. */
        size_t mid = lo + (hi - lo) / 2;
        int midval = data[mid];

        if (midval == target) {
            return (long)mid;
        } else if (midval < target) {
            /* Target, if present, must be strictly to the RIGHT. */
            lo = mid + 1;
        } else {
            /* Target, if present, must be strictly LEFT of mid. */
            hi = mid;
        }
    }

    /* Loop exits with lo == hi: candidate window is empty. */
    return -1;
}

/* A small driver that calls binary_search on a few representative
 * inputs so the build also serves as a smoke test. */
int main(void)
{
    int sorted[] = {1, 3, 5, 7, 9, 11, 13, 17, 19, 23};
    size_t n = sizeof(sorted) / sizeof(sorted[0]);

    /* Probe a value that's present in the middle. */
    long idx = binary_search(sorted, n, 13);
    printf("13 -> index %ld\n", idx);   /* 6 */

    /* Probe boundary values. */
    idx = binary_search(sorted, n, 1);
    printf(" 1 -> index %ld\n", idx);   /* 0 */
    idx = binary_search(sorted, n, 23);
    printf("23 -> index %ld\n", idx);   /* 9 */

    /* Probe a missing value to confirm the -1 contract. */
    idx = binary_search(sorted, n, 4);
    printf(" 4 -> index %ld (expect -1)\n", idx);

    /* Probe an empty array — the function must handle n == 0. */
    idx = binary_search(sorted, 0, 13);
    printf("13 in empty -> index %ld (expect -1)\n", idx);

    return 0;
}
