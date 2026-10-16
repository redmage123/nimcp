/**
 * @file nimcp_basal_ganglia_executive_bridge.h
 * @brief Basal ganglia-executive prefrontal bridge
 *
 * WHAT: Integration bridge for goal-directed vs habitual control
 * WHY:  PFC exerts top-down control over BG action selection mode
 * HOW:  Executive controller modulates goal-directed/habitual balance
 *
 * BIOLOGICAL BASIS:
 * - PFC projects to dorsomedial striatum (DMS) for goal-directed behavior
 * - Reduced PFC input allows dorsolateral striatum (DLS) habits to dominate
 * - Task switching involves PFC suppression of current action and selection of new
 * - Cognitive load affects PFC capacity to maintain goal-directed control
 * - Inhibitory control from PFC can suppress BG-selected actions
 *
 * INTEGRATION PATHWAYS:
 * 1. DLPFC → DMS: Goal-directed action planning
 * 2. OFC → VS: Value computation
 * 3. ACC → BG: Conflict monitoring and effort allocation
 * 4. PFC inhibition → STN: Action suppression
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BASAL_GANGLIA_EXECUTIVE_BRIDGE_H
#define NIMCP_BASAL_GANGLIA_EXECUTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_executive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BGE_MAX_ACTIVE_GOALS       8     /**< Maximum concurrent goals */
#define BGE_DEFAULT_PFC_WEIGHT     0.7f  /**< Default PFC influence */
#define BGE_DEFAULT_LOAD_THRESHOLD 0.7f  /**< Cognitive load threshold */
#define BGE_HABIT_TAKEOVER_LOAD    0.9f  /**< Load for habit takeover */
#define BGE_SWITCH_COST_DA_DIP     0.2f  /**< DA dip during task switch */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Control mode
 */
typedef enum {
    BGE_CONTROL_GOAL_DIRECTED = 0, /**< PFC-controlled goal-directed */
    BGE_CONTROL_HABITUAL,          /**< Habit-dominated automatic */
    BGE_CONTROL_MIXED,             /**< Mixture of both */
    BGE_CONTROL_SUPPRESSED         /**< Action suppression active */
} bge_control_mode_t;

/**
 * @brief Goal state
 */
typedef enum {
    BGE_GOAL_PENDING = 0,         /**< Goal registered, not started */
    BGE_GOAL_ACTIVE,              /**< Currently pursuing goal */
    BGE_GOAL_ACHIEVED,            /**< Goal completed */
    BGE_GOAL_ABANDONED,           /**< Goal abandoned */
    BGE_GOAL_BLOCKED              /**< Goal blocked by inhibition */
} bge_goal_state_t;

/**
 * @brief Conflict type
 */
typedef enum {
    BGE_CONFLICT_NONE = 0,        /**< No conflict */
    BGE_CONFLICT_GOAL_HABIT,      /**< Goal vs habit conflict */
    BGE_CONFLICT_GOAL_GOAL,       /**< Multiple goals conflict */
    BGE_CONFLICT_RESPONSE         /**< Response conflict in BG */
} bge_conflict_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Goal representation
 */
typedef struct {
    uint32_t goal_id;             /**< Unique goal identifier */
    uint32_t target_action;       /**< Target action for this goal */
    float priority;               /**< Goal priority [0-1] */
    float value;                  /**< Expected value of achieving goal */
    bge_goal_state_t state;       /**< Current goal state */
    uint64_t start_time_ms;       /**< When goal became active */
    bool requires_inhibition;     /**< Goal requires suppressing habits */
} bge_goal_t;

/**
 * @brief Control state
 */
typedef struct {
    bge_control_mode_t mode;      /**< Current control mode */
    float pfc_influence;          /**< PFC control strength [0-1] */
    float cognitive_load;         /**< Current cognitive load [0-1] */
    float habit_pressure;         /**< Pressure to use habits [0-1] */
    float conflict_level;         /**< Current conflict [0-1] */
    bge_conflict_type_t conflict_type; /**< Type of conflict */
    uint32_t active_goal_count;   /**< Number of active goals */
} bge_control_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    float pfc_weight;             /**< PFC influence weight */
    float habit_weight;           /**< Habit system weight */
    float load_threshold;         /**< Load threshold for habit takeover */
    float conflict_threshold;     /**< Conflict detection threshold */
    float switch_cost_duration_ms; /**< Task switch cost duration */
    float inhibition_strength;    /**< Action inhibition strength */
    bool enable_load_monitoring;  /**< Monitor cognitive load */
    bool enable_conflict_detection; /**< Detect goal-habit conflicts */
    bool enable_switch_cost;      /**< Apply task switch costs */
} bge_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_decisions;     /**< Total action decisions */
    uint64_t goal_directed_count; /**< Goal-directed decisions */
    uint64_t habitual_count;      /**< Habitual decisions */
    uint64_t inhibited_count;     /**< Inhibited actions */
    uint64_t switch_events;       /**< Task switch events */
    uint64_t conflict_events;     /**< Conflict events detected */
    float avg_cognitive_load;     /**< Average cognitive load */
    float avg_pfc_influence;      /**< Average PFC influence */
    float avg_conflict_level;     /**< Average conflict level */
} bge_bridge_stats_t;

/**
 * @brief BG-Executive bridge instance
 */
typedef struct bge_bridge {
    bridge_base_t base;           /**< MUST be first: base bridge infrastructure */

    /* Connected components */
    basal_ganglia_t* bg;          /**< Connected basal ganglia */
    executive_controller_t* exec; /**< Connected executive controller */

    /* Goals */
    bge_goal_t goals[BGE_MAX_ACTIVE_GOALS]; /**< Active goals */
    uint32_t num_goals;           /**< Number of active goals */
    uint32_t next_goal_id;        /**< Next goal ID to assign */

    /* Control state */
    bge_control_state_t state;    /**< Current control state */

    /* Task switching */
    uint64_t last_switch_time_ms; /**< When last task switch occurred */
    bool in_switch_cost;          /**< Currently in switch cost period */

    /* Inhibition */
    float inhibition_level;       /**< Current action inhibition [0-1] */
    uint32_t inhibited_action;    /**< Currently inhibited action */

    /* Configuration */
    bge_bridge_config_t config;   /**< Configuration */

    /* Statistics */
    bge_bridge_stats_t stats;     /**< Runtime statistics *//**< Mutex for thread safety */
} bge_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Configuration to initialize
 */
void bge_bridge_default_config(bge_bridge_config_t* config);

/**
 * @brief Create BG-executive bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
bge_bridge_t* bge_bridge_create(const bge_bridge_config_t* config);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void bge_bridge_destroy(bge_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int bge_bridge_reset(bge_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect basal ganglia to bridge
 * @param bridge Bridge instance
 * @param bg Basal ganglia to connect
 * @return 0 on success, -1 on error
 */
int bge_bridge_connect_bg(bge_bridge_t* bridge, basal_ganglia_t* bg);

/**
 * @brief Connect executive controller to bridge
 * @param bridge Bridge instance
 * @param exec Executive controller to connect
 * @return 0 on success, -1 on error
 */
int bge_bridge_connect_executive(
    bge_bridge_t* bridge,
    executive_controller_t* exec
);

/**
 * @brief Check if fully connected
 * @param bridge Bridge instance
 * @return true if both BG and executive connected
 */
bool bge_bridge_is_connected(const bge_bridge_t* bridge);

/* ============================================================================
 * Goal Management Functions
 * ============================================================================ */

/**
 * @brief Register a goal
 * @param bridge Bridge instance
 * @param target_action Action needed to achieve goal
 * @param priority Goal priority [0-1]
 * @param value Expected value
 * @param goal_id Output: assigned goal ID
 * @return 0 on success, -1 on error
 */
int bge_bridge_register_goal(
    bge_bridge_t* bridge,
    uint32_t target_action,
    float priority,
    float value,
    uint32_t* goal_id
);

/**
 * @brief Mark goal as achieved
 * @param bridge Bridge instance
 * @param goal_id Goal to mark
 * @return 0 on success, -1 on error
 */
int bge_bridge_goal_achieved(bge_bridge_t* bridge, uint32_t goal_id);

/**
 * @brief Abandon goal
 * @param bridge Bridge instance
 * @param goal_id Goal to abandon
 * @return 0 on success, -1 on error
 */
int bge_bridge_abandon_goal(bge_bridge_t* bridge, uint32_t goal_id);

/**
 * @brief Get goal state
 * @param bridge Bridge instance
 * @param goal_id Goal to query
 * @return Goal state
 */
bge_goal_state_t bge_bridge_get_goal_state(
    const bge_bridge_t* bridge,
    uint32_t goal_id
);

/**
 * @brief Get highest priority goal
 * @param bridge Bridge instance
 * @return Goal ID of highest priority, or 0 if no goals
 */
uint32_t bge_bridge_get_top_goal(const bge_bridge_t* bridge);

/* ============================================================================
 * Control Mode Functions
 * ============================================================================ */

/**
 * @brief Update control state from executive
 *
 * Reads executive state and updates:
 * 1. Cognitive load → PFC influence
 * 2. Active tasks → goal-directed pressure
 * 3. Task switching → BG mode changes
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current time
 * @return 0 on success, -1 on error
 */
int bge_bridge_update_control(
    bge_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Apply control modulation to action values
 *
 * Modifies action values based on:
 * - Goal alignment (boost goal-relevant actions)
 * - Habit pressure (boost habitual actions if load high)
 * - Conflict resolution (suppress conflicting actions)
 *
 * @param bridge Bridge instance
 * @param action_values Action values to modify (in-place)
 * @param num_actions Number of actions
 * @return 0 on success, -1 on error
 */
int bge_bridge_apply_control(
    bge_bridge_t* bridge,
    float* action_values,
    uint32_t num_actions
);

/**
 * @brief Get current control mode
 * @param bridge Bridge instance
 * @return Current control mode
 */
bge_control_mode_t bge_bridge_get_mode(const bge_bridge_t* bridge);

/**
 * @brief Force control mode (for testing/override)
 * @param bridge Bridge instance
 * @param mode Mode to set
 * @return 0 on success, -1 on error
 */
int bge_bridge_set_mode(bge_bridge_t* bridge, bge_control_mode_t mode);

/**
 * @brief Get PFC influence level
 * @param bridge Bridge instance
 * @return PFC influence [0-1]
 */
float bge_bridge_get_pfc_influence(const bge_bridge_t* bridge);

/* ============================================================================
 * Inhibition Functions
 * ============================================================================ */

/**
 * @brief Inhibit specific action
 * @param bridge Bridge instance
 * @param action_id Action to inhibit
 * @param strength Inhibition strength [0-1]
 * @return 0 on success, -1 on error
 */
int bge_bridge_inhibit_action(
    bge_bridge_t* bridge,
    uint32_t action_id,
    float strength
);

/**
 * @brief Release action inhibition
 * @param bridge Bridge instance
 * @param action_id Action to release
 * @return 0 on success, -1 on error
 */
int bge_bridge_release_inhibition(
    bge_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Check if action is inhibited
 * @param bridge Bridge instance
 * @param action_id Action to check
 * @return true if inhibited
 */
bool bge_bridge_is_inhibited(
    const bge_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Global stop (inhibit all actions)
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int bge_bridge_global_stop(bge_bridge_t* bridge);

/* ============================================================================
 * Conflict Detection Functions
 * ============================================================================ */

/**
 * @brief Detect goal-habit conflict
 * @param bridge Bridge instance
 * @param conflict_type Output: type of conflict
 * @param conflict_level Output: conflict severity [0-1]
 * @return true if conflict detected
 */
bool bge_bridge_detect_conflict(
    bge_bridge_t* bridge,
    bge_conflict_type_t* conflict_type,
    float* conflict_level
);

/**
 * @brief Get current conflict level
 * @param bridge Bridge instance
 * @return Conflict level [0-1]
 */
float bge_bridge_get_conflict(const bge_bridge_t* bridge);

/* ============================================================================
 * Task Switch Functions
 * ============================================================================ */

/**
 * @brief Notify bridge of task switch
 * @param bridge Bridge instance
 * @param new_task_id New task ID from executive
 * @param current_time_ms Current time
 * @return 0 on success, -1 on error
 */
int bge_bridge_task_switch(
    bge_bridge_t* bridge,
    uint32_t new_task_id,
    uint64_t current_time_ms
);

/**
 * @brief Check if in switch cost period
 * @param bridge Bridge instance
 * @return true if switch cost active
 */
bool bge_bridge_in_switch_cost(const bge_bridge_t* bridge);

/**
 * @brief Get remaining switch cost duration
 * @param bridge Bridge instance
 * @param current_time_ms Current time
 * @return Remaining duration in ms
 */
float bge_bridge_get_switch_cost_remaining(
    const bge_bridge_t* bridge,
    uint64_t current_time_ms
);

/* ============================================================================
 * State Query Functions
 * ============================================================================ */

/**
 * @brief Get full control state
 * @param bridge Bridge instance
 * @param state Output: control state
 * @return 0 on success, -1 on error
 */
int bge_bridge_get_state(
    const bge_bridge_t* bridge,
    bge_control_state_t* state
);

/**
 * @brief Get cognitive load
 * @param bridge Bridge instance
 * @return Cognitive load [0-1]
 */
float bge_bridge_get_cognitive_load(const bge_bridge_t* bridge);

/**
 * @brief Get habit pressure
 * @param bridge Bridge instance
 * @return Habit pressure [0-1]
 */
float bge_bridge_get_habit_pressure(const bge_bridge_t* bridge);

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int bge_bridge_get_stats(
    const bge_bridge_t* bridge,
    bge_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 */
void bge_bridge_reset_stats(bge_bridge_t* bridge);

/**
 * @brief Get control mode name
 * @param mode Control mode
 * @return Mode name string
 */
const char* bge_control_mode_name(bge_control_mode_t mode);

/**
 * @brief Get goal state name
 * @param state Goal state
 * @return State name string
 */
const char* bge_goal_state_name(bge_goal_state_t state);

/**
 * @brief Get conflict type name
 * @param type Conflict type
 * @return Type name string
 */
const char* bge_conflict_type_name(bge_conflict_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_EXECUTIVE_BRIDGE_H */
