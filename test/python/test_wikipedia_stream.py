"""Unit tests for scripts/sources/wikipedia.py — Wikipedia streaming source.

Mocks urllib.request.urlopen so no network is touched. Exercises:
  * Self-registration on import.
  * list_for_stage roster sizes and content.
  * fetch_chunks yields chunked text with monotonically increasing position.
  * HTML stripping removes markup and decodes entities.
  * is_complete flips True after consumption and drops the cache.
  * Network errors propagate (not swallowed).
  * Abandoned iterators don't leak — only state is the in-memory cache,
    which is intentionally retained for the next call (and dropped by
    is_complete).
  * Wikitext fallback stripper exists and behaves.
"""

import io
import os
import sys
import unittest
import urllib.error
from unittest.mock import patch, MagicMock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

# Force a clean import — under repeated test invocations the modules may be
# cached with stale state from another test file (e.g. test_streaming_ingest
# which sets NIMCP_STREAMING_TEST_STUB=1).
for _mod in [
    "scripts.sources.wikipedia",
    "scripts.sources._test_stub",
    "scripts.sources",
    "scripts.streaming_ingest",
    "streaming_ingest",
]:
    if _mod in sys.modules:
        del sys.modules[_mod]

# Make sure the test stub does NOT load — wikipedia must self-register on
# its own.
os.environ.pop("NIMCP_STREAMING_TEST_STUB", None)

from scripts import streaming_ingest as si  # noqa: E402
from scripts.sources import wikipedia as wp  # noqa: E402


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _mock_urlopen_with(body: str):
    """Return a context-manager-compatible MagicMock that yields `body` from
    .read(), matching urllib.request.urlopen's interface."""
    resp = MagicMock()
    resp.read.return_value = body.encode("utf-8")
    cm = MagicMock()
    cm.__enter__.return_value = resp
    cm.__exit__.return_value = False
    return cm


_SAMPLE_HTML = """<!DOCTYPE html>
<html><head><title>Cat</title>
<style>.foo{color:red;}</style>
<script>var x = 1;</script>
</head>
<body>
<table class="infobox"><tr><td>infobox junk that should be stripped</td></tr></table>
<p>The <b>cat</b> (<i>Felis catus</i>) is a small, domesticated carnivorous mammal.
It is the only domesticated species of the family Felidae.</p>

<p>Cats are similar in anatomy to other felid species: they have strong, flexible
bodies, quick reflexes, and sharp teeth adapted to killing small prey.</p>

<p>Their night vision and sense of smell are well developed. Cat communication
includes vocalizations like meowing, purring, trilling, hissing, growling, and
grunting as well as cat body language.</p>

<p>It can hear sounds too faint or too high in frequency for human ears, such as
those made by mice and other small mammals. It secretes and perceives pheromones.</p>

<p>Female domestic cats can have kittens from spring to late autumn in temperate
zones, with litter sizes often ranging from two to five kittens.</p>

<p>Domestic cats are bred and shown at events as registered pedigreed cats, a
hobby known as cat fancy. Population control of cats may be effected by spaying
and neutering, but their proliferation, abandonment of pets, and the lack of
sterilization of pets has resulted in large numbers of feral cats worldwide.</p>

<sup class="reference">[1]</sup>
&amp; &lt; &gt; &nbsp; &mdash;
</body></html>"""


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


class TestSelfRegistration(unittest.TestCase):
    def test_wikipedia_registered(self):
        kinds = si.available_kinds()
        self.assertIn("wikipedia", kinds, f"available_kinds()={kinds}")
        src = si.get_source("wikipedia")
        self.assertIsNotNone(src)
        self.assertEqual(src.kind, "wikipedia")
        self.assertIsInstance(src, wp.WikipediaSource)


class TestStageRosters(unittest.TestCase):
    def setUp(self):
        self.src = si.get_source("wikipedia")

    def test_stage1_simple_concrete_titles(self):
        roster = self.src.list_for_stage(1)
        self.assertGreaterEqual(len(roster), 5)
        self.assertLessEqual(len(roster), 8)
        # Sanity: only string titles, no URLs / paths.
        for t in roster:
            self.assertIsInstance(t, str)
            self.assertNotIn("/", t)
            self.assertNotIn(" ", t)
        # Spot-check expected entries.
        self.assertIn("Cat", roster)
        self.assertIn("Sun", roster)

    def test_stage2_elementary_science(self):
        roster = self.src.list_for_stage(2)
        self.assertGreaterEqual(len(roster), 8)
        self.assertLessEqual(len(roster), 12)
        self.assertIn("Atom", roster)
        self.assertIn("DNA", roster)

    def test_stage3_deeper_topics(self):
        roster = self.src.list_for_stage(3)
        self.assertGreaterEqual(len(roster), 10)
        self.assertLessEqual(len(roster), 15)
        self.assertIn("Consciousness", roster)
        self.assertIn("Artificial_intelligence", roster)

    def test_unknown_stage_empty(self):
        self.assertEqual(self.src.list_for_stage(0), [])
        self.assertEqual(self.src.list_for_stage(99), [])

    def test_returned_lists_are_independent_copies(self):
        # Mutating one returned list must not affect the next call's result.
        a = self.src.list_for_stage(1)
        a.append("Mutation")
        b = self.src.list_for_stage(1)
        self.assertNotIn("Mutation", b)


class TestHtmlStripping(unittest.TestCase):
    def test_script_and_style_removed(self):
        cleaned = wp._strip_html(_SAMPLE_HTML)
        self.assertNotIn("var x = 1", cleaned)
        self.assertNotIn(".foo{", cleaned)
        self.assertNotIn("color:red", cleaned)

    def test_tags_removed(self):
        cleaned = wp._strip_html(_SAMPLE_HTML)
        # No actual tags survive — check for tag-shaped patterns rather
        # than literal '<' chars (entity-decoded text legitimately may
        # contain a bare '<' from &lt;).
        import re as _re
        self.assertFalse(_re.search(r"<[a-zA-Z/!][^>]*>", cleaned),
                          f"HTML tag remnant in: {cleaned[:200]!r}")
        # Real word-content survives.
        self.assertIn("cat", cleaned.lower())
        self.assertIn("Felis catus", cleaned)

    def test_entities_decoded(self):
        cleaned = wp._strip_html("<p>Tom &amp; Jerry &mdash; &lt;3 &#65;</p>")
        self.assertIn("Tom & Jerry", cleaned)
        self.assertIn("—", cleaned)
        self.assertIn("<3", cleaned)
        self.assertIn("A", cleaned)  # &#65;

    def test_infobox_stripped(self):
        cleaned = wp._strip_html(_SAMPLE_HTML)
        self.assertNotIn("infobox junk", cleaned)

    def test_html_comment_removed(self):
        cleaned = wp._strip_html("<p>before<!-- private note -->after</p>")
        self.assertNotIn("private note", cleaned)
        self.assertIn("before", cleaned)
        self.assertIn("after", cleaned)

    def test_wikitext_fallback_strips_templates_and_links(self):
        wt = ("'''Cat''' ([[Felis catus]]) is a {{small}} mammal.\n"
              "== Habitat ==\n"
              "Sometimes lives [[house|indoors]].")
        cleaned = wp._strip_wikitext(wt)
        self.assertIn("Cat", cleaned)
        self.assertIn("Felis catus", cleaned)
        self.assertIn("indoors", cleaned)
        self.assertIn("Habitat", cleaned)
        self.assertNotIn("{{", cleaned)
        self.assertNotIn("[[", cleaned)
        self.assertNotIn("'''", cleaned)


class TestFetchChunks(unittest.TestCase):
    def setUp(self):
        # Use a fresh instance so cache state doesn't bleed between tests.
        self.src = wp.WikipediaSource()

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_yields_pairs_with_monotonic_position(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        chunks = list(self.src.fetch_chunks("Cat", chunk_chars=200))
        self.assertGreater(len(chunks), 1, "Expected multiple chunks")
        prev_pos = 0
        for next_pos, text in chunks:
            self.assertIsInstance(next_pos, int)
            self.assertIsInstance(text, str)
            self.assertTrue(text)
            self.assertGreater(next_pos, prev_pos,
                                "Position must be strictly increasing")
            prev_pos = next_pos
        # Final position equals the cleaned-text length.
        cleaned = wp._strip_html(_SAMPLE_HTML)
        self.assertEqual(prev_pos, len(cleaned))

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_chunk_sizes_near_target(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        target = 200
        chunks = list(self.src.fetch_chunks("Cat", chunk_chars=target))
        # Window allows +/- 25% of target. The last chunk may be shorter.
        upper = int(target * 1.30)
        for i, (pos, text) in enumerate(chunks[:-1]):
            self.assertLessEqual(len(text), upper,
                                  f"Chunk {i} too large: {len(text)} > {upper}")

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_resume_at_position(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        # Pull first chunk, capture next_position, restart fresh from there.
        gen = self.src.fetch_chunks("Cat", chunk_chars=200, start_position=0)
        first_pos, first_text = next(gen)
        gen.close()  # abandon
        cleaned = wp._strip_html(_SAMPLE_HTML)
        # Resuming should yield the slice starting at first_pos.
        gen2 = self.src.fetch_chunks("Cat", chunk_chars=200,
                                       start_position=first_pos)
        second_pos, second_text = next(gen2)
        gen2.close()
        self.assertEqual(second_text, cleaned[first_pos:second_pos])

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_caches_after_first_fetch(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        # Run two full iterations of the same source_id.
        list(self.src.fetch_chunks("Cat", chunk_chars=500))
        list(self.src.fetch_chunks("Cat", chunk_chars=500))
        self.assertEqual(mock_urlopen.call_count, 1,
                          "Cached article must not re-fetch")

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_url_uses_quoted_title(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        list(self.src.fetch_chunks("Cell_(biology)", chunk_chars=500))
        called_url = mock_urlopen.call_args[0][0].full_url
        self.assertIn("Cell_%28biology%29", called_url)
        self.assertTrue(called_url.startswith("https://en.wikipedia.org/"))


class TestIsComplete(unittest.TestCase):
    def setUp(self):
        self.src = wp.WikipediaSource()

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_flips_true_after_consumption(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        chunks = list(self.src.fetch_chunks("Cat", chunk_chars=200))
        last_pos = chunks[-1][0]
        self.assertTrue(self.src.is_complete("Cat", last_pos))

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_false_mid_consumption(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        gen = self.src.fetch_chunks("Cat", chunk_chars=200, start_position=0)
        first_pos, _ = next(gen)
        gen.close()
        self.assertFalse(self.src.is_complete("Cat", first_pos))

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_completion_drops_cache(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        chunks = list(self.src.fetch_chunks("Cat", chunk_chars=200))
        last_pos = chunks[-1][0]
        self.assertIn("Cat", self.src._cache)
        self.assertTrue(self.src.is_complete("Cat", last_pos))
        self.assertNotIn("Cat", self.src._cache,
                          "Cache must be dropped on completion")

    def test_uncached_position_zero_returns_false(self):
        # Without ever fetching, is_complete must return False for any pos.
        self.assertFalse(self.src.is_complete("NeverFetched", 0))
        self.assertFalse(self.src.is_complete("NeverFetched", 999999))


class TestNetworkErrors(unittest.TestCase):
    def setUp(self):
        self.src = wp.WikipediaSource()

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_url_error_propagates(self, mock_urlopen):
        mock_urlopen.side_effect = urllib.error.URLError("connection refused")
        gen = self.src.fetch_chunks("Cat", chunk_chars=200)
        with self.assertRaises(urllib.error.URLError):
            next(gen)

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_http_error_propagates(self, mock_urlopen):
        mock_urlopen.side_effect = urllib.error.HTTPError(
            url="https://en.wikipedia.org/api/rest_v1/page/html/Cat",
            code=404, msg="Not Found", hdrs=None, fp=io.BytesIO(b""),
        )
        gen = self.src.fetch_chunks("Missing_Article", chunk_chars=200)
        with self.assertRaises(urllib.error.HTTPError):
            next(gen)

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_timeout_propagates(self, mock_urlopen):
        mock_urlopen.side_effect = TimeoutError("timed out")
        gen = self.src.fetch_chunks("Cat", chunk_chars=200)
        with self.assertRaises(TimeoutError):
            next(gen)

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_failed_fetch_does_not_cache(self, mock_urlopen):
        mock_urlopen.side_effect = urllib.error.URLError("boom")
        gen = self.src.fetch_chunks("Cat", chunk_chars=200)
        with self.assertRaises(urllib.error.URLError):
            next(gen)
        # No partial cache entry.
        self.assertNotIn("Cat", self.src._cache)


class TestAbandonedIterator(unittest.TestCase):
    def setUp(self):
        self.src = wp.WikipediaSource()

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_close_after_one_chunk_does_not_raise(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        gen = self.src.fetch_chunks("Cat", chunk_chars=200)
        next(gen)
        # Closing the generator triggers GeneratorExit inside try/finally.
        gen.close()
        # Cache entry should still be present (intentional — next call
        # should reuse it without re-fetching).
        self.assertIn("Cat", self.src._cache)

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_garbage_collected_iterator_does_not_leak_state(self,
                                                              mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        # Create an iterator and drop the only reference to it.
        gen = self.src.fetch_chunks("Cat", chunk_chars=200)
        next(gen)
        del gen
        import gc
        gc.collect()
        # Single _cache entry (the cached article), and it's the only
        # persistent state on `self`. No other attrs should have appeared.
        attrs = set(vars(self.src).keys())
        self.assertEqual(attrs, {"_cache"})

    @patch("scripts.sources.wikipedia.urllib.request.urlopen")
    def test_subsequent_fetch_reuses_cache(self, mock_urlopen):
        mock_urlopen.return_value = _mock_urlopen_with(_SAMPLE_HTML)
        gen = self.src.fetch_chunks("Cat", chunk_chars=200)
        next(gen)
        gen.close()
        # Second call resumes; must NOT re-fetch.
        gen2 = self.src.fetch_chunks("Cat", chunk_chars=200,
                                       start_position=0)
        next(gen2)
        gen2.close()
        self.assertEqual(mock_urlopen.call_count, 1)


class TestBoundaryHelper(unittest.TestCase):
    def test_paragraph_boundary_preferred(self):
        text = "first paragraph.\n\nsecond paragraph here."
        target = 18  # ~middle of paragraph break
        end = wp._find_boundary(text, target, chunk_chars=20)
        # Should land right after the paragraph break.
        self.assertGreaterEqual(end, len("first paragraph.\n\n"))

    def test_sentence_boundary_when_no_paragraph(self):
        text = "Sentence one. Sentence two. Sentence three."
        target = 14
        end = wp._find_boundary(text, target, chunk_chars=15)
        # Cut should be after "Sentence one. ".
        self.assertEqual(text[:end].rstrip(), "Sentence one.")

    def test_falls_back_to_target_when_no_boundary(self):
        text = "x" * 1000  # no boundaries at all
        target = 500
        end = wp._find_boundary(text, target, chunk_chars=200)
        self.assertEqual(end, target)

    def test_target_past_end_returns_len(self):
        text = "short"
        end = wp._find_boundary(text, target=100, chunk_chars=200)
        self.assertEqual(end, len(text))


if __name__ == "__main__":
    unittest.main()
