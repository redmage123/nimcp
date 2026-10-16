"""Conductance-based (CB) weight-rescale checkpoint marker.

WHAT: Sidecar file (`<checkpoint>.cb_rescaled`, JSON) that records that a
      checkpoint's weights are in CB-rescaled form (factor applied) and
      pins the marker to that exact file via mtime comparison.

WHY:  CB rescale is in-memory only. brain.save() writes whatever's in
      memory to disk. Without a marker, a daemon that loads a saved-
      after-rescale checkpoint and then re-applies the load-time
      force-rescale would multiply by the rescale factor a second time
      (double-rescale → ×1/2500 effective drive → SNN silent). The
      marker breaks the double-apply: load-time rescale checks the
      marker first and skips if the loaded checkpoint is already
      rescaled.

USAGE: Both `scripts/brain_daemon.py` (daemon-side auto-checkpoint and
       admin RPCs) and `scripts/immerse_athena.py` (trainer snapshots)
       MUST call `write_marker(path, factor)` after every successful
       brain.save() when CB is enabled. The daemon's
       `_load_persistent_snn_tunes` calls `is_marked(path)` before
       deciding whether to force-rescale.

INVARIANT: Marker mtime == checkpoint mtime (within 1.0 s tolerance).
           If a save updates the checkpoint without updating the marker,
           the marker is treated as STALE and discarded — load path
           force-rescales (safer to over-rescale once than to risk
           silent SNN). Followers must always touch the marker after
           any brain.save().
"""

import json
import os
import time
import logging

_logger = logging.getLogger("cb_rescaled_marker")


def marker_path(checkpoint_path: str) -> str:
    """Path to the CB-rescaled sidecar for a given checkpoint.

    Resolves the symlink so that `athena_immersive.bin -> athena_s1_step4500.bin`
    yields a marker pinned to the *concrete* file, not the symlink. Both
    paths still resolve to the same marker via `os.path.realpath`.
    """
    real = os.path.realpath(checkpoint_path)
    return real + ".cb_rescaled"


def is_marked(checkpoint_path: str) -> bool:
    """True iff the checkpoint has a valid (mtime-matched) CB-rescaled marker.

    Returns False on any error (missing file, malformed JSON, mtime drift).
    Conservative: a corrupt/stale marker is treated as 'not rescaled' so
    the load path force-rescales rather than risk under-rescale.
    """
    if not checkpoint_path:
        return False
    try:
        sidecar = marker_path(checkpoint_path)
        if not os.path.exists(sidecar):
            return False
        with open(sidecar) as f:
            data = json.load(f)
        real = os.path.realpath(checkpoint_path)
        if not os.path.exists(real):
            return False
        actual_mtime = os.path.getmtime(real)
        recorded_mtime = float(data.get("checkpoint_mtime", 0.0))
        # 1.0 s tolerance covers fs mtime granularity and rsync slop.
        if abs(actual_mtime - recorded_mtime) >= 1.0:
            _logger.warning("CB marker mtime drift for %s: marker=%.2f file=%.2f "
                            "(diff %.2fs) — treating as stale, will re-rescale",
                            checkpoint_path, recorded_mtime, actual_mtime,
                            abs(actual_mtime - recorded_mtime))
            return False
        return True
    except Exception as e:
        _logger.warning("CB marker check failed for %s: %s — assuming not rescaled",
                        checkpoint_path, e)
        return False


def write_marker(checkpoint_path: str, factor: float) -> bool:
    """Write a CB-rescaled marker for a freshly-saved-while-CB-rescaled checkpoint.

    Call this AFTER brain.save() completes successfully and the file is at
    its final path (post-rename). If called before the rename, the marker's
    mtime won't match.

    Returns True on success, False on any error (caller should log if it
    cares — silent failure means the next load will force-rescale, which
    is safe).
    """
    if not checkpoint_path:
        return False
    try:
        real = os.path.realpath(checkpoint_path)
        if not os.path.exists(real):
            _logger.warning("CB marker write skipped: %s does not exist", real)
            return False
        sidecar = real + ".cb_rescaled"
        payload = {
            "factor": float(factor),
            "checkpoint_mtime": os.path.getmtime(real),
            "checkpoint_path": real,
            "rescaled_at": time.time(),
        }
        # Atomic write: write to .tmp then rename.
        tmp = sidecar + ".tmp"
        with open(tmp, "w") as f:
            json.dump(payload, f, indent=2)
        os.replace(tmp, sidecar)
        return True
    except Exception as e:
        _logger.warning("CB marker write failed for %s: %s", checkpoint_path, e)
        return False


def clear_marker(checkpoint_path: str) -> None:
    """Delete the CB-rescaled marker for a checkpoint (operator override).

    Used when manually clearing CB state, e.g. before disabling CB and
    rolling back to current-mode weights via inverse rescale.
    """
    if not checkpoint_path:
        return
    try:
        sidecar = marker_path(checkpoint_path)
        if os.path.exists(sidecar):
            os.remove(sidecar)
    except Exception as e:
        _logger.warning("CB marker clear failed for %s: %s", checkpoint_path, e)
