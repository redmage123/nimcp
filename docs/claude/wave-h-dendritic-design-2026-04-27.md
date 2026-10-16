# Wave H — Dendritic Compartments + NMDA Plateau Design

**Status**: Design draft 2026-04-27. Not yet implemented.
**Triggers**: Wave H (task #265), prerequisite for active dendritic plasticity.
**Owner**: SNN biological-fidelity initiative.

## Goal

Add a basal-vs-apical dendritic compartment to the lightweight SNN
population so apical NMDA plateau spikes can be modelled separately from
soma firing. Currently every neuron is single-compartment: all four
receptor conductances sum into one membrane potential and contribute to
the same spike-vs-no-spike decision. Real cortical pyramidal cells run
two roughly-independent integration zones — basal dendrites + soma feed
the action-potential output, apical tuft integrates top-down NMDA on a
slower timescale and feeds back into the soma via electrotonic coupling
+ NMDA plateau.

Default OFF behind a runtime flag. The flag-OFF path must be
bit-identical to current behaviour (regression contract).

## Locked equations

### Two-compartment LIF
```
dv_basal  = ( (v_rest - v_basal)
            + g_ampa_b   * (E_ampa  - v_basal)
            + g_gaba_a_b * (E_gabaa - v_basal)
            + g_coup     * (v_apical - v_basal)
          ) / tau_basal * dt

dv_apical = ( (v_rest - v_apical)
            + g_nmda_a   * mg_block(v_apical) * (E_nmda - v_apical)
            + g_gaba_b_a * (E_gabab - v_apical)
            + g_coup     * (v_basal - v_apical)
            + plateau_drive(v_apical)
          ) / tau_apical * dt

soma_v    = v_basal       (threshold check + spike emission here)
```

`g_coup` is the inter-compartmental electrotonic coupling — small (~0.05),
like the gap-junction pattern from Wave D but stronger because the basal
and apical compartments are physically continuous.

### Plateau drive
```
plateau_drive(v_apical) =
    plateau_gain * H(v_apical - V_plateau_threshold)
                 * exp(-(t - t_plateau_onset) / tau_plateau)
```

`H(...)` is Heaviside. When apical V crosses ~-40 mV, the plateau drive
ramps and decays over τ_plateau ≈ 50 ms. While active, it depolarises
the apical compartment AND, via `g_coup`, the basal — making the soma
much more likely to fire on subsequent input.

This is what Larkum + Spruston 2008 measured as the "BAC firing"
phenomenon: an apical NMDA spike + a coincident basal AP causes a burst.

## Locked struct additions

`snn_population_t`:
```c
/* Compartment runtime state (allocated only when dendritic_enabled = true) */
float* v_basal;        /* [n_neurons] basal compartment V (mV)   */
float* v_apical;       /* [n_neurons] apical compartment V (mV)  */
float* g_ampa_b;       /* [n_neurons] AMPA on basal              */
float* g_gaba_a_b;     /* [n_neurons] GABA_A on basal            */
float* g_nmda_a;       /* [n_neurons] NMDA on apical             */
float* g_gaba_b_a;     /* [n_neurons] GABA_B on apical           */
uint8_t plateau_active; /* [n_neurons] flag: apical plateau on   */
uint64_t* plateau_t0;   /* [n_neurons] tick of plateau onset     */
```

The legacy single-compartment fields (`g_ampa`, `g_nmda`, `g_gaba_a`,
`g_gaba_b`) remain. When `dendritic_enabled = false`, they're used; when
`true`, the eight new arrays are used and the legacy ones are skipped.

Memory cost per pop with dendritic ON: 8 × 4 bytes × N_neurons = 32 N
bytes additional. For a 60K-neuron tier-pyr pop, that's ≈ 2 MB/pop. With
40+ pops in the hierarchy, ≈ 80 MB extra RAM total — acceptable.

## Locked routing

The deposit kernel needs to know which compartment each receptor lands
on. Per the Larkum / Spruston biology:

| Receptor | Default target |
|----------|----------------|
| AMPA     | basal soma     |
| GABA_A   | basal soma     |
| NMDA     | apical tuft    |
| GABA_B   | apical tuft    |

This means top-down NMDA (P1.1, L5→L3 / L6→L2) lands on apical, where it
can drive a plateau, while bottom-up AMPA lands on basal. SOM→pyr GABA_A
also goes to basal (perisomatic), which gates soma firing directly. VIP
→ SOM disinhibition still works because SOM's inhibition of basal is
what was clamping the soma.

Per-pop override via a new field `pop->dendritic_routing_override` if
ever needed (anatomical edge cases like L5 corticothalamic NMDA on
basal). Default = the table above.

## Hot-loop integration

Two new helper inlines in `nimcp_snn_membrane.h`:
```c
static inline void snn_membrane_compute_dv_two_compartment(
    const float v_basal, const float v_apical,
    const float v_rest, const float tau_b, const float tau_a, const float dt,
    const float g_ampa_b, const float g_gaba_a_b,
    const float g_nmda_a, const float g_gaba_b_a,
    const float g_coup,
    const float e_ampa, const float e_nmda, const float e_gaba_a, const float e_gaba_b,
    const float mg_mm,
    const bool plateau_active, const float t_since_plateau_onset, const float plateau_gain, const float tau_plateau,
    float* dv_basal_out, float* dv_apical_out);

static inline bool snn_membrane_check_plateau_onset(float v_apical, float v_threshold);
```

The single-compartment `snn_membrane_compute_dv` is preserved unchanged
for the OFF-path and for the cases we don't want compartments
(interneurons — PV/SOM/VIP are smaller cells without elaborate apical
arbours, single-compartment is biologically correct for them).

## Locked runtime flag

```c
extern void  snn_tune_set_dendritic_enabled(float v);   /* default 0.0 */
extern float snn_tune_get_dendritic_enabled(void);
```

Population-side: `pop->dendritic_enabled` (bool, default false). Wave H
init code in `nimcp_snn_hierarchical.c` sets it to `true` ONLY for tier-
pyr pops with subclass in {PYRAMIDAL_L23, L4_STELLATE, L5_BETZ,
PYRAMIDAL} — i.e., principal cells with dendrites. Interneurons stay
single-compartment.

## Test matrix (Phase 4)

- **Unit** — single-cell two-compartment math: rest equilibrium with
  zero g's, AMPA-on-basal raises basal V, NMDA-on-apical at rest is
  silent (Mg block), NMDA-on-apical at depolarised is unblocked,
  electrotonic coupling pulls compartments toward shared V, plateau
  triggers when apical crosses -40 mV.
- **Integration** — 3-pop circuit (sensor → basal-AMPA, top-down →
  apical-NMDA, output ← soma_v threshold). Verify (a) bottom-up alone
  fires soma, (b) top-down alone does NOT fire soma (apical NMDA plateau
  doesn't reach threshold without basal companionship), (c) coincident
  bottom-up + top-down fires a BURST (multiple consecutive spikes via
  plateau drive) within 30 ms of coincidence — the Larkum BAC phenotype.
- **Regression** — flag OFF: behaviour bit-identical to pre-Wave-H.
  Verify by running existing per-receptor + conductance regression tests
  with dendritic_enabled = 0.
- **E2E** — full hierarchical brain with dendritic ON for tier-pyr pops,
  off for interneurons, runs N steps without crashing, V's stay finite,
  no NaN, no infinite loops.

## What is explicitly OUT of scope

- **Active dendritic spikes (sodium / calcium APs)** beyond the NMDA
  plateau. Real apical dendrites have voltage-gated channels that
  generate full-blown action potentials. Out of scope for v1; could be a
  Wave H+ follow-up.
- **Per-synapse compartment assignment**. We use receptor-class →
  compartment mapping (table above). Real synapses can target either
  compartment regardless of receptor; if needed, add a per-synapse
  compartment field later.
- **GPU port**. Like CB itself, dendritic mode forces CPU fallback.
- **Plateau-triggered structural plasticity**. Out of scope.

## Failure modes & guards

| Failure | Detection | Mitigation |
|---------|-----------|------------|
| 8 new arrays alloc fail | `nimcp_calloc` returns NULL | Mark dendritic_enabled = false on this pop, log warning, fall back to single-compartment for this pop |
| Plateau drives v_apical to +∞ | dv > 100 mV clamp in compute_dv | Existing biophysical guard catches it |
| Coupling g_coup too high → oscillation | Visual: V_basal ↔ V_apical ringing | Cap g_coup at 0.2; integration test asserts no cross-compartment ringing |
| Flag flipped ON without compartment alloc | pop->v_basal == NULL | Skip dendritic update on that pop, log once |

## Acceptance criteria

- All existing SNN tests pass with `dendritic_enabled = 0` (regression).
- New unit + integration + e2e tests pass with `dendritic_enabled = 1`.
- BAC firing phenotype demonstrated in integration test.
- No measurable perf regression with flag OFF (≤2% slowdown).
- Perf with flag ON ≤30% slower than single-compartment.

## Rollout

1. **Phase 1** (data): add 8 new arrays + alloc/free; flag OFF.
2. **Phase 2** (math): add two-compartment compute_dv helper; flag OFF
   tests bit-identical.
3. **Phase 3** (hot loop): wire into snn_network_step; flag still OFF;
   regression sweep.
4. **Phase 4** (tests): walkthroughs after each test category.
5. **Hierarchical wiring**: set `dendritic_enabled = true` on tier-pyr
   pops only. Receptor → compartment routing applies automatically via
   the table above.

---

**End of Wave H design.** Implementation begins after Wave E full fix +
Wave G heterogeneity land cleanly. Estimated implementation effort: 2-4
focused hours including all four test tiers + walkthrough.
