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
from real_sensory import RealSensoryBank, SYNONYMS, _candidate_words, read_allowlist_levels  # noqa: E402

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
    seqs = [{"item": "film1", "title": "Film One", "description": "a moving scene from the film 'Film One'",
             "start_s": 0.0, "frames": [bytes([i]) * (32 * 32 * 3) for i in range(8)]} for _ in range(3)]
    # maturity fixtures: film2 level 1 (its pkl field says 0; the allowlist wins),
    # film3 level 2, film4 unlisted, film5 listed without a level
    def extra(item, n, **kw):
        return [dict({"item": item, "title": item, "description": "a moving scene", "start_s": float(k),
                      "frames": [bytes([k]) * (32 * 32 * 3)] * 8}, **kw) for k in range(n)]
    seqs += extra("film2", 2, maturity=0) + extra("film3", 1) + extra("film4", 1, maturity=0) + extra("film5", 1)
    with open(os.path.join(d, "video_sequences.pkl"), "wb") as f:
        pickle.dump(seqs, f)
    with open(os.path.join(d, "allowlist.txt"), "w") as f:
        f.write("# test list\nfilm1 0  # one\nfilm2 1\nfilm3   2 # three\nfilm5\nbadlvl x\nhigh 7\n")


with tempfile.TemporaryDirectory() as d:
    make_cache(d)
    bank = RealSensoryBank(d, seed=1)
    bank.ALLOWLIST_PATH = os.path.join(d, "allowlist.txt")
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

    # held-out split: disjoint, covers everything, stable
    expect(len(bank.sequences) + len(bank.heldout_sequences) == 3, 'split covers all sequences')
    expect(not ({s['item'] for s in bank.sequences} & {s['item'] for s in bank.heldout_sequences}), 'films disjoint')
    import zlib
    expect(all(zlib.crc32(s['item'].encode()) % 20 == 0 for s in bank.heldout_sequences), 'held-out rule (fallback when no named films present)')

    # motion sequences: queue plays frames in order through pop_frame
    seq = bank.get_sequence()
    expect(seq is not None and len(seq["frames"]) == 8 and "moving scene" in seq["description"], "sequence")
    bank.queue_frames(seq["frames"], label=bank.sequence_label(seq))
    popped = [bank.pop_frame() for _ in range(9)]
    expect(all(p is not None for p in popped[:8]) and popped[8] is None, "queue drains in order then None")
    expect(popped[0][0] == seq["frames"][0] and popped[7][0] == seq["frames"][7] and popped[0][4] == "a scene from the film Film One",
           "frames come out in order with the video label")
    expect("motion sequences: 1 (3 available)" in bank.stats_str(), f"stats mention sequences: {bank.stats_str()}")
    print(bank.stats_str())

    # maturity: allowlist levels, stage caps, unlisted/untagged films
    lv = read_allowlist_levels(os.path.join(d, "allowlist.txt"))
    expect(lv == {"film1": 0, "film2": 1, "film3": 2, "film5": 2, "badlvl": 2, "high": 2},
           f"allowlist levels parsed (untagged/bad/out-of-range -> 2): {lv}")
    expect(read_allowlist_levels(os.path.join(d, "missing.txt")) == {}, "missing allowlist -> {}")

    def films(b):
        return {q["item"] for q in b.sequences} | {q["item"] for q in b.heldout_sequences}
    expect(bank.max_maturity == 0 and films(bank) == {"film1"}, f"stage 0 plays level 0 only: {films(bank)}")
    expect(bank.set_stage(4) == 1 and films(bank) == {"film1", "film2"}, f"stage 4 adds level 1: {films(bank)}")
    expect(bank.set_stage(6) == 1 and "film3" not in films(bank), "level 2 stays locked at stage 6")
    expect("film4" not in films(bank) and "film5" not in films(bank), "unlisted and untagged films never play here")
    expect(all(q["maturity"] <= 1 for q in bank.sequences), "every playable sequence within the cap")
    expect(bank.set_stage(0) == 0 and films(bank) == {"film1"}, "cap tightens again on a lower stage")
    expect(bank.set_stage(99) == max(RealSensoryBank.STAGE_MAX_MATURITY.values()), "unknown stage -> highest defined cap")

    # ---- visual acuity schedule ------------------------------------------
    import io
    from PIL import Image

    def jpeg(color, side=128):
        buf = io.BytesIO()
        Image.new("RGB", (side, side), color).save(buf, format="JPEG", quality=95)
        return buf.getvalue()

    with open(os.path.join(d, "photos_hires_by_label.pkl"), "wb") as f:
        pickle.dump({"dog": [jpeg((250, 0, 0))], "giraffe": [jpeg((0, 250, 0))]}, f)
    with open(os.path.join(d, "photos_hires_heldout_by_label.pkl"), "wb") as f:
        pickle.dump({"dog": [jpeg((0, 0, 250))]}, f)
    hires = [{"item": "film1", "title": "Film One", "description": "a moving scene", "start_s": 0.0,
              "fps": 4.0, "size": 128, "encoding": "jpeg",
              "frames": [jpeg((i * 15, 0, 0)) for i in range(16)]}]
    with open(os.path.join(d, "video_sequences_hires.pkl"), "wb") as f:
        pickle.dump(hires, f)

    ac = RealSensoryBank(d, seed=3, augment=False)
    ac.ALLOWLIST_PATH = os.path.join(d, "allowlist.txt")
    expect((ac.resolution, ac.video_fps) == (32, 2.0), "stage 0 acuity is 32x32 @ 2 fps")
    f0 = ac.get_frame("a dog")
    expect(f0 is not None and f0[1:4] == (32, 32, 3) and len(f0[0]) == 32 * 32 * 3, "stage 0 photo is CIFAR 32x32")
    expect(ac.get_frame("a giraffe") is None, "hires-only labels unused at stage 0")
    s0 = ac.get_sequence()
    expect(s0 is not None and len(s0["frames"]) == 8 and s0.get("fps", 2.0) == 2.0, "stage 0 plays the 32x32 cache")
    ac.queue_frames(s0["frames"][:1])
    expect(ac.pop_frame()[0] == s0["frames"][0], "stage 0 frames pass through byte-identical")

    ac.set_stage(1)
    expect((ac.resolution, ac.video_fps) == (64, 4.0), f"stage 1 acuity: {ac.resolution} @ {ac.video_fps}")
    f1 = ac.get_frame("a dog")
    px = np.frombuffer(f1[0], dtype=np.uint8).reshape(64, 64, 3)
    expect(f1[1:4] == (64, 64, 3) and px[..., 0].mean() > 200 and px[..., 1].mean() < 30,
           "stage 1 prefers the sharp photo, resized to 64x64")
    expect(ac.get_frame("a giraffe") is not None, "hires-only label reachable from stage 1")
    s1 = ac.get_sequence()
    expect(s1 is not None and len(s1["frames"]) == 16 and s1["frames"][0][:2] == b"\xff\xd8",
           "stage 1 plays the high-resolution cache (16 frames @ 4 fps)")
    ac.queue_frames(s1["frames"][:2], label="x")
    p1 = ac.pop_frame()
    expect(p1[1:4] == (64, 64, 3) and len(p1[0]) == 64 * 64 * 3, "queued JPEG frame decoded to 64x64")
    ho = ac.get_heldout_frame("dog", index=0)
    hpx = np.frombuffer(ho[0], dtype=np.uint8).reshape(64, 64, 3)
    expect(ho[1] == 64 and hpx[..., 2].mean() > 200, "held-out prefers the sharp held-out photo")
    ho_c = ac.get_heldout_frame("sea", index=0)
    expect(ho_c is not None and ho_c[1:4] == (64, 64, 3), "CIFAR-only held-out label upscaled to 64x64")
    expect("acuity 64x64 @ 4 fps" in ac.stats_str(), f"stats show acuity: {ac.stats_str()}")

    ac.set_stage(3)
    expect(ac.resolution == 128, "stage 3 acuity is 128x128")
    ac.queue_frames(s1["frames"][:1])
    expect(len(ac.pop_frame()[0]) == 128 * 128 * 3, "stage 3 frames at 128x128")
    expect(len(ac.to_stage_frame(s0["frames"][0])) == 128 * 128 * 3, "32x32 raw frame upscaled at stage 3")

    # frame rate: a 4 fps shot played at 2 fps keeps every other frame
    ac.video_fps = 2.0
    v = ac._fps_view(hires[0])
    expect(len(v["frames"]) == 8 and v["frames"][1] == hires[0]["frames"][2] and v["fps"] == 2.0,
           "4 fps source played at 2 fps drops alternate frames")
    expect(len(hires[0]["frames"]) == 16, "fps view does not modify the cached shot")
    ac.video_fps = 4.0

    big = ac.augment_image(ac.to_stage_frame(jpeg((90, 90, 90))), 128, 128, 3)
    expect(len(big) == 128 * 128 * 3, "augmentation keeps the frame size at 128x128")

    ac.set_stage(0)
    s0b = ac.get_sequence()
    expect(ac.resolution == 32 and s0b is not None and len(s0b["frames"]) == 8,
           "returning to stage 0 restores 32x32 and the 32x32 cache")

    # without an allowlist the pkl field is used; a missing field is the most restrictive level
    fb = RealSensoryBank(d, seed=1)
    fb.ALLOWLIST_PATH = os.path.join(d, "missing.txt")
    expect(films(fb) == {"film2", "film4"}, f"fallback to pkl maturity at stage 0: {films(fb)}")

# Kubric synthetic physics clips (2026-11-05): own pool, thinned to the stage
# frame rate, drawn with probability synthetic_weight, never held out.
import io  # noqa: E402
from PIL import Image  # noqa: E402


def jpeg(v, side=128):
    buf = io.BytesIO()
    Image.fromarray(np.full((side, side, 3), v, np.uint8)).save(buf, format="JPEG")
    return buf.getvalue()


with tempfile.TemporaryDirectory() as d:
    make_cache(d)
    kub = [{"item": f"kubric:movi_c:{k}", "title": "Kubric MOVi-C",
            "description": "a shoe and a cup tumbling and rolling on the ground",
            "frames": [jpeg(10 * f) for f in range(24)], "fps": 12.0, "size": 128,
            "encoding": "jpeg", "source": "kubric", "maturity": 0} for k in range(2)]
    with open(os.path.join(d, "kubric_sequences.pkl"), "wb") as f:
        pickle.dump(kub, f)
    kb = RealSensoryBank(d, seed=3)
    kb.ALLOWLIST_PATH = os.path.join(d, "allowlist.txt")
    syn = kb.synthetic_sequences
    expect(len(syn) == 2 and all(len(q["frames"]) == 4 for q in syn),
           f"stage 0 (2 fps) thins 24 frames at 12 fps to 4: {[len(q['frames']) for q in syn]}")
    expect(len(kb.to_stage_frame(syn[0]["frames"][0])) == 32 * 32 * 3, "kubric JPEG frames resized to 32x32")
    expect(all(not str(q.get("item")).startswith("kubric:") for q in kb.heldout_sequences),
           "kubric clips are never held out")
    kb.synthetic_weight = 1.0
    picks = [kb.get_sequence() for _ in range(20)]
    expect(all(q.get("source") == "kubric" for q in picks), "weight 1.0 always draws kubric")
    expect(kb.synthetic_hits == 20, f"synthetic hits counted ({kb.synthetic_hits})")
    expect(RealSensoryBank.sequence_label(picks[0]) == picks[0]["description"],
           "a kubric clip's visual label is its description, not a film title")
    kb.synthetic_weight = 0.0
    expect(all(kb.get_sequence().get("source") != "kubric" for _ in range(20)),
           "weight 0.0 never draws kubric")
    kb.set_stage(3)                    # 4 fps
    expect(all(len(q["frames"]) == 8 for q in kb.synthetic_sequences),
           "stage 3 (4 fps) re-thins to 8 frames")
    expect("kubric" in kb.stats_str(), "stats mention kubric")

# Kubric builder: TFRecord + tf.train.Example parsing without TensorFlow
from build_kubric_cache import tfrecords, parse_example, caption  # noqa: E402


def _vint(x):
    out = bytearray()
    while True:
        b = x & 0x7F
        x >>= 7
        out.append(b | (0x80 if x else 0))
        if not x:
            return bytes(out)


def _ld(fn, payload):
    return _vint((fn << 3) | 2) + _vint(len(payload)) + payload


def _example(feats):
    entries = b""
    for key, (kind, vals) in feats.items():
        if kind == "bytes":
            lst = b"".join(_ld(1, v) for v in vals)
            feature = _ld(1, lst)
        else:
            lst = _ld(1, b"".join(_vint(v) for v in vals))     # packed int64
            feature = _ld(3, lst)
        entries += _ld(1, _ld(1, key.encode()) + _ld(2, feature))
    return _ld(1, entries)


ex = _example({"video": ("bytes", [b"png0", b"png1", b"png2"]),
               "instances/category": ("int", [14, 1, 14, 300]),
               "metadata/video_name": ("bytes", [b"42"]),
               "depth": ("bytes", [b"ignored"])})
rec = bytes(8) + bytes(4) + ex + bytes(4)
rec = len(ex).to_bytes(8, "little") + bytes(4) + ex + bytes(4)
parsed = list(tfrecords(rec + rec))
expect(len(parsed) == 2, f"two records read ({len(parsed)})")
got = parse_example(parsed[0], {"video", "instances/category", "metadata/video_name"})
expect(got.get("video") == [b"png0", b"png1", b"png2"], f"video frames parsed: {got.get('video')}")
expect(got.get("instances/category") == [14, 1, 14, 300], f"categories parsed: {got.get('instances/category')}")
expect("depth" not in got, "unwanted keys skipped")
labels = ["Action Figures", "Bag", "Board Games", "Bottles and Cans and Cups", "Camera", "Car Seat",
          "Consumer Goods", "Hat", "Headphones", "Keyboard", "Legos", "Media Cases", "Mouse", "None",
          "Shoe", "Stuffed Toys", "Toys"]
expect(caption([14, 1, 14, 300], labels) == "a shoe, a bag and an object tumbling and rolling on the ground",
       f"caption: {caption([14, 1, 14, 300], labels)}")
expect(caption([], labels).startswith("some objects"), "empty categories caption")

# builder chunking: cuts split sequences, static runs are dropped
from build_video_cache import chunk_frames  # noqa: E402
rng = np.random.RandomState(1)
moving = [np.clip(np.full((32, 32, 3), 100, np.uint8).astype(np.int16) + rng.randint(-6, 7, (32, 32, 3)), 0, 255).astype(np.uint8)
          for _ in range(16)]
static = [np.full((32, 32, 3), 50, np.uint8) for _ in range(16)]
cut = [np.full((32, 32, 3), 250, np.uint8).astype(np.int16) + rng.randint(-6, 7, (32, 32, 3)) for _ in range(8)]
cut = [np.clip(c, 0, 255).astype(np.uint8) for c in cut]
seqs = chunk_frames(moving + static + cut, seq_len=8)
expect(len(seqs) >= 2, f"moving + cut segment yield sequences ({len(seqs)})")
expect(all(len(s) == 8 and all(len(f) == 32 * 32 * 3 for f in s) for s in seqs), "sequence shape")
expect(len(chunk_frames(static, seq_len=8)) == 0, "static run dropped")
expect(len(chunk_frames(moving[:5], seq_len=8)) == 0, "too short -> none")

# synonym table sanity: every value label is a plausible CIFAR name
for k, v in SYNONYMS.items():
    expect(all(isinstance(x, str) and x for x in v), f"synonym {k}")

if failures:
    print(f"test_real_sensory: {failures} failure(s)")
    sys.exit(1)
print("test_real_sensory: all passed")
