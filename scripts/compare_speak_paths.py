#!/usr/bin/env python3
"""Compare nimcp's two text-production paths on the live brain.

nimcp_brain_speak() normally uses the LNN language generator
(src/generation/, its own tokenizer). grounded_language production
(nimcp_brain_produce_text) is the main language system. Setting
NIMCP_SPEAK_VIA_GROUNDED=1 in the daemon's environment routes speak()
through grounded production instead; this script measures whether that
would be an improvement before anyone flips it.

Task: the trainer's own "repeat after me" exercise
(Parent._teach_speech in immerse_athena.py). For each phrase:
compose the speech input, decide_full -> output_vector, then produce text
from that vector with each path. A reply scores a hit when it contains any
word of the target phrase -- the same rule the trainer rewards.

Two phrase sets: the trainer's stage 0-3 phrases ("seen") and phrases it
never uses ("held-out"). Also reported: empty replies, mean length, and
distinct replies / total (low = one answer for everything).

Both paths read the same output_vector, so differences come from the
production path alone. Uses the main socket (produce_text is not on the
read-only socket); each call takes the daemon lock briefly, like the
trainer's own speak() calls. Nothing is learned and no reward is sent.

Usage (on the box running brain_daemon):
    PYTHONPATH=scripts python3 scripts/compare_speak_paths.py [--repeats 3]
"""
import argparse
import json
import re
import sys
import time

from brain_client import BrainProxy
from immerse_athena import SensoryComposer

SEEN = [
    "dog", "cat", "water", "ball", "sun", "tree",
    "hello", "yes", "no", "good", "come", "go", "see", "the", "is",
    "big dog", "red ball", "I see", "come here", "good day",
    "the dog runs", "I see you", "it is good", "come and see",
    "the cat is here", "water is good",
]
HELD_OUT = [
    "bird", "milk", "house", "car", "hand", "book",
    "small cat", "blue water", "my house",
    "the bird sings", "the car is red", "I like milk",
]
STOP = {"the", "is", "a", "and", "i", "it", "you", "my"}


def words(text):
    return re.findall(r"[a-z']+", (text or "").lower())


def hit(target, reply):
    tw = set(words(target))
    content = tw - STOP or tw        # "the"/"is" alone would make any sentence a hit
    return bool(content & set(words(reply)))


def run(proxy, composer, phrases, repeats):
    rows = []
    for _ in range(repeats):
        for p in phrases:
            feats = composer.compose(text=p, modality="speech")
            out = proxy.decide_full(feats).get("output_vector")
            if not out:
                rows.append({"phrase": p, "lnn": "", "grounded": "", "novec": True})
                continue
            t0 = time.time()
            lnn = proxy.speak(out).get("text", "") or ""
            t1 = time.time()
            gr = proxy.produce_text(out).get("text", "") or ""
            t2 = time.time()
            rows.append({"phrase": p, "lnn": lnn, "grounded": gr,
                         "lnn_s": t1 - t0, "grounded_s": t2 - t1})
    return rows


def summary(rows, key):
    n = len(rows)
    replies = [r[key] for r in rows]
    nonempty = [x for x in replies if x.strip()]
    return {
        "n": n,
        "hit": sum(hit(r["phrase"], r[key]) for r in rows) / max(1, n),
        "empty": 1 - len(nonempty) / max(1, n),
        "mean_words": sum(len(words(x)) for x in nonempty) / max(1, len(nonempty)),
        "distinct": len(set(x.strip().lower() for x in nonempty)) / max(1, len(nonempty)),
        "secs": sum(r.get(f"{key}_s", 0) for r in rows) / max(1, n),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--socket", default=None)
    ap.add_argument("--out", default="speak_compare.json")
    a = ap.parse_args()

    proxy = BrainProxy(a.socket) if a.socket else BrainProxy()
    composer = SensoryComposer(proxy)
    result = {}
    for name, phrases in (("seen", SEEN), ("held_out", HELD_OUT)):
        rows = run(proxy, composer, phrases, a.repeats)
        result[name] = {"lnn": summary(rows, "lnn"), "grounded": summary(rows, "grounded"),
                        "rows": rows}
        print(f"== {name} ({len(rows)} replies per path)")
        for k in ("lnn", "grounded"):
            s = result[name][k]
            print(f"  {k:9s} hit {s['hit']:.3f}  empty {s['empty']:.3f}  "
                  f"words {s['mean_words']:.1f}  distinct {s['distinct']:.3f}  {s['secs']:.2f}s/call")
        for r in rows[:6]:
            print(f"    {r['phrase']!r:20s} lnn={r['lnn'][:50]!r}  grounded={r['grounded'][:50]!r}")
    with open(a.out, "w") as f:
        json.dump(result, f, indent=1)
    print(f"wrote {a.out}")


if __name__ == "__main__":
    sys.exit(main())
