/*
 * array_basics.c
 * Teaches: declaring a fixed-size int array, iterating with a for-loop,
 * computing a sum, and doing a linear search. Beginner pedagogical sample.
 * Build: gcc -Wall -o array_basics array_basics.c
 * Run:   ./array_basics
 */

#include <stdio.h>

/* `sum_array` returns the sum of `n` ints starting at `data`.
 * In C, arrays "decay" to pointers when passed to a function, so we
 * also pass the length explicitly — the function cannot tell otherwise. */
int sum_array(const int *data, int n)
{
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += data[i];   /* data[i] is equivalent to *(data + i) */
    }
    return total;
}

/* `find_index` returns the position of `needle` in `data`, or -1 if
 * not present. This is a textbook linear search — O(n). */
int find_index(const int *data, int n, int needle)
{
    for (int i = 0; i < n; i++) {
        if (data[i] == needle) {
            return i;
        }
    }
    return -1;
}

int main(void)
{
    /* A fixed-size array. The size (5) is part of the type at compile
     * time. Once declared, you cannot resize it; for that you'd use
     * malloc/realloc and a heap pointer. */
    int values[5] = {3, 1, 4, 1, 5};

    /* `sizeof` operates on the array type, so this gives the byte size
     * of the whole array; dividing by sizeof(values[0]) recovers the
     * element count. This trick only works on real arrays, NOT on
     * pointers — which is why we passed `n` explicitly above. */
    int n = (int)(sizeof(values) / sizeof(values[0]));

    int total = sum_array(values, n);
    printf("sum = %d\n", total);   /* 14 */

    /* Linear search for a value that is present, and one that isn't. */
    int idx = find_index(values, n, 4);
    if (idx >= 0) {
        printf("found 4 at index %d\n", idx);
    } else {
        printf("4 not found\n");
    }

    idx = find_index(values, n, 99);
    if (idx >= 0) {
        printf("found 99 at index %d\n", idx);
    } else {
        printf("99 not found (as expected)\n");
    }

    return 0;
}
