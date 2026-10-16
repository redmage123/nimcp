# Key API Patterns

## Tensor Library (`include/utils/tensor/nimcp_tensor.h`)

```c
// Create: 3 args — dims array, rank (not ndims), dtype
nimcp_tensor_t* t = nimcp_tensor_create(dims, rank, NIMCP_DTYPE_F32);

// Sum returns tensor*, NOT scalar
nimcp_tensor_t* sum_t = nimcp_tensor_sum(t);
double val = nimcp_tensor_get_flat(sum_t, 0);    // Extract scalar from tensor

// Norms and in-place ops
double norm = nimcp_tensor_norm_p(t, 2.0);       // Returns double directly
nimcp_tensor_mul_scalar_(t, 0.5f);               // In-place multiplication

nimcp_tensor_destroy(t);
```

**GOTCHA**: `nimcp_tensor_create` requires 3 args: `(dims, rank, NIMCP_DTYPE_F32)`. The dtype parameter is mandatory.

**GOTCHA**: `nimcp_tensor_sum()` returns `nimcp_tensor_t*`, must extract with `nimcp_tensor_get_flat()`

**GOTCHA**: `op_div` uses epsilon clamping (1e-7) instead of returning 0. No LOG_WARN emitted.

---

## Mutex API Pattern (IMPORTANT)

```c
// Option 1: Allocate + init (heap)
nimcp_mutex_t* m = nimcp_mutex_create(attr);  // Returns nimcp_mutex_t*, NOT error code
// ... use mutex ...
nimcp_mutex_free(m);  // destroy + free (CORRECT for heap-allocated mutexes)

// Option 2: Init existing struct
nimcp_mutex_t m;
nimcp_mutex_init(&m, NULL);
// ... use mutex ...
nimcp_mutex_destroy(&m);  // destroy only, no free
```

**GOTCHA**: `nimcp_mutex_create(attr)` returns `nimcp_mutex_t*`, NOT an error code.

**GOTCHA**: `nimcp_mutex_free()` = destroy + free. This IS correct for heap-allocated mutexes from `nimcp_mutex_create()`. This is NOT a bug.

**GOTCHA**: `mutex_attr_t` supports `MUTEX_TYPE_NORMAL`, `MUTEX_TYPE_RECURSIVE`, `MUTEX_TYPE_ERRORCHECK`.

**Deadlock prevention**: Never call public mutex-locking functions from within locked code. Create `*_unlocked()` helpers instead.

---

## Brain API

```c
// Public handle vs internal pointer
nimcp_brain_t* handle;          // Public handle
brain_t* brain = handle->internal_brain;  // Internal pointer

// Inference and training
brain_decide(brain, features, num_features);   // Inference
brain_apply_reward_learning(brain, reward);    // Training

// Decision lifecycle
copy_decision_deep(src);         // Deep copy for cache (not CoW)
brain_free_decision(decision);   // Cleanup (not nimcp_free())
```

**Brain init modes**: `brain_init_mode_t` enum — FULL / FAST / MINIMAL. FAST skips 60+ subsystems, runs 6 of 27 dependency waves. 1.5M neurons: ~14s (FAST) vs 10+ min (FULL). Python: `Brain(..., init_mode='fast')`.

---

## FEP Bridges

**GOTCHA**: FEP bridges return `0` for success, `-1` for errors. They do NOT use NIMCP_OK/NIMCP_ERROR_* codes.

**GOTCHA**: `metabolic_compute_effects()` also returns `0`/`-1`, not NIMCP error codes.

---

## Bio-Async API

**GOTCHA**: `nimcp_bio_promise_complete(promise, result)` takes 2 args, NOT 3.

---

## Brain Immune System

```c
// Thread-safe antigen access (PREFERRED)
brain_immune_get_antigen_copy(immune);  // Returns struct copy under mutex

// AVOID: returns pointer that may dangle
brain_immune_get_antigen(immune);
```

**GOTCHA**: B cells must be in PLASMA state to produce antibodies. State progression: NAIVE -> ACTIVATED -> PLASMA. Use `brain_immune_t_help_b()` to transition.

---

## GPU Stream Pool

```c
cudaStream_t stream = nimcp_gpu_get_pool_stream(ctx);  // Round-robin from 8 streams
nimcp_gpu_sync_pool(ctx);  // Sync all pool streams
```

CPU fallback stubs provided for non-CUDA builds.

---

## Synapse Metadata Pool

Chunked block allocator with 64K entries per block. Pointers are stable across growth (no realloc invalidation).

---

## Atomic Operations

**GOTCHA**: `_Atomic double` requires `-latomic` on GCC/Linux. NVCC does not support C11 `_Atomic` — use `volatile` + GCC `__atomic_*` builtins in headers shared with `.cu` files.

---

## Positional Encoding (`include/utils/encoding/nimcp_positional_encoding.h`)

- Types: `NIMCP_POS_SINUSOIDAL`, `NIMCP_POS_LEARNED`, `NIMCP_POS_ROTARY`, `NIMCP_POS_ALIBI`, `NIMCP_POS_RELATIVE`
- API: `nimcp_pos_encoder_create()`, `nimcp_pos_encode_position()`, `nimcp_pos_rope_apply()`, `nimcp_pos_alibi_get_bias()`

---

## Logging Macros

```c
NIMCP_LOGGING_ERROR("message");
NIMCP_LOGGING_WARN("message");
NIMCP_LOGGING_INFO("message");
NIMCP_LOGGING_DEBUG("message");
NIMCP_LOGGING_TRACE("message");  // Verbose, for hot paths
```

---

## Error Codes

```c
// Correct error codes
NIMCP_ERROR_NULL_POINTER
NIMCP_ERROR_INVALID_STATE
NIMCP_ERROR_INVALID_PARAMETER
NIMCP_ERROR_OPERATION_FAILED
NIMCP_ERROR_NO_MEMORY

// Wrong (don't use)
NIMCP_ERR_*, NIMCP_ERROR_INVALID_ARGUMENT, NIMCP_ERROR_RESOURCE_EXHAUSTED
```

---

## Platform Tiers

**GOTCHA**: Platform tiers use `PLATFORM_TIER_FULL/MEDIUM/CONSTRAINED/MINIMAL`, not `PLATFORM_TIER_0/1/2/3`.

---

## Cytokine Enum Naming

```c
// Base cytokine types (from nimcp_swarm_immune.h)
CYTOKINE_IL1B, CYTOKINE_IL6, CYTOKINE_IL10, CYTOKINE_TNFA

// Brain-specific wrapper (from nimcp_brain_immune.h)
BRAIN_CYTOKINE_IL1 = CYTOKINE_IL1B
BRAIN_CYTOKINE_IL6 = CYTOKINE_IL6
BRAIN_CYTOKINE_IL10 = CYTOKINE_IL10
BRAIN_CYTOKINE_TNF = CYTOKINE_TNFA
BRAIN_CYTOKINE_IFN_GAMMA = 5          // Brain-specific (quarantine)
BRAIN_CYTOKINE_COUNT = 6
```

---

## Metabolic Modulation API (`include/cognitive/common/nimcp_metabolic_modulation.h`)

```c
float nimcp_clamp_f(float value, float min_val, float max_val);

metabolic_modulation_config_t cfg = metabolic_modulation_default_config();

metabolic_input_t input = { .atp_level = atp, .metabolic_capacity = cap };
metabolic_effects_t effects;
metabolic_effects_init_full(&effects);
metabolic_compute_effects(&input, &cfg, &effects);  // Returns 0 on success, -1 on error
```

**GOTCHA**: Always call `metabolic_effects_init_full()` before `metabolic_compute_effects()` to ensure defaults.

---

## Hamiltonian Neural Network API (`include/lnn/nimcp_lnn_hamiltonian.h`)

```c
// Create H-network
lnn_hamiltonian_config_t cfg;
lnn_hamiltonian_config_default(&cfg);
lnn_hamiltonian_net_t* net = lnn_hamiltonian_net_create(state_dim, &cfg);

// Evaluate energy
float H = lnn_hamiltonian_eval(net, q, p);

// Compute gradients (Hamilton's equations: dq/dt = ∂H/∂p, dp/dt = -∂H/∂q)
lnn_hamiltonian_grad(net, q, p, dH_dq, dH_dp);

// Symplectic step (Störmer-Verlet)
lnn_hamiltonian_step_stormer_verlet(net, q, p, input, dt, coupling);

// Monitor energy conservation
float deviation = lnn_hamiltonian_get_energy_deviation(net);
```

**GOTCHA**: H-network is an AUXILIARY network that defines the energy landscape — it is NOT the LNN layer itself.

**GOTCHA**: Momentum tensor `p` must be allocated alongside state `x`. Both are destroyed in layer cleanup.

---

## Fourier Neural Operator API

### Spectral Convolution (`include/training/nimcp_fno_layer.h`)

```c
fno_spectral_conv_t* layer = fno_spectral_conv_create(in_ch, out_ch, n_modes, spatial_size);
fno_spectral_conv_forward(layer, input, output);
fno_spectral_conv_backward(layer, dl_dout, dl_din);
fno_spectral_conv_step(layer, lr);
```

### Audio Processor (`include/training/nimcp_fno_layer.h`)

```c
fno_audio_processor_t* proc = fno_audio_create(mel_size, embed_dim, hidden_ch, n_modes, n_blocks);
fno_audio_forward(proc, mel, mel_size, embedding);
fno_audio_backward(proc, dl_dembed, NULL);
fno_audio_step(proc, lr);
```

### Population Dynamics (`include/snn/nimcp_snn_fno.h`)

```c
snn_fno_config_t cfg;
snn_fno_config_default(&cfg);
snn_fno_population_t* fno = snn_fno_population_create(pop_id, n_neurons, &cfg);
snn_fno_record_pair(fno, v_before, i_syn, v_after, spikes, n);
snn_fno_train(fno, n_epochs);
snn_fno_predict(fno, v_current, i_syn, n, v_next, spikes);
```

**GOTCHA**: FFT size is padded to next power of 2. For mel_size=128, fft_size=128 (already power of 2).

**GOTCHA**: IFFT uses Hermitian symmetry — 2× real-part contribution for positive frequencies + separate Nyquist handling.

---

## Per-Cortex CNN API (`include/training/nimcp_cortex_cnn.h`)

```c
// Create modality-specific CNN
cortex_cnn_processor_t* proc = cortex_cnn_create(CORTEX_CNN_VISUAL, 64);

// Forward (returns embedding pointer, owned by proc)
const float* emb = cortex_cnn_forward_visual(proc, pixels, 64, 64, 3);
const float* emb = cortex_cnn_forward_audio(proc, mel, 128);

// Backward (returns loss)
float loss = cortex_cnn_backward(proc, label, num_outputs);

// Attention-weighted fusion across modalities
uint32_t dim = cortex_cnn_fuse(procs, count, fused_out, max_dim);
```

**GOTCHA**: Types are `CORTEX_CNN_VISUAL`, `CORTEX_CNN_AUDIO`, `CORTEX_CNN_SPEECH`, `CORTEX_CNN_SOMATO`.
