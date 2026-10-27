#!/usr/bin/env python3
"""Standalone test for scripts/real_sensory.py (no datasets needed: builds a
tiny synthetic cache in a temp dir).

Run: python3 tests/python/test_real_sensory.py
"""
import os
import pickle
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
from real_sensory import RealSensoryBank, SYNONYMS, _candidate_words  # noqa: E402

failures = 0


def expect(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


def make_cache(d):
    rng = np.random.RandomState(0)
    imgs = {lab: [rng.randint(0, 256, size=(32, 32, 3), dtype=np.uint8).tobytes()
                  for _ in range(3)]
            for lab in ("dog", "cat", "oak_tree", "pine_tree", "sea", "woman", "truck")}
    with open(os.path.join(d, "cifar_by_label.pkl"), "wb") as f:
        pickle.dump(imgs, f)
    with open(os.path.join(d, "cifar_heldout_by_label.pkl"), "wb") as f:
        pickle.dump({"dog": imgs["dog"][:1], "sea": imgs["sea"][:1]}, f)
    clip = (np.sin(np.linspace(0, 200, 16000)) * np.hanning(16000)).astype(np.float32)
    with open(os.path.join(d, "speech_cmds_by_label.pkl"), "wb") as f:
        pickle.dump({"dog": [clip.tobytes()], "happy": [clip.tobytes()]}, f)


with tempfile.TemporaryDirectory() as d:
    make_cache(d)
    bank = RealSensoryBank(d, seed=1)
    expect(bank.available(), "bank should report available")

    # matching: direct label, synonym, plural, no match
    expect(bank.match_label("A friendly dog with soft fur") == "dog", "direct label")
    expect(bank.match_label("The loving mother holds her child") == "woman", "synonym mother->woman")
    expect(bank.match_label("Tall trees sway in the wind") in ("oak_tree", "pine_tree"), "plural trees -> tree classes present")
    expect(bank.match_label("The vast blue ocean with waves") == "sea", "ocean->sea")
    expect(bank.match_label("Bright warm sunlight") is None, "no image class -> None")
    expect("tree" in _candidate_words("trees"), "singularization")

    # frames: real when matched, augmented, right shape; None otherwise
    fr = bank.get_frame("A friendly dog with soft fur")
    expect(fr is not None and len(fr[0]) == 32 * 32 * 3 and fr[1:4] == (32, 32, 3), "frame shape")
    fr2 = bank.get_frame("A friendly dog with soft fur")
    expect(fr2[0] != fr[0], "two presentations differ (augmentation)")
    expect(bank.get_frame("sunlight") is None and bank.last_hit is False, "fallback None")
    expect(bank.hits == 2 and bank.misses == 1, f"stats hits/misses {bank.hits}/{bank.misses}")

    # augmentation keeps uint8 range and content roughly (not all-zero)
    arr = np.frombuffer(fr[0], dtype=np.uint8)
    expect(arr.max() > 0 and arr.min() >= 0, "augmented pixels in range")

    # held-out
    ho = bank.get_heldout_frame("dog", index=7)
    expect(ho is not None and ho[4] == "dog", "held-out by label")
    expect(bank.get_heldout_frame("ocean") is not None, "held-out via synonym")
    expect(bank.get_heldout_frame("cat") is None, "no held-out for cat in this cache")

    # speech: 512-sample window, real word, None when absent
    sp = bank.get_speech("The dog is happy", n_samples=512)
    expect(sp is not None and len(sp[0]) == 512 and sp[1] == "dog", "speech window")
    expect(max(abs(x) for x in sp[0]) > 0.1, "speech window is the loud part")
    expect(bank.get_speech("silent night") is None, "no speech word -> None")
    print(bank.stats_str())

# synonym table sanity: every value label is a plausible CIFAR name
for k, v in SYNONYMS.items():
    expect(all(isinstance(x, str) and x for x in v), f"synonym {k}")

if failures:
    print(f"test_real_sensory: {failures} failure(s)")
    sys.exit(1)
print("test_real_sensory: all passed")
