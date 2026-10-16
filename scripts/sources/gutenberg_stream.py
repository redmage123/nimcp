"""Project Gutenberg streaming source for the Athena training pipeline.

CE-18: complements the on-disk literary canon at data/canonical_corpus/ with
a streaming slot for STEM, economics, philosophy, and reference works that
we want exposure to but don't need to keep on disk.

Pulls plain-text books from Project Gutenberg on demand, strips the well-
known "*** START OF" / "*** END OF" header and footer sentinels, and yields
chunked text aligned to paragraph boundaries where possible. NEVER persists
book content to disk — only an in-memory cache per source_id, dropped once
the book is fully consumed.

API used (in order, with fallback):
    https://www.gutenberg.org/files/<id>/<id>-0.txt
    https://www.gutenberg.org/cache/epub/<id>/pg<id>.txt

The first form is preferred because it's the canonical "official" URL the
Gutenberg homepage links to; the cache/epub form is the auto-generated
plain-text rendering and is the better-supported fallback when the files/
form 404s (which happens for older/translated editions).

Curated source_id list per stage focuses on STEM/economics/philosophy that
broadens Athena's exposure beyond the literary on-disk canon. IDs that are
already present on disk (canonical_corpus or math_corpus) are intentionally
omitted — see the comments on each list.

Contract notes (cross-ref scripts/streaming_ingest.py):
  * Streaming only — no on-disk write of book text, ever.
  * Network failures raise — the dispatcher catches and skips.
  * try/finally on the generator so abandoned iterators don't leak.
  * Self-registers at module import via register_source().
  * Rate-limited to >=1.0s between fetches to respect Gutenberg etiquette.
  * Identifying User-Agent so an operator can find us in their logs.
"""

from __future__ import annotations

import os
import re
import sys
import time
import urllib.error
import urllib.request
from typing import Iterator, Optional

# Allow direct-script imports (mirrors the other source backends).
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.streaming_ingest import StreamingSource, register_source


# ----------------------------------------------------------------------------
# Curated stage rosters
# ----------------------------------------------------------------------------
# IDs are stringified Gutenberg numeric book IDs.
#
# DEDUP NOTES (2026-05-05):
#   - Stage 3 "11" (Alice in Wonderland) — already on disk as
#     carroll.alice_in_wonderland (gutenberg:11). Dropped.
#   - Stage 3 "31010" (Through the Looking-Glass) — the on-disk
#     carroll.through_the_looking_glass uses gutenberg:12, which is the same
#     work in a different transcription. Dropped to avoid restreaming it.
#   - Stage 3 "21076" (Euclid's Elements) — already in math_corpus index
#     (geometry/euclid_elements.txt). Task explicitly says skip if
#     duplicating. Dropped.

# Stage 1: book-length texts are not appropriate for Stage 1's developmental
# window (parentese, simple immediate vocabulary). Empty by design.
_STAGE1_IDS: list[str] = []

# Stage 2: 6 STEM/economics/political-science accessible classics.
_STAGE2_IDS = [
    "33283",   # Ten British Mathematicians of the 19th Century, Macfarlane
    "1232",    # The Prince, Machiavelli — political science
    "33310",   # The Mathematical Analysis of Logic, Boole — primer chapters
    "11595",   # The Foundations of Geometry, Hilbert
    "61304",   # The Wealth of Nations, Smith — Book I
    "53924",   # On the Origin of Species, Darwin — preface + Ch 1
]

# Stage 3: 9 deeper philosophy/math/literature works.
_STAGE3_IDS = [
    # "11"        — Alice in Wonderland (on disk: canonical_corpus). DROPPED.
    # "31010"     — Looking-Glass (on disk via gutenberg:12).        DROPPED.
    "55",      # The Wonderful Wizard of Oz, Baum
    "1228",    # The Republic, Plato
    "8438",    # Critique of Pure Reason, Kant
    "844",     # The Importance of Being Earnest, Wilde
    # "21076"    — Euclid's Elements (on disk: math_corpus).         DROPPED.
    "8800",    # The Divine Comedy, Dante — Inferno
    "5827",    # The Problems of Philosophy, Russell
    "16659",   # A Treatise on Probability, Keynes
    "31360",   # Principia Mathematica, Vol I preface (Russell-Whitehead)
    "62",      # Tarzan of the Apes — accessible adventure
]


# ----------------------------------------------------------------------------
# Header/footer stripping
# ----------------------------------------------------------------------------
# Project Gutenberg wraps every plain-text book with sentinel lines like:
#   *** START OF THE PROJECT GUTENBERG EBOOK <TITLE> ***
#   ...book body...
#   *** END OF THE PROJECT GUTENBERG EBOOK <TITLE> ***
# Older transcriptions occasionally use "*** START OF THIS PROJECT GUTENBERG"
# or omit the trailing "***" — match permissively. We anchor on the leading
# triple-asterisk to avoid picking up incidental "START OF" prose in the body.

_RE_GUT_START = re.compile(
    r"^\*\*\*\s*START OF.*?\*\*\*\s*$",
    re.IGNORECASE | re.MULTILINE,
)
_RE_GUT_END = re.compile(
    r"^\*\*\*\s*END OF.*?\*\*\*\s*$",
    re.IGNORECASE | re.MULTILINE,
)
# Newline normalization + collapsed blank-line runs.
_RE_BLANK_LINES = re.compile(r"\n{3,}")


def _strip_gutenberg_markers(text: str) -> str:
    """Trim the Project Gutenberg header (everything up to and including the
    START line) and the footer (everything from the END line onward). If a
    sentinel is missing (rare), leave that side untouched."""
    body = text.replace("\r\n", "\n").replace("\r", "\n")

    start_match = _RE_GUT_START.search(body)
    if start_match is not None:
        body = body[start_match.end():]

    end_match = _RE_GUT_END.search(body)
    if end_match is not None:
        body = body[:end_match.start()]

    body = _RE_BLANK_LINES.sub("\n\n", body)
    return body.strip()


# ----------------------------------------------------------------------------
# Network fetch
# ----------------------------------------------------------------------------

# Two URL templates: try files/ first, fall back to cache/epub/ on 404 etc.
_URL_PRIMARY = "https://www.gutenberg.org/files/{id}/{id}-0.txt"
_URL_FALLBACK = "https://www.gutenberg.org/cache/epub/{id}/pg{id}.txt"

# Per the task contract — identifies us in Gutenberg's access logs.
_USER_AGENT = "NIMCP-Athena/1.0 streaming research"
_FETCH_TIMEOUT_S = 30.0
# >=1.0s between fetches. Gutenberg's terms ask robots to be polite.
_RATE_LIMIT_SECONDS = 1.0


def _fetch_url(url: str) -> bytes:
    """Fetch a URL and return the raw body bytes. Raises on network failure."""
    req = urllib.request.Request(
        url,
        headers={"User-Agent": _USER_AGENT, "Accept": "text/plain"},
    )
    # urlopen raises urllib.error.URLError / HTTPError on failure — let it
    # propagate per the streaming contract. The dispatcher swallows.
    with urllib.request.urlopen(req, timeout=_FETCH_TIMEOUT_S) as resp:
        return resp.read()


def _decode_body(raw: bytes) -> str:
    """Decode a fetched book body. Project Gutenberg plain-text files are
    usually UTF-8 (the "-0.txt" suffix is the convention for that), but older
    transcriptions are latin-1. Try both before giving up with replacement."""
    if not isinstance(raw, (bytes, bytearray)):
        return str(raw)
    for enc in ("utf-8", "latin-1"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", errors="replace")


# ----------------------------------------------------------------------------
# Chunking
# ----------------------------------------------------------------------------
# Yield chunks of approximately chunk_chars, paragraph-aligned where
# reasonable. Position is the character offset into the cleaned text.

# Look for a paragraph boundary within +/- this fraction of chunk_chars.
_BOUNDARY_WINDOW_FRACTION = 0.25


def _find_chunk_end(text: str, start: int, chunk_chars: int) -> int:
    """Return an end-offset for a chunk that begins at `start`. Prefers a
    paragraph break ("\\n\\n") inside [target - window, target + window];
    falls back to a sentence break; finally falls back to the raw target."""
    n = len(text)
    target = start + chunk_chars
    if target >= n:
        return n
    window = max(1, int(chunk_chars * _BOUNDARY_WINDOW_FRACTION))
    lo = max(start + 1, target - window)
    hi = min(n, target + window)

    # Prefer paragraph break inside the window. rfind looks for the LATEST
    # break, which keeps chunks near target rather than truncating early.
    seg = text[lo:hi]
    para = seg.rfind("\n\n")
    if para != -1:
        return lo + para + 2

    # Sentence boundary fallback.
    for end_seq in (". ", "! ", "? ", ".\n", "!\n", "?\n"):
        idx = seg.rfind(end_seq)
        if idx != -1:
            return lo + idx + len(end_seq)

    # No nice boundary nearby — cut at target.
    return target


# ----------------------------------------------------------------------------
# Source implementation
# ----------------------------------------------------------------------------


class GutenbergStreamSource(StreamingSource):
    """Streaming Project Gutenberg backend. STEM/econ/philosophy companion
    to the on-disk literary canonical_corpus."""

    kind = "gutenberg_stream"

    def __init__(self) -> None:
        # In-memory cache: source_id -> cleaned book text. Dropped on
        # completion via _drop_cache(). NEVER persisted to disk.
        self._cache: dict[str, str] = {}
        # Last fetch timestamp for rate limiting.
        self._last_fetch_at: float = 0.0

    # -- StreamingSource API ------------------------------------------------

    def list_for_stage(self, stage: int) -> list[str]:
        if stage == 1:
            return list(_STAGE1_IDS)
        if stage == 2:
            return list(_STAGE2_IDS)
        if stage == 3:
            return list(_STAGE3_IDS)
        return []

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = 1200
                     ) -> Iterator[tuple[int, str]]:
        if chunk_chars <= 0:
            chunk_chars = 1200

        # Resolve cleaned text — fetch + strip if not already cached.
        # _ensure_cached() raises on network/HTTP failure, per contract.
        cleaned = self._ensure_cached(source_id)
        try:
            pos = max(0, int(start_position))
            n = len(cleaned)
            while pos < n:
                end = _find_chunk_end(cleaned, pos, chunk_chars)
                if end <= pos:
                    # Defensive: ensure forward progress on pathological
                    # text (single huge line with no boundaries).
                    end = min(pos + chunk_chars, n)
                chunk = cleaned[pos:end]
                pos = end
                # Skip a chunk that is entirely whitespace (rare boundary
                # edge near the very end of the book).
                if chunk.strip():
                    yield (pos, chunk)
        finally:
            # Generator cleanup: nothing to close (the urlopen response was
            # consumed and released inside _fetch_url before we started
            # yielding). The cache lives on `self` and is intentionally
            # retained across abandoned iterations so the next call doesn't
            # re-fetch the same book; only is_complete() drops it.
            pass

    def is_complete(self, source_id: str, position: int) -> bool:
        cleaned = self._cache.get(source_id)
        if cleaned is None:
            # Not cached — can't know without a fetch. Be conservative.
            return False
        if position >= len(cleaned):
            self._drop_cache(source_id)
            return True
        return False

    # -- Internals (test-visible, prefixed with _) --------------------------

    def _ensure_cached(self, source_id: str) -> str:
        cached = self._cache.get(source_id)
        if cached is not None:
            return cached

        primary = _URL_PRIMARY.format(id=source_id)
        fallback = _URL_FALLBACK.format(id=source_id)

        # Try the primary URL; on HTTPError fall back to the cache/epub one.
        # Other URLErrors (DNS, connection refused) propagate immediately —
        # the fallback wouldn't help and we want the dispatcher to skip.
        self._respect_rate_limit()
        try:
            raw = _fetch_url(primary)
        except urllib.error.HTTPError:
            self._last_fetch_at = time.time()
            self._respect_rate_limit()
            raw = _fetch_url(fallback)
            self._last_fetch_at = time.time()
        else:
            self._last_fetch_at = time.time()

        body = _decode_body(raw)
        cleaned = _strip_gutenberg_markers(body)
        self._cache[source_id] = cleaned
        return cleaned

    def _respect_rate_limit(self) -> None:
        """Sleep so successive fetches are at least _RATE_LIMIT_SECONDS apart.

        Per the streaming contract: 1.0s minimum between fetches to respect
        Project Gutenberg's etiquette for automated clients."""
        if self._last_fetch_at <= 0.0:
            return
        elapsed = time.time() - self._last_fetch_at
        if elapsed < _RATE_LIMIT_SECONDS:
            time.sleep(_RATE_LIMIT_SECONDS)

    def _drop_cache(self, source_id: str) -> None:
        self._cache.pop(source_id, None)


# Self-register at import time, regardless of env flag — the foundation
# expects unconditional registration for production backends.
register_source(GutenbergStreamSource())
