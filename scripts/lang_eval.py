#!/usr/bin/env python3
"""
lang_eval.py — Portable language eval against the brain daemon.

Submits a fixed prompt battery via grounded_respond, captures
response/confidence/latency, and snapshots grounded_language
diagnostics pre and post. Output: JSON with the same shape as
the previous ad-hoc eval/lang_eval_step1250.json so trends can
be compared across runs.

Usage:
    python3 scripts/lang_eval.py [--out PATH] [--label STR]

Runs against /var/run/athena/brain.sock by default. Set
NIMCP_BRAIN_SOCKET to override.
"""

import argparse
import json
import os
import sys
import time

# Allow running both pod-side (where brain_client is on PYTHONPATH) and
# from the repo root (where scripts/ is the dir).
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from brain_client import BrainProxy, is_daemon_running, SOCKET_PATH

PROMPTS = [
    "What is a tree?", "Describe the sun.", "Tell me about water.",
    "What is fire?", "What does a rock look like?", "How does the wind feel?",
    "What is sand?", "Describe a mountain.", "What is a river?",
    "Describe the ocean.", "What does a cat do?", "How does a bird fly?",
    "What is a dog?", "Why do fish swim?", "How does a bee work?",
    "What does an owl do at night?", "Why do horses run?",
    "How does a snake move?", "What is an elephant?", "What does a wolf eat?",
    "How do you feel?", "Are you happy?", "What is love?", "What is fear?",
    "Why do we dream?", "What is memory?", "What does it mean to think?",
    "What is hope?", "What is silence?", "Why do people laugh?",
    "What is two plus two?", "What is bigger, three or seven?",
    "Is one less than ten?", "What comes after five?",
    "What is the opposite of hot?", "What is half of eight?",
    "Are circles round?", "Is a square a shape?", "What is zero?",
    "What is the smallest number you know?", "Tell me a story.",
    "What is a friend?", "What happens at a birthday?",
    "Who lives in a family?", "What is a school for?",
    "Why do people share food?", "What do parents do?",
    "What is kindness?", "What is a promise?", "Why do people sing?",
]


def collect_round(brain, prompts, label):
    n_unique = set()
    results = []
    confs = []
    latencies = []
    for q in prompts:
        t0 = time.monotonic()
        try:
            resp = brain.grounded_respond(q)
        except Exception as e:
            resp = {"error": str(e)}
        ms = (time.monotonic() - t0) * 1000.0
        latencies.append(ms)
        ans = resp.get("text") or resp.get("response") or ""
        conf = resp.get("confidence", resp.get("conf", 0.0))
        if isinstance(conf, str):
            try:
                conf = float(conf)
            except ValueError:
                conf = 0.0
        confs.append(conf)
        n_unique.add(ans.strip())
        results.append({"q": q, "a": ans, "conf": conf, "ms": round(ms, 1)})

    confs_sorted = sorted(confs)
    median_conf = confs_sorted[len(confs_sorted) // 2] if confs_sorted else 0.0
    lat_sorted = sorted(latencies)
    median_ms = lat_sorted[len(lat_sorted) // 2] if lat_sorted else 0.0

    return {
        "label": label,
        "n_prompts": len(prompts),
        "n_unique": len(n_unique),
        "diversity": round(len(n_unique) / max(1, len(prompts)), 4),
        "mean_conf": round(sum(confs) / max(1, len(confs)), 4),
        "median_conf": round(median_conf, 4),
        "mean_ms": round(sum(latencies) / max(1, len(latencies)), 1),
        "median_ms": round(median_ms, 1),
        "results": results,
    }


def get_diag(brain):
    """Snapshot grounded_language + bridge diagnostics."""
    try:
        d = brain._send({"cmd": "get_grounded_language_diagnostics"})
        return d.get("grounded_language", d)
    except Exception as e:
        return {"error": str(e)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None,
                    help="Output JSON path (default eval/lang_eval_<step>.json)")
    ap.add_argument("--label", default="default",
                    help="Round label for the JSON output")
    ap.add_argument("--socket", default=os.environ.get("NIMCP_BRAIN_SOCKET", SOCKET_PATH),
                    help="Daemon socket path")
    args = ap.parse_args()

    if not is_daemon_running(args.socket):
        print(f"daemon not responding at {args.socket}", file=sys.stderr)
        sys.exit(1)

    brain = BrainProxy(socket_path=args.socket)

    # Pull current stage/step from the immersive_state.json sidecar.
    # The daemon doesn't expose a get_immersive_state RPC, so we read the
    # canonical state file directly. Falls back to the --label argument.
    step_label = args.label
    state_path = "/workspace/nimcp/checkpoints/athena/immersive_state.json"
    try:
        if os.path.exists(state_path):
            with open(state_path) as f:
                st = json.load(f)
            stage = st.get("stage", "?")
            step = st.get("step", "?")
            step_label = f"stage{stage}_step{step}"
    except Exception:
        pass

    print(f"language eval @ {step_label}", flush=True)

    diag_pre = get_diag(brain)
    round_default = collect_round(brain, PROMPTS, label=args.label or "default")
    diag_post = get_diag(brain)

    out = {
        "prompts": PROMPTS,
        "rounds": [round_default],
        "diag_pre": diag_pre,
        "diag_post": diag_post,
        "meta": {
            "step": step_label,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        },
    }

    out_path = args.out or os.path.join(
        HERE, "..", "eval", f"lang_eval_{step_label}.json")
    out_path = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
    print(f"wrote {out_path}", flush=True)

    r = round_default
    print(f"  diversity={r['diversity']} mean_conf={r['mean_conf']} "
          f"median_ms={r['median_ms']}", flush=True)
    print(f"  unique answers: {r['n_unique']}/{r['n_prompts']}")


if __name__ == "__main__":
    main()
