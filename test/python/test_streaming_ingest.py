"""Unit tests for scripts/streaming_ingest.py — dispatcher + state I/O.

Exercises the abstract base, registry, atomic state writes, and the
single-chunk pull helper using the bundled `_test_stub` backend, which
self-registers when NIMCP_STREAMING_TEST_STUB=1 is set in the env.

No network, no daemon, no live brain.
"""

import json
import os
import sys
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, REPO)

# Enable the test stub BEFORE importing streaming_ingest. The stub registers
# itself on import of scripts.sources, which streaming_ingest triggers.
os.environ["NIMCP_STREAMING_TEST_STUB"] = "1"

# Force a fresh import so the env flag takes effect for this test run.
for _mod in [
    "scripts.sources._test_stub",
    "scripts.sources",
    "scripts.streaming_ingest",
    "streaming_ingest",
]:
    if _mod in sys.modules:
        del sys.modules[_mod]

from scripts import streaming_ingest as si  # noqa: E402


class TestRegistry(unittest.TestCase):
    def test_stub_registered(self):
        kinds = si.available_kinds()
        self.assertIn("_stub", kinds, f"available_kinds()={kinds}")
        src = si.get_source("_stub")
        self.assertIsNotNone(src)
        self.assertEqual(src.kind, "_stub")

    def test_get_unknown_returns_none(self):
        self.assertIsNone(si.get_source("does_not_exist"))


class TestStubSourceContract(unittest.TestCase):
    def setUp(self):
        self.src = si.get_source("_stub")

    def test_list_for_stage(self):
        self.assertEqual(self.src.list_for_stage(1), ["alpha"])
        self.assertEqual(self.src.list_for_stage(2), ["beta"])
        self.assertEqual(sorted(self.src.list_for_stage(3)),
                         ["alpha", "beta"])
        self.assertEqual(self.src.list_for_stage(99), [])

    def test_fetch_chunks_yields_pairs(self):
        chunks = list(self.src.fetch_chunks("alpha", start_position=0))
        self.assertEqual(len(chunks), 3)
        for next_pos, text in chunks:
            self.assertIsInstance(next_pos, int)
            self.assertIsInstance(text, str)
            self.assertTrue(text)

    def test_fetch_chunks_resumes_at_position(self):
        chunks = list(self.src.fetch_chunks("alpha", start_position=2))
        self.assertEqual(len(chunks), 1)
        # The third item — index 2 in the fixture, position 3.
        next_pos, text = chunks[0]
        self.assertEqual(next_pos, 3)
        self.assertIn("three", text)

    def test_is_complete(self):
        self.assertFalse(self.src.is_complete("alpha", 0))
        self.assertFalse(self.src.is_complete("alpha", 2))
        self.assertTrue(self.src.is_complete("alpha", 3))


class TestIterOneChunk(unittest.TestCase):
    def test_single_chunk_pull(self):
        pair = si.iter_one_chunk("_stub", "alpha", start_position=0)
        self.assertIsNotNone(pair)
        next_pos, text = pair
        self.assertEqual(next_pos, 1)
        self.assertIn("Alpha", text)

    def test_advance_via_returned_position(self):
        # Pull-and-advance loop, the same pattern the dispatcher uses.
        seen = []
        pos = 0
        for _ in range(10):
            pair = si.iter_one_chunk("_stub", "alpha", start_position=pos)
            if pair is None:
                break
            pos, text = pair
            seen.append(text)
        self.assertEqual(len(seen), 3)
        self.assertEqual(pos, 3)

    def test_unknown_kind_returns_none(self):
        self.assertIsNone(si.iter_one_chunk("nope", "alpha"))


class TestStateIO(unittest.TestCase):
    def test_default_state_when_missing(self):
        with tempfile.TemporaryDirectory() as td:
            state = si.load_streaming_state(td)
            self.assertEqual(state["version"], 1)
            self.assertEqual(state["by_kind"], {})
            self.assertEqual(state["totals"]["chunks_ingested"], 0)
            self.assertEqual(state["totals"]["bytes_ingested"], 0)

    def test_save_and_reload_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            state = si.load_streaming_state(td)
            sub = si._kind_state(state, "_stub")
            sub["in_progress"]["alpha"] = 2
            sub["completed"].append("beta")
            state["totals"]["chunks_ingested"] = 5
            state["totals"]["bytes_ingested"] = 12345
            si.save_streaming_state(td, state)
            # File exists and is valid JSON with the right shape.
            path = si.streaming_state_path(td)
            self.assertTrue(os.path.exists(path))
            with open(path) as f:
                disk = json.load(f)
            self.assertEqual(disk["by_kind"]["_stub"]["in_progress"]["alpha"], 2)
            self.assertEqual(disk["by_kind"]["_stub"]["completed"], ["beta"])
            self.assertEqual(disk["totals"]["chunks_ingested"], 5)
            # Reload via the API.
            state2 = si.load_streaming_state(td)
            self.assertEqual(state2["by_kind"]["_stub"]["in_progress"]["alpha"], 2)

    def test_load_corrupt_state_falls_back_to_default(self):
        with tempfile.TemporaryDirectory() as td:
            path = si.streaming_state_path(td)
            with open(path, "w") as f:
                f.write("{not json")
            state = si.load_streaming_state(td)
            self.assertEqual(state["by_kind"], {})

    def test_save_is_atomic_no_tmp_left_behind(self):
        with tempfile.TemporaryDirectory() as td:
            state = si.load_streaming_state(td)
            sub = si._kind_state(state, "_stub")
            sub["in_progress"]["alpha"] = 1
            si.save_streaming_state(td, state)
            leftovers = [n for n in os.listdir(td)
                         if n.startswith(".tmp_streaming_")]
            self.assertEqual(leftovers, [])


class TestKindStateHelper(unittest.TestCase):
    def test_lazy_create_with_keys(self):
        state = si._default_state()
        sub = si._kind_state(state, "_stub")
        self.assertIn("completed", sub)
        self.assertIn("in_progress", sub)
        self.assertEqual(sub["completed"], [])
        self.assertEqual(sub["in_progress"], {})

    def test_idempotent_on_existing(self):
        state = si._default_state()
        sub1 = si._kind_state(state, "_stub")
        sub1["in_progress"]["alpha"] = 7
        sub2 = si._kind_state(state, "_stub")
        # Same dict reference — keeps the existing data.
        self.assertIs(sub1, sub2)
        self.assertEqual(sub2["in_progress"]["alpha"], 7)


if __name__ == "__main__":
    unittest.main()
