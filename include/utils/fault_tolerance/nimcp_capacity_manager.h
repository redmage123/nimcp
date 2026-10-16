/**
 * @file nimcp_capacity_manager.h
 * @brief Dynamic capacity management for NIMCP memory modules
 * @date 2026-01-18
 *
 * Provides generic capacity management with auto-expansion, shrinking,
 * and health agent integration. Replaces static capacity macros with
 * configurable, runtime-adjustable capacity limits.
 *
 * Part of Phase 5.8: Dynamic Capacity Management
 */

#ifndef NIMCP_CAPACITY_MANAGER_H
#define NIMCP_CAPACITY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <atomic>
extern "C" {
#endif

#ifndef __cplusplus
#include <stdatomic.h>
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct nimcp_health_agent;

/* ============================================================================
 * Capacity Configuration
 * ============================================================================ */

/**
 * @brief Capacity thresholds for monitoring
 */
typedef enum {
    CAPACITY_LEVEL_NORMAL = 0,     /**< Below 75% - normal operation */
    CAPACITY_LEVEL_ELEVATED,       /**< 75-89% - elevated pressure */
    CAPACITY_LEVEL_WARNING,        /**< 90-99% - warning level */
    CAPACITY_LEVEL_CRITICAL,       /**< 100% - at capacity */
    CAPACITY_LEVEL_EXCEEDED        /**< Over capacity (error state) */
} capacity_level_t;

/**
 * @brief Generic capacity configuration
 *
 * Embed this in module-specific configuration structs to provide
 * standardized capacity management.
 */
typedef struct {
    uint32_t initial_capacity;      /**< Starting size */
    uint32_t max_capacity;          /**< Hard limit (0 = unlimited) */
    float growth_factor;            /**< Expansion multiplier (default: 2.0) */
    float shrink_threshold;         /**< Utilization below which to shrink (0.25) */
    float warning_threshold;        /**< Utilization for warning (default: 0.9) */
    float elevated_threshold;       /**< Utilization for elevated (default: 0.75) */
    bool enable_auto_expand;        /**< Auto-expand on pressure (default: true) */
    bool enable_auto_shrink;        /**< Auto-shrink when idle (default: false) */
    bool enable_immune_cleanup;     /**< Trigger cleanup before expand (default: true) */
    bool enable_trend_analysis;     /**< Track growth rate for prediction (default: true) */
} capacity_config_t;

/**
 * @brief Initialize capacity config with defaults
 */
void capacity_config_default(capacity_config_t* config);

/* ============================================================================
 * Capacity Statistics
 * ============================================================================ */

/**
 * @brief Capacity statistics for monitoring and reporting
 */
typedef struct {
    /* Current state */
    uint32_t current_count;         /**< Current number of items */
    uint32_t capacity;              /**< Current capacity */
    float utilization;              /**< current_count / capacity (0.0-1.0+) */
    capacity_level_t level;         /**< Current pressure level */

    /* Historical */
    uint32_t expansions;            /**< Number of expansions performed */
    uint32_t shrinks;               /**< Number of shrinks performed */
    uint32_t cleanup_triggers;      /**< Number of cleanup triggers */
    uint32_t failed_allocations;    /**< Number of failed allocation attempts */
    float peak_utilization;         /**< Highest utilization seen */
    uint32_t peak_count;            /**< Highest count seen */

    /* Trend analysis */
    float growth_rate_per_sec;      /**< Items per second growth rate */
    float time_to_capacity_sec;     /**< Estimated seconds until at capacity */
    bool growth_trend_valid;        /**< Whether trend analysis is valid */

    /* Timing */
    uint64_t last_expansion_time;   /**< Timestamp of last expansion */
    uint64_t last_check_time;       /**< Timestamp of last check */
} capacity_stats_t;

/* ============================================================================
 * Capacity Manager Callbacks
 * ============================================================================ */

/**
 * @brief Callback for expanding capacity
 * @param module Opaque pointer to the module
 * @param new_capacity Target capacity after expansion
 * @return 0 on success, -1 on failure
 */
typedef int (*capacity_expand_callback_t)(void* module, uint32_t new_capacity);

/**
 * @brief Callback for shrinking capacity
 * @param module Opaque pointer to the module
 * @param new_capacity Target capacity after shrinking
 * @return 0 on success, -1 on failure
 */
typedef int (*capacity_shrink_callback_t)(void* module, uint32_t new_capacity);

/**
 * @brief Callback for cleanup/garbage collection
 * @param module Opaque pointer to the module
 * @param target_free Target number of slots to free
 * @return Number of slots actually freed, or -1 on error
 */
typedef int (*capacity_cleanup_callback_t)(void* module, uint32_t target_free);

/* ============================================================================
 * Capacity Manager Structure
 * ============================================================================ */

/**
 * @brief Capacity manager structure
 *
 * Embed or allocate this for each module that needs capacity management.
 * Thread-safe via atomic operations for count updates.
 */
typedef struct capacity_manager {
    /* Configuration */
    capacity_config_t config;

#ifdef __cplusplus
    /* Current state (atomic for thread-safety) */
    std::atomic<uint32_t> current_count;
    std::atomic<uint32_t> capacity;

    /* Statistics */
    std::atomic<uint32_t> expansions;
    std::atomic<uint32_t> shrinks;
    std::atomic<uint32_t> cleanup_triggers;
    std::atomic<uint32_t> failed_allocations;
    std::atomic<float> peak_utilization;
    std::atomic<uint32_t> peak_count;
#else
    /* Current state (atomic for thread-safety) */
    _Atomic uint32_t current_count;
    _Atomic uint32_t capacity;

    /* Statistics */
    _Atomic uint32_t expansions;
    _Atomic uint32_t shrinks;
    _Atomic uint32_t cleanup_triggers;
    _Atomic uint32_t failed_allocations;
    _Atomic float peak_utilization;
    _Atomic uint32_t peak_count;
#endif

    /* Trend tracking */
    uint32_t trend_samples[8];      /**< Circular buffer of count samples */
    uint64_t trend_times[8];        /**< Timestamps for samples */
    uint8_t trend_index;            /**< Current index in circular buffer */
    bool trend_filled;              /**< Whether buffer has wrapped */

    /* Callbacks */
    capacity_expand_callback_t expand_callback;
    capacity_shrink_callback_t shrink_callback;
    capacity_cleanup_callback_t cleanup_callback;
    void* module;                   /**< Opaque pointer to module */

    /* Identification */
    char module_name[64];           /**< Human-readable module name */

#ifdef __cplusplus
    /* Timing */
    std::atomic<uint64_t> last_expansion_time;
    std::atomic<uint64_t> last_check_time;
#else
    /* Timing */
    _Atomic uint64_t last_expansion_time;
    _Atomic uint64_t last_check_time;
#endif

    /* Magic number for validation */
    uint32_t magic;
} capacity_manager_t;

#define CAPACITY_MANAGER_MAGIC 0xCA9AC1A7  /* "CAPACIT" in hex-ish */

/* ============================================================================
 * Capacity Manager API
 * ============================================================================ */

/**
 * @brief Create and initialize a capacity manager
 * @param cm Pointer to receive allocated manager
 * @param config Configuration (NULL for defaults)
 * @param module_name Human-readable name for logging
 * @return 0 on success, -1 on error
 */
int capacity_manager_create(capacity_manager_t** cm,
                            const capacity_config_t* config,
                            const char* module_name);

/**
 * @brief Initialize a capacity manager in-place
 * @param cm Pointer to pre-allocated manager structure
 * @param config Configuration (NULL for defaults)
 * @param module_name Human-readable name for logging
 * @return 0 on success, -1 on error
 */
int capacity_manager_init(capacity_manager_t* cm,
                          const capacity_config_t* config,
                          const char* module_name);

/**
 * @brief Destroy a capacity manager
 * @param cm Manager to destroy (created via capacity_manager_create)
 */
void capacity_manager_destroy(capacity_manager_t* cm);

/**
 * @brief Set callbacks for capacity operations
 * @param cm The capacity manager
 * @param module Opaque pointer passed to callbacks
 * @param expand Callback for expansion (optional)
 * @param shrink Callback for shrinking (optional)
 * @param cleanup Callback for cleanup (optional)
 * @return 0 on success, -1 on error
 */
int capacity_manager_set_callbacks(capacity_manager_t* cm,
                                   void* module,
                                   capacity_expand_callback_t expand,
                                   capacity_shrink_callback_t shrink,
                                   capacity_cleanup_callback_t cleanup);

/**
 * @brief Request a slot from the capacity manager
 *
 * Increments current_count. If at capacity, may trigger expansion or cleanup.
 *
 * @param cm The capacity manager
 * @return 0 on success (slot available), -1 if at hard limit
 */
int capacity_manager_request_slot(capacity_manager_t* cm);

/**
 * @brief Release a slot back to the capacity manager
 *
 * Decrements current_count. May trigger shrinking if enabled and
 * utilization drops below threshold.
 *
 * @param cm The capacity manager
 * @return 0 on success, -1 on error (e.g., count already 0)
 */
int capacity_manager_release_slot(capacity_manager_t* cm);

/**
 * @brief Check capacity pressure and take action if needed
 *
 * Call this periodically to trigger expansion/cleanup based on pressure.
 *
 * @param cm The capacity manager
 * @param current_count Current count (used if external tracking)
 * @return Current capacity level
 */
capacity_level_t capacity_manager_check(capacity_manager_t* cm, uint32_t current_count);

/**
 * @brief Update current count externally
 *
 * Use when the module tracks count externally rather than via request/release.
 *
 * @param cm The capacity manager
 * @param count New current count
 */
void capacity_manager_set_count(capacity_manager_t* cm, uint32_t count);

/**
 * @brief Get current utilization
 * @param cm The capacity manager
 * @return Utilization (0.0 to 1.0+)
 */
float capacity_manager_get_utilization(const capacity_manager_t* cm);

/**
 * @brief Get current pressure level
 * @param cm The capacity manager
 * @return Current capacity level
 */
capacity_level_t capacity_manager_get_level(const capacity_manager_t* cm);

/**
 * @brief Check if at or over capacity
 * @param cm The capacity manager
 * @return true if at or over capacity
 */
bool capacity_manager_is_full(const capacity_manager_t* cm);

/**
 * @brief Manually trigger capacity expansion
 * @param cm The capacity manager
 * @return 0 on success, -1 if at max or expansion failed
 */
int capacity_manager_trigger_expand(capacity_manager_t* cm);

/**
 * @brief Manually trigger capacity shrink
 * @param cm The capacity manager
 * @return 0 on success, -1 if at minimum or shrink failed
 */
int capacity_manager_trigger_shrink(capacity_manager_t* cm);

/**
 * @brief Manually trigger cleanup
 * @param cm The capacity manager
 * @param target_free Target number of slots to free
 * @return Number of slots freed, or -1 on error
 */
int capacity_manager_trigger_cleanup(capacity_manager_t* cm, uint32_t target_free);

/**
 * @brief Get capacity statistics
 * @param cm The capacity manager
 * @param stats Pointer to receive statistics
 */
void capacity_manager_get_stats(const capacity_manager_t* cm, capacity_stats_t* stats);

/**
 * @brief Get time-to-capacity prediction
 *
 * Based on trend analysis, estimates seconds until capacity is reached.
 *
 * @param cm The capacity manager
 * @return Estimated seconds until at capacity, or -1.0 if not growing
 */
float capacity_manager_time_to_capacity(const capacity_manager_t* cm);

/**
 * @brief Reset statistics counters
 * @param cm The capacity manager
 */
void capacity_manager_reset_stats(capacity_manager_t* cm);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/*
 * Health Agent Integration Functions
 *
 * These functions are declared in nimcp_health_agent.h:
 * - nimcp_health_agent_register_capacity_manager()
 * - nimcp_health_agent_unregister_capacity_manager()
 * - nimcp_health_agent_get_capacity_metrics()
 * - nimcp_health_agent_capacity_needs_attention()
 * - capacity_health_metrics_t
 *
 * Include nimcp_health_agent.h for the full API.
 */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CAPACITY_MANAGER_H */
