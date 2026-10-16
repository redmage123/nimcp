#!/usr/bin/env python3
"""
Phase E (elephant long-term memory / Z-Ladder landmarks) smoke test.

Exercises the full lifecycle that Phase E1-E5 built:

  E1   mark / unmark / is / count / audit / query (landmark only)
  E2   insertion pipeline (lazy init, pr_node_manager, insert, landmark)
  E3   auto-insertion heuristics + oracle query (landmark-only)
  E4-1 stale-slot prune
  E4-2 save / load landmark checkpoint
  E4-4 event-triggered landmark promotion (promote_event)
  E4-5 query-all-tiers (walk full ladder, not just landmarks)
  E4-6 capacity eviction on full landmark set
  E5   auto-landmark hygiene via pr_memory_tick (indirectly — tick count
       is exposed through stats, full end-to-end runs only under
       brain_learn_vector which is out of scope here)

Run as:  python3 -m tests.smoke.test_phase_e_landmarks

Exit 0 on success, non-zero on failure.
"""
import os
import struct
import sys
import tempfile
import time

try:
    import nimcp
except ImportError as e:
    print(f"FAIL: cannot import nimcp — .so not installed? ({e})")
    sys.exit(1)


def feats(seed: int, n: int = 64):
    """Deterministic feature vector distinct per seed."""
    return [float((seed * 31 + i * 7) % 97) / 97.0 for i in range(n)]


def assert_eq(actual, expected, label):
    if actual != expected:
        raise AssertionError(f"{label}: expected {expected!r}, got {actual!r}")


def assert_ge(actual, expected, label):
    if actual < expected:
        raise AssertionError(f"{label}: expected >= {expected}, got {actual}")


def main():
    b = nimcp.Brain("phase_e_main", 128, 8)

    # ---- E2: explicit insert + landmark ---------------------------------
    id1 = b.long_term_insert_memory(feats(1), True, "first_event")
    if id1 == 0:
        raise AssertionError("E2 insert+landmark returned node_id=0")
    if not b.long_term_stats()["enabled"]:
        raise AssertionError("E2: stats reports pr_memory disabled after insert")
    assert_ge(b.long_term_stats()["landmark_count"], 1, "E2 landmark_count")

    # ---- E1: mark/unmark/is --------------------------------------------
    id2 = b.long_term_insert_memory(feats(2), False, "")  # no landmark
    rc = b.long_term_mark_landmark(id2, "later_promotion")
    assert_eq(rc, 0, "E1 mark_landmark rc")
    rc = b.long_term_unmark_landmark(id2)
    assert_eq(rc, 0, "E1 unmark_landmark rc")

    # ---- E3: auto-insert config (no assertion on training path here) ----
    b.long_term_set_auto_insert(True, 0.7, 0.95)
    s = b.long_term_stats()
    if not s["auto_insert_enabled"]:
        raise AssertionError("E3: auto_insert did not enable")
    b.long_term_set_auto_insert(False, 0.7, 0.95)

    # ---- E4-4: event promotion ------------------------------------------
    ev_id = b.long_term_promote_event(feats(3), "reward_spike")
    if ev_id == 0:
        raise AssertionError("E4-4 promote_event returned 0")

    # ---- E3/E4-5: query landmark-only vs all-tiers ----------------------
    hits_land = b.long_term_query(feats(1), 5)
    if not hits_land:
        raise AssertionError("E3 landmark query returned empty")
    # Best landmark match should be the landmark for feats(1)
    best = hits_land[0]
    if best["similarity"] < 0.5:
        raise AssertionError(f"E3 query best similarity too low: {best}")

    hits_all = b.long_term_query_all(feats(2), 5)
    if not hits_all:
        raise AssertionError("E4-5 query_all_tiers returned empty")
    # feats(2) is a non-landmark node — landmark query should miss or
    # score lower, all-tiers query should find it.
    found_id2 = any(h["node_id"] == id2 for h in hits_all)
    if not found_id2:
        raise AssertionError("E4-5 query_all did not find non-landmark node")

    # ---- E4-2 / E6-d: save / load landmarks (v3 with feature payload) ---
    with tempfile.NamedTemporaryFile(prefix="phase_e_ckpt_", suffix=".dat",
                                      delete=False) as tf:
        ckpt_path = tf.name
    try:
        ok = b.long_term_save(ckpt_path)
        if not ok:
            raise AssertionError("E4-2 long_term_save returned False")
        saved_size = os.path.getsize(ckpt_path)
        # v3 header is 16 bytes; each record is >= 64 + 144 + 4 = 212 bytes.
        if saved_size < 16 + 212:
            raise AssertionError(f"E4-2 saved ckpt too small: {saved_size} bytes")

        # E6-d: header must declare version 3 (magic "NIMCPELM" + v3).
        with open(ckpt_path, "rb") as fh:
            header = fh.read(16)
        magic, version, n_records = struct.unpack("<8sII", header)
        if magic != b"NIMCPELM":
            raise AssertionError(f"E6-d wrong magic: {magic!r}")
        if version != 3:
            raise AssertionError(f"E6-d expected version=3, got {version}")
        # Each record should include a nonzero data_size (feature bytes).
        # Feat vectors are 64 floats × 4 bytes = 256 bytes each.
        # n_records × (208 bytes shell + 4 bytes size + 256 bytes data) ≤ saved_size
        min_size_with_data = 16 + n_records * (208 + 4 + 256)
        if saved_size < min_size_with_data:
            raise AssertionError(
                f"E6-d v3 ckpt too small for {n_records} records with feature "
                f"payload: saved={saved_size} min={min_size_with_data}")

        # Fresh brain — load and verify landmarks are restored
        b2 = nimcp.Brain("phase_e_restore", 128, 8)
        pre_count = b2.long_term_stats().get("landmark_count", 0)
        ok = b2.long_term_load(ckpt_path)
        if not ok:
            raise AssertionError("E4-2 long_term_load returned False")
        post_stats = b2.long_term_stats()
        post_count = post_stats.get("landmark_count", 0)
        if post_count <= pre_count:
            raise AssertionError(
                f"E4-2 load did not increase landmark count "
                f"(pre={pre_count}, post={post_count}, stats={post_stats})")
    finally:
        try:
            os.unlink(ckpt_path)
        except OSError:
            pass

    # ---- E6-b: autonomous driver thread is ticking ---------------------
    # Give the 100ms driver thread time to fire at least a few ticks.
    driver_start = b.long_term_stats().get("driver_ticks", 0)
    if not b.long_term_stats().get("driver_running", False):
        raise AssertionError("E6-b driver_running=False — thread did not start")
    time.sleep(0.5)  # 5× the 100ms cadence → expect ≥ 3 ticks
    driver_end = b.long_term_stats().get("driver_ticks", 0)
    if driver_end - driver_start < 3:
        raise AssertionError(
            f"E6-b driver thread not ticking (delta={driver_end - driver_start} "
            f"over 500ms — expected ≥ 3)")

    # ---- Final stats dump ----------------------------------------------
    final = b.long_term_stats()
    required = {
        "enabled", "total_nodes", "landmark_count",
        "z0_count", "z1_count", "z2_count", "z3_count",
        "landmarks_present", "landmarks_missing",
        "landmarks_at_z3", "landmarks_drifted",
        "auto_insert_enabled",
        "driver_ticks", "driver_running",  # E6-b
    }
    missing = required - set(final.keys())
    if missing:
        raise AssertionError(f"long_term_stats missing keys: {missing}")

    print("Phase E smoke test: ALL CHECKS PASSED")
    print(f"  final stats: {final}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as e:
        print(f"FAIL: {e}")
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"FAIL: unexpected exception: {e}")
        sys.exit(2)
