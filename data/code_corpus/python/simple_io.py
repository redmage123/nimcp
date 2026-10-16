# simple_io.py
# Teaches: read user input, parse and validate it, write a result to a
# file, and read it back. Beginner pedagogical sample on basic I/O.
# Run: python3 simple_io.py


def read_positive_int(prompt):
    """Prompt until the user enters a non-negative integer."""
    # `input()` reads a line from standard input and returns a string
    # WITHOUT the trailing newline. The user might type anything, so we
    # validate and re-prompt on bad input.
    while True:
        raw = input(prompt)
        try:
            value = int(raw)  # may raise ValueError if not numeric
        except ValueError:
            print("  not an integer, try again")
            continue
        if value < 0:
            print("  must be >= 0, try again")
            continue
        return value


def write_squares_to_file(path, n):
    """Write the squares 1..n, one per line, to `path`."""
    # `with open(...)` is the safe pattern: the file is automatically
    # closed when the block exits, even on exception. Mode "w" truncates
    # the file (creates it if missing).
    with open(path, "w", encoding="utf-8") as f:
        for i in range(1, n + 1):
            f.write(f"{i*i}\n")


def read_lines_from_file(path):
    """Return every line of `path` as a list of strings (no newlines)."""
    # Mode "r" is read-only. `f.readlines()` returns one entry per line
    # WITH the trailing "\n"; we strip it so callers see clean text.
    with open(path, "r", encoding="utf-8") as f:
        return [line.rstrip("\n") for line in f.readlines()]


def main():
    # `input()` blocks waiting for stdin, so this main() is interactive.
    # The helpers themselves are testable in isolation without stdin.
    n = read_positive_int("How many squares? ")
    out_path = "squares.txt"

    write_squares_to_file(out_path, n)
    print(f"wrote {n} squares to {out_path}")

    contents = read_lines_from_file(out_path)
    print("contents:")
    for line in contents:
        print(" ", line)


if __name__ == "__main__":
    main()
