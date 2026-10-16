"""arXiv streaming source — abstracts only.

Fetches paper abstracts from the arXiv API on demand. We deliberately use
ONLY the abstract (`<summary>` element of the Atom feed): full PDFs are
multi-MB binaries, our deployment cannot persist content to disk, and the
abstract carries the dense scientific-prose signal Athena needs.

Per the streaming-corpus contract:
  - Network failures raise (caller decides retry/skip).
  - No paper content is persisted to disk. The in-memory cache holds at
    most one fully-parsed abstract per source_id and is dropped when the
    source is marked complete via is_complete().
  - The arXiv API rate limit (1 request / 3 seconds against
    export.arxiv.org) is enforced by sleeping AT LEAST 3.0s between
    network calls, identified by a polite User-Agent header.

Curated source_id lists per stage are tuned for Athena's developmental
arc (Stage 1 = no papers, Stage 2 = popular landmarks, Stage 3 = deeper
cross-disciplinary work).
"""

from __future__ import annotations

import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from typing import Iterator, Optional

# Make the streaming_ingest module importable both as scripts.streaming_ingest
# (normal package import) and as a top-level streaming_ingest (when scripts/
# is on sys.path directly, e.g. some test runners).
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.streaming_ingest import (  # noqa: E402
    DEFAULT_CHUNK_CHARS,
    StreamingSource,
    register_source,
)


ARXIV_API_URL = "http://export.arxiv.org/api/query?id_list={paper_id}"
ARXIV_USER_AGENT = (
    "NIMCP-Athena/1.0 streaming research; mailto:claude@ai-elevate.ai"
)
ARXIV_RATE_LIMIT_SECONDS = 3.0
ARXIV_REQUEST_TIMEOUT = 30.0

# Atom XML namespace used by the arXiv API response.
_ATOM_NS = "{http://www.w3.org/2005/Atom}"


# Stage-curated paper IDs. Stage 1 returns [] — early-stage Athena isn't
# ready for academic abstracts. Stage 2 = popular-science accessible
# landmarks. Stage 3 = deeper cross-disciplinary papers.
_STAGE_2_PAPERS = [
    "1706.03762",   # Attention Is All You Need
    "1207.7235",    # ATLAS Higgs discovery
    "1607.04606",   # FastText
    "0905.3669",    # Susskind black-hole information
    "0903.5485",    # Mathematical analysis review
    "2010.11929",   # Vision Transformer
    "1810.04805",   # BERT
]

_STAGE_3_PAPERS = [
    "1503.03585",   # Diffusion models foundation
    "1406.2661",    # GAN
    "1412.6980",    # Adam optimizer
    "1810.12281",   # Quantum ML survey
    "2103.00020",   # CLIP
    "1606.05908",   # One-shot learning
    "1502.05477",   # TRPO
    "1707.06347",   # PPO
    "1611.05397",   # UNREAL
    "math/0211159", # Perelman Ricci flow
    "0710.0858",    # Neural networks complexity
    "1505.04597",   # U-Net
    "1611.07004",   # pix2pix
    "1801.04062",   # MINE info estimation
    "1909.13371",   # Brain-region ML review
]


# Sentence-boundary regex — split on ., !, ? followed by whitespace. We use
# this only to nudge chunk boundaries onto sentence ends; if no boundary is
# found within the chunk window we hard-cut.
_SENTENCE_END_RE = re.compile(r"(?<=[.!?])\s+")


def _clean_abstract_text(raw: str) -> str:
    """Collapse arXiv abstract whitespace.

    arXiv abstracts come with hard-wrapped lines and sometimes leading/
    trailing newlines around the <summary> body. Collapse runs of
    whitespace to single spaces, strip ends, but preserve the prose
    content verbatim.
    """
    if raw is None:
        return ""
    return re.sub(r"\s+", " ", raw).strip()


def _split_into_chunks(text: str, chunk_chars: int) -> list[tuple[int, str]]:
    """Split text into (next_position, chunk) pairs aligned on sentence
    boundaries when feasible. Positions are byte offsets into `text`."""
    if not text:
        return []
    if chunk_chars <= 0:
        # Defensive — caller violated the contract. Treat as one chunk.
        return [(len(text), text)]

    chunks: list[tuple[int, str]] = []
    pos = 0
    n = len(text)
    while pos < n:
        end = pos + chunk_chars
        if end >= n:
            # Final chunk — take everything that's left.
            chunks.append((n, text[pos:n]))
            break
        # Look for a sentence boundary at-or-before `end`. We scan within
        # the window [pos, end] using the sentence-end regex; pick the
        # rightmost match so we get the largest chunk that ends on a
        # boundary.
        window = text[pos:end]
        boundary = None
        for m in _SENTENCE_END_RE.finditer(window):
            boundary = m.end()
        if boundary is not None and boundary > 0:
            cut = pos + boundary
        else:
            # No sentence break in the window — hard cut.
            cut = end
        chunks.append((cut, text[pos:cut]))
        pos = cut
    return chunks


class ArxivSource(StreamingSource):
    """Streaming source backed by the public arXiv Atom-feed API."""

    kind = "arxiv"

    def __init__(self) -> None:
        # Cache of parsed abstracts keyed by paper_id. We hold at most one
        # entry per source_id and drop it on is_complete(). Per the
        # contract this is in-memory only — never written to disk.
        self._abstract_cache: dict[str, str] = {}
        # Last time we hit the network (monotonic seconds). Used to honor
        # the arXiv rate limit across calls within a single process.
        self._last_request_time: float = 0.0

    # ------------------------------------------------------------------
    # StreamingSource API
    # ------------------------------------------------------------------

    def list_for_stage(self, stage: int) -> list[str]:
        if stage <= 1:
            return []
        if stage == 2:
            return list(_STAGE_2_PAPERS)
        # Stage 3 and any later stage gets the deeper set. We do not union
        # stages here — the dispatcher decides whether to revisit Stage 2
        # papers based on the resume state.
        return list(_STAGE_3_PAPERS)

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = DEFAULT_CHUNK_CHARS
                     ) -> Iterator[tuple[int, str]]:
        # Defer the actual generator body so the rate-limit sleep + fetch
        # happen lazily on the first __next__ call. This matches the
        # iter_one_chunk caller pattern (build generator, pull one,
        # discard) and means we never do network work just to check that
        # the source exists.
        return self._chunks_generator(source_id,
                                      start_position=start_position,
                                      chunk_chars=chunk_chars)

    def is_complete(self, source_id: str, position: int) -> bool:
        abstract = self._abstract_cache.get(source_id)
        if abstract is None:
            # We have no cached length to compare against. If the caller
            # is asking with position == 0 we obviously aren't done; for
            # any non-zero position we can't tell without re-fetching, so
            # report False and let the dispatcher poll once more — that
            # call will populate the cache and the next is_complete() can
            # answer accurately.
            return False
        done = position >= len(abstract)
        if done:
            # Drop the cache entry — the source is finished and we don't
            # want to hold the abstract in memory longer than needed.
            self._abstract_cache.pop(source_id, None)
        return done

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def _chunks_generator(self, source_id: str, *,
                          start_position: int,
                          chunk_chars: int) -> Iterator[tuple[int, str]]:
        try:
            abstract = self._get_abstract(source_id)
            if not abstract:
                # Empty abstract — nothing to yield. The dispatcher will
                # see is_complete() return True on the next pass.
                return
            # Resume by slicing — chunks are computed on the (possibly
            # offset) tail of the abstract, then their reported positions
            # are rebased onto the original abstract.
            if start_position < 0:
                start_position = 0
            if start_position >= len(abstract):
                return
            tail = abstract[start_position:]
            for next_pos_in_tail, chunk_text in _split_into_chunks(
                    tail, chunk_chars):
                absolute_pos = start_position + next_pos_in_tail
                yield (absolute_pos, chunk_text)
        finally:
            # Caller may abandon the iterator mid-stream. Nothing to
            # close — urlopen() resources are released inside _fetch_xml
            # before we ever yield — but this try/finally is the contract
            # the base class documents and keeps future cleanup local.
            pass

    def _get_abstract(self, paper_id: str) -> str:
        cached = self._abstract_cache.get(paper_id)
        if cached is not None:
            return cached
        xml_text = self._fetch_xml(paper_id)
        abstract = self._parse_abstract(xml_text)
        # Cache (in memory only — never persisted) so subsequent chunk
        # pulls for the same paper don't re-hit the network.
        self._abstract_cache[paper_id] = abstract
        return abstract

    def _fetch_xml(self, paper_id: str) -> str:
        """Issue one rate-limited GET against the arXiv API.

        Network failures (URLError, HTTPError, socket timeouts, malformed
        responses) propagate to the caller per the streaming-corpus
        contract — the dispatcher decides whether to skip and retry.
        """
        self._respect_rate_limit()
        url = ARXIV_API_URL.format(paper_id=urllib.parse.quote(paper_id,
                                                               safe="/"))
        request = urllib.request.Request(
            url,
            headers={"User-Agent": ARXIV_USER_AGENT},
        )
        # Mark the request time BEFORE the network round-trip — the rate
        # limit is "1 request per 3s" so we want subsequent requests to
        # be spaced from when the request fired, not from when it
        # returned.
        self._last_request_time = time.monotonic()
        with urllib.request.urlopen(
                request, timeout=ARXIV_REQUEST_TIMEOUT) as response:
            payload = response.read()
        if isinstance(payload, bytes):
            return payload.decode("utf-8", errors="replace")
        return str(payload)

    def _respect_rate_limit(self) -> None:
        """Sleep, if necessary, to keep at least ARXIV_RATE_LIMIT_SECONDS
        between successive requests."""
        elapsed = time.monotonic() - self._last_request_time
        wait = ARXIV_RATE_LIMIT_SECONDS - elapsed
        # On the very first call _last_request_time is 0.0, so elapsed
        # is large and `wait` is negative — we skip the sleep entirely.
        # Subsequent calls within the rate window get a positive wait.
        if wait > 0:
            time.sleep(wait)
        elif self._last_request_time == 0.0:
            # First call of this process: still pay the polite delay so
            # we never burst the API even on cold start.
            time.sleep(ARXIV_RATE_LIMIT_SECONDS)

    @staticmethod
    def _parse_abstract(xml_text: str) -> str:
        """Extract <summary> text from an arXiv Atom feed.

        Returns "" if no summary element is found (malformed paper id,
        empty result set, etc.). We intentionally do NOT raise on an
        empty summary — that's a content condition, not a network
        failure.
        """
        if not xml_text:
            return ""
        try:
            root = ET.fromstring(xml_text)
        except ET.ParseError:
            # Treat malformed XML as a content-level miss; the caller
            # will see an empty stream and is_complete() will trip.
            return ""
        # Walk all <entry><summary> elements and take the first non-empty
        # one. arXiv typically returns a single entry per paper id.
        for entry in root.iter(_ATOM_NS + "entry"):
            summary_el = entry.find(_ATOM_NS + "summary")
            if summary_el is not None and summary_el.text:
                return _clean_abstract_text(summary_el.text)
        # Fallback: some responses surface <summary> directly under root.
        summary_el = root.find(_ATOM_NS + "summary")
        if summary_el is not None and summary_el.text:
            return _clean_abstract_text(summary_el.text)
        return ""


# Self-register at import time so `from scripts.sources import arxiv`
# (or the package-level import in scripts/sources/__init__.py) makes the
# kind discoverable via streaming_ingest.get_source("arxiv").
register_source(ArxivSource())
