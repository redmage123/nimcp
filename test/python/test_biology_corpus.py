"""Unit tests for data/biology_corpus/ — CE-13 biology curriculum.

Mirrors the math/physics/chemistry corpus test pattern:
  * Index exists, parses, and matches the canonical_corpus loader's expectations.
  * Stage 2 has at least 5 lessons and stage 3 has at least 6.
  * Every register declared in `registers` has at least one work using it.
  * No duplicate work IDs.
  * Every referenced lesson file exists, decodes as utf-8, has at least 400 words,
    contains paragraph structure (>= 3 blank-line splits), no markdown headers,
    and no placeholder text.
  * Loader (scripts/canonical_corpus) can read the index and yield non-zero
    chunk counts for both stages.

Pure stdlib. No daemon. No live brain.
"""
import json
import os
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CORPUS = os.path.join(REPO, "data", "biology_corpus")
INDEX_PATH = os.path.join(CORPUS, "index.json")

sys.path.insert(0, os.path.join(REPO, "scripts"))


# ---------------------------------------------------------------------------
# Index structure
# ---------------------------------------------------------------------------

class TestIndex(unittest.TestCase):
    def setUp(self):
        self.assertTrue(os.path.isfile(INDEX_PATH),
                        f"missing index.json at {INDEX_PATH}")
        with open(INDEX_PATH, "r", encoding="utf-8") as f:
            self.idx = json.load(f)

    def test_required_top_level_keys(self):
        for key in ("version", "comment", "registers", "works"):
            self.assertIn(key, self.idx, f"index.json missing top-level key: {key}")

    def test_version_is_one(self):
        self.assertEqual(self.idx["version"], 1)

    def test_registers_match_spec(self):
        expected = {
            "cell_intro",
            "living_systems",
            "genetics_intro",
            "evolution_intro",
            "ecology_intro",
            "physiology_intro",
            "microbiology_intro",
        }
        self.assertEqual(set(self.idx["registers"].keys()), expected)

    def test_no_duplicate_ids(self):
        ids = [w["id"] for w in self.idx["works"]]
        self.assertEqual(len(ids), len(set(ids)),
                         f"duplicate work IDs in index.json: {ids}")

    def test_every_register_has_at_least_one_work(self):
        registers = set(self.idx["registers"].keys())
        used = {w["register"] for w in self.idx["works"]}
        missing = registers - used
        self.assertFalse(missing,
                         f"registers declared but unused: {sorted(missing)}")
        unknown = used - registers
        self.assertFalse(unknown,
                         f"works reference unknown registers: {sorted(unknown)}")

    def test_stage_counts(self):
        stage2 = [w for w in self.idx["works"] if w.get("stage") == 2]
        stage3 = [w for w in self.idx["works"] if w.get("stage") == 3]
        self.assertGreaterEqual(len(stage2), 5,
                                f"need >= 5 stage 2 lessons, got {len(stage2)}")
        self.assertGreaterEqual(len(stage3), 6,
                                f"need >= 6 stage 3 lessons, got {len(stage3)}")

    def test_required_per_work_fields(self):
        for w in self.idx["works"]:
            for key in ("id", "register", "title", "files", "language",
                        "stage", "weight"):
                self.assertIn(key, w,
                              f"work {w.get('id', '<unknown>')} missing key {key}")

    def test_weights_in_expected_band(self):
        for w in self.idx["works"]:
            wt = float(w["weight"])
            stage = w.get("stage")
            if stage == 2:
                self.assertGreaterEqual(wt, 0.9, f"{w['id']}: stage-2 weight {wt} < 0.9")
                self.assertLessEqual(wt, 1.0, f"{w['id']}: stage-2 weight {wt} > 1.0")
            elif stage == 3:
                self.assertGreaterEqual(wt, 0.8, f"{w['id']}: stage-3 weight {wt} < 0.8")
                self.assertLessEqual(wt, 1.2, f"{w['id']}: stage-3 weight {wt} > 1.2")


# ---------------------------------------------------------------------------
# Lesson files
# ---------------------------------------------------------------------------

_PLACEHOLDER_TOKENS = (
    "TODO",
    "FIXME",
    "XXX",
    "lorem ipsum",
    "Lorem ipsum",
    "<placeholder>",
    "[placeholder]",
)


def _is_markdown_header(line: str) -> bool:
    """Lines like '# foo', '## bar', '### baz' — raw-text lessons must not have them."""
    stripped = line.lstrip()
    if not stripped.startswith("#"):
        return False
    # Allow '#' inside text (e.g. '#5' or 'C#') only at non-line-start; here
    # we already know the (lstripped) line starts with '#'. Treat as a header
    # if followed by space, a hash, or end-of-line.
    after = stripped.lstrip("#")
    return after == "" or after.startswith(" ")


class TestLessonFiles(unittest.TestCase):
    def setUp(self):
        with open(INDEX_PATH, "r", encoding="utf-8") as f:
            self.idx = json.load(f)

    def _iter_referenced_lesson_files(self):
        for w in self.idx["works"]:
            files = w.get("files") or []
            # Skip metadata-only entries (gutenberg references etc.).
            if not files:
                continue
            for rel in files:
                yield w["id"], rel, os.path.join(CORPUS, rel)

    def test_every_referenced_file_exists(self):
        any_seen = False
        for wid, rel, path in self._iter_referenced_lesson_files():
            any_seen = True
            self.assertTrue(os.path.isfile(path),
                            f"work {wid} references missing file {rel}")
        self.assertTrue(any_seen, "no lesson files referenced from index.json")

    def test_files_are_utf8_decodable(self):
        for wid, rel, path in self._iter_referenced_lesson_files():
            with open(path, "rb") as f:
                raw = f.read()
            try:
                raw.decode("utf-8")
            except UnicodeDecodeError as e:  # pragma: no cover
                self.fail(f"{wid} ({rel}) is not valid utf-8: {e}")

    def test_files_have_min_word_count(self):
        for wid, rel, path in self._iter_referenced_lesson_files():
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            words = text.split()
            self.assertGreaterEqual(len(words), 400,
                                    f"{wid} ({rel}): only {len(words)} words "
                                    "(spec requires >= 400)")

    def test_files_have_paragraph_structure(self):
        for wid, rel, path in self._iter_referenced_lesson_files():
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            # Count blank-line-separated, non-empty paragraphs.
            paras = [p for p in text.split("\n\n") if p.strip()]
            self.assertGreaterEqual(
                len(paras), 4,
                f"{wid} ({rel}): only {len(paras)} paragraphs "
                "(spec requires >= 3 blank-line splits, i.e. >= 4 paragraphs)")

    def test_no_markdown_headers(self):
        for wid, rel, path in self._iter_referenced_lesson_files():
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            for i, line in enumerate(text.splitlines(), start=1):
                self.assertFalse(
                    _is_markdown_header(line),
                    f"{wid} ({rel}) line {i}: markdown header detected: {line!r}")

    def test_no_placeholder_text(self):
        for wid, rel, path in self._iter_referenced_lesson_files():
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            for token in _PLACEHOLDER_TOKENS:
                self.assertNotIn(
                    token, text,
                    f"{wid} ({rel}): placeholder token {token!r} found in lesson")


# ---------------------------------------------------------------------------
# Loader integration
# ---------------------------------------------------------------------------

class TestLoaderIntegration(unittest.TestCase):
    """Confirm the existing canonical_corpus loader handles the biology index."""

    def test_load_index_round_trip(self):
        from canonical_corpus import load_index
        idx = load_index(CORPUS)
        self.assertEqual(idx["version"], 1)
        self.assertIn("works", idx)
        self.assertGreater(len(idx["works"]), 0)

    def test_works_for_stage_returns_non_empty(self):
        from canonical_corpus import load_index, works_for_stage
        idx = load_index(CORPUS)
        s2 = works_for_stage(idx, 2)
        s3 = works_for_stage(idx, 3)
        self.assertGreater(len(s2), 0, "no stage-2 works found by loader")
        self.assertGreater(len(s3), 0, "no stage-3 works found by loader")

    def test_iter_chunks_yields_text(self):
        from canonical_corpus import load_index, works_for_stage, iter_chunks
        idx = load_index(CORPUS)
        for stage in (2, 3):
            for work in works_for_stage(idx, stage):
                if not work.get("files"):
                    continue
                got_chunk = False
                for end_byte, chunk in iter_chunks(work, CORPUS, max_chars=1200):
                    self.assertGreater(end_byte, 0)
                    self.assertTrue(chunk.strip(),
                                    f"empty chunk yielded by {work['id']}")
                    got_chunk = True
                    break  # only need to confirm at least one chunk
                self.assertTrue(got_chunk,
                                f"loader yielded no chunks for {work['id']}")


if __name__ == "__main__":
    unittest.main()
