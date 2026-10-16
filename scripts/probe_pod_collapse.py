#!/usr/bin/env python3
"""
Pod-side mode-collapse probe.

Run this ON THE POD (where the brain daemon is live). It:
  1. Snapshots lang_status — shows current pdw, stage, vocab size, TF state.
  2. Runs a 20-intent sweep through produce_cascade (the conversation path),
     reporting per-intent output + intent-fidelity + div_unique.
  3. Snapshots lang_status again to detect any drift during the probe.

Usage on pod:
    python3 /workspace/nimcp/scripts/probe_pod_collapse.py
or remotely:
    ssh runpod 'python3 /workspace/nimcp/scripts/probe_pod_collapse.py'

To A/B test pdw:
  - Probe at the live value (whatever lang_runtime_default.json was set to)
  - On the pod: edit data/lang_runtime_default.json, set
    produce_distributional_weight to 0.7
  - supervisorctl restart athena-brain
  - Wait for daemon to come back up (it'll be ~3-10 min on a warm resume)
  - Re-run this probe

Intent-fidelity = how many produce outputs mention the intent word.
div_unique      = number of distinct produce outputs across all intents.

Collapse signature:
  intent-fidelity <= 30%   AND div_unique <= 5/20 (≤25%)
Healthy signature:
  intent-fidelity >= 70%   AND div_unique >= 10/20 (≥50%)
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time

# Allow running from any cwd as long as scripts/ is importable.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brain_client import BrainProxy, is_daemon_running, SOCKET_PATH  # noqa: E402


INTENTS_WORDS = [
    # Animals
    "cat", "dog", "bird", "fish", "fox",
    # Actions
    "ran", "ate", "jumped", "flew", "swam",
    # Objects
    "ball", "tree", "house", "book", "water",
    # Abstract — most likely to bleed into a shared cluster
    "love", "peace", "time", "idea", "hope",
]

INTENTS_PHRASES = [
    # Single-clause SVO probes — exercise the clause planner + correctors.
    "the cat ran fast",
    "the dog jumped high",
    "the bird sang loud",
    "the fish swam deep",
    "the fox ate prey",
    # Two-clause — exercises T3-2 conjunction insertion.
    "the cat ran the dog jumped",
    "the bird flew the fish swam",
    # Question forms — exercises speech-act classifier + comprehend path.
    "what is a cat",
    "where is the dog",
    "why does the bird sing",
    # Statements about abstract concepts — most collapse-prone.
    "love is warm",
    "peace is good",
    "an idea takes time",
    "hope brings love",
    "time moves on",
    # Cross-category — animacy preference probes.
    "the cat sees the ball",
    "the dog wants food",
    "a person reads a book",
    # Negation + reconsolidation triggers.
    "the cat did not run",
    "the dog is not big",
]


def load_corpus(spec: str):
    """Resolve a --corpus argument to a list of intent strings.

    spec can be:
      - 'words'   : built-in single-word intent set (20 items)
      - 'phrases' : built-in dialog-style intent set (20 items)
      - PATH      : file path with one intent per line; '#' starts a comment;
                    blank lines ignored.
    """
    if spec == "words":
        return list(INTENTS_WORDS), "built-in:words"
    if spec == "phrases":
        return list(INTENTS_PHRASES), "built-in:phrases"
    # treat as file path
    if not os.path.isfile(spec):
        raise SystemExit(
            f"--corpus: '{spec}' is not 'words', 'phrases', or a readable file")
    out = []
    with open(spec, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.split("#", 1)[0].strip()
            if line:
                out.append(line)
    if not out:
        raise SystemExit(f"--corpus: file '{spec}' produced 0 intents")
    return out, f"file:{spec}"


def hr(title: str = "") -> None:
    bar = "=" * 72
    if title:
        print(f"\n{bar}\n {title}\n{bar}")
    else:
        print(bar)


def snapshot_lang_status(b: BrainProxy, label: str) -> dict:
    hr(f"lang_status — {label}")
    try:
        s = b._send({"cmd": "lang_status"})
    except Exception as e:
        print(f"  ERROR fetching lang_status: {e}")
        return {}
    if s.get("error"):
        print(f"  ERROR: {s['error']}")
        return {}
    flags = s.get("flags", {})
    tunables = s.get("tunables", {})
    stats = s.get("stats", {})
    tf = s.get("tf", {})
    decode = s.get("decode", {})
    print(f"  vocab_size               : {stats.get('vocab_size', 'n/a')}")
    print(f"  total_bindings           : {stats.get('total_bindings', 'n/a')}")
    print(f"  total_comprehensions     : {stats.get('total_comprehensions', 'n/a')}")
    print(f"  total_productions        : {stats.get('total_productions', 'n/a')}")
    print(f"  avg_comprehension_conf   : {stats.get('avg_comprehension_confidence', 'n/a')}")
    print(f"  avg_binding_strength     : {stats.get('avg_binding_strength', 'n/a')}")
    pdw = "n/a"
    fpen = "n/a"
    # pdw + frequency penalty are surfaced through diagnostics, nested under
    # the "grounded_language" sub-dict (NOT at result top level — that nesting
    # is why this readout historically showed n/a).
    try:
        d = b._send({"cmd": "get_grounded_language_diagnostics"})
        gld = d.get("result", d).get("grounded_language", {})
        pdw = gld.get("produce_distributional_weight", "n/a")
        fpen = gld.get("produce_frequency_penalty", "n/a")
    except Exception:
        pass
    if isinstance(pdw, float):
        print(f"  produce_distributional_w : {pdw:.4f}  <-- collapse fix knob")
    else:
        print(f"  produce_distributional_w : {pdw}  <-- collapse fix knob")
    if isinstance(fpen, float):
        print(f"  produce_frequency_penalty: {fpen:.4f}  <-- filler-contamination knob")
    else:
        print(f"  produce_frequency_penalty: {fpen}  <-- filler-contamination knob")
    # TF block (added 2026-05-29 for TF-6 telemetry surface).
    if tf:
        cfg = tf.get("config", {})
        print(f"  tf.master                : {cfg.get('master', 'n/a')}")
        print(f"  tf.lr_trigram            : {cfg.get('lr_trigram', 'n/a')}")
        print(f"  tf.lr_distrib            : {cfg.get('lr_distrib', 'n/a')}")
        print(f"  tf.lr_bridge_stdp        : {cfg.get('lr_bridge_stdp', 'n/a')}")
    print(f"  decode.avg_us_per_call   : {decode.get('avg_us_per_call', 'n/a')}")
    print(f"  decode.total_calls       : {decode.get('total_calls', 'n/a')}")
    return s


def try_set_stage(b: BrainProxy, stage: int) -> None:
    """Best-effort — if BrainProxy has set_active_stage and the daemon
    accepts it, push the gl stage so >=2 correctors fire. Skipped if not."""
    try:
        b.set_active_stage(stage)
        print(f"  set_active_stage({stage}) OK")
    except Exception as e:
        print(f"  set_active_stage skipped: {e}")


def sweep_cascade(b: BrainProxy, intents) -> dict:
    """Run each intent through produce_cascade (conversation path) and
    collect outputs + per-intent confidence + fidelity match."""
    hr("Sweep — produce_cascade(prompt=intent_word)")
    seen = []
    fidelity_hits = 0
    rows = []
    for i, intent in enumerate(intents):
        try:
            resp = b.produce_cascade(prompt=intent)
        except Exception as e:
            print(f"  [{i:2d}] {intent:<10s} -> EXCEPTION {e}")
            continue
        out_text = ""
        # produce_cascade returns ~46 fields; the canonical output text is in
        # 'utterance' or 'text' depending on daemon version. Try both.
        for key in ("utterance", "text", "output", "result"):
            v = resp.get(key) if isinstance(resp, dict) else None
            if isinstance(v, str) and v.strip():
                out_text = v.strip()
                break
            if isinstance(v, dict):
                t = v.get("text") or v.get("utterance")
                if isinstance(t, str) and t.strip():
                    out_text = t.strip()
                    break
        if not out_text:
            out_text = "<EMPTY>"
        confidence = resp.get("self_match", resp.get("confidence", None)) \
            if isinstance(resp, dict) else None

        mentions = intent in out_text.lower()
        if mentions:
            fidelity_hits += 1
        marker = "" if mentions else "  [INTENT-MISS]"
        cstr = f" conf={confidence:.2f}" if isinstance(confidence, (int, float)) else ""
        print(f"  [{i:2d}] {intent:<10s} -> '{out_text}'{cstr}{marker}")
        if out_text != "<EMPTY>" and out_text not in seen:
            seen.append(out_text)
        rows.append({"intent": intent, "out": out_text, "miss": not mentions})

    return {
        "div_unique": len(seen),
        "n_intents": len(intents),
        "fidelity_hits": fidelity_hits,
        "rows": rows,
    }


def sweep_raw(b: BrainProxy, intents) -> dict:
    """RAW mode — comprehend(text) -> semantic_vector -> produce_text(vec).
    Bypasses the 15-stage cascade entirely (no F4/T2/T3-1/T3-2 correctors,
    no TF, no F1 surface polish). This is the path that isolates produce-
    side scoring collapse from corrector behavior — the score_word_against_vector
    selection and the produce_distributional_weight knob both live here.

    A divergence between sweep_raw and sweep_cascade tells you the cascade
    is masking (or causing) a problem the readout doesn't show."""
    hr("Sweep — RAW comprehend->produce_text (no cascade)")
    seen = []
    fidelity_hits = 0
    rows = []
    for i, intent in enumerate(intents):
        try:
            cr = b.comprehend(intent)
        except Exception as e:
            print(f"  [{i:2d}] {intent:<10s} -> comprehend EXCEPTION {e}")
            continue
        vec = cr.get("semantic_vector") if isinstance(cr, dict) else None
        comp_conf = cr.get("confidence", 0.0) if isinstance(cr, dict) else 0.0
        if not isinstance(vec, list) or not vec:
            print(f"  [{i:2d}] {intent:<10s} -> comprehend returned no vector "
                  f"(cr keys: {list(cr.keys()) if isinstance(cr, dict) else type(cr)})")
            continue
        try:
            pr = b.produce_text(vec)
        except Exception as e:
            print(f"  [{i:2d}] {intent:<10s} -> produce_text EXCEPTION {e}")
            continue
        out_text = pr.get("text", "") if isinstance(pr, dict) else ""
        if not isinstance(out_text, str) or not out_text.strip():
            out_text = "<EMPTY>"
        else:
            out_text = out_text.strip()
        prod_conf = pr.get("confidence", 0.0) if isinstance(pr, dict) else 0.0

        mentions = intent in out_text.lower()
        if mentions:
            fidelity_hits += 1
        marker = "" if mentions else "  [INTENT-MISS]"
        print(f"  [{i:2d}] {intent:<10s} -> '{out_text}'"
              f"  comp_conf={comp_conf:.2f} prod_conf={prod_conf:.2f}{marker}")
        if out_text != "<EMPTY>" and out_text not in seen:
            seen.append(out_text)
        rows.append({"intent": intent, "out": out_text, "miss": not mentions})

    return {
        "div_unique": len(seen),
        "n_intents": len(intents),
        "fidelity_hits": fidelity_hits,
        "rows": rows,
    }


def aggregate_rounds(rounds: list, n_intents: int) -> dict:
    """Compute min/mean/max per metric across a list of sweep results.
    Returns an empty-ish dict when `rounds` is empty so the caller never
    has to NULL-check the individual fields."""
    if not rounds:
        return {"mean_div": 0.0, "min_div": 0, "max_div": 0,
                "mean_fid": 0.0, "min_fid": 0, "max_fid": 0,
                "n_rounds": 0, "n_intents": n_intents}
    divs = [r["div_unique"]    for r in rounds]
    fids = [r["fidelity_hits"] for r in rounds]
    return {
        "mean_div": sum(divs) / len(divs),
        "min_div":  min(divs),
        "max_div":  max(divs),
        "mean_fid": sum(fids) / len(fids),
        "min_fid":  min(fids),
        "max_fid":  max(fids),
        "n_rounds": len(rounds),
        "n_intents": n_intents,
    }


def persistent_misses(rounds: list, n_rounds: int) -> set:
    """Intents that missed in at least ceil(n_rounds / 2) of the rounds.
    Filters out one-off misses caused by softmax sampling noise so the
    verdict highlights genuine collapse patterns. Returns a set of intent
    strings. Single-round runs degenerate to "missed at all"."""
    if not rounds or n_rounds <= 0:
        return set()
    threshold = (n_rounds + 1) // 2   # ceil(n/2)
    counts: dict = {}
    for r in rounds:
        for row in r["rows"]:
            if row["miss"]:
                counts[row["intent"]] = counts.get(row["intent"], 0) + 1
    return {intent for intent, c in counts.items() if c >= threshold}


def verdict_agg(agg: dict, rounds: int) -> None:
    """Verdict over aggregated rounds. Single round prints the same numbers
    as before; multi-round prints min/mean/max."""
    n = agg["n_intents"]
    if rounds <= 1:
        du = int(agg["mean_div"])
        fh = int(agg["mean_fid"])
        print(f"  div_unique      = {du:>2d} / {n}")
        print(f"  intent-fidelity = {fh:>2d} / {n}  ({100*fh/n:.0f}%)")
    else:
        print(f"  div_unique      : min={agg['min_div']:>2d}  mean={agg['mean_div']:>4.1f}  max={agg['max_div']:>2d}   / {n}")
        print(f"  intent-fidelity : min={agg['min_fid']:>2d}  mean={agg['mean_fid']:>4.1f}  max={agg['max_fid']:>2d}   / {n}  (mean {100*agg['mean_fid']/n:.0f}%)")
    # Classify against the same thresholds, but use means for multi-round.
    du = agg["mean_div"]
    fid_frac = agg["mean_fid"] / n if n else 0.0
    collapse_ceiling = n // 4   # 25%
    healthy_floor   = n // 2   # 50%
    print()
    if du <= collapse_ceiling and fid_frac <= 0.30:
        print("  STATUS: CATASTROPHIC COLLAPSE — div_unique very low AND <30% intent fidelity")
    elif du <= collapse_ceiling:
        print("  STATUS: OUTPUT COLLAPSE — many intents converged to same text")
    elif fid_frac <= 0.30:
        print("  STATUS: INTENT-MISS COLLAPSE — outputs diverse but don't match intent")
    elif du >= healthy_floor and fid_frac >= 0.70:
        print("  STATUS: HEALTHY — diversity + intent fidelity both above threshold")
    else:
        print(f"  STATUS: BORDERLINE — div_unique={du:.1f}/{n}, fidelity={100*fid_frac:.0f}%")


def verdict(result: dict) -> None:
    hr("VERDICT")
    n = result["n_intents"]
    du = result["div_unique"]
    fh = result["fidelity_hits"]
    print(f"  div_unique      = {du:>2d} / {n}")
    print(f"  intent-fidelity = {fh:>2d} / {n}  ({100*fh/n:.0f}%)")
    misses = [r["intent"] for r in result["rows"] if r["miss"]]
    if misses:
        print(f"  intent-misses   = {misses}")
    collapse_ceiling = n // 4   # 25%
    healthy_floor   = n // 2   # 50%
    print()
    if du <= collapse_ceiling and fh / n <= 0.30:
        print("  STATUS: CATASTROPHIC COLLAPSE — div_unique very low AND <30% intent fidelity")
    elif du <= collapse_ceiling:
        print("  STATUS: OUTPUT COLLAPSE — many intents converged to same text")
    elif fh / n <= 0.30:
        print("  STATUS: INTENT-MISS COLLAPSE — outputs diverse but don't match intent")
    elif du >= healthy_floor and fh / n >= 0.70:
        print("  STATUS: HEALTHY — diversity + intent fidelity both above threshold")
    else:
        print(f"  STATUS: BORDERLINE — div_unique={du}/{n}, fidelity={100*fh/n:.0f}%")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--socket", default=SOCKET_PATH,
                    help=f"daemon socket path (default {SOCKET_PATH})")
    ap.add_argument("--stage", type=int, default=2,
                    help="push set_active_stage(N) before probing (default 2)")
    ap.add_argument("--json", action="store_true",
                    help="emit a final machine-readable JSON line")
    ap.add_argument("--no-set-stage", action="store_true",
                    help="skip set_active_stage (use whatever the daemon has)")
    ap.add_argument("--corpus", default="words",
                    help="intent set: 'words' (20 single-word), 'phrases' "
                         "(20 dialog-style), or PATH to a file with one "
                         "intent per line (default: words)")
    ap.add_argument("--rounds", type=int, default=1,
                    help="run the sweep N times and report min/mean/max per "
                         "metric — averages out sampling noise from produce's "
                         "softmax temperature (default: 1)")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--raw", action="store_true",
                      help="sweep RAW path only: comprehend->produce_text (no cascade, no correctors)")
    mode.add_argument("--both", action="store_true",
                      help="sweep BOTH raw and cascade (default cascade-only)")
    args = ap.parse_args()

    if args.rounds < 1:
        print("ERROR: --rounds must be >= 1", file=sys.stderr)
        return 2

    try:
        intents, corpus_label = load_corpus(args.corpus)
    except SystemExit as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    if not is_daemon_running(args.socket):
        print(f"ERROR: brain daemon not running at {args.socket}", file=sys.stderr)
        return 2

    b = BrainProxy(socket_path=args.socket)

    print(f"connected to {args.socket}")
    print(f"probing at {time.strftime('%Y-%m-%d %H:%M:%S')}")
    mode_str = "BOTH (raw + cascade)" if args.both else ("RAW only" if args.raw else "CASCADE only")
    print(f"sweep mode: {mode_str}")
    print(f"corpus    : {corpus_label} ({len(intents)} intents)")
    print(f"rounds    : {args.rounds}")

    snapshot_lang_status(b, "BEFORE")

    if not args.no_set_stage:
        hr("STAGE PUSH")
        try_set_stage(b, args.stage)

    raw_rounds = []
    cas_rounds = []
    for r in range(args.rounds):
        if args.rounds > 1:
            hr(f"ROUND {r+1} / {args.rounds}")
        if args.raw or args.both:
            raw_rounds.append(sweep_raw(b, intents))
        if not args.raw or args.both:
            cas_rounds.append(sweep_cascade(b, intents))

    snapshot_lang_status(b, "AFTER")

    raw_agg = aggregate_rounds(raw_rounds, len(intents)) if raw_rounds else None
    cas_agg = aggregate_rounds(cas_rounds, len(intents)) if cas_rounds else None

    if raw_agg is not None:
        hr("VERDICT — RAW (no cascade)")
        verdict_agg(raw_agg, args.rounds)
    if cas_agg is not None:
        hr("VERDICT — CASCADE")
        verdict_agg(cas_agg, args.rounds)

    # Cross-mode comparison when both ran — reveals what the cascade adds /
    # masks. Compares MEAN metrics across rounds for stability.
    if raw_agg is not None and cas_agg is not None:
        hr("RAW vs CASCADE — diff (means across rounds)")
        n = len(intents)
        print(f"  div_unique     : raw={raw_agg['mean_div']:.1f}/{n}   cascade={cas_agg['mean_div']:.1f}/{n}   delta={cas_agg['mean_div']-raw_agg['mean_div']:+.1f}")
        print(f"  intent-fidelity: raw={raw_agg['mean_fid']:.1f}/{n}   cascade={cas_agg['mean_fid']:.1f}/{n}   delta={cas_agg['mean_fid']-raw_agg['mean_fid']:+.1f}")
        # Persistent misses across rounds — intents that missed in >= ceil(rounds/2)
        # of the rounds. More signal than "missed once".
        raw_pers = persistent_misses(raw_rounds, args.rounds)
        cas_pers = persistent_misses(cas_rounds, args.rounds)
        common  = sorted(raw_pers & cas_pers)
        raw_only = sorted(raw_pers - cas_pers)
        cas_only = sorted(cas_pers - raw_pers)
        if common:
            print(f"  persistent miss on both     : {common}")
        if raw_only:
            print(f"  persistent miss raw-only    : {raw_only}  (cascade rescued)")
        if cas_only:
            print(f"  persistent miss cascade-only: {cas_only}  (cascade broke)")

    if args.json:
        compact = {"rounds": args.rounds, "corpus": corpus_label,
                   "n_intents": len(intents)}
        if raw_agg is not None:
            compact["raw"] = {
                "mean_div_unique": raw_agg["mean_div"],
                "min_div_unique":  raw_agg["min_div"],
                "max_div_unique":  raw_agg["max_div"],
                "mean_fidelity":   raw_agg["mean_fid"],
                "persistent_misses": sorted(persistent_misses(raw_rounds, args.rounds)),
            }
        if cas_agg is not None:
            compact["cascade"] = {
                "mean_div_unique": cas_agg["mean_div"],
                "min_div_unique":  cas_agg["min_div"],
                "max_div_unique":  cas_agg["max_div"],
                "mean_fidelity":   cas_agg["mean_fid"],
                "persistent_misses": sorted(persistent_misses(cas_rounds, args.rounds)),
            }
        print()
        print("PROBE_JSON " + json.dumps(compact))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
