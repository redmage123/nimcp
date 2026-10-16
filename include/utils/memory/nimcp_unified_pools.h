/**
 * @file nimcp_unified_pools.h
 * @brief Unified Memory Pool + COW Integration (Phase 4)
 *
 * WHAT: Production-ready unified memory management combining pools + COW
 * WHY:  Provide adaptive, observable, quota-enforced memory allocation
 * HOW:  Integrate brain_pools, layer_pools, cow_manager with pressure management
 *
 * PHASE: 4 (Unified COW + Pools Integration)
 *
 * MATHEMATICAL FOUNDATIONS:
 * 1. Adaptive Sizing: N_optimal = λ/μ + k√(ΔH/Δt) where ΔH = entropy change rate
 * 2. Pressure Index: P = Σ(used_i/quota_i)^2 / n (normalized pressure across layers)
 * 3. COW Efficiency: η_cow = memory_saved / (memory_saved + copy_overhead)
 * 4. Resource Fairness: Weighted Jain's F = (Σw_i*x_i)^2 / (Σw_i * Σw_i*x_i^2)
 *
 * ARCHITECTURE:
 * ┌────────────────────────────────────────────────────────────────┐
 * │                    UNIFIED POOLS MANAGER                       │
 * │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
 * │  │ Quotas   │  │ Pressure │  │ Adaptive │  │ Observability│  │
 * │  │ Manager  │  │ Monitor  │  │ Sizer    │  │ Metrics      │  │
 * │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘  │
 * │       └──────────┬──┴──────────┬──┘               │          │
 * │                  ▼             ▼                  │          │
 * │  ┌─────────────────────────────────────────────────────────┐ │
 * │  │                   COW COORDINATOR                       │ │
 * │  │  (Pool-aware COW with lazy copy from any pool type)     │ │
 * │  └─────────────────────────────────────────────────────────┘ │
 * │                  │             │             │               │
 * │       ┌──────────┴─────┐  ┌───┴────┐  ┌────┴─────┐         │
 * │       ▼                ▼  ▼        ▼  ▼          ▼         │
 * │  ┌─────────┐    ┌─────────┐    ┌─────────┐   ┌─────────┐   │
 * │  │ Brain   │    │ Layer   │    │ Buffer  │   │ Generic │   │
 * │  │ Pools   │    │ Pools   │    │ Pool    │   │ Pool    │   │
 * │  └─────────┘    └─────────┘    └─────────┘   └─────────┘   │
 * └────────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 * @date 2025-11-24
 * @version 4.0.0
 */

#ifndef NIMCP_UNIFIED_POOLS_H
#define NIMCP_UNIFIED_POOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/memory/nimcp_brain_pools.h"
#include "utils/memory/nimcp_layer_pools.h"
#include "utils/memory/nimcp_cow_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/** Opaque unified pools manager handle */
typedef struct unified_pools* unified_pools_t;

/** Opaque COW handle for unified pools */
typedef struct unified_cow_handle* unified_cow_handle_t;

/** Pool source types */
typedef enum {
    UNIFIED_POOL_BRAIN_DECISION = 0,
    UNIFIED_POOL_BRAIN_ACTIVATION,
    UNIFIED_POOL_BRAIN_SPIKE,
    UNIFIED_POOL_BRAIN_FEATURE,
    UNIFIED_POOL_LAYER_COGNITIVE,
    UNIFIED_POOL_LAYER_MIDDLEWARE,
    UNIFIED_POOL_LAYER_TRAINING,
    UNIFIED_POOL_BUFFER,
    UNIFIED_POOL_GENERIC,
    UNIFIED_POOL_COUNT
} unified_pool_type_t;

/** Memory pressure levels */
typedef enum {
    PRESSURE_NONE = 0,      /* <50% utilization */
    PRESSURE_LOW,           /* 50-70% utilization */
    PRESSURE_MEDIUM,        /* 70-85% utilization */
    PRESSURE_HIGH,          /* 85-95% utilization */
    PRESSURE_CRITICAL       /* >95% utilization */
} pressure_level_t;

/** Quota enforcement modes */
typedef enum {
    QUOTA_MODE_SOFT = 0,    /* Warn but allow over-quota */
    QUOTA_MODE_HARD,        /* Block allocation over quota */
    QUOTA_MODE_ADAPTIVE     /* Borrow from other layers if available */
} quota_mode_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Per-pool quota configuration
 */
typedef struct {
    size_t max_bytes;           /* Maximum bytes for this pool */
    size_t reserved_bytes;      /* Minimum reserved (cannot be borrowed) */
    float priority;             /* Priority weight (0.0-1.0) for borrowing */
    quota_mode_t mode;          /* Enforcement mode */
} pool_quota_t;

/**
 * @brief Adaptive sizing configuration
 */
typedef struct {
    bool enabled;                       /* Enable adaptive sizing */
    float entropy_threshold;            /* Trigger resize if ΔH > threshold */
    float min_utilization;              /* Contract if util < this */
    float max_utilization;              /* Expand if util > this */
    uint32_t sample_window_ms;          /* Sampling window for metrics */
    uint32_t resize_cooldown_ms;        /* Minimum time between resizes */
    float growth_factor;                /* Expansion multiplier (e.g., 1.5) */
    float shrink_factor;                /* Contraction multiplier (e.g., 0.75) */
} adaptive_config_t;

/**
 * @brief Pressure management configuration
 */
typedef struct {
    bool enabled;                       /* Enable pressure management */
    float low_threshold;                /* Threshold for PRESSURE_LOW */
    float medium_threshold;             /* Threshold for PRESSURE_MEDIUM */
    float high_threshold;               /* Threshold for PRESSURE_HIGH */
    float critical_threshold;           /* Threshold for PRESSURE_CRITICAL */
    bool enable_cow_on_pressure;        /* Force COW mode under pressure */
    bool enable_gc_on_pressure;         /* Trigger GC under pressure */
    uint32_t gc_interval_ms;            /* Minimum GC interval */
} pressure_config_t;

/**
 * @brief COW coordinator configuration
 */
typedef struct {
    bool enabled;                       /* Enable unified COW */
    bool lazy_initialization;           /* Lazy template creation */
    bool share_across_pools;            /* Allow cross-pool COW sharing */
    size_t max_shared_templates;        /* Maximum shared templates */
    uint32_t template_ttl_ms;           /* Template time-to-live */
} cow_config_t;

/**
 * @brief Observability configuration
 */
typedef struct {
    bool enable_metrics;                /* Enable metrics collection */
    bool enable_tracing;                /* Enable allocation tracing */
    bool enable_histograms;             /* Enable latency histograms */
    uint32_t metrics_interval_ms;       /* Metrics sampling interval */
    size_t trace_buffer_size;           /* Trace buffer size */
} observability_config_t;

/**
 * @brief Unified pools configuration
 */
typedef struct {
    /* Underlying pool configs */
    brain_pools_config_t brain_config;
    layer_pools_config_t layer_config;

    /* Per-pool quotas */
    pool_quota_t quotas[UNIFIED_POOL_COUNT];
    size_t total_memory_limit;          /* Global memory limit */

    /* Feature configs */
    adaptive_config_t adaptive;
    pressure_config_t pressure;
    cow_config_t cow;
    observability_config_t observability;
} unified_pools_config_t;

//=============================================================================
// Metrics Structures
//=============================================================================

/**
 * @brief Per-pool metrics
 */
typedef struct {
    unified_pool_type_t type;
    size_t allocated_bytes;
    size_t quota_bytes;
    float utilization;
    uint64_t total_acquires;
    uint64_t total_releases;
    uint64_t cow_triggers;
    uint64_t quota_violations;
    float avg_acquire_ns;
    float p99_acquire_ns;
} pool_metrics_t;

/**
 * @brief Pressure metrics
 */
typedef struct {
    pressure_level_t current_level;
    float pressure_index;               /* Normalized pressure (0-1) */
    float pressure_trend;               /* Rate of change */
    uint64_t gc_runs;
    uint64_t cow_forced;
    uint64_t borrowing_events;
    uint64_t quota_rejections;
} pressure_metrics_t;

/**
 * @brief COW efficiency metrics
 */
typedef struct {
    uint64_t total_templates;
    uint64_t active_templates;
    size_t memory_saved_bytes;
    size_t copy_overhead_bytes;
    float cow_efficiency;               /* η_cow */
    uint64_t cow_triggers;
    uint64_t cow_avoided;               /* Reads that avoided copies */
    float avg_refcount;
} cow_metrics_t;

/**
 * @brief Adaptive sizing metrics
 */
typedef struct {
    uint64_t expansions;
    uint64_t contractions;
    float entropy_current;
    float entropy_delta;
    float utilization_trend;
    uint64_t last_resize_ms;
} adaptive_metrics_t;

/**
 * @brief Comprehensive unified metrics
 */
typedef struct {
    /* Aggregate metrics */
    size_t total_allocated;
    size_t total_limit;
    float global_utilization;
    float weighted_fairness;            /* Weighted Jain's index */

    /* Component metrics */
    pool_metrics_t pools[UNIFIED_POOL_COUNT];
    pressure_metrics_t pressure;
    cow_metrics_t cow;
    adaptive_metrics_t adaptive;

    /* Timing */
    uint64_t uptime_ms;
    uint64_t metrics_timestamp_ms;
} unified_metrics_t;

//=============================================================================
// Allocation Result
//=============================================================================

/**
 * @brief Allocation result with metadata
 */
typedef struct {
    void* ptr;                          /* Allocated pointer */
    unified_pool_type_t source;         /* Pool that provided allocation */
    bool is_cow;                        /* True if COW handle */
    bool borrowed;                      /* True if borrowed from another pool */
    size_t actual_size;                 /* Actual allocated size */
} allocation_result_t;

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Pressure callback type
 */
typedef void (*pressure_callback_t)(
    unified_pools_t pools,
    pressure_level_t level,
    float pressure_index,
    void* user_data
);

/**
 * @brief Quota violation callback type
 */
typedef void (*quota_callback_t)(
    unified_pools_t pools,
    unified_pool_type_t pool,
    size_t requested,
    size_t available,
    void* user_data
);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default unified pools configuration
 *
 * WHAT: Provide sensible defaults for unified pools
 * WHY:  Enable quick startup without manual tuning
 * HOW:  Use mathematical models for optimal defaults
 */
unified_pools_config_t unified_pools_default_config(void);

/**
 * @brief Get production-optimized configuration
 *
 * WHAT: Configuration tuned for production workloads
 * WHY:  Balance performance, memory, and observability
 * HOW:  Enable pressure management, adaptive sizing, full metrics
 */
unified_pools_config_t unified_pools_production_config(void);

/**
 * @brief Get development/debug configuration
 *
 * WHAT: Configuration with maximum observability
 * WHY:  Enable debugging and profiling during development
 * HOW:  Enable tracing, histograms, verbose metrics
 */
unified_pools_config_t unified_pools_debug_config(void);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create unified pools manager
 *
 * WHAT: Initialize all underlying pools with unified management
 * WHY:  Provide single entry point for memory management
 * HOW:  Create brain_pools, layer_pools, COW coordinator
 *
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on failure
 */
unified_pools_t unified_pools_create(const unified_pools_config_t* config);

/**
 * @brief Destroy unified pools and release all memory
 *
 * WHAT: Clean shutdown of all pools
 * WHY:  Prevent memory leaks
 * HOW:  Destroy COW templates, pools, metrics
 *
 * @param pools Handle to destroy
 */
void unified_pools_destroy(unified_pools_t pools);

//=============================================================================
// Unified Allocation API
//=============================================================================

/**
 * @brief Acquire memory from best available pool
 *
 * WHAT: Allocate memory with automatic pool selection
 * WHY:  Simplify allocation by hiding pool complexity
 * HOW:  Select pool based on size, pressure, quotas
 *
 * @param pools Unified pools handle
 * @param size Requested size in bytes
 * @param hint Pool type hint (or UNIFIED_POOL_COUNT for auto)
 * @return Allocation result with metadata
 */
allocation_result_t unified_pools_acquire(
    unified_pools_t pools,
    size_t size,
    unified_pool_type_t hint
);

/**
 * @brief Release memory back to appropriate pool
 *
 * WHAT: Return memory to its source pool
 * WHY:  Maintain pool integrity
 * HOW:  Track allocation source, return to correct pool
 *
 * @param pools Unified pools handle
 * @param ptr Pointer to release
 */
void unified_pools_release(unified_pools_t pools, void* ptr);

/**
 * @brief Acquire with COW semantics
 *
 * WHAT: Get COW handle that shares template until write
 * WHY:  Maximize memory sharing for read-heavy workloads
 * HOW:  Create/share template, return COW handle
 *
 * @param pools Unified pools handle
 * @param template_data Template data to share
 * @param size Size of template
 * @param pool_hint Pool for private copies
 * @return COW handle or NULL on failure
 */
unified_cow_handle_t unified_pools_cow_acquire(
    unified_pools_t pools,
    const void* template_data,
    size_t size,
    unified_pool_type_t pool_hint
);

/**
 * @brief Get read-only pointer from COW handle
 *
 * WHAT: Get pointer for reading without triggering copy
 * WHY:  Enable zero-copy reads
 * HOW:  Return shared template pointer
 *
 * @param handle COW handle
 * @return Read-only pointer
 */
const void* unified_pools_cow_read(unified_cow_handle_t handle);

/**
 * @brief Get writable pointer from COW handle
 *
 * WHAT: Get pointer for writing, triggering copy if shared
 * WHY:  Enable lazy copy-on-write
 * HOW:  Trigger copy from pool if currently shared
 *
 * @param handle COW handle
 * @return Writable pointer (may trigger allocation)
 */
void* unified_pools_cow_write(unified_cow_handle_t handle);

/**
 * @brief Release COW handle
 *
 * WHAT: Release COW handle and decrement refcount
 * WHY:  Enable template cleanup when no longer referenced
 * HOW:  Atomic decrement, cleanup on zero
 *
 * @param pools Unified pools handle
 * @param handle COW handle to release
 */
void unified_pools_cow_release(unified_pools_t pools, unified_cow_handle_t handle);

//=============================================================================
// Quota Management API
//=============================================================================

/**
 * @brief Set quota for specific pool
 *
 * WHAT: Update memory quota for a pool
 * WHY:  Enable dynamic resource allocation
 * HOW:  Update quota, rebalance if needed
 *
 * @param pools Unified pools handle
 * @param pool Pool type
 * @param quota New quota configuration
 * @return true if quota set successfully
 */
bool unified_pools_set_quota(
    unified_pools_t pools,
    unified_pool_type_t pool,
    const pool_quota_t* quota
);

/**
 * @brief Get current quota for pool
 *
 * @param pools Unified pools handle
 * @param pool Pool type
 * @param quota Output quota configuration
 * @return true if quota retrieved
 */
bool unified_pools_get_quota(
    unified_pools_t pools,
    unified_pool_type_t pool,
    pool_quota_t* quota
);

/**
 * @brief Check if allocation would exceed quota
 *
 * @param pools Unified pools handle
 * @param pool Pool type
 * @param size Size to check
 * @return true if allocation would fit within quota
 */
bool unified_pools_check_quota(
    unified_pools_t pools,
    unified_pool_type_t pool,
    size_t size
);

//=============================================================================
// Pressure Management API
//=============================================================================

/**
 * @brief Get current pressure level
 *
 * @param pools Unified pools handle
 * @return Current pressure level
 */
pressure_level_t unified_pools_get_pressure_level(unified_pools_t pools);

/**
 * @brief Get detailed pressure metrics
 *
 * @param pools Unified pools handle
 * @param metrics Output pressure metrics
 * @return true if metrics retrieved
 */
bool unified_pools_get_pressure_metrics(
    unified_pools_t pools,
    pressure_metrics_t* metrics
);

/**
 * @brief Register pressure callback
 *
 * @param pools Unified pools handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return true if registered
 */
bool unified_pools_on_pressure(
    unified_pools_t pools,
    pressure_callback_t callback,
    void* user_data
);

/**
 * @brief Manually trigger garbage collection
 *
 * @param pools Unified pools handle
 * @return Bytes reclaimed
 */
size_t unified_pools_gc(unified_pools_t pools);

//=============================================================================
// Adaptive Sizing API
//=============================================================================

/**
 * @brief Trigger adaptive resize check
 *
 * WHAT: Check if pools should resize based on usage
 * WHY:  Optimize memory based on actual patterns
 * HOW:  Evaluate entropy, utilization, trigger resize
 *
 * @param pools Unified pools handle
 * @return Number of pools resized
 */
uint32_t unified_pools_adaptive_resize(unified_pools_t pools);

/**
 * @brief Get adaptive sizing metrics
 *
 * @param pools Unified pools handle
 * @param metrics Output adaptive metrics
 * @return true if metrics retrieved
 */
bool unified_pools_get_adaptive_metrics(
    unified_pools_t pools,
    adaptive_metrics_t* metrics
);

/**
 * @brief Get recommended configuration based on usage
 *
 * @param pools Unified pools handle
 * @param recommended Output recommended config
 * @return true if recommendation generated
 */
bool unified_pools_get_recommended_config(
    unified_pools_t pools,
    unified_pools_config_t* recommended
);

//=============================================================================
// Metrics API
//=============================================================================

/**
 * @brief Get comprehensive unified metrics
 *
 * @param pools Unified pools handle
 * @param metrics Output metrics structure
 * @return true if metrics retrieved
 */
bool unified_pools_get_metrics(
    unified_pools_t pools,
    unified_metrics_t* metrics
);

/**
 * @brief Get metrics for specific pool
 *
 * @param pools Unified pools handle
 * @param pool Pool type
 * @param metrics Output pool metrics
 * @return true if metrics retrieved
 */
bool unified_pools_get_pool_metrics(
    unified_pools_t pools,
    unified_pool_type_t pool,
    pool_metrics_t* metrics
);

/**
 * @brief Get COW efficiency metrics
 *
 * @param pools Unified pools handle
 * @param metrics Output COW metrics
 * @return true if metrics retrieved
 */
bool unified_pools_get_cow_metrics(
    unified_pools_t pools,
    cow_metrics_t* metrics
);

/**
 * @brief Reset all metrics
 *
 * @param pools Unified pools handle
 */
void unified_pools_reset_metrics(unified_pools_t pools);

//=============================================================================
// Underlying Pool Access
//=============================================================================

/**
 * @brief Get underlying brain pools handle
 *
 * @param pools Unified pools handle
 * @return Brain pools handle
 */
brain_pools_t unified_pools_get_brain_pools(unified_pools_t pools);

/**
 * @brief Get underlying layer pools handle
 *
 * @param pools Unified pools handle
 * @return Layer pools handle
 */
layer_pools_t unified_pools_get_layer_pools(unified_pools_t pools);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Calculate total memory usage
 *
 * @param config Configuration
 * @return Estimated memory usage in bytes
 */
size_t unified_pools_calculate_memory(const unified_pools_config_t* config);

/**
 * @brief Get pool type name
 *
 * @param pool Pool type
 * @return Human-readable name
 */
const char* unified_pools_get_pool_name(unified_pool_type_t pool);

/**
 * @brief Get pressure level name
 *
 * @param level Pressure level
 * @return Human-readable name
 */
const char* unified_pools_get_pressure_name(pressure_level_t level);

/**
 * @brief Check if unified pools is healthy
 *
 * @param pools Unified pools handle
 * @return true if all pools healthy
 */
bool unified_pools_is_healthy(unified_pools_t pools);

/**
 * @brief Check if unified pools is performant
 *
 * @param pools Unified pools handle
 * @return true if meeting performance targets
 */
bool unified_pools_is_performant(unified_pools_t pools);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_UNIFIED_POOLS_H */
