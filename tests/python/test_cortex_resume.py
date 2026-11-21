#!/usr/bin/env python3
"""Cortex CNN weights survive save -> load (2026-11-05).

The cortexes are created lazily on the first learn step with staged sensory
data, and brain load read a cortex sidecar only if that cortex already
existed — so on every resume the visual/audio/speech/somato cortexes started
from scratch (the pod's visual cortex reset from 6009 trained steps to 0).
Load now creates a cortex for its sidecar.

Needs the built Python module: run with PYTHONPATH=build/lib/python
  python3 tests/python/test_cortex_resume.py
"""
import os
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "build", "lib", "python"))
os.environ.setdefault("CUDA_VISIBLE_DEVICES", "")
import nimcp  # noqa: E402

failures = 0


def expect(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


SIDE = 32


def frame(seed):
    return list(np.random.default_rng(seed).integers(0, 256, SIDE * SIDE * 3, dtype=np.uint8))


def main():
    b = nimcp.Brain("resume", num_inputs=1024, num_outputs=4096, neuron_count=4096,
                    init_mode="fast")
    expect(b.cortex_embed_visual(frame(0), SIDE, SIDE, 3) is None, "no visual cortex before any frame")
    rng = np.random.default_rng(1)
    for i in range(20):   # train the visual cortex so its weights move off init
        b.submit_sensory("visual", frame(100 + i), width=SIDE, height=SIDE, channels=3)
        b.set_visual_target(list(rng.standard_normal(256).astype(float)), 0)
        b.learn_vector([0.1] * 1024, [0.01] * 4096, label=f"frame {i}")
    probe = frame(7)
    before = b.cortex_embed_visual(probe, SIDE, SIDE, 3)
    expect(before is not None and len(before) == 256, "trained cortex embeds")

    d = tempfile.mkdtemp(prefix="cortex_resume_")
    path = os.path.join(d, "brain.bin")
    b.save(path)
    expect(os.path.exists(path + ".cortex_visual"), "visual cortex sidecar written")

    fresh = nimcp.Brain("other", num_inputs=1024, num_outputs=4096, neuron_count=4096,
                        init_mode="fast")
    fresh.submit_sensory("visual", frame(5), width=SIDE, height=SIDE, channels=3)
    fresh.learn_vector([0.1] * 1024, [0.01] * 4096, label="untrained")
    untrained = fresh.cortex_embed_visual(probe, SIDE, SIDE, 3)

    loaded = nimcp.Brain.load(path)
    after = loaded.cortex_embed_visual(probe, SIDE, SIDE, 3)
    expect(after is not None, "visual cortex exists right after load, before any learn step")
    # The CNN weights are restored; the FNO visual component and its mix-in
    # ramp are not persisted (cortex_cnn_load restores the CNN trainer only),
    # so the embedding is close to, not identical with, the saved one.
    if before is not None and after is not None and untrained is not None:
        b0, a0, u0 = (np.asarray(x, dtype=np.float64) for x in (before, after, untrained))
        d_loaded = float(np.linalg.norm(a0 - b0) / (np.linalg.norm(b0) + 1e-12))
        d_fresh = float(np.linalg.norm(u0 - b0) / (np.linalg.norm(b0) + 1e-12))
        print(f"  relative distance to the saved embedding: loaded {d_loaded:.4f}, "
              f"untrained {d_fresh:.4f}")
        expect(d_loaded < 0.25 * d_fresh,
               f"loaded cortex is far closer to the saved one than an untrained cortex "
               f"({d_loaded:.4f} vs {d_fresh:.4f})")

    for f in os.listdir(d):
        os.remove(os.path.join(d, f))
    os.rmdir(d)


if __name__ == "__main__":
    main()
    if failures:
        print(f"test_cortex_resume: {failures} failure(s)")
        sys.exit(1)
    print("test_cortex_resume: all passed")
