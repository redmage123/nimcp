#!/usr/bin/env python3
"""
snn_tune.py — Real-time SNN parameter tuning + live population diagnostics.

No brain restart needed — parameters update in place on the next homeostatic
or R-STDP call after the RPC completes.

Usage:
    # Show current tunable values
    ./snn_tune.py --show

    # Show per-population firing rate snapshot (colored hot/cold/deadband)
    ./snn_tune.py --pops

    # Set a single parameter
    ./snn_tune.py rstdp_lr 0.00015

    # Bulk set (space-separated name=value pairs)
    ./snn_tune.py --set rstdp_lr=0.00015 rstdp_baseline_alpha=0.002

    # Watch mode: show pops + params every 2 seconds until Ctrl-C
    ./snn_tune.py --watch [--interval 2]

Known tunable names:
    rstdp_lr                      R-STDP learning rate
    rstdp_baseline_alpha          reward baseline EMA alpha
    target_rate                   homeostatic target firing fraction
    target_rate_input             homeostatic target firing fraction for input tier
    homeo_min_scale               scale-down floor (clamp <1.0)
    homeo_max_scale               normal scale-up ceiling
    max_scale_dead                escape scale-up for pops far below target
    dead_threshold                rate fraction-of-target below which max_scale_dead kicks in
    metabolic_cap                 sum(|w|) ≤ factor × fan_in
    noise_rate_hz                 Poisson background noise rate (Hz)
    noise_pulse_mv                background noise pulse amplitude (mV)
    intrinsic_alpha               intrinsic-reward EMA alpha

  Biophysical stability (Wave A + B1):
    anti_reward_enabled           1.0 = on, 0.0 = off
    anti_reward_threshold_ratio   rate / target at which anti-reward kicks in (default 2.0)
    anti_reward_gain              magnitude of negative reward at saturation (default 0.5)
    depression_inc                per-spike short-term depression increment (0..1)
    depression_tau_ms             short-term depression recovery time constant (ms)
    depression_cap                max short-term depression (0..1)
    ahp_enabled                   after-hyperpolarization adaptation on/off
    ahp_tau_ms                    AHP decay time constant (ms)
    ahp_gain_mv                   AHP per-spike kick (mV, hyperpolarizing)
    pump_enabled                  Na/K pump (slow) adaptation on/off
    pump_tau_ms                   pump decay time constant (ms, slower than AHP)
    pump_gain_mv                  pump per-spike kick (mV, hyperpolarizing)
    basket_enabled                fast-spiking inhibitory basket pool on/off
    basket_fraction               basket cell fraction per population (0.01..0.5)
    noise_ei_ratio                fraction of background noise that is inhibitory (0..1)

  Substrate adapter (Phase 1 biological-substrate wiring):
    substrate_enabled             1.0 = substrate modulates SNN dynamics, 0 = off
    substrate_update_period       steps between substrate effect recomputations (default 10)
    substrate_spike_dropout_on    1.0 = ATP-dependent spike reliability, 0 = off
    substrate_plasticity_mod_on   1.0 = ATP-modulated R-STDP LR, 0 = off
"""
import argparse
import json
import os
import socket
import struct
import sys
import time

SOCKET_PATH = os.environ.get("BRAIN_SOCKET", "/var/run/athena/brain.sock")


def _rpc(cmd_dict: dict, timeout: float = 5.0) -> dict:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(SOCKET_PATH)
    payload = json.dumps(cmd_dict).encode()
    s.sendall(struct.pack(">I", len(payload)) + payload)
    size = struct.unpack(">I", _recv_n(s, 4))[0]
    resp = json.loads(_recv_n(s, size).decode())
    s.close()
    return resp


def _recv_n(s, n):
    buf = b""
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed mid-message")
        buf += chunk
    return buf


def show_params():
    r = _rpc({"cmd": "snn_tune_get"})
    params = r.get("params") or {}
    if not params:
        print("(no params returned — brain not responding or SNN not initialized)")
        return 1
    w = max(len(k) for k in params)
    print(f"{'name':<{w}}  value")
    print(f"{'-' * w}  {'-' * 10}")
    for k in sorted(params):
        print(f"{k:<{w}}  {params[k]:.6g}")
    return 0


def set_param(name: str, value: float):
    r = _rpc({"cmd": "snn_tune", "name": name, "value": value})
    if "error" in r:
        print(f"ERROR: {r['error']}", file=sys.stderr)
        return 1
    print(f"set {name} = {value}")
    return 0


def _colorize(rate: float, target: float, dead: float) -> str:
    # ANSI color for terminal output
    frac = rate / target if target > 0 else 0
    if rate < dead * target:
        return f"\033[31m{rate:.4f}\033[0m"  # red — dead
    if 0.85 <= frac <= 1.15:
        return f"\033[32m{rate:.4f}\033[0m"  # green — deadband
    if frac > 2.0:
        return f"\033[33m{rate:.4f}\033[0m"  # yellow — hot
    return f"{rate:.4f}"


def show_pops(colorize: bool = True):
    r = _rpc({"cmd": "snn_pop_stats"})
    p = _rpc({"cmd": "snn_tune_get"})
    target = (p.get("params") or {}).get("target_rate", 0.03)
    dead = (p.get("params") or {}).get("dead_threshold", 0.1)
    pops = r.get("pops") or []
    if not pops:
        print("(no pops — brain has no SNN, or daemon unresponsive)")
        return 1
    counts = {"dead": 0, "quiet": 0, "near": 0, "band": 0, "over": 0, "hot": 0, "saturated": 0}
    print(f"{'#':>3}  {'name':<25}  {'n':>7}  {'rate':>8}  {'samples':>8}")
    print(f"{'-' * 3}  {'-' * 25}  {'-' * 7}  {'-' * 8}  {'-' * 8}")
    for i, pop in enumerate(pops):
        rate = pop.get("firing_rate_ema", 0.0)
        rate_str = _colorize(rate, target, dead) if colorize else f"{rate:.4f}"
        name = pop.get("name", "?")
        n = pop.get("n_neurons", 0)
        samples = pop.get("rate_samples", 0)
        print(f"{i:>3}  {name:<25}  {n:>7d}  {rate_str:>8}  {samples:>8d}")
        # Bucket for summary
        if rate < 0.005:
            counts["dead"] += 1
        elif rate < 0.02:
            counts["quiet"] += 1
        elif rate < 0.025:
            counts["near"] += 1
        elif rate < 0.035:
            counts["band"] += 1
        elif rate < 0.05:
            counts["over"] += 1
        elif rate < 0.10:
            counts["hot"] += 1
        else:
            counts["saturated"] += 1
    print()
    print(
        f"  dead(<0.5%): {counts['dead']}  quiet(0.5-2%): {counts['quiet']}  "
        f"near(2-2.5%): {counts['near']}  IN-BAND(2.5-3.5%): {counts['band']}  "
        f"over(3.5-5%): {counts['over']}  hot(5-10%): {counts['hot']}  "
        f"saturated(>10%): {counts['saturated']}"
    )
    print(f"  target={target:.3g}  dead-threshold={dead:.2g}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--show", action="store_true", help="show all current parameters")
    parser.add_argument("--pops", action="store_true", help="show per-population firing rates")
    parser.add_argument("--watch", action="store_true", help="refresh pops + params in a loop")
    parser.add_argument("--interval", type=float, default=2.0, help="watch refresh seconds")
    parser.add_argument("--no-color", action="store_true", help="disable ANSI colors")
    parser.add_argument("--set", nargs="+", metavar="NAME=VALUE", help="bulk set")
    parser.add_argument("positional", nargs="*", help="NAME VALUE (single set)")
    args = parser.parse_args()

    if args.watch:
        try:
            while True:
                os.system("clear")
                print(f"=== {time.strftime('%Y-%m-%d %H:%M:%S')} — SNN tune + pops ===\n")
                show_params()
                print()
                show_pops(colorize=not args.no_color)
                time.sleep(args.interval)
        except KeyboardInterrupt:
            return 0

    if args.show:
        return show_params()
    if args.pops:
        return show_pops(colorize=not args.no_color)
    if args.set:
        rc = 0
        for kv in args.set:
            if "=" not in kv:
                print(f"bad form: {kv!r}; expected NAME=VALUE", file=sys.stderr)
                rc = 2
                continue
            name, value = kv.split("=", 1)
            rc = set_param(name.strip(), float(value.strip())) or rc
        return rc
    if len(args.positional) == 2:
        return set_param(args.positional[0], float(args.positional[1]))
    if not args.positional:
        # Default: show both
        show_params()
        print()
        show_pops(colorize=not args.no_color)
        return 0
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
