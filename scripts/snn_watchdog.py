#!/usr/bin/env python3
"""
snn_watchdog.py — autonomous SNN health monitor + auto-tuner.

Polls the brain daemon every POLL_INTERVAL seconds, classifies the SNN's
dynamical regime, and issues `snn_tune` / `snn_force_quench` RPCs to
correct known failure modes before they collapse training.

Managed by supervisord — survives brain restarts, operator disconnects,
and SIGSEGV autorestarts. All actions logged to a dedicated log file so
you can see exactly what happened while you were away.

Failure modes handled:
  DEAD_CASCADE     — input pops firing but L1+ silent; bump dead-escape
  FULL_COLLAPSE    — nearly all pops < 0.5% firing; lower target + force quench recovery attempt
  SATURATION       — many pops > 10% firing; tighten scale-up, drop R-STDP LR
  WHIPLASH         — reward modulations swinging ±0.1+; halve baseline alpha
  HEALTHY          — pops mostly converging on target; no action

Each action has a cooldown so the watchdog doesn't fight its own prior
tuning — dynamics take minutes to respond to parameter changes.

Env config (optional):
  BRAIN_SOCKET   default /var/run/athena/brain.sock
  POLL_INTERVAL  default 10 (seconds)
  COOLDOWN       default 300 (seconds between corrective tunings)
  STATUS_PERIOD  default 60 (seconds between "all quiet" status heartbeats)
"""
import json
import logging
import os
import socket
import struct
import sys
import time

SOCKET_PATH   = os.environ.get("BRAIN_SOCKET",  "/var/run/athena/brain.sock")
POLL_INTERVAL = float(os.environ.get("POLL_INTERVAL", "10"))
COOLDOWN      = float(os.environ.get("COOLDOWN",      "300"))
STATUS_PERIOD = float(os.environ.get("STATUS_PERIOD", "60"))

# -----------------------------------------------------------------------------
# Logging — rotating file at /var/log/athena-snn-watchdog.log
# -----------------------------------------------------------------------------
LOG_PATH = os.environ.get("WATCHDOG_LOG", "/var/log/athena-snn-watchdog.log")
logging.basicConfig(
    filename=LOG_PATH,
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
logger = logging.getLogger("snn_watchdog")
# Also mirror to stdout so supervisord captures it.
_stdout = logging.StreamHandler(sys.stdout)
_stdout.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
logger.addHandler(_stdout)


# -----------------------------------------------------------------------------
# RPC — length-prefixed JSON over Unix socket
# -----------------------------------------------------------------------------
def rpc(cmd_dict: dict, timeout: float = 10.0) -> dict:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(SOCKET_PATH)
    payload = json.dumps(cmd_dict).encode()
    s.sendall(struct.pack(">I", len(payload)) + payload)
    size_bytes = _recv_n(s, 4)
    size = struct.unpack(">I", size_bytes)[0]
    resp = json.loads(_recv_n(s, size).decode())
    s.close()
    return resp


def _recv_n(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed mid-message")
        buf += chunk
    return buf


# -----------------------------------------------------------------------------
# Pop classification — firing-rate buckets
# -----------------------------------------------------------------------------
def classify(pops: list, target: float) -> dict:
    """Bucket populations by firing rate and return counts + sample names."""
    buckets = {
        "dead":       [],  # < 0.5%
        "quiet":      [],  # 0.5–2%
        "near":       [],  # 2–2.5%
        "band":       [],  # 2.5–3.5%  (deadband)
        "over":       [],  # 3.5–5%
        "hot":        [],  # 5–10%
        "saturated":  [],  # > 10%
    }
    for pop in pops:
        rate = pop.get("firing_rate_ema", 0.0)
        name = pop.get("name", "?")
        if rate < 0.005:        buckets["dead"].append(name)
        elif rate < 0.02:       buckets["quiet"].append(name)
        elif rate < 0.025:      buckets["near"].append(name)
        elif rate < 0.035:      buckets["band"].append(name)
        elif rate < 0.05:       buckets["over"].append(name)
        elif rate < 0.10:       buckets["hot"].append(name)
        else:                   buckets["saturated"].append(name)
    return buckets


def input_pop_rate(pops: list) -> float:
    """Mean firing rate of populations whose name starts with 'input'."""
    rates = [p.get("firing_rate_ema", 0.0) for p in pops if p.get("name", "").startswith("input")]
    return sum(rates) / len(rates) if rates else 0.0


# -----------------------------------------------------------------------------
# Failure-mode detectors
# -----------------------------------------------------------------------------
def detect(pops: list, params: dict) -> tuple[str, str]:
    """Return (regime, rationale). 'HEALTHY' means no action needed."""
    if not pops:
        return "UNKNOWN", "no populations reported"

    target = params.get("target_rate", 0.03)
    buckets = classify(pops, target)
    n_total = len(pops)
    n_dead = len(buckets["dead"])
    n_quiet = len(buckets["quiet"])
    n_band = len(buckets["band"])
    n_saturated = len(buckets["saturated"])
    n_hot = len(buckets["hot"])
    in_rate = input_pop_rate(pops)

    if n_dead + n_quiet >= n_total - 2:
        return "FULL_COLLAPSE", f"{n_dead} dead + {n_quiet} quiet of {n_total}"

    # DEAD_CASCADE: inputs firing, hidden layers silent
    hidden_dead = [n for n in buckets["dead"] if not n.startswith("input")]
    if in_rate > 0.05 and len(hidden_dead) >= n_total - 6:
        return (
            "DEAD_CASCADE",
            f"inputs @ {in_rate:.2%}, {len(hidden_dead)} hidden pops dead",
        )

    if n_saturated >= 10:
        return "SATURATION", f"{n_saturated} pops > 10% firing"

    if n_band + len(buckets["near"]) + len(buckets["over"]) >= n_total // 2:
        return "HEALTHY", f"{n_band + len(buckets['near']) + len(buckets['over'])}/{n_total} pops near target"

    return "TRANSIENT", (
        f"dead={n_dead} quiet={n_quiet} band={n_band} hot={n_hot} "
        f"saturated={n_saturated} input_rate={in_rate:.2%}"
    )


# -----------------------------------------------------------------------------
# Corrective actions
# -----------------------------------------------------------------------------
def _tune(name: str, value: float):
    try:
        r = rpc({"cmd": "snn_tune", "name": name, "value": value})
        if r.get("error"):
            logger.error(f"snn_tune({name}={value}) failed: {r['error']}")
            return False
        logger.info(f"snn_tune({name}={value}) OK")
        return True
    except Exception as e:
        logger.error(f"snn_tune({name}={value}) rpc error: {e}")
        return False


def _force_quench(n: int):
    try:
        rpc({"cmd": "snn_force_quench", "n": n}, timeout=30.0)
        logger.info(f"snn_force_quench({n}) issued")
    except Exception as e:
        logger.error(f"snn_force_quench({n}) rpc error: {e}")


def act(regime: str, params: dict):
    """Apply corrective tunings for a detected failure mode.
    Actions are cumulative; rely on cooldown to prevent runaway."""
    if regime == "DEAD_CASCADE":
        # Quiet pops need to escape faster. Bump dead-escape cap and widen threshold.
        _tune("max_scale_dead", min(1.10, params.get("max_scale_dead", 1.05) + 0.02))
        _tune("dead_threshold", min(0.3, params.get("dead_threshold", 0.1) + 0.05))
    elif regime == "FULL_COLLAPSE":
        # Everyone dead: make target gentler, push escape cap, then force-quench.
        _tune("target_rate", max(0.01, params.get("target_rate", 0.03) - 0.005))
        _tune("max_scale_dead", min(1.10, params.get("max_scale_dead", 1.05) + 0.02))
        _force_quench(15)
    elif regime == "SATURATION":
        # Too many pops firing hot: drop LR, narrow scale-up ceiling.
        _tune("rstdp_lr", max(0.00002, params.get("rstdp_lr", 0.0001) * 0.5))
        _tune("homeo_max_scale", max(1.005, params.get("homeo_max_scale", 1.02) - 0.01))
    elif regime == "WHIPLASH":
        # Reward baseline oscillating: halve the EMA alpha.
        _tune("rstdp_baseline_alpha", max(0.0001,
              params.get("rstdp_baseline_alpha", 0.001) * 0.5))


# -----------------------------------------------------------------------------
# Symmetric recovery — undo crushed knobs after regime stabilizes.
#
# The old watchdog was one-way: SATURATION halved rstdp_lr each cooldown,
# but nothing restored it when things recovered. Over a long session the
# knobs ratchet monotonically down — eventually R-STDP stops learning
# entirely and any transient dead pop becomes permanent.
#
# Recovery: when regime is HEALTHY or TRANSIENT for N consecutive polls
# (drift-safe), step each watchdog-managed knob fractionally back toward
# its baseline. Uses a small step (20% of gap) so recovery is gentle.
# -----------------------------------------------------------------------------
_MANAGED_KEYS = ("rstdp_lr", "homeo_max_scale", "target_rate", "max_scale_dead",
                 "rstdp_baseline_alpha")
_RECOVERY_STEP = 0.2    # 20% of gap per restoration tick
_HEALTHY_POLLS_TO_RECOVER = 3  # N consecutive non-problem polls before easing knobs


def recover_toward_baseline(baseline: dict, current: dict):
    """Nudge watchdog-managed knobs back toward their startup baseline.

    Only knobs whose current value is meaningfully off the baseline are
    restored — identical values skip the RPC. This runs AT MOST once per
    poll (no cooldown needed since steps are small)."""
    if not baseline:
        return
    for k in _MANAGED_KEYS:
        base = baseline.get(k)
        cur = current.get(k)
        if base is None or cur is None:
            continue
        if abs(cur - base) < 1e-6:
            continue  # already at baseline, no RPC needed
        new_val = cur + _RECOVERY_STEP * (base - cur)
        # Round to 6 sig figs to avoid spurious precision-noise logs.
        new_val = float(f"{new_val:.6g}")
        if abs(new_val - cur) < 1e-6:
            continue
        logger.info(f"recovery: {k} {cur:.6g} → {new_val:.6g} (baseline {base:.6g})")
        _tune(k, new_val)


# -----------------------------------------------------------------------------
# Main loop
# -----------------------------------------------------------------------------
def main():
    logger.info(
        f"SNN watchdog starting — socket={SOCKET_PATH} "
        f"poll={POLL_INTERVAL}s cooldown={COOLDOWN}s"
    )
    last_action_time = 0.0
    last_status_time = 0.0
    consecutive_errors = 0
    last_regime = None
    baseline_params: dict | None = None   # captured on first successful RPC
    calm_polls = 0                        # consecutive HEALTHY/TRANSIENT polls

    while True:
        now = time.time()
        try:
            pops_resp = rpc({"cmd": "snn_pop_stats"})
            params_resp = rpc({"cmd": "snn_tune_get"})
            pops = pops_resp.get("pops") or []
            params = params_resp.get("params") or {}
            consecutive_errors = 0
        except (ConnectionError, ConnectionRefusedError, FileNotFoundError,
                socket.timeout, OSError) as e:
            consecutive_errors += 1
            if consecutive_errors <= 3 or consecutive_errors % 30 == 0:
                logger.warning(
                    f"brain RPC failed ({consecutive_errors} consecutive): {e}"
                )
            time.sleep(POLL_INTERVAL)
            continue

        # First healthy read freezes the baseline for recovery.
        if baseline_params is None and params:
            baseline_params = {k: params.get(k) for k in _MANAGED_KEYS
                               if params.get(k) is not None}
            logger.info(f"baseline captured: {baseline_params}")

        regime, rationale = detect(pops, params)

        if regime != last_regime:
            logger.info(f"regime → {regime}: {rationale}")
            last_regime = regime

        # Count consecutive non-problem polls for the symmetric recovery.
        if regime in ("HEALTHY", "TRANSIENT"):
            calm_polls += 1
        else:
            calm_polls = 0

        if regime in ("DEAD_CASCADE", "FULL_COLLAPSE", "SATURATION", "WHIPLASH"):
            since = now - last_action_time
            if since >= COOLDOWN:
                logger.warning(
                    f"ACTING on {regime} ({rationale}) — last action {since:.0f}s ago"
                )
                act(regime, params)
                last_action_time = now
            else:
                logger.debug(
                    f"{regime} detected but in cooldown "
                    f"({since:.0f}s/{COOLDOWN:.0f}s)"
                )
        elif calm_polls >= _HEALTHY_POLLS_TO_RECOVER:
            # Symmetric: once the regime stabilizes, ease the crushed knobs
            # back toward their baseline. Prevents the monotonic ratchet-down
            # that causes permanent collapse after transient saturation.
            recover_toward_baseline(baseline_params, params)

        if now - last_status_time >= STATUS_PERIOD:
            target = params.get("target_rate", 0.03)
            b = classify(pops, target)
            logger.info(
                f"status regime={regime} pops={len(pops)} "
                f"dead={len(b['dead'])} quiet={len(b['quiet'])} "
                f"band={len(b['band'])} hot={len(b['hot'])} "
                f"saturated={len(b['saturated'])} input_rate={input_pop_rate(pops):.2%} "
                f"params={{rstdp_lr={params.get('rstdp_lr',0):.5f} "
                f"target={target:.3f} "
                f"max_dead={params.get('max_scale_dead',0):.3f}}}"
            )
            last_status_time = now

        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        logger.info("watchdog interrupted — exiting")
    except Exception as e:
        logger.exception(f"fatal error: {e}")
        sys.exit(1)
