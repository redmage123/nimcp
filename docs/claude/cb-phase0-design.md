# Conductance-Based Synapse Migration — Phase 0 Design Freeze

**Status**: Locked 2026-04-26. Changes require explicit re-review.
**Owner**: Athena SNN runaway suppression initiative.
**Triggers task IDs**: 196 (this), 197–207.

## Goal

Replace the current-based synaptic input model in the SNN hot loop with a
biologically-grounded conductance-based model, behind a runtime flag that
defaults OFF. This eliminates the unbounded positive-feedback loop responsible
for the dead↔runaway oscillation observed in pod training (700+ Hz peaks
alternating with 3 Hz collapse on a ~17-min cycle).

## Locked equations

### Synaptic conductance update (per receptor type)
```
g_exc[n](t+dt) = g_exc[n](t) * exp(-dt / tau_exc)  +  Σ w⁺_s * spike_s
g_inh[n](t+dt) = g_inh[n](t) * exp(-dt / tau_inh)  +  Σ |w⁻_s| * spike_s
```
where `w⁺_s` is positive-weight synapses (excitatory) and `w⁻_s` is negative
(inhibitory). Receptor type is derived from sign of weight — no struct field
change needed for the synapse itself.

### Membrane integration (conductance mode)
```
dv = ( (v_rest - v) + g_exc * (E_exc - v) + g_inh * (E_inh - v) ) / tau_eff * dt
```

### Membrane integration (current mode, unchanged)
```
dv = ( (v_rest - v) + I_syn ) / tau_eff * dt
```

Both branches converge into the same `v += dv` followed by the existing
threshold check, refractory, AHP/pump subtraction, and spike emission logic.
**No changes to threshold dynamics, refractory, AHP, pump, basket, depression,
intrinsic plasticity, or homeostasis.**

## Locked constants

| Constant | Value | Rationale |
|----------|-------|-----------|
| `SNN_E_EXC_MV` | `0.0` | Cortical AMPA reversal (~0 mV) |
| `SNN_E_INH_MV` | `-80.0` | Cortical GABA-A reversal (Cl⁻, ~−80 mV) |
| `SNN_TAU_EXC_MS` | `2.0` | Fast AMPA decay |
| `SNN_TAU_INH_MS` | `8.0` | GABA-A decay (slower than AMPA — gives I time to brake E) |
| `SNN_CB_WEIGHT_SCALE` | `1.0 / 50.0` | Rescale factor (current→conductance); average driving force at rest is ~50 mV |

`SNN_CB_WEIGHT_SCALE` is applied **once** by `snn_rescale_weights_for_conductance()`
(Phase 3) and then persisted as a checkpoint flag so it never double-applies.

## Locked struct additions

### `snn_population_t` (include/snn/nimcp_snn_types.h:309–390)
Two new per-neuron arrays appended after `depression`, before the biophysical
mechanism pointers:

```c
float* g_exc;  /**< [n_neurons] excitatory conductance (dimensionless) */
float* g_inh;  /**< [n_neurons] inhibitory conductance (dimensionless) */
```

Conductances are dimensionless in our implementation — they are scalar
multipliers on the driving force. (The physical units would be nS, but
they're absorbed into `tau_eff`.)

### Reversal potentials and conductance taus — RUNTIME GLOBALS

**Decision (post-walkthrough #1):** these are NOT struct fields — they
live as runtime-tunable globals in `nimcp_snn_training.c` alongside the
other CB knobs. Reasons:

1. Adding fields to `snn_config_t` would break the binary checkpoint
   serializer at `nimcp_snn_network.c:3386` (`fwrite(&config, sizeof, ...)`).
2. The hierarchical SNN init at `nimcp_snn_hierarchical.c:207` does
   `memset(&cfg, 0, ...)` then field-by-field assignment — adding a
   validation requirement (`e_exc > e_inh`) would silently break it.
3. Single source of truth — heterogeneous per-pop reversal potentials
   are explicitly out of scope (see "open questions").

```c
static float g_snn_e_exc_mv   =   0.0f;  /* AMPA reversal */
static float g_snn_e_inh_mv   = -80.0f;  /* GABA-A reversal */
static float g_snn_tau_exc_ms =   2.0f;  /* fast AMPA decay */
static float g_snn_tau_inh_ms =   8.0f;  /* slower GABA-A decay */
```

Exposed via `snn_tune("e_exc_mv", v)` etc., persisted automatically
by the daemon's `snn_tune.json` mechanism.

## Locked runtime flag

### Global in nimcp_snn_training.c
```c
static float g_snn_conductance_enabled = 0.0f;  /* default OFF */
static float g_snn_cb_weights_rescaled = 0.0f;  /* sticky flag — set after rescale */
```

### Setter/getter pattern
```c
void  snn_tune_set_conductance_enabled(float v);
float snn_tune_get_conductance_enabled(void);
void  snn_tune_set_cb_weights_rescaled(float v);
float snn_tune_get_cb_weights_rescaled(void);
```

### Python binding exposure (`Brain_snn_tune` + `Brain_snn_tune_get`)
Names: `"conductance_enabled"`, `"cb_weights_rescaled"`.

## SOLID/DRY architecture

To keep the hot loop clean and avoid scattering `if (conductance_enabled)`
branches:

### New module: `src/snn/nimcp_snn_membrane.c` + `.h`
Single Responsibility — owns the membrane equation. Two functions:

```c
/* Apply one membrane integration step. Conductance fields may be NULL
 * for current-mode pops; I_syn carries current-mode drive. The function
 * decides which branch to use based on a single flag passed in (NOT
 * by reaching into a global), so it is unit-testable in isolation. */
float snn_membrane_compute_dv(
    float v, float v_rest, float tau_eff_ms, float dt_ms,
    float i_syn,                          /* current-mode drive */
    float g_exc, float g_inh,             /* conductance-mode drive */
    float e_exc, float e_inh,
    bool  conductance_mode);

/* Decay one neuron's conductances by exp(-dt/tau). Caller passes
 * pre-computed decay factors so the inner loop avoids expf(). */
void snn_membrane_decay_conductances(
    float* g_exc, float* g_inh,
    float decay_exc, float decay_inh);
```

The decay factors are computed **once per population step** (not per
neuron), satisfying DRY and matching the existing pattern for `dep_decay`
at nimcp_snn_network.c:1504.

### Synaptic deposit helper (inline, in the hot loop)
Replace the four current-mode I_syn accumulators with a 5-line block that
respects the conductance flag:

```c
/* Synaptic deposit — current OR conductance, NOT both. */
if (cb_mode) {
    if (w >= 0) g_exc_local[n] += w;
    else        g_inh_local[n] += -w;
} else {
    I_syn += w;
}
```

`cb_mode` is a `bool` captured ONCE per pop step (line 1304-area, alongside
`ahp_on`/`pump_on`), not per neuron. The four call sites (CSR loop, basket
contribution, Poisson noise pulse, external_current) all use this same
branch.

### Why not virtual dispatch?
Function pointers per-pop-per-step would inflate I-cache and break
auto-vectorization. Compiler-inlined `bool cb_mode` branch + LICM hoisting
keeps the fast path hot.

## What is explicitly OUT of scope

- **NMDA / Mg block**: voltage-dependent unblock is a second-order nonlinearity.
  Add later if conductance alone isn't sufficient.
- **GABA-B (slow inhibition)**: 100+ ms timescale — handled by AHP for now.
- **Per-synapse heterogeneity in tau or E_rev**: all synapses of one type share
  the same tau and reversal.
- **Conductance-based AHP / pump rewriting**: AHP and pump remain as direct
  hyperpolarization mV subtractions. They model intrinsic K⁺ channels which
  are already current-like in our implementation. Touching them is out of
  scope for this migration.
- **GPU path**: the existing GPU kernel runs at line 856–1022. **Conductance
  mode forces CPU fallback** for now; GPU port is a follow-up. The CPU code is
  what handles the live oscillation on the pod (Hetzner box is CPU-bound for
  these subsystems anyway).

## Insertion point map (exact line numbers, current HEAD)

| Where | File | Line | Action |
|-------|------|------|--------|
| Struct field add | `include/snn/nimcp_snn_types.h` | 389 | Insert `g_exc` + `g_inh` before biophysical pointers |
| Config field add | `include/snn/nimcp_snn_types.h` | 414 | Insert `e_exc` + `e_inh` after `tau_syn` |
| Config default set | `src/snn/nimcp_snn_config.c` | 57 | Add `e_exc`, `e_inh` defaults |
| Config validate | `src/snn/nimcp_snn_config.c` | 281 | Validate `e_exc > e_inh` and finite |
| Allocation | `src/snn/nimcp_snn_network.c` | 209 | Calloc `g_exc`, `g_inh` after `depression` |
| Free | `src/snn/nimcp_snn_network.c` | 340 | Free `g_exc`, `g_inh` |
| Tunable globals | `src/snn/nimcp_snn_training.c` | ~111 | Two new statics + 4 setter/getter funcs |
| Hot-loop captures | `src/snn/nimcp_snn_network.c` | ~1306 | `cb_mode`, `decay_exc`, `decay_inh`, pop-local `g_exc/g_inh` ptrs |
| Synaptic deposit (CSR) | `src/snn/nimcp_snn_network.c` | 1378 | Wrap weight application |
| Synaptic deposit (basket) | `src/snn/nimcp_snn_network.c` | 1383 | Wrap basket_contrib |
| Synaptic deposit (noise) | `src/snn/nimcp_snn_network.c` | 1395-1404 | Wrap noise pulses |
| Membrane update | `src/snn/nimcp_snn_network.c` | 1421 | Replace with `snn_membrane_compute_dv` call |
| Conductance decay | `src/snn/nimcp_snn_network.c` | ~1357 | Decay g_exc/g_inh before synaptic loop |
| Legacy path (non-lightweight) | `src/snn/nimcp_snn_network.c` | 1599+ | Mirror the same wrapping |
| New module | `src/snn/nimcp_snn_membrane.c` | (new) | Implement `snn_membrane_compute_dv` + decay |
| New module hdr | `include/snn/nimcp_snn_membrane.h` | (new) | Public API |
| Python binding | `src/bindings/python/nimcp_python.c` | 1843+ | Add 2 string-dispatch entries |
| CMake | `src/snn/CMakeLists.txt` | (existing) | Add `nimcp_snn_membrane.c` to source list |

## Failure modes & guards

| Failure | Detection | Mitigation |
|---------|-----------|------------|
| `g_exc`/`g_inh` allocation fails | `nimcp_calloc` returns NULL | Mechanism becomes no-op (matches existing pattern); `cb_mode` evaluates to `(g_exc && g_inh && flag)` |
| Flag flipped ON without rescale | `cb_weights_rescaled == 0.0` while `conductance_enabled != 0.0` | One-time WARN in `snn_network_step` (rate-limited via static counter) |
| `e_exc <= e_inh` (config error) | Validate in `snn_config_validate` | Reject + return error |
| `tau_exc` or `tau_inh` ≤ 0 | Compile-time constant — N/A | — |
| GPU path activates with `cb_mode` ON | Check `cb_mode` before GPU dispatch | Force CPU fallback (log once) |

## Test matrix (Phase 4)

| Test type | File | Coverage |
|-----------|------|----------|
| Unit | `tests/snn/test_snn_membrane_unit.c` | `snn_membrane_compute_dv` math: saturation, reversal-clamp, decay, current-mode identity |
| Integration | `tests/snn/test_snn_conductance_integration.c` | Multi-pop with E/I balance, divisive inhibition, AHP+CB, STD+CB, iSTDP+CB |
| Regression | `tests/snn/test_snn_conductance_regression.c` | All existing SNN tests pass with flag OFF |
| E2E | `tests/snn/test_snn_runaway_suppression_e2e.c` | Drive runaway under current-mode, flip to CB, verify peak <500Hz over 60s sim |

## Rollout cadence

1. **Phase 1** (data): no behavior change. Tests should pass identically.
2. **Phase 2** (hot loop): with flag OFF, behavior identical. With flag ON,
   measurable saturation at high g_exc.
3. **Phase 3** (rescale): one-shot CLI command via `snn_tune cb_rescale 1.0`.
4. **Phase 4** (tests): walkthroughs after each test category.
5. **Pod cutover** (out of scope for this doc — Phase 5 of the plan):
   - Deploy code with flag OFF.
   - Verify one full sleep cycle unchanged.
   - Flip flag + trigger rescale via socket.
   - Watch 3 sleep cycles for runaway suppression.

## Acceptance criteria

- All existing SNN tests pass with flag OFF (regression).
- New unit tests pass (saturation, decay, equation correctness).
- E2E test demonstrates runaway suppression at 60-s simulated time.
- No measurable perf regression with flag OFF (≤2% slowdown).
- Perf with flag ON ≤15% slower than current-based.

## Open questions deferred

1. Should `tau_exc` / `tau_inh` be runtime-tunable? **Decision: NO for v1.**
   Add later if needed. Compile-time constants keep the hot loop free of
   per-step extern calls.
2. Should rescale apply to plasticity learning rate too? **Decision: NO.**
   Weights are rescaled but `rstdp_lr` operates on the post-rescale magnitudes,
   which are 50× smaller. Effective LR becomes 50× weaker — empirically validate
   in Phase 4 integration test; if learning stalls, expose `cb_lr_compensation`
   as a separate knob.
3. Should we support per-population CB enable/disable? **Decision: NO.**
   Network-wide flag only — heterogeneous mode is a future feature.

---

**End of Phase 0 design.** Implementation starts at Phase 1.
