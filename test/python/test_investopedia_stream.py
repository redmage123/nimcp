"""Unit tests for scripts/sources/investopedia.py.

Covers the StreamingSource contract for the Investopedia backend with
all network calls mocked. No live HTTP. No on-disk article persistence.

Run:
    cd /home/bbrelin/nimcp && python3 test/python/test_investopedia_stream.py
"""

import io
import os
import sys
import unittest
from unittest.mock import patch, MagicMock
from urllib.error import URLError

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

# Force fresh import so the backend re-registers cleanly.
for _mod in [
    "scripts.sources.investopedia",
    "scripts.sources",
    "scripts.streaming_ingest",
]:
    if _mod in sys.modules:
        del sys.modules[_mod]

from scripts import streaming_ingest as si  # noqa: E402
from scripts.sources import investopedia as ipd  # noqa: E402


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


_FAKE_HTML = """
<!doctype html>
<html><head>
<title>Money</title>
<script>var x = 1;</script>
<style>.foo { color: red; }</style>
</head><body>
<nav>top nav menu links</nav>
<div class="article-body">
  <h1>Money</h1>
  <p>Money is any item or verifiable record that is generally accepted
     as payment for goods and services. It serves as a medium of exchange,
     a unit of account, and a store of value. Throughout history, many
     forms of money have existed including commodities, coins, paper notes,
     and more recently, digital currencies.</p>
  <p>Modern economies rely on fiat money, which has value because a
     government maintains it and people have faith in it. Central banks
     control the supply of money, influencing interest rates and inflation
     in pursuit of macroeconomic stability.</p>
  <p>Cryptocurrencies represent a new chapter, decentralizing the issuance
     and verification of money through cryptographic ledgers rather than
     central authorities. Their long-term role remains an open question.</p>
  <p>Understanding money is foundational to financial literacy. It
     underpins concepts like saving, investing, debt, and economic
     productivity, all of which interact across personal, corporate, and
     national scales of decision-making.</p>
</div>
<footer>page footer copyright stuff</footer>
</body></html>
""".strip()


def _make_response(body: bytes):
    """Return a context-manager-compatible mock urlopen response."""
    resp = MagicMock()
    resp.read.return_value = body
    resp.__enter__.return_value = resp
    resp.__exit__.return_value = False
    return resp


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


class TestRegistration(unittest.TestCase):
    def test_backend_self_registers(self):
        kinds = si.available_kinds()
        self.assertIn("investopedia", kinds)
        src = si.get_source("investopedia")
        self.assertIsNotNone(src)
        self.assertEqual(src.kind, "investopedia")
        self.assertIsInstance(src, ipd.InvestopediaSource)


class TestListForStage(unittest.TestCase):
    def setUp(self):
        self.src = ipd.InvestopediaSource()

    def test_stage_1_is_empty(self):
        self.assertEqual(self.src.list_for_stage(1), [])

    def test_stage_0_is_empty(self):
        self.assertEqual(self.src.list_for_stage(0), [])

    def test_stage_2_count(self):
        slugs = self.src.list_for_stage(2)
        self.assertGreaterEqual(len(slugs), 8)
        self.assertLessEqual(len(slugs), 10)
        # Sanity: each looks like a terms slug.
        for s in slugs:
            self.assertTrue(s.startswith("terms/"))
            self.assertTrue(s.endswith(".asp"))

    def test_stage_3_count(self):
        slugs = self.src.list_for_stage(3)
        self.assertGreaterEqual(len(slugs), 10)
        self.assertLessEqual(len(slugs), 15)
        for s in slugs:
            self.assertTrue(s.startswith("terms/"))
            self.assertTrue(s.endswith(".asp"))

    def test_stage_2_and_3_disjoint_topics(self):
        # Just make sure stage 3 is the deeper-concepts list, not a
        # repeat of stage 2.
        s2 = set(self.src.list_for_stage(2))
        s3 = set(self.src.list_for_stage(3))
        self.assertNotEqual(s2, s3)

    def test_returns_a_copy(self):
        # Mutating the returned list must not affect later calls.
        slugs = self.src.list_for_stage(2)
        slugs.append("garbage")
        self.assertNotIn("garbage", self.src.list_for_stage(2))


class TestHtmlStripping(unittest.TestCase):
    def test_drops_script_style_nav_footer(self):
        cleaned = ipd._strip_html(_FAKE_HTML)
        self.assertNotIn("var x = 1", cleaned)
        self.assertNotIn("color: red", cleaned)
        self.assertNotIn("top nav menu links", cleaned)
        self.assertNotIn("page footer copyright", cleaned)

    def test_keeps_article_body_text(self):
        cleaned = ipd._strip_html(_FAKE_HTML)
        self.assertIn("Money is any item", cleaned)
        self.assertIn("medium of exchange", cleaned)
        self.assertIn("Cryptocurrencies", cleaned)

    def test_no_html_tags_remain(self):
        cleaned = ipd._strip_html(_FAKE_HTML)
        # Crude: no angle brackets in the cleaned output.
        self.assertNotIn("<", cleaned)
        self.assertNotIn(">", cleaned)

    def test_empty_input(self):
        self.assertEqual(ipd._strip_html(""), "")


class TestFetchChunks(unittest.TestCase):
    def _new_src(self):
        return ipd.InvestopediaSource()

    def test_yields_chunks_with_monotonic_position(self):
        src = self._new_src()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep"):
            chunks = list(src.fetch_chunks("terms/m/money.asp",
                                           start_position=0,
                                           chunk_chars=200))
        self.assertGreater(len(chunks), 1, "should split into >1 chunk")
        last_pos = -1
        for next_pos, text in chunks:
            self.assertIsInstance(next_pos, int)
            self.assertIsInstance(text, str)
            self.assertGreater(next_pos, last_pos)
            self.assertTrue(text.strip())
            last_pos = next_pos

    def test_chunk_text_is_clean(self):
        src = self._new_src()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep"):
            chunks = list(src.fetch_chunks("terms/m/money.asp",
                                           chunk_chars=200))
        joined = "".join(c[1] for c in chunks)
        self.assertNotIn("<", joined)
        self.assertNotIn("</p>", joined)
        self.assertIn("Money is any item", joined)

    def test_resume_from_position(self):
        src = self._new_src()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep"):
            first = list(src.fetch_chunks("terms/m/money.asp",
                                          chunk_chars=200))
            self.assertGreater(len(first), 1)
            mid_pos = first[0][0]
            # Resume — should NOT re-fetch (cached) and should pick up
            # at the requested offset.
            with patch("scripts.sources.investopedia.urlopen") as no_call:
                rest = list(src.fetch_chunks("terms/m/money.asp",
                                             start_position=mid_pos,
                                             chunk_chars=200))
                no_call.assert_not_called()
            # The first chunk of `rest` should match the second chunk of
            # the original.
            self.assertEqual(rest[0], first[1])

    def test_fetch_is_cached_on_self(self):
        src = self._new_src()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))) \
                as mock_open, \
             patch("scripts.sources.investopedia.time.sleep"):
            list(src.fetch_chunks("terms/m/money.asp", chunk_chars=200))
            list(src.fetch_chunks("terms/m/money.asp", chunk_chars=200))
            # Only ONE network fetch despite two iterations.
            self.assertEqual(mock_open.call_count, 1)

    def test_iterator_can_be_abandoned(self):
        # try/finally cleanup must not raise. Just pull one chunk and
        # drop the rest.
        src = self._new_src()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep"):
            gen = src.fetch_chunks("terms/m/money.asp", chunk_chars=200)
            first = next(gen)
            self.assertIsInstance(first, tuple)
            gen.close()  # explicit abandonment


class TestIsComplete(unittest.TestCase):
    def test_flips_true_after_exhaustion(self):
        src = ipd.InvestopediaSource()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep"):
            chunks = list(src.fetch_chunks("terms/m/money.asp",
                                           chunk_chars=200))
        last_pos = chunks[-1][0]
        # Mid-stream not complete.
        self.assertFalse(src.is_complete("terms/m/money.asp", 0))
        # Re-prime cache (is_complete dropped it on the True path? No —
        # only on the True branch). 0 is False so cache is still there.
        self.assertTrue(src.is_complete("terms/m/money.asp", last_pos))

    def test_unfetched_source_is_not_complete(self):
        src = ipd.InvestopediaSource()
        # Never fetched — conservative False.
        self.assertFalse(src.is_complete("terms/x/never.asp", 0))


class TestNetworkErrorPropagates(unittest.TestCase):
    def test_urlerror_raises(self):
        src = ipd.InvestopediaSource()
        with patch("scripts.sources.investopedia.urlopen",
                   side_effect=URLError("dns down")), \
             patch("scripts.sources.investopedia.time.sleep"):
            gen = src.fetch_chunks("terms/m/money.asp", chunk_chars=200)
            with self.assertRaises(URLError):
                next(gen)

    def test_generic_oserror_raises(self):
        src = ipd.InvestopediaSource()
        with patch("scripts.sources.investopedia.urlopen",
                   side_effect=OSError("connection refused")), \
             patch("scripts.sources.investopedia.time.sleep"):
            gen = src.fetch_chunks("terms/m/money.asp", chunk_chars=200)
            with self.assertRaises(OSError):
                next(gen)


class TestRateLimit(unittest.TestCase):
    def test_sleep_called_between_fetches_with_at_least_one_second(self):
        src = ipd.InvestopediaSource()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep") as mock_sleep:
            # First fetch: no sleep (last_fetch_at == 0).
            list(src.fetch_chunks("terms/m/money.asp", chunk_chars=2000))
            mock_sleep.assert_not_called()
            # Second fetch on a different slug: must sleep.
            list(src.fetch_chunks("terms/i/income.asp", chunk_chars=2000))
            self.assertTrue(mock_sleep.called)
            # At least one sleep call was for >= 1.0 seconds.
            slept_long_enough = any(
                (call.args and call.args[0] >= 1.0)
                for call in mock_sleep.call_args_list
            )
            self.assertTrue(
                slept_long_enough,
                f"no sleep >= 1.0s; calls={mock_sleep.call_args_list}",
            )


class TestUserAgent(unittest.TestCase):
    def test_user_agent_header_set(self):
        src = ipd.InvestopediaSource()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))) \
                as mock_open, \
             patch("scripts.sources.investopedia.time.sleep"):
            list(src.fetch_chunks("terms/m/money.asp", chunk_chars=2000))
            req = mock_open.call_args.args[0]
            ua = req.get_header("User-agent")
            self.assertIsNotNone(ua)
            self.assertIn("NIMCP-Athena", ua)


class TestNoOnDiskPersistence(unittest.TestCase):
    """Sanity: this backend must not write article content to disk."""

    def test_no_open_writes_during_fetch(self):
        src = ipd.InvestopediaSource()
        with patch("scripts.sources.investopedia.urlopen",
                   return_value=_make_response(_FAKE_HTML.encode("utf-8"))), \
             patch("scripts.sources.investopedia.time.sleep"), \
             patch("builtins.open") as mock_open:
            list(src.fetch_chunks("terms/m/money.asp", chunk_chars=200))
            # The backend itself must not have called open().
            self.assertFalse(
                mock_open.called,
                f"open() called {mock_open.call_count}x during fetch — "
                f"investopedia backend must never persist to disk",
            )


if __name__ == "__main__":
    unittest.main()
