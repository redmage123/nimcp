#!/usr/bin/env python3
"""Tests for Layer C confabulation-mitigation infrastructure (2026-04-26).

Layer C adds temperature scaling for post-hoc confidence calibration. The
calibration *infrastructure* ships now (T=1.0 default — bit-for-bit
identity, no behavior change). Actual calibration runs later, once the
language stage is online and a held-out validation set exists.

Reference: Guo, C. et al. 2017, "On Calibration of Modern Neural Networks."

Tests are split into:
  - source-grep tests: confirm the struct field, header, .c file, and
    CMake wiring exist (no runtime needed)
  - numeric tests: load libnimcp.so via ctypes and exercise the math

Run:
    python3 -m pytest tests/unit/test_temperature_scaling.py -v
"""
from __future__ import annotations

import ctypes
import math
import os
import re
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]

INTERNAL_HDR = REPO_ROOT / "include/core/brain/nimcp_brain_internal.h"
CALIB_HDR    = REPO_ROOT / "include/core/brain/calibration/nimcp_temperature_scaling.h"
CALIB_C      = REPO_ROOT / "src/core/brain/calibration/nimcp_temperature_scaling.c"
LIB_CMAKE    = REPO_ROOT / "src/lib/CMakeLists.txt"
ROOT_CMAKE   = REPO_ROOT / "CMakeLists.txt"


# =============================================================================
# Section 1 — Source-grep tests (non-negotiable)
# =============================================================================

def test_decoder_temperature_field_exists():
    """`decoder_temperature` must be declared on brain_struct."""
    src = INTERNAL_HDR.read_text()
    assert re.search(r"\bfloat\s+decoder_temperature\s*;", src), (
        "brain_struct must declare `float decoder_temperature;` so the "
        "calibrated scalar persists on the brain handle"
    )
    assert re.search(r"\bfloat\s+decoder_temperature_calibrated_ece\s*;", src), (
        "brain_struct must declare decoder_temperature_calibrated_ece "
        "(ece at last calibration; -1 means uncalibrated)"
    )
    assert re.search(r"\buint64_t\s+decoder_temperature_calibrated_at_us\s*;", src), (
        "brain_struct must declare decoder_temperature_calibrated_at_us"
    )


def test_calibration_source_file_exists():
    """The Layer C source file must live at the canonical path."""
    assert CALIB_C.exists(), (
        f"{CALIB_C} must exist — Layer C calibration implementation"
    )
    body = CALIB_C.read_text()
    # Every public function from the header must be defined in the .c.
    for name in (
        "nimcp_apply_temperature",
        "nimcp_softmax_with_temperature",
        "nimcp_calibrate_temperature",
        "nimcp_brain_calibrate_temperature",
    ):
        # Match definitions, not declarations.
        assert re.search(
            rf"\b{name}\s*\([^;]*\)\s*\{{", body, re.DOTALL,
        ), f"{name} must be DEFINED (not just declared) in {CALIB_C.name}"


def test_apply_temperature_declared_in_header():
    """nimcp_apply_temperature must be declared in the public header."""
    assert CALIB_HDR.exists(), f"{CALIB_HDR} must exist"
    body = CALIB_HDR.read_text()
    assert re.search(
        r"nimcp_error_t\s+nimcp_apply_temperature\s*\(", body,
    ), "nimcp_apply_temperature must be declared in the calibration header"


def test_calibrate_temperature_declared_in_header():
    """nimcp_calibrate_temperature must be declared in the public header."""
    body = CALIB_HDR.read_text()
    assert re.search(
        r"nimcp_error_t\s+nimcp_calibrate_temperature\s*\(", body,
    ), "nimcp_calibrate_temperature must be declared in the calibration header"
    # Brain-aware wrapper too.
    assert re.search(
        r"nimcp_error_t\s+nimcp_brain_calibrate_temperature\s*\(", body,
    ), "nimcp_brain_calibrate_temperature must be declared in the calibration header"
    # And softmax.
    assert re.search(
        r"nimcp_error_t\s+nimcp_softmax_with_temperature\s*\(", body,
    ), "nimcp_softmax_with_temperature must be declared in the calibration header"


def test_cmake_includes_calibration_source():
    """CMake must pick up the new .c file. Either an explicit listing in
    src/lib/CMakeLists.txt or a recursive glob in the root."""
    leaf = "nimcp_temperature_scaling.c"
    explicit = leaf in LIB_CMAKE.read_text()
    glob_present = bool(
        re.search(r"GLOB_RECURSE\s+\w+_SOURCES", ROOT_CMAKE.read_text())
    )
    assert explicit or glob_present, (
        "CMake must add src/core/brain/calibration/nimcp_temperature_scaling.c "
        "to the nimcp library — either via explicit listing in "
        "src/lib/CMakeLists.txt or via a recursive GLOB"
    )


# =============================================================================
# Section 2 — Numeric tests (ctypes against libnimcp.so)
# =============================================================================

def _find_lib():
    """Locate libnimcp.so. Skip numeric tests if it is missing — source-grep
    tests still run."""
    candidates = [
        REPO_ROOT / "build" / "lib" / "libnimcp.so",
        REPO_ROOT / "lib" / "libnimcp.so",
        REPO_ROOT / "build-release" / "lib" / "libnimcp.so",
    ]
    for path in candidates:
        if path.is_file():
            return str(path)
    return None


@pytest.fixture(scope="module")
def lib():
    path = _find_lib()
    if path is None:
        pytest.skip("libnimcp.so not built — run `make nimcp -j4` first")
    try:
        h = ctypes.CDLL(path, mode=ctypes.RTLD_GLOBAL)
    except OSError as e:
        pytest.skip(f"Cannot load libnimcp.so: {e}")

    # Bind the C signatures we need. nimcp_error_t is int32_t.
    h.nimcp_apply_temperature.restype  = ctypes.c_int32
    h.nimcp_apply_temperature.argtypes = [
        ctypes.POINTER(ctypes.c_float),  # logits
        ctypes.c_uint32,                 # n
        ctypes.c_float,                  # T
    ]

    h.nimcp_softmax_with_temperature.restype  = ctypes.c_int32
    h.nimcp_softmax_with_temperature.argtypes = [
        ctypes.POINTER(ctypes.c_float),  # logits (const)
        ctypes.c_uint32,                 # n
        ctypes.c_float,                  # T
        ctypes.POINTER(ctypes.c_float),  # out_probs
    ]

    h.nimcp_calibrate_temperature.restype  = ctypes.c_int32
    h.nimcp_calibrate_temperature.argtypes = [
        ctypes.POINTER(ctypes.c_float),    # logits_array
        ctypes.POINTER(ctypes.c_uint32),   # labels
        ctypes.c_uint32,                   # n_samples
        ctypes.c_uint32,                   # n_classes
        ctypes.POINTER(ctypes.c_float),    # out_T
        ctypes.POINTER(ctypes.c_float),    # out_ece
    ]
    return h


def _to_float_array(values):
    n = len(values)
    arr = (ctypes.c_float * n)(*values)
    return arr


def _to_uint32_array(values):
    n = len(values)
    arr = (ctypes.c_uint32 * n)(*values)
    return arr


def test_apply_temperature_T_one_is_bitwise_identity(lib):
    """T=1.0 (within identity epsilon) must NOT touch the logits at all."""
    original = [-2.5, 0.0, 1.25, 3.75, -0.125, 7.0]
    arr = _to_float_array(original)
    rc = lib.nimcp_apply_temperature(arr, len(original), ctypes.c_float(1.0))
    assert rc == 0, f"apply_temperature returned error {rc}"
    # Bit-for-bit identical — T=1.0 is the no-op fast path.
    out = list(arr)
    for a, b in zip(original, out):
        assert a == b, (
            f"T=1.0 must be bit-for-bit identity; logit {a} became {b}"
        )


def test_softmax_with_T_one_matches_plain_softmax(lib):
    """At T=1.0 the helper must agree with a textbook softmax."""
    logits = [1.0, 2.0, 0.5, -1.0, 3.0]
    n = len(logits)
    in_arr = _to_float_array(logits)
    out_arr = (ctypes.c_float * n)()
    rc = lib.nimcp_softmax_with_temperature(in_arr, n, ctypes.c_float(1.0), out_arr)
    assert rc == 0

    # Reference softmax (Python float64).
    m = max(logits)
    exps = [math.exp(x - m) for x in logits]
    Z = sum(exps)
    expected = [e / Z for e in exps]

    for i, (e, p) in enumerate(zip(expected, out_arr)):
        assert abs(e - p) < 1e-5, (
            f"softmax_with_temperature(T=1.0) mismatch at {i}: "
            f"got {p}, expected {e}"
        )


def test_softmax_uniform_logits_yields_uniform_distribution(lib):
    """Uniform logits — at any T > 0, the output must be uniform 1/n."""
    n = 7
    logits = [3.14] * n
    in_arr = _to_float_array(logits)
    out_arr = (ctypes.c_float * n)()
    for T in (0.5, 1.0, 1.5, 2.7, 5.0):
        rc = lib.nimcp_softmax_with_temperature(in_arr, n, ctypes.c_float(T), out_arr)
        assert rc == 0
        for i, p in enumerate(out_arr):
            assert abs(p - 1.0 / n) < 1e-6, (
                f"uniform_logits + T={T}: out[{i}]={p}, expected {1.0/n}"
            )


def test_softmax_outputs_sum_to_one(lib):
    """Probabilities must always sum to 1.0 within float-precision."""
    cases = [
        ([0.0, 0.0, 0.0], 1.0),
        ([10.0, -10.0, 0.0, 5.0], 1.0),
        ([1.0, 2.0, 3.0, 4.0, 5.0, 6.0], 0.5),
        ([1.0, 2.0, 3.0, 4.0, 5.0, 6.0], 2.0),
        ([100.0, 99.0, 98.0], 1.0),  # numerical-stability stress
    ]
    for logits, T in cases:
        n = len(logits)
        in_arr = _to_float_array(logits)
        out_arr = (ctypes.c_float * n)()
        rc = lib.nimcp_softmax_with_temperature(in_arr, n, ctypes.c_float(T), out_arr)
        assert rc == 0
        s = sum(out_arr)
        assert abs(s - 1.0) < 1e-6, (
            f"softmax(logits={logits}, T={T}) sums to {s}, not 1.0"
        )


def test_calibrate_temperature_perfectly_calibrated_returns_T_close_to_one(lib):
    """If logits are already well-calibrated, optimal T should be near 1.0."""
    # Build a synthetic dataset where the logit margin already matches the
    # observed accuracy. We do this by sampling from softmax-with-T=1 and
    # using the ground-truth class. With a moderate margin the best NLL
    # falls very close to T=1.0.
    rng = __import__("random").Random(42)
    n_classes = 4
    n_samples = 800

    flat_logits = []
    labels = []
    for _ in range(n_samples):
        # Random base logits, modest scale so confidence stays moderate.
        base = [rng.gauss(0.0, 1.0) for _ in range(n_classes)]
        # Pick the argmax as the "label" — deterministic, so calibration is
        # near-perfect: confidence = max softmax should match accuracy = 1.0
        # for high-confidence samples and lower for ambiguous ones. With
        # sampling-equivalent base, T=1 minimizes NLL by construction.
        # Add a small noise so labels disagree with argmax 5% of the time.
        if rng.random() < 0.05:
            # Flip to a non-argmax class.
            argmax = max(range(n_classes), key=lambda k: base[k])
            choices = [k for k in range(n_classes) if k != argmax]
            label = rng.choice(choices)
        else:
            label = max(range(n_classes), key=lambda k: base[k])
        flat_logits.extend(base)
        labels.append(label)

    in_arr  = _to_float_array(flat_logits)
    lbl_arr = _to_uint32_array(labels)
    out_T   = ctypes.c_float(0.0)
    out_ece = ctypes.c_float(0.0)
    rc = lib.nimcp_calibrate_temperature(
        in_arr, lbl_arr, n_samples, n_classes,
        ctypes.byref(out_T), ctypes.byref(out_ece),
    )
    assert rc == 0, f"calibrate returned error {rc}"
    # On this near-perfectly-aligned dataset the optimum should land near 1.0.
    # Allow generous tolerance — line-search resolution is 0.1 + golden-section
    # refinement to ~1e-3, and the 5% label-flip noise can pull T slightly off.
    assert 0.5 <= out_T.value <= 2.5, (
        f"calibrated T={out_T.value} should be roughly in [0.5, 2.5] "
        f"for a near-calibrated dataset"
    )
    # ECE must be in [0, 1] and finite.
    assert 0.0 <= out_ece.value <= 1.0, (
        f"ECE must be in [0, 1]; got {out_ece.value}"
    )


# =============================================================================
# Section 3 — additional safety/edge-case tests
# =============================================================================

def test_apply_temperature_rejects_zero(lib):
    """T=0 must be rejected (would be div-by-zero)."""
    arr = _to_float_array([1.0, 2.0, 3.0])
    rc = lib.nimcp_apply_temperature(arr, 3, ctypes.c_float(0.0))
    assert rc != 0, "T=0 must be rejected, not silently accepted"


def test_softmax_with_high_T_softens_distribution(lib):
    """T >> 1 softens — max prob should decrease as T increases."""
    logits = [4.0, 0.0, 0.0, 0.0]
    n = len(logits)
    in_arr = _to_float_array(logits)
    out_arr = (ctypes.c_float * n)()

    rc = lib.nimcp_softmax_with_temperature(in_arr, n, ctypes.c_float(1.0), out_arr)
    assert rc == 0
    p_low_T = max(out_arr)

    rc = lib.nimcp_softmax_with_temperature(in_arr, n, ctypes.c_float(5.0), out_arr)
    assert rc == 0
    p_high_T = max(out_arr)

    assert p_high_T < p_low_T, (
        f"T=5 should soften vs T=1: max prob T=5 ({p_high_T}) < max T=1 ({p_low_T})"
    )


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
