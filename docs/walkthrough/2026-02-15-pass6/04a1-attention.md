# Pass 6 Walkthrough: src/cognitive/attention/

**Files reviewed**: 7
**Date**: 2026-02-15

## Summary

| Severity | Count |
|----------|-------|
| P1       | 8     |
| P2       | 19    |

## Issues

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_attention_snn_bridge.c | 1278 | P1 race | `static float prev_focus` in `attention_snn_get_focus_strength_unlocked()` is shared across all bridge instances; two bridges calling concurrently with their own mutexes race on this variable |
| 2 | nimcp_attention_snn_bridge.c | 1096 | P1 div-by-zero | `weights[h] = (weights[h] - min_rate) / (max_rate - min_rate)` divides by zero if `config.max_rate_hz == config.baseline_rate_hz` |
| 3 | nimcp_attention_snn_bridge.c | 933 | P1 div-by-zero | `int steps = (int)(duration_ms / bridge->config.dt_ms)` divides by zero if `dt_ms == 0.0f` |
| 4 | nimcp_attention_snn_bridge.c | 991 | P1 div-by-zero | Same `duration_ms / bridge->config.dt_ms` divide-by-zero in `attention_snn_compete()` |
| 5 | nimcp_attention_plasticity_bridge.c | 465 | P1 buffer-overflow | `register_synapse()` stores `head_idx` without validating `head_idx < bridge->num_heads`; later in `update()` at line 1026, `bridge->head_states[syn->head_idx]` is an out-of-bounds read/write |
| 6 | nimcp_attention_plasticity_bridge.c | 172 | P1 div-by-zero | `compute_stdp_weight_change()`: `dw *= (1.0f / attention_mod)` divides by zero if `attention_mod == 0.0f`; no guard on the parameter |
| 7 | nimcp_attention_plasticity_bridge.c | 755 | P1 div-by-zero | `attention_plasticity_salience()`: `avg_salience /= (float)sequence_length` divides by zero if `sequence_length == 0` (no zero-length guard) |
| 8 | nimcp_attention_substrate_bridge.c | 510-567 | P1 race | Query functions (`get_focus_capacity`, `get_shifting_efficiency`, `get_filter_strength`, `get_vigilance`, `get_effects`) read `bridge->effects` fields without holding the mutex while `attention_substrate_update()` writes them under lock; `get_effects` copies entire struct non-atomically |
| 9 | nimcp_attention_fep_bridge.c | 84 | P2 wrong-error-code | Returns `NIMCP_ERROR_NULL_POINTER` (1003) instead of `-1`; FEP bridges should return 0/-1 per project convention |
| 10 | nimcp_attention_fep_bridge.c | 182 | P2 wrong-error-code | `attention_fep_bridge_connect_fep()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 11 | nimcp_attention_fep_bridge.c | 201 | P2 wrong-error-code | `attention_fep_bridge_connect_attention()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 12 | nimcp_attention_fep_bridge.c | 219 | P2 wrong-error-code | `attention_fep_bridge_disconnect()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 13 | nimcp_attention_fep_bridge.c | 243 | P2 wrong-error-code | `attention_fep_apply_precision_gain_modulation()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 14 | nimcp_attention_fep_bridge.c | 327 | P2 wrong-error-code | `attention_fep_efe_info_seeking()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 15 | nimcp_attention_fep_bridge.c | 366 | P2 wrong-error-code | `attention_fep_apply_attentional_gating()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 16 | nimcp_attention_fep_bridge.c | 413 | P2 wrong-error-code | `attention_fep_modulate_learning_rate()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 17 | nimcp_attention_fep_bridge.c | 456 | P2 wrong-error-code | `attention_fep_apply_focus_model_narrowing()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 18 | nimcp_attention_fep_bridge.c | 497 | P2 wrong-error-code | `attention_fep_bridge_update()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 19 | nimcp_attention_fep_bridge.c | 542 | P2 wrong-error-code | `attention_fep_bridge_get_state()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 20 | nimcp_attention_fep_bridge.c | 562 | P2 wrong-error-code | `attention_fep_bridge_get_stats()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 21 | nimcp_attention_fep_bridge.c | 583 | P2 wrong-error-code | `attention_fep_bridge_connect_bio_async()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 22 | nimcp_attention_fep_bridge.c | 612 | P2 wrong-error-code | `attention_fep_bridge_disconnect_bio_async()` returns `NIMCP_ERROR_NULL_POINTER` instead of `-1` |
| 23 | nimcp_attention_substrate_bridge.c | 191-194 | P2 leak | On `nimcp_platform_mutex_init()` failure, `bridge->base.mutex` (allocated at line 184) is not freed before `nimcp_free(bridge)` |
| 24 | nimcp_attention_plasticity_bridge.c | 543 | P2 wrong-error-code | `attention_plasticity_get_synapse()` throws `NIMCP_ERROR_NULL_POINTER` when synapse is not found; should be `NIMCP_ERROR_NOT_FOUND` |
| 25 | nimcp_attention_plasticity_bridge.c | 520 | P2 wrong-error-code | `attention_plasticity_unregister_synapse()` throws `NIMCP_ERROR_INVALID_PARAM` when synapse not found; should be `NIMCP_ERROR_NOT_FOUND` |
| 26 | nimcp_attention_plasticity_bridge.c | 450 | P2 false-positive-throw | `attention_plasticity_register_synapse()` throws `NIMCP_ERROR_INVALID_PARAM` for duplicate synapse_id; duplicate registration is a normal caller error, not an immune-worthy event |
| 27 | nimcp_attention_plasticity_bridge.c | 1029 | P2 div-by-zero | `homeostatic_tau_ms` used as divisor; if set to 0 by caller, `dt_ms / bridge->config.homeostatic_tau_ms` is div-by-zero (config value, defaults to 10000 but unvalidated) |

## Details

### P1-1: Static `prev_focus` race in SNN bridge (line 1278)

```c
static float prev_focus = 0.0f;  // Shared across ALL bridge instances
if (fabsf(focus - prev_focus) > 0.2f) {
    bridge->stats.attention_shifts++;
}
prev_focus = focus;
```

The `static` variable is shared across all `attention_snn_bridge_t` instances. Each instance holds its own mutex, but two different bridges calling `attention_snn_get_focus_strength_unlocked()` concurrently will race on `prev_focus`. Fix: move `prev_focus` into the bridge struct.

### P1-5: Missing `head_idx` bounds check in `register_synapse` (line 465)

```c
synapse->head_idx = head_idx;  // No check: head_idx < bridge->num_heads
```

Later in `attention_plasticity_update()` (line 1026):
```c
attention_head_plasticity_t* head = &bridge->head_states[syn->head_idx];  // OOB if head_idx >= num_heads
```

Other functions like `attention_plasticity_focus()` (line 572) DO validate `head_idx >= bridge->num_heads`, but `register_synapse` does not. Fix: add `if (head_idx >= bridge->num_heads) { ... return -1; }` guard.

### P1-8: Unprotected reads in substrate bridge query functions

Functions `attention_substrate_get_focus_capacity()` through `attention_substrate_get_effects()` (lines 510-573) read `bridge->effects` without holding the mutex, while `attention_substrate_update()` writes these fields under `nimcp_platform_mutex_lock()`. The `get_effects()` function copies an entire struct non-atomically, risking a torn read.

### P2-23: Mutex memory leak on init failure (line 191-194)

```c
bridge->base.mutex = (nimcp_mutex_t*)nimcp_malloc(sizeof(nimcp_mutex_t));  // allocated
if (nimcp_platform_mutex_init(bridge->base.mutex, false) != 0) {
    // bridge->base.mutex NOT freed here
    nimcp_free(bridge);  // only frees bridge, leaking mutex allocation
    return NULL;
}
```

### P2-9 through P2-22: FEP bridge wrong error codes (systemic)

14 functions in `nimcp_attention_fep_bridge.c` return `NIMCP_ERROR_NULL_POINTER` (value 1003) on error instead of `-1`. Per project convention documented in MEMORY.md, FEP bridges must return `0` for success and `-1` for errors. The training functions at the bottom of the same file correctly return `-1`, showing inconsistency within the file.
