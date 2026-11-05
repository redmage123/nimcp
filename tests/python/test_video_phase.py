#!/usr/bin/env python3
"""Standalone test for the stage-0 video phase in scripts/immerse_athena.py
(2026-11-05): each shot is learned frame by frame against next-frame targets.

Uses a mock brain and sensory bank (no datasets, no C library calls).
Run: python3 tests/python/test_video_phase.py
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
sys.argv = [sys.argv[0]]
import immerse_athena as ia  # noqa: E402

failures = 0


def expect(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


class Bank:
    resolution = 2

    def to_stage_frame(self, raw):
        return raw

    @staticmethod
    def sequence_label(seq):
        return "a scene from the film " + seq["title"]


class Brain:
    """Records the order of calls and what each learn step saw."""

    def __init__(self, embed_ok=True):
        self.calls = []
        self.staged = None
        self.learned = []          # (staged frame, target)
        self.embed_ok = embed_ok

    def cortex_embed_visual(self, pixels, width, height, channels=3):
        self.calls.append(("embed", pixels[0]))
        if not self.embed_ok:
            return None
        v = np.zeros(256, dtype=np.float32)
        v[pixels[0] % 256] = 1.0   # frame identity -> distinct embedding
        return list(v)

    def submit_sensory(self, modality, pixels, width, height, channels):
        self.calls.append(("stage", pixels[0]))
        self.staged = pixels[0]

    def set_visual_target(self, target, modality=0):
        pass

    def learn_vector(self, features, target, label=None, confidence=1.0, learning_rate=0.0):
        self.calls.append(("learn", self.staged))
        self.learned.append((self.staged, np.asarray(target, dtype=np.float32)))
        return 0.5


class Composer:
    def compose(self, text, modality):
        return np.ones(1024, dtype=np.float32)


def frame(i):
    return bytes([i] * 12)   # 2x2x3, first value identifies the frame


SEQ = {"title": "Film One", "description": "a moving scene from the film 'Film One'",
       "frames": [frame(10 + k) for k in range(5)]}


def test_next_frame_tile():
    e = np.zeros(256, dtype=np.float32)
    e[3] = 1.0
    y = ia._next_frame_tile(e, 0.05)
    expect(y is not None and y.shape == (1024,), "tile is 1024-d")
    expect(abs(float(np.sqrt(np.mean(y * y))) - 0.05) < 1e-4, "tile scaled to the caption RMS")
    expect(np.allclose(y, ia._next_frame_tile(e, 0.05)), "projection is fixed")
    e2 = np.zeros(256, dtype=np.float32)
    e2[4] = 1.0
    expect(not np.allclose(y, ia._next_frame_tile(e2, 0.05)), "different embeddings differ")
    expect(ia._next_frame_tile(np.zeros(256), 0.05) is None, "zero embedding gives no target")


def test_shot_frame_by_frame():
    ia.VIDEO_TARGET_FULL_PROB = 0.0
    for k in ia._VIDEO_PHASE_STATS:
        ia._VIDEO_PHASE_STATS[k] = 0
    b = Brain()
    losses = ia._learn_video_shot(b, Bank(), SEQ, Composer(), 0.01)
    expect(len(losses) == 4, f"one learn per frame pair (got {len(losses)})")
    expect([s for s, _ in b.learned] == [10, 11, 12, 13], "frame t is staged when it is learned")
    # each step: embed t+1, then stage t, then learn
    expect(b.calls[:3] == [("embed", 11), ("stage", 10), ("learn", 10)],
           f"embed next frame before staging the current one: {b.calls[:3]}")
    caption = np.asarray(ia.make_semantic_target(SEQ["description"]), dtype=np.float32)
    rms = float(np.sqrt(np.mean(caption[:1024] ** 2)))
    for t, (_, tgt) in enumerate(b.learned):
        expect(np.allclose(tgt[:3072], caption[:3072]), f"step {t}: caption kept in tiles 1-3")
        e = np.zeros(256, dtype=np.float32)
        e[11 + t] = 1.0
        expect(np.allclose(tgt[3072:4096], ia._next_frame_tile(e, rms), atol=1e-6),
               f"step {t}: tile 4 is frame {11 + t}'s projection")
    s = ia._VIDEO_PHASE_STATS
    expect(s["shots"] == 1 and s["frames"] == 4 and s["full"] == 0, f"stats {s}")


def test_full_targets():
    ia.VIDEO_TARGET_FULL_PROB = 1.0
    b = Brain()
    ia._learn_video_shot(b, Bank(), SEQ, Composer(), 0.01)
    for t, (_, tgt) in enumerate(b.learned):
        tile = tgt[3072:4096]
        expect(np.allclose(tgt, np.tile(tile, 4)), f"step {t}: whole target is the next frame")
    ia.VIDEO_TARGET_FULL_PROB = 0.3


def test_mix_probability():
    ia.VIDEO_TARGET_FULL_PROB = 0.3
    ia._VIDEO_PHASE_STATS["full"] = 0
    ia._VIDEO_PHASE_STATS["frames"] = 0
    ia.random.seed(7)
    b = Brain()
    for _ in range(200):
        ia._learn_video_shot(b, Bank(), SEQ, Composer(), 0.01)
    s = ia._VIDEO_PHASE_STATS
    frac = s["full"] / max(1, s["frames"])
    expect(0.22 < frac < 0.38, f"full-target fraction ~0.3 (got {frac:.3f})")


def test_no_embedding_stops():
    b = Brain(embed_ok=False)
    before = ia._VIDEO_PHASE_STATS["no_embed"]
    losses = ia._learn_video_shot(b, Bank(), SEQ, Composer(), 0.01)
    expect(losses == [] and not b.learned, "no visual cortex yet -> nothing learned")
    expect(ia._VIDEO_PHASE_STATS["no_embed"] == before + 1, "embed miss counted")


def test_short_shot():
    b = Brain()
    expect(ia._learn_video_shot(b, Bank(), dict(SEQ, frames=[frame(1)]), Composer(), 0.01) == [],
           "single-frame shot is skipped")


if __name__ == "__main__":
    test_next_frame_tile()
    test_shot_frame_by_frame()
    test_full_targets()
    test_mix_probability()
    test_no_embedding_stops()
    test_short_shot()
    if failures:
        print(f"test_video_phase: {failures} failure(s)")
        sys.exit(1)
    print("test_video_phase: all passed")
