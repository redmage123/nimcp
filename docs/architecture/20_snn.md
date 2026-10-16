# SNN Architecture

**Last Updated:** 2026-04-23 (schema v7 + G7 synapse_id + G8 lightweight bridges)

## Recent updates (2.7.0 release)

- **G7** widened `synapse_id` from uint32 to **uint64** (bijective
  `(pre<<32)|post`). The pre-G7 hash `pre*10000 + post` overflowed
  uint32 at 430K pre-neurons and collided deterministically when
  post ≥ 10000 — modulation / pruning would route to the wrong
  astrocyte / microglia at 2M scale. Propagated through
  `glial_integration_t`, `monitored_synapse_t`, the microglia
  public API (signature change: uint32 → uint64).
- **G8** converted the four cognitive SNN bridges
  (attention, mirror-neurons, emotion, working-memory) off the dense
  `snn_network_add_population` path to the lightweight CSR variant.
  Prior to G8 these bridges exhausted the base `neural_network_t` slot
  budget, leaving populations like `wm_inhibition`, `emotion_va`, and
  mirror-neurons' `hidden` / `output` empty at init. All 17 bridge
  pop-creation sites now use `snn_network_add_population_lightweight()`.
  Each bridge calls `snn_network_finalize_connections()` once after all
  `connect_populations` calls.
- **G8 H1 shunts** added per bridge: dense `input_pop` (id 0) →
  bridge's primary lightweight input pop, and bridge's output pop →
  dense `output_pop` (id 2), so `snn_network_set_inputs()` and
  `snn_network_get_outputs()` reach the bridge's custom graph.
- **`snn_network_step_sparse` lightweight-safe branch (M1)**: reads
  from `pop->external_current[n]` when `pop->lightweight`, avoiding
  a segfault on empty `neuron_ids[]`.

## Scale

- **1,800,000 neurons** (`DEFAULT_SNN_NEURONS` in `brain_daemon.py`)
- ~1.45 billion synapses across ~46 populations
- Hierarchical: 7 tiers (L1_feature → L7_output) plus input_pop / output_pop
- CSR (Compressed Sparse Row) incoming-synapse storage per population
- This is the **primary learner** in the architecture — ANN (150K) serves
  as teacher, SNN does the biological learning.

Override via `--snn-neuron-count N` on the daemon; the brain's checkpoint
format (schema v7) preserves all per-neuron state across restarts.

## Per-Population Data

Each `snn_population_t` owns:

```c
typedef struct snn_population_t {
    uint32_t id, n_neurons;
    char name[64];
    neuron_type_t neuron_type;
    bool lightweight;

    // Spike / LIF state
    nimcp_tensor_t* membrane_v;
    nimcp_tensor_t* spike_output;
    nimcp_tensor_t* refractory;

    // External current (set by upstream inputs)
    float* external_current;          // [n_neurons]

    // Homeostatic state — PERSISTED in .snn (schema v7+)
    float* threshold_offset;          // [n_neurons] — IP threshold shift
    float* neuron_rate_ema;           // [n_neurons] — per-neuron rate EMA
    float* depression;                // [n_neurons] — depression level

    // Synapse storage (incoming)
    snn_csr_storage_t* incoming_csr;

    // Population-level stats
    float firing_rate_ema;
} snn_population_t;
```

## CSR Synapse Storage

See `include/snn/nimcp_snn_synapse.h`. Per population:

```c
typedef struct snn_csr_storage_s {
    uint32_t*          row_ptr;        // [n_neurons + 1]
    snn_csr_synapse_t* entries;        // [n_synapses] each is (src_pop, src_neuron, weight)
    uint32_t           n_neurons;
    uint32_t           n_synapses;
    uint32_t           capacity;
    bool               finalized;

    // Flattened for GPU
    float*             weights;        // [n_synapses] (host)
    uint32_t*          flat_col_idx;   // [n_synapses] (host, global neuron index)
    bool               gpu_ready;

    // NEW (V2, 2026-04): Persistent GPU copies
    void*              d_weights;
    void*              d_flat_col_idx;
    void*              d_row_ptr;
    bool               gpu_resident;
} snn_csr_storage_t;
```

### Build Flow

1. `snn_csr_create(n_neurons, estimated_synapses)` — allocate
2. `snn_csr_add_entry_coo(csr, dst, src_pop, src_neuron, weight)` × N
3. `snn_csr_finalize(csr)` — sort by dst, build row_ptr
4. `snn_csr_prepare_gpu(csr, pop_offsets, n_pops)` — flatten to host arrays
5. `snn_csr_upload_to_gpu(csr, gpu_ctx)` — NEW, V2, upload to device

### Query Flow

For neuron `i` in population `p`, incoming synapses live at
`csr->entries[csr->row_ptr[i] : csr->row_ptr[i+1]]`.

## Checkpoint Schema (`.snn` file format)

```
Header:
  magic              (u32)  = SNN_CHECKPOINT_MAGIC
  version            (u32)  = SNN_CHECKPOINT_VERSION
  wiring_schema      (u32)  = 7 (2026-04-19)

Config:
  snn_config_t       (struct)
  is_training        (bool)

Legacy neuron blob:
  total_neurons      (u32)
  per neuron:
    state, threshold, bias (3 floats)
    n_synapses (u32)
    per synapse: weight (float) + [v2: target_id (u32)]

Lightweight populations:
  n_lightweight      (u32)
  n_populations      (u32)
  per population:
    id, n_neurons    (u32 each)
    neuron_type      (enum)
    name             (char[64])
    CSR:
      n_neurons, n_synapses (u32 each)
      row_ptr        (u32[n_neurons + 1])
      entries        (snn_csr_synapse_t[n_synapses])
      has_gpu        (u8) = 0 (GPU arrays regenerated on demand)
    NEW V7 — homeostatic state:
      has_homeostatic (u8)
      if has_homeostatic:
        threshold_offset (float[n_neurons])
        neuron_rate_ema  (float[n_neurons])
        depression       (float[n_neurons])
```

### Schema Compatibility

`SNN_MIN_COMPATIBLE_SCHEMA = 5`. V5, V6 files load cleanly with V7 binaries
(homeostatic state skipped, defaults to seeds). V7 files rejected by V5/V6
binaries (can't safely skip unknown format).

### Skip-Path Fix (2026-04-19)

When a lightweight population fails to create during load (e.g., OOM),
the loader skips its CSR bytes. The skip logic must account for the
full on-disk layout:

```
[csr_n][csr_nnz]                        # 8 bytes
[row_ptr]        csr_n+1 × u32
[entries]        csr_nnz × snn_csr_synapse_t (12 bytes each)
[has_gpu]        1 byte
[if has_gpu=1:   csr_nnz × (weight+col)  8 bytes each — currently unused path]
[if v7+:
  [has_homeostatic] 1 byte
  [if has_homeostatic=1:
    threshold_offset  pop_n × float
    neuron_rate_ema   pop_n × float
    depression        pop_n × float
  ]
]
```

Earlier skip code missed the v7 homeostatic block, leaving the file cursor
mid-array and corrupting subsequent population reads. Fixed by adding a
`wiring_schema >= 7` branch that seeks past `3 × pop_n × sizeof(float)`.

## Population Topology (hierarchical)

```
input_pop (1024 neurons)
    │   (fan-out)
    ▼
tier L1_feature   (20K neurons × few populations)
    │   (recurrent + forward)
    ▼
tier L2_pattern
    │
    ▼
  ... tiers L3-L7 ...
    │
    ▼
tier L7_integration
    │   (fan-in convergence)
    ▼
output_pop (2048 neurons)
```

Skip connections: L1→L5, L2→L6 (biological cortical bypass).

## GPU Processing

Per SNN step:
```
1. Upload spike_output tensors (concatenated) to GPU    # small (~7MB)
2. For each population:
   a. Upload external_current                           # small (~7MB)
   b. Launch kernel_snn_isyn_csr:
      - 1 thread per destination neuron
      - iterates incoming synapses
      - checks src spike, accumulates weight
   c. Download I_syn                                    # small
3. Launch kernel_lif_forward on all neurons             # fast
4. Download spike flags                                 # small
5. Update traces / homeostasis on CPU
```

### V2 Optimization (Phase A.1, 2026-04)

Before V2: weights, col_indices, row_ptr were re-uploaded every step
(~12 GB/step for 1.45B synapses → PCIe-bound, ~1.3s/step).

After V2: CSR data lives on GPU permanently (uploaded once in `upload_to_gpu`).
Host-side weight updates sync via `snn_csr_sync_weights_to_gpu()` during
homeostatic scaling.

Expected SNN step time: 1.3s → 100-200ms.

## Training Mechanisms

- **R-STDP** (Reward-modulated Spike-Timing Dependent Plasticity) —
  sequential per-sample eligibility traces + dopamine modulation
- **Surrogate gradient** (for BPTT through spikes)
- **Homeostatic scaling** — per-population, keeps firing rate near 3% target
- **Intrinsic plasticity** — per-neuron threshold adaptation
- **Short-term depression** — transient synaptic fatigue

All run per-sample — no batch-safe equivalents currently.

## Key Constants

| Name | Value | Notes |
|------|-------|-------|
| Target firing rate | 3% | 0.03 in code |
| IP gain | 0.5 | `ip_gain` |
| IP cap | ±10 mV | Per-neuron threshold shift limit |
| Depression jump | 0.2 | On each fire |
| Depression decay | 0.95 / step | |
| Depression cap | 0.5 | |
| Emergency range (wide) | ±10% | When rate >3× or <1/3 target |
| Emergency range (normal) | ±2% | Otherwise |
| Metabolic budget | 0.8 × fan_in | Per-neuron `sum|weights|` cap |
| R-STDP learning rate | 0.0005 | |

## Known Limitations

1. SNN `kernel_snn_isyn_csr` iterates destinations, not sources. A forward-
   push kernel could be 10-30× faster at 1-10% firing sparsity but hasn't
   been implemented.

2. LIF simulation uses 100 timesteps/sample. Biology does recognition in
   ~20-50. Reducing window is low-risk 2-4× speedup (Phase 4.4, deferred).

3. R-STDP eligibility traces are sequential by design — batching would
   destroy temporal ordering.

## See Also

- [30_gpu_memory.md](30_gpu_memory.md) — GPU memory lifecycle
- [10_training_paradigm.md](10_training_paradigm.md) — how SNN fits into training
