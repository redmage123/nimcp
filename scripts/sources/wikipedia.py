"""Wikipedia streaming source for the Athena training pipeline.

Pulls article HTML from the Wikipedia REST API on demand and yields cleaned,
chunked plain text. NEVER persists article content to disk — only an
in-memory cache per source_id, dropped once the article is fully consumed.

API used:
    https://en.wikipedia.org/api/rest_v1/page/html/<title>

This endpoint returns the rendered article HTML in a single response, which
keeps the implementation simple (no continuation tokens) and avoids the
additional indirection of action=parse&prop=wikitext. It also gives us
cleaner output than wikitext (templates already expanded), so the regex
strip below is straightforward.

Curated source_ids per stage are tuned for Athena's developmental arc —
simple concrete nouns at Stage 1, elementary science at Stage 2, deeper
abstract topics at Stage 3.

Contract notes (cross-ref scripts/streaming_ingest.py):
  * Streaming only — no on-disk write of article text, ever.
  * Network failures raise — the dispatcher catches and skips.
  * try/finally on the generator so abandoned iterators don't leak.
  * Self-registers at module import via register_source().
"""

from __future__ import annotations

import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Iterator, Optional

# Allow direct-script imports (mirrors _test_stub.py shape).
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.streaming_ingest import StreamingSource, register_source


# ----------------------------------------------------------------------------
# Curated stage rosters
# ----------------------------------------------------------------------------

# Stage 1: simple, concrete, everyday — animals + objects a toddler knows.
_STAGE1_TITLES = [
    "Cat",
    "Dog",
    "Apple",
    "Water",
    "Sun",
    "Moon",
    "Tree",
    "Bird",
]

# Stage 2: elementary science — building blocks of how the world works.
_STAGE2_TITLES = [
    "Atom",
    "Cell_(biology)",
    "Gravity",
    "Light",
    "Sound",
    "Photosynthesis",
    "DNA",
    "Electricity",
    "Energy",
    "Force",
]

# Stage 3: deeper, abstract — the kinds of articles that demand reasoning.
_STAGE3_TITLES = [
    "Neuroscience",
    "Quantum_mechanics",
    "Evolution",
    "Ecology",
    "Algorithm",
    "Logic",
    "Philosophy",
    "History",
    "Mathematics",
    "Linguistics",
    "Consciousness",
    "Artificial_intelligence",
]


# ----------------------------------------------------------------------------
# HTML stripping
# ----------------------------------------------------------------------------
# Lightweight regex-based strip — no bs4 dependency. The Wikipedia REST HTML
# is well-formed enough that a few targeted passes give us readable plain
# text. We don't need a full DOM parse; we need text that's good enough for
# language exposure in the brain training loop.

_RE_SCRIPT_STYLE = re.compile(
    r"<(script|style)[^>]*>.*?</\1>", re.IGNORECASE | re.DOTALL
)
_RE_HTML_COMMENT = re.compile(r"<!--.*?-->", re.DOTALL)
# Wikipedia-specific noise: references list, edit links, navboxes, infoboxes,
# navigation elements that bloat the text without adding language value.
_RE_NOISE_SECTIONS = re.compile(
    r"<(table|figure|sup|nav|aside)[^>]*>.*?</\1>",
    re.IGNORECASE | re.DOTALL,
)
_RE_TAG = re.compile(r"<[^>]+>")
_RE_ENTITY = re.compile(r"&(#\d+|#x[0-9a-fA-F]+|[a-zA-Z]+);")
_RE_WHITESPACE = re.compile(r"[ \t]+")
_RE_BLANK_LINES = re.compile(r"\n{3,}")

# Wikitext fallback strip (in case a future revision swaps endpoints).
_RE_WIKITEXT_TEMPLATE = re.compile(r"\{\{[^{}]*\}\}", re.DOTALL)
_RE_WIKITEXT_LINK = re.compile(r"\[\[([^\]|]*\|)?([^\]]+)\]\]")
_RE_WIKITEXT_HEADING = re.compile(r"^=+\s*(.*?)\s*=+\s*$", re.MULTILINE)
_RE_WIKITEXT_BOLD_ITALIC = re.compile(r"'{2,5}")

_ENTITY_MAP = {
    "amp": "&",
    "lt": "<",
    "gt": ">",
    "quot": '"',
    "apos": "'",
    "nbsp": " ",
    "mdash": "—",
    "ndash": "–",
    "hellip": "…",
}


def _decode_entity(match: re.Match) -> str:
    raw = match.group(1)
    if raw.startswith("#x") or raw.startswith("#X"):
        try:
            return chr(int(raw[2:], 16))
        except (ValueError, OverflowError):
            return ""
    if raw.startswith("#"):
        try:
            return chr(int(raw[1:]))
        except (ValueError, OverflowError):
            return ""
    return _ENTITY_MAP.get(raw.lower(), "")


def _strip_html(html: str) -> str:
    """Convert Wikipedia REST HTML into plain text. Best-effort, no bs4."""
    text = _RE_SCRIPT_STYLE.sub(" ", html)
    text = _RE_HTML_COMMENT.sub(" ", text)
    # Strip noisy structural elements before generic tag removal.
    # Loop a couple of times so nested figures/tables collapse.
    for _ in range(3):
        new = _RE_NOISE_SECTIONS.sub(" ", text)
        if new == text:
            break
        text = new
    text = _RE_TAG.sub(" ", text)
    text = _RE_ENTITY.sub(_decode_entity, text)
    text = _RE_WHITESPACE.sub(" ", text)
    # Normalise newlines without losing paragraph breaks.
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = _RE_BLANK_LINES.sub("\n\n", text)
    return text.strip()


def _strip_wikitext(wt: str) -> str:
    """Fallback wikitext stripper. Not used by default but kept for symmetry
    with the documented alternative endpoint, in case _fetch_url switches."""
    text = wt
    # Templates can nest; iterate until stable or we give up.
    for _ in range(6):
        new = _RE_WIKITEXT_TEMPLATE.sub(" ", text)
        if new == text:
            break
        text = new
    text = _RE_WIKITEXT_LINK.sub(lambda m: m.group(2), text)
    text = _RE_WIKITEXT_HEADING.sub(r"\1", text)
    text = _RE_WIKITEXT_BOLD_ITALIC.sub("", text)
    text = _RE_WHITESPACE.sub(" ", text)
    text = _RE_BLANK_LINES.sub("\n\n", text)
    return text.strip()


# ----------------------------------------------------------------------------
# Network fetch
# ----------------------------------------------------------------------------

_REST_URL_TEMPLATE = "https://en.wikipedia.org/api/rest_v1/page/html/{title}"
# Wikipedia asks for a contact in the User-Agent. Keep it identifiable so an
# operator hitting an issue can find us in their logs.
_USER_AGENT = (
    "NIMCP-Athena/0.9 (https://github.com/bbrelin/nimcp; streaming corpus)"
)
_FETCH_TIMEOUT_S = 30.0


def _fetch_url(url: str) -> str:
    """Fetch a URL and return the decoded body. Raises on network failure."""
    req = urllib.request.Request(url, headers={"User-Agent": _USER_AGENT,
                                                "Accept": "text/html"})
    # urlopen raises urllib.error.URLError / HTTPError on failure — we let it
    # propagate per the contract. The dispatcher swallows.
    with urllib.request.urlopen(req, timeout=_FETCH_TIMEOUT_S) as resp:
        raw = resp.read()
    if isinstance(raw, bytes):
        # Wikipedia REST returns UTF-8.
        return raw.decode("utf-8", errors="replace")
    return raw


# ----------------------------------------------------------------------------
# Chunking
# ----------------------------------------------------------------------------
# We want chunks ~chunk_chars long, aligned to a paragraph or sentence
# boundary when one is reasonably close. Position is the byte offset (really
# character offset, since we work on str) into the cleaned text.

# Look for a boundary within +/- this fraction of chunk_chars.
_BOUNDARY_WINDOW_FRACTION = 0.25


def _find_boundary(text: str, target: int, chunk_chars: int) -> int:
    """Return an offset in [target - window, target + window] that ends on a
    paragraph or sentence boundary, or `target` itself if nothing close-by."""
    if target >= len(text):
        return len(text)
    window = max(1, int(chunk_chars * _BOUNDARY_WINDOW_FRACTION))
    lo = max(target - window, 1)
    hi = min(target + window, len(text))
    # Prefer paragraph break.
    seg = text[lo:hi]
    para = seg.rfind("\n\n")
    if para != -1:
        return lo + para + 2
    # Then sentence end (".", "!", "?") followed by space or newline.
    for end in (". ", "! ", "? ", ".\n", "!\n", "?\n"):
        idx = seg.rfind(end)
        if idx != -1:
            return lo + idx + len(end)
    # No nice boundary — cut at target.
    return target


# ----------------------------------------------------------------------------
# Source implementation
# ----------------------------------------------------------------------------


class WikipediaSource(StreamingSource):
    """English Wikipedia REST HTML, streamed in cleaned-text chunks."""

    kind = "wikipedia"

    def __init__(self) -> None:
        # In-memory cache: source_id -> cleaned text. Dropped on completion
        # or via _drop_cache(). NEVER persisted.
        self._cache: dict[str, str] = {}

    # -- StreamingSource API ------------------------------------------------

    def list_for_stage(self, stage: int) -> list[str]:
        if stage == 1:
            return list(_STAGE1_TITLES)
        if stage == 2:
            return list(_STAGE2_TITLES)
        if stage == 3:
            return list(_STAGE3_TITLES)
        return []

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = 1200
                     ) -> Iterator[tuple[int, str]]:
        # Resolve cleaned text — fetch + clean if we don't already have it.
        # _ensure_cached() raises on network/HTTP failure, per contract.
        cleaned = self._ensure_cached(source_id)
        try:
            yield from self._iter_chunks(source_id, cleaned,
                                          start_position=start_position,
                                          chunk_chars=chunk_chars)
        finally:
            # Generator cleanup: nothing to close (urlopen response was
            # consumed inside _fetch_url and the connection released by
            # context-manager exit before we ever yielded). Cache lives on
            # `self` and is intentionally retained across abandoned
            # iterations so the next call doesn't re-fetch the same article;
            # only is_complete() drops it.
            pass

    def is_complete(self, source_id: str, position: int) -> bool:
        cleaned = self._cache.get(source_id)
        if cleaned is None:
            # Not cached: we have no way to know without a fetch, but if the
            # caller has already advanced position past zero they almost
            # certainly hit the end on a previous call. Conservative: return
            # False so the next fetch_chunks() either yields more or
            # immediately exhausts.
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
        url = _REST_URL_TEMPLATE.format(
            title=urllib.parse.quote(source_id, safe="")
        )
        body = _fetch_url(url)
        cleaned = _strip_html(body)
        self._cache[source_id] = cleaned
        return cleaned

    def _iter_chunks(self, source_id: str, cleaned: str, *,
                     start_position: int,
                     chunk_chars: int) -> Iterator[tuple[int, str]]:
        if chunk_chars <= 0:
            chunk_chars = 1200
        pos = max(0, int(start_position))
        n = len(cleaned)
        while pos < n:
            target = pos + chunk_chars
            end = _find_boundary(cleaned, target, chunk_chars)
            if end <= pos:
                # Defensive: ensure forward progress even on pathological
                # text (e.g. a single 10MB line with no boundaries).
                end = min(pos + chunk_chars, n)
            chunk = cleaned[pos:end]
            pos = end
            yield (pos, chunk)

    def _drop_cache(self, source_id: str) -> None:
        self._cache.pop(source_id, None)


# Self-register at import time, regardless of env flag — the foundation
# expects unconditional registration for production backends.
register_source(WikipediaSource())
