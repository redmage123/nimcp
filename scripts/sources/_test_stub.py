"""Test-only streaming source. Yields a deterministic synthetic stream so
unit tests can exercise the dispatcher without touching the network."""

from __future__ import annotations

from typing import Iterator

import sys, os
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.streaming_ingest import StreamingSource, register_source


_FIXTURE = {
    "alpha": [
        "Alpha chunk one — synthetic test data.",
        "Alpha chunk two — more synthetic test data.",
        "Alpha chunk three — final alpha chunk.",
    ],
    "beta": [
        "Beta chunk one.",
        "Beta chunk two.",
    ],
}


class _StubSource(StreamingSource):
    kind = "_stub"

    def list_for_stage(self, stage: int) -> list[str]:
        # Stage 1 sees alpha, Stage 2 sees beta, Stage 3 sees both.
        if stage == 1:
            return ["alpha"]
        if stage == 2:
            return ["beta"]
        if stage == 3:
            return ["alpha", "beta"]
        return []

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = 1200) -> Iterator[tuple[int, str]]:
        items = _FIXTURE.get(source_id, [])
        for i, text in enumerate(items):
            if i < start_position:
                continue
            yield (i + 1, text)

    def is_complete(self, source_id: str, position: int) -> bool:
        return position >= len(_FIXTURE.get(source_id, []))


register_source(_StubSource())
