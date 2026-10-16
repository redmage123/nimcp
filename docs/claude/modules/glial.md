# Glial Cell Infrastructure

**Last updated:** 2026-04-23 (G1–G8 campaign)
**Status:** Active — wired into brain init and the forward-pass hot path.

Glial cells are non-neuronal cells in the brain that provide metabolic,
structural, and immune support to neurons. NIMCP models three types and
their interactions with the neural graph:

| Glial type | Role in NIMCP | Implementation |
|------------|---------------|----------------|
| Astrocytes | Tripartite synapse modulation, calcium-wave gating of synaptic efficacy (0.8× – 1.2×). | `src/glial/astrocytes/` + forward-pass hook |
| Oligodendrocytes | Myelination decisions, axonal conduction boost. | `src/glial/oligodendrocytes/` + forward-pass hook |
| Microglia | Synaptic surveillance + activity-dependent pruning. | `src/glial/microglia/` + forward-pass hook |
| Myelin sheath | Structural modeling of myelin, saltatory conduction timing. | `src/glial/myelin_sheath/` |

All three cell networks + the myelin sheath network are owned by
`brain_t` and orchestrated through a `glial_integration_t` instance.

## Lifecycle

### Init (brain creation)

During brain init Wave 1 (parallel pool), `init_glial_if_needed()` calls
`nimcp_brain_factory_init_glial_subsystem(brain)` which:

1. Creates `brain->glial = glial_integration_create(base_neural_network, 1000)`.
2. Creates the three cell networks sized from `config.num_astrocytes /
   num_oligodendrocytes / num_microglia` (defaults `neurons/5`, `/7`, `/10`).
3. Wires each network into `brain->glial` via
   `glial_integration_set_astrocyte_network()` / `_set_oligodendrocyte_network()` /
   `_set_microglia_network()`.
4. Enables astrocyte modulation + oligo myelination by default. Microglia
   pruning stays off until `config.enable_microglia_pruning` opts in.
5. Optionally creates the myelin sheath network (`config.enable_myelin_sheath`).
6. **Publishes `brain->glial` onto the neural network**:
   `neural_network_set_glial_integration(base, brain->glial)`. Without this
   the forward-pass hot-path gate stays NULL and the entire subsystem is
   a no-op — the "statue" pattern that F6 fixed for substrate/thalamic.
7. Calls `nimcp_brain_attach_glial(brain)` which invokes
   `glial_integration_auto_assign_spatial()` to populate the synapse→astrocyte,
   neuron→oligo, and synapse→microglia lookup tables before the first
   tick. Honors `config.enable_microglia_pruning` at this step too.

### Tick (brain_decide / brain_learn_vector)

Every 50 ticks in `brain_decide`, `glial_integration_step(brain->glial, t)`
updates calcium dynamics / myelination / microglia surveillance. The
integration layer also runs inside `brain_learn_vector` for consistent
cadence during training.

### Destroy (brain_destroy)

`nimcp_brain_factory_destroy_glial_subsystem(brain)` tears down in
reverse order: integration layer first (releases lookup tables but not
the borrowed network pointers), then the three cell networks. NULL-tolerant,
safe to call twice.

## Hot-path integration

`compute_input_for_neuron()` in `src/core/neuralnet/nimcp_neuralnet.c`
consults the glial system for every firing synapse, applying three
modulation effects *after* STP + semantic-embedding modulation:

```
if (network->glial_integration && transmission > 0) {
    if (should_prune_synapse(gi, pre, post)) {         // G5: microglia
        transmission = 0;
    } else {
        transmission *= get_synaptic_modulation(gi, pre, post);         // G3: astrocyte
        transmission *= 1.0 + 0.5 * get_myelination_factor(gi, pre);    // G4: myelin
    }
}
```

- **G3 astrocyte modulation** returns `1.0` when no astrocyte is assigned
  (safe no-op at brain scale); otherwise `[0.8, 1.2]` based on that
  astrocyte's calcium state.
- **G4 myelin boost** returns `0.0` when no oligodendrocyte is assigned
  (boost factor `1.0` → pass-through); otherwise up to `+0.5` added to
  the multiplier (so transmission can be boosted up to 1.5×). Full
  saltatory delay modeling is deferred.
- **G5 microglia pruning** short-circuits transmission to `0` when the
  synapse is flagged for pruning. Weight remains non-zero; the actual
  weight-kill sweep lives in the learning path. Gated by
  `config.enable_microglia_pruning`, default OFF.

## Config (brain_config_t)

| Field | Default | Purpose |
|-------|---------|---------|
| `enable_glial` | true | Master gate for the whole subsystem. |
| `enable_myelin_sheath` | false | Create a myelin sheath network for detailed biophysics. |
| `num_astrocytes` | `neurons/5` | Astrocyte population capacity. |
| `num_oligodendrocytes` | `neurons/7` | Oligodendrocyte population capacity. |
| `num_microglia` | `neurons/10` | Microglia population capacity. |
| `enable_microglia_pruning` | false | Allow microglia to zero transmission on synapses it flags. Default off until validated end-to-end. |
| `enable_glial_synaptic_modulation` | true (when enable_glial) | G3 gate. |
| `enable_glial_myelin_conduction` | true (when enable_glial + myelin_sheath) | G4 gate. |

Each cell network is capped at `neuron_count` and has a floor of 1.

## synapse_id representation (G7)

Internally, each synapse is keyed by a `uint64`:

```
synapse_id = ((uint64_t)pre_neuron_id << 32) | (uint64_t)post_neuron_id
```

Bijective over `(uint32, uint32)` pairs. This replaces the pre-G7
`pre * 10000 + post` scheme which overflowed uint32 past 430K pre-neurons
and collided deterministically when post ≥ 10000. The widening cascades
through `glial_integration_t` synapse hash tables (UINT64 key type),
`monitored_synapse_t.synapse_id`, `microglia_network_t.monitored_synapse_ids[]`,
and the public `microglia_monitor_synapse / track_synapse_activity /
get_synapse_activity_score / set_synapse_centrality / should_prune_synapse`
API (signature change: uint32 → uint64).

## Accessors (for tests + diagnostics)

All exposed in `include/core/brain/factory/init/nimcp_brain_init_subsystems.h`
as `void*` returns — tests that need field access cast back to the
concrete type after including the relevant glial header:

```c
void* nimcp_brain_get_glial(brain_t brain);
void* nimcp_brain_get_astrocyte_network(brain_t brain);
void* nimcp_brain_get_oligodendrocyte_network(brain_t brain);
void* nimcp_brain_get_microglia_network(brain_t brain);
bool  nimcp_brain_glial_is_enabled(brain_t brain);
```

And on the neural-network side:

```c
bool  neural_network_set_glial_integration(neural_network_t, void* glial);
void* neural_network_get_glial_integration(neural_network_t);
```

## Tests

- `test/unit/brain/test_brain_glial_init.cpp` — 9 tests covering creation,
  accessors, idempotent attach, destroy safety, NULL tolerance, the C1
  regression guard (`NeuralNetworkReceivesGlialPointer`), and
  modulation-disabled identity behavior.
- `test/integration/glial/test_glial_hot_path.cpp` — 5 tests covering
  forward-pass behavior, unassigned-synapse safety, and G7 bijection
  across old collision points.

All 14 tests pass on local (CUDA 12.0) and pod (CUDA 12.8) builds.

## Related modules

- **Brain immune system** (`brain-immune.md`) — microglia cytokine signals
  feed into the immune subsystem.
- **Training-immune integration** (`training-immune.md`) — learning-phase
  gating respects microglia-reported health.
- **Metabolic modulation** (`metabolic-modulation.md`) — astrocyte ATP
  tracking interacts with substrate metabolic state.
- **FEP bridges** — each glial type has a bidirectional FEP bridge
  (`include/glial/*/nimcp_*_fep_bridge.h`).

## Known gaps (roadmap)

- Saltatory conduction with true spike-delay queues (G4 is currently an
  efficacy boost, not a true timing model).
- Per-region substrate sensitivity (prefrontal vs. V1) — deferred to G-bus work.
- Microglia pruning weight-kill sweep in the learning path (forward-pass
  gate zeros transmission but doesn't touch the weight).
