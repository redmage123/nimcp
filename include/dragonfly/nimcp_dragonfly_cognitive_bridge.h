/**
 * @file nimcp_dragonfly_cognitive_bridge.h
 * @brief Dragonfly-to-Cognitive Systems Bridge
 *
 * WHAT: Bridges dragonfly target tracking to cognitive systems
 * WHY:  Enable bio-inspired interception to influence attention and decisions
 * HOW:  Connect TSDN/tracking to salience, attention, working memory, executive
 *
 * BIOLOGICAL RATIONALE:
 * Dragonfly interception requires tight integration with cognitive systems:
 *
 *   1. SALIENCE: Moving targets drive visual salience
 *      - TSDN responses map to salience intensity
 *      - Evasive maneuvers increase surprise/novelty
 *      - Target confidence affects salience confidence
 *
 *   2. ATTENTION: Target tracking modulates attention focus
 *      - Active target gets attention priority
 *      - Predicted position guides anticipatory attention
 *      - Multi-target scenarios require attention switching
 *
 *   3. WORKING MEMORY: Trajectory prediction requires state maintenance
 *      - Target history maintained in working memory
 *      - Prediction horizon uses WM capacity
 *      - Evasion patterns stored for recognition
 *
 *   4. EXECUTIVE: Interception planning needs executive control
 *      - Action selection (pursue/abort/switch target)
 *      - Resource allocation (speed vs accuracy tradeoff)
 *      - Goal maintenance (interception objective)
 *
 * ARCHITECTURE:
 *
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │                    Dragonfly System                             │
 *   │  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌───────────┐     │
 *   │  │   TSDN   │  │ Tracking │  │ Prediction│  │ Intercept │     │
 *   │  └────┬─────┘  └────┬─────┘  └─────┬─────┘  └─────┬─────┘     │
 *   └───────┼─────────────┼──────────────┼──────────────┼───────────┘
 *           │             │              │              │
 *   ┌───────▼─────────────▼──────────────▼──────────────▼───────────┐
 *   │                  Cognitive Bridge                              │
 *   │  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌───────────┐     │
 *   │  │ Salience │  │ Attention│  │  Working  │  │ Executive │     │
 *   │  │  Bridge  │  │  Bridge  │  │  Memory   │  │   Bridge  │     │
 *   │  └────┬─────┘  └────┬─────┘  └─────┬─────┘  └─────┬─────┘     │
 *   └───────┼─────────────┼──────────────┼──────────────┼───────────┘
 *           │             │              │              │
 *   ┌───────▼─────────────▼──────────────▼──────────────▼───────────┐
 *   │                  Cognitive Systems                             │
 *   │  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌───────────┐     │
 *   │  │ Salience │  │ Attention│  │  Working  │  │ Executive │     │
 *   │  │ Network  │  │  System  │  │  Memory   │  │  Control  │     │
 *   │  └──────────┘  └──────────┘  └───────────┘  └───────────┘     │
 *   └───────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 * @date 2024-12-28
 */

#ifndef NIMCP_DRAGONFLY_COGNITIVE_BRIDGE_H
#define NIMCP_DRAGONFLY_COGNITIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//=============================================================================
// Forward Declarations
//=============================================================================

/* Dragonfly system */
typedef struct dragonfly_system_s dragonfly_system_t;

/* Cognitive systems (opaque - may not be available) */
typedef struct salience_evaluator_struct* salience_evaluator_t;
typedef struct attention_system_struct* attention_system_t;
typedef struct working_memory_struct* working_memory_t;
typedef struct executive_control_struct* executive_control_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum targets in working memory */
#define COGNITIVE_MAX_WM_TARGETS 8

/** Maximum attention focus points */
#define COGNITIVE_MAX_ATTENTION_FOCI 4

/** Default salience decay rate */
#define COGNITIVE_SALIENCE_DECAY_RATE 0.95f

/** Working memory refresh interval (ms) */
#define COGNITIVE_WM_REFRESH_MS 50.0f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Cognitive bridge mode
 */
typedef enum {
    COGNITIVE_BRIDGE_PASSIVE = 0,   /**< Read from dragonfly, don't influence */
    COGNITIVE_BRIDGE_ACTIVE,        /**< Bidirectional influence */
    COGNITIVE_BRIDGE_OVERRIDE       /**< Dragonfly overrides cognitive systems */
} cognitive_bridge_mode_t;

/**
 * @brief Attention priority level from dragonfly
 */
typedef enum {
    ATTENTION_PRIORITY_NONE = 0,    /**< No target / below threshold */
    ATTENTION_PRIORITY_LOW,         /**< Background monitoring */
    ATTENTION_PRIORITY_MEDIUM,      /**< Active tracking, non-critical */
    ATTENTION_PRIORITY_HIGH,        /**< Active pursuit */
    ATTENTION_PRIORITY_CRITICAL     /**< Interception imminent */
} dragonfly_attention_priority_t;

/**
 * @brief Executive action recommendation
 */
typedef enum {
    EXEC_ACTION_NONE = 0,           /**< No action needed */
    EXEC_ACTION_TRACK,              /**< Continue tracking */
    EXEC_ACTION_PURSUE,             /**< Active pursuit */
    EXEC_ACTION_INTERCEPT,          /**< Execute interception */
    EXEC_ACTION_ABORT,              /**< Abort current action */
    EXEC_ACTION_SWITCH_TARGET       /**< Switch to new target */
} executive_action_t;

/**
 * @brief Target salience contribution
 */
typedef struct {
    float motion_salience;          /**< Salience from motion [0,1] */
    float velocity_salience;        /**< Salience from speed [0,1] */
    float direction_salience;       /**< Salience from heading toward self [0,1] */
    float evasion_salience;         /**< Salience from evasive behavior [0,1] */
    float combined_salience;        /**< Weighted combination [0,1] */
    float surprise;                 /**< Prediction error / surprise [0,1] */
    float novelty;                  /**< Novel motion pattern [0,1] */
    float urgency;                  /**< Time-critical urgency [0,1] */
} target_salience_t;

/**
 * @brief Attention focus derived from dragonfly
 */
typedef struct {
    float focus_position[3];        /**< Attention focus position (x,y,z) */
    float focus_direction;          /**< Direction of attention (radians) */
    float focus_width;              /**< Width of attention cone (radians) */
    dragonfly_attention_priority_t priority;
    float confidence;               /**< Focus confidence [0,1] */
    uint32_t target_id;             /**< Associated target ID */
} dragonfly_attention_focus_t;

/**
 * @brief Working memory target entry
 */
typedef struct {
    uint32_t target_id;             /**< Target identifier */
    float position[3];              /**< Current position */
    float velocity[3];              /**< Current velocity */
    float predicted_position[3];    /**< Predicted position */
    float confidence;               /**< Track confidence [0,1] */
    uint64_t last_update_us;        /**< Last update timestamp */
    uint32_t update_count;          /**< Number of updates */
    bool is_evading;                /**< Evasive behavior detected */
    uint8_t evasion_pattern;        /**< Evasion pattern ID */
} wm_target_entry_t;

/**
 * @brief Executive state for dragonfly actions
 */
typedef struct {
    executive_action_t current_action;
    executive_action_t recommended_action;
    uint32_t active_target_id;
    float action_confidence;
    float time_to_intercept_ms;
    float success_probability;
    bool abort_recommended;
    const char* abort_reason;
} executive_state_t;

/**
 * @brief Cognitive bridge configuration
 */
typedef struct {
    /* Bridge mode */
    cognitive_bridge_mode_t mode;

    /* Salience parameters */
    float motion_salience_weight;       /**< Weight for motion salience */
    float velocity_salience_weight;     /**< Weight for velocity salience */
    float direction_salience_weight;    /**< Weight for direction salience */
    float evasion_salience_weight;      /**< Weight for evasion salience */
    float salience_threshold;           /**< Min salience for attention */
    float salience_decay_rate;          /**< Temporal decay rate */

    /* Attention parameters */
    float attention_base_width;         /**< Base attention cone width */
    float attention_narrowing_factor;   /**< Narrowing with confidence */
    uint32_t max_attention_foci;        /**< Maximum simultaneous foci */

    /* Working memory parameters */
    uint32_t wm_max_targets;            /**< Max targets to maintain */
    float wm_refresh_interval_ms;       /**< How often to refresh WM */
    float wm_decay_time_ms;             /**< Time before target fades */

    /* Executive parameters */
    float pursuit_threshold;            /**< Min confidence for pursuit */
    float intercept_threshold;          /**< Min confidence for intercept */
    float abort_threshold;              /**< Max failure prob before abort */
    bool allow_target_switching;        /**< Enable target switching */

    /* Integration */
    bool sync_on_update;                /**< Sync cognitive on each update */
    bool enable_feedback;               /**< Enable cognitive feedback */
} dragonfly_cognitive_config_t;

/**
 * @brief Cognitive bridge statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t salience_updates;
    uint64_t attention_updates;
    uint64_t wm_updates;
    uint64_t executive_updates;

    /* Metrics */
    float avg_salience;
    float avg_attention_width;
    float avg_wm_occupancy;
    float action_success_rate;

    /* Performance */
    uint64_t total_processing_time_us;
    float avg_processing_time_us;

    /* Decisions */
    uint32_t pursue_decisions;
    uint32_t intercept_decisions;
    uint32_t abort_decisions;
    uint32_t switch_decisions;
} cognitive_bridge_stats_t;

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Opaque bridge structure
 */
typedef struct dragonfly_cognitive_bridge_s dragonfly_cognitive_bridge_t;

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_bridge_default_config(dragonfly_cognitive_config_t* config);

/**
 * @brief Validate configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int dragonfly_cognitive_bridge_validate_config(const dragonfly_cognitive_config_t* config);

/**
 * @brief Create cognitive bridge
 *
 * @param dragonfly Dragonfly system (required)
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
dragonfly_cognitive_bridge_t* dragonfly_cognitive_bridge_create(
    dragonfly_system_t* dragonfly,
    const dragonfly_cognitive_config_t* config
);

/**
 * @brief Destroy cognitive bridge
 *
 * @param bridge Bridge handle
 */
void dragonfly_cognitive_bridge_destroy(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_bridge_reset(dragonfly_cognitive_bridge_t* bridge);

//=============================================================================
// Cognitive System Registration
//=============================================================================

/**
 * @brief Register salience evaluator
 *
 * @param bridge Bridge handle
 * @param salience Salience evaluator handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_register_salience(
    dragonfly_cognitive_bridge_t* bridge,
    salience_evaluator_t salience
);

/**
 * @brief Register attention system
 *
 * @param bridge Bridge handle
 * @param attention Attention system handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_register_attention(
    dragonfly_cognitive_bridge_t* bridge,
    attention_system_t attention
);

/**
 * @brief Register working memory
 *
 * @param bridge Bridge handle
 * @param wm Working memory handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_register_working_memory(
    dragonfly_cognitive_bridge_t* bridge,
    working_memory_t wm
);

/**
 * @brief Register executive control
 *
 * @param bridge Bridge handle
 * @param executive Executive control handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_register_executive(
    dragonfly_cognitive_bridge_t* bridge,
    executive_control_t executive
);

//=============================================================================
// Salience Functions
//=============================================================================

/**
 * @brief Compute target salience from dragonfly state
 *
 * @param bridge Bridge handle
 * @param target_id Target to evaluate
 * @param salience Output salience structure
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_compute_salience(
    dragonfly_cognitive_bridge_t* bridge,
    uint32_t target_id,
    target_salience_t* salience
);

/**
 * @brief Update salience system with dragonfly state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_update_salience(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Get most salient target
 *
 * @param bridge Bridge handle
 * @param target_id Output: most salient target ID
 * @param salience Output: salience value
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_get_most_salient(
    const dragonfly_cognitive_bridge_t* bridge,
    uint32_t* target_id,
    float* salience
);

//=============================================================================
// Attention Functions
//=============================================================================

/**
 * @brief Update attention focus from dragonfly tracking
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_update_attention(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Get current attention focus
 *
 * @param bridge Bridge handle
 * @param focus Output attention focus
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_get_attention_focus(
    const dragonfly_cognitive_bridge_t* bridge,
    dragonfly_attention_focus_t* focus
);

/**
 * @brief Get all attention foci (for multi-target)
 *
 * @param bridge Bridge handle
 * @param foci Output array of foci
 * @param max_foci Maximum foci to return
 * @param num_foci Output: actual number of foci
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_get_attention_foci(
    const dragonfly_cognitive_bridge_t* bridge,
    dragonfly_attention_focus_t* foci,
    uint32_t max_foci,
    uint32_t* num_foci
);

/**
 * @brief Set attention priority for target
 *
 * @param bridge Bridge handle
 * @param target_id Target ID
 * @param priority Priority level
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_set_attention_priority(
    dragonfly_cognitive_bridge_t* bridge,
    uint32_t target_id,
    dragonfly_attention_priority_t priority
);

//=============================================================================
// Working Memory Functions
//=============================================================================

/**
 * @brief Update working memory with target state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_update_working_memory(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Get target from working memory
 *
 * @param bridge Bridge handle
 * @param target_id Target ID
 * @param entry Output target entry
 * @return 0 on success, -1 if not found
 */
int dragonfly_cognitive_wm_get_target(
    const dragonfly_cognitive_bridge_t* bridge,
    uint32_t target_id,
    wm_target_entry_t* entry
);

/**
 * @brief Get all targets in working memory
 *
 * @param bridge Bridge handle
 * @param entries Output array
 * @param max_entries Maximum entries
 * @param num_entries Output: actual count
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_wm_get_all_targets(
    const dragonfly_cognitive_bridge_t* bridge,
    wm_target_entry_t* entries,
    uint32_t max_entries,
    uint32_t* num_entries
);

/**
 * @brief Get working memory occupancy
 *
 * @param bridge Bridge handle
 * @return Occupancy ratio [0,1]
 */
float dragonfly_cognitive_wm_get_occupancy(const dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Clear working memory
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_wm_clear(dragonfly_cognitive_bridge_t* bridge);

//=============================================================================
// Executive Control Functions
//=============================================================================

/**
 * @brief Update executive state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_update_executive(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Get current executive state
 *
 * @param bridge Bridge handle
 * @param state Output executive state
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_get_executive_state(
    const dragonfly_cognitive_bridge_t* bridge,
    executive_state_t* state
);

/**
 * @brief Get recommended action
 *
 * @param bridge Bridge handle
 * @return Recommended action
 */
executive_action_t dragonfly_cognitive_get_recommended_action(
    const dragonfly_cognitive_bridge_t* bridge
);

/**
 * @brief Execute recommended action
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_execute_action(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Request action abort
 *
 * @param bridge Bridge handle
 * @param reason Abort reason
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_request_abort(
    dragonfly_cognitive_bridge_t* bridge,
    const char* reason
);

//=============================================================================
// Unified Update Functions
//=============================================================================

/**
 * @brief Update all cognitive systems
 *
 * Synchronizes dragonfly state to all registered cognitive systems.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_update_all(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Step cognitive bridge (call each frame/cycle)
 *
 * @param bridge Bridge handle
 * @param dt_ms Time delta in milliseconds
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_step(
    dragonfly_cognitive_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_bridge_get_stats(
    const dragonfly_cognitive_bridge_t* bridge,
    cognitive_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_bridge_reset_stats(dragonfly_cognitive_bridge_t* bridge);

/**
 * @brief Get configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_bridge_get_config(
    const dragonfly_cognitive_bridge_t* bridge,
    dragonfly_cognitive_config_t* config
);

/**
 * @brief Set configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int dragonfly_cognitive_bridge_set_config(
    dragonfly_cognitive_bridge_t* bridge,
    const dragonfly_cognitive_config_t* config
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get action name
 *
 * @param action Executive action
 * @return Action name string
 */
const char* dragonfly_cognitive_action_name(executive_action_t action);

/**
 * @brief Get priority name
 *
 * @param priority Attention priority
 * @return Priority name string
 */
const char* dragonfly_cognitive_priority_name(dragonfly_attention_priority_t priority);

/**
 * @brief Get bridge mode name
 *
 * @param mode Bridge mode
 * @return Mode name string
 */
const char* dragonfly_cognitive_mode_name(cognitive_bridge_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_COGNITIVE_BRIDGE_H */
