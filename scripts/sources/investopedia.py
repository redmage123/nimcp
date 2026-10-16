"""Investopedia streaming source — financial-literacy curriculum for Athena.

Scrapes article HTML on demand from www.investopedia.com/terms/<letter>/<slug>.asp
and yields cleaned text in chunks. NEVER writes article content to disk —
only the resume position (an int) survives across calls, and that is owned
by the dispatcher, not this backend.

Usage contract (from streaming_ingest.StreamingSource):
  - kind: "investopedia"
  - list_for_stage(stage) -> stage-appropriate slugs (terms/<l>/<slug>.asp)
      stage 1: []                  (financial concepts skipped at infant stage)
      stage 2: 10 elementary terms (money, income, saving, debt, ...)
      stage 3: 15 deeper terms     (compound interest, GDP, derivatives, ...)
  - fetch_chunks(source_id, start_position, chunk_chars):
      streaming generator — first call fetches the article over HTTP,
      caches the cleaned plain text on `self`, and yields chunks of
      ~chunk_chars aligned on paragraph/sentence boundaries. Position
      is the byte offset into the cleaned text.
  - is_complete(source_id, position): True iff position >= len(cached text).

Politeness:
  - User-Agent: "NIMCP-Athena/1.0 streaming research"
  - 1.0s sleep between distinct article fetches per source instance
    (enforced before each network call after the first).
  - Robots.txt: Investopedia's robots permits /terms/* for general
    crawlers. We do not bypass it.

Hard rules:
  - No on-disk persistence of article text under any circumstance.
  - Network errors propagate (URLError, HTTPError, etc.) — caller
    decides whether to skip the source.
  - Iterators may be abandoned mid-stream; cleanup uses try/finally.
  - Stdlib only — no requests, no bs4.
"""

from __future__ import annotations

import os
import re
import sys
import time
from typing import Iterator, Optional
from urllib.request import Request, urlopen

# Ensure repo root is importable when executed standalone.
_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.streaming_ingest import (  # noqa: E402
    DEFAULT_CHUNK_CHARS,
    StreamingSource,
    register_source,
)


_BASE_URL = "https://www.investopedia.com/"
_USER_AGENT = "NIMCP-Athena/1.0 streaming research"
_RATE_LIMIT_SECONDS = 1.0
_HTTP_TIMEOUT = 30.0


# Curated slug lists per developmental stage.
_STAGE_2_SLUGS: list[str] = [
    "terms/m/money.asp",
    "terms/i/income.asp",
    "terms/s/saving.asp",
    "terms/d/debt.asp",
    "terms/i/interest.asp",
    "terms/b/budget.asp",
    "terms/c/credit.asp",
    "terms/t/tax.asp",
    "terms/i/investment.asp",
    "terms/p/profit.asp",
]

_STAGE_3_SLUGS: list[str] = [
    "terms/c/compoundinterest.asp",
    "terms/i/inflation.asp",
    "terms/g/gdp.asp",
    "terms/r/recession.asp",
    "terms/s/stock.asp",
    "terms/b/bond.asp",
    "terms/d/derivatives.asp",
    "terms/p/portfolio.asp",
    "terms/e/equity.asp",
    "terms/r/risk.asp",
    "terms/d/diversification.asp",
    "terms/h/hedge.asp",
    "terms/m/macroeconomics.asp",
    "terms/m/microeconomics.asp",
    "terms/c/capital.asp",
]


# ----------------------------------------------------------------------------
# HTML cleaning
# ----------------------------------------------------------------------------

# Drop these structural blocks entirely (script/style/nav/footer/aside/etc).
_BLOCK_DROP_RE = re.compile(
    r"<(script|style|noscript|nav|footer|aside|header|form|iframe|svg)"
    r"\b[^>]*>.*?</\1>",
    re.IGNORECASE | re.DOTALL,
)

# Try to isolate the article body if Investopedia marks one.
# The site uses a few class names over time; accept the common ones.
_ARTICLE_BODY_RE = re.compile(
    r'<(?:div|article|main)\b[^>]*\b(?:class|id)\s*=\s*'
    r'["\'][^"\']*'
    r'(?:article-body|article__body|comp\s+article-body|mntl-sc-page|main-content)'
    r'[^"\']*["\'][^>]*>(.*?)</(?:div|article|main)>',
    re.IGNORECASE | re.DOTALL,
)

_TAG_RE = re.compile(r"<[^>]+>")
_WS_RE = re.compile(r"[ \t\f\v]+")
_MULTI_NL_RE = re.compile(r"\n\s*\n+")

# A minimal HTML-entity unescape table; we avoid html.unescape's overhead
# but still handle the common ones. Fall back to html.unescape for the rest.
import html as _html  # noqa: E402


def _strip_html(raw_html: str) -> str:
    """Convert Investopedia article HTML to plain text.

    Strategy:
      1. Try to isolate the article body region.
      2. Drop script/style/nav/footer/etc blocks.
      3. Drop all remaining tags.
      4. Unescape entities, normalize whitespace, collapse blank lines.

    Lightweight regex pass; no external deps. Good enough for chunked
    text streaming into the brain — we are not building a search index.
    """
    if not raw_html:
        return ""

    # Step 1: try to grab the article body.
    match = _ARTICLE_BODY_RE.search(raw_html)
    body = match.group(1) if match else raw_html

    # Step 2: drop noise blocks.
    body = _BLOCK_DROP_RE.sub(" ", body)

    # Step 3: replace block-level tags with newlines so paragraph
    # boundaries survive the strip.
    body = re.sub(
        r"</(?:p|div|section|article|li|h[1-6]|br|tr)\s*>",
        "\n",
        body,
        flags=re.IGNORECASE,
    )
    body = re.sub(r"<br\s*/?>", "\n", body, flags=re.IGNORECASE)

    # Strip remaining tags.
    body = _TAG_RE.sub("", body)

    # Step 4: unescape entities and normalize whitespace.
    body = _html.unescape(body)
    body = _WS_RE.sub(" ", body)
    body = _MULTI_NL_RE.sub("\n\n", body)
    body = body.strip()
    return body


# ----------------------------------------------------------------------------
# Chunking
# ----------------------------------------------------------------------------

_PARA_BREAK_RE = re.compile(r"\n\n+")
_SENT_END_RE = re.compile(r"(?<=[.!?])\s+")


def _chunk_boundary(text: str, target: int) -> int:
    """Return a cut index <= len(text) close to `target`, preferring a
    paragraph break, then a sentence end, then a whitespace boundary.

    Always returns at least 1 (so we make progress) and at most len(text).
    """
    n = len(text)
    if n == 0:
        return 0
    if n <= target:
        return n

    # Look for a paragraph break in [target*0.6, target*1.2].
    lo = max(1, int(target * 0.6))
    hi = min(n, int(target * 1.2))
    window = text[lo:hi]

    # Prefer the LAST paragraph break in the window (closest to target).
    para_idx = window.rfind("\n\n")
    if para_idx != -1:
        return lo + para_idx + 2  # consume the break

    # Fall back: last sentence terminator in the window.
    for m in reversed(list(_SENT_END_RE.finditer(window))):
        return lo + m.end()

    # Last resort: nearest whitespace at or after target.
    ws_idx = text.find(" ", target)
    if ws_idx != -1 and ws_idx < n:
        return ws_idx + 1

    # No boundary at all — hard cut.
    return min(target, n)


# ----------------------------------------------------------------------------
# Source implementation
# ----------------------------------------------------------------------------


class InvestopediaSource(StreamingSource):
    """Streaming source for Investopedia term articles.

    Caches each fetched article's cleaned text in-memory (keyed by
    source_id). The cache entry is dropped when `is_complete` is called
    with a position past end-of-text. NEVER touches disk.
    """

    kind = "investopedia"

    def __init__(self) -> None:
        # source_id -> cleaned plain-text body
        self._cache: dict[str, str] = {}
        # Last fetch wall-clock for rate-limit pacing.
        self._last_fetch_at: float = 0.0

    # -- contract methods ---------------------------------------------------

    def list_for_stage(self, stage: int) -> list[str]:
        if stage <= 1:
            return []
        if stage == 2:
            return list(_STAGE_2_SLUGS)
        # Stage 3+ gets the deeper-concepts list. Higher stages reuse it
        # (the dispatcher round-robins; the consumer marks slugs done).
        return list(_STAGE_3_SLUGS)

    def fetch_chunks(self, source_id: str, *,
                     start_position: int = 0,
                     chunk_chars: int = DEFAULT_CHUNK_CHARS
                     ) -> Iterator[tuple[int, str]]:
        if chunk_chars <= 0:
            chunk_chars = DEFAULT_CHUNK_CHARS

        # Fetch + cache on first encounter. May raise; that's the contract.
        text = self._cache.get(source_id)
        if text is None:
            text = self._fetch_and_clean(source_id)
            self._cache[source_id] = text

        try:
            pos = max(0, int(start_position))
            n = len(text)
            while pos < n:
                end = _chunk_boundary(text[pos:], chunk_chars)
                if end <= 0:
                    break
                chunk = text[pos:pos + end]
                next_pos = pos + end
                # Skip empty chunks (purely whitespace boundary edge).
                if chunk.strip():
                    yield (next_pos, chunk)
                pos = next_pos
        finally:
            # No external resources held — but the contract requires
            # try/finally so abandoned iterators are explicitly OK. If we
            # ever start streaming the HTTP body chunked, close it here.
            pass

    def is_complete(self, source_id: str, position: int) -> bool:
        text = self._cache.get(source_id)
        if text is None:
            # Not yet fetched — can't know. Be conservative: not complete.
            return False
        done = position >= len(text)
        if done:
            # Drop the cached body to free RAM. Position was the only
            # thing that needed to survive, and the dispatcher owns it.
            self._cache.pop(source_id, None)
        return done

    # -- internals ----------------------------------------------------------

    def _fetch_and_clean(self, source_id: str) -> str:
        """Synchronous fetch + clean. Raises on network errors."""
        url = self._url_for(source_id)
        self._respect_rate_limit()
        req = Request(url, headers={"User-Agent": _USER_AGENT})
        # urlopen is a context manager; closes the response on exit.
        with urlopen(req, timeout=_HTTP_TIMEOUT) as resp:
            raw = resp.read()
        # Update rate-limit clock AFTER the network call completes.
        self._last_fetch_at = time.time()

        if isinstance(raw, bytes):
            # Investopedia serves UTF-8; fall back to latin-1 if needed.
            try:
                html_text = raw.decode("utf-8")
            except UnicodeDecodeError:
                html_text = raw.decode("latin-1", errors="replace")
        else:
            html_text = str(raw)

        return _strip_html(html_text)

    def _respect_rate_limit(self) -> None:
        """Sleep so successive fetches are at least _RATE_LIMIT_SECONDS apart."""
        if self._last_fetch_at <= 0.0:
            return
        elapsed = time.time() - self._last_fetch_at
        if elapsed < _RATE_LIMIT_SECONDS:
            time.sleep(_RATE_LIMIT_SECONDS)

    @staticmethod
    def _url_for(source_id: str) -> str:
        slug = source_id.lstrip("/")
        return _BASE_URL + slug


# Self-register at import time (per the foundation contract).
register_source(InvestopediaSource())
