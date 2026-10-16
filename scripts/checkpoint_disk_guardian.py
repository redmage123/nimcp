#!/usr/bin/env python3
"""checkpoint_disk_guardian.py — out-of-process disk safety net for brain checkpoints.

Stopgap until the brain daemon is restarted to pick up the in-process auto-save
fix in brain_daemon.py (quota-aware free check + orphan cleanup + rolling-2).
This script polls the checkpoint directory every POLL_SECONDS, computes usage
against NIMCP_CHECKPOINT_QUOTA_GB, and prunes:

  1. Orphan .tmp files (anywhere in dir, any age)
  2. Orphan sidecar shards whose parent .bin is missing
     (e.g. a .snn left behind by a failed save where the .bin never wrote)
  3. Excess athena_auto_*.bin sets — keeps the N=KEEP_AUTO most recent;
     deletes older sets (core .bin + all .bin.* sidecars together)
  4. Headroom enforcement — if free space (quota - used) drops below
     MIN_FREE_GB, drops *additional* auto sets (oldest first) until it recovers

Designed to be safe to run alongside the brain daemon's own save loop:
  - Never touches athena_immersive.bin or athena_immersive.bin.* (the live
    checkpoint set)
  - Never touches athena_manual_step*.bin* (manual saves are intentional)
  - Never touches athena_s*_step*.bin* (per-step exports are intentional)
  - Only prunes athena_auto_*.bin and unconditional .tmp orphans

Usage (one-shot):
  python3 checkpoint_disk_guardian.py --once

Usage (daemon mode):
  python3 checkpoint_disk_guardian.py --interval 60

Env:
  NIMCP_CHECKPOINT_DIR        default /workspace/nimcp/checkpoints/athena
  NIMCP_CHECKPOINT_QUOTA_GB   default 75
  NIMCP_CHECKPOINT_MIN_FREE_GB default 20
  NIMCP_CHECKPOINT_KEEP_AUTO  default 2
"""

import argparse
import glob
import logging
import os
import sys
import time

CHECKPOINT_DIR = os.environ.get("NIMCP_CHECKPOINT_DIR",
                                "/workspace/nimcp/checkpoints/athena")
QUOTA_GB = float(os.environ.get("NIMCP_CHECKPOINT_QUOTA_GB", "75"))
MIN_FREE_GB = float(os.environ.get("NIMCP_CHECKPOINT_MIN_FREE_GB", "20"))
KEEP_AUTO = int(os.environ.get("NIMCP_CHECKPOINT_KEEP_AUTO", "2"))

# Sidecar extensions written by brain_save (used for orphan detection +
# bundle-aware deletion).
SIDECAR_EXTS = (
    "snn", "lnn", "cnn", "meta", "tokenizer", "mirror_neurons",
    "executive", "cortex_visual", "cortex_audio", "cortex_speech",
    "cortex_somato", "cb_rescaled", "temperature",
)

# Files this script must NEVER touch (live + intentional).
PROTECTED_PREFIXES = (
    "athena_immersive.bin",
    "athena_manual_",
    "athena_s",  # per-step exports e.g. athena_s0_step15450.bin
)

logging.basicConfig(
    format="%(asctime)s [guardian] %(levelname)s %(message)s",
    level=logging.INFO,
)
log = logging.getLogger("guardian")


def is_protected(name):
    """Return True if filename is in the never-touch set."""
    return any(name.startswith(p) for p in PROTECTED_PREFIXES)


def dir_used_gb(path):
    """Total bytes used under path, expressed in GiB."""
    total = 0
    for root, _, files in os.walk(path):
        for f in files:
            try:
                total += os.path.getsize(os.path.join(root, f))
            except OSError:
                pass
    return total / (1024 ** 3)


def free_gb():
    """Quota headroom: quota - used."""
    return QUOTA_GB - dir_used_gb(CHECKPOINT_DIR)


def remove_safe(path):
    try:
        os.remove(path)
        return True
    except OSError as e:
        log.warning("rm %s failed: %s", path, e)
        return False


def prune_tmp_orphans():
    """Drop any *.tmp file in the checkpoint dir."""
    n = 0
    for p in glob.glob(os.path.join(CHECKPOINT_DIR, "*.tmp")):
        if remove_safe(p):
            n += 1
    for p in glob.glob(os.path.join(CHECKPOINT_DIR, "*.bin.tmp.*")):
        if remove_safe(p):
            n += 1
    if n:
        log.info("pruned %d .tmp orphan(s)", n)
    return n


def prune_sidecar_orphans():
    """Drop sidecar files whose parent .bin is missing.

    Skips protected names so we never touch athena_immersive.* or
    athena_manual_*.* even if their .bin somehow temporarily disappears.
    """
    n = 0
    for ext in SIDECAR_EXTS:
        for sc in glob.glob(os.path.join(CHECKPOINT_DIR, f"*.bin.{ext}")):
            parent_bin = sc[: -(len(ext) + 1)]  # strip ".<ext>"
            if is_protected(os.path.basename(parent_bin)):
                continue
            if not os.path.exists(parent_bin):
                if remove_safe(sc):
                    n += 1
    if n:
        log.info("pruned %d orphan sidecar shard(s)", n)
    return n


def auto_sets():
    """Return list of (mtime, base_path) for athena_auto_*.bin core files."""
    out = []
    for p in glob.glob(os.path.join(CHECKPOINT_DIR, "athena_auto_*.bin")):
        # Filter to core .bin only (not .bin.snn etc.)
        if any(p.endswith(f".bin.{ext}") for ext in SIDECAR_EXTS):
            continue
        try:
            out.append((os.path.getmtime(p), p))
        except OSError:
            pass
    out.sort()  # oldest first
    return out


def remove_auto_set(base):
    """Remove a full athena_auto_*.bin set (core + all sidecars)."""
    files = [base] + glob.glob(base + ".*")
    n = 0
    for p in files:
        if remove_safe(p):
            n += 1
    log.info("dropped auto-set %s (%d files)",
             os.path.basename(base), n)
    return n


def prune_excess_auto():
    """Cap rolling auto-snapshots at KEEP_AUTO."""
    sets = auto_sets()
    n_drop = max(0, len(sets) - KEEP_AUTO)
    for _, base in sets[:n_drop]:
        remove_auto_set(base)
    if n_drop:
        log.info("kept %d/%d auto-sets (KEEP_AUTO=%d)",
                 KEEP_AUTO, len(sets), KEEP_AUTO)
    return n_drop


def enforce_headroom():
    """If free_gb < MIN_FREE_GB, drop oldest auto-sets until recovered."""
    free = free_gb()
    if free >= MIN_FREE_GB:
        return 0
    log.warning("headroom %.1f GB < %.1f GB — pruning extra auto-sets",
                free, MIN_FREE_GB)
    sets = auto_sets()
    dropped = 0
    for _, base in sets:
        if free_gb() >= MIN_FREE_GB:
            break
        remove_auto_set(base)
        dropped += 1
    free = free_gb()
    if free < MIN_FREE_GB:
        log.error("STILL %.1f GB free after dropping %d auto-set(s) — "
                  "manual intervention required",
                  free, dropped)
    else:
        log.info("recovered to %.1f GB free after dropping %d auto-set(s)",
                 free, dropped)
    return dropped


def run_once():
    if not os.path.isdir(CHECKPOINT_DIR):
        log.error("checkpoint dir missing: %s", CHECKPOINT_DIR)
        return False
    used = dir_used_gb(CHECKPOINT_DIR)
    free = QUOTA_GB - used
    log.info("dir=%s used=%.1f GB quota=%.1f GB free=%.1f GB",
             CHECKPOINT_DIR, used, QUOTA_GB, free)
    prune_tmp_orphans()
    prune_sidecar_orphans()
    prune_excess_auto()
    enforce_headroom()
    used = dir_used_gb(CHECKPOINT_DIR)
    log.info("post-sweep used=%.1f GB free=%.1f GB",
             used, QUOTA_GB - used)
    return True


def run_loop(interval):
    log.info("guardian starting: dir=%s quota=%.0f GB min_free=%.0f GB "
             "keep_auto=%d interval=%ds",
             CHECKPOINT_DIR, QUOTA_GB, MIN_FREE_GB, KEEP_AUTO, interval)
    while True:
        try:
            run_once()
        except Exception as e:
            log.exception("sweep raised: %s", e)
        time.sleep(interval)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--once", action="store_true",
                   help="Run a single sweep and exit")
    p.add_argument("--interval", type=int, default=60,
                   help="Polling interval in seconds (daemon mode)")
    args = p.parse_args()
    if args.once:
        ok = run_once()
        sys.exit(0 if ok else 1)
    else:
        run_loop(args.interval)


if __name__ == "__main__":
    main()
