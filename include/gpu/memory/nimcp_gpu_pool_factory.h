/**
 * @file nimcp_gpu_pool_factory.h
 * @brief GPU Memory Pool Factory for Creating Appropriate Allocators
 *
 * WHAT: Factory pattern implementation for GPU memory pool creation
 * WHY:  Abstracts allocator selection based on usage patterns and workload
 * HOW:  Analyzes requirements and creates optimal allocator configuration
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                    GPU POOL FACTORY                              |
 *   |                                                                  |
 *   |  Client Code                                                     |
 *   |       |                                                          |
 *   |       v                                                          |
 *   |  +-------------------+                                           |
 *   |  | Pool Factory      | <-- Usage hints, workload analysis        |
 *   |  +-------------------+                                           |
 *   |       |                                                          |
 *   |       +--------+--------+--------+                               |
 *   |       |        |        |        |                               |
 *   |       v        v        v        v                               |
 *   |   Scratch  Persistent Activation Composite                       |
 *   |   (Bump)   (Buddy)    (Slab)    (Multi-Tier)                     |
 *   +------------------------------------------------------------------+
 *
 * DESIGN PATTERN: Factory Pattern
 * - Encapsulates object creation logic
 * - Hides complexity of allocator configuration
 * - Enables extension without modifying client code
 *
 * USAGE SCENARIOS:
 *
 * 1. TRAINING:
 *    - Scratch pool for forward/backward activations (reset per iteration)
 *    - Persistent pool for weights and gradients
 *    - Activation pool for fixed-size intermediate tensors
 *
 * 2. INFERENCE:
 *    - Persistent pool for model weights (loaded once)
 *    - Scratch pool for per-batch activations
 *
 * 3. DYNAMIC WORKLOADS:
 *    - Composite pool with auto-sizing
 *    - Buddy allocator for varied tensor sizes
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GPU_POOL_FACTORY_H
#define NIMCP_GPU_POOL_FACTORY_H

#include "gpu/memory/nimcp_gpu_pool.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Factory Enumerations
//=============================================================================

/**
 * @brief Pool usage pattern hints
 *
 * Guides the factory in selecting optimal allocator strategy
 */
typedef enum nimcp_pool_usage_e {
    /**
     * @brief Per-iteration temporary memory
     *
     * - Allocations are short-lived (one training iteration)
     * - All memory freed at once via reset
     * - Maximum allocation speed required
     * - Uses: Bump allocator
     */
    NIMCP_POOL_USAGE_SCRATCH = 0,

    /**
     * @brief Long-lived persistent allocations
     *
     * - Allocations live for entire training run
     * - Individual alloc/free operations
     * - Varied allocation sizes
     * - Uses: Buddy allocator
     */
    NIMCP_POOL_USAGE_PERSISTENT,

    /**
     * @brief Fixed-size activation tensors
     *
     * - Known tensor sizes (e.g., batch_size * hidden_dim)
     * - Frequent alloc/free cycles
     * - Minimal fragmentation needed
     * - Uses: Slab allocator
     */
    NIMCP_POOL_USAGE_ACTIVATION,

    /**
     * @brief Mixed workload with tiered memory
     *
     * - Combines scratch + persistent + activation
     * - Automatic tier selection based on allocation pattern
     * - Uses: Composite allocator
     */
    NIMCP_POOL_USAGE_COMPOSITE,

    /**
     * @brief Inference-only workload
     *
     * - Optimized for read-heavy access
     * - Pre-allocated fixed tensors
     * - Uses: Slab allocator with inference-tuned sizes
     */
    NIMCP_POOL_USAGE_INFERENCE,

    NIMCP_POOL_USAGE_COUNT
} nimcp_pool_usage_t;

/**
 * @brief Model size category for auto-configuration
 */
typedef enum nimcp_model_size_e {
    NIMCP_MODEL_SIZE_SMALL = 0,     /**< < 1B parameters */
    NIMCP_MODEL_SIZE_MEDIUM,        /**< 1B - 10B parameters */
    NIMCP_MODEL_SIZE_LARGE,         /**< 10B - 100B parameters */
    NIMCP_MODEL_SIZE_XLARGE,        /**< > 100B parameters */
    NIMCP_MODEL_SIZE_COUNT
} nimcp_model_size_t;

/**
 * @brief Training phase for optimization hints
 */
typedef enum nimcp_training_phase_e {
    NIMCP_PHASE_WARMUP = 0,         /**< Initial warmup phase */
    NIMCP_PHASE_TRAINING,           /**< Main training loop */
    NIMCP_PHASE_VALIDATION,         /**< Validation/eval mode */
    NIMCP_PHASE_INFERENCE,          /**< Inference only */
    NIMCP_PHASE_COUNT
} nimcp_training_phase_t;

//=============================================================================
// Factory Hints and Configuration
//=============================================================================

/**
 * @brief Activation tensor size configuration
 *
 * Used by slab allocator to create optimized size classes
 */
typedef struct nimcp_activation_size_s {
    size_t batch_size;              /**< Batch dimension */
    size_t hidden_dim;              /**< Hidden dimension */
    size_t seq_len;                 /**< Sequence length (for transformers) */
    size_t num_layers;              /**< Number of layers */
    size_t num_heads;               /**< Number of attention heads */
    size_t dtype_size;              /**< Bytes per element (4 for fp32, 2 for fp16) */
} nimcp_activation_size_t;

/**
 * @brief Factory hints for optimal pool creation
 */
typedef struct nimcp_pool_factory_hints_s {
    // Usage pattern
    nimcp_pool_usage_t usage;           /**< Primary usage pattern */
    nimcp_model_size_t model_size;      /**< Model size category */
    nimcp_training_phase_t phase;       /**< Current training phase */

    // Memory budget
    size_t max_memory_bytes;            /**< Maximum memory to use (0 = auto) */
    float memory_fraction;              /**< Fraction of GPU memory (0.0-1.0, default 0.9) */

    // Performance hints
    bool prefer_speed;                  /**< Prefer allocation speed over memory efficiency */
    bool enable_stats;                  /**< Enable detailed statistics */
    bool enable_debugging;              /**< Enable allocation tracking/debugging */

    // Activation tensor hints (for slab allocator)
    nimcp_activation_size_t activation_sizes; /**< Expected activation sizes */

    // Custom slab sizes (optional)
    size_t custom_slab_sizes[NIMCP_GPU_POOL_MAX_SLABS];
    size_t num_custom_slabs;

    // Composite pool ratios (for NIMCP_POOL_USAGE_COMPOSITE)
    float scratch_ratio;                /**< Fraction for scratch (default 0.6) */
    float persistent_ratio;             /**< Fraction for persistent (default 0.3) */
    float activation_ratio;             /**< Fraction for activation (default 0.1) */
} nimcp_pool_factory_hints_t;

/**
 * @brief Composite pool handle (contains multiple sub-pools)
 */
typedef struct nimcp_composite_pool_s {
    nimcp_gpu_pool_t* scratch;          /**< Scratch pool (bump allocator) */
    nimcp_gpu_pool_t* persistent;       /**< Persistent pool (buddy allocator) */
    nimcp_gpu_pool_t* activation;       /**< Activation pool (slab allocator) */
    size_t total_size;                  /**< Total memory across all pools */
} nimcp_composite_pool_t;

//=============================================================================
// Factory API
//=============================================================================

/**
 * @brief Get default factory hints
 *
 * @return Default hints suitable for general training workloads
 */
NIMCP_EXPORT nimcp_pool_factory_hints_t nimcp_pool_factory_hints_default(void);

/**
 * @brief Get factory hints for training
 *
 * @param model_size Model size category
 * @param batch_size Training batch size
 * @param seq_len Sequence length (for transformers)
 * @return Hints optimized for training
 */
NIMCP_EXPORT nimcp_pool_factory_hints_t nimcp_pool_factory_hints_training(
    nimcp_model_size_t model_size,
    size_t batch_size,
    size_t seq_len
);

/**
 * @brief Get factory hints for inference
 *
 * @param model_size Model size category
 * @param max_batch_size Maximum batch size
 * @param max_seq_len Maximum sequence length
 * @return Hints optimized for inference
 */
NIMCP_EXPORT nimcp_pool_factory_hints_t nimcp_pool_factory_hints_inference(
    nimcp_model_size_t model_size,
    size_t max_batch_size,
    size_t max_seq_len
);

/**
 * @brief Create pool from factory hints
 *
 * DESIGN PATTERN: Factory Method
 * - Analyzes hints to determine optimal allocator
 * - Configures pool based on usage pattern
 * - Returns ready-to-use pool
 *
 * @param ctx GPU context
 * @param hints Factory hints (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * EXAMPLE:
 *   nimcp_pool_factory_hints_t hints = nimcp_pool_factory_hints_training(
 *       NIMCP_MODEL_SIZE_LARGE, 32, 2048);
 *   nimcp_gpu_pool_t* pool = nimcp_pool_factory_create(ctx, &hints);
 */
NIMCP_EXPORT nimcp_gpu_pool_t* nimcp_pool_factory_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_pool_factory_hints_t* hints
);

/**
 * @brief Create composite pool with multiple tiers
 *
 * Creates a composite pool with separate sub-pools for different use cases:
 * - Scratch pool for per-iteration temporaries
 * - Persistent pool for weights and gradients
 * - Activation pool for fixed-size tensors
 *
 * @param ctx GPU context
 * @param hints Factory hints
 * @return Composite pool or NULL on failure
 *
 * EXAMPLE:
 *   nimcp_pool_factory_hints_t hints = nimcp_pool_factory_hints_default();
 *   hints.usage = NIMCP_POOL_USAGE_COMPOSITE;
 *   hints.scratch_ratio = 0.5;
 *   hints.persistent_ratio = 0.4;
 *   hints.activation_ratio = 0.1;
 *
 *   nimcp_composite_pool_t* pools = nimcp_pool_factory_create_composite(ctx, &hints);
 */
NIMCP_EXPORT nimcp_composite_pool_t* nimcp_pool_factory_create_composite(
    nimcp_gpu_context_t* ctx,
    const nimcp_pool_factory_hints_t* hints
);

/**
 * @brief Destroy composite pool
 *
 * @param pools Composite pool to destroy
 */
NIMCP_EXPORT void nimcp_pool_factory_destroy_composite(nimcp_composite_pool_t* pools);

/**
 * @brief Create scratch pool (convenience function)
 *
 * @param ctx GPU context
 * @param size_bytes Pool size (0 = auto-detect based on GPU memory)
 * @return Scratch pool with bump allocator
 */
NIMCP_EXPORT nimcp_gpu_pool_t* nimcp_pool_factory_create_scratch(
    nimcp_gpu_context_t* ctx,
    size_t size_bytes
);

/**
 * @brief Create persistent pool (convenience function)
 *
 * @param ctx GPU context
 * @param size_bytes Pool size (0 = auto-detect)
 * @return Persistent pool with buddy allocator
 */
NIMCP_EXPORT nimcp_gpu_pool_t* nimcp_pool_factory_create_persistent(
    nimcp_gpu_context_t* ctx,
    size_t size_bytes
);

/**
 * @brief Create activation pool (convenience function)
 *
 * @param ctx GPU context
 * @param activation_sizes Expected activation tensor sizes
 * @return Activation pool with slab allocator
 */
NIMCP_EXPORT nimcp_gpu_pool_t* nimcp_pool_factory_create_activation(
    nimcp_gpu_context_t* ctx,
    const nimcp_activation_size_t* activation_sizes
);

//=============================================================================
// Auto-Configuration API
//=============================================================================

/**
 * @brief Auto-detect optimal pool configuration
 *
 * Analyzes GPU capabilities and available memory to determine
 * optimal pool configuration.
 *
 * @param ctx GPU context
 * @param usage Primary usage pattern
 * @param config Output configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_pool_factory_auto_configure(
    nimcp_gpu_context_t* ctx,
    nimcp_pool_usage_t usage,
    nimcp_gpu_pool_config_t* config
);

/**
 * @brief Estimate memory requirements
 *
 * @param model_size Model size category
 * @param batch_size Batch size
 * @param seq_len Sequence length
 * @param dtype_size Bytes per element
 * @return Estimated memory in bytes
 */
NIMCP_EXPORT size_t nimcp_pool_factory_estimate_memory(
    nimcp_model_size_t model_size,
    size_t batch_size,
    size_t seq_len,
    size_t dtype_size
);

/**
 * @brief Generate optimal slab sizes for transformer model
 *
 * Computes slab sizes based on typical transformer tensor shapes
 *
 * @param activation_sizes Transformer architecture parameters
 * @param slab_sizes Output array of slab sizes
 * @param max_slabs Maximum number of slabs
 * @return Number of slab sizes generated
 */
NIMCP_EXPORT size_t nimcp_pool_factory_compute_slab_sizes(
    const nimcp_activation_size_t* activation_sizes,
    size_t* slab_sizes,
    size_t max_slabs
);

//=============================================================================
// Pool Selection Helpers
//=============================================================================

/**
 * @brief Allocate from appropriate tier in composite pool
 *
 * Automatically selects best pool based on allocation characteristics:
 * - Small, frequently freed -> activation pool
 * - Large, long-lived -> persistent pool
 * - Temporary, batch-reset -> scratch pool
 *
 * @param pools Composite pool
 * @param size Allocation size
 * @param lifetime_hint Expected lifetime (0=scratch, 1=activation, 2=persistent)
 * @return Device pointer or NULL
 */
NIMCP_EXPORT void* nimcp_composite_pool_alloc(
    nimcp_composite_pool_t* pools,
    size_t size,
    int lifetime_hint
);

/**
 * @brief Free to composite pool (finds correct sub-pool)
 *
 * @param pools Composite pool
 * @param ptr Pointer to free
 */
NIMCP_EXPORT void nimcp_composite_pool_free(
    nimcp_composite_pool_t* pools,
    void* ptr
);

/**
 * @brief Reset scratch tier only
 *
 * Resets only the scratch pool, leaving persistent and activation intact
 *
 * @param pools Composite pool
 * @return Number of allocations freed
 */
NIMCP_EXPORT size_t nimcp_composite_pool_reset_scratch(
    nimcp_composite_pool_t* pools
);

/**
 * @brief Get combined statistics for composite pool
 *
 * @param pools Composite pool
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_composite_pool_get_stats(
    const nimcp_composite_pool_t* pools,
    nimcp_gpu_pool_stats_t* stats
);

/**
 * @brief Print composite pool statistics
 *
 * @param pools Composite pool
 */
NIMCP_EXPORT void nimcp_composite_pool_print_stats(
    const nimcp_composite_pool_t* pools
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GPU_POOL_FACTORY_H
