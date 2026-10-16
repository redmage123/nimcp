#!/usr/bin/env python3
"""Inbox importer for the NIMCP canonical literary corpus.

The user drops copyrighted (or otherwise user-supplied) text files into
``data/canonical_corpus/_inbox/``, each accompanied by a sidecar
``<basename>.meta.json`` declaring author/title/year/language/register/stage.
This tool walks the inbox, validates the metadata, decodes + NFC-normalizes
the text via ``scripts.canonical_corpus.decode_with_fallback``, computes
sha256 + approximate token count, moves files into the canonical
``<author_slug>/<work_slug>/<file>.txt`` layout, and appends entries to
``data/canonical_corpus/index.json``.

Tolkien + Vance entries are explicitly in scope per the user's directive:
``public_domain: false`` is accepted without complaint.

Suggested target slugs (drop your files in the inbox with matching meta):

  Tolkien
    tolkien/the_hobbit
    tolkien/lord_of_the_rings/book_01_fellowship
    tolkien/lord_of_the_rings/book_02_two_towers
    tolkien/lord_of_the_rings/book_03_return
    tolkien/silmarillion
    tolkien/unfinished_tales
    tolkien/letters
    tolkien/on_fairy_stories
    tolkien/tom_bombadil

  Vance — Dying Earth
    vance/dying_earth/the_dying_earth
    vance/dying_earth/eyes_of_overworld
    vance/dying_earth/cugels_saga
    vance/dying_earth/rhialto_marvellous

  Vance — Lyonesse
    vance/lyonesse/suldruns_garden
    vance/lyonesse/green_pearl
    vance/lyonesse/madouc

  Vance — Demon Princes
    vance/demon_princes/star_king
    vance/demon_princes/killing_machine
    vance/demon_princes/palace_of_love
    vance/demon_princes/face
    vance/demon_princes/book_of_dreams

Inbox layout the user creates::

    data/canonical_corpus/_inbox/
        the_hobbit.txt
        the_hobbit.meta.json
        fellowship_chapter_01.txt
        fellowship_chapter_02.txt
        ...
        fellowship_of_the_ring.meta.json   # files_glob: "fellowship_*.txt"

Meta sidecar schema (required + optional)::

    {
      "author": "J.R.R. Tolkien",          # required
      "author_slug": "tolkien",            # required
      "title": "The Hobbit",               # required
      "work_slug": "the_hobbit",           # required
      "year": 1937,                        # required (int)
      "language": "en",                    # required
      "register": "epic_fantasy",          # required
      "stage": 3,                          # required (int)
      "weight": 1.5,                       # optional, default 1.0
      "public_domain": false,              # optional, default false
      "source": "user_supplied",           # optional, default "user_supplied"
      "original_language": "en",           # optional
      "translator": null,                  # optional
      "comment": "",                       # optional
      "files_glob": "fellowship_*.txt"     # optional, multi-file batching
    }

CLI
---

    tools/import_inbox.py [--corpus-root data/canonical_corpus]
                          [--inbox _inbox]
                          [--dry-run]
                          [--keep-inbox]

Exit code: 0 on full success, 1 if any entry failed.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import sys
import tempfile
import unicodedata
from typing import Optional

# Make ``scripts.canonical_corpus`` importable when invoked from anywhere.
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_HERE, ".."))
if _REPO not in sys.path:
    sys.path.insert(0, _REPO)

from scripts.canonical_corpus import decode_with_fallback  # noqa: E402


REQUIRED_FIELDS: tuple = (
    ("author", str),
    ("author_slug", str),
    ("title", str),
    ("work_slug", str),
    ("year", int),
    ("language", str),
    ("register", str),
    ("stage", int),
)

OPTIONAL_DEFAULTS: dict = {
    "weight": 1.0,
    "public_domain": False,
    "source": "user_supplied",
    "original_language": None,
    "translator": None,
    "comment": "",
}


class MetaError(ValueError):
    """Raised when a meta sidecar fails validation."""


def validate_meta(meta: dict) -> dict:
    """Validate a parsed meta dict; return a normalized copy with defaults applied.

    Raises MetaError on the first violation. ``bool`` is rejected for ``int``
    fields because Python ``True``/``False`` would otherwise sneak through
    ``isinstance(x, int)`` checks.
    """
    if not isinstance(meta, dict):
        raise MetaError(f"meta must be a JSON object, got {type(meta).__name__}")

    out: dict = {}
    for name, typ in REQUIRED_FIELDS:
        if name not in meta:
            raise MetaError(f"missing required field: {name!r}")
        value = meta[name]
        if typ is int and isinstance(value, bool):
            raise MetaError(f"field {name!r} must be int, got bool")
        if not isinstance(value, typ):
            raise MetaError(
                f"field {name!r} must be {typ.__name__}, "
                f"got {type(value).__name__}"
            )
        if typ is str and not value.strip():
            raise MetaError(f"field {name!r} must be a non-empty string")
        out[name] = value

    # Slug sanity: lowercase letters/digits/underscore only (matches existing index.json).
    for slug_field in ("author_slug", "work_slug"):
        slug = out[slug_field]
        if not all(c.isalnum() or c == "_" for c in slug):
            raise MetaError(
                f"field {slug_field!r}={slug!r} must contain only "
                f"alphanumerics and underscores"
            )

    # Optionals with defaults.
    for name, default in OPTIONAL_DEFAULTS.items():
        out[name] = meta.get(name, default)

    if not isinstance(out["weight"], (int, float)) or isinstance(out["weight"], bool):
        raise MetaError("field 'weight' must be a number")
    out["weight"] = float(out["weight"])

    if not isinstance(out["public_domain"], bool):
        raise MetaError("field 'public_domain' must be a boolean")

    for name in ("source", "comment"):
        if not isinstance(out[name], str):
            raise MetaError(f"field {name!r} must be a string")

    for name in ("original_language", "translator"):
        if out[name] is not None and not isinstance(out[name], str):
            raise MetaError(f"field {name!r} must be a string or null")

    if "files_glob" in meta:
        if not isinstance(meta["files_glob"], str) or not meta["files_glob"].strip():
            raise MetaError("field 'files_glob' must be a non-empty string")
        out["files_glob"] = meta["files_glob"]

    return out


def _approx_token_count(text: str) -> int:
    """Whitespace-split word count. Cheap, deterministic, good enough for a manifest."""
    return len(text.split())


def _atomic_write_json(path: str, data: dict) -> None:
    """Write JSON atomically: tempfile in same dir + fsync + os.replace."""
    parent = os.path.dirname(os.path.abspath(path)) or "."
    fd, tmp = tempfile.mkstemp(prefix=".idx.", suffix=".tmp", dir=parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except Exception:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def _resolve_inputs(meta_path: str, meta: dict, inbox_dir: str) -> list:
    """Return a list of absolute paths to the .txt files associated with this meta.

    Raises FileNotFoundError if any expected file is missing.
    """
    if "files_glob" in meta:
        pattern = meta["files_glob"]
        matches = sorted(
            os.path.join(inbox_dir, name)
            for name in os.listdir(inbox_dir)
            if fnmatch.fnmatch(name, pattern) and name.endswith(".txt")
        )
        if not matches:
            raise FileNotFoundError(
                f"files_glob={pattern!r} matched no .txt files in inbox"
            )
        return matches

    stem = os.path.basename(meta_path)
    if stem.endswith(".meta.json"):
        stem = stem[: -len(".meta.json")]
    else:
        stem = os.path.splitext(stem)[0]
    txt = os.path.join(inbox_dir, stem + ".txt")
    if not os.path.isfile(txt):
        raise FileNotFoundError(f"expected sibling text file not found: {txt}")
    return [txt]


def _process_one(
    meta_path: str,
    inbox_dir: str,
    corpus_root: str,
    dry_run: bool,
) -> dict:
    """Validate one meta sidecar, materialize canonical files, return an index entry.

    Returns a dict with keys::

        {
          "entry": <full index entry>,
          "src_files": [<absolute paths in inbox>],
          "dst_files": [<absolute paths under corpus_root>],
        }

    Raises MetaError, FileNotFoundError, OSError on any failure.
    """
    with open(meta_path, "r", encoding="utf-8") as f:
        try:
            raw = json.load(f)
        except json.JSONDecodeError as exc:
            raise MetaError(f"meta is not valid JSON: {exc}") from exc

    meta = validate_meta(raw)
    inputs = _resolve_inputs(meta_path, meta, inbox_dir)

    rel_files: list = []
    abs_dst_files: list = []
    sha = hashlib.sha256()
    total_size = 0
    total_tokens = 0

    work_dir_rel = os.path.join(meta["author_slug"], meta["work_slug"])
    work_dir_abs = os.path.join(corpus_root, work_dir_rel)

    if not dry_run:
        os.makedirs(work_dir_abs, exist_ok=True)

    for src in inputs:
        text = decode_with_fallback(src)
        # decode_with_fallback already NFC-normalizes; belt-and-braces here
        # in case someone changes that helper later.
        text = unicodedata.normalize("NFC", text)
        encoded = text.encode("utf-8")
        sha.update(encoded)
        total_size += len(encoded)
        total_tokens += _approx_token_count(text)

        dst_name = os.path.basename(src)
        rel = os.path.join(work_dir_rel, dst_name)
        rel_files.append(rel.replace(os.sep, "/"))
        dst_abs = os.path.join(corpus_root, rel)
        abs_dst_files.append(dst_abs)

        if not dry_run:
            with open(dst_abs, "w", encoding="utf-8") as out:
                out.write(text)

    entry = {
        "id": f"{meta['author_slug']}.{meta['work_slug']}",
        "author": meta["author"],
        "author_slug": meta["author_slug"],
        "title": meta["title"],
        "work_slug": meta["work_slug"],
        "files": rel_files,
        "language": meta["language"],
        "register": meta["register"],
        "year": meta["year"],
        "public_domain": meta["public_domain"],
        "source": meta["source"],
        "stage": meta["stage"],
        "weight": meta["weight"],
        "size_bytes": total_size,
        "sha256": sha.hexdigest(),
        "approx_tokens": total_tokens,
    }

    if meta["original_language"] is not None:
        entry["original_language"] = meta["original_language"]
    if meta["translator"] is not None:
        entry["translator"] = meta["translator"]
    if meta["comment"]:
        entry["comment"] = meta["comment"]

    return {
        "entry": entry,
        "src_files": [meta_path] + inputs,
        "dst_files": abs_dst_files,
    }


def _upsert_entry(index: dict, new_entry: dict) -> str:
    """Insert or replace an entry in the index. Returns 'added' or 'replaced'."""
    works = index.setdefault("works", [])
    new_id = new_entry["id"]
    for i, w in enumerate(works):
        if w.get("id") == new_id:
            works[i] = new_entry
            return "replaced"
    works.append(new_entry)
    return "added"


def import_inbox(
    corpus_root: str,
    inbox_subdir: str = "_inbox",
    dry_run: bool = False,
    keep_inbox: bool = False,
    log=print,
) -> dict:
    """Run the inbox import. Returns a summary dict with counts and details.

    Programmatic entry point for tests. The CLI just wraps this.
    """
    inbox_dir = os.path.join(corpus_root, inbox_subdir)
    index_path = os.path.join(corpus_root, "index.json")

    if not os.path.isdir(inbox_dir):
        log(f"[error] inbox directory does not exist: {inbox_dir}")
        return {"added": 0, "replaced": 0, "errored": 1, "errors": ["no inbox dir"]}

    # Load existing index (or initialize).
    if os.path.isfile(index_path):
        with open(index_path, "r", encoding="utf-8") as f:
            index = json.load(f)
    else:
        index = {"version": 1, "works": []}

    if index.get("version") != 1:
        log(f"[error] unsupported index.json version: {index.get('version')!r}")
        return {"added": 0, "replaced": 0, "errored": 1,
                "errors": [f"bad index version: {index.get('version')!r}"]}

    # Find every .meta.json in the inbox (skip helper files).
    skip = {".gitkeep", ".gitignore"}
    meta_files = sorted(
        os.path.join(inbox_dir, name)
        for name in os.listdir(inbox_dir)
        if name.endswith(".meta.json") and name not in skip
    )

    added = 0
    replaced = 0
    errors: list = []
    cleanup_files: list = []

    for meta_path in meta_files:
        try:
            result = _process_one(meta_path, inbox_dir, corpus_root, dry_run)
        except (MetaError, FileNotFoundError, OSError) as exc:
            errors.append(f"{os.path.basename(meta_path)}: {exc}")
            log(f"[error] {os.path.basename(meta_path)}: {exc}")
            continue

        verb = _upsert_entry(index, result["entry"])
        if verb == "added":
            added += 1
        else:
            replaced += 1
        log(f"[{verb}] {result['entry']['id']} "
            f"({len(result['entry']['files'])} file(s), "
            f"{result['entry']['size_bytes']} bytes, "
            f"~{result['entry']['approx_tokens']} tokens)")

        if not dry_run:
            cleanup_files.extend(result["src_files"])

    if dry_run:
        log("[dry-run] not writing index.json, not removing inbox files")
    else:
        if added or replaced:
            _atomic_write_json(index_path, index)
            log(f"[index] wrote {index_path}")
        if not keep_inbox:
            for path in cleanup_files:
                try:
                    os.unlink(path)
                except OSError as exc:
                    log(f"[warn] failed to remove {path}: {exc}")

    log(f"[summary] added={added} replaced={replaced} errored={len(errors)}")

    return {
        "added": added,
        "replaced": replaced,
        "errored": len(errors),
        "errors": errors,
    }


def main(argv: Optional[list] = None) -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument(
        "--corpus-root",
        default=os.path.join(_REPO, "data", "canonical_corpus"),
        help="Root of the canonical corpus (default: data/canonical_corpus).",
    )
    p.add_argument(
        "--inbox",
        default="_inbox",
        help="Inbox subdirectory under --corpus-root (default: _inbox).",
    )
    p.add_argument("--dry-run", action="store_true",
                   help="Validate + plan only; don't move files or modify index.json.")
    p.add_argument("--keep-inbox", action="store_true",
                   help="On success, leave the inbox files in place instead of "
                        "deleting them.")
    args = p.parse_args(argv)

    summary = import_inbox(
        corpus_root=args.corpus_root,
        inbox_subdir=args.inbox,
        dry_run=args.dry_run,
        keep_inbox=args.keep_inbox,
    )
    return 0 if summary["errored"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
