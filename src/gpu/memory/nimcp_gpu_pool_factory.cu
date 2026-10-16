/**
 * @file nimcp_gpu_pool_factory.cu
 * @brief GPU Memory Pool Factory Implementation
 *
 * WHAT: Factory pattern for creating GPU memory pools
 * WHY:  Simplifies pool creation with intelligent defaults
 * HOW:  Analyzes workload hints and GPU capabilities
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

#include "gpu/memory/nimcp_gpu_pool_factory.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <string.h>

#define LOG_MODULE "GPU_POOL_FACTORY"

//=============================================================================
// Internal Constants
//=============================================================================

// Memory size thresholds (in bytes)
#define SIZE_1GB  (1ULL * 1024 * 1024 * 1024)
#define SIZE_2GB  (2ULL * 1024 * 1024 * 1024)
#define SIZE_4GB  (4ULL * 1024 * 1024 * 1024)
#define SIZE_8GB  (8ULL * 1024 * 1024 * 1024)
#define SIZE_16GB (16ULL * 1024 * 1024 * 1024)
#define SIZE_32GB (32ULL * 1024 * 1024 * 1024)
#define SIZE_64GB (64ULL * 1024 * 1024 * 1024)
#define SIZE_80GB (80ULL * 1024 * 1024 * 1024)

// Default ratios for composite pools
#define DEFAULT_SCRATCH_RATIO    0.60f
#define DEFAULT_PERSISTENT_RATIO 0.30f
#define DEFAULT_ACTIVATION_RATIO 0.10f

// Minimum pool sizes
#define MIN_SCRATCH_SIZE    (256 * 1024 * 1024)   // 256 MB
#define MIN_PERSISTENT_SIZE (128 * 1024 * 1024)   // 128 MB
#define MIN_ACTIVATION_SIZE (64 * 1024 * 1024)    // 64 MB

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get available GPU memory
 */
static size_t get_available_gpu_memory(nimcp_gpu_context_t* ctx) {
    size_t free_mem = 0;
    size_t total_mem = 0;

#ifdef NIMCP_ENABLE_CUDA
    if (ctx) {
        cudaMemGetInfo(&free_mem, &total_mem);
    }
#else
    (void)ctx;
    // Fallback: assume 8GB available on CPU
    free_mem = SIZE_8GB;
#endif

    return free_mem;
}

/**
 * @brief Get total GPU memory
 */
static size_t get_total_gpu_memory(nimcp_gpu_context_t* ctx) {
    if (!ctx) return SIZE_8GB;
    return ctx->total_memory;
}

/**
 * @brief Estimate model memory based on size category
 */
static size_t estimate_model_memory(nimcp_model_size_t size, size_t dtype_size) {
    // Rough estimates based on typical model sizes
    // Assumes: parameters + gradients + optimizer states = ~4x parameters
    size_t param_bytes;

    switch (size) {
        case NIMCP_MODEL_SIZE_SMALL:
            param_bytes = 500ULL * 1000000 * dtype_size;  // 500M params
            break;
        case NIMCP_MODEL_SIZE_MEDIUM:
            param_bytes = 5ULL * 1000000000 * dtype_size; // 5B params
            break;
        case NIMCP_MODEL_SIZE_LARGE:
            param_bytes = 30ULL * 1000000000 * dtype_size; // 30B params
            break;
        case NIMCP_MODEL_SIZE_XLARGE:
            param_bytes = 175ULL * 1000000000 * dtype_size; // 175B params
            break;
        default:
            param_bytes = SIZE_1GB;
    }

    return param_bytes * 4;  // 4x for gradients + optimizer states
}

/**
 * @brief Estimate activation memory for transformer
 */
static size_t estimate_activation_memory(
    const nimcp_activation_size_t* sizes,
    size_t dtype_size
) {
    if (!sizes) return SIZE_1GB;

    size_t batch = sizes->batch_size > 0 ? sizes->batch_size : 32;
    size_t hidden = sizes->hidden_dim > 0 ? sizes->hidden_dim : 4096;
    size_t seq = sizes->seq_len > 0 ? sizes->seq_len : 2048;
    size_t layers = sizes->num_layers > 0 ? sizes->num_layers : 32;
    size_t dtype = dtype_size > 0 ? dtype_size : 4;

    // Per-layer activation: batch * seq * hidden
    size_t per_layer = batch * seq * hidden * dtype;

    // Attention: batch * heads * seq * seq
    size_t heads = sizes->num_heads > 0 ? sizes->num_heads : 32;
    size_t attention = batch * heads * seq * seq * dtype;

    // Total for all layers (rough estimate)
    // In practice, we may recompute or use gradient checkpointing
    return (per_layer + attention) * layers;
}

//=============================================================================
// Factory Hints API
//=============================================================================

nimcp_pool_factory_hints_t nimcp_pool_factory_hints_default(void) {
    nimcp_pool_factory_hints_t hints;
    memset(&hints, 0, sizeof(hints));

    hints.usage = NIMCP_POOL_USAGE_PERSISTENT;
    hints.model_size = NIMCP_MODEL_SIZE_MEDIUM;
    hints.phase = NIMCP_PHASE_TRAINING;
    hints.max_memory_bytes = 0;  // Auto-detect
    hints.memory_fraction = 0.9f;
    hints.prefer_speed = false;
    hints.enable_stats = true;
    hints.enable_debugging = false;

    // Default activation sizes (GPT-2 scale)
    hints.activation_sizes.batch_size = 32;
    hints.activation_sizes.hidden_dim = 768;
    hints.activation_sizes.seq_len = 1024;
    hints.activation_sizes.num_layers = 12;
    hints.activation_sizes.num_heads = 12;
    hints.activation_sizes.dtype_size = 4;  // FP32

    hints.num_custom_slabs = 0;

    hints.scratch_ratio = DEFAULT_SCRATCH_RATIO;
    hints.persistent_ratio = DEFAULT_PERSISTENT_RATIO;
    hints.activation_ratio = DEFAULT_ACTIVATION_RATIO;

    return hints;
}

nimcp_pool_factory_hints_t nimcp_pool_factory_hints_training(
    nimcp_model_size_t model_size,
    size_t batch_size,
    size_t seq_len
) {
    nimcp_pool_factory_hints_t hints = nimcp_pool_factory_hints_default();

    hints.usage = NIMCP_POOL_USAGE_COMPOSITE;
    hints.model_size = model_size;
    hints.phase = NIMCP_PHASE_TRAINING;

    hints.activation_sizes.batch_size = batch_size;
    hints.activation_sizes.seq_len = seq_len;

    // Adjust dimensions based on model size
    switch (model_size) {
        case NIMCP_MODEL_SIZE_SMALL:
            hints.activation_sizes.hidden_dim = 768;
            hints.activation_sizes.num_layers = 12;
            hints.activation_sizes.num_heads = 12;
            hints.scratch_ratio = 0.5f;
            hints.persistent_ratio = 0.35f;
            hints.activation_ratio = 0.15f;
            break;

        case NIMCP_MODEL_SIZE_MEDIUM:
            hints.activation_sizes.hidden_dim = 4096;
            hints.activation_sizes.num_layers = 32;
            hints.activation_sizes.num_heads = 32;
            hints.scratch_ratio = 0.55f;
            hints.persistent_ratio = 0.30f;
            hints.activation_ratio = 0.15f;
            break;

        case NIMCP_MODEL_SIZE_LARGE:
            hints.activation_sizes.hidden_dim = 8192;
            hints.activation_sizes.num_layers = 64;
            hints.activation_sizes.num_heads = 64;
            hints.scratch_ratio = 0.60f;
            hints.persistent_ratio = 0.25f;
            hints.activation_ratio = 0.15f;
            break;

        case NIMCP_MODEL_SIZE_XLARGE:
            hints.activation_sizes.hidden_dim = 12288;
            hints.activation_sizes.num_layers = 96;
            hints.activation_sizes.num_heads = 96;
            hints.scratch_ratio = 0.65f;
            hints.persistent_ratio = 0.20f;
            hints.activation_ratio = 0.15f;
            break;

        default:
            break;
    }

    return hints;
}

nimcp_pool_factory_hints_t nimcp_pool_factory_hints_inference(
    nimcp_model_size_t model_size,
    size_t max_batch_size,
    size_t max_seq_len
) {
    nimcp_pool_factory_hints_t hints = nimcp_pool_factory_hints_default();

    hints.usage = NIMCP_POOL_USAGE_INFERENCE;
    hints.model_size = model_size;
    hints.phase = NIMCP_PHASE_INFERENCE;
    hints.prefer_speed = true;

    hints.activation_sizes.batch_size = max_batch_size;
    hints.activation_sizes.seq_len = max_seq_len;

    // Inference uses more slab allocation for consistent performance
    hints.scratch_ratio = 0.3f;
    hints.persistent_ratio = 0.5f;  // Model weights
    hints.activation_ratio = 0.2f;

    // Use FP16 for inference typically
    hints.activation_sizes.dtype_size = 2;

    return hints;
}

//=============================================================================
// Main Factory Functions
//=============================================================================

nimcp_gpu_pool_t* nimcp_pool_factory_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_pool_factory_hints_t* hints
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    // Use defaults if no hints
    nimcp_pool_factory_hints_t default_hints = nimcp_pool_factory_hints_default();
    if (!hints) {
        hints = &default_hints;
    }

    // Determine pool size
    size_t pool_size;
    if (hints->max_memory_bytes > 0) {
        pool_size = hints->max_memory_bytes;
    } else {
        size_t available = get_available_gpu_memory(ctx);
        pool_size = (size_t)(available * hints->memory_fraction);
    }

    // Create appropriate pool based on usage
    nimcp_gpu_pool_config_t config;

    switch (hints->usage) {
        case NIMCP_POOL_USAGE_SCRATCH:
            config = nimcp_gpu_pool_config_scratch(pool_size);
            break;

        case NIMCP_POOL_USAGE_PERSISTENT:
            config = nimcp_gpu_pool_config_persistent(pool_size);
            break;

        case NIMCP_POOL_USAGE_ACTIVATION:
        case NIMCP_POOL_USAGE_INFERENCE: {
            // Compute optimal slab sizes
            size_t slab_sizes[NIMCP_GPU_POOL_MAX_SLABS];
            size_t num_slabs = nimcp_pool_factory_compute_slab_sizes(
                &hints->activation_sizes,
                slab_sizes,
                NIMCP_GPU_POOL_MAX_SLABS
            );
            config = nimcp_gpu_pool_config_activation(pool_size, slab_sizes, num_slabs);
            break;
        }

        case NIMCP_POOL_USAGE_COMPOSITE:
            // For composite, just create a persistent pool
            // Use nimcp_pool_factory_create_composite for full composite
            config = nimcp_gpu_pool_config_persistent(pool_size);
            break;

        default:
            config = nimcp_gpu_pool_config_default();
            config.initial_size = pool_size;
    }

    config.enable_stats = hints->enable_stats;

    LOG_INFO("Factory creating pool: usage=%d, size=%zu MB",
             hints->usage, pool_size / (1024 * 1024));

    return nimcp_gpu_pool_create(ctx, &config);
}

nimcp_composite_pool_t* nimcp_pool_factory_create_composite(
    nimcp_gpu_context_t* ctx,
    const nimcp_pool_factory_hints_t* hints
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    nimcp_pool_factory_hints_t default_hints = nimcp_pool_factory_hints_default();
    default_hints.usage = NIMCP_POOL_USAGE_COMPOSITE;
    if (!hints) {
        hints = &default_hints;
    }

    // Allocate composite structure
    nimcp_composite_pool_t* composite =
        (nimcp_composite_pool_t*)nimcp_calloc(1, sizeof(nimcp_composite_pool_t));
    if (!composite) {
        LOG_ERROR("Failed to allocate composite pool structure");
        return NULL;
    }

    // Determine total memory
    size_t total_size;
    if (hints->max_memory_bytes > 0) {
        total_size = hints->max_memory_bytes;
    } else {
        size_t available = get_available_gpu_memory(ctx);
        total_size = (size_t)(available * hints->memory_fraction);
    }

    // Calculate tier sizes
    float scratch_ratio = hints->scratch_ratio > 0 ?
                          hints->scratch_ratio : DEFAULT_SCRATCH_RATIO;
    float persistent_ratio = hints->persistent_ratio > 0 ?
                             hints->persistent_ratio : DEFAULT_PERSISTENT_RATIO;
    float activation_ratio = hints->activation_ratio > 0 ?
                             hints->activation_ratio : DEFAULT_ACTIVATION_RATIO;

    // Normalize ratios
    float total_ratio = scratch_ratio + persistent_ratio + activation_ratio;
    scratch_ratio /= total_ratio;
    persistent_ratio /= total_ratio;
    activation_ratio /= total_ratio;

    size_t scratch_size = (size_t)(total_size * scratch_ratio);
    size_t persistent_size = (size_t)(total_size * persistent_ratio);
    size_t activation_size = (size_t)(total_size * activation_ratio);

    // Ensure minimum sizes
    if (scratch_size < MIN_SCRATCH_SIZE) scratch_size = MIN_SCRATCH_SIZE;
    if (persistent_size < MIN_PERSISTENT_SIZE) persistent_size = MIN_PERSISTENT_SIZE;
    if (activation_size < MIN_ACTIVATION_SIZE) activation_size = MIN_ACTIVATION_SIZE;

    // Create scratch pool (bump allocator)
    nimcp_gpu_pool_config_t scratch_config = nimcp_gpu_pool_config_scratch(scratch_size);
    scratch_config.enable_stats = hints->enable_stats;
    composite->scratch = nimcp_gpu_pool_create(ctx, &scratch_config);

    if (!composite->scratch) {
        LOG_ERROR("Failed to create scratch pool");
        nimcp_free(composite);
        return NULL;
    }

    // Create persistent pool (buddy allocator)
    nimcp_gpu_pool_config_t persistent_config = nimcp_gpu_pool_config_persistent(persistent_size);
    persistent_config.enable_stats = hints->enable_stats;
    composite->persistent = nimcp_gpu_pool_create(ctx, &persistent_config);

    if (!composite->persistent) {
        LOG_ERROR("Failed to create persistent pool");
        nimcp_gpu_pool_destroy(composite->scratch);
        nimcp_free(composite);
        return NULL;
    }

    // Create activation pool (slab allocator)
    size_t slab_sizes[NIMCP_GPU_POOL_MAX_SLABS];
    size_t num_slabs = nimcp_pool_factory_compute_slab_sizes(
        &hints->activation_sizes,
        slab_sizes,
        NIMCP_GPU_POOL_MAX_SLABS
    );

    nimcp_gpu_pool_config_t activation_config =
        nimcp_gpu_pool_config_activation(activation_size, slab_sizes, num_slabs);
    activation_config.enable_stats = hints->enable_stats;
    composite->activation = nimcp_gpu_pool_create(ctx, &activation_config);

    if (!composite->activation) {
        LOG_ERROR("Failed to create activation pool");
        nimcp_gpu_pool_destroy(composite->persistent);
        nimcp_gpu_pool_destroy(composite->scratch);
        nimcp_free(composite);
        return NULL;
    }

    composite->total_size = scratch_size + persistent_size + activation_size;

    LOG_INFO("Created composite pool: scratch=%zu MB, persistent=%zu MB, activation=%zu MB",
             scratch_size / (1024 * 1024),
             persistent_size / (1024 * 1024),
             activation_size / (1024 * 1024));

    return composite;
}

void nimcp_pool_factory_destroy_composite(nimcp_composite_pool_t* pools) {
    if (!pools) return;

    if (pools->activation) nimcp_gpu_pool_destroy(pools->activation);
    if (pools->persistent) nimcp_gpu_pool_destroy(pools->persistent);
    if (pools->scratch) nimcp_gpu_pool_destroy(pools->scratch);

    nimcp_free(pools);
    LOG_INFO("Destroyed composite pool");
}

nimcp_gpu_pool_t* nimcp_pool_factory_create_scratch(
    nimcp_gpu_context_t* ctx,
    size_t size_bytes
) {
    if (!ctx) return NULL;

    if (size_bytes == 0) {
        size_t available = get_available_gpu_memory(ctx);
        size_bytes = available / 2;  // Use half for scratch
    }

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_scratch(size_bytes);
    return nimcp_gpu_pool_create(ctx, &config);
}

nimcp_gpu_pool_t* nimcp_pool_factory_create_persistent(
    nimcp_gpu_context_t* ctx,
    size_t size_bytes
) {
    if (!ctx) return NULL;

    if (size_bytes == 0) {
        size_t available = get_available_gpu_memory(ctx);
        size_bytes = available / 2;
    }

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_persistent(size_bytes);
    return nimcp_gpu_pool_create(ctx, &config);
}

nimcp_gpu_pool_t* nimcp_pool_factory_create_activation(
    nimcp_gpu_context_t* ctx,
    const nimcp_activation_size_t* activation_sizes
) {
    if (!ctx) return NULL;

    size_t available = get_available_gpu_memory(ctx);
    size_t pool_size = available / 4;  // Use quarter for activations

    size_t slab_sizes[NIMCP_GPU_POOL_MAX_SLABS];
    size_t num_slabs = nimcp_pool_factory_compute_slab_sizes(
        activation_sizes,
        slab_sizes,
        NIMCP_GPU_POOL_MAX_SLABS
    );

    nimcp_gpu_pool_config_t config =
        nimcp_gpu_pool_config_activation(pool_size, slab_sizes, num_slabs);
    return nimcp_gpu_pool_create(ctx, &config);
}

//=============================================================================
// Auto-Configuration API
//=============================================================================

bool nimcp_pool_factory_auto_configure(
    nimcp_gpu_context_t* ctx,
    nimcp_pool_usage_t usage,
    nimcp_gpu_pool_config_t* config
) {
    if (!ctx || !config) return false;

    size_t available = get_available_gpu_memory(ctx);
    size_t pool_size = (size_t)(available * 0.9);  // Use 90%

    switch (usage) {
        case NIMCP_POOL_USAGE_SCRATCH:
            *config = nimcp_gpu_pool_config_scratch(pool_size);
            break;

        case NIMCP_POOL_USAGE_PERSISTENT:
            *config = nimcp_gpu_pool_config_persistent(pool_size);
            break;

        case NIMCP_POOL_USAGE_ACTIVATION:
        case NIMCP_POOL_USAGE_INFERENCE:
            *config = nimcp_gpu_pool_config_activation(pool_size, NULL, 0);
            break;

        default:
            *config = nimcp_gpu_pool_config_default();
            config->initial_size = pool_size;
    }

    return true;
}

size_t nimcp_pool_factory_estimate_memory(
    nimcp_model_size_t model_size,
    size_t batch_size,
    size_t seq_len,
    size_t dtype_size
) {
    if (dtype_size == 0) dtype_size = 4;

    nimcp_activation_size_t sizes = {0};
    sizes.batch_size = batch_size;
    sizes.seq_len = seq_len;
    sizes.dtype_size = dtype_size;

    // Set typical dimensions for model size
    switch (model_size) {
        case NIMCP_MODEL_SIZE_SMALL:
            sizes.hidden_dim = 768;
            sizes.num_layers = 12;
            sizes.num_heads = 12;
            break;
        case NIMCP_MODEL_SIZE_MEDIUM:
            sizes.hidden_dim = 4096;
            sizes.num_layers = 32;
            sizes.num_heads = 32;
            break;
        case NIMCP_MODEL_SIZE_LARGE:
            sizes.hidden_dim = 8192;
            sizes.num_layers = 64;
            sizes.num_heads = 64;
            break;
        case NIMCP_MODEL_SIZE_XLARGE:
            sizes.hidden_dim = 12288;
            sizes.num_layers = 96;
            sizes.num_heads = 96;
            break;
        default:
            sizes.hidden_dim = 2048;
            sizes.num_layers = 24;
            sizes.num_heads = 16;
    }

    // Model memory (weights + gradients + optimizer)
    size_t model_mem = estimate_model_memory(model_size, dtype_size);

    // Activation memory
    size_t activation_mem = estimate_activation_memory(&sizes, dtype_size);

    // Add overhead for fragmentation, buffers, etc.
    size_t overhead = (model_mem + activation_mem) / 10;  // 10% overhead

    return model_mem + activation_mem + overhead;
}

size_t nimcp_pool_factory_compute_slab_sizes(
    const nimcp_activation_size_t* activation_sizes,
    size_t* slab_sizes,
    size_t max_slabs
) {
    if (!slab_sizes || max_slabs == 0) return 0;

    size_t count = 0;

    // Default slab sizes if no hints
    if (!activation_sizes) {
        static const size_t defaults[] = {
            256,        // Small buffers
            1024,       // 1KB
            4096,       // 4KB
            16384,      // 16KB
            65536,      // 64KB
            262144,     // 256KB
            1048576,    // 1MB
            4194304,    // 4MB
            16777216,   // 16MB
        };

        for (size_t i = 0; i < sizeof(defaults)/sizeof(defaults[0]) && count < max_slabs; i++) {
            slab_sizes[count++] = defaults[i];
        }
        return count;
    }

    size_t batch = activation_sizes->batch_size > 0 ? activation_sizes->batch_size : 32;
    size_t hidden = activation_sizes->hidden_dim > 0 ? activation_sizes->hidden_dim : 4096;
    size_t seq = activation_sizes->seq_len > 0 ? activation_sizes->seq_len : 2048;
    size_t heads = activation_sizes->num_heads > 0 ? activation_sizes->num_heads : 32;
    size_t dtype = activation_sizes->dtype_size > 0 ? activation_sizes->dtype_size : 4;

    // Compute typical tensor sizes for transformer
    // Align to 256 bytes

    #define ALIGN_256(x) (((x) + 255) & ~255)

    // Small buffers (bias, norms)
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(hidden * dtype);

    // Per-token embedding
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * hidden * dtype);

    // Attention scores (per head, per batch)
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * seq * seq * dtype / heads);

    // Attention output (per head)
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * seq * (hidden / heads) * dtype);

    // Full sequence activation
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * seq * hidden * dtype);

    // FFN intermediate (4x hidden)
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * seq * hidden * 4 * dtype);

    // All attention heads
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * heads * seq * seq * dtype);

    // QKV projection
    if (count < max_slabs) slab_sizes[count++] = ALIGN_256(batch * seq * hidden * 3 * dtype);

    // Add some standard sizes for misc allocations
    static const size_t misc_sizes[] = { 256, 1024, 4096, 16384, 65536 };
    for (size_t i = 0; i < sizeof(misc_sizes)/sizeof(misc_sizes[0]) && count < max_slabs; i++) {
        // Only add if not already present
        bool exists = false;
        for (size_t j = 0; j < count; j++) {
            if (slab_sizes[j] == misc_sizes[i]) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            slab_sizes[count++] = misc_sizes[i];
        }
    }

    // Sort slab sizes
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (slab_sizes[i] > slab_sizes[j]) {
                size_t tmp = slab_sizes[i];
                slab_sizes[i] = slab_sizes[j];
                slab_sizes[j] = tmp;
            }
        }
    }

    #undef ALIGN_256

    return count;
}

//=============================================================================
// Composite Pool Operations
//=============================================================================

void* nimcp_composite_pool_alloc(
    nimcp_composite_pool_t* pools,
    size_t size,
    int lifetime_hint
) {
    if (!pools) return NULL;

    void* ptr = NULL;

    switch (lifetime_hint) {
        case 0:  // Scratch (temporary)
            if (pools->scratch) {
                ptr = nimcp_gpu_pool_alloc(pools->scratch, size, 0, NULL);
            }
            break;

        case 1:  // Activation (fixed-size, frequently reused)
            if (pools->activation) {
                ptr = nimcp_gpu_pool_alloc(pools->activation, size, 0, NULL);
            }
            break;

        case 2:  // Persistent (long-lived)
            if (pools->persistent) {
                ptr = nimcp_gpu_pool_alloc(pools->persistent, size, 0, NULL);
            }
            break;

        default:
            // Auto-select based on size
            // Small sizes -> activation (slab for O(1))
            // Medium sizes -> persistent (buddy)
            // Could also use scratch if we're in iteration context
            if (size <= 64 * 1024 && pools->activation) {
                ptr = nimcp_gpu_pool_alloc(pools->activation, size, 0, NULL);
            } else if (pools->persistent) {
                ptr = nimcp_gpu_pool_alloc(pools->persistent, size, 0, NULL);
            }
    }

    // Fallback: try any pool that has space
    if (!ptr) {
        if (pools->scratch) {
            ptr = nimcp_gpu_pool_alloc(pools->scratch, size, 0, NULL);
        }
        if (!ptr && pools->persistent) {
            ptr = nimcp_gpu_pool_alloc(pools->persistent, size, 0, NULL);
        }
        if (!ptr && pools->activation) {
            ptr = nimcp_gpu_pool_alloc(pools->activation, size, 0, NULL);
        }
    }

    return ptr;
}

void nimcp_composite_pool_free(
    nimcp_composite_pool_t* pools,
    void* ptr
) {
    if (!pools || !ptr) return;

    // Find which pool owns this pointer
    if (pools->scratch && nimcp_gpu_pool_owns(pools->scratch, ptr)) {
        nimcp_gpu_pool_free(pools->scratch, ptr);
    } else if (pools->persistent && nimcp_gpu_pool_owns(pools->persistent, ptr)) {
        nimcp_gpu_pool_free(pools->persistent, ptr);
    } else if (pools->activation && nimcp_gpu_pool_owns(pools->activation, ptr)) {
        nimcp_gpu_pool_free(pools->activation, ptr);
    } else {
        LOG_WARN("Composite pool free: pointer %p not found in any pool", ptr);
    }
}

size_t nimcp_composite_pool_reset_scratch(nimcp_composite_pool_t* pools) {
    if (!pools || !pools->scratch) return 0;
    return nimcp_gpu_pool_reset(pools->scratch);
}

bool nimcp_composite_pool_get_stats(
    const nimcp_composite_pool_t* pools,
    nimcp_gpu_pool_stats_t* stats
) {
    if (!pools || !stats) return false;

    memset(stats, 0, sizeof(nimcp_gpu_pool_stats_t));

    nimcp_gpu_pool_stats_t sub_stats;

    if (pools->scratch && nimcp_gpu_pool_get_stats(pools->scratch, &sub_stats)) {
        stats->total_size += sub_stats.total_size;
        stats->used_size += sub_stats.used_size;
        stats->peak_used += sub_stats.peak_used;
        stats->available_size += sub_stats.available_size;
        stats->allocation_count += sub_stats.allocation_count;
        stats->free_count += sub_stats.free_count;
        stats->failed_allocations += sub_stats.failed_allocations;
        stats->current_allocations += sub_stats.current_allocations;
        stats->num_blocks += sub_stats.num_blocks;
    }

    if (pools->persistent && nimcp_gpu_pool_get_stats(pools->persistent, &sub_stats)) {
        stats->total_size += sub_stats.total_size;
        stats->used_size += sub_stats.used_size;
        stats->peak_used += sub_stats.peak_used;
        stats->available_size += sub_stats.available_size;
        stats->allocation_count += sub_stats.allocation_count;
        stats->free_count += sub_stats.free_count;
        stats->failed_allocations += sub_stats.failed_allocations;
        stats->current_allocations += sub_stats.current_allocations;
        stats->num_blocks += sub_stats.num_blocks;
        stats->fragmentation_bytes += sub_stats.fragmentation_bytes;
    }

    if (pools->activation && nimcp_gpu_pool_get_stats(pools->activation, &sub_stats)) {
        stats->total_size += sub_stats.total_size;
        stats->used_size += sub_stats.used_size;
        stats->peak_used += sub_stats.peak_used;
        stats->available_size += sub_stats.available_size;
        stats->allocation_count += sub_stats.allocation_count;
        stats->free_count += sub_stats.free_count;
        stats->failed_allocations += sub_stats.failed_allocations;
        stats->current_allocations += sub_stats.current_allocations;
        stats->num_blocks += sub_stats.num_blocks;
    }

    // Calculate overall fragmentation ratio
    if (stats->used_size > 0) {
        stats->fragmentation_ratio =
            (float)stats->fragmentation_bytes / (float)stats->used_size;
    }

    return true;
}

void nimcp_composite_pool_print_stats(const nimcp_composite_pool_t* pools) {
    if (!pools) {
        printf("Composite Pool: NULL\n");
        return;
    }

    printf("=== Composite GPU Memory Pool Statistics ===\n\n");

    printf("--- Scratch Pool (Bump Allocator) ---\n");
    if (pools->scratch) {
        nimcp_gpu_pool_print_stats(pools->scratch);
    } else {
        printf("  Not configured\n");
    }

    printf("\n--- Persistent Pool (Buddy Allocator) ---\n");
    if (pools->persistent) {
        nimcp_gpu_pool_print_stats(pools->persistent);
    } else {
        printf("  Not configured\n");
    }

    printf("\n--- Activation Pool (Slab Allocator) ---\n");
    if (pools->activation) {
        nimcp_gpu_pool_print_stats(pools->activation);
    } else {
        printf("  Not configured\n");
    }

    printf("\n--- Combined Statistics ---\n");
    nimcp_gpu_pool_stats_t combined;
    if (nimcp_composite_pool_get_stats(pools, &combined)) {
        printf("  Total Size: %zu MB\n", combined.total_size / (1024 * 1024));
        printf("  Used Size: %zu MB (%.1f%%)\n",
               combined.used_size / (1024 * 1024),
               100.0 * combined.used_size / combined.total_size);
        printf("  Peak Used: %zu MB\n", combined.peak_used / (1024 * 1024));
        printf("  Available: %zu MB\n", combined.available_size / (1024 * 1024));
        printf("  Total Allocations: %llu\n", (unsigned long long)combined.allocation_count);
        printf("  Current Allocations: %llu\n", (unsigned long long)combined.current_allocations);
        printf("  Failed Allocations: %llu\n", (unsigned long long)combined.failed_allocations);
    }

    printf("\n=============================================\n");
}
