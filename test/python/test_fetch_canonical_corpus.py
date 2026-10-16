"""Unit tests for tools/fetch_canonical_corpus.py.

No network calls — all urlopen activity is mocked.
"""

import io
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

import tools.fetch_canonical_corpus as fcc  # noqa: E402


def _build_mini_corpus(root: str) -> None:
    idx = {
        "version": 1,
        "works": [
            {"id": "alpha.one", "author_slug": "alpha", "work_slug": "one",
             "title": "One", "files": ["alpha/one/one.txt"],
             "source": "gutenberg:42",
             "stage": 1, "weight": 1.0,
             "register": "test", "language": "en", "year": 2026,
             "public_domain": True},
            {"id": "beta.user", "author_slug": "beta", "work_slug": "user",
             "title": "Two", "files": ["beta/user/user.txt"],
             "source": "user_supplied",
             "stage": 1, "weight": 1.0,
             "register": "test", "language": "en", "year": 2026,
             "public_domain": False},
        ],
    }
    with open(os.path.join(root, "index.json"), "w") as f:
        json.dump(idx, f)


SAMPLE_GUTENBERG = b"""\
Title: Sample Work

Some prefatory matter from Gutenberg.
This should be stripped.

*** START OF THIS PROJECT GUTENBERG EBOOK SAMPLE ***

Chapter I.

Hello world. This is the body of the work. The body has multiple
sentences and paragraphs.

Chapter II.

Second chapter content.

*** END OF THIS PROJECT GUTENBERG EBOOK SAMPLE ***

Trailing license / metadata that should also be stripped.
"""


class TestStripMarkers(unittest.TestCase):
    def test_strips_start_and_end_markers(self):
        text = SAMPLE_GUTENBERG.decode("utf-8")
        body = fcc.strip_gutenberg_markers(text)
        self.assertNotIn("prefatory matter", body)
        self.assertNotIn("Trailing license", body)
        self.assertIn("Chapter I.", body)
        self.assertIn("Second chapter content.", body)

    def test_no_markers_returns_original(self):
        text = "Just a poem.\n\nNo Gutenberg markers here.\n"
        self.assertEqual(fcc.strip_gutenberg_markers(text), text)

    def test_only_start_marker(self):
        text = "skip me\n*** START OF X ***\nkeep me\n"
        body = fcc.strip_gutenberg_markers(text)
        self.assertNotIn("skip me", body)
        self.assertIn("keep me", body)

    def test_only_end_marker(self):
        text = "keep me\n*** END OF X ***\nskip me\n"
        body = fcc.strip_gutenberg_markers(text)
        self.assertIn("keep me", body)
        self.assertNotIn("skip me", body)


class TestParseGutenbergId(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(fcc.parse_gutenberg_id("gutenberg:11"), 11)
        self.assertEqual(fcc.parse_gutenberg_id("gutenberg:2701"), 2701)

    def test_user_supplied(self):
        self.assertIsNone(fcc.parse_gutenberg_id("user_supplied"))

    def test_malformed(self):
        self.assertIsNone(fcc.parse_gutenberg_id("gutenberg:notanumber"))
        self.assertIsNone(fcc.parse_gutenberg_id("gutenberg:"))


class TestNormalizeText(unittest.TestCase):
    def test_smart_quotes_folded(self):
        text = "“Hello,” he said — ‘ok’."
        out = fcc.normalize_text(text)
        self.assertIn('"Hello,"', out)
        self.assertIn(" -- ", out)
        self.assertIn("'ok'", out)


class TestKJVSlice(unittest.TestCase):
    def setUp(self):
        self.kjv = (
            "The First Book of Moses: Called Genesis\n\nIn the beginning...\n\n"
            "The Second Book of Moses: Called Exodus\n\nNow these are the names...\n\n"
            "The Book of Psalms\n\nBlessed is the man...\n\n"
            "The Proverbs\n\nThe proverbs of Solomon...\n\n"
            "The Gospel According to Saint Matthew\n\nThe book of the generation...\n\n"
            "The Acts of the Apostles\n\nThe former treatise have I made...\n\n"
            "The Revelation of Saint John the Divine\n\nThe revelation of Jesus Christ...\n"
        )

    def test_genesis_slice(self):
        out = fcc.slice_kjv(self.kjv, "genesis")
        self.assertIn("In the beginning", out)
        self.assertNotIn("these are the names", out)

    def test_psalms_slice(self):
        out = fcc.slice_kjv(self.kjv, "psalms")
        self.assertIn("Blessed is the man", out)
        self.assertNotIn("proverbs of Solomon", out)

    def test_gospels_slice(self):
        out = fcc.slice_kjv(self.kjv, "gospels")
        self.assertIn("book of the generation", out)
        self.assertNotIn("former treatise", out)


class TestMainDryRun(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        _build_mini_corpus(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_dry_run_makes_no_changes(self):
        idx_path = os.path.join(self.tmp.name, "index.json")
        before = open(idx_path).read()
        rc = fcc.main(["--corpus-root", self.tmp.name, "--dry-run"])
        self.assertEqual(rc, 0)
        after = open(idx_path).read()
        self.assertEqual(before, after)
        # No work file created.
        self.assertFalse(os.path.exists(os.path.join(self.tmp.name, "alpha", "one", "one.txt")))

    def test_only_filters(self):
        # alpha.one is the only gutenberg-sourced work; beta.user is user_supplied.
        # --only=beta.user should produce 0 fetches (beta is filtered out by source check).
        rc = fcc.main(["--corpus-root", self.tmp.name, "--only", "beta.user", "--dry-run"])
        self.assertEqual(rc, 0)


class TestMainMockedFetch(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        _build_mini_corpus(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_full_fetch_with_mock_urlopen(self):
        # Mock urlopen to return SAMPLE_GUTENBERG.
        fake_resp = mock.MagicMock()
        fake_resp.read.return_value = SAMPLE_GUTENBERG
        fake_resp.__enter__.return_value = fake_resp
        fake_resp.__exit__.return_value = False
        with mock.patch("tools.fetch_canonical_corpus.urlopen", return_value=fake_resp):
            rc = fcc.main(["--corpus-root", self.tmp.name])
        self.assertEqual(rc, 0)
        target = os.path.join(self.tmp.name, "alpha", "one", "one.txt")
        self.assertTrue(os.path.exists(target))
        body = open(target, encoding="utf-8").read()
        self.assertIn("Chapter I.", body)
        self.assertNotIn("prefatory matter", body)
        # Index updated with stats.
        idx = json.load(open(os.path.join(self.tmp.name, "index.json")))
        alpha = next(w for w in idx["works"] if w["id"] == "alpha.one")
        self.assertIn("size_bytes", alpha)
        self.assertIn("sha256", alpha)
        self.assertIn("approx_tokens", alpha)
        self.assertGreater(alpha["size_bytes"], 0)
        self.assertEqual(len(alpha["sha256"]), 64)


if __name__ == "__main__":
    unittest.main()
