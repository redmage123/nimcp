# Test Infrastructure

**Last Updated:** 2026-04-19

## Layout

```
tests/
  smoke/              — Fast (<5 min) tests; run on every change
    test_brain_roundtrip.py
    test_deterministic_inference.py
    test_python_bindings.py
    test_harness_integration.py
    run_all.sh
  regression/         — Baseline gate; run before deploy
    run_regression.sh
    capture_baseline.py
    compare_baseline.py
    ab_compare.py
  unit/               — Per-module unit tests
    test_snn_csr_gpu_residency.py
    test_curiosity_selector.py
    test_curriculum.py
    test_symbolic_writer.py
    test_synthesized_sensory.py
    test_gradient_accumulator.py
  baseline/
    battery_baseline.json    — Captured baseline for comparison
```

## Regression Gate (`tests/regression/run_regression.sh`)

Four phases, fail-fast:

1. **Build** — `make nimcp nimcp_python -j4` must succeed
2. **Python import** — `import nimcp` must work
3. **Smoke tests** — all 10 test functions must pass
4. **Baseline compare** — battery scores within tolerance of captured baseline

Typical run time: 2-3 minutes.

Flags:
- `--skip-build` — use existing build
- `--capture` — update baseline after successful run

## Smoke Tests

### test_brain_roundtrip.py
- save → load → save sequence produces well-formed outputs
- SNN checkpoint schema compatibility
- Documents known save/load drift issue (not yet fixed, tolerance widened)

### test_deterministic_inference.py
- Same input → same output on same brain
- save → load preserves inference (within documented drift tolerance)

### test_python_bindings.py
- 29 critical methods exposed on Brain object
- Methods callable without segfault (non-fatal exceptions OK)

### test_harness_integration.py
- Harness module imports
- Stimulus banks load from JSON
- Battery runs end-to-end (against minimal local brain)
- Report card generates text/JSON/HTML
- SQLite result store works

## Unit Tests

### test_snn_csr_gpu_residency.py
- SNN stats accessible
- Forward step runs without crash (with persistent GPU path)

### test_curiosity_selector.py (4 tests)
- Falls back when no gaps reported
- Targets gaps when available
- Graceful on API exceptions
- Recent deduplication

### test_curriculum.py (4 tests)
- Narrows scope in early steps
- Expands with progress
- Handles empty-category extractor
- Does not starve on category-flood

### test_symbolic_writer.py (4 tests)
- All layers written when all APIs present
- Graceful with no APIs
- Partial APIs used, missing ones skipped
- Exceptions caught and counted

### test_synthesized_sensory.py (5 tests)
- Category inference correct
- All rich channels present
- Multi-view produces variants
- Valence differentiated by category
- Stats counter works

### test_gradient_accumulator.py (6 tests)
- Window fills
- LR scales down on high grad norm
- LR scales up on low grad norm
- LR clamped to bounds
- NaN/Inf rejected
- Loss trend captured

## A/B Comparison (`ab_compare.py`)

Compares two `.so` files for numerical equivalence:
```
python3 tests/regression/ab_compare.py \
    baseline.so new.so \
    --seeds 100 --tolerance 1e-4
```

Each `.so` is loaded in an isolated subprocess. Both processes load from the
same checkpoint to ensure identical starting state. Outputs compared within
tolerance. Use `--accept-known-drift` to allow larger delta while save/load
drift remains unresolved.

## Running Tests

```bash
# Individual smoke test
python3 tests/smoke/test_brain_roundtrip.py

# All smoke tests
bash tests/smoke/run_all.sh

# Full regression gate (standard)
bash tests/regression/run_regression.sh

# Regression skipping rebuild
bash tests/regression/run_regression.sh --skip-build

# Update baseline after intentional change
bash tests/regression/run_regression.sh --capture

# A/B comparison
cp build/lib/python/nimcp.so baseline.so
# ... make changes, rebuild ...
python3 tests/regression/ab_compare.py baseline.so build/lib/python/nimcp.so

# Unit tests (one)
python3 tests/unit/test_curiosity_selector.py
```

## Continuous Integration Patterns

Although there's no CI server yet, the patterns this infrastructure supports:

1. **Pre-commit**: `tests/regression/run_regression.sh --skip-build`
2. **Pre-deploy**: full `run_regression.sh` + manual review
3. **Post-merge**: update baseline if behavior intentionally changed
4. **Release**: all tests + canary brain on pod

## Known Issues Tracked Here

1. **Save/load drift** — `predict()` differs before/after save by up to 0.3.
   Smoke test accepts, regression catches large divergence.
2. **perturb_weights stub** — does not actually mutate weights in small brains.
   Logging/audit trail works; real mutation needs adaptive_network accessor.
3. **Hash-based text encoding** (now semantic-hash) — still not a real
   tokenizer. Real tokenizer wiring is a follow-up.
4. **jepa_latent_create warnings** on certain brain configurations — pre-
   existing, non-fatal.

## See Also

- [../plans/session_roadmap.md](../plans/session_roadmap.md) — full plan
- [../plans/session_handoff.md](../plans/session_handoff.md) — handoff state
