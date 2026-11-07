#!/usr/bin/env python3
"""Standalone test for the video monitor and the video-phase exit rule in
scripts/immerse_athena.py (2026-11-07).

Mock brain and sensory bank (no datasets, no C library calls). A brain whose
output tile 4 repeats the CURRENT frame must not beat the copy baseline; one
that shows the true NEXT frame must, and three such monitor runs (with the
temporal probe passing) end the video phase and record it on disk.

Functions under test (scripts/immerse_athena.py):
  _video_prediction_probe(brain, composer, n_shots=VIDEO_PROBE_SHOTS, seed=0) -> dict
  _video_monitor(brain, composer, step, stage=0) -> dict
  _video_exit_streak() -> int
  _video_phase_ended() -> (bool, dict | None)
  _end_video_phase(step, reason) -> None
  _video_train_window() -> dict
  _next_frame_tile(emb, tile_rms) -> np.ndarray | None
Brain methods the monitor calls (mocked here): cortex_embed_visual,
submit_sensory, decide_full, lnn_get_state, lnn_tune, lnn_tune_get,
snn_tune, snn_tune_get, reset_inference_state, lnn_get_stats, snn_get_stats.

Run: python3 tests/python/test_video_monitor.py
"""
import json
import os
import sys
import tempfile

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


def frame(i):
    return bytes([i] * 12)   # 2x2x3, first value identifies the frame


def embed(i):
    v = np.zeros(256, dtype=np.float32)
    v[i % 256] = 1.0
    v[(i * 7 + 3) % 256] += 0.5
    return v


SHOTS = [{"title": f"Film {k}", "description": f"film {k}",
          "frames": [frame(10 * k + j) for j in range(6)]} for k in range(1, 5)]


class Bank:
    resolution = 2
    heldout_sequences = SHOTS

    def to_stage_frame(self, raw):
        return raw


class Composer:
    def compose(self, text, modality):
        return np.ones(1024, dtype=np.float32)


class Brain:
    """mode 'copy': tile 4 = projected current frame; 'oracle': projected next frame."""

    def __init__(self, mode):
        self.mode = mode
        self.staged = None
        self.tune = {"decide_readout": 1.0, "substrate_enabled": 1.0, "thalamic_enabled": 1.0}
        self.snn = {"noise_rate_hz": 20.0, "decide_carry_state": 1.0}
        self.lnn_state = np.zeros(64)

    def cortex_embed_visual(self, pixels, width, height, channels=3):
        return list(embed(pixels[0]))

    def submit_sensory(self, modality, pixels, width, height, channels):
        self.staged = pixels[0]

    def decide_full(self, features):
        fid = self.staged
        show = fid + 1 if self.mode == "oracle" else fid
        out = np.full(4096, 0.01)
        out[3072:4096] = ia._next_frame_tile(list(embed(show)), 1.0)
        if self.tune["decide_readout"]:
            out[:64] += 0.5           # the LNN readout's contribution
        self.lnn_state = 0.9 * self.lnn_state + 0.1 * (fid % 7)
        return {"output_vector": list(out)}

    def lnn_get_state(self):
        return list(self.lnn_state)

    def lnn_tune(self, k, v):
        self.tune[k] = v

    def lnn_tune_get(self):
        return dict(self.tune)

    def snn_tune(self, k, v):
        self.snn[k] = v

    def snn_tune_get(self):
        return dict(self.snn)

    def reset_inference_state(self):
        self.lnn_state = np.zeros(64)

    def lnn_get_stats(self):
        return {"gradient_norm": 3.5, "backward_steps": 10}

    def snn_get_stats(self):
        return {"mean_firing_rate": 6.0}


def setup(tmp):
    ia._REAL_SENSORY = Bank()
    ia.VIDEO_MONITOR_LOG = os.path.join(tmp, "video_monitor.jsonl")
    ia.VIDEO_PHASE_DONE_FILE = os.path.join(tmp, "video_phase_done.json")
    ia.VIDEO_PHASE = True


def test_copy_brain_does_not_beat_baseline(tmp):
    setup(tmp)
    ia._video_temporal_probe = lambda *a, **k: {"temporal": 5e-3}
    b = Brain("copy")
    rec = ia._video_monitor(b, Composer(), 109000)
    p = rec["prediction"]
    expect(p["pairs"] == 4 * 5, f"5 frame pairs per 6-frame shot, 4 shots: {p['pairs']}")
    expect(abs(p["skill"]) < 1e-6, f"copying the current frame has zero skill: {p['skill']}")
    expect(abs(p["track_cos"] - 1.0) < 1e-6, f"copy brain tracks the frame: {p['track_cos']}")
    expect(not rec["exit_pass"], "no skill -> no exit pass")
    expect(p["lnn_readout_share"] and p["lnn_readout_share"] > 0, "readout share measured")
    expect(b.tune["decide_readout"] == 1.0, "readout restored after the share measurement")
    expect(b.snn["noise_rate_hz"] == 20.0, "controlled inference restores SNN noise")
    with open(ia.VIDEO_MONITOR_LOG) as f:
        lines = [json.loads(x) for x in f]
    expect(len(lines) == 1 and lines[0]["step"] == 109000, "one JSON line per monitor run")
    expect(lines[0]["lnn"]["gradient_norm"] == 3.5 and lines[0]["snn_rate_hz"] == 6.0,
           "LNN and SNN stats recorded")
    expect(ia.VIDEO_PHASE, "video phase still on")


def test_oracle_brain_ends_phase_after_three(tmp):
    setup(tmp)
    ia._video_temporal_probe = lambda *a, **k: {"temporal": 5e-3}
    b = Brain("oracle")
    for n, step in enumerate((110000, 111000, 112000), start=1):
        rec = ia._video_monitor(b, Composer(), step)
        expect(rec["exit_pass"], f"oracle passes at {step}")
        expect(rec["prediction"]["skill"] > 0, "oracle beats the copy baseline")
        expect(ia._video_exit_streak() == n, f"streak {n}")
        expect(ia.VIDEO_PHASE == (n < 3), f"phase on until the third pass (n={n})")
    done, done_rec = ia._video_phase_ended()
    expect(done and done_rec["step"] == 112000, f"end recorded on disk: {done_rec}")


def test_temporal_gate_blocks(tmp):
    setup(tmp)
    ia._video_temporal_probe = lambda *a, **k: {"temporal": 1e-6}
    rec = ia._video_monitor(Brain("oracle"), Composer(), 113000)
    expect(not rec["exit_pass"], "skill without order sensitivity does not pass")


def test_streak_resets(tmp):
    setup(tmp)
    with open(ia.VIDEO_MONITOR_LOG, "w") as f:
        for ok in (True, True, False, True):
            f.write(json.dumps({"exit_pass": ok}) + "\n")
    expect(ia._video_exit_streak() == 1, "a failing run resets the streak")


def test_train_window():
    ia._VIDEO_TRAIN_SNAPSHOT = {}
    ia._video_train_window()
    s = ia._VIDEO_PHASE_STATS
    s["frames"] += 4
    s["loss_sum"] += 4.0
    s["n_full_loss"] += 1
    s["loss_full_sum"] += 2.0
    s["n_tiled_loss"] += 3
    s["loss_tiled_sum"] += 3.0
    w = ia._video_train_window()
    expect(w["frames"] == 4 and abs(w["loss_mean"] - 1.0) < 1e-9, f"window mean: {w}")
    expect(abs(w["loss_full_mean"] - 2.0) < 1e-9 and abs(w["loss_tiled_mean"] - 1.0) < 1e-9,
           f"split by target type: {w}")
    expect(ia._video_train_window()["frames"] == 0, "window restarts after each read")


if __name__ == "__main__":
    for t in (test_copy_brain_does_not_beat_baseline, test_oracle_brain_ends_phase_after_three,
              test_temporal_gate_blocks, test_streak_resets):
        with tempfile.TemporaryDirectory() as tmp:
            t(tmp)
    test_train_window()
    if failures:
        print(f"test_video_monitor: {failures} failure(s)")
        sys.exit(1)
    print("test_video_monitor: all passed")
