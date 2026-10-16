"""Structural tests for data/finance_corpus/.

Verifies the hand-authored CE-14 finance + economics curriculum:
  - index.json parses, version=1, has required keys.
  - Stage 2 has at least 5 lessons; Stage 3 has at least 6 lessons.
  - Every register declared in `registers` has at least one lesson.
  - No duplicate work IDs.
  - Every referenced lesson file exists, decodes as utf-8, has >= 400 words.
  - Each lesson has paragraph structure (>= 3 blank-line-separated chunks).
  - No markdown headers, no obvious placeholder text.
  - The `scripts/canonical_corpus` loader can ingest the corpus end-to-end.

Pure read-side. No daemon, no live brain.
"""

import json
import os
import re
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

CORPUS_ROOT = os.path.join(REPO, "data", "finance_corpus")

from scripts.canonical_corpus import (  # noqa: E402
    load_index,
    works_for_stage,
    iter_chunks,
    decode_with_fallback,
)


_PARA_BREAK = re.compile(r"\n\s*\n")
_MD_HEADER = re.compile(r"(?m)^#{1,6}\s")
_MD_BULLET = re.compile(r"(?m)^\s*[-*+]\s")
_PLACEHOLDER_PATTERNS = [
    re.compile(r"\bTODO\b"),
    re.compile(r"\bFIXME\b"),
    re.compile(r"\bXXX\b"),
    re.compile(r"\blorem ipsum\b", re.IGNORECASE),
    re.compile(r"\bplaceholder\b", re.IGNORECASE),
    re.compile(r"<<<"),
    re.compile(r">>>"),
]


def _word_count(text: str) -> int:
    return len(text.split())


def _file_entries(work):
    """Skip metadata-only entries that have files: [] (e.g. gutenberg refs)."""
    return [f for f in work.get("files", []) if f]


class TestFinanceCorpusIndex(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index_path = os.path.join(CORPUS_ROOT, "index.json")
        cls.assertTrue_path = os.path.isfile(cls.index_path)
        with open(cls.index_path, "r", encoding="utf-8") as f:
            cls.idx = json.load(f)

    def test_index_exists_and_parses(self):
        self.assertTrue(os.path.isfile(self.index_path),
                        f"missing {self.index_path}")
        # Loader's version check.
        idx = load_index(CORPUS_ROOT)
        self.assertEqual(idx["version"], 1)

    def test_required_top_level_keys(self):
        for key in ("version", "comment", "registers", "works"):
            self.assertIn(key, self.idx, f"index.json missing '{key}'")

    def test_registers_is_dict_with_descriptions(self):
        regs = self.idx["registers"]
        self.assertIsInstance(regs, dict)
        self.assertGreater(len(regs), 0)
        for name, desc in regs.items():
            self.assertIsInstance(name, str)
            self.assertIsInstance(desc, str)
            self.assertGreater(len(desc), 0,
                               f"register '{name}' has empty description")

    def test_works_is_nonempty_list(self):
        works = self.idx["works"]
        self.assertIsInstance(works, list)
        self.assertGreater(len(works), 0)

    def test_required_registers_present(self):
        required = {
            "money_basics", "personal_finance", "markets_intro",
            "investing_intro", "macro_intro",
            "risk_and_uncertainty", "ethics_finance",
        }
        declared = set(self.idx["registers"].keys())
        self.assertTrue(required.issubset(declared),
                        f"missing registers: {required - declared}")

    def test_stage_counts(self):
        idx = load_index(CORPUS_ROOT)
        s2 = works_for_stage(idx, 2)
        s3 = works_for_stage(idx, 3)
        self.assertGreaterEqual(len(s2), 5,
                                 f"need >=5 stage-2 lessons, got {len(s2)}")
        self.assertGreaterEqual(len(s3), 6,
                                 f"need >=6 stage-3 lessons, got {len(s3)}")

    def test_every_register_has_at_least_one_lesson(self):
        works = self.idx["works"]
        registers = set(self.idx["registers"].keys())
        used = {w["register"] for w in works}
        for reg in registers:
            self.assertIn(reg, used,
                          f"register '{reg}' has no lessons assigned")

    def test_no_duplicate_ids(self):
        ids = [w["id"] for w in self.idx["works"]]
        dupes = {i for i in ids if ids.count(i) > 1}
        self.assertEqual(dupes, set(), f"duplicate work IDs: {dupes}")

    def test_every_lesson_has_register_in_registers_dict(self):
        registers = set(self.idx["registers"].keys())
        for w in self.idx["works"]:
            self.assertIn(w["register"], registers,
                          f"work {w['id']} register '{w['register']}' not declared")

    def test_every_referenced_file_exists(self):
        for w in self.idx["works"]:
            for rel in _file_entries(w):
                path = os.path.join(CORPUS_ROOT, rel)
                self.assertTrue(os.path.isfile(path),
                                f"work {w['id']} references missing file: {rel}")

    def test_every_lesson_file_meets_word_minimum(self):
        for w in self.idx["works"]:
            for rel in _file_entries(w):
                path = os.path.join(CORPUS_ROOT, rel)
                text = decode_with_fallback(path)
                words = _word_count(text)
                self.assertGreaterEqual(
                    words, 400,
                    f"work {w['id']} file {rel}: only {words} words (need >=400)")

    def test_every_lesson_file_has_paragraph_structure(self):
        for w in self.idx["works"]:
            for rel in _file_entries(w):
                path = os.path.join(CORPUS_ROOT, rel)
                text = decode_with_fallback(path)
                paras = [p for p in _PARA_BREAK.split(text) if p.strip()]
                self.assertGreaterEqual(
                    len(paras), 3,
                    f"work {w['id']} file {rel}: only {len(paras)} paragraphs"
                    f" (need >=3 blank-line splits)")

    def test_no_markdown_headers_or_bullets(self):
        for w in self.idx["works"]:
            for rel in _file_entries(w):
                path = os.path.join(CORPUS_ROOT, rel)
                with open(path, "r", encoding="utf-8") as f:
                    raw = f.read()
                self.assertIsNone(
                    _MD_HEADER.search(raw),
                    f"work {w['id']} file {rel}: contains markdown header"
                    f" (lines starting with '#')")
                self.assertIsNone(
                    _MD_BULLET.search(raw),
                    f"work {w['id']} file {rel}: contains markdown bullet"
                    f" (lines starting with '-', '*', or '+')")

    def test_no_placeholder_text(self):
        for w in self.idx["works"]:
            for rel in _file_entries(w):
                path = os.path.join(CORPUS_ROOT, rel)
                with open(path, "r", encoding="utf-8") as f:
                    raw = f.read()
                for pat in _PLACEHOLDER_PATTERNS:
                    self.assertIsNone(
                        pat.search(raw),
                        f"work {w['id']} file {rel}: placeholder pattern"
                        f" '{pat.pattern}' found")

    def test_lesson_files_decode_as_utf8(self):
        # Hand-authored corpus must be clean utf-8, no latin-1 fallback needed.
        for w in self.idx["works"]:
            for rel in _file_entries(w):
                path = os.path.join(CORPUS_ROOT, rel)
                with open(path, "rb") as f:
                    raw = f.read()
                try:
                    raw.decode("utf-8")
                except UnicodeDecodeError as e:
                    self.fail(
                        f"work {w['id']} file {rel}: not valid utf-8: {e}")

    def test_loader_iter_chunks_works_end_to_end(self):
        """iter_chunks must produce nonempty content for at least one work."""
        idx = load_index(CORPUS_ROOT)
        any_chunks = False
        for w in idx["works"]:
            if not _file_entries(w):
                continue
            chunks = list(iter_chunks(w, CORPUS_ROOT, max_chars=1200))
            if chunks:
                any_chunks = True
                # Cumulative end_byte must be monotonically non-decreasing.
                ends = [end for end, _ in chunks]
                self.assertEqual(ends, sorted(ends),
                                 f"work {w['id']}: chunk offsets not monotonic")
                # Joined content must contain something nontrivial.
                joined = "".join(c for _, c in chunks)
                self.assertGreater(len(joined.strip()), 100)
        self.assertTrue(any_chunks,
                        "iter_chunks produced no chunks for any work")

    def test_weights_in_expected_band(self):
        """Stage 2 weights 0.9-1.0; Stage 3 weights 0.8-1.2 (per spec)."""
        for w in self.idx["works"]:
            if w.get("source") != "hand_authored":
                continue
            stage = int(w.get("stage", -1))
            weight = float(w.get("weight", 0))
            if stage == 2:
                self.assertGreaterEqual(
                    weight, 0.9,
                    f"work {w['id']}: stage-2 weight {weight} < 0.9")
                self.assertLessEqual(
                    weight, 1.0,
                    f"work {w['id']}: stage-2 weight {weight} > 1.0")
            elif stage == 3:
                self.assertGreaterEqual(
                    weight, 0.8,
                    f"work {w['id']}: stage-3 weight {weight} < 0.8")
                self.assertLessEqual(
                    weight, 1.2,
                    f"work {w['id']}: stage-3 weight {weight} > 1.2")


if __name__ == "__main__":
    unittest.main()
