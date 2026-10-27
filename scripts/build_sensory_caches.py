#!/usr/bin/env python3
"""Build the real-sensory caches used by scripts/real_sensory.py.

Inputs (original archives, downloaded separately):
    cifar-100-python.tar.gz        https://www.cs.toronto.edu/~kriz/cifar-100-python.tar.gz
    cifar-10-python.tar.gz         https://www.cs.toronto.edu/~kriz/cifar-10-python.tar.gz
    speech_commands_v0.02.tar.gz   http://download.tensorflow.org/data/speech_commands_v0.02.tar.gz  (CC BY 4.0)

Outputs (in --out):
    cifar_by_label.pkl          train images, {label: [32x32x3 uint8 bytes]}  (CIFAR-100 fine + CIFAR-10)
    cifar_heldout_by_label.pkl  test images, same layout (evaluation only)
    cifar100_by_label.pkl       CIFAR-100-only train split in the layout
                                scripts/multimodal_data.py expects (so the
                                stage-1 real-image hook starts working too)
    speech_cmds_by_label.pkl    {word: [float32 16 kHz mono samples]}, up to
                                --clips-per-word clips per word

Usage:
    python3 build_sensory_caches.py --archives DIR --out DIR [--clips-per-word 150]
"""
import argparse
import io
import os
import pickle
import tarfile
import wave

import numpy as np


def _cifar_batches(tar_path, names):
    """Yield (data[N,3072] uint8 CHW, labels list) for the named members."""
    with tarfile.open(tar_path, "r:gz") as tf:
        members = {os.path.basename(m.name): m for m in tf.getmembers() if m.isfile()}
        meta = None
        for mname in ("meta", "batches.meta"):
            if mname in members:
                meta = pickle.load(tf.extractfile(members[mname]), encoding="latin1")
        for n in names:
            m = members.get(n)
            if m is None:
                continue
            d = pickle.load(tf.extractfile(m), encoding="latin1")
            yield d, meta


def _chw_to_hwc_bytes(row):
    return np.asarray(row, dtype=np.uint8).reshape(3, 32, 32).transpose(1, 2, 0).tobytes()


def build_cifar(archives, out):
    train, test, c100_train = {}, {}, {}
    # CIFAR-100
    p100 = os.path.join(archives, "cifar-100-python.tar.gz")
    for d, meta in _cifar_batches(p100, ["train", "test"]):
        names = meta["fine_label_names"]
        is_train = len(d["fine_labels"]) == 50000
        dst = train if is_train else test
        for row, li in zip(d["data"], d["fine_labels"]):
            dst.setdefault(names[li], []).append(_chw_to_hwc_bytes(row))
            if is_train:
                c100_train.setdefault(names[li], []).append(dst[names[li]][-1])
    # CIFAR-10
    p10 = os.path.join(archives, "cifar-10-python.tar.gz")
    batches = [f"data_batch_{i}" for i in range(1, 6)] + ["test_batch"]
    for d, meta in _cifar_batches(p10, batches):
        names = meta["label_names"]
        dst = test if d.get("batch_label", "").startswith("testing") else train
        for row, li in zip(d["data"], d["labels"]):
            dst.setdefault(names[li], []).append(_chw_to_hwc_bytes(row))
    for name, obj in (("cifar_by_label.pkl", train), ("cifar_heldout_by_label.pkl", test),
                      ("cifar100_by_label.pkl", c100_train)):
        with open(os.path.join(out, name), "wb") as f:
            pickle.dump(obj, f, protocol=4)
        print(f"{name}: {len(obj)} labels, {sum(len(v) for v in obj.values())} images")


def build_speech(archives, out, clips_per_word):
    p = os.path.join(archives, "speech_commands_v0.02.tar.gz")
    if not os.path.exists(p):
        print("speech_commands_v0.02.tar.gz not found — skipping speech cache")
        return
    by_word = {}
    with tarfile.open(p, "r:gz") as tf:
        for m in tf:
            if not m.isfile() or not m.name.endswith(".wav"):
                continue
            parts = m.name.split("/")
            if len(parts) < 2:
                continue
            word = parts[-2]
            if word.startswith("_"):  # _background_noise_
                continue
            lst = by_word.setdefault(word, [])
            if len(lst) >= clips_per_word:
                continue
            data = tf.extractfile(m).read()
            with wave.open(io.BytesIO(data)) as w:
                sr = w.getframerate()
                frames = w.readframes(w.getnframes())
            samples = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0
            if sr != 16000:
                n = int(len(samples) * 16000 / sr)
                samples = np.interp(np.linspace(0, len(samples) - 1, n),
                                    np.arange(len(samples)), samples).astype(np.float32)
            lst.append(samples.astype(np.float32).tobytes())  # numpy-version-proof
    with open(os.path.join(out, "speech_cmds_by_label.pkl"), "wb") as f:
        pickle.dump(by_word, f, protocol=4)
    print(f"speech_cmds_by_label.pkl: {len(by_word)} words, "
          f"{sum(len(v) for v in by_word.values())} clips: {sorted(by_word)}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--archives", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--clips-per-word", type=int, default=150)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    build_cifar(a.archives, a.out)
    build_speech(a.archives, a.out, a.clips_per_word)


if __name__ == "__main__":
    main()
