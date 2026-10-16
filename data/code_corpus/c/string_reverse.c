/*
 * string_reverse.c
 * Teaches: in-place reversal of a C string, the role of the trailing
 * '\0' terminator, two-pointer technique, and length-aware iteration.
 * Intermediate sample.
 * Build: gcc -Wall -o string_reverse string_reverse.c
 * Run:   ./string_reverse
 */

#include <stdio.h>
#include <string.h>

/* `string_length` counts bytes up to (but not including) the '\0'.
 * We re-implement strlen here for the teaching value; in real code
 * you'd just call strlen() from <string.h>. */
static size_t string_length(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* `string_reverse_in_place` flips `s` end-for-end in place.
 *
 * The two-pointer technique walks `i` forward from the start and `j`
 * backward from the end; on each step we swap their characters and
 * step both inward. They meet at the middle in O(n/2) iterations.
 *
 * The trailing '\0' is left at the end (we never touch it), so the
 * result is still a valid C string. */
static void string_reverse_in_place(char *s)
{
    if (s == NULL) {
        return;   /* defensive: don't crash on NULL */
    }

    size_t n = string_length(s);
    if (n < 2) {
        return;   /* zero- and one-character strings are their own reverse */
    }

    size_t i = 0;
    size_t j = n - 1;
    while (i < j) {
        char tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
        i++;
        j--;
    }
}

int main(void)
{
    /* A string literal "hello" placed in a char[] is a MUTABLE buffer
     * (the array is on the stack). Note that `char *p = "hello"` would
     * point at a read-only literal — modifying that is undefined
     * behavior. Always use char[] when you intend to mutate. */
    char buf[] = "Hello, world!";
    printf("before: %s\n", buf);

    string_reverse_in_place(buf);
    printf("after : %s\n", buf);

    /* Reverse it again to confirm we get the original back. */
    string_reverse_in_place(buf);
    printf("twice : %s\n", buf);

    /* Edge cases: empty and one-character strings should be untouched. */
    char empty[] = "";
    string_reverse_in_place(empty);
    printf("empty : '%s'\n", empty);

    char one[] = "x";
    string_reverse_in_place(one);
    printf("one   : '%s'\n", one);

    return 0;
}
