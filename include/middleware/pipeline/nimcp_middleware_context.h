//=============================================================================
// nimcp_middleware_context.h - Shared Middleware Execution Context
//=============================================================================

#ifndef NIMCP_MIDDLEWARE_CONTEXT_H
#define NIMCP_MIDDLEWARE_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_middleware_context.h
 * @brief Shared context for middleware pipeline execution
 *
 * WHAT: Shared state passed between pipeline stages
 * WHY:  Enable data sharing without tight coupling
 * HOW:  Context object with brain ref, cache, event history
 */

// Forward declarations
typedef struct brain_struct* brain_t;

//=============================================================================
// Context Structure
//=============================================================================

/**
 * @brief Middleware execution context
 *
 * WHAT: Shared state for entire pipeline execution
 * WHY:  Pass data between stages efficiently
 * HOW:  Reference to brain + cached computations + event history
 */
typedef struct {
    // Brain reference
    brain_t brain;                  /**< Brain being processed */

    // Current state
    uint64_t timestamp_us;          /**< Current timestamp */
    uint32_t* active_neurons;       /**< Currently active neurons */
    uint32_t num_active_neurons;    /**< Count of active neurons */

    // Feature cache (avoid recomputation)
    float* cached_features;         /**< Extracted features */
    uint32_t num_cached_features;   /**< Feature count */
    uint32_t cached_features_capacity; /**< Allocated capacity of cached_features */
    bool features_valid;            /**< Cache valid? */

    // Pattern detection cache
    uint32_t* detected_patterns;    /**< Detected pattern IDs */
    float* pattern_confidences;     /**< Pattern match confidences */
    uint32_t num_detected_patterns; /**< Pattern count */
    uint32_t pattern_capacity;      /**< Allocated capacity of pattern arrays */

    // Recent events (for debugging/replay)
    brain_event_t* recent_events;   /**< Circular buffer of recent events */
    uint32_t recent_event_capacity; /**< Buffer capacity */
    uint32_t recent_event_count;    /**< Current event count */
    uint32_t recent_event_head;     /**< Write position */

    // Performance profiling
    uint64_t* stage_timings_us;     /**< Time spent in each stage */
    uint32_t num_stages;            /**< Number of pipeline stages */

    // Custom data (extensibility)
    void* user_data;                /**< User-defined data */

} middleware_context_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create middleware context
 *
 * @param brain Brain reference
 * @param max_features Max features to cache
 * @param max_patterns Max patterns to cache
 * @param event_history_size Size of event history buffer
 * @param num_stages Number of pipeline stages
 * @return Context or NULL on error
 */
middleware_context_t* middleware_context_create(brain_t brain,
                                                 uint32_t max_features,
                                                 uint32_t max_patterns,
                                                 uint32_t event_history_size,
                                                 uint32_t num_stages);

/**
 * @brief Destroy middleware context
 */
void middleware_context_destroy(middleware_context_t* context);

//=============================================================================
// Context Operations
//=============================================================================

/**
 * @brief Update active neurons
 *
 * @param context Context
 * @param neurons Array of active neuron IDs
 * @param count Number of active neurons
 */
void middleware_context_set_active_neurons(middleware_context_t* context,
                                           uint32_t* neurons, uint32_t count);

/**
 * @brief Cache extracted features
 */
void middleware_context_cache_features(middleware_context_t* context,
                                       float* features, uint32_t count);

/**
 * @brief Get cached features
 */
bool middleware_context_get_cached_features(middleware_context_t* context,
                                            float** features, uint32_t* count);

/**
 * @brief Invalidate feature cache
 */
void middleware_context_invalidate_cache(middleware_context_t* context);

/**
 * @brief Add event to history
 */
void middleware_context_add_event(middleware_context_t* context, const brain_event_t* event);

/**
 * @brief Get recent events
 */
uint32_t middleware_context_get_recent_events(middleware_context_t* context,
                                              brain_event_t** events);

/**
 * @brief Record stage timing
 */
void middleware_context_record_stage_time(middleware_context_t* context,
                                          uint32_t stage_index, uint64_t time_us);

/**
 * @brief Get stage timings
 */
bool middleware_context_get_stage_timings(middleware_context_t* context,
                                          uint64_t** timings, uint32_t* count);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIDDLEWARE_CONTEXT_H
