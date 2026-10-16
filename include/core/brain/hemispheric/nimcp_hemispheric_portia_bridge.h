//=============================================================================
// nimcp_hemispheric_portia_bridge.h - Hemispheric Brain Portia Integration
//=============================================================================
/**
 * @file nimcp_hemispheric_portia_bridge.h
 * @brief Bidirectional integration between hemispheric brain and Portia system
 *
 * WHAT: Integration layer connecting hemispheric brain with Portia tier management
 * WHY:  Per-hemisphere resource allocation enables asymmetric processing based on
 *       task demands and available resources
 * HOW:  Subscribe to Portia tier switch events, apply per-hemisphere tiers,
 *       manage asymmetric resource allocation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ASYMMETRIC RESOURCE ALLOCATION:
 * --------------------------------
 * 1. Hemispheric Dominance:
 *    - Active hemisphere gets more blood flow (resources)
 *    - Dominant hemisphere for current task receives priority
 *    - Split attention divides resources ~50/50
 *
 * 2. Task-Dependent Allocation:
 *    - Language tasks: Left hemisphere priority
 *    - Spatial tasks: Right hemisphere priority
 *    - Complex tasks: Both hemispheres active
 *
 * 3. Energy Conservation:
 *    - Non-dominant hemisphere can reduce to "idle" mode
 *    - Sleep: One hemisphere at lower activity
 *    - Fatigue: Gradual tier reduction
 *
 * PORTIA SPIDER ANALOGY:
 * ----------------------
 * Like Portia fimbriata adapting hunting strategy to prey type:
 * - Easy prey (simple task): One hemisphere, low resources
 * - Dangerous prey (complex task): Both hemispheres, full resources
 * - Deception (creative task): Right hemisphere priority
 * - Planning (logical task): Left hemisphere priority
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    HEMISPHERIC-PORTIA BRIDGE                            |
 * +=========================================================================+
 * |                                                                          |
 * |   +-------------------------------+    +-----------------------------+  |
 * |   |     PORTIA TIER SWITCH        |    |    HEMISPHERIC BRAIN       |  |
 * |   |                               |    |                             |  |
 * |   |  - Memory pressure            |    |  LEFT    |    RIGHT        |  |
 * |   |  - Thermal throttle           |--->|  TIER    |    TIER         |  |
 * |   |  - Battery level              |    |  -----   |    -----        |  |
 * |   |  - Load monitoring            |    |  FULL    |    MEDIUM       |  |
 * |   +-------------------------------+    +-----------------------------+  |
 * |                   |                                  |                   |
 * |                   v                                  v                   |
 * |   +-------------------------------+    +-----------------------------+  |
 * |   |     ALLOCATION STRATEGY       |    |   RESOURCE DISTRIBUTION     |  |
 * |   |                               |    |                             |  |
 * |   |  TASK_BALANCED: 50/50         |    |  Left:  60% of resources    |  |
 * |   |  LEFT_DOMINANT: 70/30         |    |  Right: 40% of resources    |  |
 * |   |  RIGHT_DOMINANT: 30/70        |    |                             |  |
 * |   |  ADAPTIVE: Based on demand    |    |  (Example: LEFT_DOMINANT)   |  |
 * |   +-------------------------------+    +-----------------------------+  |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_HEMISPHERIC_PORTIA_BRIDGE_H
#define NIMCP_HEMISPHERIC_PORTIA_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "async/nimcp_bio_async.h"
#include "utils/thread/nimcp_thread.h"

// Forward declaration for Portia tier switch (opaque)
typedef struct portia_tier_switch_struct* portia_tier_switch_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default resource allocation fractions */
#define HEMI_PORTIA_BALANCED_FRACTION        0.50f   // 50/50 split
#define HEMI_PORTIA_LEFT_DOMINANT_FRACTION   0.70f   // 70% left
#define HEMI_PORTIA_RIGHT_DOMINANT_FRACTION  0.30f   // 30% left (70% right)
#define HEMI_PORTIA_EXTREME_DOMINANT         0.85f   // Near-exclusive

/** Tier transition hysteresis (ms) */
#define HEMI_PORTIA_TRANSITION_COOLDOWN_MS   500

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Resource allocation strategy
 */
typedef enum {
    ALLOCATION_BALANCED,        /**< Equal resources to both hemispheres */
    ALLOCATION_LEFT_DOMINANT,   /**< More resources to left (language/logic) */
    ALLOCATION_RIGHT_DOMINANT,  /**< More resources to right (spatial/creative) */
    ALLOCATION_ADAPTIVE,        /**< Dynamic based on current activity */
    ALLOCATION_TASK_DRIVEN      /**< Based on current task type */
} allocation_strategy_t;

/**
 * @brief Task types for task-driven allocation
 */
typedef enum {
    TASK_TYPE_UNKNOWN,
    TASK_TYPE_LANGUAGE,         /**< Left dominant */
    TASK_TYPE_LOGIC,            /**< Left dominant */
    TASK_TYPE_SPATIAL,          /**< Right dominant */
    TASK_TYPE_CREATIVE,         /**< Right dominant */
    TASK_TYPE_EMOTIONAL,        /**< Right dominant */
    TASK_TYPE_MOTOR,            /**< Contralateral */
    TASK_TYPE_ATTENTION,        /**< Balanced or right */
    TASK_TYPE_MEMORY            /**< Depends on content type */
} task_type_t;

/**
 * @brief Per-hemisphere resource state
 */
typedef struct {
    platform_tier_t current_tier;    /**< Current Portia tier */
    platform_tier_t target_tier;     /**< Target tier (during transition) */
    float resource_fraction;         /**< Fraction of total resources (0.0-1.0) */
    bool transition_in_progress;     /**< Tier transition active */
    uint64_t last_transition_ms;     /**< Time of last transition */
} hemisphere_resource_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    // Allocation settings
    allocation_strategy_t initial_strategy;  /**< Initial allocation strategy */
    float left_base_fraction;                /**< Base left hemisphere fraction */

    // Transition settings
    uint32_t transition_cooldown_ms;         /**< Min time between transitions */
    bool enable_gradual_transition;          /**< Smooth tier changes */

    // Adaptive settings
    float activity_threshold;                /**< Activity level to trigger reallocation */
    float hysteresis_margin;                 /**< Prevent oscillation */

    // Bio-async settings
    bool enable_bio_async;                   /**< Enable bio-async messaging */
    bool subscribe_tier_events;              /**< Subscribe to Portia events */
} hemispheric_portia_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t tier_transitions;               /**< Total tier transitions */
    uint64_t allocation_changes;             /**< Strategy changes */
    uint64_t portia_events_received;         /**< Portia tier switch events */
    float avg_left_fraction;                 /**< Average left hemisphere fraction */
    float time_in_left_dominant;             /**< Seconds in left-dominant mode */
    float time_in_right_dominant;            /**< Seconds in right-dominant mode */
    float time_in_balanced;                  /**< Seconds in balanced mode */
} hemispheric_portia_stats_t;

/**
 * @brief Hemispheric Portia bridge structure
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // Connected systems
    hemispheric_brain_t* brain;              /**< Hemispheric brain */
    portia_tier_switch_t portia_system;      /**< Portia tier switch (optional) */

    // Configuration
    hemispheric_portia_config_t config;

    // Current state
    allocation_strategy_t current_strategy;
    hemisphere_resource_state_t left_state;
    hemisphere_resource_state_t right_state;
    task_type_t current_task;
    platform_tier_t global_tier;             /**< Global tier from Portia */

    // Statistics
    hemispheric_portia_stats_t stats;

    // Bio-async
    // Thread safety
    // State
    bool initialized;
} hemispheric_portia_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default bridge configuration
 */
hemispheric_portia_config_t hemispheric_portia_default_config(void);

/**
 * @brief Create hemispheric Portia bridge
 *
 * @param config Bridge configuration
 * @param brain Hemispheric brain to connect
 * @param portia Portia tier switch system (optional, can be NULL)
 * @return Bridge instance or NULL on failure
 */
hemispheric_portia_bridge_t* hemispheric_portia_create(
    const hemispheric_portia_config_t* config,
    hemispheric_brain_t* brain,
    portia_tier_switch_t portia
);

/**
 * @brief Destroy hemispheric Portia bridge
 */
void hemispheric_portia_destroy(hemispheric_portia_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state (poll Portia, check activity)
 *
 * @param bridge Bridge instance
 * @param dt Time step in seconds
 * @return 0 on success, negative on error
 */
int hemispheric_portia_update(hemispheric_portia_bridge_t* bridge, float dt);

/**
 * @brief Apply computed tiers to hemispheric brain
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int hemispheric_portia_apply_allocation(hemispheric_portia_bridge_t* bridge);

//=============================================================================
// Control API
//=============================================================================

/**
 * @brief Set allocation strategy
 *
 * @param bridge Bridge instance
 * @param strategy New allocation strategy
 * @return 0 on success, negative on error
 */
int hemispheric_portia_set_strategy(
    hemispheric_portia_bridge_t* bridge,
    allocation_strategy_t strategy
);

/**
 * @brief Set current task type (for TASK_DRIVEN strategy)
 *
 * @param bridge Bridge instance
 * @param task Current task type
 * @return 0 on success, negative on error
 */
int hemispheric_portia_set_task(
    hemispheric_portia_bridge_t* bridge,
    task_type_t task
);

/**
 * @brief Set custom resource fraction
 *
 * @param bridge Bridge instance
 * @param left_fraction Fraction for left hemisphere (0.0-1.0)
 * @return 0 on success, negative on error
 */
int hemispheric_portia_set_fraction(
    hemispheric_portia_bridge_t* bridge,
    float left_fraction
);

/**
 * @brief Force tier for hemisphere
 *
 * @param bridge Bridge instance
 * @param hemisphere Target hemisphere
 * @param tier Target tier
 * @return 0 on success, negative on error
 */
int hemispheric_portia_force_tier(
    hemispheric_portia_bridge_t* bridge,
    hemisphere_id_t hemisphere,
    platform_tier_t tier
);

/**
 * @brief Handle global tier change from Portia
 *
 * @param bridge Bridge instance
 * @param new_tier New global tier from Portia
 * @return 0 on success, negative on error
 */
int hemispheric_portia_handle_tier_change(
    hemispheric_portia_bridge_t* bridge,
    platform_tier_t new_tier
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current allocation strategy
 */
allocation_strategy_t hemispheric_portia_get_strategy(
    const hemispheric_portia_bridge_t* bridge
);

/**
 * @brief Get current resource fraction
 */
float hemispheric_portia_get_fraction(
    const hemispheric_portia_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Get current tier for hemisphere
 */
platform_tier_t hemispheric_portia_get_tier(
    const hemispheric_portia_bridge_t* bridge,
    hemisphere_id_t hemisphere
);

/**
 * @brief Get bridge statistics
 */
hemispheric_portia_stats_t hemispheric_portia_get_stats(
    const hemispheric_portia_bridge_t* bridge
);

/**
 * @brief Reset statistics
 */
void hemispheric_portia_reset_stats(hemispheric_portia_bridge_t* bridge);

//=============================================================================
// Bio-async API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemispheric_portia_connect_bio_async(hemispheric_portia_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int hemispheric_portia_disconnect_bio_async(hemispheric_portia_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEMISPHERIC_PORTIA_BRIDGE_H
