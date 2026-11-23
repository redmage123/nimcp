"""Text corpus -> token ids, with a word or subword vocabulary.

Text is first split into words (lower-cased, punctuation as separate tokens).
A word vocabulary maps each word to one token, and words outside it to <unk>.
A subword vocabulary (byte-pair merges learned on the training words) keeps
frequent words whole and spells rare ones from pieces ("##" marks a piece
that continues a word), so nothing is <unk>. Either way the naming words are
whole tokens, so the word Athena picks for a picture and the word her
language model predicts are the same token.
Each book is split by position: the last `held_frac` of every book is held
out (half validation, half test), so no held-out passage is ever trained on.
"""
from __future__ import annotations

import heapq
import os
import re
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import torch

CACHE_DIR = Path(os.environ.get("ATHENA_CACHE", Path.home() / ".cache" / "athena"))
CORPUS_DIR = Path(os.environ.get("ATHENA_CORPUS_DIR", CACHE_DIR / "corpus" / "gutenberg"))

PAD, UNK, BOS, EOS = "<pad>", "<unk>", "<bos>", "<eos>"
SPECIALS = [PAD, UNK, BOS, EOS]
CONT = "##"                       # marks a subword piece that continues a word
_ALPHABET = "abcdefghijklmnopqrstuvwxyz0123456789'"
_PUNCT = '.,;:!?"()-'
_TOKEN = re.compile(r"[a-z]+(?:'[a-z]+)?|[0-9]+|[.,;:!?\"()\-]")
_START = re.compile(r"\*\*\* ?START OF (THE|THIS) PROJECT GUTENBERG EBOOK[^\n]*\n", re.I)
_END = re.compile(r"\*\*\* ?END OF (THE|THIS) PROJECT GUTENBERG EBOOK", re.I)


def strip_gutenberg(text: str) -> str:
    """Keep only the book between the Project Gutenberg start and end markers."""
    m = _START.search(text)
    if m:
        text = text[m.end():]
    m = _END.search(text)
    if m:
        text = text[:m.start()]
    return text


def tokenize(text: str) -> list[str]:
    return _TOKEN.findall(text.lower().replace("’", "'").replace("“", '"').replace("”", '"'))


class Vocab:
    def __init__(self, tokens: list[str]):
        self.itos = list(tokens)
        self.stoi = {t: i for i, t in enumerate(self.itos)}
        self.subword = any(t.startswith(CONT) for t in self.itos)
        self._split_cache: dict[str, list[int]] = {}

    def __len__(self) -> int:
        return len(self.itos)

    def word_starts(self) -> torch.Tensor:
        """True for tokens that begin a word (everything but continuation
        pieces): per-word perplexity divides by the number of these."""
        return torch.tensor([not t.startswith(CONT) for t in self.itos])

    def pieces(self, word: str) -> list[int]:
        i = self.stoi.get(word)
        if i is not None:
            return [i]
        if not self.subword:
            return [self.stoi[UNK]]
        hit = self._split_cache.get(word)
        if hit is None:
            hit = self._split_cache[word] = self._split(word)
        return hit

    def _split(self, word: str) -> list[int]:
        """Greedy longest-match spelling of a word from the pieces."""
        out, i = [], 0
        while i < len(word):
            for j in range(len(word), i, -1):
                piece = word[i:j] if i == 0 else CONT + word[i:j]
                k = self.stoi.get(piece)
                if k is not None:
                    out.append(k)
                    i = j
                    break
            else:
                return [self.stoi[UNK]]
        return out

    def encode(self, tokens: list[str]) -> list[int]:
        if not self.subword:
            unk = self.stoi[UNK]
            return [self.stoi.get(t, unk) for t in tokens]
        out: list[int] = []
        for t in tokens:
            out.extend(self.pieces(t))
        return out

    def decode(self, ids) -> str:
        return " ".join(self.itos[int(i)] for i in ids).replace(" " + CONT, "")

    @classmethod
    def build(cls, counts: Counter, size: int, required: list[str] = ()) -> "Vocab":
        """The `size` most frequent tokens, plus every token of `required`
        (e.g. the naming words) whether frequent or not."""
        must = [t for w in required for t in tokenize(w)]
        tokens = list(SPECIALS)
        seen = set(tokens)
        for t in must:
            if t not in seen:
                tokens.append(t)
                seen.add(t)
        for t, _ in counts.most_common():
            if len(tokens) >= size:
                break
            if t not in seen:
                tokens.append(t)
                seen.add(t)
        return cls(tokens)

    @classmethod
    def learn_subwords(cls, counts: Counter, size: int, required: list[str] = ()) -> "Vocab":
        """Byte-pair merges over the training words until the vocabulary has
        `size` tokens. Starts from every character (word-initial and "##"
        continuing forms), and every token of `required` is added whole."""
        tokens = list(SPECIALS)
        seen = set(tokens)

        def add(t: str) -> None:
            if t not in seen:
                tokens.append(t)
                seen.add(t)

        for t in (t for w in required for t in tokenize(w)):
            add(t)
        chars = set(_ALPHABET) | set(_PUNCT) | {c for w in counts for c in w}
        for c in sorted(chars):
            add(c)
            if c not in _PUNCT:
                add(CONT + c)
        words = [([w[0]] + [CONT + c for c in w[1:]], f) for w, f in counts.items() if len(w) > 1]
        pair_count: Counter = Counter()
        where: dict[tuple[str, str], set[int]] = defaultdict(set)
        for i, (syms, f) in enumerate(words):
            for p in zip(syms, syms[1:]):
                pair_count[p] += f
                where[p].add(i)
        heap = [(-c, p) for p, c in pair_count.items()]
        heapq.heapify(heap)
        while len(tokens) < size and heap:
            c, pair = heapq.heappop(heap)
            if -c != pair_count.get(pair, 0) or c == 0:
                continue                          # stale entry
            a, b = pair
            merged = a + b[len(CONT):]
            add(merged)
            touched = set()
            for i in where.pop(pair, ()):
                syms, f = words[i]
                for p in zip(syms, syms[1:]):
                    pair_count[p] -= f
                    touched.add(p)
                new, k = [], 0
                while k < len(syms):
                    if k + 1 < len(syms) and syms[k] == a and syms[k + 1] == b:
                        new.append(merged)
                        k += 2
                    else:
                        new.append(syms[k])
                        k += 1
                for p in zip(new, new[1:]):
                    pair_count[p] += f
                    where[p].add(i)
                    touched.add(p)
                words[i] = (new, f)
            pair_count.pop(pair, None)
            for p in touched:
                if pair_count.get(p, 0) > 0 and p != pair:
                    heapq.heappush(heap, (-pair_count[p], p))
        return cls(tokens)


@dataclass
class Corpus:
    vocab: Vocab
    train: torch.Tensor   # int32 token ids, books concatenated with <eos> between them
    val: torch.Tensor
    test: torch.Tensor
    books: list[str]
    val_by_source: dict[str, torch.Tensor] | None = None
    test_by_source: dict[str, torch.Tensor] | None = None
    n_canonical_train: int | None = None   # train[:n] is the canonical books (the rest is extra text)

    def unk_rate(self, split: str = "val") -> float:
        ids = getattr(self, split)
        return (ids == self.vocab.stoi[UNK]).float().mean().item()


def _split_book(toks: list[str], held_frac: float) -> tuple[list[str], list[str], list[str]]:
    n = len(toks)
    cut_v = int(n * (1 - held_frac))
    cut_t = int(n * (1 - held_frac / 2))
    return toks[:cut_v], toks[cut_v:cut_t], toks[cut_t:]


def _ids(vocab: Vocab, chunks: list[list[str]]) -> torch.Tensor:
    eos = vocab.stoi[EOS]
    out: list[int] = []
    for c in chunks:
        out.extend(vocab.encode(c))
        out.append(eos)
    return torch.tensor(np.asarray(out, dtype=np.int32))


def _key(*parts: str) -> str:
    import hashlib

    return hashlib.md5("|".join(parts).encode()).hexdigest()[:10]


def _pg19_parts(path: Path, vocab: Vocab, held_frac: float, exclude_ids: set[str]) -> dict[str, torch.Tensor]:
    """Tokenise a PG-19 parquet shard into train/val/test ids with the given
    vocabulary, cached next to the shard (tokenising thousands of books is
    slow; once cached, the parquet file itself can be deleted). Ids are
    stored as uint16 when the vocabulary allows. Books whose Gutenberg id is
    in `exclude_ids` (the canonical books) are skipped: their held-out ends
    would otherwise leak into training."""
    key = _key("\n".join(vocab.itos), ",".join(sorted(exclude_ids)))
    cache = path.with_suffix(f".{key}.h{held_frac}.npz")
    if not cache.exists():
        import pyarrow.parquet as pq

        dtype = np.uint16 if len(vocab) <= 65536 else np.int32
        eos = vocab.stoi[EOS]
        parts: dict[str, list[np.ndarray]] = {"train": [], "val": [], "test": []}
        pf = pq.ParquetFile(path)
        for rg in range(pf.num_row_groups):
            table = pf.read_row_group(rg, columns=["url", "text"])
            for url, text in zip(table.column("url").to_pylist(), table.column("text").to_pylist()):
                if url.rstrip("/").rsplit("/", 1)[-1] in exclude_ids:
                    continue
                for name, chunk in zip(("train", "val", "test"), _split_book(tokenize(text), held_frac)):
                    parts[name].append(np.asarray(vocab.encode(chunk) + [eos], dtype=dtype))
            del table
        vocab._split_cache.clear()
        tmp = cache.with_suffix(".tmp.npz")
        np.savez_compressed(tmp, **{k: np.concatenate(v) for k, v in parts.items()})
        tmp.replace(cache)
    z = np.load(cache)
    return {k: torch.from_numpy(z[k].astype(np.int32)) for k in ("train", "val", "test")}


def _subword_vocab(files: list[Path], counts: Counter, size: int, required: list[str], held_frac: float) -> Vocab:
    """Learned subword vocabulary, cached (learning takes a minute or two)."""
    key = _key(",".join(f"{f.name}:{f.stat().st_size}" for f in files), str(size), "\n".join(required),
               str(held_frac))
    cache = CACHE_DIR / "vocab" / f"subword_{key}.txt"
    if cache.exists():
        return Vocab(cache.read_text(encoding="utf-8").split("\n"))
    vocab = Vocab.learn_subwords(counts, size, required)
    cache.parent.mkdir(parents=True, exist_ok=True)
    tmp = cache.with_suffix(".tmp")
    tmp.write_text("\n".join(vocab.itos), encoding="utf-8")
    tmp.replace(cache)
    return vocab


def load_corpus(vocab_size: int = 16384, required_words: list[str] = (), held_frac: float = 0.1,
                corpus_dir: Path | None = None, extra: list[Path] = (), subwords: bool = False) -> Corpus:
    """The canonical books (.txt in corpus_dir), plus optional PG-19 parquet
    shards. The vocabulary always comes from the canonical books' training
    text, so perplexity on their held-out text stays comparable however much
    extra text is added (extra text maps onto the same vocabulary).
    subwords=True learns byte-pair pieces instead of keeping whole words."""
    corpus_dir = corpus_dir or CORPUS_DIR
    files = sorted(corpus_dir.glob("*.txt"), key=lambda p: (len(p.stem), p.stem))
    if not files:
        raise FileNotFoundError(f"no .txt books in {corpus_dir}")
    books = [tokenize(strip_gutenberg(f.read_text(encoding="utf-8", errors="replace"))) for f in files]
    parts: dict[str, list[list[str]]] = {"train": [], "val": [], "test": []}
    counts: Counter = Counter()
    for toks in books:
        tr, va, te = _split_book(toks, held_frac)
        parts["train"].append(tr)
        parts["val"].append(va)
        parts["test"].append(te)
        counts.update(tr)                     # vocabulary from training text only
    if subwords:
        vocab = _subword_vocab(files, counts, vocab_size, list(required_words), held_frac)
    else:
        vocab = Vocab.build(counts, vocab_size, required_words)
    canon = {k: _ids(vocab, v) for k, v in parts.items()}
    val_by, test_by = {"canonical": canon["val"]}, {"canonical": canon["test"]}
    train = [canon["train"]]
    for path in extra:
        p = _pg19_parts(Path(path), vocab, held_frac, {f.stem for f in files})
        name = Path(path).stem
        train.append(p["train"])
        val_by[name], test_by[name] = p["val"], p["test"]
    return Corpus(vocab, torch.cat(train), canon["val"], canon["test"], [f.stem for f in files],
                  val_by, test_by, n_canonical_train=len(canon["train"]))
