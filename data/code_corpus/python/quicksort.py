# quicksort.py
# Teaches: classic quicksort via Lomuto partitioning, with a recursion
# trace and a non-mutating top-level wrapper. Intermediate sample.
# Run: python3 quicksort.py


def _partition(values, lo, hi):
    """Lomuto partition scheme.

    Choose the rightmost element as pivot, then rearrange `values[lo..hi]`
    in place so that everything <= pivot ends up to the left of its
    final position and everything > pivot ends up to the right.

    Returns the final index of the pivot.
    """
    pivot = values[hi]

    # `i` tracks the boundary between "known <= pivot" (indices lo..i)
    # and the still-unscanned region (i+1..j-1).
    i = lo - 1
    for j in range(lo, hi):
        if values[j] <= pivot:
            i += 1
            # Swap so values[j] (which is <= pivot) joins the left side.
            values[i], values[j] = values[j], values[i]

    # Place the pivot just past the left side; everything left of the
    # new pivot index is <= pivot, everything right is > pivot.
    values[i + 1], values[hi] = values[hi], values[i + 1]
    return i + 1


def _quicksort_in_place(values, lo, hi):
    """Recursively sort the inclusive range values[lo..hi]."""
    # Base case: 0 or 1 elements are already sorted.
    if lo >= hi:
        return

    p = _partition(values, lo, hi)

    # Recurse on the two sides AROUND the pivot — the pivot itself is
    # already in its final position.
    _quicksort_in_place(values, lo, p - 1)
    _quicksort_in_place(values, p + 1, hi)


def quicksort(values):
    """Return a new sorted list. Does NOT mutate the input.

    Wrapping the in-place recursion gives callers the simpler
    "function returns sorted output" mental model.
    """
    # Slice copies the list — caller's original is preserved.
    out = list(values)
    _quicksort_in_place(out, 0, len(out) - 1)
    return out


def main():
    cases = [
        [],                           # empty
        [42],                         # singleton
        [3, 1],                       # two elements
        [5, 2, 8, 1, 9, 3, 7, 4, 6],  # random small
        [1, 1, 1, 1, 1],              # all equal — pathological for naive
        [9, 8, 7, 6, 5, 4, 3, 2, 1],  # reverse-sorted
    ]

    for original in cases:
        sorted_out = quicksort(original)
        # `sorted()` is Python's built-in reference. We compare against
        # it as a quick correctness check.
        ok = sorted_out == sorted(original)
        print(f"{original}  ->  {sorted_out}   ok={ok}")


if __name__ == "__main__":
    main()
