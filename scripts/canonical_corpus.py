"""Canonical literary corpus loader for Athena's developmental curriculum.

Reads `data/canonical_corpus/index.json`, exposes per-stage iteration over
paragraph-aligned text chunks, and supports byte-offset resume so a partial
ingest can pick up exactly where it left off across daemon restarts.

Public API:
    load_index(corpus_root)     -> dict
    works_for_stage(index, n)   -> list[work_entry]
    iter_chunks(work, root,     -> Iterator[(end_byte, text)]
                start_byte, max_chars)
    decode_with_fallback(path)  -> str

The loader is pure read-side: no daemon calls, no mutation of corpus files.
The caller (immerse_athena._ingest_canonical_corpus) is responsible for
driving chunks into the brain and persisting resume state.
"""

import json
import os
import re
import unicodedata
from typing import Iterator, Tuple


_PARA_BREAK = re.compile(r"\n\s*\n")
_SENTENCE_BOUNDARY = re.compile(r"(?<=[.!?])\s+")


def load_index(corpus_root: str) -> dict:
    path = os.path.join(corpus_root, "index.json")
    with open(path, "r", encoding="utf-8") as f:
        idx = json.load(f)
    if idx.get("version") != 1:
        raise ValueError(f"unsupported index.json version: {idx.get('version')!r}")
    return idx


def works_for_stage(index: dict, stage: int) -> list:
    return [w for w in index.get("works", []) if int(w.get("stage", -1)) == stage]


def decode_with_fallback(path: str) -> str:
    """Project Gutenberg files mix UTF-8 and ISO-8859-1; try both, normalize NFC."""
    with open(path, "rb") as f:
        raw = f.read()
    for enc in ("utf-8", "latin-1"):
        try:
            text = raw.decode(enc)
            break
        except UnicodeDecodeError:
            continue
    else:
        text = raw.decode("utf-8", errors="replace")
    text = unicodedata.normalize("NFC", text)
    text = (text
            .replace("‘", "'").replace("’", "'")
            .replace("“", '"').replace("”", '"')
            .replace("—", "--").replace("–", "-"))
    return text


def _split_chunk(text: str, max_chars: int) -> Tuple[str, str]:
    """Split text at the rightmost paragraph or sentence boundary <= max_chars.
    Returns (chunk, remainder). If no boundary found, hard-split at max_chars."""
    if len(text) <= max_chars:
        return text, ""
    window = text[:max_chars]
    para = list(_PARA_BREAK.finditer(window))
    if para:
        cut = para[-1].end()
        return text[:cut], text[cut:]
    sent = list(_SENTENCE_BOUNDARY.finditer(window))
    if sent:
        cut = sent[-1].end()
        return text[:cut], text[cut:]
    return text[:max_chars], text[max_chars:]


def iter_chunks(work: dict, corpus_root: str,
                start_byte: int = 0,
                max_chars: int = 1200) -> Iterator[Tuple[int, str]]:
    """Yield (end_byte_offset, chunk_text) pairs for one work.

    `end_byte_offset` is the cumulative count of UTF-8-encoded bytes consumed
    after this chunk. Persist that value as the resume cursor; on the next
    run, pass it as `start_byte` to resume exactly past this chunk.

    Multi-file works are walked in declaration order; offsets are global
    across the file list so a single cursor covers the whole work.
    """
    files = work.get("files", [])
    cursor_global = 0
    pending = ""

    for rel in files:
        path = os.path.join(corpus_root, rel)
        if not os.path.isfile(path):
            continue
        body = decode_with_fallback(path)
        body_bytes = body.encode("utf-8")
        file_size = len(body_bytes)

        # Skip the whole file if start_byte is past its end.
        if start_byte >= cursor_global + file_size:
            cursor_global += file_size
            continue

        # If the resume cursor falls inside this file, slice from there.
        if start_byte > cursor_global:
            skip = start_byte - cursor_global
            body = body_bytes[skip:].decode("utf-8", errors="replace")
            cursor_global = start_byte
        text = pending + body
        pending = ""

        while text.strip():
            chunk, remainder = _split_chunk(text, max_chars)
            if not chunk.strip():
                # All-whitespace chunk: advance cursor and discard.
                cursor_global += len(chunk.encode("utf-8"))
                text = remainder
                continue
            cursor_global += len(chunk.encode("utf-8"))
            yield cursor_global, chunk
            text = remainder
        pending = text  # carry trailing fragment into next file (rare)


def round_robin_schedule(works: list, total_chunks: int,
                          chunk_chars: int = 1200) -> list:
    """Plan a weighted round-robin pull schedule over a stage's works.

    Returns a list of work_ids in the order chunks should be requested,
    sized to `total_chunks`. Heavier weights get proportionally more
    pulls *across* the schedule, but interleaved so no work monopolizes
    a contiguous run of chunks.
    """
    if not works:
        return []
    counters = {w["id"]: 0.0 for w in works}
    weights = {w["id"]: max(0.01, float(w.get("weight", 1.0))) for w in works}

    order = []
    while len(order) < total_chunks:
        for w in works:
            counters[w["id"]] += weights[w["id"]]
        # Pick the work whose accumulator has the highest deficit vs what
        # it has already been served. Largest accumulator wins; ties broken
        # by index in `works` for determinism.
        served = {wid: order.count(wid) for wid in counters}
        best_wid = max(counters, key=lambda wid: counters[wid] - served[wid])
        order.append(best_wid)
        counters[best_wid] -= 1.0  # consumed one chunk's worth
    return order
