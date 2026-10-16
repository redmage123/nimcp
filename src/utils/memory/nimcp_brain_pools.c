/**
 * @file nimcp_brain_pools.c
 * @brief Brain Substrate Memory Pool System Implementation (Phase 2)
 *
 * WHAT: Specialized memory pools for brain subsystem allocations
 * WHY:  Achieve 50-100x allocation speedup over malloc for hot paths
 * HOW:  Pre-allocated pools with O(1) acquire/release and COW support
 *
 * MATHEMATICAL FOUNDATIONS:
 * 1. Shannon Entropy: H(pool) = -Σ p(i) log₂ p(i)
 * 2. Queuing Theory: N_opt = λ/μ + k√(λ/μ)
 * 3. Little's Law: L = λW
 *
 * PHASE: 2 (Substrate Layer - Brain Integration)
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include "utils/memory/nimcp_brain_pools.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_pools)

//=============================================================================
// Constants
//=============================================================================

/** Size class boundaries in bytes */
static const size_t SIZE_CLASS_BYTES[POOL_SIZE_COUNT] = {
    BRAIN_POOL_SIZE_TINY,       /* 64 */
    BRAIN_POOL_SIZE_SMALL,      /* 256 */
    BRAIN_POOL_SIZE_MEDIUM,     /* 1024 */
    BRAIN_POOL_SIZE_LARGE,      /* 4096 */
    BRAIN_POOL_SIZE_XLARGE      /* 16384 */
};

/** Natural log of 2 for entropy calculations */
#define LN2 0.693147180559945f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal brain pools structure
 */
struct brain_pools {
    /* Configuration */
    brain_pools_config_t config;

    /* Specialized pools */
    memory_pool_t decision_pool;
    memory_pool_t activation_pool;
    memory_pool_t spike_pool;

    /* Size-class pools for variable allocations */
    memory_pool_t size_class_pools[POOL_SIZE_COUNT];

    /* COW managers for template sharing */
    cow_manager_t decision_cow;
    cow_manager_t activation_cow;

    /* Statistics */
    pool_stats_t decision_stats;
    pool_stats_t activation_stats;
    pool_stats_t spike_stats;
    pool_stats_t feature_stats;
    pool_stats_t size_class_stats[POOL_SIZE_COUNT];

    /* Shannon entropy tracking */
    uint64_t size_class_counts[POOL_SIZE_COUNT];
    uint64_t total_feature_allocs;

    /* Queuing metrics tracking */
    uint64_t first_alloc_time_ms;
    uint64_t last_alloc_time_ms;
    uint64_t total_alloc_count;
    uint64_t total_release_count;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;

    /* State */
    bool initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static inline uint64_t get_time_ns(void) {
    return nimcp_time_monotonic_ns();
}

/**
 * @brief Get current time in milliseconds
 */
static inline uint64_t get_time_ms(void) {
    return nimcp_time_monotonic_ms();
}

/**
 * @brief Calculate Shannon entropy from counts
 *
 * FORMULA: H = -Σ p(i) log₂ p(i)
 *        = -Σ (n_i/N) log₂(n_i/N)
 *        = log₂(N) - (1/N) Σ n_i log₂(n_i)
 *
 * @param counts Array of counts per category
 * @param num_categories Number of categories
 * @param total Total count (sum of counts)
 * @return Shannon entropy in bits
 */
static float calculate_shannon_entropy(
    const uint64_t* counts,
    size_t num_categories,
    uint64_t total)
{
    if (total == 0) return 0.0F;

    float entropy = 0.0F;
    float total_f = (float)total;

    for (size_t i = 0; i < num_categories; i++) {
        if (counts[i] > 0) {
            float p = (float)counts[i] / total_f;
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

/**
 * @brief Calculate maximum possible entropy
 *
 * H_max = log₂(n) for n equally likely categories
 */
static float calculate_max_entropy(size_t num_categories) {
    if (num_categories <= 1) return 0.0F;
    return log2f((float)num_categories);
}

/**
 * @brief Calculate queuing metrics from timing data
 *
 * MATHEMATICAL MODEL (M/M/1):
 * - λ = alloc_count / elapsed_time
 * - μ = release_count / elapsed_time
 * - ρ = λ/μ
 * - L = ρ/(1-ρ)
 * - W = 1/(μ-λ)
 */
static void calculate_queuing_metrics(
    uint64_t alloc_count,
    uint64_t release_count,
    uint64_t elapsed_ms,
    float safety_factor_k,
    queuing_metrics_t* metrics)
{
    if (elapsed_ms == 0 || alloc_count == 0) {
        memset(metrics, 0, sizeof(queuing_metrics_t));
        return;
    }

    float elapsed_sec = (float)elapsed_ms / (float)NIMCP_MS_PER_SEC;

    /* Arrival and service rates */
    metrics->arrival_rate_lambda = (float)alloc_count / elapsed_sec;
    metrics->service_rate_mu = (float)release_count / elapsed_sec;

    /* Utilization */
    if (metrics->service_rate_mu > 0.0F) {
        metrics->utilization_rho = metrics->arrival_rate_lambda / metrics->service_rate_mu;
    } else {
        metrics->utilization_rho = 1.0F;
    }

    /* Queue length (M/M/1 model) */
    if (metrics->utilization_rho < 1.0F) {
        metrics->avg_queue_length = metrics->utilization_rho / (1.0F - metrics->utilization_rho);
    } else {
        metrics->avg_queue_length = INFINITY;
    }

    /* Wait time */
    float rate_diff = metrics->service_rate_mu - metrics->arrival_rate_lambda;
    if (rate_diff > 0.0F) {
        metrics->avg_wait_time_us = (float)NIMCP_US_PER_SEC / rate_diff;
    } else {
        metrics->avg_wait_time_us = INFINITY;
    }

    /* Blocking probability (Erlang B approximation) */
    if (metrics->utilization_rho < 1.0F) {
        metrics->blocking_probability = powf(metrics->utilization_rho, 10.0F) *
                                         (1.0F - metrics->utilization_rho);
    } else {
        metrics->blocking_probability = 1.0F;
    }

    /* Recommended capacity: N = λ/μ + k√(λ/μ) */
    if (metrics->service_rate_mu > 0.0F) {
        float base = metrics->utilization_rho;
        float margin = safety_factor_k * sqrtf(base > 0 ? base : 0.0F);
        metrics->recommended_capacity = (size_t)(base + margin + 1.0F);
    } else {
        metrics->recommended_capacity = (size_t)(metrics->arrival_rate_lambda * 2.0F);
    }
}

/**
 * @brief Update pool statistics after acquire
 */
static void update_stats_acquire(pool_stats_t* stats, uint64_t elapsed_ns, bool success) {
    if (success) {
        stats->total_acquires++;
        stats->current_in_use++;
        if (stats->current_in_use > stats->peak_in_use) {
            stats->peak_in_use = stats->current_in_use;
        }
        stats->total_acquire_time_ns += elapsed_ns;
    } else {
        stats->failed_acquires++;
    }
}

/**
 * @brief Update pool statistics after release
 */
static void update_stats_release(pool_stats_t* stats, uint64_t elapsed_ns) {
    stats->total_releases++;
    if (stats->current_in_use > 0) {
        stats->current_in_use--;
    }
    stats->total_release_time_ns += elapsed_ns;
}

//=============================================================================
// Core API Implementation
//=============================================================================

brain_pools_t brain_pools_create(const brain_pools_config_t* config) {
    /* Use defaults if no config provided */
    brain_pools_config_t cfg = config ? *config : brain_pools_default_config();

    /* Allocate pool structure */
    struct brain_pools* pools = nimcp_calloc(1, sizeof(struct brain_pools));
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pools is NULL");

        return NULL;

    }

    pools->config = cfg;

    /* Initialize mutex (non-recursive) */
    if (nimcp_platform_mutex_init(&pools->mutex, false) != 0) {
        nimcp_free(pools);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "brain_pools_create: validation failed");
        return NULL;
    }

    /* Create decision pool */
    memory_pool_config_t mp_config = {
        .block_size = cfg.decision_block_size,
        .num_blocks = cfg.decision_pool_capacity,
        .alignment = cfg.alignment,
        .enable_tracking = cfg.enable_metrics,
        .enable_guard_pages = false
    };
    pools->decision_pool = memory_pool_create(&mp_config);

    /* Create activation pool */
    mp_config.block_size = cfg.activation_block_size;
    mp_config.num_blocks = cfg.activation_pool_capacity;
    pools->activation_pool = memory_pool_create(&mp_config);

    /* Create spike pool (high capacity for hot path) */
    mp_config.block_size = cfg.spike_block_size;
    mp_config.num_blocks = cfg.spike_pool_capacity;
    pools->spike_pool = memory_pool_create(&mp_config);

    /* Create size-class pools */
    for (int i = 0; i < POOL_SIZE_COUNT; i++) {
        mp_config.block_size = SIZE_CLASS_BYTES[i];
        mp_config.num_blocks = cfg.size_class_capacities[i];
        pools->size_class_pools[i] = memory_pool_create(&mp_config);
    }

    /* Create COW managers if enabled */
    if (cfg.enable_cow) {
        /* Decision COW template */
        void* decision_template = nimcp_calloc(1, cfg.decision_block_size);
        if (decision_template) {
            cow_manager_config_t cow_cfg = {
                .data_size = cfg.decision_block_size,
                .pool = pools->decision_pool,
                .copy_fn = NULL,
                .dtor_fn = NULL,
                .user_data = NULL,
                .enable_tracking = cfg.enable_metrics
            };
            pools->decision_cow = cow_manager_create(&cow_cfg, decision_template);
            nimcp_free(decision_template);
        }

        /* Activation COW template */
        void* activation_template = nimcp_calloc(1, cfg.activation_block_size);
        if (activation_template) {
            cow_manager_config_t cow_cfg = {
                .data_size = cfg.activation_block_size,
                .pool = pools->activation_pool,
                .copy_fn = NULL,
                .dtor_fn = NULL,
                .user_data = NULL,
                .enable_tracking = cfg.enable_metrics
            };
            pools->activation_cow = cow_manager_create(&cow_cfg, activation_template);
            nimcp_free(activation_template);
        }
    }

    /* Initialize timing */
    pools->first_alloc_time_ms = get_time_ms();
    pools->last_alloc_time_ms = pools->first_alloc_time_ms;
    pools->initialized = true;

    return pools;
}

void brain_pools_destroy(brain_pools_t pools) {
    if (!pools) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Destroy COW managers */
    if (pools->decision_cow) {
        cow_manager_destroy(pools->decision_cow);
    }
    if (pools->activation_cow) {
        cow_manager_destroy(pools->activation_cow);
    }

    /* Destroy specialized pools */
    if (pools->decision_pool) {
        memory_pool_destroy(pools->decision_pool);
    }
    if (pools->activation_pool) {
        memory_pool_destroy(pools->activation_pool);
    }
    if (pools->spike_pool) {
        memory_pool_destroy(pools->spike_pool);
    }

    /* Destroy size-class pools */
    for (int i = 0; i < POOL_SIZE_COUNT; i++) {
        if (pools->size_class_pools[i]) {
            memory_pool_destroy(pools->size_class_pools[i]);
        }
    }

    nimcp_platform_mutex_unlock(&pools->mutex);
    nimcp_platform_mutex_destroy(&pools->mutex);

    nimcp_free(pools);
}

//=============================================================================
// Decision Pool Implementation
//=============================================================================

void* brain_pools_acquire_decision(brain_pools_t pools) {
    if (!pools || !pools->decision_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_pools_acquire_decision: required parameter is NULL (pools, pools->decision_pool)");
        return NULL;
    }

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    void* block = memory_pool_acquire(pools->decision_pool);
    uint64_t elapsed_ns = get_time_ns() - start_ns;

    update_stats_acquire(&pools->decision_stats, elapsed_ns, block != NULL);
    pools->total_alloc_count++;
    pools->last_alloc_time_ms = get_time_ms();

    nimcp_platform_mutex_unlock(&pools->mutex);

    return block;
}

void brain_pools_release_decision(brain_pools_t pools, void* decision) {
    if (!pools || !pools->decision_pool || !decision) return;

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    memory_pool_release(pools->decision_pool, decision);
    uint64_t elapsed_ns = get_time_ns() - start_ns;

    update_stats_release(&pools->decision_stats, elapsed_ns);
    pools->total_release_count++;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// Activation Pool Implementation
//=============================================================================

float* brain_pools_acquire_activation(brain_pools_t pools, size_t num_floats) {
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    size_t required_bytes = num_floats * sizeof(float);
    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    float* block = NULL;

    /* Use activation pool if size fits, otherwise use size-class pools */
    if (required_bytes <= pools->config.activation_block_size && pools->activation_pool) {
        block = memory_pool_acquire(pools->activation_pool);
    } else {
        /* Find appropriate size class */
        for (int i = 0; i < POOL_SIZE_COUNT; i++) {
            if (required_bytes <= SIZE_CLASS_BYTES[i] && pools->size_class_pools[i]) {
                block = memory_pool_acquire(pools->size_class_pools[i]);
                if (block) {
                    pools->size_class_counts[i]++;
                    pools->total_feature_allocs++;
                    break;
                }
            }
        }
    }

    uint64_t elapsed_ns = get_time_ns() - start_ns;
    update_stats_acquire(&pools->activation_stats, elapsed_ns, block != NULL);
    pools->total_alloc_count++;
    pools->last_alloc_time_ms = get_time_ms();

    nimcp_platform_mutex_unlock(&pools->mutex);

    return block;
}

void brain_pools_release_activation(brain_pools_t pools, float* activation) {
    if (!pools || !activation) return;

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Try to release to activation pool first */
    /* Note: In production, would track which pool it came from */
    memory_pool_release(pools->activation_pool, activation);

    uint64_t elapsed_ns = get_time_ns() - start_ns;
    update_stats_release(&pools->activation_stats, elapsed_ns);
    pools->total_release_count++;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// Spike Event Pool Implementation
//=============================================================================

void* brain_pools_acquire_spike_event(brain_pools_t pools) {
    if (!pools || !pools->spike_pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_pools_acquire_spike_event: required parameter is NULL (pools, pools->spike_pool)");
        return NULL;
    }

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    void* block = memory_pool_acquire(pools->spike_pool);
    uint64_t elapsed_ns = get_time_ns() - start_ns;

    update_stats_acquire(&pools->spike_stats, elapsed_ns, block != NULL);
    pools->total_alloc_count++;
    pools->last_alloc_time_ms = get_time_ms();

    nimcp_platform_mutex_unlock(&pools->mutex);

    return block;
}

void brain_pools_release_spike_event(brain_pools_t pools, void* event) {
    if (!pools || !pools->spike_pool || !event) return;

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    memory_pool_release(pools->spike_pool, event);
    uint64_t elapsed_ns = get_time_ns() - start_ns;

    update_stats_release(&pools->spike_stats, elapsed_ns);
    pools->total_release_count++;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

size_t brain_pools_acquire_spike_batch(
    brain_pools_t pools,
    size_t count,
    void** events)
{
    if (!pools || !pools->spike_pool || !events || count == 0) return 0;

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    size_t acquired = 0;
    for (size_t i = 0; i < count; i++) {
        events[i] = memory_pool_acquire(pools->spike_pool);
        if (events[i]) {
            acquired++;
        } else {
            break;
        }
    }

    uint64_t elapsed_ns = get_time_ns() - start_ns;

    pools->spike_stats.total_acquires += acquired;
    pools->spike_stats.current_in_use += acquired;
    if (pools->spike_stats.current_in_use > pools->spike_stats.peak_in_use) {
        pools->spike_stats.peak_in_use = pools->spike_stats.current_in_use;
    }
    pools->spike_stats.total_acquire_time_ns += elapsed_ns;
    pools->total_alloc_count += acquired;
    pools->last_alloc_time_ms = get_time_ms();

    nimcp_platform_mutex_unlock(&pools->mutex);

    return acquired;
}

void brain_pools_release_spike_batch(
    brain_pools_t pools,
    size_t count,
    void** events)
{
    if (!pools || !pools->spike_pool || !events || count == 0) return;

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    for (size_t i = 0; i < count; i++) {
        if (events[i]) {
            memory_pool_release(pools->spike_pool, events[i]);
        }
    }

    uint64_t elapsed_ns = get_time_ns() - start_ns;

    pools->spike_stats.total_releases += count;
    if (pools->spike_stats.current_in_use >= count) {
        pools->spike_stats.current_in_use -= count;
    } else {
        pools->spike_stats.current_in_use = 0;
    }
    pools->spike_stats.total_release_time_ns += elapsed_ns;
    pools->total_release_count += count;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// Feature Buffer Pool Implementation
//=============================================================================

void* brain_pools_acquire_feature_buffer(
    brain_pools_t pools,
    size_t min_bytes,
    size_t* actual_size)
{
    if (!pools || min_bytes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_pools_acquire_feature_buffer: pools is NULL");
        return NULL;
    }

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    void* block = NULL;
    size_t allocated_size = 0;

    /* Find smallest size class that fits */
    for (int i = 0; i < POOL_SIZE_COUNT; i++) {
        if (min_bytes <= SIZE_CLASS_BYTES[i] && pools->size_class_pools[i]) {
            block = memory_pool_acquire(pools->size_class_pools[i]);
            if (block) {
                allocated_size = SIZE_CLASS_BYTES[i];
                pools->size_class_counts[i]++;
                pools->total_feature_allocs++;
                update_stats_acquire(&pools->size_class_stats[i],
                                    get_time_ns() - start_ns, true);
                break;
            }
        }
    }

    /* Fallback: try larger size classes */
    if (!block) {
        for (int i = 0; i < POOL_SIZE_COUNT; i++) {
            if (pools->size_class_pools[i]) {
                block = memory_pool_acquire(pools->size_class_pools[i]);
                if (block) {
                    allocated_size = SIZE_CLASS_BYTES[i];
                    pools->size_class_counts[i]++;
                    pools->total_feature_allocs++;
                    update_stats_acquire(&pools->size_class_stats[i],
                                        get_time_ns() - start_ns, true);
                    break;
                }
            }
        }
    }

    uint64_t elapsed_ns = get_time_ns() - start_ns;
    update_stats_acquire(&pools->feature_stats, elapsed_ns, block != NULL);
    pools->total_alloc_count++;
    pools->last_alloc_time_ms = get_time_ms();

    if (actual_size) {
        *actual_size = allocated_size;
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return block;
}

void brain_pools_release_feature_buffer(
    brain_pools_t pools,
    void* buffer,
    size_t size)
{
    if (!pools || !buffer) return;

    uint64_t start_ns = get_time_ns();

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Find the pool this came from based on size */
    pool_size_class_t size_class = brain_pools_get_size_class(size);
    if (pools->size_class_pools[size_class]) {
        memory_pool_release(pools->size_class_pools[size_class], buffer);
        update_stats_release(&pools->size_class_stats[size_class],
                            get_time_ns() - start_ns);
    }

    uint64_t elapsed_ns = get_time_ns() - start_ns;
    update_stats_release(&pools->feature_stats, elapsed_ns);
    pools->total_release_count++;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// COW API Implementation
//=============================================================================

void* brain_pools_cow_acquire(brain_pools_t pools, int pool_type) {
    if (!pools) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pools is NULL");

        return NULL;

    }

    nimcp_platform_mutex_lock(&pools->mutex);

    cow_handle_t handle = NULL;
    switch (pool_type) {
        case 0: /* Decision */
            if (pools->decision_cow) {
                handle = cow_acquire(pools->decision_cow);
            }
            break;
        case 1: /* Activation */
            if (pools->activation_cow) {
                handle = cow_acquire(pools->activation_cow);
            }
            break;
        default:
            break;
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return handle;
}

const void* brain_pools_cow_read(void* handle) {
    if (!handle) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;

    }
    return cow_read((cow_handle_t)handle);
}

void* brain_pools_cow_write(void* handle) {
    if (!handle) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "handle is NULL");

        return NULL;

    }
    return cow_write((cow_handle_t)handle);
}

void brain_pools_cow_release(brain_pools_t pools, void* handle) {
    if (!pools || !handle) return;

    nimcp_platform_mutex_lock(&pools->mutex);
    cow_release((cow_handle_t)handle);
    nimcp_platform_mutex_unlock(&pools->mutex);
}

//=============================================================================
// Metrics API Implementation
//=============================================================================

bool brain_pools_get_metrics(brain_pools_t pools, brain_pools_metrics_t* metrics) {
    if (!pools || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_pools_get_metrics: required parameter is NULL (pools, metrics)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    memset(metrics, 0, sizeof(brain_pools_metrics_t));

    /* Copy per-pool stats */
    metrics->decision_stats = pools->decision_stats;
    metrics->activation_stats = pools->activation_stats;
    metrics->spike_stats = pools->spike_stats;
    metrics->feature_stats = pools->feature_stats;
    memcpy(metrics->size_class_stats, pools->size_class_stats,
           sizeof(pools->size_class_stats));

    /* Calculate Shannon metrics */
    metrics->shannon.entropy_bits = calculate_shannon_entropy(
        pools->size_class_counts, POOL_SIZE_COUNT, pools->total_feature_allocs);
    metrics->shannon.max_entropy_bits = calculate_max_entropy(POOL_SIZE_COUNT);
    if (metrics->shannon.max_entropy_bits > 0.0F) {
        metrics->shannon.efficiency = metrics->shannon.entropy_bits /
                                      metrics->shannon.max_entropy_bits;
    }
    metrics->shannon.redundancy = 1.0F - metrics->shannon.efficiency;
    memcpy(metrics->shannon.size_class_counts, pools->size_class_counts,
           sizeof(pools->size_class_counts));

    /* Calculate queuing metrics */
    uint64_t elapsed_ms = pools->last_alloc_time_ms - pools->first_alloc_time_ms;
    calculate_queuing_metrics(
        pools->total_alloc_count,
        pools->total_release_count,
        elapsed_ms,
        pools->config.safety_factor_k,
        &metrics->queuing);

    /* Calculate aggregate metrics */
    metrics->total_memory_bytes = brain_pools_calculate_memory(&pools->config);

    uint64_t total_acquires = pools->decision_stats.total_acquires +
                              pools->activation_stats.total_acquires +
                              pools->spike_stats.total_acquires +
                              pools->feature_stats.total_acquires;
    uint64_t total_acquire_time = pools->decision_stats.total_acquire_time_ns +
                                   pools->activation_stats.total_acquire_time_ns +
                                   pools->spike_stats.total_acquire_time_ns +
                                   pools->feature_stats.total_acquire_time_ns;

    if (total_acquires > 0) {
        metrics->avg_acquire_ns = (float)total_acquire_time / (float)total_acquires;
    }

    uint64_t total_releases = pools->decision_stats.total_releases +
                              pools->activation_stats.total_releases +
                              pools->spike_stats.total_releases +
                              pools->feature_stats.total_releases;
    uint64_t total_release_time = pools->decision_stats.total_release_time_ns +
                                   pools->activation_stats.total_release_time_ns +
                                   pools->spike_stats.total_release_time_ns +
                                   pools->feature_stats.total_release_time_ns;

    if (total_releases > 0) {
        metrics->avg_release_ns = (float)total_release_time / (float)total_releases;
    }

    /* Estimate speedup vs malloc (malloc ~ 150-300ns, we target <50ns) */
    if (metrics->avg_acquire_ns > 0.0F) {
        metrics->speedup_vs_malloc = 200.0F / metrics->avg_acquire_ns;
    }

    /* Timing */
    metrics->uptime_ms = elapsed_ms;
    metrics->last_update_ms = get_time_ms();

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

void brain_pools_reset_metrics(brain_pools_t pools) {
    if (!pools) return;

    nimcp_platform_mutex_lock(&pools->mutex);

    memset(&pools->decision_stats, 0, sizeof(pool_stats_t));
    memset(&pools->activation_stats, 0, sizeof(pool_stats_t));
    memset(&pools->spike_stats, 0, sizeof(pool_stats_t));
    memset(&pools->feature_stats, 0, sizeof(pool_stats_t));
    memset(pools->size_class_stats, 0, sizeof(pools->size_class_stats));
    memset(pools->size_class_counts, 0, sizeof(pools->size_class_counts));

    pools->total_feature_allocs = 0;
    pools->total_alloc_count = 0;
    pools->total_release_count = 0;
    pools->first_alloc_time_ms = get_time_ms();
    pools->last_alloc_time_ms = pools->first_alloc_time_ms;

    nimcp_platform_mutex_unlock(&pools->mutex);
}

bool brain_pools_get_shannon_metrics(brain_pools_t pools, shannon_metrics_t* shannon) {
    if (!pools || !shannon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_pools_get_shannon_metrics: required parameter is NULL (pools, shannon)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    shannon->entropy_bits = calculate_shannon_entropy(
        pools->size_class_counts, POOL_SIZE_COUNT, pools->total_feature_allocs);
    shannon->max_entropy_bits = calculate_max_entropy(POOL_SIZE_COUNT);
    if (shannon->max_entropy_bits > 0.0F) {
        shannon->efficiency = shannon->entropy_bits / shannon->max_entropy_bits;
    } else {
        shannon->efficiency = 0.0F;
    }
    shannon->redundancy = 1.0F - shannon->efficiency;
    memcpy(shannon->size_class_counts, pools->size_class_counts,
           sizeof(pools->size_class_counts));

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool brain_pools_get_queuing_metrics(brain_pools_t pools, queuing_metrics_t* queuing) {
    if (!pools || !queuing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_pools_get_queuing_metrics: required parameter is NULL (pools, queuing)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    uint64_t elapsed_ms = pools->last_alloc_time_ms - pools->first_alloc_time_ms;
    calculate_queuing_metrics(
        pools->total_alloc_count,
        pools->total_release_count,
        elapsed_ms,
        pools->config.safety_factor_k,
        queuing);

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

bool brain_pools_is_performant(brain_pools_t pools) {
    if (!pools) {
        return false;
    }

    brain_pools_metrics_t metrics;
    if (!brain_pools_get_metrics(pools, &metrics)) {
        return false;
    }

    /* Check against targets */
    bool acquire_ok = metrics.avg_acquire_ns < (float)BRAIN_POOL_TARGET_ACQUIRE_NS;
    bool speedup_ok = metrics.speedup_vs_malloc >= (float)BRAIN_POOL_TARGET_SPEEDUP;

    return acquire_ok && speedup_ok;
}

bool brain_pools_get_recommended_config(
    brain_pools_t pools,
    brain_pools_config_t* recommended)
{
    if (!pools || !recommended) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_pools_get_recommended_config: required parameter is NULL (pools, recommended)");
        return false;
    }

    nimcp_platform_mutex_lock(&pools->mutex);

    /* Start with current config */
    *recommended = pools->config;

    /* Get queuing metrics for each pool */
    uint64_t elapsed_ms = pools->last_alloc_time_ms - pools->first_alloc_time_ms;
    if (elapsed_ms == 0) elapsed_ms = 1;

    float elapsed_sec = (float)elapsed_ms / (float)NIMCP_MS_PER_SEC;
    float k = pools->config.safety_factor_k;

    /* Decision pool recommendation */
    float decision_rate = (float)pools->decision_stats.total_acquires / elapsed_sec;
    if (decision_rate > 0.0F) {
        recommended->decision_pool_capacity = (size_t)(decision_rate + k * sqrtf(decision_rate) + 1);
    }

    /* Spike pool recommendation (high traffic) */
    float spike_rate = (float)pools->spike_stats.total_acquires / elapsed_sec;
    if (spike_rate > 0.0F) {
        recommended->spike_pool_capacity = (size_t)(spike_rate + k * sqrtf(spike_rate) + 1);
    }

    /* Size class recommendations based on usage patterns */
    for (int i = 0; i < POOL_SIZE_COUNT; i++) {
        float class_rate = (float)pools->size_class_counts[i] / elapsed_sec;
        if (class_rate > 0.0F) {
            recommended->size_class_capacities[i] =
                (size_t)(class_rate + k * sqrtf(class_rate) + 1);
        }
    }

    nimcp_platform_mutex_unlock(&pools->mutex);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

size_t brain_pools_calculate_memory(const brain_pools_config_t* config) {
    if (!config) return 0;

    size_t total = 0;

    /* Specialized pools */
    total += config->decision_pool_capacity * config->decision_block_size;
    total += config->activation_pool_capacity * config->activation_block_size;
    total += config->spike_pool_capacity * config->spike_block_size;

    /* Size class pools */
    for (int i = 0; i < POOL_SIZE_COUNT; i++) {
        total += config->size_class_capacities[i] * SIZE_CLASS_BYTES[i];
    }

    /* Pool overhead (~10%) */
    total = (size_t)((float)total * 1.1F);

    return total;
}

pool_size_class_t brain_pools_get_size_class(size_t bytes) {
    if (bytes <= BRAIN_POOL_SIZE_TINY) return POOL_SIZE_TINY;
    if (bytes <= BRAIN_POOL_SIZE_SMALL) return POOL_SIZE_SMALL;
    if (bytes <= BRAIN_POOL_SIZE_MEDIUM) return POOL_SIZE_MEDIUM;
    if (bytes <= BRAIN_POOL_SIZE_LARGE) return POOL_SIZE_LARGE;
    return POOL_SIZE_XLARGE;
}

size_t brain_pools_get_class_size(pool_size_class_t size_class) {
    if (size_class >= POOL_SIZE_COUNT) return 0;
    return SIZE_CLASS_BYTES[size_class];
}
