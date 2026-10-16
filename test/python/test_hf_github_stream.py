"""Unit tests for scripts/sources/hf_github.py — HF datasets-server backend.

All network calls are mocked. We never touch datasets-server.huggingface.co
from these tests. Coverage:
  - Backend self-registers under kind="hf_github"
  - list_for_stage() returns [] for stage 1 and ints/lists for 2/3
  - fetch_chunks() yields (packed_position, text) chunks with monotonic
    positions and correctly packs row_offset * 1_000_000 + char_offset
  - Resume from start_position = 1_000_000 picks up at row 1, not row 0
  - Language filter skips rows where row.lang != source_id
  - A long content row gets split into multiple chunk_chars-sized pieces
  - Network errors propagate (don't get swallowed)
  - Rate limit: time.sleep called with >= 1.0 seconds between fetches
  - User-Agent header is set on the urllib Request
  - NIMCP_HF_GITHUB_DATASET env override changes the URL
  - Abandoned iterator after one chunk doesn't raise / leak state
"""

from __future__ import annotations

import importlib
import io
import json
import os
import sys
import unittest
import urllib.error
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

# Force a fresh import so the registry is in a known state for this run.
for _mod in [
    "scripts.sources.hf_github",
    "scripts.sources",
    "scripts.streaming_ingest",
]:
    if _mod in sys.modules:
        del sys.modules[_mod]

from scripts import streaming_ingest as si  # noqa: E402
from scripts.sources import hf_github as hf_mod  # noqa: E402


# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------


def _resp(body_bytes: bytes):
    """Build a context-manager-shaped mock for urllib.request.urlopen.

    `read()` returns the bytes payload; `__enter__/__exit__` make it
    usable in a `with urllib.request.urlopen(...) as resp:` block.
    """
    cm = mock.MagicMock()
    cm.read.return_value = body_bytes
    cm.__enter__.return_value = cm
    cm.__exit__.return_value = False
    return cm


def _fake_rows_response(rows, *, num_rows_total=None):
    """Build a mock urlopen() response carrying these rows.

    `rows` is a list of dicts (the inner `row` payloads). We wrap them
    as the datasets-server /rows endpoint does, with row_idx + row.
    """
    if num_rows_total is None:
        num_rows_total = len(rows)
    body = json.dumps({
        "rows": [{"row_idx": i, "row": r} for i, r in enumerate(rows)],
        "num_rows_total": num_rows_total,
    }).encode("utf-8")
    return _resp(body)


def _make_urlopen_returning(payload_factory):
    """Wrap a payload-producing callable into a urlopen-shaped MagicMock.

    Each call to the resulting mock invokes payload_factory(*args, **kw)
    and returns a context-manager response carrying its bytes.
    """
    def _side_effect(*args, **kwargs):
        payload = payload_factory(*args, **kwargs)
        if isinstance(payload, bytes):
            return _resp(payload)
        if isinstance(payload, str):
            return _resp(payload.encode("utf-8"))
        # Already a response-shaped object.
        return payload
    return mock.MagicMock(side_effect=_side_effect)


# ----------------------------------------------------------------------------
# Registration
# ----------------------------------------------------------------------------


class TestRegistration(unittest.TestCase):
    def test_hf_github_registered(self):
        self.assertIn("hf_github", si.available_kinds())
        src = si.get_source("hf_github")
        self.assertIsNotNone(src)
        self.assertIsInstance(src, hf_mod.HFGithubSource)
        self.assertEqual(src.kind, "hf_github")


# ----------------------------------------------------------------------------
# list_for_stage
# ----------------------------------------------------------------------------


class TestListForStage(unittest.TestCase):
    def setUp(self):
        self.src = hf_mod.HFGithubSource()

    def test_stage_1_empty(self):
        self.assertEqual(self.src.list_for_stage(1), [])

    def test_stage_0_and_negative_empty(self):
        self.assertEqual(self.src.list_for_stage(0), [])
        self.assertEqual(self.src.list_for_stage(-1), [])

    def test_stage_2_returns_python_and_c(self):
        langs = self.src.list_for_stage(2)
        self.assertIn("Python", langs)
        self.assertIn("C", langs)
        self.assertGreaterEqual(len(langs), 2)

    def test_stage_3_broader(self):
        langs = self.src.list_for_stage(3)
        self.assertIn("Python", langs)
        self.assertIn("JavaScript", langs)
        self.assertIn("Go", langs)
        self.assertIn("Rust", langs)


# ----------------------------------------------------------------------------
# Position packing
# ----------------------------------------------------------------------------


class TestPositionPacking(unittest.TestCase):
    def test_pack_unpack_zero(self):
        self.assertEqual(hf_mod._pack_position(0, 0), 0)
        self.assertEqual(hf_mod._unpack_position(0), (0, 0))

    def test_pack_char_offset_only(self):
        # row_offset 0, char_offset N → packed = N
        self.assertEqual(hf_mod._pack_position(0, 500), 500)
        row, col = hf_mod._unpack_position(500)
        self.assertEqual((row, col), (0, 500))

    def test_pack_row_offset_only(self):
        # row_offset 1, char_offset 0 → packed = 1_000_000
        self.assertEqual(hf_mod._pack_position(1, 0), 1_000_000)
        row, col = hf_mod._unpack_position(1_000_000)
        self.assertEqual((row, col), (1, 0))

    def test_pack_combined(self):
        self.assertEqual(hf_mod._pack_position(2, 1234),
                         2 * 1_000_000 + 1234)
        row, col = hf_mod._unpack_position(2 * 1_000_000 + 1234)
        self.assertEqual((row, col), (2, 1234))

    def test_negative_clamped(self):
        self.assertEqual(hf_mod._pack_position(-5, -1), 0)
        self.assertEqual(hf_mod._unpack_position(-1), (0, 0))


# ----------------------------------------------------------------------------
# Content chunk splitting
# ----------------------------------------------------------------------------


class TestSplitContentChunks(unittest.TestCase):
    def test_short_content_single_chunk(self):
        chunks = list(hf_mod._split_content_chunks(
            "hello world", start_char=0, chunk_chars=1200))
        self.assertEqual(chunks, [(11, "hello world")])

    def test_empty_content_no_chunks(self):
        self.assertEqual(
            list(hf_mod._split_content_chunks(
                "", start_char=0, chunk_chars=100)),
            [])

    def test_long_content_split(self):
        # 5000 chars of repeating "abc\n" — easy to chunk on \n.
        content = ("abc\n" * 1250)  # 5000 chars
        chunks = list(hf_mod._split_content_chunks(
            content, start_char=0, chunk_chars=500))
        self.assertGreater(len(chunks), 1)
        # Final position = len(content)
        self.assertEqual(chunks[-1][0], len(content))
        # Concatenation reproduces the original.
        self.assertEqual("".join(c for _, c in chunks), content)
        # Positions strictly monotonic.
        last = -1
        for pos, _ in chunks:
            self.assertGreater(pos, last)
            last = pos

    def test_resume_from_char_offset(self):
        content = "abcdefghij" * 200  # 2000 chars
        # First chunk
        chunks_full = list(hf_mod._split_content_chunks(
            content, start_char=0, chunk_chars=500))
        midway = chunks_full[0][0]
        # Resume at midway should produce exactly the rest.
        chunks_resume = list(hf_mod._split_content_chunks(
            content, start_char=midway, chunk_chars=500))
        joined = "".join(c for _, c in chunks_resume)
        self.assertEqual(joined, content[midway:])
        # Final reported pos still equals total length.
        self.assertEqual(chunks_resume[-1][0], len(content))


# ----------------------------------------------------------------------------
# fetch_chunks against mocked urlopen
# ----------------------------------------------------------------------------


class TestFetchChunks(unittest.TestCase):
    def setUp(self):
        self.src = hf_mod.HFGithubSource()
        self._sleep_patcher = mock.patch.object(hf_mod.time, "sleep")
        self.mock_sleep = self._sleep_patcher.start()

    def tearDown(self):
        self._sleep_patcher.stop()

    def test_yields_chunks_with_monotonic_positions(self):
        rows = [
            {"lang": "Python", "content": "print('a')\n", "path": "a.py"},
            {"lang": "Python", "content": "print('b')\n", "path": "b.py"},
            {"lang": "Python", "content": "print('c')\n", "path": "c.py"},
        ]
        # First call returns rows; second call returns empty (EOF).
        responses = [_fake_rows_response(rows),
                     _fake_rows_response([], num_rows_total=3)]
        with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                side_effect=responses) as _u:
            chunks = list(self.src.fetch_chunks(
                "Python", start_position=0, chunk_chars=1200))
        self.assertGreater(len(chunks), 0)
        last_pos = -1
        for next_pos, text in chunks:
            self.assertIsInstance(next_pos, int)
            self.assertIsInstance(text, str)
            self.assertGreater(next_pos, last_pos,
                               f"position regressed: {next_pos} <= {last_pos}")
            last_pos = next_pos

    def test_chunk_position_advances_to_next_row_after_row_consumed(self):
        rows = [
            {"lang": "Python", "content": "row zero contents\n", "path": "0"},
        ]
        responses = [_fake_rows_response(rows),
                     _fake_rows_response([], num_rows_total=1)]
        with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                side_effect=responses):
            chunks = list(self.src.fetch_chunks(
                "Python", start_position=0, chunk_chars=1200))
        # The single row produces a single chunk; its packed next_position
        # must point to row 1, char 0 = 1_000_000.
        self.assertEqual(len(chunks), 1)
        self.assertEqual(chunks[0][0], 1_000_000)
        self.assertEqual(chunks[0][1], "row zero contents\n")

    def test_resume_from_row_one(self):
        # Build 2 rows. Resume start_position = 1_000_000 → first chunk
        # comes from row index 1, not 0.
        rows = [
            {"lang": "Python", "content": "ROW0_CONTENT\n", "path": "0"},
            {"lang": "Python", "content": "ROW1_CONTENT\n", "path": "1"},
        ]
        # When resuming, the source asks for offset=1 first.
        # Our mock will return rows starting at index 1 — but datasets-server
        # always re-indexes from 0 in its response. The implementation
        # tolerates both since it advances based on the row's reported
        # row_idx field. We model server behaviour: rows come back with
        # row_idx counting from the requested offset.
        def _row_response_at_offset(offset_int):
            visible = rows[offset_int:]
            body = json.dumps({
                "rows": [{"row_idx": offset_int + i, "row": r}
                         for i, r in enumerate(visible)],
                "num_rows_total": len(rows),
            }).encode("utf-8")
            return _resp(body)

        def _factory(*args, **kwargs):
            req = args[0]
            url = req.full_url
            # Pull `offset` out of the URL.
            from urllib.parse import urlparse, parse_qs
            qs = parse_qs(urlparse(url).query)
            offset = int(qs.get("offset", ["0"])[0])
            visible = rows[offset:]
            return json.dumps({
                "rows": [{"row_idx": offset + i, "row": r}
                         for i, r in enumerate(visible)],
                "num_rows_total": len(rows),
            }).encode("utf-8")

        with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                _make_urlopen_returning(_factory)):
            chunks = list(self.src.fetch_chunks(
                "Python", start_position=1_000_000, chunk_chars=1200))
        # First chunk must be row 1's content.
        self.assertGreater(len(chunks), 0)
        self.assertEqual(chunks[0][1], "ROW1_CONTENT\n")
        # Row 0 must NOT appear anywhere.
        for _, text in chunks:
            self.assertNotIn("ROW0_CONTENT", text)

    def test_language_filter_skips_non_matching_rows(self):
        rows = [
            {"lang": "JavaScript", "content": "JS content\n", "path": "x.js"},
            {"lang": "Python", "content": "PY content\n", "path": "y.py"},
            {"lang": "C", "content": "C content\n", "path": "z.c"},
        ]
        responses = [_fake_rows_response(rows),
                     _fake_rows_response([], num_rows_total=3)]
        with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                side_effect=responses):
            chunks = list(self.src.fetch_chunks(
                "Python", start_position=0, chunk_chars=1200))
        joined = "".join(c for _, c in chunks)
        self.assertIn("PY content", joined)
        self.assertNotIn("JS content", joined)
        self.assertNotIn("C content", joined)

    def test_long_content_row_split_into_multiple_chunks(self):
        big = ("def f():\n    return 1\n\n" * 500)  # ~10K chars
        rows = [{"lang": "Python", "content": big, "path": "big.py"}]
        responses = [_fake_rows_response(rows),
                     _fake_rows_response([], num_rows_total=1)]
        with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                side_effect=responses):
            chunks = list(self.src.fetch_chunks(
                "Python", start_position=0, chunk_chars=500))
        # Multi-chunk row.
        self.assertGreater(len(chunks), 1)
        # Concatenation reproduces the row.
        joined = "".join(c for _, c in chunks)
        self.assertEqual(joined, big)
        # Final position must point to the next row (row 1, char 0).
        self.assertEqual(chunks[-1][0], 1_000_000)

    def test_user_agent_header_is_set(self):
        rows = [{"lang": "Python", "content": "print()\n", "path": "x"}]
        responses = [_fake_rows_response(rows),
                     _fake_rows_response([], num_rows_total=1)]
        with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                side_effect=responses) as mu:
            list(self.src.fetch_chunks(
                "Python", start_position=0, chunk_chars=1200))
        # First call's Request object.
        req = mu.call_args_list[0].args[0]
        ua = req.get_header("User-agent")
        self.assertIsNotNone(ua)
        self.assertIn("NIMCP-Athena", ua)
        self.assertIn("mailto:", ua)


# ----------------------------------------------------------------------------
# Network errors propagate
# ----------------------------------------------------------------------------


class TestNetworkErrorRaises(unittest.TestCase):
    def test_url_error_propagates(self):
        src = hf_mod.HFGithubSource()
        with mock.patch.object(hf_mod.time, "sleep"):
            with mock.patch.object(
                    hf_mod.urllib.request, "urlopen",
                    side_effect=urllib.error.URLError("boom")):
                gen = src.fetch_chunks("Python", chunk_chars=400)
                with self.assertRaises(urllib.error.URLError):
                    next(gen)

    def test_http_error_propagates(self):
        src = hf_mod.HFGithubSource()
        with mock.patch.object(hf_mod.time, "sleep"):
            err = urllib.error.HTTPError(
                "http://x", 503, "Service Unavailable", {}, None)
            with mock.patch.object(
                    hf_mod.urllib.request, "urlopen", side_effect=err):
                gen = src.fetch_chunks("Python", chunk_chars=400)
                with self.assertRaises(urllib.error.HTTPError):
                    next(gen)


# ----------------------------------------------------------------------------
# Rate limit
# ----------------------------------------------------------------------------


class TestRateLimitSleep(unittest.TestCase):
    def test_sleep_called_with_at_least_one_second(self):
        src = hf_mod.HFGithubSource()
        rows1 = [{"lang": "Python", "content": "a\n", "path": "x"}]
        rows2 = [{"lang": "Python", "content": "b\n", "path": "y"}]
        # Two paginated responses + one empty terminator.
        def _factory(*args, **kwargs):
            req = args[0]
            from urllib.parse import urlparse, parse_qs
            qs = parse_qs(urlparse(req.full_url).query)
            offset = int(qs.get("offset", ["0"])[0])
            if offset == 0:
                visible = [{"row_idx": 0, "row": rows1[0]}]
                payload = {"rows": visible, "num_rows_total": 2}
            elif offset == 1:
                visible = [{"row_idx": 1, "row": rows2[0]}]
                payload = {"rows": visible, "num_rows_total": 2}
            else:
                payload = {"rows": [], "num_rows_total": 2}
            return json.dumps(payload).encode("utf-8")

        with mock.patch.object(hf_mod.time, "sleep") as mock_sleep:
            with mock.patch.object(hf_mod.urllib.request, "urlopen",
                                    _make_urlopen_returning(_factory)):
                list(src.fetch_chunks("Python", chunk_chars=400))
        self.assertGreater(mock_sleep.call_count, 0,
                           "Rate-limit sleep was never invoked")
        durations = [c.args[0] for c in mock_sleep.call_args_list if c.args]
        self.assertTrue(
            any(d >= 1.0 for d in durations),
            f"No sleep>=1.0s observed; saw {durations}")


# ----------------------------------------------------------------------------
# Env override
# ----------------------------------------------------------------------------


class TestEnvOverride(unittest.TestCase):
    def test_dataset_env_override_changes_url(self):
        # Construct a fresh source with the env var set.
        with mock.patch.dict(os.environ,
                              {"NIMCP_HF_GITHUB_DATASET": "foo/bar"}):
            src = hf_mod.HFGithubSource()
            self.assertEqual(src.dataset, "foo/bar")
            with mock.patch.object(hf_mod.time, "sleep"):
                with mock.patch.object(
                        hf_mod.urllib.request, "urlopen",
                        side_effect=[
                            _fake_rows_response([], num_rows_total=0),
                        ]) as mu:
                    list(src.fetch_chunks("Python", chunk_chars=400))
            self.assertGreater(mu.call_count, 0)
            req = mu.call_args_list[0].args[0]
            self.assertIn("foo%2Fbar", req.full_url)


# ----------------------------------------------------------------------------
# is_complete
# ----------------------------------------------------------------------------


class TestIsComplete(unittest.TestCase):
    def test_unknown_source_returns_false(self):
        src = hf_mod.HFGithubSource()
        # No size hint cached → must return False.
        self.assertFalse(src.is_complete("Python", 0))
        self.assertFalse(src.is_complete("Python", 1_000_000))

    def test_with_size_hint_complete_when_past_end(self):
        src = hf_mod.HFGithubSource()
        src._dataset_size_hint["Python"] = 5
        # Position before end.
        self.assertFalse(src.is_complete("Python", 0))
        self.assertFalse(src.is_complete("Python", 4_000_000))
        # Position at/past end.
        self.assertTrue(src.is_complete("Python", 5_000_000))
        self.assertTrue(src.is_complete("Python", 10_000_000))

    def test_complete_drops_row_cache(self):
        src = hf_mod.HFGithubSource()
        src._dataset_size_hint["Python"] = 2
        src._row_cache["Python"] = (1, "stale content")
        self.assertTrue(src.is_complete("Python", 2_000_000))
        self.assertNotIn("Python", src._row_cache)


# ----------------------------------------------------------------------------
# Abandoned iterator
# ----------------------------------------------------------------------------


class TestAbandonedIterator(unittest.TestCase):
    """fetch_chunks must tolerate being abandoned mid-stream without
    raising on cleanup."""

    def test_abandon_after_one_chunk(self):
        src = hf_mod.HFGithubSource()
        rows = [
            {"lang": "Python", "content": "first\n", "path": "1"},
            {"lang": "Python", "content": "second\n", "path": "2"},
        ]
        responses = [_fake_rows_response(rows),
                     _fake_rows_response([], num_rows_total=2)]
        with mock.patch.object(hf_mod.time, "sleep"):
            with mock.patch.object(
                    hf_mod.urllib.request, "urlopen",
                    side_effect=responses):
                gen = src.fetch_chunks("Python", chunk_chars=400)
                first = next(gen)
                self.assertIsInstance(first, tuple)
                # Closing the generator must not raise.
                gen.close()

    def test_abandon_before_first_next_no_state_leak(self):
        # Instantiate but never call next() — fetch_chunks defers the
        # network call to the first __next__, so dropping the gen here
        # must not have triggered any cache writes.
        src = hf_mod.HFGithubSource()
        with mock.patch.object(hf_mod.urllib.request, "urlopen") as mu:
            gen = src.fetch_chunks("Python", chunk_chars=400)
            # Drop reference before pulling.
            del gen
            self.assertEqual(mu.call_count, 0)
            self.assertEqual(src._row_cache, {})


if __name__ == "__main__":
    unittest.main()
