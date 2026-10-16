# NIMCP Enhancements Architecture Plan

**Date:** 2025-11-04
**Status:** 🚧 PLANNING
**Target Version:** 2.7.0

---

## Overview

This document outlines the architecture for 5 major enhancements to NIMCP, building on Phase 3 COW implementation.

---

## Enhancement 1: Separate Weights from Statistics

### Problem
Current `adaptive_network_t` bundles immutable weights with mutable statistics, preventing indefinite COW sharing during training.

### Solution Architecture

```c
// src/plasticity/adaptive/nimcp_adaptive.h

/**
 * @brief Immutable network weights (shared across clones)
 */
typedef struct network_weights_struct {
    neural_network_t* base_network;      // Neural network with weights
    adaptive_spike_params_t spike_params; // Spike encoding config

    uint32_t* refcount;                  // Reference count for COW
    pthread_mutex_t* refcount_mutex;     // Thread-safe refcount
} network_weights_t;

/**
 * @brief Mutable network statistics (per-brain)
 */
typedef struct network_stats_struct {
    float running_sparsity;              // Current sparsity
    uint64_t total_inferences;           // Total forward passes
    uint64_t total_learning_steps;       // Total learning updates
    float avg_inference_time_us;         // Average inference time
    uint32_t active_neuron_count;        // Recent active neurons
} network_stats_t;

/**
 * @brief Adaptive network with separated weights and stats
 */
typedef struct adaptive_network_struct {
    network_weights_t* weights;          // Shared, immutable
    network_stats_t* stats;              // Private, mutable

    adaptive_network_config_t config;    // Configuration
    bool owns_weights;                   // Do we own weights?
} adaptive_network_internal_t;
```

### Migration Strategy
1. Create new structures in `nimcp_adaptive.h`
2. Update `adaptive_network_create()` to allocate both
3. Modify all access to use `network->weights->...` and `network->stats->...`
4. Update COW functions to only share weights
5. Maintain backward compatibility with typedef

### Benefits
- 95%+ memory savings for shared inference during training
- Each brain gets private statistics
- Cleaner separation of concerns

---

## Enhancement 2: Batch API

### Problem
Current API only supports single-sample inference, inefficient for batched workloads.

### Solution Architecture

```c
// src/include/nimcp.h

/**
 * @brief Batch prediction configuration
 */
typedef struct {
    const float** features;      // Array of feature vectors
    uint32_t batch_size;         // Number of samples
    uint32_t feature_dim;        // Dimension of each sample

    // Optional batch settings
    uint32_t max_parallel;       // Max parallel inferences (0 = auto)
    bool return_raw_outputs;     // Return raw activations?
} nimcp_batch_input_t;

/**
 * @brief Batch prediction results
 */
typedef struct {
    char** labels;               // Predicted labels [batch_size]
    float* confidences;          // Confidence scores [batch_size]
    float** raw_outputs;         // Raw output vectors (optional)
    uint32_t batch_size;         // Number of results
} nimcp_batch_output_t;

/**
 * @brief Batch prediction API
 *
 * @param brain Brain handle
 * @param batch_input Batch input configuration
 * @param batch_output Output buffer (allocated by caller)
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_predict_batch(
    nimcp_brain_t brain,
    const nimcp_batch_input_t* batch_input,
    nimcp_batch_output_t* batch_output
);

/**
 * @brief Batch learning API
 *
 * @param brain Brain handle
 * @param features Array of feature vectors [batch_size][feature_dim]
 * @param labels Array of label strings [batch_size]
 * @param confidences Array of confidence weights [batch_size]
 * @param batch_size Number of training examples
 * @return Average loss over batch
 */
float nimcp_brain_learn_batch(
    nimcp_brain_t brain,
    const float** features,
    const char** labels,
    const float* confidences,
    uint32_t batch_size
);
```

### Implementation Plan
1. Add batch structures to public API
2. Implement `nimcp_brain_predict_batch()` in `src/api/nimcp.c`
3. Leverage existing thread pool from `src/utils/thread/nimcp_thread_pool.h`
4. Parallelize across available cores
5. Handle memory efficiently with reusable buffers

### Benefits
- 5-10x throughput improvement for batched workloads
- Better cache locality
- Enables GPU batching in future

---

## Enhancement 3: SIMD Vectorization

### Problem
Current forward pass uses scalar operations, leaving 4-8x performance on the table.

### Solution Architecture

```c
// src/core/neuralnet/nimcp_neuralnet_simd.h

/**
 * @brief SIMD-accelerated forward pass
 *
 * Requires: AVX2 or AVX-512 CPU support
 * Falls back to scalar if not available
 */

#ifdef __AVX2__
#include <immintrin.h>

/**
 * @brief Vectorized matrix-vector multiplication
 *
 * Computes: output = weights * input
 * Uses AVX2 for 8-wide float SIMD
 *
 * @param input Input vector [n]
 * @param weights Weight matrix [m][n]
 * @param output Output vector [m]
 * @param m Number of output neurons
 * @param n Number of input neurons
 */
void neural_network_forward_simd_avx2(
    const float* input,
    const float* weights,
    float* output,
    uint32_t m,
    uint32_t n
);
#endif

#ifdef __AVX512F__
/**
 * @brief AVX-512 vectorized forward pass (16-wide)
 */
void neural_network_forward_simd_avx512(
    const float* input,
    const float* weights,
    float* output,
    uint32_t m,
    uint32_t n
);
#endif

/**
 * @brief Auto-detect best SIMD implementation
 */
void neural_network_forward_auto(
    const float* input,
    const float* weights,
    float* output,
    uint32_t m,
    uint32_t n
);
```

### Implementation Plan
1. Create `src/core/neuralnet/nimcp_neuralnet_simd.c`
2. Implement AVX2 version (most common)
3. Implement AVX-512 version (high-end CPUs)
4. Add runtime CPU feature detection
5. Integrate with existing `neural_network_forward()`
6. Add compiler flags to CMakeLists.txt

### CPU Feature Detection
```c
// Use __builtin_cpu_supports() or cpuid
bool has_avx2 = __builtin_cpu_supports("avx2");
bool has_avx512 = __builtin_cpu_supports("avx512f");
```

### Benefits
- 4-8x faster inference (AVX2: 8 floats/cycle, AVX-512: 16 floats/cycle)
- Lower latency for real-time applications
- Energy efficient (more work per instruction)

---

## Enhancement 4: Async Inference

### Problem
Synchronous API blocks caller during inference, wasting CPU cycles.

### Solution Architecture

```c
// src/include/nimcp.h

/**
 * @brief Opaque handle to async operation
 */
typedef struct nimcp_future_handle* nimcp_future_t;

/**
 * @brief Future status
 */
typedef enum {
    NIMCP_FUTURE_PENDING,    // Still computing
    NIMCP_FUTURE_READY,      // Result available
    NIMCP_FUTURE_ERROR,      // Computation failed
    NIMCP_FUTURE_CANCELLED   // Operation cancelled
} nimcp_future_status_t;

/**
 * @brief Submit async inference
 *
 * Returns immediately with future handle
 * Computation happens on background thread
 *
 * @param brain Brain handle
 * @param features Input features
 * @param num_features Feature dimension
 * @return Future handle or NULL on error
 */
nimcp_future_t nimcp_brain_predict_async(
    nimcp_brain_t brain,
    const float* features,
    uint32_t num_features
);

/**
 * @brief Check future status (non-blocking)
 *
 * @param future Future handle
 * @return Current status
 */
nimcp_future_status_t nimcp_future_status(nimcp_future_t future);

/**
 * @brief Wait for future completion (blocking)
 *
 * @param future Future handle
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @param out_label Output label buffer (min 64 bytes)
 * @param out_confidence Output confidence score
 * @return NIMCP_OK if ready, NIMCP_ERROR_TIMEOUT if timed out
 */
nimcp_status_t nimcp_future_wait(
    nimcp_future_t future,
    uint32_t timeout_ms,
    char* out_label,
    float* out_confidence
);

/**
 * @brief Cancel pending future
 *
 * @param future Future handle
 * @return true if cancelled, false if already completed
 */
bool nimcp_future_cancel(nimcp_future_t future);

/**
 * @brief Destroy future and free resources
 *
 * @param future Future handle
 */
void nimcp_future_destroy(nimcp_future_t future);
```

### Implementation Plan
1. Create `src/utils/async/nimcp_async.h`
2. Implement future structure with condition variable
3. Use existing thread pool for background execution
4. Add timeout support with `pthread_cond_timedwait()`
5. Handle cancellation gracefully

### Internal Structure
```c
typedef struct nimcp_future_handle {
    nimcp_future_status_t status;

    // Result storage
    char label[64];
    float confidence;
    nimcp_status_t error_code;

    // Synchronization
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    // Cancellation
    bool cancel_requested;
} nimcp_future_handle_t;
```

### Benefits
- Better throughput (overlap I/O with compute)
- Responsive UI in interactive applications
- Pipeline multiple inferences

---

## Enhancement 5: Automatic Checkpointing

### Problem
No fault tolerance - training progress lost on crash or power failure.

### Solution Architecture

```c
// src/cognitive/checkpointing/nimcp_checkpointing.h

/**
 * @brief Checkpoint configuration
 */
typedef struct {
    const char* checkpoint_dir;      // Directory for checkpoints
    uint32_t interval_steps;         // Save every N learning steps
    uint32_t max_checkpoints;        // Keep last N checkpoints (0 = unlimited)
    bool compress;                   // Use gzip compression?
    bool atomic_write;               // Use atomic rename?
} nimcp_checkpoint_config_t;

/**
 * @brief Enable automatic checkpointing
 *
 * Brain will auto-save every N learning steps
 *
 * @param brain Brain handle
 * @param config Checkpoint configuration
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_enable_checkpointing(
    nimcp_brain_t brain,
    const nimcp_checkpoint_config_t* config
);

/**
 * @brief Manually trigger checkpoint
 *
 * @param brain Brain handle
 * @return NIMCP_OK on success
 */
nimcp_status_t nimcp_brain_checkpoint(nimcp_brain_t brain);

/**
 * @brief Load latest checkpoint
 *
 * Searches checkpoint_dir for latest .ckpt file
 *
 * @param checkpoint_dir Directory containing checkpoints
 * @return Brain handle or NULL if no checkpoint found
 */
nimcp_brain_t nimcp_brain_load_latest_checkpoint(const char* checkpoint_dir);

/**
 * @brief List available checkpoints
 *
 * @param checkpoint_dir Directory to search
 * @param checkpoints Output array of checkpoint info
 * @param max_checkpoints Maximum checkpoints to return
 * @return Number of checkpoints found
 */
uint32_t nimcp_checkpoints_list(
    const char* checkpoint_dir,
    nimcp_checkpoint_info_t* checkpoints,
    uint32_t max_checkpoints
);

/**
 * @brief Checkpoint metadata
 */
typedef struct {
    char filepath[256];              // Full path to checkpoint
    uint64_t timestamp;              // Unix timestamp
    uint64_t learning_step;          // Learning step number
    size_t size_bytes;               // Checkpoint size
} nimcp_checkpoint_info_t;
```

### Implementation Plan
1. Create `src/cognitive/checkpointing/nimcp_checkpointing.c`
2. Hook into `brain_learn_example()` to check interval
3. Use atomic write: save to `.tmp`, then rename
4. Implement checkpoint rotation (keep last N)
5. Add compression with zlib if enabled
6. Store metadata in JSON sidecar file

### Checkpoint Format
```
checkpoints/
├── brain_classifier_step_1000.ckpt
├── brain_classifier_step_1000.json      # Metadata
├── brain_classifier_step_2000.ckpt
├── brain_classifier_step_2000.json
└── latest -> brain_classifier_step_2000.ckpt  # Symlink
```

### Metadata JSON
```json
{
  "version": "2.7.0",
  "brain_name": "classifier",
  "learning_step": 2000,
  "timestamp": 1699123456,
  "accuracy": 0.95,
  "num_neurons": 1000,
  "num_synapses": 50000
}
```

### Benefits
- Fault tolerance - resume after crash
- Experiment tracking - compare checkpoints
- Reproducibility - restore exact training state

---

## Testing Strategy

### Unit Tests (Per Enhancement)
- Enhancement 1: Test weights/stats separation, refcounting
- Enhancement 2: Test batch API correctness, edge cases
- Enhancement 3: Test SIMD correctness vs scalar baseline
- Enhancement 4: Test async futures, cancellation, timeouts
- Enhancement 5: Test checkpoint save/load, rotation

### Integration Tests
- E1 + COW: Verify indefinite sharing during training
- E2 + E4: Batch async inference
- E3 + E2: SIMD with batching
- E5 + E1: Checkpoint with weights/stats separation

### E2E Tests
- Full training pipeline with all enhancements
- Crash recovery scenario (kill during training, resume)
- Multi-threaded batch inference with COW clones

### Lint Tests
- Run `clang-format` on all new code
- Run `cppcheck` for static analysis
- Check for memory leaks with Valgrind

### Regression Tests
- Verify Phase 3 COW still works
- Verify existing API unchanged
- Verify performance not degraded

---

## Implementation Order

### Phase 1: Foundation (Week 1)
1. ✅ Enhancement 1: Weights/Stats Separation
   - Core implementation
   - Unit tests
   - Integration with Phase 3 COW

### Phase 2: Performance (Week 2)
2. ✅ Enhancement 3: SIMD Vectorization
   - AVX2 implementation
   - CPU detection
   - Benchmarks
3. ✅ Enhancement 2: Batch API
   - Batch structures
   - Parallel execution
   - Unit tests

### Phase 3: Reliability (Week 3)
4. ✅ Enhancement 5: Checkpointing
   - Core implementation
   - Compression
   - Rotation
5. ✅ Enhancement 4: Async Inference
   - Future API
   - Thread pool integration
   - Timeout handling

### Phase 4: Testing & Polish (Week 4)
6. ✅ Comprehensive test suite
7. ✅ Documentation
8. ✅ Benchmarks
9. ✅ Release v2.7.0

---

## Performance Targets

| Metric | Baseline (v2.6) | Target (v2.7) | Improvement |
|--------|-----------------|---------------|-------------|
| **Inference latency** | 2.0ms | 0.3ms | 6.7x faster |
| **Batch throughput** | 500/sec | 5000/sec | 10x faster |
| **Memory (10 clones)** | 680MB | 68MB | 90% reduction |
| **Training stability** | Crash = loss | Resume from checkpoint | ∞ improvement |

---

## API Compatibility

### Breaking Changes
None - all enhancements are additive.

### Deprecated APIs
None planned.

### Migration Path
All existing code continues to work. New APIs are opt-in.

---

## Files to Create/Modify

### New Files
```
src/plasticity/adaptive/nimcp_weights.h
src/plasticity/adaptive/nimcp_weights.c
src/core/neuralnet/nimcp_neuralnet_simd.h
src/core/neuralnet/nimcp_neuralnet_simd.c
src/utils/async/nimcp_async.h
src/utils/async/nimcp_async.c
src/cognitive/checkpointing/nimcp_checkpointing.h
src/cognitive/checkpointing/nimcp_checkpointing.c
src/tests/test_weights_separation.cpp
src/tests/test_batch_api.cpp
src/tests/test_simd.cpp
src/tests/test_async.cpp
src/tests/test_checkpointing.cpp
src/tests/test_integration_v27.cpp
src/tests/test_e2e_v27.cpp
src/tests/test_regression_v27.cpp
```

### Modified Files
```
src/include/nimcp.h                    # Add new APIs
src/api/nimcp.c                        # Implement batch API
src/plasticity/adaptive/nimcp_adaptive.h  # Add weights/stats
src/plasticity/adaptive/nimcp_adaptive.c  # Separate weights/stats
src/core/brain/nimcp_brain.c           # Integrate checkpointing
CMakeLists.txt                         # Add SIMD flags
```

---

## Success Criteria

✅ All unit tests pass
✅ All integration tests pass
✅ All E2E tests pass
✅ No regressions in existing functionality
✅ Performance targets met
✅ Memory targets met
✅ Code passes lint checks
✅ Documentation complete
✅ Backward compatible

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| SIMD not available on CPU | Runtime detection, fallback to scalar |
| Thread pool exhaustion | Queue and backpressure handling |
| Checkpoint disk full | Check space before write, fail gracefully |
| Async operation leaked | Destructor cleanup, leak tests |
| Breaking existing code | Comprehensive regression suite |

---

**Next Steps:** Begin Phase 1 - Enhancement 1: Weights/Stats Separation
