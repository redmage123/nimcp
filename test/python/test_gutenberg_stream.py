"""Unit tests for scripts/sources/gutenberg_stream.py — CE-18.

Mocks urllib.request.urlopen with a small synthetic Project Gutenberg
fixture so the test suite never touches the network.

Coverage:
  * Backend self-registers under kind="gutenberg_stream".
  * list_for_stage(1)==[]; stages 2 and 3 return expected counts.
  * Header / footer "*** START OF" / "*** END OF" sentinels are stripped.
  * fetch_chunks yields chunked text with monotonic position.
  * Chunks usually align on paragraph boundaries ("\\n\\n").
  * is_complete flips True after exhaustion (and drops the cache).
  * Network errors propagate (do not get swallowed).
  * Rate-limit sleep is invoked (>= 1.0s) between successive fetches.
  * Fallback URL is tried when the primary URL returns HTTPError 404.
  * Generator tolerates being abandoned mid-iteration.

No live brain, no daemon, no network.
"""

from __future__ import annotations

import io
import os
import sys
import unittest
import urllib.error
from contextlib import contextmanager
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

# Make sure we get a fresh import so module-level register_source() runs into
# the registry that the test sees. Other tests may have flushed sys.modules.
for _mod in [
    "scripts.sources.gutenberg_stream",
    "scripts.sources",
    "scripts.streaming_ingest",
    "streaming_ingest",
]:
    if _mod in sys.modules:
        del sys.modules[_mod]

from scripts import streaming_ingest as si  # noqa: E402
from scripts.sources import gutenberg_stream as gs  # noqa: E402


# ----------------------------------------------------------------------------
# Synthetic fixture: a tiny Project Gutenberg book with the canonical
# header/footer sentinels and a few paragraphs of body text.
# ----------------------------------------------------------------------------

_BOOK_BODY = (
    "Chapter One.\n\n"
    "It was the best of times, it was the worst of times. " * 12 + "\n\n"
    "Chapter Two.\n\n"
    "All happy families are alike. " * 14 + "\n\n"
    "Chapter Three.\n\n"
    "Call me Ishmael. " * 16 + "\n\n"
    "Chapter Four.\n\n"
    "Stately, plump Buck Mulligan came from the stairhead. " * 10 + "\n\n"
    "The End."
)

_FIXTURE_BOOK = (
    "The Project Gutenberg eBook of Test Book\n\n"
    "This eBook is for the use of anyone anywhere at no cost...\n\n"
    "Title: Test Book\nAuthor: Test Author\n\n"
    "*** START OF THE PROJECT GUTENBERG EBOOK TEST BOOK ***\n"
    + _BOOK_BODY +
    "\n\n*** END OF THE PROJECT GUTENBERG EBOOK TEST BOOK ***\n\n"
    "This and all associated files of various formats will be found in:..."
)


def _fake_response(body: str | bytes):
    """Build a context-manager-compatible fake urlopen response."""
    if isinstance(body, str):
        body = body.encode("utf-8")
    buf = io.BytesIO(body)

    @contextmanager
    def _cm():
        try:
            yield buf
        finally:
            buf.close()

    return _cm()


# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------


class TestRegistration(unittest.TestCase):
    def test_self_registered(self):
        kinds = si.available_kinds()
        self.assertIn("gutenberg_stream", kinds, f"got: {kinds}")
        src = si.get_source("gutenberg_stream")
        self.assertIsNotNone(src)
        self.assertEqual(src.kind, "gutenberg_stream")
        self.assertIsInstance(src, gs.GutenbergStreamSource)


class TestStageRosters(unittest.TestCase):
    def setUp(self):
        self.src = gs.GutenbergStreamSource()

    def test_stage1_empty(self):
        self.assertEqual(self.src.list_for_stage(1), [])

    def test_stage2_count(self):
        ids = self.src.list_for_stage(2)
        # Curated 4-6 STEM/econ classics (we shipped 6).
        self.assertGreaterEqual(len(ids), 4)
        self.assertLessEqual(len(ids), 6)
        # Every ID is a stringified positive integer.
        for sid in ids:
            self.assertIsInstance(sid, str)
            self.assertTrue(sid.isdigit(), f"non-numeric id: {sid!r}")

    def test_stage3_count(self):
        ids = self.src.list_for_stage(3)
        # Curated 8-12 deeper works minus a couple of dedup drops.
        # We expect at least 7 (we shipped 9 after dropping 11/31010/21076).
        self.assertGreaterEqual(len(ids), 7)
        self.assertLessEqual(len(ids), 12)
        for sid in ids:
            self.assertTrue(sid.isdigit(), f"non-numeric id: {sid!r}")

    def test_unknown_stage_empty(self):
        self.assertEqual(self.src.list_for_stage(0), [])
        self.assertEqual(self.src.list_for_stage(99), [])

    def test_dedup_drops(self):
        # IDs known to already live on disk must NOT be in any stage roster.
        all_ids = (set(self.src.list_for_stage(1))
                   | set(self.src.list_for_stage(2))
                   | set(self.src.list_for_stage(3)))
        # gutenberg:11 = Alice in Wonderland (canonical_corpus on-disk)
        self.assertNotIn("11", all_ids)
        # gutenberg:21076 = Euclid's Elements (math_corpus on-disk)
        self.assertNotIn("21076", all_ids)


class TestHeaderFooterStrip(unittest.TestCase):
    def test_strip_removes_header_and_footer(self):
        cleaned = gs._strip_gutenberg_markers(_FIXTURE_BOOK)
        # Body content survives.
        self.assertIn("Chapter One.", cleaned)
        self.assertIn("Call me Ishmael.", cleaned)
        self.assertIn("The End.", cleaned)
        # Header preamble is gone.
        self.assertNotIn("This eBook is for the use of anyone", cleaned)
        # Footer trailer is gone.
        self.assertNotIn("This and all associated files", cleaned)
        # Sentinel lines themselves are stripped.
        self.assertNotIn("START OF THE PROJECT GUTENBERG", cleaned)
        self.assertNotIn("END OF THE PROJECT GUTENBERG", cleaned)

    def test_strip_tolerates_missing_sentinels(self):
        # No sentinels at all — body comes through untouched (modulo trim).
        plain = "Just some text.\n\nAnother paragraph."
        cleaned = gs._strip_gutenberg_markers(plain)
        self.assertIn("Just some text.", cleaned)
        self.assertIn("Another paragraph.", cleaned)


class TestFetchChunks(unittest.TestCase):
    def setUp(self):
        self.src = gs.GutenbergStreamSource()

    def _patch_urlopen(self, body=_FIXTURE_BOOK):
        return mock.patch.object(
            gs.urllib.request, "urlopen",
            return_value=_fake_response(body),
        )

    def test_chunks_yield_monotonic_position(self):
        with self._patch_urlopen():
            chunks = list(self.src.fetch_chunks("999999",
                                                  start_position=0,
                                                  chunk_chars=400))
        self.assertGreater(len(chunks), 1, "expected multiple chunks")
        positions = [p for p, _ in chunks]
        # Strictly monotonic increase.
        for prev, nxt in zip(positions, positions[1:]):
            self.assertLess(prev, nxt,
                             f"non-monotonic position: {prev} -> {nxt}")
        # Every chunk has non-empty text.
        for _, text in chunks:
            self.assertTrue(text.strip())

    def test_chunks_paragraph_aligned_when_possible(self):
        with self._patch_urlopen():
            chunks = list(self.src.fetch_chunks("999999",
                                                  start_position=0,
                                                  chunk_chars=400))
        # Several paragraph breaks exist in the fixture, so MOST chunks
        # (excluding the final one) should end on "\n\n". We allow some slack
        # because the boundary search uses a +/-25% window.
        non_terminal = chunks[:-1]
        if non_terminal:
            ending_on_paragraph = sum(
                1 for _, text in non_terminal if text.endswith("\n\n")
            )
            self.assertGreater(
                ending_on_paragraph, 0,
                "expected at least one paragraph-aligned chunk",
            )

    def test_resumes_at_start_position(self):
        with self._patch_urlopen():
            full = list(self.src.fetch_chunks("999999",
                                                start_position=0,
                                                chunk_chars=300))
        self.assertGreater(len(full), 2)
        # Mid-stream resume from chunk #2's reported position.
        skip_to = full[1][0]
        # Cache is populated already from the previous call — no re-fetch.
        with mock.patch.object(gs.urllib.request, "urlopen",
                                side_effect=AssertionError(
                                    "must use cache, not refetch")):
            tail = list(self.src.fetch_chunks("999999",
                                                start_position=skip_to,
                                                chunk_chars=300))
        # The first tail chunk should match what full[2] yielded.
        self.assertEqual(tail[0][1], full[2][1])

    def test_is_complete_after_exhaustion(self):
        with self._patch_urlopen():
            chunks = list(self.src.fetch_chunks("999999", chunk_chars=500))
        last_pos = chunks[-1][0]
        self.assertTrue(self.src.is_complete("999999", last_pos))
        # And the cache is dropped after completion.
        self.assertNotIn("999999", self.src._cache)

    def test_is_complete_false_before_end(self):
        with self._patch_urlopen():
            chunks = list(self.src.fetch_chunks("999999", chunk_chars=500))
        # Mid-text position is not complete.
        mid = chunks[0][0]
        self.assertFalse(self.src.is_complete("999999", mid))

    def test_is_complete_false_when_uncached(self):
        # Pristine source — we never fetched 999999.
        fresh = gs.GutenbergStreamSource()
        self.assertFalse(fresh.is_complete("999999", 0))

    def test_abandoned_iterator_does_not_leak(self):
        # Pull one chunk, drop the iterator. Re-create + iterate again.
        # No exception, no resource warning, and the cache survives.
        with self._patch_urlopen():
            it = self.src.fetch_chunks("999999", chunk_chars=300)
            first = next(it)
            del it  # explicit drop
        self.assertIn("999999", self.src._cache)
        self.assertGreater(first[0], 0)


class TestNetworkErrorPropagates(unittest.TestCase):
    def test_url_error_raises(self):
        src = gs.GutenbergStreamSource()
        with mock.patch.object(
            gs.urllib.request, "urlopen",
            side_effect=urllib.error.URLError("boom"),
        ):
            with self.assertRaises(urllib.error.URLError):
                # Generator construction triggers the fetch (cache is empty).
                list(src.fetch_chunks("999999"))


class TestRateLimitSleep(unittest.TestCase):
    def test_sleep_invoked_between_fetches(self):
        src = gs.GutenbergStreamSource()
        sleep_calls: list[float] = []

        def _fake_sleep(seconds):
            sleep_calls.append(seconds)

        with mock.patch.object(
            gs.urllib.request, "urlopen",
            side_effect=lambda *a, **k: _fake_response(_FIXTURE_BOOK),
        ):
            with mock.patch.object(gs.time, "sleep", side_effect=_fake_sleep):
                # First fetch — _last_fetch_at is 0 so no sleep triggered.
                list(src.fetch_chunks("aaaa", chunk_chars=2000))
                # Second fetch in quick succession — must sleep.
                list(src.fetch_chunks("bbbb", chunk_chars=2000))

        # At least one rate-limit sleep with a value of at least 1.0s.
        self.assertTrue(sleep_calls, "expected time.sleep to be called")
        self.assertTrue(
            any(s >= 1.0 for s in sleep_calls),
            f"expected a >= 1.0s sleep, got: {sleep_calls}",
        )


class TestFallbackURL(unittest.TestCase):
    def test_fallback_when_primary_404s(self):
        src = gs.GutenbergStreamSource()
        urls_seen: list[str] = []

        def _side_effect(req, timeout=None):
            url = req.get_full_url() if hasattr(req, "get_full_url") else req
            urls_seen.append(url)
            if len(urls_seen) == 1:
                raise urllib.error.HTTPError(
                    url, 404, "Not Found", hdrs=None, fp=None,
                )
            return _fake_response(_FIXTURE_BOOK)

        with mock.patch.object(gs.urllib.request, "urlopen",
                                side_effect=_side_effect):
            chunks = list(src.fetch_chunks("999999", chunk_chars=2000))

        self.assertEqual(len(urls_seen), 2,
                          f"expected exactly 2 URL attempts, got {urls_seen}")
        # First was the files/ form, second was the cache/epub fallback.
        self.assertIn("/files/999999/999999-0.txt", urls_seen[0])
        self.assertIn("/cache/epub/999999/pg999999.txt", urls_seen[1])
        # And we got real chunks out of the fallback fetch.
        self.assertGreater(len(chunks), 0)

    def test_fallback_url_failure_propagates(self):
        # If both URLs fail with HTTPError, the second one's exception
        # should propagate to the caller.
        src = gs.GutenbergStreamSource()

        def _side_effect(req, timeout=None):
            url = req.get_full_url() if hasattr(req, "get_full_url") else req
            raise urllib.error.HTTPError(
                url, 404, "Not Found", hdrs=None, fp=None,
            )

        with mock.patch.object(gs.urllib.request, "urlopen",
                                side_effect=_side_effect):
            with self.assertRaises(urllib.error.HTTPError):
                list(src.fetch_chunks("999999"))


class TestUserAgent(unittest.TestCase):
    def test_user_agent_string_is_set(self):
        # Sanity-check the contract-mandated UA string is what we configured.
        self.assertEqual(gs._USER_AGENT,
                          "NIMCP-Athena/1.0 streaming research")

    def test_user_agent_sent_in_request(self):
        src = gs.GutenbergStreamSource()
        captured = {}

        def _side_effect(req, timeout=None):
            captured["headers"] = dict(req.headers)
            return _fake_response(_FIXTURE_BOOK)

        with mock.patch.object(gs.urllib.request, "urlopen",
                                side_effect=_side_effect):
            list(src.fetch_chunks("999999", chunk_chars=2000))

        # urllib normalizes header keys to title-case.
        self.assertIn("User-agent", captured["headers"])
        self.assertEqual(captured["headers"]["User-agent"],
                          "NIMCP-Athena/1.0 streaming research")


if __name__ == "__main__":
    unittest.main()
