"""Unit tests for scripts/sources/arxiv.py — arXiv streaming backend.

All network calls are mocked. We never touch export.arxiv.org from these
tests. Coverage:
  - Backend self-registers under kind="arxiv"
  - list_for_stage() returns [] for stage 1, the curated lists for 2/3
  - fetch_chunks() extracts <summary> from Atom XML and yields chunks
    with monotonic positions
  - is_complete() flips True after the cached abstract is exhausted
  - Network errors propagate (don't get swallowed)
  - Rate limit: time.sleep is called with at least 3.0 seconds between
    successive network requests
"""

from __future__ import annotations

import os
import sys
import unittest
import urllib.error
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

# Force a fresh import so the registry is in a known state for this run.
for _mod in [
    "scripts.sources.arxiv",
    "scripts.sources",
    "scripts.streaming_ingest",
]:
    if _mod in sys.modules:
        del sys.modules[_mod]

from scripts import streaming_ingest as si  # noqa: E402
from scripts.sources import arxiv as arxiv_mod  # noqa: E402


# A small but realistic Atom-XML fixture. The summary text spans multiple
# lines (matching how arXiv hard-wraps abstracts) and contains several
# sentences so chunking has something to align on.
_FIXTURE_XML = """<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <entry>
    <id>http://arxiv.org/abs/1706.03762v5</id>
    <title>Attention Is All You Need</title>
    <summary>The dominant sequence transduction models are based on
complex recurrent or convolutional neural networks that include an
encoder and a decoder. The best performing models also connect the
encoder and decoder through an attention mechanism. We propose a new
simple network architecture, the Transformer, based solely on attention
mechanisms, dispensing with recurrence and convolutions entirely.
Experiments on two machine translation tasks show these models to be
superior in quality while being more parallelizable and requiring
significantly less time to train.</summary>
  </entry>
</feed>
"""

_FIXTURE_ABSTRACT_PREFIX = "The dominant sequence transduction models"


def _mk_urlopen_mock(payload: bytes):
    """Build a mock for urllib.request.urlopen returning `payload`."""
    cm = mock.MagicMock()
    cm.read.return_value = payload
    cm.__enter__.return_value = cm
    cm.__exit__.return_value = False
    return mock.MagicMock(return_value=cm)


class TestRegistration(unittest.TestCase):
    def test_arxiv_registered(self):
        self.assertIn("arxiv", si.available_kinds())
        src = si.get_source("arxiv")
        self.assertIsNotNone(src)
        self.assertIsInstance(src, arxiv_mod.ArxivSource)
        self.assertEqual(src.kind, "arxiv")


class TestListForStage(unittest.TestCase):
    def setUp(self):
        self.src = arxiv_mod.ArxivSource()

    def test_stage_1_empty(self):
        self.assertEqual(self.src.list_for_stage(1), [])

    def test_stage_0_and_negative_empty(self):
        # Defensive — anything <=1 should be empty.
        self.assertEqual(self.src.list_for_stage(0), [])
        self.assertEqual(self.src.list_for_stage(-3), [])

    def test_stage_2_landmark_papers(self):
        ids = self.src.list_for_stage(2)
        # Spec calls for 5-8 well-known landmark abstracts.
        self.assertGreaterEqual(len(ids), 5)
        self.assertLessEqual(len(ids), 8)
        # Must include the headline transformer paper.
        self.assertIn("1706.03762", ids)
        # All entries are non-empty strings.
        for pid in ids:
            self.assertIsInstance(pid, str)
            self.assertTrue(pid)

    def test_stage_3_deeper_papers(self):
        ids = self.src.list_for_stage(3)
        # Spec calls for 10-15 deeper papers.
        self.assertGreaterEqual(len(ids), 10)
        self.assertLessEqual(len(ids), 15)
        # Spot-check a few that the spec named explicitly.
        self.assertIn("1412.6980", ids)        # Adam
        self.assertIn("math/0211159", ids)     # Perelman
        self.assertIn("1505.04597", ids)       # U-Net


class TestParseAbstract(unittest.TestCase):
    def test_extracts_and_collapses_whitespace(self):
        abstract = arxiv_mod.ArxivSource._parse_abstract(_FIXTURE_XML)
        self.assertTrue(abstract.startswith(_FIXTURE_ABSTRACT_PREFIX))
        # Hard-wrapped newlines should have been collapsed to spaces.
        self.assertNotIn("\n", abstract)
        self.assertNotIn("  ", abstract)
        # Trailing whitespace stripped.
        self.assertEqual(abstract.strip(), abstract)

    def test_missing_summary_returns_empty(self):
        empty_feed = ("<?xml version='1.0' encoding='UTF-8'?>"
                      "<feed xmlns='http://www.w3.org/2005/Atom'>"
                      "<entry><id>x</id></entry>"
                      "</feed>")
        self.assertEqual(arxiv_mod.ArxivSource._parse_abstract(empty_feed), "")

    def test_malformed_xml_returns_empty(self):
        self.assertEqual(arxiv_mod.ArxivSource._parse_abstract("<not xml"), "")

    def test_empty_input_returns_empty(self):
        self.assertEqual(arxiv_mod.ArxivSource._parse_abstract(""), "")


class TestFetchChunks(unittest.TestCase):
    def setUp(self):
        self.src = arxiv_mod.ArxivSource()
        # Patch out time.sleep so tests don't actually wait 3 seconds.
        self._sleep_patcher = mock.patch.object(arxiv_mod.time, "sleep")
        self.mock_sleep = self._sleep_patcher.start()
        # Patch urllib.request.urlopen with our fixture.
        self.mock_urlopen = _mk_urlopen_mock(_FIXTURE_XML.encode("utf-8"))
        self._urlopen_patcher = mock.patch.object(
            arxiv_mod.urllib.request, "urlopen", self.mock_urlopen)
        self._urlopen_patcher.start()

    def tearDown(self):
        self._sleep_patcher.stop()
        self._urlopen_patcher.stop()

    def test_yields_chunks_with_monotonic_positions(self):
        chunks = list(self.src.fetch_chunks("1706.03762",
                                            start_position=0,
                                            chunk_chars=200))
        self.assertGreater(len(chunks), 0)
        last_pos = -1
        joined = ""
        for next_pos, text in chunks:
            self.assertIsInstance(next_pos, int)
            self.assertIsInstance(text, str)
            self.assertGreater(next_pos, last_pos,
                               f"position regressed: {next_pos} <= {last_pos}")
            last_pos = next_pos
            joined += text
        # Concatenating chunks should reproduce the cleaned abstract.
        cached = self.src._abstract_cache["1706.03762"]
        self.assertEqual(joined, cached)
        # Final position is the abstract length.
        self.assertEqual(chunks[-1][0], len(cached))

    def test_first_chunk_starts_at_abstract_start(self):
        chunks = list(self.src.fetch_chunks("1706.03762",
                                            start_position=0,
                                            chunk_chars=400))
        # First chunk must contain the abstract's prefix.
        first_text = chunks[0][1]
        self.assertTrue(first_text.startswith(_FIXTURE_ABSTRACT_PREFIX))

    def test_resume_at_position(self):
        # Prime the cache.
        all_chunks = list(self.src.fetch_chunks("1706.03762",
                                                start_position=0,
                                                chunk_chars=200))
        cached = self.src._abstract_cache["1706.03762"]
        # Pick a midpoint and resume from there.
        midpoint = all_chunks[0][0]
        resumed = list(self.src.fetch_chunks("1706.03762",
                                             start_position=midpoint,
                                             chunk_chars=200))
        # Concatenation should match the tail of the cached abstract.
        joined_tail = "".join(t for _, t in resumed)
        self.assertEqual(joined_tail, cached[midpoint:])
        # Final position is still the full abstract length.
        self.assertEqual(resumed[-1][0], len(cached))

    def test_fetch_chunks_only_hits_network_once_per_paper(self):
        list(self.src.fetch_chunks("1706.03762", chunk_chars=200))
        list(self.src.fetch_chunks("1706.03762", chunk_chars=200))
        # Second call must use the cached abstract — no second urlopen.
        self.assertEqual(self.mock_urlopen.call_count, 1)

    def test_user_agent_is_set(self):
        list(self.src.fetch_chunks("1706.03762", chunk_chars=200))
        # Inspect the Request object passed to urlopen.
        request_arg = self.mock_urlopen.call_args.args[0]
        # urllib normalizes headers — "User-Agent" becomes "User-agent".
        ua = request_arg.get_header("User-agent")
        self.assertIsNotNone(ua, f"User-Agent missing; headers="
                                  f"{dict(request_arg.header_items())}")
        self.assertIn("NIMCP-Athena", ua)
        self.assertIn("mailto:", ua)


class TestIsComplete(unittest.TestCase):
    def setUp(self):
        self.src = arxiv_mod.ArxivSource()
        self._sleep_patcher = mock.patch.object(arxiv_mod.time, "sleep")
        self._sleep_patcher.start()
        self._urlopen_patcher = mock.patch.object(
            arxiv_mod.urllib.request, "urlopen",
            _mk_urlopen_mock(_FIXTURE_XML.encode("utf-8")))
        self._urlopen_patcher.start()

    def tearDown(self):
        self._sleep_patcher.stop()
        self._urlopen_patcher.stop()

    def test_flips_after_exhaustion(self):
        # Pull all chunks so the cache is populated.
        chunks = list(self.src.fetch_chunks("1706.03762", chunk_chars=400))
        final_pos = chunks[-1][0]
        self.assertTrue(self.src.is_complete("1706.03762", final_pos))

    def test_not_complete_partway_through(self):
        # Populate cache by pulling the full stream.
        chunks = list(self.src.fetch_chunks("1706.03762", chunk_chars=200))
        midpoint = chunks[0][0]
        # Re-populate the cache (is_complete dropped it on the previous
        # exhaustion call) so we can ask about a midpoint.
        chunks2 = list(self.src.fetch_chunks("1706.03762", chunk_chars=200))
        self.assertFalse(self.src.is_complete("1706.03762", midpoint))
        # End position is complete.
        self.assertTrue(
            self.src.is_complete("1706.03762", chunks2[-1][0]))

    def test_unknown_paper_not_complete(self):
        # Without a populated cache we can't tell; default is False.
        self.assertFalse(self.src.is_complete("9999.99999", 0))


class TestNetworkErrorRaises(unittest.TestCase):
    def test_url_error_propagates(self):
        src = arxiv_mod.ArxivSource()
        with mock.patch.object(arxiv_mod.time, "sleep"):
            with mock.patch.object(
                    arxiv_mod.urllib.request, "urlopen",
                    side_effect=urllib.error.URLError("boom")):
                gen = src.fetch_chunks("1706.03762", chunk_chars=200)
                with self.assertRaises(urllib.error.URLError):
                    next(gen)

    def test_http_error_propagates(self):
        src = arxiv_mod.ArxivSource()
        with mock.patch.object(arxiv_mod.time, "sleep"):
            err = urllib.error.HTTPError(
                "http://x", 503, "Service Unavailable", {}, None)
            with mock.patch.object(
                    arxiv_mod.urllib.request, "urlopen", side_effect=err):
                gen = src.fetch_chunks("1706.03762", chunk_chars=200)
                with self.assertRaises(urllib.error.HTTPError):
                    next(gen)


class TestRateLimitSleep(unittest.TestCase):
    def test_sleep_invoked_with_at_least_three_seconds(self):
        src = arxiv_mod.ArxivSource()
        with mock.patch.object(arxiv_mod.time, "sleep") as mock_sleep:
            with mock.patch.object(
                    arxiv_mod.urllib.request, "urlopen",
                    _mk_urlopen_mock(_FIXTURE_XML.encode("utf-8"))):
                # First fetch — cold-start polite delay.
                list(src.fetch_chunks("1706.03762", chunk_chars=400))
                # Second fetch for a different paper — within rate window.
                # We need to clear the cache to force another network hit.
                src._abstract_cache.clear()
                list(src.fetch_chunks("1207.7235", chunk_chars=400))
        # At least one sleep call must have been made with >= 3.0s.
        self.assertGreater(mock_sleep.call_count, 0,
                           "Rate-limit sleep was never invoked")
        sleep_durations = [c.args[0] for c in mock_sleep.call_args_list
                           if c.args]
        self.assertTrue(
            any(d >= 3.0 for d in sleep_durations),
            f"No sleep>=3.0s observed; saw {sleep_durations}")

    def test_no_sleep_when_using_cache(self):
        src = arxiv_mod.ArxivSource()
        with mock.patch.object(arxiv_mod.urllib.request, "urlopen",
                                _mk_urlopen_mock(
                                    _FIXTURE_XML.encode("utf-8"))):
            with mock.patch.object(arxiv_mod.time, "sleep") as mock_sleep:
                # Prime the cache.
                list(src.fetch_chunks("1706.03762", chunk_chars=400))
                cold_calls = mock_sleep.call_count
                # Second call hits the cache — no network, no sleep.
                list(src.fetch_chunks("1706.03762", chunk_chars=400))
                self.assertEqual(mock_sleep.call_count, cold_calls,
                                 "Cached fetch should not trigger sleep")


class TestAbandonedIterator(unittest.TestCase):
    """The contract requires fetch_chunks to tolerate being abandoned
    mid-iteration without leaking resources. We verify by pulling one
    chunk then dropping the generator — nothing should raise on cleanup.
    """

    def test_abandon_after_one_chunk(self):
        src = arxiv_mod.ArxivSource()
        with mock.patch.object(arxiv_mod.time, "sleep"):
            with mock.patch.object(
                    arxiv_mod.urllib.request, "urlopen",
                    _mk_urlopen_mock(_FIXTURE_XML.encode("utf-8"))):
                gen = src.fetch_chunks("1706.03762", chunk_chars=200)
                first = next(gen)
                self.assertIsInstance(first, tuple)
                # Drop the generator — the GeneratorExit must not raise.
                gen.close()


if __name__ == "__main__":
    unittest.main()
