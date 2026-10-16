#!/usr/bin/env python3
"""early_stop_watchdog.py — keep UTM early-stopping disabled on the live brain.

Stopgap for the 2026-04-28 fix bug: brain_daemon.py calls
utm_set_early_stopping_enabled(False) at init time, but UTM is created
lazily on the first learn_vector call, so the init-time disable warns
"UTM not initialized" and the C-side defaults (enable=true, patience=5000,
min_delta=1e-3) take over. With a noisy plateau the latch trips and
ANN/Adaptive freezes.

This watchdog polls utm_get_training_health every INTERVAL seconds; if it
sees early_stopped=1 it calls utm_reset_early_stopping +
utm_set_early_stopping_enabled(False) to clear it.

Run as nohup on the pod until brain_daemon.py is fixed + redeployed.
"""

import argparse
import json
import logging
import socket
import struct
import sys
import time

SOCK = "/var/run/athena/brain.sock"

logging.basicConfig(
    format="%(asctime)s [es-watchdog] %(levelname)s %(message)s",
    level=logging.INFO,
)
log = logging.getLogger("es-watchdog")


def call(cmd, **kwargs):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(10.0)
    s.connect(SOCK)
    msg = {"cmd": cmd, **kwargs}
    data = json.dumps(msg).encode()
    s.sendall(struct.pack(">I", len(data)) + data)
    hdr = s.recv(4)
    if len(hdr) < 4:
        s.close()
        return None
    n = struct.unpack(">I", hdr)[0]
    buf = b""
    while len(buf) < n:
        chunk = s.recv(min(65536, n - len(buf)))
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf)


def check_and_clear():
    h = call("utm_get_training_health")
    if not h or "health" not in h:
        log.warning("health probe returned no data: %s", h)
        return False
    es = h["health"].get("early_stopped", 0)
    if es:
        log.warning("early_stopped=1 detected — resetting + disabling")
        r1 = call("utm_reset_early_stopping")
        r2 = call("utm_set_early_stopping_enabled", enabled=False)
        log.info("reset=%s  disable=%s", r1, r2)
        return True
    return False


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--interval", type=int, default=120,
                   help="Polling interval in seconds (default 120)")
    args = p.parse_args()
    log.info("watchdog starting: sock=%s interval=%ds", SOCK, args.interval)
    while True:
        try:
            check_and_clear()
        except Exception as e:
            log.exception("poll raised: %s", e)
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
