"""Unit tests for data/chemistry_corpus/ — CE-12 chemistry curriculum.

Validates the manifest schema, lesson coverage by stage and register, and
basic content quality (length, paragraph structure, no markdown, no
placeholders). Also exercises the canonical_corpus loader against the
chemistry corpus so that ingest-time failures get caught here.

Pure-stdlib tests. No daemon, no live brain.
"""
import json
import os
import re
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CORPUS = os.path.join(REPO, "data", "chemistry_corpus")
INDEX_PATH = os.path.join(CORPUS, "index.json")

# Make scripts/ importable for the loader smoke test.
sys.path.insert(0, os.path.join(REPO, "scripts"))


REQUIRED_REGISTERS = {
    "atoms_molecules",
    "states_of_matter",
    "chemical_reactions",
    "bonds_intro",
    "acids_bases",
    "organic_intro",
    "thermodynamics_chem",
}

PLACEHOLDER_PATTERNS = [
    re.compile(r"\blorem\b", re.IGNORECASE),
    re.compile(r"\bTODO\b"),
    re.compile(r"\bPLACEHOLDER\b", re.IGNORECASE),
    re.compile(r"\btk\b"),
]

MARKDOWN_HEADER_RE = re.compile(r"^\s*#")


def _load_index():
    with open(INDEX_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


def _read_lesson(rel_path):
    full = os.path.join(CORPUS, rel_path)
    with open(full, "rb") as f:
        raw = f.read()
    # utf-8 decoding: must succeed strictly. ASCII-clean lessons + a few
    # subscript glyphs are within UTF-8.
    return raw.decode("utf-8")


class TestIndexSchema(unittest.TestCase):
    def test_index_exists(self):
        self.assertTrue(os.path.isfile(INDEX_PATH),
                        f"missing manifest: {INDEX_PATH}")

    def test_index_parses_as_json(self):
        idx = _load_index()
        self.assertIsInstance(idx, dict)

    def test_index_has_required_keys(self):
        idx = _load_index()
        for key in ("version", "comment", "registers", "works"):
            self.assertIn(key, idx, f"manifest missing key: {key}")
        self.assertEqual(idx["version"], 1)
        self.assertIsInstance(idx["registers"], dict)
        self.assertIsInstance(idx["works"], list)

    def test_registers_cover_required_set(self):
        idx = _load_index()
        declared = set(idx["registers"].keys())
        missing = REQUIRED_REGISTERS - declared
        self.assertFalse(missing,
                         f"manifest missing registers: {sorted(missing)}")


class TestStageCoverage(unittest.TestCase):
    def test_at_least_five_stage_two_lessons(self):
        idx = _load_index()
        stage2 = [w for w in idx["works"] if int(w.get("stage", -1)) == 2]
        self.assertGreaterEqual(len(stage2), 5,
                                f"stage 2 has only {len(stage2)} works (need >=5)")

    def test_at_least_six_stage_three_lessons(self):
        idx = _load_index()
        stage3 = [w for w in idx["works"] if int(w.get("stage", -1)) == 3]
        self.assertGreaterEqual(len(stage3), 6,
                                f"stage 3 has only {len(stage3)} works (need >=6)")

    def test_every_register_has_at_least_one_lesson(self):
        idx = _load_index()
        used = {w.get("register") for w in idx["works"] if w.get("files")}
        for reg in REQUIRED_REGISTERS:
            self.assertIn(reg, used, f"register has no lesson: {reg}")


class TestUniqueness(unittest.TestCase):
    def test_no_duplicate_ids(self):
        idx = _load_index()
        ids = [w["id"] for w in idx["works"]]
        seen = set()
        dups = []
        for i in ids:
            if i in seen:
                dups.append(i)
            seen.add(i)
        self.assertFalse(dups, f"duplicate work ids: {dups}")

    def test_no_duplicate_file_paths(self):
        idx = _load_index()
        paths = []
        for w in idx["works"]:
            paths.extend(w.get("files", []))
        seen = set()
        dups = []
        for p in paths:
            if p in seen:
                dups.append(p)
            seen.add(p)
        self.assertFalse(dups, f"duplicate lesson paths: {dups}")


class TestLessonFiles(unittest.TestCase):
    """Iterate over works that ship lesson files (skip metadata-only entries
    like project gutenberg refs that may have files: [])."""

    def setUp(self):
        idx = _load_index()
        self.lesson_works = [w for w in idx["works"] if w.get("files")]
        self.assertTrue(self.lesson_works,
                        "no file-bearing lessons in the manifest")

    def test_every_referenced_file_exists(self):
        for w in self.lesson_works:
            for rel in w["files"]:
                full = os.path.join(CORPUS, rel)
                self.assertTrue(os.path.isfile(full),
                                f"missing lesson file: {full} (work {w['id']})")

    def test_every_lesson_is_utf8(self):
        for w in self.lesson_works:
            for rel in w["files"]:
                try:
                    _read_lesson(rel)
                except UnicodeDecodeError as e:
                    self.fail(f"non-utf-8 in {rel} (work {w['id']}): {e}")

    def test_every_lesson_has_at_least_400_words(self):
        for w in self.lesson_works:
            for rel in w["files"]:
                text = _read_lesson(rel)
                wc = len(text.split())
                self.assertGreaterEqual(
                    wc, 400,
                    f"{rel} has only {wc} words (need >=400)")

    def test_every_lesson_has_paragraph_structure(self):
        # At least 3 blank-line splits => at least 4 paragraph blocks.
        para_break = re.compile(r"\n\s*\n")
        for w in self.lesson_works:
            for rel in w["files"]:
                text = _read_lesson(rel)
                splits = para_break.split(text)
                # Count non-empty paragraphs.
                paras = [p for p in splits if p.strip()]
                self.assertGreaterEqual(
                    len(paras) - 1, 3,
                    f"{rel} has only {len(paras)} paragraphs "
                    f"(need >=4 paragraphs / >=3 blank-line splits)")

    def test_no_markdown_headers(self):
        for w in self.lesson_works:
            for rel in w["files"]:
                text = _read_lesson(rel)
                for n, line in enumerate(text.splitlines(), start=1):
                    self.assertFalse(
                        MARKDOWN_HEADER_RE.match(line),
                        f"markdown header at {rel}:{n}: {line!r}")

    def test_no_placeholder_text(self):
        for w in self.lesson_works:
            for rel in w["files"]:
                text = _read_lesson(rel)
                for pat in PLACEHOLDER_PATTERNS:
                    m = pat.search(text)
                    self.assertIsNone(
                        m,
                        f"placeholder {pat.pattern!r} in {rel}: matched {m.group(0)!r}"
                        if m else "")


class TestRegisterValidity(unittest.TestCase):
    def test_every_work_register_is_declared(self):
        idx = _load_index()
        declared = set(idx["registers"].keys())
        for w in idx["works"]:
            reg = w.get("register")
            self.assertIn(
                reg, declared,
                f"work {w.get('id')} uses undeclared register {reg!r}")


class TestLoaderCompatibility(unittest.TestCase):
    """Smoke-test the canonical_corpus loader against the chemistry corpus.
    This catches schema mismatches that would crash the live ingest path."""

    def test_loader_reads_manifest(self):
        from canonical_corpus import load_index, works_for_stage
        idx = load_index(CORPUS)
        self.assertEqual(idx["version"], 1)

        s2 = works_for_stage(idx, 2)
        s3 = works_for_stage(idx, 3)
        self.assertGreaterEqual(len(s2), 5)
        self.assertGreaterEqual(len(s3), 6)

    def test_loader_iter_chunks_yields_text(self):
        from canonical_corpus import load_index, iter_chunks
        idx = load_index(CORPUS)
        # Pick the first file-bearing work.
        work = next(w for w in idx["works"] if w.get("files"))
        chunks = list(iter_chunks(work, CORPUS, start_byte=0, max_chars=1200))
        self.assertGreater(len(chunks), 0,
                           f"iter_chunks yielded nothing for {work['id']}")
        # Each chunk is (end_byte, text). End byte strictly increases.
        last_end = 0
        for end, text in chunks:
            self.assertGreater(end, last_end)
            self.assertTrue(text.strip())
            last_end = end


if __name__ == "__main__":
    unittest.main()
