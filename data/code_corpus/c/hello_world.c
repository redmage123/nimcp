/*
 * hello_world.c
 * Teaches: #include, main(), printf, return 0, the role of stdio.h, and
 * the conventional shape of every C program. Beginner pedagogical sample.
 * Build: gcc -Wall -o hello_world hello_world.c
 * Run:   ./hello_world
 */

/* `#include <stdio.h>` makes printf and friends visible to the compiler.
 * Angle brackets mean "search the system include paths"; quotes would
 * mean "search the project's own include paths first". */
#include <stdio.h>

/* `main` is the program's entry point. The runtime calls it with argc
 * (count of command-line arguments) and argv (array of those argument
 * strings). We don't use them in this minimal example.
 *
 * The return type `int` is the program's exit status: 0 for success,
 * non-zero for failure. Shells use this to chain commands (&&, ||).
 */
int main(int argc, char *argv[])
{
    /* `printf` writes formatted text to standard output. The format
     * string controls what gets printed; "\n" is a newline character. */
    printf("Hello, world!\n");

    /* Show that we can use the arguments even though we don't here.
     * argc is always >= 1: argv[0] is the program name itself. */
    if (argc > 1) {
        printf("first argument was: %s\n", argv[1]);
    } else {
        printf("(no arguments passed)\n");
    }

    /* Returning 0 from main() signals "the program completed successfully"
     * to the operating system. */
    return 0;
}
