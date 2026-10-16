# Feed-Forward Inhibition (FFI) Timing Audit — 2026-04-27

**Scope**: Thalamic → L4 pyramidal AMPA vs Thalamic → PV → pyramidal GABA_A
timing window in the SNN (Wave E task #262). Read-only audit.

## 1. Conduction-Delay Model

**Finding: there is no per-synapse, per-axon, or per-population conduction
delay applied in the SNN step path.**

- `snn_csr_synapse_t` (`include/snn/nimcp_snn_synapse.h:29-33`) is 12 bytes and
  contains only `{src_pop, src_neuron, weight}`. **No `delay` field.**
- `snn_population_t` (`include/snn/nimcp_snn_types.h:361-`) carries no delay
  array. The receptor table `synapse_type_per_src[SNN_MAX_POPULATIONS]` is
  per-pop-pair but type-only, not delay.
- `snn_config_t` declares `bool use_axon_delays`
  (`include/snn/nimcp_snn_types.h:526`); it is set in
  `nimcp_snn_config.c:88,238,557` but **read nowhere outside the config dump
  on line 611** — confirmed by `grep -rn 'use_axon_delays' src/snn/`.
- The CPU step loop (`src/snn/nimcp_snn_network.c:1486-1511`) checks
  `src_pop->spike_output[s] > 0.5f` for the **same tick** spike vector.
  Populations are processed sequentially in the
  `for (p = 0; p < n_populations; p++)` outer loop (line 1263), so a src pop
  whose index < dst pop index delivers its synaptic drive in the **same
  step**, with effective delay = 0 ms.
- Where delay machinery exists, it is in **bridges**, not the SNN core hot
  path:
  - `snn_buffer_bridge` has `enable_delay_lines` + `add_delayed_spike`
    (`min_delay_ms=0.5`, `max_delay_ms=10`). Used by ad-hoc consumers, not
    by the main step.
  - `snn_thalamic_bridge` has `ct_delay_ms = 5.0f` for **corticothalamic
    feedback**, not feed-forward thalamic→cortex drive.
  - `snn_cortical_bridge` has `inhibitory_delay = 2.0f`; not consumed by
    the per-tick deposit kernel.

**Effective default conduction delay in the FFI hot path: 0 ms (formula:
none; spikes are read from the same-tick `spike_output` tensor).**

## 2. PV Population Properties

PV pops are created in `src/snn/nimcp_snn_hierarchical.c:436-474` for
recurrent tiers 2..6.

- Subclass set: `SNN_NSC_PV` (line 471) → `snn_pop_lif_params()` returns
  `tau_mem=10ms`, `t_ref=1ms` (`include/snn/nimcp_snn_types.h:795-798`).
  Biologically correct fast-spiking profile.
- **No PV-specific conduction-delay setting exists** because no per-pop
  conduction-delay setting exists at all (see §1). PV inherits the same
  zero-delay treatment as every other pop.
- PV does **not** receive direct input-pop / thalamic drive. The wiring
  pattern is `pyr → PV (AMPA)` within the same recurrent tier
  (`nimcp_snn_hierarchical.c:495-508`). Tier 0 (the thalamic-relay role)
  has **no** PV/SOM/VIP — disinhibition pops only exist for tiers 2..6.

## 3. One-Path Timing Budget — Thalamic→Pyr AMPA vs Thalamic→PV→Pyr GABA

Trace using a tier-2..6 pyramidal pop receiving "thalamic" drive proxied via
the `input_pop → tier 0 → tier 1 → tier 2` feedforward chain:

| Event                                  | Time (ms)               | Notes                              |
|----------------------------------------|-------------------------|------------------------------------|
| Thalamic spike at input_pop            | t = 0                   |                                    |
| Δ1: input_pop → pyramidal (same step)  | t ≈ 0 (one step = 0.1)  | zero-delay deposit                 |
| Δ2: input_pop → PV                     | n/a                     | **PV has no input-pop afferent**   |
| Pyr at tier-2 must spike first         | t ≈ τ_mem-bound, ~5-15  | PV drive is `pyr→PV`, not direct   |
| ε_PV: PV integrates to threshold       | + ~3-5                  | tau_mem=10, fluct-driven           |
| Δ3: PV → pyr GABA (same step)          | + 0                     | zero-delay deposit                 |

For the canonical FFI sketch (PV must arrive ≤ Δ1 + 1-3 ms after AMPA), the
network is **off by a full integration cycle**: PV cannot fire until at
least one pyr in its tier has already fired and reached PV via AMPA. PV's
GABA_A then arrives in the *next* step on the pyramidal that was already
driven through full integration.

In other words: there is no biological FFI window at all. Every inhibitory
contribution is **feedback inhibition** by construction, because (a) PV is
not driven by the thalamic afferent, only by the pyramidal it's supposed
to gate, and (b) the same-tick spike vector means the pyramidal has
already integrated full thalamic AMPA before its PV partner has had a
chance to spike.

## 4. CB Mode + CSR Lightweight Path

CB-mode deposit (`src/snn/nimcp_snn_network.c:1442-1521`) routes weight
into `g_ampa / g_gaba_a` etc. with no extra queue. There is **no batching
or jitter** added by CB — the timing is whatever the same-tick / same-step
deposit gives, which is what §3 describes. So CB mode does not introduce
new timing pathology, but it does not fix the missing conduction delay
either.

## 5. Verdict

**FFI timing: BROKEN.**

Two compounding bugs:
1. **Zero conduction delay everywhere** — `use_axon_delays` is a dead flag.
   Same-tick spike-deposit collapses Δ1 = Δ3 = 0, so PV→pyr GABA arrives
   *after* pyr has already integrated the AMPA in the same step.
2. **PV has no thalamic afferent** — the canonical thalamus→{pyr, PV}
   parallel projection is not wired. PV is driven only by intra-tier
   pyramidal collaterals, making the "feed-forward" arm structurally
   feedback.

Either one alone would weaken FFI; together they eliminate it.

## 6. Minimal Fix Sketch (DO NOT APPLY HERE)

Three-line conceptual fix; concrete patch needs design review:

1. Add `uint8_t conduction_delay_steps` (default 0) to
   `snn_population_t`; populate from subclass (PV=1 step ≈ 0.1 ms, default
   pyramidal = 10 steps ≈ 1 ms) in `snn_pop_lif_params()` or a sibling
   helper.
2. In `nimcp_snn_network.c:~1494`, replace the same-tick read
   `src_spikes[syns[s].src_neuron] > 0.5f` with a small per-pop ring
   buffer of past `spike_output` indexed by `current_step −
   src_pop->conduction_delay_steps`. (Bounded memory: max delay × pop.)
3. In `nimcp_snn_hierarchical.c:495-508`, add an `input_pop →
   inh_pop_pv[i]` wiring with the SAME thalamic-style AMPA weight used by
   `input_pop → tier_0 pyr`, so PV receives parallel thalamic drive. Then
   PV's faster `tau_mem=10` + `t_ref=1` + step-1 conduction beats
   pyramidal's `tau_mem=20` + step-10 conduction → PV GABA wins the
   1-3 ms race that biology requires.

Rough budget after fix: Δ1 ≈ 1.0 ms (pyr conduction), Δ2 ≈ 0.1 ms (PV
conduction), ε_PV ≈ 1-2 ms (fast-spiking integration to threshold), Δ3 ≈
0.1 ms → GABA arrival at ≈ 1.3-2.3 ms; AMPA at ≈ 1.0 ms; window ≈
0.3-1.3 ms. Within the 1-3 ms biological band.

## 7. References

- Step path: `src/snn/nimcp_snn_network.c:1263-1521`
- CSR struct: `include/snn/nimcp_snn_synapse.h:29-33`
- PV LIF profile: `include/snn/nimcp_snn_types.h:783-827`
- PV wiring: `src/snn/nimcp_snn_hierarchical.c:388-553`
- Dead flag: `include/snn/nimcp_snn_types.h:526`,
  `src/snn/nimcp_snn_config.c:88,238,557`
- Bridge-side delay APIs (not in hot path):
  `src/snn/bridges/nimcp_snn_buffer_bridge.c:383-409`,
  `src/snn/bridges/nimcp_snn_thalamic_bridge.c:36`
