/**
 * @file nimcp_portia_degradation.h
 * @brief Graceful Degradation Profiles for Portia Spider System
 *
 * Implements progressive feature reduction when resources become constrained,
 * allowing the system to maintain core functionality under stress.
 */

#ifndef NIMCP_PORTIA_DEGRADATION_H
#define NIMCP_PORTIA_DEGRADATION_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Degradation levels - progressive severity
 */
typedef enum {
    DEGRADATION_LEVEL_NONE = 0,      /**< Normal operation, all features enabled */
    DEGRADATION_LEVEL_MINOR = 1,     /**< Reduce non-essential features */
    DEGRADATION_LEVEL_MODERATE = 2,  /**< Disable some cognitive modules */
    DEGRADATION_LEVEL_SEVERE = 3,    /**< Core functions only */
    DEGRADATION_LEVEL_CRITICAL = 4,  /**< Survival mode */
    DEGRADATION_LEVEL_COUNT
} degradation_level_t;

/**
 * Feature definition for degradation management
 */
typedef struct {
    uint32_t feature_id;              /**< Unique feature identifier */
    const char* name;                 /**< Human-readable name */
    degradation_level_t disable_at;   /**< Level when this feature is disabled */
    float resource_cost;              /**< Relative resource cost (0.0-1.0) */
    bool is_core;                     /**< Cannot be disabled */
    bool currently_enabled;           /**< Current state */
} degradation_feature_t;

/**
 * Current degradation state
 */
typedef struct {
    degradation_feature_t* features;  /**< Array of features */
    uint32_t feature_count;           /**< Number of features */
    uint32_t feature_capacity;        /**< Allocated capacity */
    degradation_level_t current_level; /**< Current degradation level */
    degradation_level_t target_level; /**< Target level (for transitions) */
    uint32_t active_features;         /**< Count of enabled features */
    float resource_usage;             /**< Current usage % (0.0-100.0) */
    uint64_t last_change_time_ms;     /**< Last level change timestamp */
    nimcp_mutex_t lock;             /**< Thread safety */
} degradation_state_t;

/**
 * Configuration for degradation behavior (internal)
 */
typedef struct {
    float level_thresholds[DEGRADATION_LEVEL_COUNT];  /**< Resource % triggers */
    uint32_t hysteresis_ms;           /**< Prevent rapid changes (ms) */
    bool enable_auto_degrade;         /**< Automatic degradation */
    bool enable_auto_restore;         /**< Automatic restoration */
    float restore_threshold;          /**< % below trigger to restore */
} degradation_internal_config_t;

/**
 * Feature ID definitions
 */
#define FEATURE_PLASTICITY      0x0001  /**< Synaptic plasticity */
#define FEATURE_LEARNING        0x0002  /**< Learning mechanisms */
#define FEATURE_EMOTIONS        0x0004  /**< Emotional processing */
#define FEATURE_PLANNING        0x0008  /**< Future planning */
#define FEATURE_MEMORY_LONG     0x0010  /**< Long-term memory */
#define FEATURE_MEMORY_WORKING  0x0020  /**< Working memory */
#define FEATURE_SENSORS_FULL    0x0040  /**< Full sensor suite */
#define FEATURE_COMMUNICATION   0x0080  /**< Swarm communication */
#define FEATURE_LOGGING_VERBOSE 0x0100  /**< Verbose logging */
#define FEATURE_METRICS         0x0200  /**< Metrics collection */

/**
 * Degradation event types
 */
typedef enum {
    DEGRADATION_EVENT_LEVEL_CHANGE,   /**< Degradation level changed */
    DEGRADATION_EVENT_FEATURE_DISABLED, /**< Feature disabled */
    DEGRADATION_EVENT_FEATURE_ENABLED,  /**< Feature enabled */
    DEGRADATION_EVENT_RESOURCE_WARNING  /**< Resource threshold warning */
} degradation_event_type_t;

/**
 * Degradation event data
 */
typedef struct {
    degradation_event_type_t type;
    degradation_level_t old_level;
    degradation_level_t new_level;
    uint32_t feature_id;
    float resource_usage;
    const char* reason;
} degradation_event_t;

/**
 * Initialize degradation system
 *
 * @param config Configuration parameters
 * @return Degradation state or NULL on failure
 */
degradation_state_t* portia_degradation_init(
    const degradation_internal_config_t* config
);

/**
 * Cleanup degradation system
 *
 * @param state Degradation state to cleanup
 */
void portia_degradation_cleanup(degradation_state_t* state);

/**
 * Evaluate current resource usage and adjust degradation level if needed
 *
 * @param state Degradation state
 * @param resource_usage Current resource usage % (0.0-100.0)
 * @param bio_ctx Bio-async context for events (currently unused, pass NULL)
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_evaluate(
    degradation_state_t* state,
    float resource_usage,
    void* bio_ctx
);

/**
 * Force degradation to specific level
 *
 * @param state Degradation state
 * @param level Target degradation level
 * @param bio_ctx Bio-async context for events (currently unused, pass NULL)
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_set_level(
    degradation_state_t* state,
    degradation_level_t level,
    void* bio_ctx
);

/**
 * Disable a specific feature
 *
 * @param state Degradation state
 * @param feature_id Feature to disable
 * @param bio_ctx Bio-async context for events (currently unused, pass NULL)
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_disable_feature(
    degradation_state_t* state,
    uint32_t feature_id,
    void* bio_ctx
);

/**
 * Enable a specific feature
 *
 * @param state Degradation state
 * @param feature_id Feature to enable
 * @param bio_ctx Bio-async context for events (currently unused, pass NULL)
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_enable_feature(
    degradation_state_t* state,
    uint32_t feature_id,
    void* bio_ctx
);

/**
 * Get current degradation state (read-only)
 *
 * @param state Degradation state
 * @param level Output: current level
 * @param active_features Output: number of active features
 * @param resource_usage Output: current resource usage
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_get_state(
    const degradation_state_t* state,
    degradation_level_t* level,
    uint32_t* active_features,
    float* resource_usage
);

/**
 * Register a new feature for degradation management
 *
 * @param state Degradation state
 * @param feature Feature definition
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_register_feature(
    degradation_state_t* state,
    const degradation_feature_t* feature
);

/**
 * Get the ordered degradation chain (sorted by disable_at level)
 *
 * @param state Degradation state
 * @param chain Output array (must be pre-allocated)
 * @param chain_size Size of chain array
 * @param actual_count Output: actual number of features
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_get_chain(
    const degradation_state_t* state,
    degradation_feature_t* chain,
    uint32_t chain_size,
    uint32_t* actual_count
);

/**
 * Check if a specific feature is currently enabled
 *
 * @param state Degradation state
 * @param feature_id Feature to check
 * @param is_enabled Output: feature state
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_is_feature_enabled(
    const degradation_state_t* state,
    uint32_t feature_id,
    bool* is_enabled
);

/**
 * Get recommended features to disable at a given level
 *
 * @param state Degradation state
 * @param level Target level
 * @param features Output array
 * @param max_features Size of output array
 * @param actual_count Output: actual count
 * @return NIMCP_OK or error code
 */
nimcp_result_t portia_degradation_get_features_for_level(
    const degradation_state_t* state,
    degradation_level_t level,
    uint32_t* features,
    uint32_t max_features,
    uint32_t* actual_count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_DEGRADATION_H */
