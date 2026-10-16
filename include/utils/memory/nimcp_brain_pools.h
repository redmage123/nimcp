/**
 * @file nimcp_brain_pools.h
 * @brief Brain Substrate Memory Pool System (Phase 2)
 *
 * WHAT: Specialized memory pools for brain subsystem allocations
 * WHY:  Achieve 50-100x allocation speedup over malloc for hot paths
 * HOW:  Pre-allocated pools with O(1) acquire/release and COW support
 *
 * MATHEMATICAL FOUNDATIONS:
 * 1. Shannon Entropy: H(pool) = -Σ p(size_class) log₂ p(size_class)
 *    - Measures allocation pattern diversity
 *    - Lower entropy = more predictable, better pool tuning
 *
 * 2. Queuing Theory (M/M/1): Optimal pool size N = λ/μ + k√(λ/μ)
 *    - λ = arrival rate (allocations/sec)
 *    - μ = service rate (releases/sec)
 *    - k = safety factor (typically 2-3 for 95-99% availability)
 *
 * 3. Little's Law: L = λW
 *    - L = average items in system (pool utilization)
 *    - λ = arrival rate
 *    - W = average time in system (allocation lifetime)
 *
 * 4. Information Efficiency: η = H(actual) / H(max)
 *    - Measures how well pool matches actual usage patterns
 *
 * PHASE: 2 (Substrate Layer - Brain Integration)
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 * @version 2.0.0
 */

#ifndef NIMCP_BRAIN_POOLS_H
#define NIMCP_BRAIN_POOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

//=============================================================================
// Constants
//=============================================================================

/** Maximum size classes for variable-size pools */
#define BRAIN_POOL_MAX_SIZE_CLASSES     8

/** Default pool sizes (tuned via queuing theory) */
#define BRAIN_POOL_DEFAULT_DECISIONS    256
#define BRAIN_POOL_DEFAULT_ACTIVATIONS  512
#define BRAIN_POOL_DEFAULT_SPIKES       4096
#define BRAIN_POOL_DEFAULT_FEATURES     1024

/** Size class boundaries (bytes) */
#define BRAIN_POOL_SIZE_TINY            64
#define BRAIN_POOL_SIZE_SMALL           256
#define BRAIN_POOL_SIZE_MEDIUM          1024
#define BRAIN_POOL_SIZE_LARGE           4096
#define BRAIN_POOL_SIZE_XLARGE          16384

/** Performance targets */
#define BRAIN_POOL_TARGET_ACQUIRE_NS    50      /* <50ns acquire time */
#define BRAIN_POOL_TARGET_SPEEDUP       50      /* 50x vs malloc */

//=============================================================================
// Type Definitions
//=============================================================================

/** Opaque pool handle */
typedef struct brain_pools* brain_pools_t;

/**
 * @brief Size class enumeration for variable-size allocations
 */
typedef enum {
    POOL_SIZE_TINY = 0,     /* 64 bytes */
    POOL_SIZE_SMALL,        /* 256 bytes */
    POOL_SIZE_MEDIUM,       /* 1024 bytes */
    POOL_SIZE_LARGE,        /* 4096 bytes */
    POOL_SIZE_XLARGE,       /* 16384 bytes */
    POOL_SIZE_COUNT
} pool_size_class_t;

/**
 * @brief Pool configuration with mathematical optimization hints
 *
 * MATHEMATICAL MODEL:
 * Optimal size = base_size × (1 + overalloc_factor × √(variance/mean))
 * This accounts for allocation pattern variability.
 */
typedef struct {
    /* Pool capacities */
    size_t decision_pool_capacity;      /* Pre-allocated decision structures */
    size_t activation_pool_capacity;    /* Activation array blocks */
    size_t spike_pool_capacity;         /* Spike event structures */
    size_t feature_pool_capacity;       /* Feature buffer blocks */

    /* Size class capacities for variable-size pools */
    size_t size_class_capacities[POOL_SIZE_COUNT];

    /* Block sizes */
    size_t decision_block_size;         /* Size of decision structure */
    size_t activation_block_size;       /* Default activation array size */
    size_t spike_block_size;            /* Size of spike event */

    /* Mathematical tuning parameters */
    float overallocation_factor;        /* Pool overallocation (1.0-2.0) */
    float safety_factor_k;              /* Queuing theory k (2.0-3.0) */
    float target_utilization;           /* Target pool utilization (0.7-0.9) */

    /* Feature flags */
    bool enable_metrics;                /* Track allocation metrics */
    bool enable_cow;                    /* Enable COW for templates */
    bool enable_adaptive_sizing;        /* Auto-adjust pool sizes */
    bool enable_shannon_tracking;       /* Track Shannon entropy */

    /* Memory alignment */
    size_t alignment;                   /* Memory alignment (16, 32, 64) */
} brain_pools_config_t;

/**
 * @brief Per-pool statistics
 */
typedef struct {
    uint64_t total_acquires;            /* Total acquisitions */
    uint64_t total_releases;            /* Total releases */
    uint64_t current_in_use;            /* Currently allocated */
    uint64_t peak_in_use;               /* Peak allocation */
    uint64_t failed_acquires;           /* Failed due to exhaustion */
    uint64_t cow_triggers;              /* COW copy operations */
    uint64_t total_acquire_time_ns;     /* Cumulative acquire time */
    uint64_t total_release_time_ns;     /* Cumulative release time */
} pool_stats_t;

/**
 * @brief Shannon entropy metrics for allocation patterns
 *
 * MATHEMATICAL DEFINITION:
 * H = -Σ p(i) log₂ p(i)  where p(i) = count(i) / total
 *
 * INTERPRETATION:
 * - H = 0: All allocations same size (perfect prediction)
 * - H = log₂(n): Uniform distribution (maximum uncertainty)
 * - Lower H = Better pool tuning opportunity
 */
typedef struct {
    float entropy_bits;                 /* Shannon entropy H(X) */
    float max_entropy_bits;             /* Maximum possible entropy */
    float efficiency;                   /* η = H/H_max (0-1) */
    float redundancy;                   /* R = 1 - η */
    uint64_t size_class_counts[POOL_SIZE_COUNT];  /* Per-class counts */
} shannon_metrics_t;

/**
 * @brief Queuing theory metrics for pool optimization
 *
 * MATHEMATICAL MODEL (M/M/c queue):
 * - ρ = λ/(cμ) = utilization
 * - L = ρ/(1-ρ) = average queue length
 * - W = 1/(μ-λ) = average wait time
 * - P_block = ρ^c × c!/(c-1)! × (1-ρ) (Erlang B)
 */
typedef struct {
    float arrival_rate_lambda;          /* λ: allocations per second */
    float service_rate_mu;              /* μ: releases per second */
    float utilization_rho;              /* ρ = λ/μ */
    float avg_queue_length;             /* L = ρ/(1-ρ) */
    float avg_wait_time_us;             /* W in microseconds */
    float blocking_probability;         /* P(pool exhausted) */
    size_t recommended_capacity;        /* Optimal N = λ/μ + k√(λ/μ) */
} queuing_metrics_t;

/**
 * @brief Comprehensive brain pools metrics
 */
typedef struct {
    /* Per-pool statistics */
    pool_stats_t decision_stats;
    pool_stats_t activation_stats;
    pool_stats_t spike_stats;
    pool_stats_t feature_stats;
    pool_stats_t size_class_stats[POOL_SIZE_COUNT];

    /* Information-theoretic metrics */
    shannon_metrics_t shannon;

    /* Queuing theory metrics */
    queuing_metrics_t queuing;

    /* Aggregate metrics */
    uint64_t total_memory_bytes;        /* Total pool memory */
    uint64_t used_memory_bytes;         /* Currently in use */
    uint64_t saved_memory_bytes;        /* Memory saved via COW */
    float avg_acquire_ns;               /* Average acquire latency */
    float avg_release_ns;               /* Average release latency */
    float speedup_vs_malloc;            /* Measured speedup */

    /* Timing */
    uint64_t uptime_ms;                 /* Time since creation */
    uint64_t last_update_ms;            /* Last metrics update */
} brain_pools_metrics_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default configuration optimized via queuing theory
 *
 * MATHEMATICAL BASIS:
 * Default sizes computed as N = λ_expected/μ_expected + 3√(λ/μ)
 * with λ=1000 alloc/s, μ=1000 release/s, k=3 for 99.7% availability
 *
 * @return Default configuration
 */
static inline brain_pools_config_t brain_pools_default_config(void) {
    brain_pools_config_t config = {
        /* Pool capacities (queuing theory optimized) */
        .decision_pool_capacity = BRAIN_POOL_DEFAULT_DECISIONS,
        .activation_pool_capacity = BRAIN_POOL_DEFAULT_ACTIVATIONS,
        .spike_pool_capacity = BRAIN_POOL_DEFAULT_SPIKES,
        .feature_pool_capacity = BRAIN_POOL_DEFAULT_FEATURES,

        /* Size class capacities */
        .size_class_capacities = {
            [POOL_SIZE_TINY] = 2048,
            [POOL_SIZE_SMALL] = 1024,
            [POOL_SIZE_MEDIUM] = 512,
            [POOL_SIZE_LARGE] = 256,
            [POOL_SIZE_XLARGE] = 64
        },

        /* Block sizes */
        .decision_block_size = 512,     /* sizeof(brain_decision_t) estimate */
        .activation_block_size = 4096,  /* 1024 floats */
        .spike_block_size = 64,         /* Compact spike event */

        /* Mathematical tuning */
        .overallocation_factor = 1.2f,  /* 20% headroom */
        .safety_factor_k = 3.0f,        /* 99.7% availability */
        .target_utilization = 0.75f,    /* 75% target utilization */

        /* Features */
        .enable_metrics = true,
        .enable_cow = true,
        .enable_adaptive_sizing = false,
        .enable_shannon_tracking = true,

        /* Alignment */
        .alignment = 64                 /* Cache line alignment */
    };
    return config;
}

/**
 * @brief Compute optimal pool size using queuing theory
 *
 * FORMULA: N = λ/μ + k × √(λ/μ)
 *
 * @param arrival_rate Expected allocations per second
 * @param service_rate Expected releases per second
 * @param safety_factor k value (2.0 = 95%, 3.0 = 99.7%)
 * @return Recommended pool capacity
 */
static inline size_t brain_pools_optimal_size(
    float arrival_rate,
    float service_rate,
    float safety_factor)
{
    if (service_rate <= 0.0f) return 0;
    float rho = arrival_rate / service_rate;
    float base = rho;
    float margin = safety_factor * sqrtf(rho);
    return (size_t)(base + margin + 0.5f);
}

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create brain pools with given configuration
 *
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on error
 */
brain_pools_t brain_pools_create(const brain_pools_config_t* config);

/**
 * @brief Destroy brain pools and release all memory
 *
 * @param pools Pool handle
 */
void brain_pools_destroy(brain_pools_t pools);

//=============================================================================
// Decision Pool API
//=============================================================================

/**
 * @brief Acquire decision structure from pool
 *
 * PERFORMANCE: O(1), <50ns target
 *
 * @param pools Pool handle
 * @return Pointer to decision block or NULL if exhausted
 */
void* brain_pools_acquire_decision(brain_pools_t pools);

/**
 * @brief Release decision structure back to pool
 *
 * @param pools Pool handle
 * @param decision Previously acquired decision
 */
void brain_pools_release_decision(brain_pools_t pools, void* decision);

//=============================================================================
// Activation Pool API
//=============================================================================

/**
 * @brief Acquire activation array from pool
 *
 * @param pools Pool handle
 * @param num_floats Number of floats needed
 * @return Pointer to float array or NULL if exhausted
 */
float* brain_pools_acquire_activation(brain_pools_t pools, size_t num_floats);

/**
 * @brief Release activation array back to pool
 *
 * @param pools Pool handle
 * @param activation Previously acquired array
 */
void brain_pools_release_activation(brain_pools_t pools, float* activation);

//=============================================================================
// Spike Event Pool API
//=============================================================================

/**
 * @brief Acquire spike event structure from pool
 *
 * PERFORMANCE: Critical path - O(1), <30ns target
 *
 * @param pools Pool handle
 * @return Pointer to spike event or NULL if exhausted
 */
void* brain_pools_acquire_spike_event(brain_pools_t pools);

/**
 * @brief Release spike event back to pool
 *
 * @param pools Pool handle
 * @param event Previously acquired event
 */
void brain_pools_release_spike_event(brain_pools_t pools, void* event);

/**
 * @brief Acquire batch of spike events (optimized for burst allocation)
 *
 * MATHEMATICAL NOTE: Batch allocation reduces mutex overhead
 * Speedup ≈ n / (1 + (n-1) × mutex_overhead_ratio)
 *
 * @param pools Pool handle
 * @param count Number of events to acquire
 * @param events Output array of event pointers
 * @return Number of events actually acquired
 */
size_t brain_pools_acquire_spike_batch(
    brain_pools_t pools,
    size_t count,
    void** events);

/**
 * @brief Release batch of spike events
 *
 * @param pools Pool handle
 * @param count Number of events to release
 * @param events Array of event pointers
 */
void brain_pools_release_spike_batch(
    brain_pools_t pools,
    size_t count,
    void** events);

//=============================================================================
// Feature Buffer Pool API (Size-Class Based)
//=============================================================================

/**
 * @brief Acquire feature buffer with automatic size class selection
 *
 * SIZE CLASS SELECTION:
 * - Finds smallest size class >= requested size
 * - Falls back to larger classes if preferred exhausted
 *
 * @param pools Pool handle
 * @param min_bytes Minimum buffer size in bytes
 * @param actual_size Output: actual allocated size (may be larger)
 * @return Buffer pointer or NULL if exhausted
 */
void* brain_pools_acquire_feature_buffer(
    brain_pools_t pools,
    size_t min_bytes,
    size_t* actual_size);

/**
 * @brief Release feature buffer back to pool
 *
 * @param pools Pool handle
 * @param buffer Previously acquired buffer
 * @param size Size that was allocated (from acquire)
 */
void brain_pools_release_feature_buffer(
    brain_pools_t pools,
    void* buffer,
    size_t size);

//=============================================================================
// COW (Copy-On-Write) API
//=============================================================================

/**
 * @brief Acquire COW handle for template data sharing
 *
 * MATHEMATICAL BENEFIT:
 * Memory savings = (N-1) × size for N readers
 * where only first writer triggers actual copy
 *
 * @param pools Pool handle
 * @param pool_type Which pool to use (0=decision, 1=activation, etc.)
 * @return COW handle or NULL on error
 */
void* brain_pools_cow_acquire(brain_pools_t pools, int pool_type);

/**
 * @brief Get read-only pointer (no copy triggered)
 *
 * @param handle COW handle
 * @return Read-only pointer to data
 */
const void* brain_pools_cow_read(void* handle);

/**
 * @brief Get writable pointer (triggers copy if shared)
 *
 * @param handle COW handle
 * @return Writable pointer to private copy
 */
void* brain_pools_cow_write(void* handle);

/**
 * @brief Release COW handle
 *
 * @param pools Pool handle
 * @param handle COW handle to release
 */
void brain_pools_cow_release(brain_pools_t pools, void* handle);

//=============================================================================
// Metrics API
//=============================================================================

/**
 * @brief Get comprehensive pool metrics
 *
 * @param pools Pool handle
 * @param metrics Output metrics structure
 * @return true on success
 */
bool brain_pools_get_metrics(brain_pools_t pools, brain_pools_metrics_t* metrics);

/**
 * @brief Reset all metrics counters
 *
 * @param pools Pool handle
 */
void brain_pools_reset_metrics(brain_pools_t pools);

/**
 * @brief Get Shannon entropy of allocation patterns
 *
 * @param pools Pool handle
 * @param shannon Output Shannon metrics
 * @return true on success
 */
bool brain_pools_get_shannon_metrics(brain_pools_t pools, shannon_metrics_t* shannon);

/**
 * @brief Get queuing theory metrics for optimization
 *
 * @param pools Pool handle
 * @param queuing Output queuing metrics
 * @return true on success
 */
bool brain_pools_get_queuing_metrics(brain_pools_t pools, queuing_metrics_t* queuing);

/**
 * @brief Check if pools are performant (meeting targets)
 *
 * @param pools Pool handle
 * @return true if meeting performance targets
 */
bool brain_pools_is_performant(brain_pools_t pools);

/**
 * @brief Get recommended configuration based on observed patterns
 *
 * Uses queuing theory and Shannon entropy to optimize pool sizes.
 *
 * @param pools Pool handle
 * @param recommended Output recommended configuration
 * @return true on success
 */
bool brain_pools_get_recommended_config(
    brain_pools_t pools,
    brain_pools_config_t* recommended);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate total memory required for configuration
 *
 * @param config Pool configuration
 * @return Total bytes required
 */
size_t brain_pools_calculate_memory(const brain_pools_config_t* config);

/**
 * @brief Get size class for given byte size
 *
 * @param bytes Requested size
 * @return Appropriate size class
 */
pool_size_class_t brain_pools_get_size_class(size_t bytes);

/**
 * @brief Get actual size for size class
 *
 * @param size_class Size class enum
 * @return Actual block size in bytes
 */
size_t brain_pools_get_class_size(pool_size_class_t size_class);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_POOLS_H */
