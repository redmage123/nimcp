r"""Source-code corpus loader for Athena's symbolic-program register.

Companion to scripts/canonical_corpus.py. The literary loader is
paragraph-aligned, which is the wrong granularity for source code:
splitting Python on `\n\s*\n` would cut classes in half and orphan
their docstrings; splitting C similarly would shred the leading
comment block off a function definition.

This module provides a code-aware chunker that splits on
function/class boundaries and keeps the leading comment block attached
to the chunk that documents.

Public API:
    iter_code_chunks(file_path, *, max_chars=2000)
        -> Iterator[(chunk_id, code_text)]
    iter_repo_chunks(repo_relative_path, *, repo_root=None, max_chars=2000)
        -> Iterator[(chunk_id, code_text)]
    resolve_work_files(work, *, corpus_root, repo_root=None) -> list[str]
    load_code_index(root) -> dict   # delegates to canonical_corpus.load_index

The loader is pure read-side: no daemon calls, no mutation of corpus
files. The caller is responsible for driving chunks into the brain.

Schema notes:
    Each work entry has EITHER ``files`` (corpus-relative paths under
    ``data/code_corpus/``) OR ``repo_files`` (repo-relative paths
    resolved against the NIMCP source tree). The latter lets us point
    at curated slices of NIMCP's own implementation as a meta-grounding
    register without copying source files into the corpus.
"""

from __future__ import annotations

import os
import re
import sys
from typing import Iterator, List, Tuple


# --- Splitter regexes ------------------------------------------------------
#
# Python: a function or class begins on a line that starts (after optional
# indentation) with `def `, `async def `, or `class `. We accept any
# indentation level so nested defs/classes ALSO act as chunk boundaries —
# this is good for code-aware ingestion because the model sees one
# self-contained logical unit per chunk.
_PY_BOUNDARY = re.compile(r"^[ \t]*(?:async[ \t]+def|def|class)[ \t]+")

# C: a top-level function definition is a line at column 0 (no leading
# whitespace) that contains a parenthesized parameter list followed by an
# opening brace, optionally on the next line. We capture the common case
# where `{` is on the same line as the signature — this is the project's
# house style and matches every C sample we ship.
_C_BOUNDARY = re.compile(r"^[A-Za-z_][\w\s\*]*\([^)]*\)\s*\{")

# We treat "blank line" as a whitespace-only line, used as a fallback when
# a single chunk exceeds max_chars.
_BLANK_LINE = re.compile(r"^\s*$")

# Minimum size for a chunk before we coalesce it into its neighbor.
# Tiny fragments (e.g. a single one-line `def foo(): pass`) are not very
# informative on their own.
_MIN_CHUNK_CHARS = 50


def _detect_language(file_path: str) -> str:
    """Return 'python', 'c', or 'unknown' from the file extension."""
    ext = os.path.splitext(file_path)[1].lower()
    if ext == ".py":
        return "python"
    if ext in (".c", ".h"):
        return "c"
    return "unknown"


def _is_boundary(line: str, language: str) -> bool:
    """Does `line` start a new function/class definition?"""
    if language == "python":
        return bool(_PY_BOUNDARY.match(line))
    if language == "c":
        return bool(_C_BOUNDARY.match(line))
    return False


def _is_comment_or_blank(line: str, language: str) -> bool:
    """Is `line` part of a leading-comment block (so it should attach
    to the next function chunk, not the previous one)?"""
    stripped = line.strip()
    if stripped == "":
        return True
    if language == "python":
        # `#` comments. Triple-quoted docstrings are inside the function
        # body itself (they appear AFTER the def line) so they never
        # show up here; we don't need to handle them specially.
        return stripped.startswith("#")
    if language == "c":
        # Single-line `//` and any line that is part of a `/* ... */`
        # block. We approximate the block-comment case by checking for
        # leading "*", "/*", or "*/" — typical when comments are formatted
        # in the project's house style.
        return (stripped.startswith("//")
                or stripped.startswith("/*")
                or stripped.startswith("*"))
    return False


def _split_oversize(chunk: str, max_chars: int) -> List[str]:
    """Fallback split for chunks larger than max_chars: cut on blank lines.

    We greedily pack lines into pieces of <= max_chars. If a single line
    is itself longer than max_chars (rare in real code), we hard-split it.
    """
    if len(chunk) <= max_chars:
        return [chunk]

    pieces: List[str] = []
    buf: List[str] = []
    buf_len = 0

    for line in chunk.splitlines(keepends=True):
        if buf_len + len(line) > max_chars and buf:
            # Prefer to break at a blank line: if `buf` ends in a blank
            # line, this is a clean cut. Otherwise we still cut here —
            # there isn't a better choice within the budget.
            pieces.append("".join(buf))
            buf = [line]
            buf_len = len(line)
        else:
            buf.append(line)
            buf_len += len(line)

    if buf:
        pieces.append("".join(buf))

    # Hard-split any single piece that is still oversize — happens only
    # for pathological one-liners.
    final: List[str] = []
    for p in pieces:
        if len(p) <= max_chars:
            final.append(p)
        else:
            for i in range(0, len(p), max_chars):
                final.append(p[i:i + max_chars])
    return final


def _coalesce_tiny(chunks: List[str]) -> List[str]:
    """Merge any chunk shorter than _MIN_CHUNK_CHARS into a neighbor."""
    if not chunks:
        return chunks

    out: List[str] = []
    for chunk in chunks:
        if out and len(chunk) < _MIN_CHUNK_CHARS:
            # Attach to the previous chunk.
            out[-1] = out[-1] + chunk
        elif not out and len(chunk) < _MIN_CHUNK_CHARS and len(chunks) > 1:
            # First chunk is tiny — defer it, we'll merge it with the
            # next one we see.
            out.append(chunk)
        else:
            out.append(chunk)

    # Second pass for the "first chunk was tiny" case: if out[0] is
    # still under the threshold and there's a next chunk, fold them.
    if len(out) >= 2 and len(out[0]) < _MIN_CHUNK_CHARS:
        out[0:2] = [out[0] + out[1]]
    return out


def iter_code_chunks(file_path: str,
                     *,
                     max_chars: int = 2000) -> Iterator[Tuple[int, str]]:
    """Yield (chunk_id, code_text) pairs for one source file.

    Splits on function/class boundaries; the leading comment block above
    each definition stays attached to that definition's chunk. Chunks
    larger than `max_chars` are split on blank lines as fallback. Chunks
    smaller than 50 chars are coalesced with a neighbor.

    Language is auto-detected from the file extension (.py -> Python,
    .c/.h -> C). Unknown extensions fall back to a single-chunk yield.
    """
    language = _detect_language(file_path)

    with open(file_path, "r", encoding="utf-8") as f:
        text = f.read()

    if not text.strip():
        return

    if language == "unknown":
        # No splitting rule available — emit the whole file as one chunk.
        yield 0, text
        return

    lines = text.splitlines(keepends=True)
    n = len(lines)

    # --- Step 1: find every "anchor" line index (a function/class start)
    # plus the start of any leading comment block that immediately
    # precedes it. The chunk for that anchor includes both.
    anchors: List[int] = []   # the line index where each chunk starts
    i = 0
    while i < n:
        if _is_boundary(lines[i], language):
            # Walk backwards to absorb a leading comment / blank-line block.
            start = i
            j = i - 1
            while j >= 0 and _is_comment_or_blank(lines[j], language):
                j -= 1
            start = j + 1
            # Don't claim lines that already belong to a prior chunk.
            if anchors and start <= anchors[-1]:
                start = i
            # Don't claim lines that are part of the file's leading
            # block (which becomes its own chunk).
            anchors.append(start)
        i += 1

    # --- Step 2: slice into chunks. The "preamble" (anything before the
    # first anchor) is itself a chunk if it has content.
    raw_chunks: List[str] = []

    if not anchors:
        # No def/class found — single-chunk file.
        raw_chunks.append("".join(lines))
    else:
        if anchors[0] > 0:
            preamble = "".join(lines[:anchors[0]])
            if preamble.strip():
                raw_chunks.append(preamble)

        for k, start in enumerate(anchors):
            end = anchors[k + 1] if k + 1 < len(anchors) else n
            chunk = "".join(lines[start:end])
            if chunk.strip():
                raw_chunks.append(chunk)

    # --- Step 3: split any oversized chunks on blank lines.
    sized: List[str] = []
    for c in raw_chunks:
        sized.extend(_split_oversize(c, max_chars))

    # --- Step 4: coalesce tiny chunks into neighbors.
    sized = _coalesce_tiny(sized)

    # --- Step 5: yield with monotonic chunk ids starting at 0.
    for chunk_id, chunk in enumerate(sized):
        yield chunk_id, chunk


def load_code_index(root: str) -> dict:
    """Load the code corpus index.json.

    Thin wrapper around canonical_corpus.load_index — same shape, same
    version contract. We import canonical_corpus lazily and add the
    scripts/ directory to sys.path if needed so callers don't have to
    care which working directory they run from.
    """
    # Add scripts/ to sys.path on the fly. The streaming_ingest module
    # uses a similar trick.
    here = os.path.dirname(os.path.abspath(__file__))
    if here not in sys.path:
        sys.path.insert(0, here)

    from canonical_corpus import load_index  # noqa: E402
    return load_index(root)


# --- Repo-relative resolution ---------------------------------------------
#
# The "nimcp_self" register points at curated slices of NIMCP's own source
# tree (e.g. ``src/api/nimcp_part_lifecycle.c``, ``include/nimcp.h``).
# These files are referenced via ``repo_files`` rather than ``files`` so we
# don't duplicate the source on disk.

# Markers we expect to find at the NIMCP repo root. We require BOTH so that
# we don't accidentally identify some random ancestor (e.g. a parent dir
# that happens to have a ``scripts/`` subdir) as the repo root.
_REPO_ROOT_MARKERS = ("CMakeLists.txt", "scripts")


def _find_repo_root(start: str) -> str:
    """Walk up from ``start`` looking for a directory that contains all of
    the markers in :data:`_REPO_ROOT_MARKERS`.

    Returns the absolute path to the first matching ancestor. Falls back
    to the directory three levels above this module (``scripts/code_corpus.py``
    -> ``scripts/`` -> repo root) if no marker match is found — that's the
    layout the project ships with today.
    """
    here = os.path.abspath(start)
    if os.path.isfile(here):
        here = os.path.dirname(here)

    cur = here
    while True:
        if all(os.path.exists(os.path.join(cur, m))
               for m in _REPO_ROOT_MARKERS):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            # Reached filesystem root without finding markers — fall back
            # to a heuristic based on this module's known location.
            module_dir = os.path.dirname(os.path.abspath(__file__))
            return os.path.dirname(module_dir)
        cur = parent


def resolve_work_files(work: dict,
                       *,
                       corpus_root: str,
                       repo_root: str = None) -> List[str]:
    """Return absolute paths for one work entry.

    The work dict may carry ``files`` (corpus-relative, joined against
    ``corpus_root``) OR ``repo_files`` (repo-relative, joined against
    ``repo_root``). If ``repo_root`` is ``None`` we auto-detect by
    walking up from ``corpus_root``.

    If neither key is present, returns an empty list — the caller is
    expected to skip such entries rather than error out, so an empty
    list is a soft signal.
    """
    if "files" in work and work["files"]:
        return [os.path.join(corpus_root, rel) for rel in work["files"]]

    if "repo_files" in work and work["repo_files"]:
        if repo_root is None:
            repo_root = _find_repo_root(corpus_root)
        return [os.path.join(repo_root, rel) for rel in work["repo_files"]]

    return []


def iter_repo_chunks(repo_relative_path: str,
                     *,
                     repo_root: str = None,
                     max_chars: int = 2000) -> Iterator[Tuple[int, str]]:
    """Yield (chunk_id, code_text) pairs for a repo-relative source path.

    Thin wrapper around :func:`iter_code_chunks` that resolves the path
    against the NIMCP repo root. ``repo_root`` is auto-detected if not
    supplied (see :func:`_find_repo_root`).

    Used for the ``nimcp_self`` register, which lets Athena read curated
    slices of her own implementation — meta-grounding, not data leakage,
    because she's the artificial person whose source this is.
    """
    if repo_root is None:
        repo_root = _find_repo_root(os.path.abspath(os.curdir))
    abs_path = os.path.join(repo_root, repo_relative_path)
    yield from iter_code_chunks(abs_path, max_chars=max_chars)
