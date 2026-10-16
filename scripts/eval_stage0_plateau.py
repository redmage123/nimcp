#!/usr/bin/env python3
"""Stage 0 plateau analysis from live training log.

Rather than run inference-only predict() (which is ~130s per item due to
the 100ms SNN decode window), exploit the fact that Stage 0 cycles
through 1079 sensory items — each learn_vector call is effectively on an
unseen item early in the stage. Analyzing the loss curve tells us:

  1. Has the running-mean loss stopped decreasing?  (plateau)
  2. Is the variance shrinking?                     (stabilization)
  3. Are new lows still happening?                  (still learning)

USAGE (on RunPod):
    python3 /workspace/nimcp/scripts/eval_stage0_plateau.py
    python3 /workspace/nimcp/scripts/eval_stage0_plateau.py --window 500

DECISION:
  - Still-decreasing slope            → CONTINUE
  - Flat ≥ 200 steps, decreasing P95  → CONTINUE (tail still improving)
  - Flat mean + flat P95 ≥ 500 steps  → ADVANCE (nothing more to absorb)
"""

import argparse
import os
import re
import statistics


BRAIN_LOG = "/var/log/athena-brain.log"
TRAIN_LOG = "/var/log/athena-training.log"

# learn_vector step 37: total=3595ms (...) loss=11.4298
LV_RE = re.compile(r"learn_vector step (\d+):.*?loss=([0-9.]+)")

# Training log averaged snapshots:  [1200] loss=0.2849 (50/50 non-zero) ...
# Must anchor to start-of-line so the regex doesn't accidentally match
# the CNN/SNN loss fragments later in the same log line.
TL_RE = re.compile(r"^\s*\[(\d+)\] loss=([0-9.]+)", re.MULTILINE)


def parse_losses(path, regex):
    """Regex finds all matches of (step, loss) in the file. If the regex uses
    MULTILINE mode, the whole file is scanned at once so ^ anchors work. For
    regexes without ^, line-by-line scan is equivalent and cheaper on memory."""
    out = []
    if not os.path.exists(path):
        return out
    try:
        if regex.flags & re.MULTILINE:
            with open(path, "r", errors="replace") as f:
                text = f.read()
            for m in regex.finditer(text):
                out.append((int(m.group(1)), float(m.group(2))))
        else:
            with open(path, "r", errors="replace") as f:
                for line in f:
                    m = regex.search(line)
                    if m:
                        out.append((int(m.group(1)), float(m.group(2))))
    except Exception as e:
        print(f"[warn] Could not fully parse {path}: {e}")
    return out


def slice_current_session(losses):
    """Training logs persist across brain restarts; each restart resets the
    step counter from a high N back to a low one. Scan backwards to find the
    last counter-reset boundary and return only entries from that point on.
    A counter reset is defined as steps[i] < steps[i-1] (step went down)."""
    if len(losses) < 2:
        return losses
    last_reset = 0
    for i in range(1, len(losses)):
        # A fresh session typically restarts the counter below the previous
        # value. Use a generous heuristic: step dropped by at least 20.
        if losses[i][0] + 20 < losses[i - 1][0]:
            last_reset = i
    return losses[last_reset:]


def summarize(tag, losses, last_n):
    if not losses:
        print(f"[{tag}] no data")
        return None
    steps = [s for s, _ in losses]
    values = [v for _, v in losses]
    tail = values[-last_n:] if len(values) >= last_n else values
    tail_mean = statistics.mean(tail)
    tail_median = statistics.median(tail)
    tail_p95 = sorted(tail)[int(len(tail) * 0.95)] if len(tail) >= 2 else tail[0]
    tail_p05 = sorted(tail)[int(len(tail) * 0.05)] if len(tail) >= 2 else tail[0]
    tail_std = statistics.stdev(tail) if len(tail) >= 2 else 0.0
    min_so_far = min(values)
    min_idx = values.index(min_so_far)
    min_step = steps[min_idx]
    print(f"[{tag}] n={len(values)}  steps={steps[0]}..{steps[-1]}")
    print(f"  last {len(tail)} window:  mean={tail_mean:.4f}  "
          f"median={tail_median:.4f}  P05={tail_p05:.4f}  P95={tail_p95:.4f}  "
          f"std={tail_std:.4f}")
    print(f"  all-time min:  loss={min_so_far:.4f} at step {min_step}")
    return {
        "n": len(values),
        "tail_mean": tail_mean,
        "tail_p95": tail_p95,
        "tail_p05": tail_p05,
        "tail_std": tail_std,
        "min": min_so_far,
        "min_step": min_step,
        "last_step": steps[-1],
    }


def slope(losses, last_n):
    """Linear-regression slope over last N points, in loss-units per step."""
    if len(losses) < 10:
        return None
    tail = losses[-last_n:] if len(losses) >= last_n else losses
    n = len(tail)
    xs = [s for s, _ in tail]
    ys = [v for _, v in tail]
    mx = sum(xs) / n
    my = sum(ys) / n
    num = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    den = sum((x - mx) ** 2 for x in xs) or 1e-9
    return num / den


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--window", type=int, default=300,
                    help="Tail window for stats (default 300)")
    ap.add_argument("--decision_tail", type=int, default=10,
                    help="Number of most-recent snapshots used for final "
                         "decision (default 10). Small to avoid pre-fix "
                         "saturation-era outliers bleeding into aggregates.")
    ap.add_argument("--max_loss", type=float, default=None,
                    help="If set, drop any entry with loss > max_loss from "
                         "both series (handy for filtering pre-Option-C "
                         "saturation-era data).")
    args = ap.parse_args()

    def maybe_cap(series):
        if args.max_loss is None:
            return series
        return [(s, v) for s, v in series if v <= args.max_loss]

    print("=== Per-step learn_vector losses (raw, from brain log) ===")
    lv_all = parse_losses(BRAIN_LOG, LV_RE)
    lv_losses = maybe_cap(slice_current_session(lv_all))
    print(f"[raw] current session: {len(lv_losses)} of {len(lv_all)} entries")
    lv_stats = summarize("raw", lv_losses, args.window)

    print()
    print("=== Averaged snapshots (from training log) ===")
    tl_all = parse_losses(TRAIN_LOG, TL_RE)
    tl_losses = maybe_cap(slice_current_session(tl_all))
    print(f"[avg] current session: {len(tl_losses)} of {len(tl_all)} entries")
    tl_stats = summarize("avg", tl_losses, args.window)

    # Verbatim last-N trajectory — trust your eyes more than summary stats
    # when the log spans a saturation-era transition.
    print()
    print(f"=== Last {args.decision_tail} averaged snapshots (verbatim) ===")
    for s, v in tl_losses[-args.decision_tail:]:
        print(f"  [{s:>5}] loss={v:.4f}")

    print()
    print("=== Slope analysis ===")
    if lv_stats:
        sl_raw = slope(lv_losses, args.window)
        print(f"  raw learn_vector slope over last {args.window} steps: "
              f"{sl_raw:+.6f} loss/step")
    if tl_stats:
        sl_avg = slope(tl_losses, args.window)
        print(f"  averaged snapshot slope over last {args.window} snaps: "
              f"{sl_avg:+.6f} loss/snapshot")

    print()
    print("=== Decision ===")

    # Base decision on the LAST decision_tail entries of the averaged
    # series — these are the most recent and least contaminated by the
    # saturation-era earlier in the same session.
    decision_series = tl_losses[-args.decision_tail:] if tl_losses else []
    if not decision_series:
        decision_series = lv_losses[-args.decision_tail:] if lv_losses else []
    if not decision_series:
        print("No usable loss data; cannot decide.")
        return
    dec_vals = [v for _, v in decision_series]
    dec_mean = statistics.mean(dec_vals)
    dec_min = min(dec_vals)
    dec_max = max(dec_vals)
    print(f"Last {len(decision_series)} snapshots → mean={dec_mean:.4f}  "
          f"min={dec_min:.4f}  max={dec_max:.4f}")

    sl = slope(decision_series, args.decision_tail)
    if sl is not None:
        print(f"Slope over those {len(decision_series)}: {sl:+.6f} loss/snapshot")

    # The series-wide stats used later in the threshold checks
    series = tl_stats if tl_stats else lv_stats
    recent_min_gap = series["last_step"] - series["min_step"]
    tail_mean = series["tail_mean"]

    verdicts = []
    if sl is not None and sl < -0.0005:
        verdicts.append("CONTINUE — loss still decreasing noticeably")
    elif sl is not None and -0.0005 <= sl <= 0.0:
        if recent_min_gap > 500:
            verdicts.append("ADVANCE — loss plateaued, no new lows in ≥500 steps")
        else:
            verdicts.append("MARGINAL — near plateau, but a new low "
                            f"happened {recent_min_gap} steps ago; "
                            "give it another 300 steps")
    elif sl is not None and sl > 0:
        verdicts.append(f"INVESTIGATE — loss TREND IS UP (+{sl:.5f}/step); "
                        "possible instability or curriculum shift")
    else:
        verdicts.append("INSUFFICIENT DATA")

    # Low-end envelope check — are the easy examples being solved near-perfectly?
    if tail_mean < 0.5 and series["tail_p05"] < 0.1:
        verdicts.append("EASY-EXAMPLE MASTERY — P05 < 0.1 means the easy curriculum "
                        "items are effectively solved")

    for v in verdicts:
        print(f"  → {v}")


if __name__ == "__main__":
    main()
