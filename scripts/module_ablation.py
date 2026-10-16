#!/usr/bin/env python3
"""module_ablation.py — mechanical statue-detector for the NIMCP brain daemon.

Toggles each runtime-toggleable cognitive module off, watches ann_loss for
N seconds, toggles it back on, and classifies the module as STATUE / WEAK /
STRONG / BROKE_TRAINING based on z-score against baseline pure_ann variance.

A "statue" is a wired-in module that looks alive (logs metrics, runs in the
hot path) but ablating it produces no detectable change in training loss —
i.e. it isn't actually contributing.

Wire protocol matches early_stop_watchdog.py:
  - AF_UNIX socket /var/run/athena/brain.sock
  - 4-byte big-endian length prefix, then JSON body
  - request: {"cmd": "<name>", ...args}; response: same envelope.

The harness only uses RPCs that brain_daemon.py already exposes — no
daemon-side changes required.
"""

import argparse
import json
import logging
import math
import signal
import socket
import statistics
import struct
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

# ----------------------------------------------------------------------------
# Defaults
# ----------------------------------------------------------------------------

DEFAULT_SOCK = "/var/run/athena/brain.sock"
DEFAULT_BASELINE_S = 120
DEFAULT_ABLATION_S = 120
DEFAULT_RECOVERY_S = 60
DEFAULT_SOAK_S = 30
DEFAULT_MIN_STEPS = 20
DEFAULT_OUT = "ablation_report.json"
SAMPLE_INTERVAL_S = 10
SOCK_TIMEOUT_S = 10.0

# Default module order: least-disruptive first (auxiliary bridges/options),
# then training-freeze toggles last.
DEFAULT_MODULES: List[str] = [
    "enable_world_model_bridge",
    "enable_world_model",
    "enable_mixed_precision",
    "enable_gradient_checkpointing",
    "enable_biological_plasticity",
    "set_train_cnn",
    "set_train_lnn",
    "set_train_snn",
    "set_train_ann",
]

# RPC schema per toggle.
#   field   — JSON arg name carrying the toggle value
#   off     — value that disables the module
#   on      — value that re-enables it (the module's "live" state)
#   kind    — "bool" | "scale" — how to type the field
TOGGLE_SPEC: Dict[str, Dict[str, Any]] = {
    "set_train_ann":               {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "set_train_cnn":               {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "set_train_snn":               {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "set_train_lnn":               {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "set_snn_only_recovery":       {"field": "enabled", "off": True,  "on": False, "kind": "bool"},
    "enable_biological_plasticity":{"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "enable_world_model_bridge":   {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "enable_world_model":          {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "enable_mixed_precision":      {"field": "enabled", "off": False, "on": True,  "kind": "bool"},
    "enable_gradient_checkpointing":{"field": "enabled","off": False, "on": True,  "kind": "bool"},
    "set_ensemble_warmup_scale":   {"field": "scale",   "off": 0.0,   "on": 1.0,   "kind": "scale"},
    # set_plasticity_state — toggling plasticity mode is a multi-state
    # operation; we expose it via the spec but it's not in the default
    # rotation. Off=CONSOLIDATION (no learning), On=ACQUISITION.
    "set_plasticity_state":        {"field": "state",   "off": "CONSOLIDATION",
                                    "on": "ACQUISITION", "kind": "state"},
}

logging.basicConfig(
    format="%(asctime)s [ablation] %(levelname)s %(message)s",
    level=logging.INFO,
)
log = logging.getLogger("ablation")


# ----------------------------------------------------------------------------
# Wire protocol — same as early_stop_watchdog.py
# ----------------------------------------------------------------------------

class RPCError(RuntimeError):
    pass


def call(sock_path: str, cmd: str, **kwargs: Any) -> Optional[Dict[str, Any]]:
    """Length-prefixed JSON RPC over Unix socket. Returns parsed reply, or
    None if the daemon closed without sending a reply. Raises RPCError on
    socket-level failures (timeout, connection refused, malformed JSON)."""
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(SOCK_TIMEOUT_S)
    try:
        s.connect(sock_path)
        msg = {"cmd": cmd, **kwargs}
        data = json.dumps(msg).encode()
        s.sendall(struct.pack(">I", len(data)) + data)
        hdr = s.recv(4)
        if len(hdr) < 4:
            return None
        n = struct.unpack(">I", hdr)[0]
        buf = b""
        while len(buf) < n:
            chunk = s.recv(min(65536, n - len(buf)))
            if not chunk:
                break
            buf += chunk
        if len(buf) < n:
            raise RPCError(f"truncated reply ({len(buf)}/{n} bytes)")
        try:
            return json.loads(buf)
        except json.JSONDecodeError as e:
            raise RPCError(f"non-JSON reply: {e}") from e
    except (socket.timeout, ConnectionError, OSError) as e:
        raise RPCError(f"socket error on {cmd}: {e}") from e
    finally:
        try:
            s.close()
        except OSError:
            pass


# ----------------------------------------------------------------------------
# Metric helpers
# ----------------------------------------------------------------------------

def decompose_pure_ann(ann_loss: float, snn_loss: float, lnn_loss: float,
                       cnn_loss: float) -> float:
    """Recover the pure_ann component from the blended ann_loss.

    Blend formula (per CLAUDE.md):
        cnn_norm = min(1, log2(1+cnn_loss))
        ann_loss = 0.50*pure_ann + 0.15*snn_loss + 0.25*lnn_loss + 0.10*cnn_norm
    """
    try:
        cnn_norm = min(1.0, math.log2(1.0 + max(0.0, cnn_loss)))
    except (ValueError, OverflowError):
        cnn_norm = 1.0
    pure = (ann_loss - 0.15 * snn_loss - 0.25 * lnn_loss - 0.10 * cnn_norm) / 0.50
    return pure


def sample_metrics(sock_path: str) -> Optional[Dict[str, float]]:
    """One snapshot of (ann_loss, pure_ann, snn_loss, lnn_loss, cnn_loss,
    ann_steps). Returns None on RPC failure (caller decides whether to
    retry)."""
    try:
        reply = call(sock_path, "get_network_metrics")
    except RPCError as e:
        log.warning("get_network_metrics failed: %s", e)
        return None
    if not reply or "metrics" not in reply:
        return None
    m = reply["metrics"] or {}
    ann_loss = float(m.get("ann_loss", 0.0) or 0.0)
    snn_loss = float(m.get("snn_loss", 0.0) or 0.0)
    lnn_loss = float(m.get("lnn_loss", 0.0) or 0.0)
    cnn_loss = float(m.get("cnn_loss", 0.0) or 0.0)
    ann_steps = int(m.get("ann_steps", 0) or 0)
    return {
        "ann_loss": ann_loss,
        "snn_loss": snn_loss,
        "lnn_loss": lnn_loss,
        "cnn_loss": cnn_loss,
        "pure_ann": decompose_pure_ann(ann_loss, snn_loss, lnn_loss, cnn_loss),
        "ann_steps": float(ann_steps),
        "t": time.time(),
    }


def collect_window(sock_path: str, duration_s: float, label: str) -> List[Dict[str, float]]:
    """Sample every SAMPLE_INTERVAL_S for duration_s. Returns list of
    snapshots (failures silently dropped, but logged)."""
    samples: List[Dict[str, float]] = []
    deadline = time.time() + duration_s
    next_sample = time.time()
    log.info("[%s] collecting for %.0fs", label, duration_s)
    while time.time() < deadline:
        if time.time() >= next_sample:
            snap = sample_metrics(sock_path)
            if snap is not None:
                samples.append(snap)
            next_sample = time.time() + SAMPLE_INTERVAL_S
        time.sleep(0.5)
    log.info("[%s] collected %d samples", label, len(samples))
    return samples


def summarize(samples: List[Dict[str, float]]) -> Dict[str, float]:
    """Mean+std of pure_ann over the window, plus step delta."""
    if not samples:
        return {
            "n_samples": 0,
            "pure_ann_mean": float("nan"),
            "pure_ann_std": float("nan"),
            "ann_loss_mean": float("nan"),
            "ann_steps_in_window": 0,
            "ann_steps_start": 0,
            "ann_steps_end": 0,
        }
    pure = [s["pure_ann"] for s in samples]
    ann = [s["ann_loss"] for s in samples]
    ann_steps_start = int(samples[0]["ann_steps"])
    ann_steps_end = int(samples[-1]["ann_steps"])
    return {
        "n_samples": len(samples),
        "pure_ann_mean": statistics.fmean(pure),
        "pure_ann_std": statistics.stdev(pure) if len(pure) > 1 else 0.0,
        "ann_loss_mean": statistics.fmean(ann),
        "ann_steps_in_window": ann_steps_end - ann_steps_start,
        "ann_steps_start": ann_steps_start,
        "ann_steps_end": ann_steps_end,
    }


# ----------------------------------------------------------------------------
# Toggle helpers
# ----------------------------------------------------------------------------

def toggle_module(sock_path: str, module: str, target: str) -> Tuple[bool, str]:
    """Send the off/on RPC for `module`. target ∈ {"off","on"}.
    Returns (ok, detail)."""
    spec = TOGGLE_SPEC.get(module)
    if spec is None:
        return False, f"unknown module {module!r}"
    field = spec["field"]
    value = spec[target]
    try:
        reply = call(sock_path, module, **{field: value})
    except RPCError as e:
        return False, str(e)
    if reply is None:
        return False, "no reply"
    if isinstance(reply, dict) and reply.get("status") == "error":
        return False, f"daemon error: {reply.get('error') or reply}"
    return True, json.dumps(reply)[:160]


# ----------------------------------------------------------------------------
# Cleanup / signal handling
# ----------------------------------------------------------------------------

class CleanupRegistry:
    """Tracks modules currently toggled off so a SIGINT/SIGTERM can restore
    them before exit."""

    def __init__(self, sock_path: str) -> None:
        self.sock = sock_path
        self.disabled: List[str] = []

    def mark_off(self, module: str) -> None:
        if module not in self.disabled:
            self.disabled.append(module)

    def mark_on(self, module: str) -> None:
        if module in self.disabled:
            self.disabled.remove(module)

    def restore_all(self) -> None:
        if not self.disabled:
            return
        log.warning("restoring %d modules left disabled: %s",
                    len(self.disabled), self.disabled)
        for module in list(self.disabled):
            ok, detail = toggle_module(self.sock, module, "on")
            if ok:
                log.info("  restored %s -> %s", module, detail)
                self.mark_on(module)
            else:
                log.error("  FAILED to restore %s: %s", module, detail)


# ----------------------------------------------------------------------------
# Verdict
# ----------------------------------------------------------------------------

def classify(baseline: Dict[str, float], ablated: Dict[str, float],
             recovered: Dict[str, float]) -> Tuple[str, float, bool]:
    """Return (verdict, delta_z, broke_training)."""
    bmean = baseline["pure_ann_mean"]
    bstd = baseline["pure_ann_std"]
    amean = ablated["pure_ann_mean"]
    rmean = recovered["pure_ann_mean"]

    # Need a real baseline std to compute z. If std is ~0, fall back to a
    # tiny epsilon so any non-zero delta reads as STRONG.
    if not math.isfinite(bmean) or not math.isfinite(amean):
        return "RPC_ERROR", float("nan"), False

    eps = max(abs(bmean) * 1e-6, 1e-9)
    denom = bstd if (math.isfinite(bstd) and bstd > eps) else eps
    delta_z = (amean - bmean) / denom

    # Broke training: recovery did not return within 1σ of baseline.
    broke = False
    if math.isfinite(rmean):
        recov_z = (rmean - bmean) / denom
        if abs(recov_z) > 1.0:
            broke = True

    abs_z = abs(delta_z)
    if abs_z < 0.5:
        verdict = "STATUE"
    elif abs_z < 2.0:
        verdict = "WEAK"
    else:
        verdict = "STRONG"
    return verdict, delta_z, broke


# ----------------------------------------------------------------------------
# Per-module run
# ----------------------------------------------------------------------------

def run_module(sock_path: str, module: str, registry: CleanupRegistry,
               args: argparse.Namespace) -> Dict[str, Any]:
    """Run baseline → ablate → soak → ablation window → restore → recovery
    window → classify. Returns the full record for the report."""
    log.info("=" * 72)
    log.info("MODULE: %s", module)
    log.info("=" * 72)

    record: Dict[str, Any] = {
        "module": module,
        "baseline": {},
        "ablated": {},
        "recovered": {},
        "delta_z": float("nan"),
        "verdict": "PENDING",
        "broke_training": False,
        "notes": [],
    }

    if module not in TOGGLE_SPEC:
        record["verdict"] = "UNKNOWN_MODULE"
        record["notes"].append(f"no spec for {module}")
        return record

    # Baseline.
    base_samples = collect_window(sock_path, args.baseline_s, "baseline")
    base_summary = summarize(base_samples)
    record["baseline"] = base_summary
    log.info("baseline: pure_ann=%.5f ± %.5f, ann_steps_advanced=%d, n=%d",
             base_summary["pure_ann_mean"], base_summary["pure_ann_std"],
             base_summary["ann_steps_in_window"], base_summary["n_samples"])

    if base_summary["ann_steps_in_window"] < args.min_steps:
        record["verdict"] = "INSUFFICIENT_PROGRESS"
        record["notes"].append(
            f"ann_steps advanced {base_summary['ann_steps_in_window']} during "
            f"baseline (< min_steps={args.min_steps}); skipping ablation.")
        log.warning("INSUFFICIENT_PROGRESS — skipping %s", module)
        return record

    # Ablate.
    log.info("ablating %s ...", module)
    ok, detail = toggle_module(sock_path, module, "off")
    if not ok:
        record["verdict"] = "RPC_ERROR"
        record["notes"].append(f"toggle off failed: {detail}")
        log.error("RPC_ERROR on toggle-off: %s", detail)
        return record
    registry.mark_off(module)
    log.info("  off-reply: %s", detail)

    # Soak transients.
    log.info("soaking %ds ...", args.soak_s)
    time.sleep(args.soak_s)

    # Ablation window.
    abl_samples = collect_window(sock_path, args.ablation_s, "ablated")
    abl_summary = summarize(abl_samples)
    record["ablated"] = abl_summary
    log.info("ablated: pure_ann=%.5f, ann_steps_advanced=%d",
             abl_summary["pure_ann_mean"], abl_summary["ann_steps_in_window"])

    # Restore.
    log.info("restoring %s ...", module)
    ok, detail = toggle_module(sock_path, module, "on")
    if not ok:
        record["verdict"] = "RPC_ERROR"
        record["notes"].append(f"toggle on failed: {detail}")
        log.error("RPC_ERROR on toggle-on: %s", detail)
        # Leave it in registry so cleanup retries on exit.
        return record
    registry.mark_on(module)
    log.info("  on-reply: %s", detail)

    # Recovery window.
    rec_samples = collect_window(sock_path, args.recovery_s, "recovery")
    rec_summary = summarize(rec_samples)
    record["recovered"] = rec_summary
    log.info("recovered: pure_ann=%.5f, ann_steps_advanced=%d",
             rec_summary["pure_ann_mean"], rec_summary["ann_steps_in_window"])

    # Classify.
    verdict, delta_z, broke = classify(base_summary, abl_summary, rec_summary)
    record["verdict"] = verdict
    record["delta_z"] = delta_z
    record["broke_training"] = broke
    log.info("VERDICT: %s  (Δz=%+.2f, broke=%s)", verdict, delta_z, broke)
    return record


# ----------------------------------------------------------------------------
# Reporting
# ----------------------------------------------------------------------------

def print_summary(records: List[Dict[str, Any]]) -> None:
    print()
    print("=" * 96)
    print("ABLATION SUMMARY")
    print("=" * 96)
    hdr = f"{'MODULE':<32} {'VERDICT':<22} {'Δz':>8} {'BASE μ':>10} {'ABL μ':>10} {'BROKE':>6}"
    print(hdr)
    print("-" * len(hdr))
    for r in records:
        m = r["module"]
        v = r["verdict"]
        dz = r.get("delta_z", float("nan"))
        bm = r.get("baseline", {}).get("pure_ann_mean", float("nan"))
        am = r.get("ablated", {}).get("pure_ann_mean", float("nan"))
        broke = "yes" if r.get("broke_training") else ""

        def fmt(x: float, w: int) -> str:
            return f"{x:>{w}.4f}" if (isinstance(x, float) and math.isfinite(x)) else f"{'n/a':>{w}}"

        print(f"{m:<32} {v:<22} {fmt(dz, 8)} {fmt(bm, 10)} {fmt(am, 10)} {broke:>6}")
    print("=" * 96)
    statues = [r["module"] for r in records if r["verdict"] == "STATUE"]
    if statues:
        print(f"\nStatues detected ({len(statues)}): {', '.join(statues)}")
    else:
        print("\nNo statues detected in this run.")


# ----------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Module-ablation harness for the NIMCP brain daemon. "
                    "Toggles each module off, watches ann_loss, toggles "
                    "back on, and classifies modules as STATUE / WEAK / "
                    "STRONG / BROKE_TRAINING.")
    p.add_argument("--socket", default=DEFAULT_SOCK,
                   help=f"Unix socket path (default {DEFAULT_SOCK})")
    p.add_argument("--baseline-s", type=int, default=DEFAULT_BASELINE_S,
                   dest="baseline_s",
                   help=f"Baseline window seconds (default {DEFAULT_BASELINE_S})")
    p.add_argument("--ablation-s", type=int, default=DEFAULT_ABLATION_S,
                   dest="ablation_s",
                   help=f"Ablation window seconds (default {DEFAULT_ABLATION_S})")
    p.add_argument("--recovery-s", type=int, default=DEFAULT_RECOVERY_S,
                   dest="recovery_s",
                   help=f"Recovery window seconds (default {DEFAULT_RECOVERY_S})")
    p.add_argument("--soak-s", type=int, default=DEFAULT_SOAK_S,
                   dest="soak_s",
                   help=f"Post-ablate soak seconds (default {DEFAULT_SOAK_S})")
    p.add_argument("--min-steps", type=int, default=DEFAULT_MIN_STEPS,
                   dest="min_steps",
                   help=f"Min ann_steps that must advance during baseline "
                        f"(default {DEFAULT_MIN_STEPS})")
    p.add_argument("--modules", default=None,
                   help="Comma-separated module list (default: full ordered list)")
    p.add_argument("--out", default=DEFAULT_OUT,
                   help=f"Output JSON report path (default {DEFAULT_OUT})")
    return p.parse_args()


def resolve_modules(spec_arg: Optional[str]) -> List[str]:
    if not spec_arg:
        return list(DEFAULT_MODULES)
    out = []
    for tok in spec_arg.split(","):
        tok = tok.strip()
        if tok:
            out.append(tok)
    return out


def preflight(sock_path: str) -> bool:
    """Verify daemon is reachable, training is progressing, and not
    early-stopped. Returns True if it's safe to proceed."""
    log.info("preflight: probing %s", sock_path)
    try:
        m = call(sock_path, "get_network_metrics")
    except RPCError as e:
        log.error("preflight: cannot reach daemon: %s", e)
        return False
    if not m or "metrics" not in m:
        log.error("preflight: get_network_metrics returned no data: %s", m)
        return False
    log.info("preflight: ann_loss=%.5f, ann_steps=%s",
             m["metrics"].get("ann_loss", float("nan")),
             m["metrics"].get("ann_steps", "?"))

    try:
        h = call(sock_path, "utm_get_training_health")
    except RPCError as e:
        log.warning("preflight: utm_get_training_health failed (%s); proceeding anyway", e)
        return True
    if h and "health" in h:
        es = int(h["health"].get("early_stopped", 0) or 0)
        if es:
            log.error("preflight: early_stopped=1 — training is frozen, "
                      "ablation results would be meaningless. Aborting.")
            return False
        log.info("preflight: training healthy (health=%s, dfa=%s)",
                 h["health"].get("health_name"),
                 h["health"].get("dfa_exponent"))
    return True


def main() -> int:
    args = parse_args()
    modules = resolve_modules(args.modules)

    log.info("ablation harness starting")
    log.info("  socket   = %s", args.socket)
    log.info("  windows  = baseline %ds / ablation %ds / recovery %ds (soak %ds)",
             args.baseline_s, args.ablation_s, args.recovery_s, args.soak_s)
    log.info("  modules  = %s", modules)
    log.info("  out      = %s", args.out)

    if not preflight(args.socket):
        return 2

    registry = CleanupRegistry(args.socket)

    def handle_signal(signum: int, _frame: Any) -> None:
        log.warning("received signal %d — restoring modules and exiting", signum)
        registry.restore_all()
        sys.exit(130 if signum == signal.SIGINT else 143)

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    records: List[Dict[str, Any]] = []
    try:
        for module in modules:
            try:
                rec = run_module(args.socket, module, registry, args)
            except Exception as e:  # noqa: BLE001 — never let one module kill the run
                log.exception("module %s raised: %s", module, e)
                rec = {
                    "module": module,
                    "verdict": "EXCEPTION",
                    "notes": [f"unhandled exception: {e!r}"],
                    "baseline": {}, "ablated": {}, "recovered": {},
                    "delta_z": float("nan"), "broke_training": False,
                }
            records.append(rec)

            # Persist after every module so a kill mid-run still leaves
            # partial results on disk.
            try:
                with open(args.out, "w") as f:
                    json.dump({
                        "generated": time.time(),
                        "socket": args.socket,
                        "config": {
                            "baseline_s": args.baseline_s,
                            "ablation_s": args.ablation_s,
                            "recovery_s": args.recovery_s,
                            "soak_s": args.soak_s,
                            "min_steps": args.min_steps,
                        },
                        "modules": records,
                    }, f, indent=2, default=str)
            except OSError as e:
                log.error("could not write %s: %s", args.out, e)
    finally:
        registry.restore_all()

    print_summary(records)
    log.info("report written to %s", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
