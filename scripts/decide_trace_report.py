#!/usr/bin/env python3
"""Summarise a NIMCP_DECIDE_TRACE dump written by overfit_probe.py --trace.

WHY (2026-11-11): learn_vector does not move decide_full outputs toward the
trained targets. This report shows, for each brain_decide stage, how close the
output is to the item's target and how input-dependent it is, so the stage
that loses (or never had) the learned signal is visible. It also shows the ANN
prediction just before and after each learn_vector update.

Record format: see include/core/brain/nimcp_brain_decide_trace.h.

USAGE:
    python3 scripts/decide_trace_report.py TRACE [--epochs 0,1,5]
Each eval pass is labelled by the number of learn_vector calls before it
divided by the item count, i.e. the epoch it follows.
"""

import argparse
import struct
import sys
from collections import OrderedDict, defaultdict

import numpy as np

MAGIC = 0x54524344


def read_trace(path):
    calls = OrderedDict()   # seq -> OrderedDict(stage -> vector)
    with open(path, "rb") as f:
        data = f.read()
    off, n_data = 0, len(data)
    while off + 48 <= n_data:
        magic, seq = struct.unpack_from("<IQ", data, off)
        if magic != MAGIC:
            raise SystemExit(f"bad magic at offset {off}")
        name = data[off + 12:off + 44].split(b"\0", 1)[0].decode()
        (n,) = struct.unpack_from("<I", data, off + 44)
        v = np.frombuffer(data, dtype="<f4", count=n, offset=off + 48)
        off += 48 + 4 * n
        calls.setdefault(seq, OrderedDict())
        # a stage can repeat inside one call (e.g. perform_forward_pass re-run)
        key, k = name, 2
        while key in calls[seq]:
            key, k = f"{name}#{k}", k + 1
        calls[seq][key] = v.astype(np.float64)
    return calls


def cos(a, b):
    m = min(len(a), len(b))
    a, b = a[:m], b[:m]
    d = np.linalg.norm(a) * np.linalg.norm(b)
    return float(a @ b / d) if d > 0 else 0.0


def match(x, items):
    """Index of the item whose input equals x (cos > 0.999), else None."""
    for i, it in enumerate(items):
        if cos(x, it) > 0.999:
            return i
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("trace")
    ap.add_argument("--epochs", default=None, help="comma list of epochs to show (default all)")
    args = ap.parse_args()

    items = np.load(args.trace + ".items.npz")
    tx, tt, hx, ht = items["train_x"], items["train_t"], items["held_x"], items["held_t"]
    n_items = len(tx)
    calls = read_trace(args.trace)

    n_learn = 0
    evals = defaultdict(list)       # epoch -> [(kind, idx, stages)]
    learns = defaultdict(list)      # epoch(1-based) -> [stages]
    for seq, st in calls.items():
        if "learn_target" in st:
            n_learn += 1
            learns[(n_learn - 1) // n_items + 1].append(st)
            continue
        x = st.get("in_features")
        if x is None:
            continue
        i = match(x, tx)
        kind = "train"
        if i is None:
            i, kind = match(x, hx), "held"
        if i is None:
            continue
        evals[n_learn // n_items].append((kind, i, st))

    show = None if args.epochs is None else {int(e) for e in args.epochs.split(",")}

    for ep in sorted(set(evals) | set(learns)):
        if show is not None and ep not in show:
            continue
        if ep in learns:
            ls = learns[ep]
            pre = [cos(s["learn_ann_pre"], s["learn_target"]) for s in ls if "learn_ann_pre" in s]
            post = [cos(s["learn_ann_post"], s["learn_target"]) for s in ls if "learn_ann_post" in s]
            dn = [np.linalg.norm(s["learn_ann_post"] - s["learn_ann_pre"]) for s in ls
                  if "learn_ann_post" in s]
            meta = np.array([s["learn_ann_loss_lr_frozen"] for s in ls
                             if "learn_ann_loss_lr_frozen" in s])
            pn = [np.linalg.norm(s["learn_ann_pre"]) for s in ls if "learn_ann_pre" in s]
            inp = [cos(s["learn_input"], tx[match(s["learn_input"], tx)])
                   if match(s["learn_input"], tx) is not None else float("nan") for s in ls]
            print(f"\n=== epoch {ep} learn_vector x{len(ls)} ===")
            if pre:
                print(f"  ANN cos(pred,target) pre {np.mean(pre):+.4f}  post {np.mean(post):+.4f}  "
                      f"|pred| {np.mean(pn):.3f}  |Δpred| per update {np.mean(dn):.4f}")
            if len(meta):
                print(f"  ann loss {meta[:, 0].mean():.4f}  lr {meta[:, 1].mean():.2e}  "
                      f"frozen {int(meta[:, 2].sum())}/{len(meta)}  "
                      f"input matches raw item: {sum(1 for c in inp if c > 0.999)}/{len(inp)}")
        if ep not in evals:
            continue
        ev = [(k, i, s) for k, i, s in evals[ep] if k == "train"]
        hv = [(k, i, s) for k, i, s in evals[ep] if k == "held"]
        stages = []
        for _, _, s in ev:
            for name in s:
                if name not in stages and name != "in_features" and name != "pf0_fwd_features":
                    stages.append(name)
        print(f"\n=== epoch {ep} eval: {len(ev)} trained + {len(hv)} held-out decide calls ===")
        print(f"  {'stage':<28}{'calls':>6}{'own_cos':>9}{'retr':>6}{'xsim':>7}{'held':>8}"
              f"{'|out|':>10}{'Δ prev':>9}")
        prev = {}
        for name in stages:
            rows = [(i, s[name]) for _, i, s in ev if name in s]
            if not rows:
                continue
            own = [cos(v, tt[i]) for i, v in rows]
            hits = sum(int(np.argmax([cos(v, t) for t in tt]) == i) for i, v in rows)
            vs = [v for _, v in rows]
            xs = [cos(vs[a], vs[b]) for a in range(len(vs)) for b in range(a + 1, len(vs))]
            hrows = [(i, s[name]) for _, i, s in hv if name in s]
            held = [cos(v, ht[i]) for i, v in hrows]
            # change from the previous stage of the same call (relative L2)
            dl = []
            for i, v in rows:
                p = prev.get(i)
                if p is not None and len(p) == len(v) and np.linalg.norm(p) > 0:
                    dl.append(np.linalg.norm(v - p) / np.linalg.norm(p))
            for i, v in rows:
                prev[i] = v
            print(f"  {name:<28}{len(rows):>6}{np.mean(own):>+9.4f}{hits:>3}/{len(rows):<2}"
                  f"{(np.mean(xs) if xs else 0):>7.3f}{(np.mean(held) if held else 0):>+8.4f}"
                  f"{np.mean([np.linalg.norm(v) for v in vs]):>10.3f}"
                  f"{(np.mean(dl) if dl else float('nan')):>9.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
