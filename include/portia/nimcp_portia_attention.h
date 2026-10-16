/**
 * @file nimcp_portia_attention.h
 * @brief Attention-Based Resource Allocation for Portia Spider System
 *
 * WHAT: Dynamic resource reallocation based on attention and salience
 * WHY:  Portia spiders dynamically allocate neural resources based on task importance
 * HOW:  Fair allocation algorithm with priorities, smooth transitions, and hysteresis
 *
 * BIOLOGICAL INSPIRATION:
 * Portia spiders demonstrate remarkable cognitive flexibility by reallocating
 * neural processing resources based on task salience. When hunting, they can
 * shift resources from routine tasks to complex planning. This module implements
 * a similar attention-based resource management system.
 *
 * ALLOCATION ALGORITHM:
 * 1. Sort targets by (salience * priority)
 * 2. Allocate min_allocation to all targets
 * 3. Distribute remaining budget by salience ratio
 * 4. Respect max_allocation caps
 * 5. Apply hysteresis to prevent oscillation
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────┐
 * │           Portia Attention System                   │
 * │  ┌───────────────┐  ┌───────────────┐              │
 * │  │   Salience    │  │   Priority    │              │
 * │  │   Tracking    │  │   Queue       │              │
 * │  └───────┬───────┘  └───────┬───────┘              │
 * │          │                   │                      │
 * │          └───────┬───────────┘                      │
 * │                  ▼                                  │
 * │         ┌─────────────────┐                         │
 * │         │   Allocation    │                         │
 * │         │   Algorithm     │                         │
 * │         └────────┬────────┘                         │
 * │                  │                                  │
 * │         ┌────────▼─────────┐                        │
 * │         │  Resource Pool   │                        │
 * │         │  (Neurons/Memory)│                        │
 * │         └──────────────────┘                        │
 * └─────────────────────────────────────────────────────┘
 * ```
 *
 * THREAD SAFETY: All functions are thread-safe
 * MEMORY: Uses nimcp_malloc/nimcp_free
 * LOGGING: Uses LOG_DEBUG/LOG_INFO/LOG_WARN/LOG_ERROR
 * SECURITY: All pointers validated with bbb_validate_pointer()
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PORTIA_ATTENTION_H
#define NIMCP_PORTIA_ATTENTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_attention_state_struct* portia_attention_state_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Resource allocation targets
 */
typedef enum {
    ATTENTION_TARGET_NEURONS,        /**< Neural computation resources */
    ATTENTION_TARGET_MEMORY,         /**< Working memory capacity */
    ATTENTION_TARGET_PROCESSING,     /**< Processing cycles */
    ATTENTION_TARGET_SENSORS,        /**< Sensory input bandwidth */
    ATTENTION_TARGET_COMMUNICATION,  /**< Communication bandwidth */
    ATTENTION_TARGET_COUNT
} attention_target_t;

/**
 * @brief Resource allocation events (for bio-async broadcasting)
 */
typedef enum {
    ATTENTION_EVENT_SALIENCE_UPDATED = 0,
    ATTENTION_EVENT_ALLOCATION_CHANGED,
    ATTENTION_EVENT_RESOURCES_REQUESTED,
    ATTENTION_EVENT_RESOURCES_RELEASED,
    ATTENTION_EVENT_PREEMPTION_OCCURRED,
    ATTENTION_EVENT_THRESHOLD_EXCEEDED,
    ATTENTION_EVENT_COUNT
} attention_event_t;

//=============================================================================
// Resource Allocation Structures
//=============================================================================

/**
 * @brief Individual resource allocation entry
 */
typedef struct {
    attention_target_t target;       /**< Resource target type */
    float salience;                  /**< Importance (0.0-1.0) */
    float current_allocation;        /**< Current resource % */
    float requested_allocation;      /**< Desired resource % */
    float min_allocation;            /**< Minimum required */
    float max_allocation;            /**< Maximum allowed */
    uint32_t priority;               /**< Tie-breaker priority */
    uint64_t last_update_ms;         /**< Last update timestamp */
} attention_resource_t;

/**
 * @brief Configuration for attention system
 */
typedef struct {
    float reallocation_threshold;    /**< Min change to trigger (0.0-1.0) */
    float decay_rate_per_second;     /**< Salience decay rate */
    uint32_t update_interval_ms;     /**< How often to reallocate */
    bool enable_preemption;          /**< Can steal from low-salience */
    float preemption_threshold;      /**< Salience diff to preempt */
    float hysteresis_factor;         /**< Prevent oscillation (0.0-1.0) */
    float smoothing_alpha;           /**< Exponential smoothing (0.0-1.0) */
} portia_attention_config_t;

/**
 * @brief Attention system statistics
 */
typedef struct {
    uint64_t salience_updates;       /**< Total salience updates */
    uint64_t reallocations;          /**< Total reallocations performed */
    uint64_t preemptions;            /**< Preemption events */
    uint64_t requests;               /**< Resource requests */
    uint64_t releases;               /**< Resource releases */
    float avg_salience;              /**< Average salience across targets */
    float total_allocated;           /**< Total resources allocated */
    uint64_t last_reallocation_ms;   /**< Last reallocation timestamp */
} portia_attention_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize attention system
 *
 * WHAT: Create and configure attention-based resource allocator
 * WHY:  Set up resource management before use
 * HOW:  Allocates structures, initializes resources to defaults
 *
 * @param config Configuration (NULL for defaults)
 * @param resource_count Number of resource targets
 * @param total_budget Total available resources (normalized 0-1)
 * @return Attention state handle or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 * MEMORY: Uses nimcp_calloc
 * SECURITY: Validates all parameters
 *
 * EXAMPLE:
 * ```c
 * portia_attention_config_t config = {
 *     .reallocation_threshold = 0.05f,
 *     .decay_rate_per_second = 0.1f,
 *     .update_interval_ms = 100,
 *     .enable_preemption = true,
 *     .preemption_threshold = 0.3f,
 *     .hysteresis_factor = 0.2f,
 *     .smoothing_alpha = 0.3f
 * };
 * portia_attention_state_t state = portia_attention_init(&config, 5, 1.0f);
 * ```
 */
NIMCP_EXPORT portia_attention_state_t portia_attention_init(
    const portia_attention_config_t* config,
    uint32_t resource_count,
    float total_budget
);

/**
 * @brief Destroy attention system
 *
 * WHAT: Clean up and free attention system resources
 * WHY:  Prevent memory leaks
 * HOW:  Frees all allocated memory, logs final statistics
 *
 * @param state Attention state handle
 *
 * THREAD SAFETY: Not thread-safe (caller must ensure exclusive access)
 * MEMORY: Uses nimcp_free
 */
NIMCP_EXPORT void portia_attention_destroy(portia_attention_state_t state);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
NIMCP_EXPORT portia_attention_config_t portia_attention_default_config(void);

//=============================================================================
// Salience Management
//=============================================================================

/**
 * @brief Update target salience (importance)
 *
 * WHAT: Update the salience value for a resource target
 * WHY:  Reflect changing task priorities
 * HOW:  Updates salience, broadcasts bio-async event
 *
 * @param state Attention state handle
 * @param target Target to update
 * @param salience New salience value (0.0-1.0)
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 * SECURITY: Validates state and salience range
 * BIO-ASYNC: Broadcasts ATTENTION_EVENT_SALIENCE_UPDATED
 */
NIMCP_EXPORT int portia_attention_update_salience(
    portia_attention_state_t state,
    attention_target_t target,
    float salience
);

/**
 * @brief Apply time-based salience decay
 *
 * WHAT: Decay salience values over time (like forgetting)
 * WHY:  Prevent stale priorities from dominating
 * HOW:  Exponential decay based on elapsed time
 *
 * @param state Attention state handle
 * @param current_time_ms Current time in milliseconds
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(n) where n = number of resources
 */
NIMCP_EXPORT int portia_attention_decay(
    portia_attention_state_t state,
    uint64_t current_time_ms
);

/**
 * @brief Get current salience for target
 *
 * @param state Attention state handle
 * @param target Target to query
 * @return Salience value (0.0-1.0) or -1.0 on error
 */
NIMCP_EXPORT float portia_attention_get_salience(
    portia_attention_state_t state,
    attention_target_t target
);

//=============================================================================
// Resource Allocation
//=============================================================================

/**
 * @brief Reallocate resources based on salience
 *
 * WHAT: Redistribute resources according to current salience values
 * WHY:  Adapt resource usage to changing priorities
 * HOW:  Fair allocation algorithm with priorities and constraints
 *
 * ALGORITHM:
 * 1. Sort targets by (salience * priority)
 * 2. Allocate min_allocation to all targets
 * 3. Distribute remaining budget by salience ratio
 * 4. Respect max_allocation caps
 * 5. Apply hysteresis to prevent oscillation
 * 6. Smooth transitions with exponential averaging
 *
 * @param state Attention state handle
 * @param force_reallocation Force even if below threshold
 * @return 0 on success, negative on error
 *
 * THREAD SAFETY: Thread-safe
 * COMPLEXITY: O(n log n) where n = number of resources
 * BIO-ASYNC: Broadcasts ATTENTION_EVENT_ALLOCATION_CHANGED
 * SECURITY: Validates all calculations
 */
NIMCP_EXPORT int portia_attention_reallocate(
    portia_attention_state_t state,
    bool force_reallocation
);

/**
 * @brief Request more resources for target
 *
 * WHAT: Request additional resource allocation
 * WHY:  Handle dynamic resource needs
 * HOW:  Updates requested_allocation, triggers reallocation check
 *
 * @param state Attention state handle
 * @param target Target requesting resources
 * @param amount Requested amount (0.0-1.0)
 * @return 0 on success, negative on error
 *
 * BIO-ASYNC: Broadcasts ATTENTION_EVENT_RESOURCES_REQUESTED
 */
NIMCP_EXPORT int portia_attention_request(
    portia_attention_state_t state,
    attention_target_t target,
    float amount
);

/**
 * @brief Release unused resources
 *
 * WHAT: Return unused resources to the pool
 * WHY:  Make resources available for reallocation
 * HOW:  Updates current_allocation, triggers reallocation
 *
 * @param state Attention state handle
 * @param target Target releasing resources
 * @param amount Amount to release (0.0-1.0)
 * @return 0 on success, negative on error
 *
 * BIO-ASYNC: Broadcasts ATTENTION_EVENT_RESOURCES_RELEASED
 */
NIMCP_EXPORT int portia_attention_release(
    portia_attention_state_t state,
    attention_target_t target,
    float amount
);

/**
 * @brief Get current allocation for target
 *
 * @param state Attention state handle
 * @param target Target to query
 * @return Current allocation (0.0-1.0) or -1.0 on error
 */
NIMCP_EXPORT float portia_attention_get_allocation(
    portia_attention_state_t state,
    attention_target_t target
);

/**
 * @brief Get all resource allocations
 *
 * WHAT: Retrieve complete allocation state
 * WHY:  Enable monitoring and debugging
 * HOW:  Copies resource array
 *
 * @param state Attention state handle
 * @param resources Output buffer for resources
 * @param max_count Maximum resources to copy
 * @return Number of resources copied, or negative on error
 */
NIMCP_EXPORT int portia_attention_get_all_allocations(
    portia_attention_state_t state,
    attention_resource_t* resources,
    uint32_t max_count
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get attention system statistics
 *
 * @param state Attention state handle
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int portia_attention_get_stats(
    portia_attention_state_t state,
    portia_attention_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param state Attention state handle
 */
NIMCP_EXPORT void portia_attention_reset_stats(portia_attention_state_t state);

/**
 * @brief Check if reallocation is needed
 *
 * WHAT: Determine if resource reallocation should occur
 * WHY:  Avoid unnecessary reallocations
 * HOW:  Checks time since last reallocation and allocation changes
 *
 * @param state Attention state handle
 * @param current_time_ms Current time in milliseconds
 * @return true if reallocation is needed
 */
NIMCP_EXPORT bool portia_attention_needs_reallocation(
    portia_attention_state_t state,
    uint64_t current_time_ms
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get target name string
 *
 * @param target Target type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_attention_target_name(attention_target_t target);

/**
 * @brief Get event name string
 *
 * @param event Event type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_attention_event_name(attention_event_t event);

/**
 * @brief Print allocation state (for debugging)
 *
 * @param state Attention state handle
 */
NIMCP_EXPORT void portia_attention_print_state(portia_attention_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_ATTENTION_H */
