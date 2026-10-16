# Dale's Principle Audit — SNN Hierarchical Wiring (2026-04-27)

**Wave**: C (task #260)
**Scope**: `src/snn/nimcp_snn_hierarchical.c` — production wiring for the
1.8M-neuron primary SNN (46 base populations across 8 tiers + per-tier
PV/SOM/VIP triplets + reticular thalamic shell).

## Background

Dale's principle: a single neuron releases the same neurotransmitter at all
of its synaptic terminals. In the population model that becomes:
**every source population must emit a single broad receptor class**
— either excitatory (AMPA / NMDA) or inhibitory (GABA_A / GABA_B), never
both — across all of its outgoing dst pops.

Per-source-pop receptor type lives in `dst->synapse_type_per_src[src_pop]`
(see `include/snn/nimcp_snn_types.h`, set by `snn_network_connect_populations`
in `src/snn/nimcp_snn_network.c`). The recent CB / per-receptor migration
(`g_ampa`, `g_nmda`, `g_gaba_a`, `g_gaba_b`) makes Dale violations
biophysically observable: a single source's spike would be deposited into
both an excitatory and an inhibitory bucket simultaneously, which no real
neuron can do.

## Methodology

Static read of `nimcp_snn_hierarchical.c` (the only production wiring path
for the primary SNN). For each `snn_network_connect_populations` call, the
audit recorded `(src tier/pop, dst tier/pop, receptor)` and bucketed by
src to detect any pop emitting both classes. Modulatory / electrical /
generic synapses are out of scope (Dale governs classical fast
neurotransmission; co-release of neuromodulators is biologically allowed).

A runtime validator was also added so the per-receptor table is checked
at production-network creation time:

```c
int snn_network_validate_dale(const snn_network_t* net,
                              char* err_buf, size_t err_buf_sz);
```

## Per-source outgoing-receptor table

| Source population             | Outgoing receptor classes used                           | Dale OK?   |
|-------------------------------|----------------------------------------------------------|------------|
| `input_pop` (id 0)            | AMPA (→ tier-0 fanout)                                   | yes        |
| Tier 0 pyramidal (`input_*`)  | AMPA (FF→T1) · NMDA (→ TRN)                              | yes (E only) |
| Tier 1 pyramidal              | AMPA (FF→T2) · AMPA (skip→T5) · AMPA (long-range→VIP)    | yes (E only) |
| Tier 2 pyramidal (`L2_*`)     | AMPA (FF) · AMPA (skip→T6) · AMPA (long-range→VIP) · AMPA (→PV/SOM) · **AMPA + GABA_A (recurrent)** | **VIOLATION** |
| Tier 3 pyramidal (`L3_*`)     | AMPA (FF) · AMPA (long-range→VIP) · AMPA (→PV/SOM) · **AMPA + GABA_A (recurrent)** | **VIOLATION** |
| Tier 4 pyramidal (`L4_*`)     | AMPA (FF) · AMPA (long-range→VIP) · AMPA (→PV/SOM) · **AMPA + GABA_A (recurrent)** | **VIOLATION** |
| Tier 5 pyramidal (`L5_*`)     | AMPA (FF) · AMPA (long-range→VIP) · AMPA (→PV/SOM) · NMDA (top-down→T3) · **AMPA + GABA_A (recurrent)** | **VIOLATION** |
| Tier 6 pyramidal (`L6_*`)     | AMPA (FF→T7) · AMPA (→PV/SOM) · NMDA (top-down→T2) · NMDA (→ TRN) | yes (E only — no recurrent) |
| Tier 7 pyramidal (`output_*`) | AMPA (→ output_pop)                                      | yes        |
| `*_PV`                        | GABA_A (→pyr)                                            | yes (I only) |
| `*_SOM`                       | GABA_A (→pyr)                                            | yes (I only) |
| `*_VIP`                       | GABA_A (→SOM)                                            | yes (I only) |
| `thalamus_reticular` (TRN)    | GABA_A (→ tier 0)                                        | yes (I only) |

## Violations

**Recurrent within-tier wiring at tiers 2-5 violates Dale's principle.**

Source: `nimcp_snn_hierarchical.c` lines 358-384, the within-tier recurrent
loop:

```c
synapse_type_t type = ((sp + dp) % 5 == 0) ?
    SYNAPSE_GABA_A : SYNAPSE_AMPA;
float w_mean = (type == SYNAPSE_GABA_A) ? -4.0f * w_exc : w_exc;
```

For each ordered pair (sp, dp) in the same tier, `(sp+dp) % 5 == 0` makes
the connection inhibitory; otherwise excitatory. Since one src pop `sp`
participates in many such pairs across different `dp`, a single pyramidal
source pop emits BOTH AMPA and GABA_A synapses, depending on which dst it
is connecting to. That is biologically a chimera — pyramidal neurons in
real cortex are uniformly glutamatergic; inhibitory tone comes from a
separate population (PV/SOM/VIP basket cells), which the wiring already
provides as dedicated sub-pops.

The mod-5 trick was almost certainly a placeholder added before the
PV/SOM/VIP triplet was wired (it predates the P2.2 disinhibition block).
With the proper inhibitory sub-pops now in place, the dual-class recurrent
connection is redundant AND incorrect.

Tier 6 escapes the violation only because its `recurrent_connectivity` is
0.0 in `TIER_DEFS[]` (no recurrent loop), and tier 0/1/7 likewise.

## Proposed fix sketch

Two reasonable options:

**Option A (preferred)**: drop the GABA_A branch from the within-tier
recurrent loop entirely. Recurrence becomes pure AMPA E→E; inhibitory
balance is supplied by the existing PV→pyr / SOM→pyr GABA_A pathways
(already wired for tiers 2-6 and biologically correct). This is the
canonical cortical microcircuit and matches both Dale and the surrounding
P2.2 design.

```c
/* recurrent: pyr → pyr, AMPA only */
int nc = snn_network_connect_populations(net,
    pop_map[sp], pop_map[dp],
    SNN_TOPO_RANDOM, rec_conn,
    SYNAPSE_AMPA, w_exc, 0.05f);
```

**Option B**: route the `(sp+dp)%5 == 0` branch through the matched PV pop
of the same tier (pyr → PV → pyr) instead of pyr → pyr GABA_A. More
faithful to the disynaptic FFI motif but adds code volume and depends on
PV pops always being created (currently true via `inh_ok[i]`). Reserved
for a follow-up if behavioural testing shows the simpler option A
under-inhibits at the steady state.

Either option must:
- Preserve the existing E/I balance — currently `0.8·w − 0.2·4·w = 0` net
  drive per recurrent spike. Removing the GABA branch reduces net drive
  per spike to `0.8·w` (positive); the PV→pyr pathway must compensate so
  the closed-loop firing rate stays within the homeostatic target band.
- Bump the wiring schema version in `nimcp_snn_network.c`
  (`SNN_CHECKPOINT_VERSION`) so any cached `.snn` file from the
  Dale-violating wiring is rejected on load.

## Tests added (2026-04-27)

- **Unit** — `test/unit/snn/test_snn_dale_unit.cpp` (gtest, 5 cases):
  empty net, clean E-only, clean I-only, single-pop no-outgoing, mixed
  violator. Drives the validator against synthetic networks.
- **Integration** — `test/integration/snn/test_snn_dale_integration.c`
  (libcheck), two cases:
  1. `test_dale_catches_simulated_recurrent_pattern` — 5-pop mini-network
     mirroring the production within-tier mod-5 GABA / AMPA branching.
     Asserts the validator detects ≥ 1 violator. Always runs (~1 s).
  2. `test_dale_holds_on_full_hierarchy` — builds the real 1.8M-neuron
     hierarchy via `snn_create_hierarchical_network()` and asserts
     `snn_network_validate_dale() == 0`. Gated on
     `NIMCP_RUN_FULL_HIERARCHY=1` because the builder is ~30 GB RSS and
     several minutes of wiring; it OOM-kills on 32 GB-class CI boxes.
     **Will fail when run** until option A above is applied — the
     production wiring DOES violate Dale right now, and the failure is
     the audit signal (Wave C walkthrough rule: do not silence).
- **Regression** — `test/regression/snn/test_snn_dale_regression.cpp`
  (gtest, 2 cases): assembles a tiny 3-pop network where pop A connects
  to dst1 with AMPA AND to dst2 with GABA_A, asserts validator returns
  non-zero with a non-empty error message; second case verifies the
  validator is silent on a clean E-only mirror of the same topology.

## Validator API

```c
/* include/snn/nimcp_snn_network.h */
int snn_network_validate_dale(const snn_network_t* net,
                              char* err_buf, size_t err_buf_sz);
```

Returns 0 on clean networks (incl. NULL / empty), positive count on
violation. Pure read — never mutates wiring. Implementation in
`src/snn/nimcp_snn_network.c` next to `snn_network_validate`.
