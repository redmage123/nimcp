/**
 * @file nimcp_graceful_degradation.h
 * @brief Graceful Degradation Profiles for Fault Tolerance
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Service level tiers, feature prioritization, resource budgeting
 * WHY:  Maintain core functionality when resources are constrained
 * HOW:  Define service tiers, prioritize features, shed load progressively
 *
 * BIOLOGICAL BASIS:
 * - Autonomic regulation (reduce heart rate during rest)
 * - Energy conservation (hibernation, torpor states)
 * - Triage system (prioritize critical functions under stress)
 * - Neural plasticity (brain reassigns resources after damage)
 *
 * DEGRADATION TIERS:
 * Tier 0 (Full):      All features enabled, full quality
 * Tier 1 (Standard):  Non-essential features disabled
 * Tier 2 (Reduced):   Quality reduced, batch sizes decreased
 * Tier 3 (Minimal):   Core functions only, maximum efficiency
 * Tier 4 (Emergency): Survival mode, data preservation focus
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GRACEFUL_DEGRADATION_H
#define NIMCP_GRACEFUL_DEGRADATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define GD_MAX_TIERS 5                      /**< Maximum degradation tiers */
#define GD_MAX_FEATURES 64                  /**< Maximum feature definitions */
#define GD_MAX_RESOURCES 16                 /**< Maximum resource types */
#define GD_MAX_PROFILES 8                   /**< Maximum degradation profiles */
#define GD_MAX_THRESHOLDS 16                /**< Max thresholds per tier */
#define GD_HYSTERESIS_PERCENT 5.0f          /**< Hysteresis for tier changes */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Degradation tiers
 */
typedef enum {
    GD_TIER_FULL = 0,       /**< Full functionality */
    GD_TIER_STANDARD,       /**< Non-essential disabled */
    GD_TIER_REDUCED,        /**< Quality reduced */
    GD_TIER_MINIMAL,        /**< Core functions only */
    GD_TIER_EMERGENCY       /**< Survival mode */
} gd_tier_t;

/**
 * @brief Feature priority levels
 */
typedef enum {
    GD_PRIORITY_CRITICAL = 0,   /**< Must always run */
    GD_PRIORITY_HIGH,           /**< Run except emergency */
    GD_PRIORITY_MEDIUM,         /**< Run in standard+ */
    GD_PRIORITY_LOW,            /**< Run only in full */
    GD_PRIORITY_OPTIONAL        /**< Luxury feature */
} gd_priority_t;

/**
 * @brief Resource types
 */
typedef enum {
    GD_RESOURCE_CPU = 0,        /**< CPU utilization */
    GD_RESOURCE_MEMORY,         /**< Memory usage */
    GD_RESOURCE_GPU,            /**< GPU utilization */
    GD_RESOURCE_NETWORK,        /**< Network bandwidth */
    GD_RESOURCE_POWER,          /**< Power consumption */
    GD_RESOURCE_LATENCY,        /**< Response latency */
    GD_RESOURCE_THROUGHPUT,     /**< Processing throughput */
    GD_RESOURCE_STORAGE,        /**< Storage space */
    GD_RESOURCE_COUNT
} gd_resource_t;

/**
 * @brief Degradation actions
 */
typedef enum {
    GD_ACTION_DISABLE_FEATURE = 0,  /**< Disable feature */
    GD_ACTION_REDUCE_QUALITY,       /**< Reduce quality level */
    GD_ACTION_REDUCE_BATCH,         /**< Reduce batch size */
    GD_ACTION_INCREASE_INTERVAL,    /**< Increase processing interval */
    GD_ACTION_SHED_LOAD,            /**< Drop requests */
    GD_ACTION_CACHE_MORE,           /**< Increase caching */
    GD_ACTION_CHECKPOINT,           /**< Trigger checkpoint */
    GD_ACTION_NOTIFY               /**< Notify operators */
} gd_action_t;

/**
 * @brief Degradation trigger conditions
 */
typedef enum {
    GD_TRIGGER_RESOURCE = 0,    /**< Resource threshold */
    GD_TRIGGER_ERROR_RATE,      /**< Error rate threshold */
    GD_TRIGGER_LATENCY,         /**< Latency threshold */
    GD_TRIGGER_MANUAL,          /**< Manual trigger */
    GD_TRIGGER_CASCADING        /**< Cascading from failure */
} gd_trigger_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Feature definition
 */
typedef struct {
    char name[64];              /**< Feature name */
    uint32_t feature_id;        /**< Unique ID */
    gd_priority_t priority;     /**< Feature priority */
    float resource_cost[GD_RESOURCE_COUNT]; /**< Resource usage per type */
    gd_tier_t minimum_tier;     /**< Min tier where enabled */
    bool is_enabled;            /**< Currently enabled */
    bool can_degrade;           /**< Supports quality reduction */
    float current_quality;      /**< Current quality (0-100) */
    float min_quality;          /**< Minimum acceptable quality */
} gd_feature_t;

/**
 * @brief Resource budget
 */
typedef struct {
    gd_resource_t type;         /**< Resource type */
    float current_usage;        /**< Current usage (0-100) */
    float budget_per_tier[GD_MAX_TIERS]; /**< Budget at each tier */
    float warning_threshold;    /**< Warning level (0-100) */
    float critical_threshold;   /**< Critical level (0-100) */
} gd_resource_budget_t;

/**
 * @brief Tier transition threshold
 */
typedef struct {
    gd_resource_t resource;     /**< Resource type */
    float upgrade_threshold;    /**< Upgrade when below */
    float downgrade_threshold;  /**< Downgrade when above */
} gd_tier_threshold_t;

/**
 * @brief Degradation profile
 */
typedef struct {
    char name[64];                  /**< Profile name */
    uint32_t profile_id;            /**< Unique ID */
    gd_tier_t current_tier;         /**< Current degradation tier */
    gd_tier_threshold_t thresholds[GD_MAX_THRESHOLDS]; /**< Tier thresholds */
    uint32_t threshold_count;       /**< Number of thresholds */
    uint32_t enabled_features[GD_MAX_FEATURES]; /**< Enabled feature IDs */
    uint32_t enabled_count;         /**< Number of enabled features */
    float quality_multipliers[GD_MAX_TIERS]; /**< Quality per tier */
    uint64_t tier_change_cooldown_ms; /**< Min time between changes */
    uint64_t last_tier_change_ms;   /**< Last tier change time */
} gd_profile_t;

/**
 * @brief Degradation action
 */
typedef struct {
    gd_action_t action;         /**< Action type */
    uint32_t target_id;         /**< Target feature/resource ID */
    float parameter;            /**< Action parameter */
    char description[128];      /**< Human description */
} gd_degradation_action_t;

/**
 * @brief Tier transition event
 */
typedef struct {
    gd_tier_t from_tier;        /**< Previous tier */
    gd_tier_t to_tier;          /**< New tier */
    gd_trigger_t trigger;       /**< What triggered change */
    gd_resource_t trigger_resource; /**< Which resource (if applicable) */
    float trigger_value;        /**< Value that triggered */
    uint64_t timestamp_ms;      /**< Transition timestamp */
    gd_degradation_action_t actions[16]; /**< Actions taken */
    uint32_t action_count;      /**< Number of actions */
} gd_transition_event_t;

/**
 * @brief Load shedding configuration
 */
typedef struct {
    bool enabled;               /**< Load shedding enabled */
    float shed_rate;            /**< Percentage to shed (0-100) */
    gd_priority_t min_priority; /**< Min priority to accept */
    uint64_t shed_duration_ms;  /**< How long to shed */
    uint32_t shed_count;        /**< Items shed so far */
} gd_load_shed_config_t;

/**
 * @brief Configuration for graceful degradation
 */
typedef struct {
    bool enable_auto_degradation;   /**< Automatic tier changes */
    bool enable_load_shedding;      /**< Enable load shedding */
    bool enable_quality_reduction;  /**< Enable quality reduction */
    float hysteresis_percent;       /**< Hysteresis for transitions */
    uint64_t check_interval_ms;     /**< Resource check interval */
    uint64_t tier_cooldown_ms;      /**< Min time between changes */
    gd_tier_t initial_tier;         /**< Starting tier */
    gd_tier_t minimum_tier;         /**< Lowest allowed tier */
} gd_config_t;

/**
 * @brief Statistics for graceful degradation
 */
typedef struct {
    uint64_t total_transitions;
    uint64_t upgrades;
    uint64_t downgrades;
    uint64_t time_per_tier_ms[GD_MAX_TIERS];
    uint64_t items_shed;
    uint64_t features_disabled;
    float avg_quality;
    float min_quality_reached;
    gd_tier_t lowest_tier_reached;
    uint64_t recovery_time_ms;      /**< Avg time to full tier */
} gd_stats_t;

/**
 * @brief Tier change callback
 */
typedef void (*gd_tier_callback_t)(
    const gd_transition_event_t* event,
    void* user_data
);

/**
 * @brief Opaque graceful degradation handle
 */
typedef struct gd_context gd_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create graceful degradation context
 *
 * WHAT: Initialize degradation management
 * WHY:  Required before any degradation operations
 * HOW:  Allocate context, initialize profiles
 *
 * @param config Configuration
 * @return GD context or NULL on failure
 */
gd_context_t* gd_create(const gd_config_t* config);

/**
 * @brief Destroy graceful degradation context
 *
 * @param ctx GD context
 */
void gd_destroy(gd_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
gd_config_t gd_default_config(void);

/**
 * @brief Start degradation monitoring
 *
 * @param ctx GD context
 * @return true on success
 */
bool gd_start(gd_context_t* ctx);

/**
 * @brief Stop degradation monitoring
 *
 * @param ctx GD context
 * @return true on success
 */
bool gd_stop(gd_context_t* ctx);

//=============================================================================
// Feature Management
//=============================================================================

/**
 * @brief Register feature
 *
 * @param ctx GD context
 * @param feature Feature definition
 * @return Feature ID, 0 on failure
 */
uint32_t gd_register_feature(gd_context_t* ctx, const gd_feature_t* feature);

/**
 * @brief Unregister feature
 *
 * @param ctx GD context
 * @param feature_id Feature ID
 * @return true on success
 */
bool gd_unregister_feature(gd_context_t* ctx, uint32_t feature_id);

/**
 * @brief Check if feature is enabled
 *
 * @param ctx GD context
 * @param feature_id Feature ID
 * @return true if enabled
 */
bool gd_is_feature_enabled(gd_context_t* ctx, uint32_t feature_id);

/**
 * @brief Get feature quality level
 *
 * @param ctx GD context
 * @param feature_id Feature ID
 * @return Quality level (0-100)
 */
float gd_get_feature_quality(gd_context_t* ctx, uint32_t feature_id);

/**
 * @brief Set feature enabled state
 *
 * @param ctx GD context
 * @param feature_id Feature ID
 * @param enabled Enable state
 * @return true on success
 */
bool gd_set_feature_enabled(gd_context_t* ctx, uint32_t feature_id, bool enabled);

/**
 * @brief Set feature quality
 *
 * @param ctx GD context
 * @param feature_id Feature ID
 * @param quality Quality level (0-100)
 * @return true on success
 */
bool gd_set_feature_quality(gd_context_t* ctx, uint32_t feature_id, float quality);

//=============================================================================
// Resource Management
//=============================================================================

/**
 * @brief Update resource usage
 *
 * @param ctx GD context
 * @param resource Resource type
 * @param usage Current usage (0-100)
 * @return true on success
 */
bool gd_update_resource(gd_context_t* ctx, gd_resource_t resource, float usage);

/**
 * @brief Get resource usage
 *
 * @param ctx GD context
 * @param resource Resource type
 * @return Current usage (0-100)
 */
float gd_get_resource_usage(gd_context_t* ctx, gd_resource_t resource);

/**
 * @brief Set resource budget
 *
 * @param ctx GD context
 * @param budget Resource budget
 * @return true on success
 */
bool gd_set_resource_budget(gd_context_t* ctx, const gd_resource_budget_t* budget);

/**
 * @brief Get resource budget
 *
 * @param ctx GD context
 * @param resource Resource type
 * @param budget Output budget
 * @return true on success
 */
bool gd_get_resource_budget(gd_context_t* ctx, gd_resource_t resource, gd_resource_budget_t* budget);

/**
 * @brief Check if resource is critical
 *
 * @param ctx GD context
 * @param resource Resource type
 * @return true if critical
 */
bool gd_is_resource_critical(gd_context_t* ctx, gd_resource_t resource);

//=============================================================================
// Tier Management
//=============================================================================

/**
 * @brief Get current degradation tier
 *
 * @param ctx GD context
 * @return Current tier
 */
gd_tier_t gd_get_current_tier(gd_context_t* ctx);

/**
 * @brief Set degradation tier manually
 *
 * @param ctx GD context
 * @param tier Target tier
 * @param reason Reason for change
 * @return true on success
 */
bool gd_set_tier(gd_context_t* ctx, gd_tier_t tier, const char* reason);

/**
 * @brief Evaluate and potentially change tier
 *
 * @param ctx GD context
 * @return true if tier changed
 */
bool gd_evaluate_tier(gd_context_t* ctx);

/**
 * @brief Get tier transition history
 *
 * @param ctx GD context
 * @param events Output array
 * @param max_events Array capacity
 * @return Number of events
 */
uint32_t gd_get_transition_history(
    gd_context_t* ctx,
    gd_transition_event_t* events,
    uint32_t max_events
);

//=============================================================================
// Profile Management
//=============================================================================

/**
 * @brief Create degradation profile
 *
 * @param ctx GD context
 * @param profile Profile definition
 * @return Profile ID, 0 on failure
 */
uint32_t gd_create_profile(gd_context_t* ctx, const gd_profile_t* profile);

/**
 * @brief Delete degradation profile
 *
 * @param ctx GD context
 * @param profile_id Profile ID
 * @return true on success
 */
bool gd_delete_profile(gd_context_t* ctx, uint32_t profile_id);

/**
 * @brief Activate profile
 *
 * @param ctx GD context
 * @param profile_id Profile to activate
 * @return true on success
 */
bool gd_activate_profile(gd_context_t* ctx, uint32_t profile_id);

/**
 * @brief Get active profile
 *
 * @param ctx GD context
 * @param profile Output profile
 * @return true if profile active
 */
bool gd_get_active_profile(gd_context_t* ctx, gd_profile_t* profile);

//=============================================================================
// Load Shedding
//=============================================================================

/**
 * @brief Start load shedding
 *
 * @param ctx GD context
 * @param shed_rate Percentage to shed (0-100)
 * @param min_priority Minimum priority to accept
 * @param duration_ms Shedding duration
 * @return true on success
 */
bool gd_start_load_shedding(
    gd_context_t* ctx,
    float shed_rate,
    gd_priority_t min_priority,
    uint64_t duration_ms
);

/**
 * @brief Stop load shedding
 *
 * @param ctx GD context
 * @return true on success
 */
bool gd_stop_load_shedding(gd_context_t* ctx);

/**
 * @brief Check if request should be accepted
 *
 * @param ctx GD context
 * @param priority Request priority
 * @return true if should accept
 */
bool gd_should_accept_request(gd_context_t* ctx, gd_priority_t priority);

/**
 * @brief Get load shedding status
 *
 * @param ctx GD context
 * @param config Output configuration
 * @return true if shedding active
 */
bool gd_get_load_shed_status(gd_context_t* ctx, gd_load_shed_config_t* config);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register tier change callback
 *
 * @param ctx GD context
 * @param callback Callback function
 * @param user_data User data
 * @return true on success
 */
bool gd_register_callback(gd_context_t* ctx, gd_tier_callback_t callback, void* user_data);

/**
 * @brief Unregister tier change callback
 *
 * @param ctx GD context
 * @param callback Callback to remove
 * @return true on success
 */
bool gd_unregister_callback(gd_context_t* ctx, gd_tier_callback_t callback);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get degradation statistics
 *
 * @param ctx GD context
 * @param stats Output statistics
 * @return true on success
 */
bool gd_get_stats(gd_context_t* ctx, gd_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx GD context
 */
void gd_reset_stats(gd_context_t* ctx);

/**
 * @brief Get time spent at each tier
 *
 * @param ctx GD context
 * @param tier Tier to query
 * @return Time in milliseconds
 */
uint64_t gd_get_time_at_tier(gd_context_t* ctx, gd_tier_t tier);

//=============================================================================
// String Conversion
//=============================================================================

const char* gd_tier_to_string(gd_tier_t tier);
const char* gd_priority_to_string(gd_priority_t priority);
const char* gd_resource_to_string(gd_resource_t resource);
const char* gd_action_to_string(gd_action_t action);
const char* gd_trigger_to_string(gd_trigger_t trigger);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GRACEFUL_DEGRADATION_H
