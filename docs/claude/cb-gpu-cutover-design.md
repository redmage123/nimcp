# CB → GPU Integration Cutover — Design (CB-GPU-5)

**Status**: design-frozen, implementation in progress
**Scope**: route the GPU `nimcp_gpu_lif_forward_cb` kernel into `snn_network_step` so CB+GPU mode actually offloads integration to the GPU, instead of falling through to the CPU.
**Predecessor commits**: 1b3ae0b40 (kernel + state), 1175d4a75 (24-test scaffold), 084dd4b7f (`cb_gpu_enabled` knob default-ON).

---

## Why this is non-trivial

The CPU CB hot loop (`src/snn/nimcp_snn_network.c` ~1500-2100) fuses **five concerns** in one per-pop / per-neuron pass:

1. **Decay** — `snn_membrane_decay_one(g_ampa/g_nmda/g_gaba_a/g_gaba_b)`
2. **Gap-junction V coupling** — `v_data[n] += gap_coupling * (V_mean - v_data[n])`
3. **Refractory + substrate-emergency early-outs** — `continue` when neuron skips this step
4. **Synaptic deposit** — external_current + per-pop CSR walk (with `pop->synapse_type_per_src[src_pop]` routing) + basket inhibition + Poisson noise
5. **Membrane integration + spike emission + adaptation** — `compute_dv` (legacy or two-compartment dendritic), `v_data[n] += dv`, threshold check with substrate spike-survival, `v_reset` reset, `record_spike`, then pop-level AHP / pump update + intrinsic plasticity + depression decay.

Concerns 1-4 produce post-deposit `pop->g_*` and updated `v_data` (gap junction). Concern 5 reads those, integrates, writes `v_data` + `spike_data`, and updates per-pop adaptation state from `spike_data`. **Concern 5 is what the GPU kernel replaces.**

To split cleanly we need three helpers:

| Helper | Input | Output |
|--------|-------|--------|
| `snn_cb_deposit_pop` (concerns 1-4 + AHP/pump hyp) | pop + network + dt + cb constants | `pop->g_*` post-deposit, `v_data` post-gap-junction, refractory ticked, `ahp_hyp`+`pump_hyp` arrays |
| `snn_cb_integration_pop` (concern 5 split: integrate) | pop + network + dt + cb constants + hyp arrays | `v_data` post-integrate, `spike_data` populated |
| `snn_cb_post_spike_pop` (concern 5 split: spike-survival + adapt) | pop + network + dt + spike_data | `pop->ahp/pump/basket/threshold_offset/depression` updated, `record_spike` called |

In the GPU CB path, `snn_cb_integration_pop` is replaced by a network-level GPU pass:

```
for each pop: snn_cb_deposit_pop(pop, ...)
gather pop->v + pop->g_* → flat host buffers
upload to lif_state (v + g_ampa + g_nmda + g_gaba_a + g_gaba_b)
nimcp_gpu_lif_forward_cb(...)
download lif_state->v + lif_state->spikes + post-decay g_*
scatter back to pop->v_data, pop->spike_data, pop->g_*
for each pop: snn_cb_post_spike_pop(pop, ...)
```

In the CPU-only CB path, all three helpers run in sequence per-pop (preserves current behavior).

---

## Data flow

```
        ┌──────────────┐
prev    │  spike_output│ (from t-1)
step ──►│   pop->v_data│
        └──────────────┘
                │
                ▼
       ┌────────────────────────┐
       │ snn_cb_deposit_pop     │
       │  - decay g_*           │
       │  - gap-junction V      │
       │  - refractory tick     │
       │  - external + CSR + basket + noise deposits  │
       │  - compute ahp/pump hyp│
       └────────────────────────┘
                │
        ┌───────┴────────┐
        │                │
        ▼                ▼
   CPU integrate    GPU integrate
  ┌──────────┐    ┌──────────────┐
  │ compute_ │    │ upload v + g │
  │ dv +     │    │ forward_cb   │
  │ V update │    │ download     │
  │ + spike  │    │ V + spikes   │
  │ check    │    └──────────────┘
  └──────────┘            │
        │                 │
        └────────┬────────┘
                 ▼
       ┌────────────────────────┐
       │ snn_cb_post_spike_pop  │
       │  - substrate dropout   │
       │  - record_spike        │
       │  - ahp/pump update     │
       │  - basket step         │
       │  - IP + depression     │
       └────────────────────────┘
                 │
                 ▼
            t+1 ready
```

---

## Why we can split safely

Within a single step, no neuron in pop A reads pop B's t+1 deposits, and no integration depends on another neuron's t+1 V (gap junctions use V_mean computed BEFORE integration, basket/noise are applied as drives during deposit). So the (decay → deposit → integrate → spike) sequence within a pop can be split into two passes without changing semantics, provided:

- Gap-junction V_mean is computed **once** (already is — line ~1620)
- Integration consumes pop->g_* AFTER deposit (already does)
- Adaptation reads spike_data AFTER integration (already does)
- AHP/pump_hyp is computed BEFORE integration and consumed once (already is — line ~1585)

The split is mechanical; no algorithmic change.

---

## GPU integration ordering (matches kernel contract)

The GPU kernel `nimcp_gpu_lif_forward_cb` does `integrate → decay`. The CPU CB hot loop does `decay → deposit → integrate`. These are equivalent at steady state if the wiring layer deposits BEFORE the kernel runs:

| Step | CPU CB sequence | GPU CB sequence |
|------|----------------|-----------------|
| t   | `decay(g_{t-1}) + Δ_t = g_t` | `g_t = host(g_{t-1, post-decay} + Δ_t)` (host pre-decays via deposit-pass helper) |
| t   | `integrate(g_t) → v_t` | `forward_cb: integrate(g_t) → v_t, decay g_t → g_{t, post-decay}` |
| t   | (no extra decay; decay happens at t+1) | `download g → host has g_{t, post-decay}` ready for t+1 deposit |

**Key invariant**: the host-side `pop->g_*` arrays carry the post-decay state between steps. The deposit-pass helper does decay BEFORE deposit on the host (mirroring the CPU loop's order). The GPU kernel then integrates with the post-deposit `g_t` and applies its own per-step decay. After `download_g`, host `pop->g_*` has the same post-decay state the CPU CB path produces — so subsequent CPU-side R-STDP / STDP / save-checkpoint code reads identical state regardless of which integration path ran.

---

## Helper signatures

```c
/* Per-pop CB constants — captured once at top of step layer to avoid
 * re-reading 12+ tune knobs per pop. */
typedef struct {
    bool   cb_mode;
    bool   dend_mode;
    float  e_ampa, e_nmda, e_gaba_a, e_gaba_b;
    float  decay_ampa, decay_nmda, decay_gaba_a, decay_gaba_b;
    float  decay_ampa_b, decay_gaba_a_b, decay_nmda_a, decay_gaba_b_a;
    float  mg_mm;
    float  basket_contrib;
    float  noise_p, noise_pulse_mv, ei_ratio;
    bool   noise_exc_only;
    float  substrate_pump_factor;
    /* …et al */
} snn_cb_pop_constants_t;

/* Decay + deposit + AHP/pump hyp computation for one pop. Writes into
 * pop->g_* (post-deposit), pop->v_data (post-gap-junction), pop->refractory
 * (ticked), and the caller-owned ahp_hyp/pump_hyp arrays.
 *
 * Does NOT integrate, does NOT emit spikes. Safe to call from both CPU
 * CB hot loop and CB GPU pre-pass. */
void snn_cb_deposit_pop(snn_population_t* pop,
                        snn_network_t*    network,
                        float             dt_ms,
                        const snn_cb_pop_constants_t* k,
                        float*            ahp_hyp_out,   /* sized n_neurons or NULL */
                        float*            pump_hyp_out); /* sized n_neurons or NULL */

/* Integrate + spike check for one pop. Reads pop->g_* + pop->v_data +
 * ahp_hyp + pump_hyp; writes pop->v_data + pop->spike_data. Used by CPU
 * CB path; the GPU CB path replaces this with a network-level GPU pass. */
void snn_cb_integration_pop(snn_population_t* pop,
                            snn_network_t*    network,
                            float             dt_ms,
                            const snn_cb_pop_constants_t* k,
                            const float*      ahp_hyp,
                            const float*      pump_hyp);

/* Post-spike: substrate dropout, record_spike, AHP/pump update,
 * basket step, IP + depression update. Reads pop->spike_data; writes
 * pop->ahp/pump state, pop->basket, pop->threshold_offset,
 * pop->depression, pop->spike_trains. */
void snn_cb_post_spike_pop(snn_population_t* pop,
                           snn_network_t*    network,
                           float             dt_ms);

/* Top-level GPU CB step. Returns true if GPU path completed; false if
 * a fallback to CPU is required (e.g. lif_state CB alloc failed). */
bool snn_network_step_cb_gpu(snn_network_t* network, float dt_ms);
```

---

## Test strategy

| Test | What it pins |
|------|--------------|
| Unit: `snn_cb_deposit_pop` | post-deposit `pop->g_*` matches inline CPU loop output for a 4-pop synthetic network, 50 steps |
| Unit: `snn_cb_integration_pop` | spike + V output identical to inline CPU loop |
| Unit: `snn_cb_post_spike_pop` | adaptation state + record_spike calls match inline CPU loop |
| Integration: CPU vs GPU CB step | 16-pop, 1024-neuron, 200-step run with `cb_gpu_enabled=0` (CPU) vs `=1` (GPU). Asserts `pop->g_*`, `pop->v_data`, `pop->spike_data`, mean firing rate, total spike count agree to FP32 tolerance after each step. |
| Regression: CB OFF unchanged | with `conductance_enabled=0`, current-based path produces bit-identical output before/after the refactor (uses a synthetic 8-pop fixture) |
| E2E: pod-style CB+GPU run | 1.9M-neuron checkpoint loaded, 100 steps, no NaN, healthy 30-50 Hz across all populations, GPU util > 30% |

---

## Risk & rollback

- **Risk 1**: subtle off-by-one in the deposit/integrate split breaks training dynamics. Mitigation: integration test asserts step-by-step `pop->g_*` and `pop->v_data` match CPU.
- **Risk 2**: GPU kernel's reversal-bound clamp interacts differently than CPU when V is at threshold during deposit. Mitigation: covered by existing CB GPU integration test (commit 1175d4a75) + the new step-level test.
- **Risk 3**: per-step host↔device transfers (4× upload + 6× download) dominate runtime, making CB+GPU SLOWER than CPU. Mitigation: profile on pod after CB-GPU-6 deploy; if confirmed, follow-up CB-GPU-7 ports the deposit pass to GPU too.
- **Rollback**: setting `cb_gpu_enabled=0` via `tune_snn` RPC reverts to the CPU CB path within one step (no daemon restart needed).

---

## Implementation phases

| Phase | What | Scope |
|-------|------|-------|
| **A** (this task) | Helper extraction with **no behavior change** — refactor inline CPU CB loop to call the three helpers, prove identical output | structural refactor + regression test |
| **B** | New `snn_network_step_cb_gpu` wired in, deposit + post-spike on CPU, integrate on GPU | the actual cutover + step-level integration test |
| **C** | Profile, fix any drift, deploy to pod | CB-GPU-6 (separate task) |

Phase A is what gets shipped first because it is risk-free (pure refactor, locked by regression test). Phase B then becomes a small additive change instead of a 400-line risk surface.
