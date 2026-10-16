"""HuggingFace datasets-server streaming source for code samples (CE-5b).

Streams rows from a public HF dataset (default: bigcode/the-stack-smol-xs)
via the datasets-server /rows endpoint. Each row's `content` field is one
streaming chunk (the source file body), language-tagged in the metadata.

NEVER persists to disk — pod storage policy. Only the resume offset per
language slug lives in .streaming_corpus_state.json.

Configurable via env:
  NIMCP_HF_GITHUB_DATASET   — override dataset name (default below)
  NIMCP_HF_GITHUB_CONFIG    — override config (default "default")

Position cursor design
----------------------
Each language slug streams rows from /rows. Within a row, the `content`
string can be many KB so we further split it into chunk_chars-sized pieces
aligned on "\\n\\n" where possible. We therefore need a cursor that
encodes BOTH the absolute row offset AND the within-row character offset.

We pack the cursor into a single int as

    position = row_offset * 1_000_000 + char_offset

where char_offset is reset to 0 each time we cross a row boundary. The
1_000_000 multiplier comfortably covers individual code files since the
HF datasets-server `/rows` endpoint truncates `content` to <1MB anyway,
and any oversized content we'd see in practice is sliced into chunks
before it hits this cursor.

Cross-ref scripts/streaming_ingest.py for the full contract:
  * Streaming only — no on-disk write of fetched code, ever.
  * Network failures raise — the dispatcher catches and skips.
  * try/finally on the generator so abandoned iterators don't leak.
  * Self-registers at module import via register_source().
"""

from __future__ import annotations

import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Iterator, Optional

# Allow direct-script imports (mirrors arxiv.py / wikipedia.py shape).
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.streaming_ingest import (  # noqa: E402
    DEFAULT_CHUNK_CHARS,
    StreamingSource,
    register_source,
)


# ----------------------------------------------------------------------------
# Constants
# ----------------------------------------------------------------------------

# datasets-server endpoint root. Public datasets need no auth; gated ones
# 401. We deliberately pick a non-gated dataset by default.
HF_DATASETS_SERVER_BASE = "https://datasets-server.huggingface.co"

# Default dataset: small, public, license-permissive subset of The Stack.
# Override via NIMCP_HF_GITHUB_DATASET when the upstream availability
# shifts (HF datasets get gated/renamed regularly).
_DEFAULT_DATASET = "bigcode/the-stack-smol-xs"
_DEFAULT_CONFIG = "default"

# How many rows per /rows request. The datasets-server caps `length` at
# 100; we use 50 for a balance between bandwidth and latency.
_ROWS_PER_REQUEST = 50

# Position-packing constant. See module docstring.
_ROW_MULTIPLIER = 1_000_000

# Polite gap between back-to-back HTTP requests against the
# datasets-server. HF doesn't publish a hard rate limit on this endpoint
# but a 1.0s spacing keeps us comfortably below any throttling.
HF_RATE_LIMIT_SECONDS = 1.0

# Identify ourselves so HF can find us in their logs if there's a problem.
HF_USER_AGENT = (
    "NIMCP-Athena/1.0 streaming research; mailto:claude@ai-elevate.ai"
)

HF_REQUEST_TIMEOUT = 30.0


# ----------------------------------------------------------------------------
# Stage rosters
# ----------------------------------------------------------------------------
# Stage 1: nothing — vocab isn't ready for code at the toddler stage.
# Stage 2: a couple of beginner-friendly languages that resemble English
#          prose enough to be useful even at intermediate vocab.
# Stage 3: broader coverage including systems languages.
_STAGE_2_LANGS = ["Python", "C"]
_STAGE_3_LANGS = ["Python", "C", "JavaScript", "Go", "Rust"]


# ----------------------------------------------------------------------------
# Position packing helpers (test-visible)
# ----------------------------------------------------------------------------


def _pack_position(row_offset: int, char_offset: int) -> int:
    """Encode (row_offset, char_offset) as a single integer.

    See the module docstring for the rationale. Negative inputs are
    clamped to zero — defensive against caller bugs."""
    if row_offset < 0:
        row_offset = 0
    if char_offset < 0:
        char_offset = 0
    return row_offset * _ROW_MULTIPLIER + char_offset


def _unpack_position(position: int) -> tuple[int, int]:
    """Inverse of _pack_position."""
    if position < 0:
        return (0, 0)
    return divmod(position, _ROW_MULTIPLIER)


# ----------------------------------------------------------------------------
# Chunking
# ----------------------------------------------------------------------------


def _split_content_chunks(content: str, *, start_char: int,
                           chunk_chars: int
                           ) -> Iterator[tuple[int, str]]:
    """Split `content[start_char:]` into chunks of ~chunk_chars.

    Yields (next_char_offset_within_row, chunk_text). The next_char_offset
    is the absolute position WITHIN the row at which the next chunk would
    pick up — so the final yielded offset equals len(content), which is
    the signal the row is fully consumed.

    Boundary alignment: prefer the last "\\n\\n" within the chunk window;
    otherwise the last single "\\n"; otherwise hard-cut at chunk_chars.
    """
    if not content:
        return
    if chunk_chars <= 0:
        # Defensive — caller violated contract. Yield everything as one.
        yield (len(content), content[start_char:] if start_char < len(content)
               else "")
        return
    n = len(content)
    pos = max(0, int(start_char))
    while pos < n:
        end = pos + chunk_chars
        if end >= n:
            # Final piece.
            yield (n, content[pos:n])
            return
        window = content[pos:end]
        # Prefer paragraph break.
        cut_rel = window.rfind("\n\n")
        if cut_rel != -1 and cut_rel > 0:
            cut = pos + cut_rel + 2
        else:
            line_rel = window.rfind("\n")
            if line_rel != -1 and line_rel > 0:
                cut = pos + line_rel + 1
            else:
                cut = end
        if cut <= pos:
            # Defensive — guarantee forward progress on pathological text.
            cut = min(pos + chunk_chars, n)
        yield (cut, content[pos:cut])
        pos = cut


# ----------------------------------------------------------------------------
# Source implementation
# ----------------------------------------------------------------------------


class HFGithubSource(StreamingSource):
    """Streaming source backed by the public HuggingFace datasets-server.

    Each `source_id` is a language slug (matching the dataset's `lang`
    column). Iteration scans the dataset for rows whose `lang` matches,
    and chunk-splits each matching row's `content` field.
    """

    kind = "hf_github"

    # Read env at class-attribute time so tests that set
    # NIMCP_HF_GITHUB_DATASET before importing this module pick up the
    # override automatically. The instance also re-reads in __init__()
    # so a fresh HFGithubSource() picks up env changes mid-process.
    DATASET = os.environ.get("NIMCP_HF_GITHUB_DATASET", _DEFAULT_DATASET)
    CONFIG = os.environ.get("NIMCP_HF_GITHUB_CONFIG", _DEFAULT_CONFIG)
    BASE = HF_DATASETS_SERVER_BASE

    def __init__(self) -> None:
        # Re-read env at instantiation so that test code which sets the
        # env var and then constructs a fresh source picks up the value
        # without needing to reload the module.
        self.dataset = os.environ.get("NIMCP_HF_GITHUB_DATASET",
                                       self.DATASET)
        self.config = os.environ.get("NIMCP_HF_GITHUB_CONFIG", self.CONFIG)
        # In-memory cache of the most recently fetched row per source_id.
        # Key: source_id (language slug). Value: (row_offset, content_str)
        # pair. Dropped when we move past the row or when is_complete()
        # is called. NEVER persisted.
        self._row_cache: dict[str, tuple[int, str]] = {}
        # Last network-request time (monotonic seconds) for rate limiting.
        self._last_request_time: float = 0.0
        # Optional total-row-count cache from the /rows responses, keyed
        # by source_id. We don't pre-fetch /splits — we just remember
        # what /rows tells us in `num_rows_total`.
        self._dataset_size_hint: dict[str, int] = {}

    # ------------------------------------------------------------------
    # StreamingSource API
    # ------------------------------------------------------------------

    def list_for_stage(self, stage: int) -> list[str]:
        if stage <= 1:
            return []
        if stage == 2:
            return list(_STAGE_2_LANGS)
        # Stage 3 and higher.
        return list(_STAGE_3_LANGS)

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = DEFAULT_CHUNK_CHARS
                     ) -> Iterator[tuple[int, str]]:
        # Defer body to a separate generator function so the rate-limit
        # sleep + network fetch happen lazily on the first __next__()
        # call. This matches the iter_one_chunk() caller pattern.
        return self._chunks_generator(source_id,
                                       start_position=start_position,
                                       chunk_chars=chunk_chars)

    def is_complete(self, source_id: str, position: int) -> bool:
        size_hint = self._dataset_size_hint.get(source_id)
        if size_hint is None:
            # We have no length information without doing a network
            # round-trip; the caller will keep polling and natural EOF
            # (a row fetch returning zero rows) will stop progress.
            return False
        row_offset, _ = _unpack_position(position)
        done = row_offset >= size_hint
        if done:
            # Drop any cached row content — we're done with this slug.
            self._row_cache.pop(source_id, None)
        return done

    # ------------------------------------------------------------------
    # Internals (test-visible, prefixed with _)
    # ------------------------------------------------------------------

    def _chunks_generator(self, source_id: str, *,
                          start_position: int,
                          chunk_chars: int) -> Iterator[tuple[int, str]]:
        """The actual generator implementation.

        Iterates rows from the datasets-server, filtering by language.
        Row content is split into chunk_chars-sized pieces; cursor packing
        keeps caller resumable across both row and intra-row boundaries.
        """
        try:
            row_offset, char_offset = _unpack_position(start_position)
            # Outer loop: fetch a batch of rows starting at row_offset,
            # iterate until we exhaust or reach a known dataset end.
            while True:
                batch = self._fetch_rows(source_id,
                                          row_offset=row_offset,
                                          length=_ROWS_PER_REQUEST)
                if not batch["rows"]:
                    # No more rows — natural end of dataset. Caller can
                    # observe via is_complete() afterwards.
                    return
                advanced = False
                for entry in batch["rows"]:
                    # The /rows endpoint nests the dataset row under
                    # "row" alongside "row_idx". We tolerate the row_idx
                    # being absent and reconstruct from our own offset.
                    row_idx = entry.get("row_idx", row_offset)
                    row_data = entry.get("row", {})
                    # Language filter: only keep rows tagged with our
                    # source_id. Rows in other languages we silently
                    # advance past (next_position skips them).
                    lang = row_data.get("lang")
                    if lang != source_id:
                        # Skip and advance the row cursor past this row.
                        row_offset = row_idx + 1
                        char_offset = 0
                        advanced = True
                        continue
                    content = row_data.get("content")
                    if not isinstance(content, str) or not content:
                        # Empty / missing content — skip this row too.
                        row_offset = row_idx + 1
                        char_offset = 0
                        advanced = True
                        continue
                    # Cache the row content (in memory only).
                    self._row_cache[source_id] = (row_idx, content)
                    # Chunk-split the row body, starting at char_offset
                    # if we're resuming partway through this row.
                    for next_char, chunk_text in _split_content_chunks(
                            content,
                            start_char=char_offset,
                            chunk_chars=chunk_chars):
                        if next_char >= len(content):
                            # Last chunk of this row — next_position
                            # bumps to the start of the next row.
                            packed = _pack_position(row_idx + 1, 0)
                        else:
                            packed = _pack_position(row_idx, next_char)
                        yield (packed, chunk_text)
                    # Row fully consumed — drop its cache entry and
                    # advance our cursor to the next row.
                    self._row_cache.pop(source_id, None)
                    row_offset = row_idx + 1
                    char_offset = 0
                    advanced = True
                # Update dataset size hint if the response carried it.
                size_hint = batch.get("num_rows_total")
                if isinstance(size_hint, int) and size_hint > 0:
                    self._dataset_size_hint[source_id] = size_hint
                    if row_offset >= size_hint:
                        return
                if not advanced:
                    # Defensive: no rows matched and we didn't move the
                    # cursor — break to avoid infinite looping. This
                    # happens only if the server returns zero entries
                    # but we hadn't already exited via the empty check.
                    return
        finally:
            # Generator cleanup. The _row_cache entry (if any) is left
            # on `self` intentionally — callers may instantiate a fresh
            # generator to resume past the cached row, and the cache
            # avoids re-fetching. Anything truly stale is dropped on
            # is_complete() or on the next mismatched row read.
            pass

    def _fetch_rows(self, source_id: str, *,
                    row_offset: int, length: int) -> dict:
        """One rate-limited GET against the /rows endpoint.

        Returns the parsed JSON payload as a dict. Raises (URLError,
        HTTPError, JSONDecodeError, etc.) on any failure — the dispatcher
        catches per the contract.
        """
        self._respect_rate_limit()
        params = {
            "dataset": self.dataset,
            "config": self.config,
            "split": "train",
            "offset": str(max(0, int(row_offset))),
            "length": str(max(1, int(length))),
        }
        query = urllib.parse.urlencode(params)
        url = f"{self.BASE}/rows?{query}"
        request = urllib.request.Request(
            url, headers={"User-Agent": HF_USER_AGENT, "Accept": "application/json"},
        )
        # Mark request time BEFORE the round-trip so subsequent requests
        # are spaced from when we fired the request, not when it returned.
        self._last_request_time = time.monotonic()
        with urllib.request.urlopen(
                request, timeout=HF_REQUEST_TIMEOUT) as response:
            payload = response.read()
        if isinstance(payload, bytes):
            payload = payload.decode("utf-8", errors="replace")
        # Parse — let JSONDecodeError surface to the caller.
        data = json.loads(payload)
        if not isinstance(data, dict):
            return {"rows": []}
        # Normalize so downstream code can rely on shape.
        rows = data.get("rows")
        if not isinstance(rows, list):
            data["rows"] = []
        return data

    def _respect_rate_limit(self) -> None:
        """Sleep, if necessary, to keep at least HF_RATE_LIMIT_SECONDS
        between successive network requests."""
        elapsed = time.monotonic() - self._last_request_time
        wait = HF_RATE_LIMIT_SECONDS - elapsed
        if wait > 0:
            time.sleep(wait)
        elif self._last_request_time == 0.0:
            # First call: still pay the polite delay so we don't burst
            # the API on cold start.
            time.sleep(HF_RATE_LIMIT_SECONDS)


# Self-register at import time so streaming_ingest.get_source("hf_github")
# resolves once `from scripts.sources import hf_github` (or the package
# import) has run.
register_source(HFGithubSource())
