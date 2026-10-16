"""Unit tests for scripts/canonical_corpus.py — pure read-side loader.

These tests build a synthetic mini-corpus in a tempdir, exercise every
public helper, and tear down. No daemon, no live brain.
"""

import json
import os
import sys
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

from scripts.canonical_corpus import (  # noqa: E402
    load_index,
    works_for_stage,
    iter_chunks,
    decode_with_fallback,
    round_robin_schedule,
)


def _build_mini(root: str) -> str:
    """Build a 2-work synthetic corpus + index.json. Returns root path."""
    os.makedirs(os.path.join(root, "alpha", "one"))
    os.makedirs(os.path.join(root, "beta", "two"))
    with open(os.path.join(root, "alpha", "one", "one.txt"), "w") as f:
        f.write("Alpha one paragraph.\n\nAlpha second paragraph here.\n\n"
                "Alpha third paragraph contains more words than the others.\n")
    with open(os.path.join(root, "beta", "two", "two.txt"), "w") as f:
        f.write("Beta intro.\n\nBeta middle.\n\nBeta tail end.\n")
    idx = {
        "version": 1,
        "works": [
            {"id": "alpha.one", "author": "Alpha", "author_slug": "alpha",
             "title": "One", "work_slug": "one",
             "files": ["alpha/one/one.txt"],
             "language": "en", "register": "test", "year": 2026,
             "public_domain": True, "source": "synthetic",
             "stage": 1, "weight": 1.0},
            {"id": "beta.two", "author": "Beta", "author_slug": "beta",
             "title": "Two", "work_slug": "two",
             "files": ["beta/two/two.txt"],
             "language": "en", "register": "test", "year": 2026,
             "public_domain": True, "source": "synthetic",
             "stage": 2, "weight": 1.5},
        ],
    }
    with open(os.path.join(root, "index.json"), "w") as f:
        json.dump(idx, f)
    return root


class TestCanonicalCorpusLoader(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = _build_mini(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_load_index_version_check(self):
        idx = load_index(self.root)
        self.assertEqual(idx["version"], 1)
        self.assertEqual(len(idx["works"]), 2)

        bad = os.path.join(self.tmp.name, "bad")
        os.makedirs(bad)
        with open(os.path.join(bad, "index.json"), "w") as f:
            json.dump({"version": 99, "works": []}, f)
        with self.assertRaises(ValueError):
            load_index(bad)

    def test_works_for_stage_filters(self):
        idx = load_index(self.root)
        s1 = works_for_stage(idx, 1)
        s2 = works_for_stage(idx, 2)
        s3 = works_for_stage(idx, 3)
        self.assertEqual([w["id"] for w in s1], ["alpha.one"])
        self.assertEqual([w["id"] for w in s2], ["beta.two"])
        self.assertEqual(s3, [])

    def test_iter_chunks_full_pass(self):
        idx = load_index(self.root)
        work = idx["works"][0]
        chunks = list(iter_chunks(work, self.root, max_chars=200))
        self.assertGreater(len(chunks), 0)
        # Cumulative end_byte must be monotonically non-decreasing.
        ends = [end for end, _ in chunks]
        self.assertEqual(ends, sorted(ends))
        # Concatenated chunk text must reproduce the source (with the
        # whitespace-only chunks discarded by the loader, but content lines
        # preserved verbatim).
        joined = "".join(c for _, c in chunks)
        self.assertIn("Alpha one paragraph.", joined)
        self.assertIn("Alpha third paragraph", joined)

    def test_iter_chunks_resume_correctness(self):
        idx = load_index(self.root)
        work = idx["works"][0]
        all_chunks = list(iter_chunks(work, self.root, max_chars=200))
        # Pick a midpoint cursor.
        midpoint = all_chunks[0][0]
        resumed = list(iter_chunks(work, self.root,
                                    start_byte=midpoint, max_chars=200))
        # Resumed chunks should match the remainder of all_chunks.
        self.assertEqual([c for _, c in resumed],
                         [c for _, c in all_chunks[1:]])

    def test_iter_chunks_missing_file_is_noop(self):
        idx = load_index(self.root)
        # Point at a file that doesn't exist.
        work = dict(idx["works"][0])
        work["files"] = ["alpha/one/missing.txt"]
        self.assertEqual(list(iter_chunks(work, self.root)), [])

    def test_decode_with_fallback_handles_latin1(self):
        # Build a byte sequence that is INVALID utf-8 but valid latin-1.
        # 0xE9 alone is a continuation byte in utf-8 (raises) but is "é"
        # in latin-1; that's the exact case Project Gutenberg files hit.
        path = os.path.join(self.tmp.name, "latin1.txt")
        with open(path, "wb") as f:
            f.write(b"caf\xe9 r\xe9sum\xe9")
        text = decode_with_fallback(path)
        self.assertIn("café", text)
        self.assertIn("résumé", text)

    def test_decode_with_fallback_normalizes_smart_punctuation(self):
        path = os.path.join(self.tmp.name, "smart.txt")
        with open(path, "w", encoding="utf-8") as f:
            f.write("“Hello,” he said — ‘test’.")
        text = decode_with_fallback(path)
        self.assertIn('"Hello,"', text)
        self.assertIn(" -- ", text)
        self.assertIn("'test'", text)

    def test_round_robin_respects_weight(self):
        idx = load_index(self.root)
        works = [idx["works"][0], idx["works"][1]]  # weights 1.0 and 1.5
        sched = round_robin_schedule(works, total_chunks=20)
        self.assertEqual(len(sched), 20)
        # weight 1.5 work should appear more often than weight 1.0 work.
        c_alpha = sched.count("alpha.one")
        c_beta = sched.count("beta.two")
        self.assertGreater(c_beta, c_alpha)
        # Both must appear (interleaved, not batched).
        self.assertGreater(c_alpha, 0)
        # No work should occupy a contiguous run longer than 4 chunks
        # (sanity bound on weighted interleave).
        max_run = 1
        cur_run = 1
        for i in range(1, len(sched)):
            if sched[i] == sched[i - 1]:
                cur_run += 1
                max_run = max(max_run, cur_run)
            else:
                cur_run = 1
        self.assertLessEqual(max_run, 4)

    def test_round_robin_empty_input(self):
        self.assertEqual(round_robin_schedule([], 10), [])


if __name__ == "__main__":
    unittest.main()
