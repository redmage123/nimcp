# basic_loops.py
# Teaches: for-loop with range(), while-loop with a counter, sum/product
# accumulators, and list comprehensions. Beginner pedagogical sample.
# Run: python3 basic_loops.py


def sum_first_n(n):
    """Return 1 + 2 + ... + n using a for-loop and an accumulator."""
    # `range(1, n + 1)` produces 1, 2, 3, ..., n. The upper bound is
    # exclusive, so we add 1 to include n.
    total = 0
    for i in range(1, n + 1):
        total = total + i  # equivalent to total += i
    return total


def product_first_n(n):
    """Return 1 * 2 * ... * n (factorial of n) with a while-loop."""
    # While-loops are useful when the termination condition is not a
    # simple count. Here we could use a for-loop, but we show the
    # while-loop pattern: initialize, test, mutate.
    result = 1
    i = 1
    while i <= n:
        result = result * i
        i = i + 1
    return result


def squares_up_to(n):
    """Return [1, 4, 9, ..., n*n] using a list comprehension."""
    # A list comprehension is a compact way to build a list. It reads
    # left-to-right: "for each x in this range, collect x*x".
    return [x * x for x in range(1, n + 1)]


def first_even_above(threshold, limit=1000):
    """Return the first even number strictly greater than `threshold`,
    searching from threshold+1 up to `limit`. Returns None on failure."""
    # `break` exits the enclosing loop early. `else` on a for-loop runs
    # ONLY when the loop completes without `break` — a useful Python
    # idiom for "search and report failure".
    for n in range(threshold + 1, limit + 1):
        if n % 2 == 0:
            return n
    return None


def main():
    # Demonstrate each helper.
    print("sum 1..10  =", sum_first_n(10))           # 55
    print("10!         =", product_first_n(10))      # 3628800
    print("squares 1..5=", squares_up_to(5))         # [1, 4, 9, 16, 25]
    print("first even >7 =", first_even_above(7))    # 8


if __name__ == "__main__":
    main()
