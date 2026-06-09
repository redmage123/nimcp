#!/usr/bin/env python3
"""
One-shot comprehend trace — pod-side.

Loads the brain from the canonical checkpoint with NIMCP_COMPREHEND_TRACE=1
set in the environment, then calls comprehend() on a small set of failing
intents. The C-side instrumentation prints per-word + end-of-loop
diagnostics to stderr; we redirect those to a known file.

Run ON the pod, AFTER stopping athena-brain so we don't double-allocate:

    supervisorctl stop athena-brain
    NIMCP_COMPREHEND_TRACE=1 python3 /workspace/nimcp/scripts/comprehend_trace_oneshot.py
    supervisorctl start athena-brain

Output goes to /tmp/comprehend_trace.log (created fresh each run).
"""

from __future__ import annotations

import os
import sys
import time


def main():
    # Sanity: trace env must be set before nimcp is imported (so the
    # static cached env-read picks it up on first comprehend).
    if os.environ.get("NIMCP_COMPREHEND_TRACE", "") not in ("1", "true", "yes"):
        print("ERROR: must run with NIMCP_COMPREHEND_TRACE=1 set", file=sys.stderr)
        return 2

    CHECKPOINT = "/workspace/nimcp/checkpoints/athena/athena_immersive.bin"
    TRACE_OUT  = "/tmp/comprehend_trace.log"

    # Redirect stderr to the trace file so the C-side fprintf(stderr, ...)
    # lands in one capturable place.
    sys.stderr.flush()
    trace_f = open(TRACE_OUT, "w", buffering=1)  # line-buffered
    os.dup2(trace_f.fileno(), 2)
    print(f"[oneshot] stderr redirected to {TRACE_OUT}", file=sys.stderr)

    print(f"[oneshot] importing nimcp + loading checkpoint {CHECKPOINT}",
          file=sys.stderr, flush=True)
    t0 = time.time()
    import nimcp  # noqa: E402
    # The Brain ctor's `checkpoint=` kwarg lazy-loads via the fast path
    # without going through the heavy hemispheric init. init_mode=minimal
    # to skip non-essential subsystems.
    b = nimcp.Brain(
        "trace_oneshot",
        checkpoint=CHECKPOINT,
        init_mode="minimal",
        log_level="warn",
    )
    print(f"[oneshot] checkpoint loaded in {time.time()-t0:.1f}s", file=sys.stderr, flush=True)

    # Drive comprehend on a few diverse intents. The C-side trace prints
    # one block per call.
    INTENTS = [
        "cat",         # concrete noun — expect a real entry with bindings + context_vector
        "dog",         # ditto
        "love",        # abstract — works in pre-deploy probe
        "peace",       # abstract — returned ALL-ZERO semantic_vector pre-deploy
        "hope",        # abstract — returned ALL-ZERO semantic_vector pre-deploy
        "the cat ran", # multi-word — more signal?
    ]
    for s in INTENTS:
        # Echo the input to the trace stream as a separator so the per-word
        # blocks are easy to align.
        sys.stderr.write(f"\n[oneshot] === comprehend({s!r}) ===\n")
        sys.stderr.flush()
        try:
            r = b.comprehend(s)
        except Exception as e:
            sys.stderr.write(f"[oneshot] EXCEPTION: {e}\n")
            continue
        v = r.get("semantic_vector", [])
        c = r.get("confidence", 0.0)
        if isinstance(v, list) and v:
            norm = (sum(x * x for x in v)) ** 0.5
            nz = sum(1 for x in v if abs(x) > 1e-9)
            mx = max((abs(x) for x in v), default=0.0)
            sys.stderr.write(
                f"[oneshot] py-side: conf={c:.3f} dim={len(v)} nonzero={nz}/{len(v)} "
                f"||v||={norm:.3f} max|v|={mx:.3f}\n")
        else:
            sys.stderr.write(f"[oneshot] py-side: no semantic_vector returned\n")
        sys.stderr.flush()

    sys.stderr.write("[oneshot] done\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
