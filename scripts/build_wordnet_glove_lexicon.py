#!/usr/bin/env python3
"""
build_wordnet_glove_lexicon.py — Bulk lexicon builder for grounded_language.

Walks WordNet for word forms + POS, joins against GloVe 6B-100d embeddings
(padded to 128-d to match GL_SEMANTIC_DIM), and emits a packed binary file
that the C-side `nimcp_brain_load_bulk_lexicon()` can stream in directly.

Why binary, not JSON: at 50K+ entries, JSON parse + float-by-float
conversion costs 30+ seconds and ~10× the disk size. The fixed-layout
binary loads in under a second and keeps disk under ~30 MB at 50K words.

----- BINARY FILE FORMAT v1 -------------------------------------------------

  Header (32 bytes, little-endian, packed):
    uint32  magic            'BLEX' = 0x58454c42
    uint32  version          = 1
    uint32  word_count       number of records that follow
    uint32  vector_dim       embedding dim (must equal GL_SEMANTIC_DIM=128)
    uint32  record_max_form  max bytes a form_len field can hold (= 64)
    uint32  reserved[3]      zero — for future flags

  Records (variable length, no padding):
    uint16  form_len         strlen(form), <= 63 (fits NUL in 64)
    uint8[] form             ASCII bytes, NOT NUL-terminated
    uint8   class_enum       gl_word_class_t value (NOUN/VERB/ADJ/ADV/etc.)
    uint8   reserved         0
    float32 vec[vector_dim]  L2-normalized embedding, padded with zeros if
                             the source dim is smaller

The C loader sanity-checks magic, version, vector_dim == GL_SEMANTIC_DIM,
and form_len <= GL_MAX_WORD_LEN-1 per record. Anything else aborts the load
with a logged warning.

----- USAGE ----------------------------------------------------------------

  python3 scripts/build_wordnet_glove_lexicon.py \\
      [--out data/lexicon/wordnet_glove_v1.bin] \\
      [--cap 60000] \\
      [--glove data/lexicon/cache/glove.6B.100d.txt]

  If GloVe isn't present, the script downloads it from
  https://downloads.cs.stanford.edu/nlp/data/glove.6B.zip into
  data/lexicon/cache/ and extracts only the 100d variant. If the
  download fails, the script falls back to per-word stub vectors
  (deterministic hash-based pseudo-embeddings) so the lexicon is
  still loadable end-to-end.
"""
from __future__ import annotations

import argparse
import io
import os
import struct
import sys
import zipfile
from pathlib import Path

# ---------------------------------------------------------------- constants

MAGIC = 0x58454C42  # 'BLEX' little-endian
VERSION = 1
TARGET_DIM = 128                 # GL_SEMANTIC_DIM
GLOVE_DIM = 100                  # GloVe 6B-100d
MAX_FORM_LEN = 63                # leaves NUL room in C-side 64-byte buffer
GLOVE_URL = "https://downloads.cs.stanford.edu/nlp/data/glove.6B.zip"

# gl_word_class_t enum values (must match include/language/nimcp_grounded_language.h)
GL_CLASS_UNKNOWN   = 0
GL_CLASS_NOUN      = 1
GL_CLASS_VERB      = 2
GL_CLASS_ADJECTIVE = 3
GL_CLASS_ADVERB    = 4
GL_CLASS_FUNCTION  = 5
GL_CLASS_PRONOUN   = 6

# WordNet POS code -> gl_word_class_t
WN_POS_TO_CLASS = {
    "n": GL_CLASS_NOUN,
    "v": GL_CLASS_VERB,
    "a": GL_CLASS_ADJECTIVE,
    "s": GL_CLASS_ADJECTIVE,  # adjective satellite
    "r": GL_CLASS_ADVERB,
}


# ---------------------------------------------------------------- helpers

def log(msg: str) -> None:
    print(msg, file=sys.stderr, flush=True)


def is_ascii_word(form: str) -> bool:
    if not form or len(form) > MAX_FORM_LEN:
        return False
    if any(ch in form for ch in ("_", " ", "-", ".", "/", "'")):
        # multiword / hyphenated forms — skip; we want bare lemmas
        return False
    try:
        form.encode("ascii")
    except UnicodeEncodeError:
        return False
    return form.isalpha() and form.islower()


def stub_vector(form: str, dim: int) -> list[float]:
    """Hash-based pseudo-embedding when GloVe is unavailable."""
    import hashlib

    h = hashlib.sha256(form.encode("utf-8")).digest()
    out = []
    # repeat hash until we fill dim*4 bytes
    needed = dim * 4
    pool = bytearray()
    counter = 0
    while len(pool) < needed:
        pool.extend(hashlib.sha256(h + counter.to_bytes(4, "little")).digest())
        counter += 1
    for i in range(dim):
        # interpret 4 bytes as int32, scale to [-0.1, 0.1]
        n = int.from_bytes(pool[i * 4 : i * 4 + 4], "little", signed=True)
        out.append((n / (2**31)) * 0.1)
    # L2 normalize
    norm = sum(v * v for v in out) ** 0.5
    if norm > 1e-9:
        out = [v / norm for v in out]
    return out


def l2_normalize(v: list[float]) -> list[float]:
    norm = sum(x * x for x in v) ** 0.5
    if norm < 1e-9:
        return v
    return [x / norm for x in v]


# ---------------------------------------------------------------- glove

def ensure_glove(cache_dir: Path) -> Path | None:
    """Locate or download GloVe 6B-100d. Returns the .txt path or None."""
    cache_dir.mkdir(parents=True, exist_ok=True)
    glove_txt = cache_dir / "glove.6B.100d.txt"
    if glove_txt.exists() and glove_txt.stat().st_size > 100 * 1024 * 1024:
        log(f"glove: using cached {glove_txt} ({glove_txt.stat().st_size // (1024*1024)} MB)")
        return glove_txt

    glove_zip = cache_dir / "glove.6B.zip"
    if not glove_zip.exists() or glove_zip.stat().st_size < 800 * 1024 * 1024:
        log(f"glove: downloading {GLOVE_URL} -> {glove_zip}")
        try:
            import urllib.request

            urllib.request.urlretrieve(GLOVE_URL, glove_zip)
        except Exception as e:
            log(f"glove: download failed ({e}); falling back to stub vectors")
            return None

    log(f"glove: extracting 100d variant from {glove_zip}")
    try:
        with zipfile.ZipFile(glove_zip, "r") as zf:
            with zf.open("glove.6B.100d.txt") as src, open(glove_txt, "wb") as dst:
                while True:
                    chunk = src.read(8 * 1024 * 1024)
                    if not chunk:
                        break
                    dst.write(chunk)
    except Exception as e:
        log(f"glove: extract failed ({e}); falling back to stub vectors")
        return None

    log(f"glove: extracted to {glove_txt} ({glove_txt.stat().st_size // (1024*1024)} MB)")
    return glove_txt


def load_glove(path: Path) -> dict[str, list[float]]:
    """Load GloVe 100d into memory. Skips malformed lines."""
    log(f"glove: loading {path} ...")
    out: dict[str, list[float]] = {}
    with open(path, "r", encoding="utf-8") as f:
        for ln in f:
            parts = ln.rstrip().split(" ")
            if len(parts) != GLOVE_DIM + 1:
                continue
            word = parts[0]
            if not is_ascii_word(word):
                continue
            try:
                vec = [float(x) for x in parts[1:]]
            except ValueError:
                continue
            out[word] = vec
    log(f"glove: loaded {len(out)} ASCII vectors")
    return out


# ---------------------------------------------------------------- wordnet

def collect_wordnet(cap: int, min_freq: int) -> list[tuple[str, int, int]]:
    """
    Returns list of (form, gl_class_enum, total_count) sorted by frequency desc.
    Cap-limited to `cap` entries.
    """
    import nltk

    # Best-effort downloads — quiet=True; if already cached this is a no-op.
    for pkg in ("wordnet", "omw-1.4"):
        try:
            nltk.download(pkg, quiet=True)
        except Exception:
            pass

    from nltk.corpus import wordnet as wn

    log("wordnet: collecting lemmas across all synsets ...")
    # form -> {pos -> count}
    counts: dict[str, dict[str, int]] = {}
    for syn in wn.all_synsets():
        pos = syn.pos()  # 'n','v','a','s','r'
        if pos not in WN_POS_TO_CLASS:
            continue
        for lemma in syn.lemmas():
            form = lemma.name().lower()
            if not is_ascii_word(form):
                continue
            slot = counts.setdefault(form, {})
            slot[pos] = slot.get(pos, 0) + lemma.count() + 1  # +1 baseline

    log(f"wordnet: {len(counts)} unique ASCII lemmas")

    # Pick the dominant POS per form, sum its counts as the freq score.
    rows: list[tuple[str, int, int]] = []
    for form, pos_counts in counts.items():
        best_pos = max(pos_counts, key=lambda p: pos_counts[p])
        total = sum(pos_counts.values())
        if total < min_freq:
            continue
        rows.append((form, WN_POS_TO_CLASS[best_pos], total))

    rows.sort(key=lambda r: -r[2])
    if cap and len(rows) > cap:
        rows = rows[:cap]
    log(f"wordnet: kept {len(rows)} after freq>={min_freq} filter (cap {cap})")
    return rows


# ---------------------------------------------------------------- writer

def write_bulk_bin(path: Path, rows: list[tuple[str, int, list[float]]]) -> None:
    """Emit the .bin file with the format documented at module top."""
    with open(path, "wb") as f:
        # Header: 8 × uint32 = 32 bytes
        header = struct.pack(
            "<8I",
            MAGIC,
            VERSION,
            len(rows),
            TARGET_DIM,
            MAX_FORM_LEN + 1,  # record_max_form (includes NUL slot)
            0, 0, 0,
        )
        f.write(header)

        for form, klass, vec in rows:
            form_bytes = form.encode("ascii")
            assert len(form_bytes) <= MAX_FORM_LEN
            assert len(vec) == TARGET_DIM
            # uint16 form_len, ascii bytes, uint8 class, uint8 reserved
            f.write(struct.pack("<H", len(form_bytes)))
            f.write(form_bytes)
            f.write(struct.pack("<BB", klass, 0))
            # 128 floats
            f.write(struct.pack(f"<{TARGET_DIM}f", *vec))


# ---------------------------------------------------------------- main

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default="data/lexicon/wordnet_glove_v1.bin",
                    help="output .bin path (default: %(default)s)")
    ap.add_argument("--cap", type=int, default=60000,
                    help="max words to emit (default: %(default)s)")
    ap.add_argument("--glove",
                    default="data/lexicon/cache/glove.6B.100d.txt",
                    help="path to glove.6B.100d.txt (auto-downloaded if missing)")
    ap.add_argument("--min-freq", type=int, default=2,
                    help="min WordNet count threshold (default: %(default)s)")
    ap.add_argument("--no-glove", action="store_true",
                    help="skip GloVe entirely; use stub hash vectors")
    args = ap.parse_args()

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # 1. GloVe
    glove: dict[str, list[float]] = {}
    if not args.no_glove:
        glove_path = ensure_glove(Path(args.glove).parent)
        if glove_path is not None:
            glove = load_glove(glove_path)

    # 2. WordNet
    wn_rows = collect_wordnet(args.cap, args.min_freq)

    # 3. Join + pad/normalize
    log("merge: joining WordNet x GloVe ...")
    out_rows: list[tuple[str, int, list[float]]] = []
    have_glove = 0
    for form, klass, _freq in wn_rows:
        if form in glove:
            v = glove[form]
            have_glove += 1
        else:
            v = stub_vector(form, GLOVE_DIM)
        # pad GloVe 100d -> 128d with zeros, then L2 normalize
        if len(v) < TARGET_DIM:
            v = v + [0.0] * (TARGET_DIM - len(v))
        elif len(v) > TARGET_DIM:
            v = v[:TARGET_DIM]
        v = l2_normalize(v)
        out_rows.append((form, klass, v))

    log(f"merge: {have_glove}/{len(out_rows)} words had real GloVe vectors")

    # 4. Write
    write_bulk_bin(out_path, out_rows)
    log(f"out:   wrote {len(out_rows)} entries to {out_path} "
        f"({out_path.stat().st_size // 1024} KB)")

    # Sanity preview — print first few words for the build report.
    preview = ", ".join(f"{form}/{klass}" for form, klass, _ in out_rows[:8])
    log(f"preview: {preview}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
