# binary_search.py
# Teaches: binary search both iteratively and recursively, with bounds
# reasoning and the half-open-interval invariant. Intermediate sample.
# Run: python3 binary_search.py


def binary_search_iterative(sorted_values, target):
    """Return the index of `target` in `sorted_values`, or -1 if absent.

    Uses the half-open interval [lo, hi): values at indices lo..hi-1 are
    still candidates. This convention avoids off-by-one bugs that the
    closed-interval [lo, hi] form is prone to.
    """
    lo = 0
    hi = len(sorted_values)  # one past the last valid index

    while lo < hi:
        # `(lo + hi) // 2` is fine in Python (no overflow), but the
        # `lo + (hi - lo) // 2` form is the textbook-portable version.
        mid = lo + (hi - lo) // 2
        midval = sorted_values[mid]

        if midval == target:
            return mid
        elif midval < target:
            # Target, if present, must be to the RIGHT of `mid`.
            lo = mid + 1
        else:
            # Target, if present, must be strictly LEFT of `mid`.
            hi = mid

    # Loop exits when lo == hi, meaning the candidate window is empty.
    return -1


def binary_search_recursive(sorted_values, target, lo=0, hi=None):
    """Recursive variant. Same contract as the iterative form above."""
    # Default `hi` is computed at call time, not at function-def time —
    # we pick it here so multi-call recursion sees the right value.
    if hi is None:
        hi = len(sorted_values)

    # Base case: empty window means "not found".
    if lo >= hi:
        return -1

    mid = lo + (hi - lo) // 2
    midval = sorted_values[mid]

    if midval == target:
        return mid
    elif midval < target:
        # Search the right half.
        return binary_search_recursive(sorted_values, target, mid + 1, hi)
    else:
        # Search the left half.
        return binary_search_recursive(sorted_values, target, lo, mid)


def main():
    values = [1, 3, 5, 7, 9, 11, 13, 17, 19, 23]

    # Element present.
    print("iter: 13 ->", binary_search_iterative(values, 13))   # 6
    print("rec : 13 ->", binary_search_recursive(values, 13))   # 6

    # Element absent.
    print("iter:  4 ->", binary_search_iterative(values, 4))    # -1
    print("rec :  4 ->", binary_search_recursive(values, 4))    # -1

    # Edge cases: first and last elements.
    print("iter:  1 ->", binary_search_iterative(values, 1))    # 0
    print("iter: 23 ->", binary_search_iterative(values, 23))   # 9


if __name__ == "__main__":
    main()
