"""Sanity tests for the CE-11 Physics curriculum corpus.

These tests validate:
  - the index.json file exists, parses, and has the expected top-level shape
  - lesson-stage and per-register coverage targets are met
  - every referenced lesson file exists, decodes as UTF-8, is non-trivial,
    has paragraph structure, and contains no markdown headers or placeholder
    text (lorem ipsum, TODO, FIXME, etc.)
  - metadata-only Gutenberg entries (empty files list) are tolerated and
    skipped by the file-iteration tests but still counted toward index-level
    invariants such as id uniqueness and register coverage

The tests are stdlib-only and do not load the brain or talk to a daemon.
They exist to keep the corpus from rotting silently between releases.
"""

import json
import os
import re
import unittest


REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ROOT = os.path.join(REPO, "data", "physics_corpus")
INDEX_PATH = os.path.join(ROOT, "index.json")


def _load_index():
    with open(INDEX_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def _file_backed_works(idx):
    """Works that point to at least one file on disk (skip metadata-only refs)."""
    return [w for w in idx.get("works", []) if w.get("files")]


class TestPhysicsCorpusIndex(unittest.TestCase):

    def test_index_exists_and_parses(self):
        self.assertTrue(os.path.isfile(INDEX_PATH),
                        f"index.json missing at {INDEX_PATH}")
        idx = _load_index()
        self.assertIsInstance(idx, dict)

    def test_index_has_required_top_level_keys(self):
        idx = _load_index()
        for key in ("version", "comment", "registers", "works"):
            self.assertIn(key, idx, f"top-level key '{key}' missing")
        self.assertEqual(idx["version"], 1)
        self.assertIsInstance(idx["registers"], dict)
        self.assertIsInstance(idx["works"], list)
        self.assertGreater(len(idx["works"]), 0,
                           "works[] must be non-empty")

    def test_at_least_5_stage2_lessons(self):
        idx = _load_index()
        stage2 = [w for w in idx["works"]
                  if int(w.get("stage", -1)) == 2 and w.get("files")]
        self.assertGreaterEqual(len(stage2), 5,
                                f"expected >=5 stage-2 file-backed lessons, got {len(stage2)}")

    def test_at_least_6_stage3_lessons(self):
        idx = _load_index()
        stage3 = [w for w in idx["works"]
                  if int(w.get("stage", -1)) == 3 and w.get("files")]
        self.assertGreaterEqual(len(stage3), 6,
                                f"expected >=6 stage-3 file-backed lessons, got {len(stage3)}")

    def test_each_register_has_at_least_one_lesson(self):
        idx = _load_index()
        registers = set(idx["registers"].keys())
        seen = set()
        for w in idx["works"]:
            reg = w.get("register")
            if reg is not None:
                seen.add(reg)
        missing = registers - seen
        self.assertFalse(missing,
                         f"registers with no lesson reference: {sorted(missing)}")

    def test_no_duplicate_ids(self):
        idx = _load_index()
        ids = [w.get("id") for w in idx["works"]]
        self.assertEqual(len(ids), len(set(ids)),
                         f"duplicate work ids: {[i for i in ids if ids.count(i) > 1]}")
        for wid in ids:
            self.assertIsInstance(wid, str)
            self.assertTrue(wid, "empty work id")

    def test_every_work_has_register_in_registers(self):
        idx = _load_index()
        registers = set(idx["registers"].keys())
        for w in idx["works"]:
            reg = w.get("register")
            self.assertIn(reg, registers,
                          f"work {w.get('id')} references unknown register '{reg}'")

    def test_every_work_has_valid_stage_and_weight(self):
        idx = _load_index()
        for w in idx["works"]:
            stage = w.get("stage")
            self.assertIsInstance(stage, int,
                                  f"work {w.get('id')} stage must be int")
            self.assertGreaterEqual(stage, 1)
            self.assertLessEqual(stage, 5)
            weight = w.get("weight")
            self.assertIsInstance(weight, (int, float),
                                  f"work {w.get('id')} weight must be numeric")
            self.assertGreater(float(weight), 0.0,
                               f"work {w.get('id')} weight must be positive")


class TestPhysicsCorpusFiles(unittest.TestCase):

    def test_every_referenced_file_exists(self):
        idx = _load_index()
        missing = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if not os.path.isfile(path):
                    missing.append(path)
        self.assertFalse(missing, f"missing lesson files: {missing}")

    def test_every_lesson_is_utf8_decodable(self):
        idx = _load_index()
        broken = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if not os.path.isfile(path):
                    continue
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        f.read()
                except UnicodeDecodeError as e:
                    broken.append((path, str(e)))
        self.assertFalse(broken, f"non-utf8 lesson files: {broken}")

    def test_every_lesson_is_non_empty(self):
        idx = _load_index()
        empty = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if os.path.isfile(path) and os.path.getsize(path) == 0:
                    empty.append(path)
        self.assertFalse(empty, f"empty lesson files: {empty}")

    def test_every_lesson_is_at_least_400_words(self):
        idx = _load_index()
        too_short = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if not os.path.isfile(path):
                    continue
                with open(path, "r", encoding="utf-8") as f:
                    text = f.read()
                count = len(text.split())
                if count < 400:
                    too_short.append((path, count))
        self.assertFalse(too_short,
                         f"lessons under 400 words: {too_short}")

    def test_every_lesson_has_paragraph_structure(self):
        idx = _load_index()
        flat = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if not os.path.isfile(path):
                    continue
                with open(path, "r", encoding="utf-8") as f:
                    text = f.read()
                # at least 3 paragraph breaks => at least 4 paragraphs
                breaks = re.findall(r"\n\s*\n", text)
                if len(breaks) < 3:
                    flat.append((path, len(breaks)))
        self.assertFalse(flat,
                         f"lessons with <3 paragraph breaks: {flat}")

    def test_no_lesson_has_markdown_headers(self):
        idx = _load_index()
        offending = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if not os.path.isfile(path):
                    continue
                with open(path, "r", encoding="utf-8") as f:
                    for lineno, line in enumerate(f, start=1):
                        stripped = line.lstrip()
                        if stripped.startswith("#"):
                            offending.append((path, lineno, line.rstrip()))
                            break
        self.assertFalse(offending,
                         f"lessons with markdown-style headers: {offending}")

    def test_no_lesson_has_lorem_or_placeholder(self):
        idx = _load_index()
        bad_markers = ("lorem ipsum", "lorem-ipsum", "TODO", "FIXME",
                       "XXX", "placeholder", "PLACEHOLDER")
        flagged = []
        for w in _file_backed_works(idx):
            for rel in w["files"]:
                path = os.path.join(ROOT, rel)
                if not os.path.isfile(path):
                    continue
                with open(path, "r", encoding="utf-8") as f:
                    text = f.read()
                for marker in bad_markers:
                    if marker in text or marker.lower() in text.lower():
                        flagged.append((path, marker))
                        break
        self.assertFalse(flagged,
                         f"lessons containing placeholder text: {flagged}")


if __name__ == "__main__":
    unittest.main()
