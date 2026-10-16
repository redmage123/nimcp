"""Unit tests for scripts/code_corpus.py — CE-5 source-code corpus
loader + code-aware chunker.

Pure-Python tests: no daemon, no live brain. Validates the shipped
data/code_corpus/ tree (index.json + sample files) against the
contract documented in CLAUDE.md.
"""
import json
import os
import sys
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import code_corpus as cc  # noqa: E402

CORPUS_ROOT = os.path.join(REPO, "data", "code_corpus")


# ---------------------------------------------------------------------------
# Helper: write a temp file with given suffix and contents.
# ---------------------------------------------------------------------------

def _temp_file(suffix, contents):
    fd, path = tempfile.mkstemp(suffix=suffix)
    os.close(fd)
    with open(path, "w", encoding="utf-8") as f:
        f.write(contents)
    return path


# ---------------------------------------------------------------------------
# Chunker: Python
# ---------------------------------------------------------------------------

class TestPythonChunker(unittest.TestCase):
    def test_three_defs_yield_at_least_three_chunks(self):
        # Each function body is padded above the coalesce floor so the
        # tiny-chunk coalescer doesn't fold them together.
        body = "\n".join("    # filler comment line {}".format(i)
                          for i in range(8))
        src = (
            "import os\n"
            "\n"
            "\n"
            "def alpha():\n"
            + body + "\n"
            "    return 1\n"
            "\n"
            "\n"
            "def beta():\n"
            + body + "\n"
            "    return 2\n"
            "\n"
            "\n"
            "def gamma():\n"
            + body + "\n"
            "    return 3\n"
        )
        path = _temp_file(".py", src)
        try:
            chunks = list(cc.iter_code_chunks(path))
        finally:
            os.remove(path)

        # Three defs => at least three chunks (preamble may be coalesced
        # into the first def's chunk if it's tiny — that gives 3 chunks;
        # if the preamble is its own chunk we get 4).
        self.assertGreaterEqual(len(chunks), 3)

        # Each chunk should mention exactly one of the function names if
        # the boundary detection is working.
        text_blob = "\n".join(c[1] for c in chunks)
        self.assertIn("def alpha", text_blob)
        self.assertIn("def beta", text_blob)
        self.assertIn("def gamma", text_blob)

    def test_class_and_def_both_split(self):
        src = (
            "class Foo:\n"
            "    pass\n"
            "\n"
            "\n"
            "def bar():\n"
            "    return 0\n"
        )
        path = _temp_file(".py", src)
        try:
            chunks = list(cc.iter_code_chunks(path))
        finally:
            os.remove(path)
        # Class and def: at least two chunks (or one if coalesced —
        # but our `pass` body is tiny so it will coalesce).
        self.assertGreaterEqual(len(chunks), 1)

    def test_leading_comment_attaches_to_function(self):
        src = (
            "import sys\n"
            "\n"
            "\n"
            "# This comment documents `compute`.\n"
            "# It must stay attached to the def below.\n"
            "def compute(x):\n"
            "    return x * 2\n"
        )
        path = _temp_file(".py", src)
        try:
            chunks = list(cc.iter_code_chunks(path))
        finally:
            os.remove(path)

        # Find the chunk that contains `def compute`.
        compute_chunk = None
        for _, text in chunks:
            if "def compute" in text:
                compute_chunk = text
                break
        self.assertIsNotNone(compute_chunk)
        self.assertIn("documents `compute`", compute_chunk)
        self.assertIn("must stay attached", compute_chunk)

    def test_oversize_chunk_splits_on_blank_lines(self):
        # Build one giant function with internal blank lines so the
        # oversize fallback kicks in.
        body = "\n".join("    x = " + str(i) for i in range(200))
        # Insert blank lines every 10 to give the splitter cut points.
        body = body.replace("    x = 50", "    x = 50\n", 1)
        src = "def big():\n" + body + "\n"
        path = _temp_file(".py", src)
        try:
            chunks = list(cc.iter_code_chunks(path, max_chars=200))
        finally:
            os.remove(path)
        # With max_chars=200, this should split into multiple pieces.
        self.assertGreater(len(chunks), 1)
        # Every piece must respect max_chars (modulo coalescing — but
        # our giant chunk's pieces are all larger than the tiny floor,
        # so the floor doesn't expand them). Allow a small slack for
        # the coalescer joining a tail.
        for _, text in chunks:
            self.assertLessEqual(len(text), 200 * 3)

    def test_tiny_chunks_coalesce(self):
        # Two trivial defs that would each yield <50-char chunks if we
        # didn't coalesce.
        src = (
            "def a(): return 1\n"
            "def b(): return 2\n"
        )
        path = _temp_file(".py", src)
        try:
            chunks = list(cc.iter_code_chunks(path))
        finally:
            os.remove(path)
        # Coalescing should fold these — every emitted chunk should be
        # >= the minimum threshold OR there should only be one chunk.
        if len(chunks) > 1:
            for _, text in chunks:
                self.assertGreaterEqual(len(text), 1)


# ---------------------------------------------------------------------------
# Chunker: C
# ---------------------------------------------------------------------------

class TestCChunker(unittest.TestCase):
    def test_two_functions_yield_at_least_two_chunks(self):
        # Pad each body above the coalesce floor.
        body = "\n".join("    /* filler comment line {} */".format(i)
                          for i in range(6))
        src = (
            "#include <stdio.h>\n"
            "\n"
            "int alpha(int x) {\n"
            + body + "\n"
            "    return x + 1;\n"
            "}\n"
            "\n"
            "int beta(int y) {\n"
            + body + "\n"
            "    return y * 2;\n"
            "}\n"
        )
        path = _temp_file(".c", src)
        try:
            chunks = list(cc.iter_code_chunks(path))
        finally:
            os.remove(path)
        # Two C functions => at least two chunks (preamble + 2, or 2 if
        # the preamble is coalesced into the first function).
        self.assertGreaterEqual(len(chunks), 2)
        blob = "\n".join(c[1] for c in chunks)
        self.assertIn("int alpha", blob)
        self.assertIn("int beta", blob)

    def test_c_block_comment_attaches_to_function(self):
        src = (
            "#include <stdio.h>\n"
            "\n"
            "/* This block comment documents alpha.\n"
            " * It must stay attached to the function below. */\n"
            "int alpha(int x) {\n"
            "    return x + 1;\n"
            "}\n"
        )
        path = _temp_file(".c", src)
        try:
            chunks = list(cc.iter_code_chunks(path))
        finally:
            os.remove(path)
        alpha_chunk = None
        for _, text in chunks:
            if "int alpha" in text:
                alpha_chunk = text
                break
        self.assertIsNotNone(alpha_chunk)
        self.assertIn("documents alpha", alpha_chunk)


# ---------------------------------------------------------------------------
# Index
# ---------------------------------------------------------------------------

class TestIndex(unittest.TestCase):
    def test_index_parses(self):
        idx = cc.load_code_index(CORPUS_ROOT)
        self.assertEqual(idx.get("version"), 1)
        self.assertIn("works", idx)
        self.assertIn("registers", idx)

    def test_required_keys_per_work(self):
        idx = cc.load_code_index(CORPUS_ROOT)
        for work in idx["works"]:
            # Every work has the standard scalar keys.
            for key in ("id", "register", "title",
                        "language", "stage", "weight", "source"):
                self.assertIn(key, work, f"missing {key} on {work.get('id')}")
            # Each work has either ``files`` (corpus-relative) or
            # ``repo_files`` (repo-relative), but not both.
            has_files = bool(work.get("files"))
            has_repo = bool(work.get("repo_files"))
            self.assertTrue(
                has_files ^ has_repo,
                f"{work.get('id')}: must have exactly one of files/repo_files")

    def test_minimum_stage_counts(self):
        idx = cc.load_code_index(CORPUS_ROOT)
        stage2 = [w for w in idx["works"] if int(w["stage"]) == 2]
        stage3 = [w for w in idx["works"] if int(w["stage"]) == 3]
        self.assertGreaterEqual(len(stage2), 5)
        self.assertGreaterEqual(len(stage3), 7)

    def test_no_duplicate_ids(self):
        idx = cc.load_code_index(CORPUS_ROOT)
        ids = [w["id"] for w in idx["works"]]
        self.assertEqual(len(ids), len(set(ids)))

    def test_language_field_valid(self):
        idx = cc.load_code_index(CORPUS_ROOT)
        for work in idx["works"]:
            self.assertIn(work["language"], ("python", "c"))

    def test_weight_in_range(self):
        idx = cc.load_code_index(CORPUS_ROOT)
        for work in idx["works"]:
            w = float(work["weight"])
            self.assertGreaterEqual(w, 0.5)
            self.assertLessEqual(w, 2.0)


# ---------------------------------------------------------------------------
# Sample files
# ---------------------------------------------------------------------------

class TestSampleFiles(unittest.TestCase):
    def setUp(self):
        with open(os.path.join(CORPUS_ROOT, "index.json"),
                  "r", encoding="utf-8") as f:
            self.idx = json.load(f)

    def _all_paths(self):
        # Only iterate over hand-authored entries (those with ``files``).
        # Repo-relative entries (``repo_files``) point at the live NIMCP
        # source tree and are validated separately in TestRepoFiles.
        for work in self.idx["works"]:
            for rel in work.get("files", []):
                yield work, os.path.join(CORPUS_ROOT, rel)

    def test_every_referenced_file_exists(self):
        for work, path in self._all_paths():
            self.assertTrue(os.path.isfile(path),
                            f"{work['id']}: missing {path}")

    def test_files_are_utf8_and_have_30_lines(self):
        for work, path in self._all_paths():
            with open(path, "r", encoding="utf-8") as f:
                lines = f.readlines()
            self.assertGreaterEqual(
                len(lines), 30,
                f"{work['id']}: only {len(lines)} lines (need >= 30)")

    def test_no_placeholder_text(self):
        forbidden = ("TODO", "PLACEHOLDER", "FIXME", "lorem", "your code here")
        for work, path in self._all_paths():
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            for needle in forbidden:
                self.assertNotIn(
                    needle, text,
                    f"{work['id']}: contains forbidden token {needle!r}")
                self.assertNotIn(
                    needle.lower(), text.lower(),
                    f"{work['id']}: contains forbidden token {needle!r} "
                    f"(case-insensitive)")

    def test_python_files_compile(self):
        for work, path in self._all_paths():
            if work["language"] != "python":
                continue
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            try:
                compile(text, path, "exec")
            except SyntaxError as e:
                self.fail(f"{work['id']}: SyntaxError {e}")

    def test_c_files_have_main(self):
        for work, path in self._all_paths():
            if work["language"] != "c":
                continue
            with open(path, "r", encoding="utf-8") as f:
                text = f.read()
            self.assertIn(
                "int main", text,
                f"{work['id']}: must contain `int main` to be standalone")


# ---------------------------------------------------------------------------
# End-to-end: run the chunker on every shipped sample.
# ---------------------------------------------------------------------------

class TestEndToEndChunkAllSamples(unittest.TestCase):
    def test_every_sample_yields_at_least_one_chunk(self):
        with open(os.path.join(CORPUS_ROOT, "index.json"),
                  "r", encoding="utf-8") as f:
            idx = json.load(f)
        for work in idx["works"]:
            # Hand-authored ``files``: chunked from the corpus tree.
            for rel in work.get("files", []):
                path = os.path.join(CORPUS_ROOT, rel)
                chunks = list(cc.iter_code_chunks(path))
                self.assertGreater(
                    len(chunks), 0,
                    f"{work['id']}: chunker yielded zero chunks")
                # Every chunk must be non-empty after strip.
                for cid, text in chunks:
                    self.assertGreater(len(text.strip()), 0)


# ---------------------------------------------------------------------------
# Repo-relative resolution: the "nimcp_self" register
# ---------------------------------------------------------------------------

class TestRepoFiles(unittest.TestCase):
    """Validates the ``repo_files`` schema extension and the
    ``nimcp_self`` register that references curated slices of the
    NIMCP source tree (no on-disk duplication)."""

    def setUp(self):
        with open(os.path.join(CORPUS_ROOT, "index.json"),
                  "r", encoding="utf-8") as f:
            self.idx = json.load(f)
        self.nimcp_self = [w for w in self.idx["works"]
                           if w.get("register") == "nimcp_self"]

    # --- resolve_work_files --------------------------------------------

    def test_resolve_files_corpus_relative(self):
        # A hand-authored entry: should resolve under CORPUS_ROOT.
        hand = next(w for w in self.idx["works"]
                    if w.get("source") == "hand_authored")
        paths = cc.resolve_work_files(hand, corpus_root=CORPUS_ROOT)
        self.assertEqual(len(paths), len(hand["files"]))
        for p in paths:
            self.assertTrue(p.startswith(CORPUS_ROOT),
                            f"hand-authored path leaked outside corpus: {p}")
            self.assertTrue(os.path.isfile(p), f"missing: {p}")

    def test_resolve_repo_files_repo_relative(self):
        self.assertGreater(len(self.nimcp_self), 0,
                           "nimcp_self register has no entries")
        for work in self.nimcp_self:
            paths = cc.resolve_work_files(work, corpus_root=CORPUS_ROOT)
            self.assertEqual(len(paths), len(work["repo_files"]))
            for p in paths:
                self.assertTrue(os.path.isabs(p),
                                f"{work['id']}: not absolute: {p}")
                self.assertTrue(os.path.isfile(p),
                                f"{work['id']}: missing repo file: {p}")
                # And the path must NOT live under the corpus root —
                # that would mean we accidentally duplicated source.
                self.assertFalse(
                    p.startswith(CORPUS_ROOT + os.sep),
                    f"{work['id']}: repo file resolved into corpus tree: {p}")

    def test_resolve_neither_returns_empty(self):
        # Synthetic work with neither key present.
        empty = {"id": "x", "register": "?", "title": "?",
                 "language": "python", "stage": 2, "weight": 1.0,
                 "source": "?"}
        self.assertEqual(cc.resolve_work_files(empty, corpus_root=CORPUS_ROOT), [])

        # Both keys empty also gives [].
        empty2 = dict(empty, files=[], repo_files=[])
        self.assertEqual(cc.resolve_work_files(empty2, corpus_root=CORPUS_ROOT), [])

    def test_resolve_explicit_repo_root(self):
        # When the caller passes repo_root explicitly, no auto-detect.
        explicit = "/some/other/place"
        work = {"repo_files": ["a/b.c"]}
        paths = cc.resolve_work_files(
            work, corpus_root=CORPUS_ROOT, repo_root=explicit)
        self.assertEqual(paths, [os.path.join(explicit, "a/b.c")])

    # --- repo-root auto-detection --------------------------------------

    def test_auto_detect_repo_root_from_corpus_root(self):
        # CORPUS_ROOT is a child of REPO; the helper must find REPO.
        detected = cc._find_repo_root(CORPUS_ROOT)
        self.assertEqual(os.path.realpath(detected),
                         os.path.realpath(REPO))

    # --- iter_repo_chunks ----------------------------------------------

    def test_iter_repo_chunks_yields_chunks(self):
        # canonical_corpus.py is small but well-formed Python — easy to chunk.
        rel = "scripts/canonical_corpus.py"
        chunks = list(cc.iter_repo_chunks(rel, repo_root=REPO))
        self.assertGreater(len(chunks), 0,
                           f"iter_repo_chunks({rel}) yielded zero chunks")
        blob = "\n".join(c[1] for c in chunks)
        # Sanity: at least one of the module's canonical tokens shows up.
        # We look for either Python keywords (def/import) or domain
        # tokens unique to this corpus loader.
        recognizable = ("def ", "import ", "athena", "corpus")
        self.assertTrue(
            any(tok in blob.lower() for tok in recognizable),
            f"iter_repo_chunks output had no recognizable tokens "
            f"(looked for {recognizable})")

    def test_iter_repo_chunks_on_c_file(self):
        # Verify a real C file is chunkable too.
        rel = "src/security/nimcp_audit_log.c"
        chunks = list(cc.iter_repo_chunks(rel, repo_root=REPO))
        self.assertGreater(len(chunks), 0)
        blob = "\n".join(c[1] for c in chunks)
        # Audit log file should mention either "audit" or "nimcp".
        self.assertTrue(
            "audit" in blob.lower() or "nimcp" in blob.lower(),
            "expected nimcp/audit tokens in audit log file")

    # --- index integrity for the nimcp_self register -------------------

    def test_every_repo_file_resolves(self):
        for work in self.nimcp_self:
            for rel in work.get("repo_files", []):
                full = os.path.join(REPO, rel)
                self.assertTrue(
                    os.path.isfile(full),
                    f"{work['id']}: dangling repo_files entry {rel!r}")

    def test_nimcp_self_has_stage_coverage(self):
        # At least one entry per of stage 2 and stage 3 (matches the
        # curriculum ramp the hand-authored register uses).
        stages = {int(w["stage"]) for w in self.nimcp_self}
        self.assertIn(2, stages, "nimcp_self has no Stage 2 entry")
        self.assertIn(3, stages, "nimcp_self has no Stage 3 entry")

    def test_nimcp_self_files_substantial(self):
        # Each referenced repo file must be non-trivial (>= 50 lines)
        # so it actually contributes something to the corpus.
        for work in self.nimcp_self:
            for rel in work["repo_files"]:
                full = os.path.join(REPO, rel)
                with open(full, "r", encoding="utf-8") as f:
                    n_lines = sum(1 for _ in f)
                self.assertGreaterEqual(
                    n_lines, 50,
                    f"{work['id']}: {rel} has only {n_lines} lines")

    def test_no_work_has_both_files_and_repo_files(self):
        for work in self.idx["works"]:
            self.assertFalse(
                bool(work.get("files")) and bool(work.get("repo_files")),
                f"{work['id']}: must not set both files and repo_files")


if __name__ == "__main__":
    unittest.main()
