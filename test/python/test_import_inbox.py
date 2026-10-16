"""Unit tests for tools/import_inbox.py — canonical corpus inbox importer.

Builds a synthetic corpus root + inbox in a tempdir, drives the importer
through every supported path (single-file, files_glob, idempotent re-import,
dry-run, malformed meta), and tears down. No daemon, no network.
"""

import json
import os
import sys
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

from tools.import_inbox import (  # noqa: E402
    MetaError,
    import_inbox,
    validate_meta,
)


def _good_meta(**overrides) -> dict:
    base = {
        "author": "J.R.R. Tolkien",
        "author_slug": "tolkien",
        "title": "The Hobbit",
        "work_slug": "the_hobbit",
        "year": 1937,
        "language": "en",
        "register": "epic_fantasy",
        "stage": 3,
        "weight": 1.5,
        "public_domain": False,
        "source": "user_supplied",
    }
    base.update(overrides)
    return base


def _seed_index(root: str) -> str:
    """Create an empty (but versioned) index.json at root."""
    path = os.path.join(root, "index.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"version": 1, "works": []}, f)
    return path


def _drop(inbox: str, stem: str, text: str, meta: dict) -> None:
    """Drop a (.txt, .meta.json) pair into the inbox."""
    with open(os.path.join(inbox, stem + ".txt"), "w", encoding="utf-8") as f:
        f.write(text)
    with open(os.path.join(inbox, stem + ".meta.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f)


def _drop_meta_only(inbox: str, stem: str, meta: dict) -> str:
    path = os.path.join(inbox, stem + ".meta.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(meta, f)
    return path


def _silent(*_args, **_kwargs):
    pass


class TestValidateMeta(unittest.TestCase):
    def test_accepts_minimal_required(self):
        m = validate_meta({
            "author": "X", "author_slug": "x",
            "title": "Y", "work_slug": "y",
            "year": 2000, "language": "en",
            "register": "test", "stage": 1,
        })
        # Defaults applied:
        self.assertEqual(m["weight"], 1.0)
        self.assertEqual(m["public_domain"], False)
        self.assertEqual(m["source"], "user_supplied")

    def test_missing_required_field_rejected(self):
        bad = _good_meta()
        del bad["author_slug"]
        with self.assertRaises(MetaError) as cm:
            validate_meta(bad)
        self.assertIn("author_slug", str(cm.exception))

    def test_wrong_type_rejected(self):
        bad = _good_meta(year="1937")  # string instead of int
        with self.assertRaises(MetaError):
            validate_meta(bad)

    def test_bool_not_accepted_for_int_year(self):
        bad = _good_meta(year=True)
        with self.assertRaises(MetaError):
            validate_meta(bad)

    def test_bad_slug_rejected(self):
        bad = _good_meta(work_slug="the hobbit")  # space in slug
        with self.assertRaises(MetaError):
            validate_meta(bad)

    def test_public_domain_must_be_bool(self):
        bad = _good_meta(public_domain="false")
        with self.assertRaises(MetaError):
            validate_meta(bad)

    def test_copyrighted_accepted(self):
        # Tolkien/Vance directive: public_domain=false must be accepted.
        m = validate_meta(_good_meta(public_domain=False))
        self.assertFalse(m["public_domain"])


class TestImportInbox(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        self.inbox = os.path.join(self.root, "_inbox")
        os.makedirs(self.inbox)
        self.index_path = _seed_index(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def _read_index(self) -> dict:
        with open(self.index_path, "r", encoding="utf-8") as f:
            return json.load(f)

    # --- single-file end-to-end ---

    def test_single_file_import(self):
        body = "In a hole in the ground there lived a hobbit.\n"
        _drop(self.inbox, "the_hobbit", body, _good_meta())
        summary = import_inbox(self.root, dry_run=False, log=_silent)

        self.assertEqual(summary, {
            "added": 1, "replaced": 0, "errored": 0, "errors": [],
        })

        # Canonical file landed in author/work dir with original stem preserved.
        canonical = os.path.join(self.root, "tolkien", "the_hobbit", "the_hobbit.txt")
        self.assertTrue(os.path.isfile(canonical))
        with open(canonical, "r", encoding="utf-8") as f:
            self.assertIn("hobbit", f.read())

        # Index entry written with computed fields.
        idx = self._read_index()
        self.assertEqual(len(idx["works"]), 1)
        e = idx["works"][0]
        self.assertEqual(e["id"], "tolkien.the_hobbit")
        self.assertEqual(e["files"], ["tolkien/the_hobbit/the_hobbit.txt"])
        self.assertEqual(e["public_domain"], False)
        self.assertGreater(e["size_bytes"], 0)
        self.assertEqual(len(e["sha256"]), 64)
        self.assertGreater(e["approx_tokens"], 0)

        # Inbox cleaned by default.
        self.assertEqual(os.listdir(self.inbox), [])

    def test_keep_inbox_preserves_files(self):
        _drop(self.inbox, "the_hobbit", "body\n", _good_meta())
        import_inbox(self.root, keep_inbox=True, log=_silent)
        self.assertIn("the_hobbit.txt", os.listdir(self.inbox))
        self.assertIn("the_hobbit.meta.json", os.listdir(self.inbox))

    # --- files_glob batching ---

    def test_files_glob_multi_file_batching(self):
        # Three chapter files glued under one index entry via files_glob.
        for i in range(1, 4):
            with open(os.path.join(self.inbox, f"fellowship_ch{i:02d}.txt"),
                      "w", encoding="utf-8") as f:
                f.write(f"chapter {i} text content here.\n")
        meta = _good_meta(
            title="The Fellowship of the Ring",
            work_slug="book_01_fellowship",
        )
        meta["files_glob"] = "fellowship_ch*.txt"
        with open(os.path.join(self.inbox, "fellowship.meta.json"),
                  "w", encoding="utf-8") as f:
            json.dump(meta, f)

        summary = import_inbox(self.root, log=_silent)
        self.assertEqual(summary["added"], 1)
        self.assertEqual(summary["errored"], 0)

        idx = self._read_index()
        e = idx["works"][0]
        self.assertEqual(e["id"], "tolkien.book_01_fellowship")
        self.assertEqual(len(e["files"]), 3)
        for rel in e["files"]:
            self.assertTrue(
                os.path.isfile(os.path.join(self.root, rel)),
                f"missing canonical file: {rel}",
            )
        # Inbox emptied.
        self.assertEqual(os.listdir(self.inbox), [])

    # --- idempotent re-import ---

    def test_idempotent_reimport_replaces_not_duplicates(self):
        _drop(self.inbox, "the_hobbit", "first version body.\n", _good_meta())
        import_inbox(self.root, log=_silent)

        # Second drop with same id but different body -> replace.
        _drop(self.inbox, "the_hobbit", "REVISED body content.\n", _good_meta())
        summary = import_inbox(self.root, log=_silent)
        self.assertEqual(summary["added"], 0)
        self.assertEqual(summary["replaced"], 1)

        idx = self._read_index()
        ids = [w["id"] for w in idx["works"]]
        # Exactly one entry with that id.
        self.assertEqual(ids.count("tolkien.the_hobbit"), 1)
        self.assertEqual(len(idx["works"]), 1)

        canonical = os.path.join(self.root, "tolkien", "the_hobbit", "the_hobbit.txt")
        with open(canonical, "r", encoding="utf-8") as f:
            self.assertIn("REVISED", f.read())

    # --- dry-run is a true no-op ---

    def test_dry_run_does_not_modify_index_or_filesystem(self):
        _drop(self.inbox, "the_hobbit", "body\n", _good_meta())
        index_before = self._read_index()
        inbox_before = sorted(os.listdir(self.inbox))

        summary = import_inbox(self.root, dry_run=True, log=_silent)

        # Importer reports the planned operation.
        self.assertEqual(summary["added"], 1)
        self.assertEqual(summary["errored"], 0)

        # But neither index.json nor the filesystem was touched.
        self.assertEqual(self._read_index(), index_before)
        self.assertEqual(sorted(os.listdir(self.inbox)), inbox_before)
        self.assertFalse(os.path.exists(
            os.path.join(self.root, "tolkien", "the_hobbit", "the_hobbit.txt")
        ))

    # --- malformed meta is graceful ---

    def test_corrupt_meta_json_skipped_others_proceed(self):
        # Drop a corrupt meta with no matching .txt.
        with open(os.path.join(self.inbox, "broken.meta.json"),
                  "w", encoding="utf-8") as f:
            f.write("{ not valid json")

        # And a good entry alongside it.
        _drop(self.inbox, "the_hobbit", "ok body\n", _good_meta())

        summary = import_inbox(self.root, log=_silent)
        self.assertEqual(summary["added"], 1)
        self.assertEqual(summary["errored"], 1)
        self.assertTrue(any("broken.meta.json" in e for e in summary["errors"]))

        # Good entry made it.
        idx = self._read_index()
        self.assertEqual([w["id"] for w in idx["works"]], ["tolkien.the_hobbit"])

    def test_missing_required_field_skipped(self):
        bad = _good_meta()
        del bad["title"]
        _drop_meta_only(self.inbox, "no_title", bad)
        with open(os.path.join(self.inbox, "no_title.txt"), "w", encoding="utf-8") as f:
            f.write("body\n")

        # Plus a good one.
        _drop(self.inbox, "the_hobbit", "ok body\n", _good_meta())

        summary = import_inbox(self.root, log=_silent)
        self.assertEqual(summary["added"], 1)
        self.assertEqual(summary["errored"], 1)

    def test_missing_txt_for_meta_skipped(self):
        # meta references implicit sibling .txt that doesn't exist.
        _drop_meta_only(self.inbox, "absent_text", _good_meta(work_slug="absent_text"))
        summary = import_inbox(self.root, log=_silent)
        self.assertEqual(summary["added"], 0)
        self.assertEqual(summary["errored"], 1)
        self.assertEqual(self._read_index()["works"], [])


if __name__ == "__main__":
    unittest.main()
