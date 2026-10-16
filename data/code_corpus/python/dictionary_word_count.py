# dictionary_word_count.py
# Teaches: read a file, tokenize, count with a dictionary, and sort by
# value descending. Demonstrates dict.get with default, str.lower, and
# the `sorted(..., key=..., reverse=True)` idiom. Intermediate sample.
# Run: python3 dictionary_word_count.py


import os
import re


# A minimal English stop-word list. Real applications would use a
# bigger one (or NLTK / spaCy), but for a teaching sample this is fine.
STOP_WORDS = {
    "the", "a", "an", "and", "or", "but", "of", "to", "in", "on",
    "is", "are", "was", "were", "be", "been", "it", "its", "this",
    "that", "with", "as", "for", "by", "at", "from",
}


def tokenize(text):
    """Lowercase, then split on any non-letter run.

    Returns a list of word tokens. Numbers and punctuation are dropped
    so "Don't" becomes ["don", "t"] — acceptable for a teaching demo.
    """
    # `re.findall` returns every match. r"[a-z]+" finds runs of one or
    # more lowercase letters. We lowercase first so the regex is simple.
    return re.findall(r"[a-z]+", text.lower())


def count_words(text, *, drop_stopwords=True, min_length=2):
    """Return a dict mapping word -> count."""
    counts = {}
    for token in tokenize(text):
        if drop_stopwords and token in STOP_WORDS:
            continue
        if len(token) < min_length:
            continue
        # `dict.get(key, default)` is the canonical "increment if exists,
        # else start at the default" pattern. Concise and dependency-free.
        counts[token] = counts.get(token, 0) + 1
    return counts


def top_n(counts, n):
    """Return the `n` most-frequent (word, count) tuples, ties broken
    alphabetically for deterministic output."""
    # `sorted()` is stable, so we sort first by word (asc) then by count
    # (desc). The final result is sorted by count desc, ties alphabetical.
    by_word = sorted(counts.items(), key=lambda kv: kv[0])
    by_count = sorted(by_word, key=lambda kv: kv[1], reverse=True)
    return by_count[:n]


def count_words_in_file(path, *, n=10):
    """Read `path` as UTF-8 text, count words, return top-`n` list."""
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    return top_n(count_words(text), n)


def main():
    # Build a temporary file in the current directory for a self-contained
    # demo. Real users would pass an existing file path.
    sample_path = "wordcount_sample.txt"
    sample_text = (
        "Once upon a time the cat sat on the mat.\n"
        "The cat watched the bird and the bird watched the cat.\n"
        "Eventually the cat slept, and the bird flew away.\n"
    )
    with open(sample_path, "w", encoding="utf-8") as f:
        f.write(sample_text)

    top = count_words_in_file(sample_path, n=5)
    print("top 5 words:")
    for word, count in top:
        print(f"  {word:<10} {count}")

    # Clean up to leave the cwd as we found it.
    if os.path.isfile(sample_path):
        os.remove(sample_path)


if __name__ == "__main__":
    main()
