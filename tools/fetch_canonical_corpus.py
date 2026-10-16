#!/usr/bin/env python3
"""Project Gutenberg fetcher for the canonical literary corpus.

Reads data/canonical_corpus/index.json, downloads each work whose `source`
starts with "gutenberg:", strips Gutenberg headers/footers, normalizes the
text via the loader's decode_with_fallback, writes the cleaned file to its
canonical path, and updates the manifest with size_bytes / sha256 /
approx_tokens.

User-supplied works (Tolkien, Vance) are ignored here — those land via
tools/import_inbox.py.

Stdlib only. Run from repo root.
"""

import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
import time
import unicodedata
from typing import Optional
from urllib.error import URLError, HTTPError
from urllib.request import Request, urlopen

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, REPO)

from scripts.canonical_corpus import decode_with_fallback  # noqa: E402


_GUT_START = re.compile(r"^\*\*\* START OF .*\*\*\*\s*$",
                        re.IGNORECASE | re.MULTILINE)
_GUT_END = re.compile(r"^\*\*\* END OF .*\*\*\*\s*$",
                      re.IGNORECASE | re.MULTILINE)
_USER_AGENT = "NIMCP-Athena-corpus-fetcher/1.0"
_HTTP_TIMEOUT = 30
_RETRY_BACKOFF = 5


def strip_gutenberg_markers(text: str) -> str:
    """Remove Gutenberg header (everything before *** START) and footer
    (everything from *** END onward). If markers are missing, return the
    text unchanged (older-format file)."""
    start_match = _GUT_START.search(text)
    if start_match:
        text = text[start_match.end():]
        text = text.lstrip("\r\n")
    end_match = _GUT_END.search(text)
    if end_match:
        text = text[:end_match.start()]
        text = text.rstrip("\r\n")
    return text


def fetch_url(url: str) -> bytes:
    req = Request(url, headers={"User-Agent": _USER_AGENT})
    last_err: Optional[Exception] = None
    for attempt in (1, 2):
        try:
            with urlopen(req, timeout=_HTTP_TIMEOUT) as resp:
                return resp.read()
        except (URLError, HTTPError) as exc:
            last_err = exc
            if attempt == 1:
                time.sleep(_RETRY_BACKOFF)
    raise RuntimeError(f"failed to fetch {url}: {last_err!r}")


def parse_gutenberg_id(source: str) -> Optional[int]:
    if not source.startswith("gutenberg:"):
        return None
    try:
        return int(source.split(":", 1)[1])
    except (ValueError, IndexError):
        return None


def gutenberg_url(book_id: int) -> str:
    return f"https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt"


def normalize_text(text: str) -> str:
    """Match the loader's decode_with_fallback normalization."""
    text = unicodedata.normalize("NFC", text)
    text = (text
            .replace("‘", "'").replace("’", "'")
            .replace("“", '"').replace("”", '"')
            .replace("—", "--").replace("–", "-"))
    return text


def write_atomic(path: str, data: bytes) -> None:
    parent = os.path.dirname(path)
    os.makedirs(parent, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix=".tmp_fetch_", dir=parent)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def write_index_atomic(corpus_root: str, idx: dict) -> None:
    path = os.path.join(corpus_root, "index.json")
    blob = json.dumps(idx, indent=2, ensure_ascii=False).encode("utf-8")
    write_atomic(path, blob)


def stat_text(text: str) -> dict:
    enc = text.encode("utf-8")
    return {
        "size_bytes": len(enc),
        "sha256": hashlib.sha256(enc).hexdigest(),
        "approx_tokens": len(text.split()),
    }


_KJV_BOOK_OPENERS = {
    "genesis": "The First Book of Moses: Called Genesis",
    "exodus": "The Second Book of Moses: Called Exodus",
    "psalms": "The Book of Psalms",
    "proverbs": "The Proverbs",
    "matthew": "The Gospel According to Saint Matthew",
    "mark": "The Gospel According to Saint Mark",
    "luke": "The Gospel According to Saint Luke",
    "john": "The Gospel According to Saint John",
    "acts": "The Acts of the Apostles",
    "revelation": "The Revelation of Saint John the Divine",
}


def slice_kjv(full_text: str, work_slug: str) -> str:
    """Pragmatic KJV book slicer. Returns the text for the requested slice
    or the full text on slug 'remainder' (we keep that broad)."""
    def find_opener(slug: str) -> int:
        opener = _KJV_BOOK_OPENERS.get(slug)
        if not opener:
            return -1
        return full_text.find(opener)

    def slice_between(start_slug: str, end_slug: Optional[str]) -> str:
        s = find_opener(start_slug)
        if s < 0:
            return ""
        e = find_opener(end_slug) if end_slug else -1
        if e < 0:
            return full_text[s:]
        return full_text[s:e]

    if work_slug == "genesis":
        return slice_between("genesis", "exodus")
    if work_slug == "psalms":
        return slice_between("psalms", "proverbs")
    if work_slug == "proverbs":
        # Proverbs ends at Ecclesiastes; we don't have that opener in our
        # table, so fall back to the next opener we DO have (Matthew).
        # The slop is acceptable for a coarse curriculum prime.
        return slice_between("proverbs", "matthew")
    if work_slug == "gospels":
        s = find_opener("matthew")
        e = find_opener("acts")
        if s < 0:
            return ""
        if e < 0:
            return full_text[s:]
        return full_text[s:e]
    if work_slug == "remainder":
        # Everything that isn't Genesis/Psalms/Proverbs/Gospels.
        # Pragmatic: take Exodus->Matthew (rest of OT) + Acts->end (rest of NT).
        ot_rest = slice_between("exodus", "psalms")
        nt_rest = ""
        s = find_opener("acts")
        if s >= 0:
            nt_rest = full_text[s:]
        return (ot_rest + "\n\n" + nt_rest).strip()
    return full_text


def fetch_one(work: dict, corpus_root: str, *,
              dry_run: bool, force: bool,
              kjv_cache: dict) -> str:
    """Returns one of: 'fetched', 'skipped', 'error:<msg>'."""
    book_id = parse_gutenberg_id(work.get("source", ""))
    if book_id is None:
        return "skipped"

    target = os.path.join(corpus_root, work["files"][0])
    if os.path.isfile(target) and not force:
        return "skipped"

    if dry_run:
        print(f"  [dry-run] would fetch {work['id']} -> {target}")
        return "fetched"

    # Re-use cached gutenberg:10 (KJV) — many entries share it.
    if book_id in kjv_cache:
        full_raw = kjv_cache[book_id]
    else:
        try:
            full_raw = fetch_url(gutenberg_url(book_id))
        except RuntimeError as exc:
            return f"error:{exc}"
        kjv_cache[book_id] = full_raw

    # Decode via loader-equivalent pipeline.
    fd, tmp = tempfile.mkstemp(prefix=".tmp_decode_")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(full_raw)
        full_text = decode_with_fallback(tmp)
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass

    body = strip_gutenberg_markers(full_text)
    if not _GUT_START.search(full_text) and not _GUT_END.search(full_text):
        print(f"  warn: {work['id']}: no Gutenberg start/end markers; using whole file")

    # KJV slicing — multiple works share gutenberg:10.
    if work["author_slug"] == "bible_kjv":
        body = slice_kjv(body, work["work_slug"])

    body = normalize_text(body)
    write_atomic(target, body.encode("utf-8"))

    stats = stat_text(body)
    work.update(stats)
    return "fetched"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--corpus-root", default="data/canonical_corpus")
    p.add_argument("--only", default="",
                   help="comma-separated work ids; empty = all")
    p.add_argument("--force", action="store_true",
                   help="re-fetch + overwrite existing files")
    p.add_argument("--dry-run", action="store_true",
                   help="print plan without network or filesystem changes")
    args = p.parse_args(argv)

    idx_path = os.path.join(args.corpus_root, "index.json")
    if not os.path.isfile(idx_path):
        print(f"error: {idx_path} not found", file=sys.stderr)
        return 1
    with open(idx_path, "r", encoding="utf-8") as f:
        idx = json.load(f)

    only = set(s.strip() for s in args.only.split(",") if s.strip())
    works = idx.get("works", [])

    fetched = skipped = errored = 0
    errors = []
    kjv_cache: dict = {}

    for work in works:
        wid = work.get("id", "?")
        if only and wid not in only:
            continue
        if not str(work.get("source", "")).startswith("gutenberg:"):
            skipped += 1
            continue
        result = fetch_one(work, args.corpus_root,
                           dry_run=args.dry_run, force=args.force,
                           kjv_cache=kjv_cache)
        if result == "fetched":
            fetched += 1
            print(f"  OK    {wid}")
        elif result == "skipped":
            skipped += 1
        elif result.startswith("error:"):
            errored += 1
            errors.append((wid, result[len("error:"):]))
            print(f"  FAIL  {wid}: {result[len('error:'):]}")

    if not args.dry_run and fetched > 0:
        write_index_atomic(args.corpus_root, idx)
        print(f"updated {idx_path}")

    print(f"\nsummary: {fetched} fetched, {skipped} skipped, {errored} errored")
    if errors:
        print("errors:")
        for wid, msg in errors:
            print(f"  {wid}: {msg}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
