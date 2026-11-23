#!/usr/bin/env python3
"""Athena listens: stream continuous speech into the live brain's cochlea.

Reads LibriSpeech .tar.gz archives sequentially (from a URL, a local file or
stdin), decodes each utterance and sends it in 1 s chunks of 16 kHz float32
PCM to the brain daemon (feed_audio). In the brain: cochlea -> auditory
nerve -> Wernicke predictor, a spiking network that learns with e-prop to
predict the next 40 ms; every frame is predicted before it is learned from,
so get_hearing_stats' skill is an honest running test.

Nothing is stored: each audio file is decoded in memory and discarded.

    python3 scripts/athena_hear.py                 # train-clean-100, then 360
    python3 scripts/athena_hear.py --speed 2       # cap at 2x real time
    python3 scripts/athena_hear.py --source /data/dev-clean.tar.gz --loop

The feed runs outside the daemon's brain-wide lock (hearing has its own
mutex), so it does not stall the trainer; it costs about one CPU core.
"""
from __future__ import annotations

import argparse
import io
import os
import subprocess
import sys
import tarfile
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

SR = 16000
OPENSLR = "https://www.openslr.org/resources/12/{}.tar.gz"
DEFAULT_SOURCES = [OPENSLR.format("train-clean-100"), OPENSLR.format("train-clean-360")]


def open_source(src: str):
    """A readable binary stream for a URL, a path or '-' (stdin)."""
    if src == "-":
        return sys.stdin.buffer, None
    if src.startswith("http://") or src.startswith("https://"):
        proc = subprocess.Popen(["curl", "-sL", "--retry", "5", src], stdout=subprocess.PIPE)
        return proc.stdout, proc
    return open(src, "rb"), None


def utterances(fileobj):
    """Yield (name, float32 PCM) from a LibriSpeech .tar.gz read in order."""
    import soundfile as sf
    t = tarfile.open(fileobj=fileobj, mode="r|gz")
    for m in t:
        if not (m.isfile() and m.name.endswith(".flac")):
            continue
        try:
            data = t.extractfile(m).read()
        except (tarfile.ReadError, EOFError, OSError):
            return
        wav, sr = sf.read(io.BytesIO(data), dtype="float32")
        if sr != SR or wav.ndim != 1:
            continue
        yield os.path.basename(m.name), wav


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", action="append", help="URL, .tar.gz path or '-' (repeatable; default "
                    "LibriSpeech train-clean-100 then train-clean-360)")
    ap.add_argument("--loop", action="store_true", help="start again when the sources end")
    ap.add_argument("--speed", type=float, default=0.0,
                    help="cap at this multiple of real time (0 = as fast as the brain hears)")
    ap.add_argument("--chunk", type=float, default=1.0, help="seconds of audio per request")
    ap.add_argument("--report", type=float, default=600.0, help="seconds between progress lines")
    ap.add_argument("--max-hours", type=float, default=0.0, help="stop after this much speech (0 = no limit)")
    args = ap.parse_args()

    from brain_client import BrainProxy
    brain = BrainProxy()
    sources = args.source or DEFAULT_SOURCES
    chunk = max(1, int(args.chunk * SR))
    heard = 0                      # samples sent this run
    t0 = time.time()
    last = t0
    while True:
        for src in sources:
            print(f"[hear] listening to {src}", flush=True)
            stream, proc = open_source(src)
            try:
                for name, wav in utterances(stream):
                    for off in range(0, len(wav), chunk):
                        piece = wav[off:off + chunk]
                        for attempt in range(5):
                            try:
                                brain.feed_audio(piece)
                                break
                            except Exception as e:        # daemon restarting: wait, retry
                                print(f"[hear] feed failed ({e}); retry {attempt + 1}/5", flush=True)
                                time.sleep(30)
                        else:
                            print("[hear] daemon unreachable; stopping", flush=True)
                            return 1
                        heard += len(piece)
                        if args.speed > 0:
                            ahead = heard / SR / args.speed - (time.time() - t0)
                            if ahead > 0:
                                time.sleep(ahead)
                    now = time.time()
                    if now - last >= args.report:
                        last = now
                        s = brain.get_hearing_stats()
                        print(f"[hear] {heard / SR / 3600:.2f} h this run ({heard / SR / (now - t0):.1f}x real time); "
                              f"brain has heard {s.get('hours', 0):.2f} h, skill vs copy "
                              f"recent {s.get('skill_recent', 0):+.3f} total {s.get('skill_total', 0):+.3f}, "
                              f"{s.get('rate_hz', 0):.1f} Hz; last {name}", flush=True)
                    if args.max_hours and heard / SR / 3600 >= args.max_hours:
                        return 0
            finally:
                if proc is not None:
                    proc.kill()
        if not args.loop:
            return 0


if __name__ == "__main__":
    sys.exit(main())
