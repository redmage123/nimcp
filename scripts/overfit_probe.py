#!/usr/bin/env python3
"""Overfit probe: can the brain memorise a handful of fixed items at all?

WHY (2026-11-10): ~120k steps of stage-0/1 training left held-out naming
cosine flat at 0.02-0.04 and visual recognition at 0/8. Before more
curriculum work we need to know whether the learning pathway
(brain.learn_vector -> brain.decide_full) can move outputs toward targets.

WHAT: a fresh small brain is trained repeatedly on N fixed (input, target)
pairs. Inputs and targets are seeded random unit vectors shaped like the
trainer's (1024-d input; 1024-d target tiled to the output width, as
make_semantic_target does). After every epoch every item is run through
decide_full and scored:
  own_cos     cosine(output, own target)           -> should rise toward 1
  retrieval   own target is the nearest of the N   -> should reach N/N
  other_cos   mean cosine(output, other targets)   -> should stay ~0
  out_xsim    mean pairwise cosine between outputs -> high = collapse
  held_cos    cosine(output, target) on M never-trained items (control)
A noise floor (two decide_full calls on the same input) is measured first.

VERDICT: LEARNS if retrieval reaches N/N and own_cos clears 0.5 while
held_cos stays near 0; otherwise the pathway does not memorise and the
fault is upstream of curriculum/generalisation.

USAGE:
    python3 scripts/overfit_probe.py                      # defaults
    python3 scripts/overfit_probe.py --epochs 60 --items 10 --out /tmp/o.jsonl
Run from a directory other than src/bindings/python (stale .so there).
"""

import argparse
import json
import os
import sys
import time

import numpy as np


def unit(v):
    n = float(np.linalg.norm(v))
    return v / n if n > 0 else v


def cos(a, b):
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    m = min(len(a), len(b))
    a, b = a[:m], b[:m]
    d = float(np.linalg.norm(a) * np.linalg.norm(b))
    return float(a @ b / d) if d > 0 else 0.0


def make_items(rng, n, in_dim, out_dim, emb_dim):
    items = []
    for _ in range(n):
        x = unit(rng.standard_normal(in_dim)).astype(np.float32)
        e = unit(rng.standard_normal(emb_dim)).astype(np.float32)
        reps = (out_dim + emb_dim - 1) // emb_dim
        t = np.tile(e, reps)[:out_dim].astype(np.float32)
        items.append((x, t))
    return items


def output_of(brain, x):
    r = brain.decide_full(x.tolist())
    v = r.get("output_vector") if isinstance(r, dict) else None
    return None if v is None else np.asarray(v, dtype=np.float32)


def evaluate(brain, train, held):
    outs = [output_of(brain, x) for x, _ in train]
    if any(o is None for o in outs):
        return {"error": "decide_full returned no output_vector"}
    targets = [t for _, t in train]
    own, other, hits = [], [], 0
    for i, o in enumerate(outs):
        sims = [cos(o, t) for t in targets]
        own.append(sims[i])
        other.extend(s for j, s in enumerate(sims) if j != i)
        hits += int(int(np.argmax(sims)) == i)
    xs = [cos(outs[i], outs[j]) for i in range(len(outs)) for j in range(i + 1, len(outs))]
    held_c = []
    for x, t in held:
        o = output_of(brain, x)
        if o is not None:
            held_c.append(cos(o, t))
    return {
        "own_cos": float(np.mean(own)),
        "own_cos_min": float(np.min(own)),
        "retrieval": hits,
        "other_cos": float(np.mean(other)) if other else 0.0,
        "out_xsim": float(np.mean(xs)) if xs else 0.0,
        "held_cos": float(np.mean(held_c)) if held_c else 0.0,
        "out_norm": float(np.mean([np.linalg.norm(o) for o in outs])),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--items", type=int, default=10)
    ap.add_argument("--heldout", type=int, default=10)
    ap.add_argument("--epochs", type=int, default=40)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--num-inputs", type=int, default=1024)
    ap.add_argument("--num-outputs", type=int, default=4096)
    ap.add_argument("--embed-dim", type=int, default=1024)
    ap.add_argument("--ann", type=int, default=20000, help="ANN neuron count")
    ap.add_argument("--snn", type=int, default=50000, help="SNN neuron count")
    ap.add_argument("--lnn", type=int, default=128, help="LNN neuron count")
    ap.add_argument("--init-mode", default="full", choices=["full", "fast", "minimal"])
    ap.add_argument("--setup", default="trainer", choices=["trainer", "trainer-no-utm", "task", "none"],
                    help="trainer = immerse_athena's post-creation config (needs >45 GB: "
                         "enable_multi_network builds the full-size SNN); trainer-no-utm = "
                         "same minus enable_multi_network; task = only "
                         "set_task_type('regression'); none = constructor defaults")
    ap.add_argument("--confidence", type=float, default=0.8)
    ap.add_argument("--learning-rate", type=float, default=None)
    ap.add_argument("--out", default="overfit_probe.jsonl")
    ap.add_argument("--trace", default=None,
                    help="write a NIMCP_DECIDE_TRACE stage-by-stage dump here (plus <trace>.items.npz "
                         "for scripts/decide_trace_report.py)")
    args = ap.parse_args()

    if args.trace:
        if os.path.exists(args.trace):
            os.remove(args.trace)
        os.environ["NIMCP_DECIDE_TRACE"] = args.trace
    import nimcp

    rng = np.random.default_rng(args.seed)
    train = make_items(rng, args.items, args.num_inputs, args.num_outputs, args.embed_dim)
    held = make_items(rng, args.heldout, args.num_inputs, args.num_outputs, args.embed_dim)
    if args.trace:
        np.savez(args.trace + ".items.npz",
                 train_x=np.stack([x for x, _ in train]), train_t=np.stack([t for _, t in train]),
                 held_x=np.stack([x for x, _ in held]), held_t=np.stack([t for _, t in held]))

    t0 = time.time()
    brain = nimcp.Brain("overfit", num_inputs=args.num_inputs, num_outputs=args.num_outputs,
                        neuron_count=args.ann, init_mode=args.init_mode,
                        snn_neuron_count=args.snn, lnn_neuron_count=args.lnn)
    print(f"[overfit] brain created in {time.time() - t0:.0f}s "
          f"(ann={args.ann} snn={args.snn} lnn={args.lnn} mode={args.init_mode})", flush=True)
    # Mirror immerse_athena.py's post-creation config (non-daemon branch), in
    # order. The Python Brain defaults to CLASSIFICATION (softmax output,
    # cross-entropy on positive target dims only); the trainer switches to
    # regression. --setup none keeps the bare constructor defaults.
    steps = []
    if args.setup in ("task", "trainer", "trainer-no-utm"):
        steps.append(("set_task_type", ("regression",)))
    if args.setup in ("trainer", "trainer-no-utm"):
        steps = [("set_fast_training", (False,))] + steps + [
            ("enable_mixed_precision", (True,)),
            ("enable_gradient_checkpointing", (False, 0)),
            ("enable_biological_plasticity", (True,)),
            ("enable_world_model", (True,)),
            ("lnn_create", (128, 64, 32, 64)),
        ]
        # enable_multi_network builds the full production-size hierarchical
        # SNN (~1.8M neurons) regardless of snn_neuron_count: >45 GB RSS.
        if args.setup == "trainer":
            steps.append(("enable_multi_network", ()))
    for m, a in steps:
        try:
            getattr(brain, m)(*a)
            print(f"[overfit] setup {m}{a}: ok", flush=True)
        except Exception as e:
            print(f"[overfit] setup {m}{a}: FAILED ({e})", flush=True)
    # Same as the trainer: the UTM early-stop latch silently stops training.
    for m, a in (("utm_reset_early_stopping", ()), ("utm_set_early_stopping_enabled", (False,))):
        try:
            getattr(brain, m)(*a)
        except Exception as e:
            print(f"[overfit] {m} unavailable: {e}", flush=True)

    x0 = train[0][0]
    o1, o2 = output_of(brain, x0), output_of(brain, x0)
    noise = cos(o1, o2) if o1 is not None and o2 is not None else float("nan")
    print(f"[overfit] noise floor: cos(decide_full(x), decide_full(x)) = {noise:.4f}", flush=True)

    out = open(args.out, "w")

    def log(rec):
        out.write(json.dumps(rec) + "\n")
        out.flush()

    log({"kind": "config", **vars(args), "noise_floor": noise})
    base = evaluate(brain, train, held)
    base.update({"kind": "eval", "epoch": 0, "train_loss": None})
    log(base)
    hdr = (f"{'ep':>4} {'loss':>9} {'own_cos':>8} {'min':>7} {'retr':>6} "
           f"{'other':>7} {'out_xsim':>8} {'held':>7} {'|out|':>8} {'s/ep':>6}")
    print(hdr, flush=True)

    def row(r, dt):
        loss = "-" if r.get("train_loss") is None else f"{r['train_loss']:.4f}"
        print(f"{r['epoch']:>4} {loss:>9} {r['own_cos']:>8.4f} {r['own_cos_min']:>7.3f} "
              f"{r['retrieval']:>3}/{args.items:<2} {r['other_cos']:>7.3f} {r['out_xsim']:>8.3f} "
              f"{r['held_cos']:>7.3f} {r['out_norm']:>8.2f} {dt:>6.1f}", flush=True)

    if "error" in base:
        print(f"[overfit] {base['error']}", flush=True)
        return 2
    row(base, 0.0)

    last = base
    for ep in range(1, args.epochs + 1):
        t1 = time.time()
        losses = []
        for i in rng.permutation(len(train)):
            x, t = train[i]
            kw = {"label": f"overfit_{i}", "confidence": args.confidence}
            if args.learning_rate is not None:
                kw["learning_rate"] = args.learning_rate
            try:
                losses.append(float(brain.learn_vector(x.tolist(), t.tolist(), **kw)))
            except Exception as e:
                print(f"[overfit] learn_vector failed on item {i}: {e}", flush=True)
        r = evaluate(brain, train, held)
        if "error" in r:
            print(f"[overfit] epoch {ep}: {r['error']}", flush=True)
            return 2
        r.update({"kind": "eval", "epoch": ep,
                  "train_loss": float(np.mean(losses)) if losses else None})
        log(r)
        row(r, time.time() - t1)
        last = r

    learns = (last["retrieval"] == args.items and last["own_cos"] > 0.5
              and abs(last["held_cos"]) < 0.2)
    moved = last["own_cos"] - base["own_cos"]
    verdict = ("LEARNS: memorised every item" if learns else
               "PARTIAL: outputs moved toward targets but did not memorise" if moved > 0.1 else
               "NO LEARNING: outputs did not move toward trained targets")
    print(f"[overfit] own_cos {base['own_cos']:.4f} -> {last['own_cos']:.4f} "
          f"(delta {moved:+.4f}), retrieval {base['retrieval']} -> {last['retrieval']}/{args.items}, "
          f"held_cos {last['held_cos']:+.4f}", flush=True)
    print(f"[overfit] VERDICT: {verdict}", flush=True)
    log({"kind": "verdict", "verdict": verdict, "own_cos_delta": moved})
    return 0 if learns else 1


if __name__ == "__main__":
    sys.exit(main())
