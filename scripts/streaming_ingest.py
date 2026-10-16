"""Streaming corpus ingestion for Athena training.

Foundation layer. Sources fetch text on demand and pipe directly into the
brain's train_language / learn_language calls without any on-disk corpus
storage. Compare scripts/canonical_corpus.py: that one reads from
data/<corpus>/, this one talks to remote APIs.

Resume state is persisted (a few KB per source kind), but no source CONTENT
is ever written to disk. This is required by the deployment's storage
constraints — the Tolkien-canon-on-disk approach used by canonical_corpus
does not scale to Wikipedia / arXiv / Gutenberg-STEM.

Public API:
    register_source(source: StreamingSource)         — at module import
    get_source(kind: str) -> StreamingSource | None
    available_kinds() -> list[str]

    load_streaming_state(checkpoint_dir) -> dict
    save_streaming_state(checkpoint_dir, state)
    streaming_state_path(checkpoint_dir) -> str

    iter_one_chunk(kind, source_id, start_position, chunk_chars)
        -> (next_position, text) | None

    StreamingSource (abstract base):
        kind                                          str class attr
        list_for_stage(stage) -> list[str]            curated source_ids
        fetch_chunks(source_id, start_position,
                     chunk_chars)
            -> Iterator[tuple[int, str]]              streaming generator
        is_complete(source_id, position) -> bool      end-of-source check

Per-source backends live under scripts/sources/ and self-register via
register_source() at import time. The drivers in immerse_athena.py call
get_source(kind) to dispatch.
"""

from __future__ import annotations

import json
import os
import tempfile
from typing import Iterator, Optional


STREAMING_STATE_FILENAME = ".streaming_corpus_state.json"
DEFAULT_CHUNK_CHARS = 1200


# ----------------------------------------------------------------------------
# StreamingSource abstract base
# ----------------------------------------------------------------------------


class StreamingSource:
    """Abstract base for a streaming corpus source.

    Subclasses set `kind` to a stable string and implement the three methods.
    Subclasses must NOT persist source content to disk between calls.
    """

    kind: str = ""

    def list_for_stage(self, stage: int) -> list[str]:
        """Return curated source_ids (article titles, book IDs, paper IDs,
        category slugs, etc.) appropriate for this stage. Order matters for
        round-robin scheduling but the caller may shuffle."""
        raise NotImplementedError

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = DEFAULT_CHUNK_CHARS
                     ) -> Iterator[tuple[int, str]]:
        """Yield (next_position, chunk_text) tuples.

        The first yielded chunk picks up at start_position. Each yielded
        next_position is what the CALLER should pass back as start_position
        on the next call to resume past this chunk.

        Caller iterates as many chunks as it wants then drops the iterator.
        Implementations MUST tolerate being abandoned mid-iteration without
        leaking resources (use try/finally for any cleanup).

        Network failures should raise an exception; the dispatcher will
        skip the source for this call and not advance position."""
        raise NotImplementedError

    def is_complete(self, source_id: str, position: int) -> bool:
        """Return True iff position represents the end of source_id.

        Used by the dispatcher to flag a source as completed in the state
        file so it isn't re-visited. Implementations that don't have a
        well-defined end (infinite category feed) may always return False;
        the source will keep being polled until the user marks it done
        externally."""
        return False


# ----------------------------------------------------------------------------
# Source registry
# ----------------------------------------------------------------------------


_REGISTRY: dict[str, StreamingSource] = {}


def register_source(source: StreamingSource) -> None:
    """Idempotent registration. Re-registering the same kind overwrites."""
    if not source.kind:
        raise ValueError("StreamingSource.kind must be set")
    _REGISTRY[source.kind] = source


def get_source(kind: str) -> Optional[StreamingSource]:
    return _REGISTRY.get(kind)


def available_kinds() -> list[str]:
    return sorted(_REGISTRY.keys())


# ----------------------------------------------------------------------------
# Resume state I/O
# ----------------------------------------------------------------------------


def streaming_state_path(checkpoint_dir: str,
                          filename: str = STREAMING_STATE_FILENAME) -> str:
    return os.path.join(checkpoint_dir, filename)


def _default_state() -> dict:
    return {
        "version": 1,
        "by_kind": {},
        "totals": {"chunks_ingested": 0, "bytes_ingested": 0},
    }


def load_streaming_state(checkpoint_dir: str,
                          filename: str = STREAMING_STATE_FILENAME) -> dict:
    path = streaming_state_path(checkpoint_dir, filename)
    if not os.path.exists(path):
        return _default_state()
    try:
        with open(path, "r", encoding="utf-8") as f:
            state = json.load(f)
    except Exception:
        return _default_state()
    if not isinstance(state, dict):
        return _default_state()
    state.setdefault("version", 1)
    state.setdefault("by_kind", {})
    state.setdefault("totals", {"chunks_ingested": 0, "bytes_ingested": 0})
    if not isinstance(state["by_kind"], dict):
        state["by_kind"] = {}
    if not isinstance(state["totals"], dict):
        state["totals"] = {"chunks_ingested": 0, "bytes_ingested": 0}
    return state


def save_streaming_state(checkpoint_dir: str, state: dict,
                          filename: str = STREAMING_STATE_FILENAME) -> None:
    """Atomic write — tempfile + fsync + os.replace. Best-effort."""
    try:
        os.makedirs(checkpoint_dir, exist_ok=True)
    except Exception:
        pass
    path = streaming_state_path(checkpoint_dir, filename)
    parent = os.path.dirname(path) or "."
    fd, tmp = tempfile.mkstemp(prefix=".tmp_streaming_", dir=parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(state, f, indent=2)
            f.flush()
            try:
                os.fsync(f.fileno())
            except Exception:
                pass
        os.replace(tmp, path)
    except Exception:
        try:
            if os.path.exists(tmp):
                os.unlink(tmp)
        except Exception:
            pass


def _kind_state(state: dict, kind: str) -> dict:
    """Return (and lazily-create) the per-kind sub-dict."""
    by_kind = state.setdefault("by_kind", {})
    sub = by_kind.setdefault(kind, {})
    sub.setdefault("completed", [])
    sub.setdefault("in_progress", {})
    return sub


# ----------------------------------------------------------------------------
# Single-chunk pull helper
# ----------------------------------------------------------------------------


def iter_one_chunk(kind: str, source_id: str, *,
                   start_position: int = 0,
                   chunk_chars: int = DEFAULT_CHUNK_CHARS
                   ) -> Optional[tuple[int, str]]:
    """Pull one chunk from the source and discard the iterator.

    Returns (next_position, text), or None at end-of-source / on error.
    Used by the dispatcher to advance one chunk per call without holding
    the source open across drip windows.
    """
    src = get_source(kind)
    if src is None:
        return None
    try:
        gen = src.fetch_chunks(source_id,
                                start_position=start_position,
                                chunk_chars=chunk_chars)
        return next(gen, None)
    except StopIteration:
        return None
    except Exception:
        return None


# ----------------------------------------------------------------------------
# Auto-registration of bundled sources
# ----------------------------------------------------------------------------
# Backends register themselves on import. Importing scripts.sources triggers
# scripts.sources.__init__ which imports all available backends.
#
# Path repair: the bundled backends (wikipedia/investopedia/arxiv/gutenberg_stream/
# hf_github) write `from scripts.streaming_ingest import ...`, which only
# resolves when the *parent* of the scripts/ directory is on sys.path. The
# trainer is launched as `python3 scripts/immerse_athena.py` from the repo
# root — that puts scripts/ on sys.path but NOT the repo root, so the
# `scripts.streaming_ingest` import would silently fail and no backend would
# register. Add the repo root here so the backends find the canonical module.
import sys as _sys
_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if _REPO_ROOT not in _sys.path:
    _sys.path.insert(0, _REPO_ROOT)

# Also alias self under both top-level and package names so backends that
# do `from scripts.streaming_ingest import ...` reach THIS module — and
# THIS registry — instead of triggering a fresh re-import that creates a
# parallel _REGISTRY dict. Without this, `available_kinds()` returns []
# because backends register against the package-path twin, not the
# top-level module the trainer reads from.
_self_mod = _sys.modules.get(__name__)
if _self_mod is not None:
    if __name__ == "streaming_ingest":
        _sys.modules.setdefault("scripts.streaming_ingest", _self_mod)
    elif __name__ == "scripts.streaming_ingest":
        _sys.modules.setdefault("streaming_ingest", _self_mod)

try:
    from scripts import sources as _sources_pkg  # noqa: F401
except ImportError:
    # Direct invocation: scripts on sys.path, package import path differs.
    try:
        import sources as _sources_pkg  # type: ignore # noqa: F401
    except ImportError:
        # No bundled backends yet — that's fine for unit tests with mocks.
        pass
