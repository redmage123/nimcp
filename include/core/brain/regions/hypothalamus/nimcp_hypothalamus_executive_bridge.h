/**
 * @file nimcp_hypothalamus_executive_bridge.h
 * @brief Hypothalamus -> Executive Bridge for Drive-Aware Goal Management
 *
 * WHAT: Bridge between hypothalamus drives and executive goal management
 * WHY:  Drive states must influence goal priorities (survival over growth)
 * HOW:  Maps drive urgencies to goal priorities, enables survival interrupts
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem (hypothalamus) must be able to interrupt and
 * reprioritize executive goals based on drive states. A sudden threat
 * (SAFETY drive spike) should interrupt cognitive tasks. Hunger should
 * bias goals toward food-related activities.
 *
 * GOAL PRIORITY MAPPING:
 * Drive urgency maps to goal priority with survival drives dominating:
 * - SAFETY urgent: All other goals deprioritized (threat response)
 * - HUNGER/THIRST urgent: Food/water goals boosted
 * - FATIGUE urgent: Activity goals deprioritized, rest prioritized
 *
 * SURVIVAL INTERRUPTION:
 * When survival drives exceed threshold, the bridge can:
 * - Signal immediate goal interruption
 * - Inject survival-related goals
 * - Block non-survival goals until drive satisfied
 *
 * BIO-ASYNC MESSAGES:
 * - Receives: BIO_MSG_HYPO_DRIVE_STATE (from hypothalamus)
 * - Sends: BIO_MSG_EXEC_GOAL_PRIORITY, BIO_MSG_EXEC_INTERRUPT
 *
 * @version Phase 6: Executive Bridge
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_EXECUTIVE_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_EXECUTIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum number of goals that can be tracked */
#define HYPO_EXEC_MAX_GOALS         32

/** Maximum length of goal description */
#define HYPO_EXEC_MAX_GOAL_DESC     128

/** Default survival interrupt threshold */
#define HYPO_EXEC_SURVIVAL_THRESHOLD 0.8f

/** Goal priority boost for survival drives */
#define HYPO_EXEC_SURVIVAL_BOOST    2.0f

/** Goal priority reduction for conflicting drives */
#define HYPO_EXEC_CONFLICT_REDUCTION 0.5f

/*=============================================================================
 * GOAL TYPES
 *===========================================================================*/

/**
 * @brief Goal category types
 *
 * Goals are categorized by the drive they serve.
 * This enables drive-aware prioritization.
 */
typedef enum {
    HYPO_GOAL_CAT_SURVIVAL = 0,     /**< Safety, immediate needs */
    HYPO_GOAL_CAT_PHYSIOLOGICAL,    /**< Hunger, thirst, temperature */
    HYPO_GOAL_CAT_SOCIAL,           /**< Social connection, cooperation */
    HYPO_GOAL_CAT_COGNITIVE,        /**< Learning, curiosity, exploration */
    HYPO_GOAL_CAT_GROWTH,           /**< Competence, autonomy, mastery */
    HYPO_GOAL_CAT_EXTERNAL,         /**< User-specified goals */
    HYPO_GOAL_CAT_COUNT
} hypo_goal_category_t;

/**
 * @brief Interrupt level for survival drives
 */
typedef enum {
    HYPO_INTERRUPT_NONE = 0,        /**< No interruption */
    HYPO_INTERRUPT_SUGGEST,         /**< Suggest goal change */
    HYPO_INTERRUPT_PRIORITIZE,      /**< Reprioritize goals */
    HYPO_INTERRUPT_URGENT,          /**< Interrupt current goal */
    HYPO_INTERRUPT_CRITICAL         /**< Immediate goal override */
} hypo_interrupt_level_t;

/*=============================================================================
 * GOAL STRUCTURE
 *===========================================================================*/

/**
 * @brief Goal representation with drive-aware priority
 */
typedef struct {
    uint32_t goal_id;               /**< Unique goal identifier */
    char description[HYPO_EXEC_MAX_GOAL_DESC];  /**< Goal description */
    hypo_goal_category_t category;  /**< Goal category */

    /* Priority management */
    float base_priority;            /**< Base priority [0, 1] */
    float drive_boost;              /**< Boost from relevant drive */
    float effective_priority;       /**< Computed priority (base * boost) */

    /* Drive association */
    hypo_drive_type_t primary_drive;   /**< Primary drive served */
    float drive_relevance;             /**< How relevant to drive [0, 1] */

    /* State */
    bool active;                    /**< Goal is being pursued */
    bool blocked;                   /**< Blocked by survival interrupt */
    bool completed;                 /**< Goal achieved */

    /* Timing */
    uint64_t created_us;            /**< Creation timestamp */
    uint64_t activated_us;          /**< When activated */
    uint64_t deadline_us;           /**< Optional deadline (0 = none) */
} hypo_exec_goal_t;

/*=============================================================================
 * INTERRUPT SIGNAL
 *===========================================================================*/

/**
 * @brief Survival interrupt signal
 *
 * Sent when survival drives demand goal reprioritization
 */
typedef struct {
    hypo_interrupt_level_t level;   /**< Interrupt urgency */
    hypo_drive_type_t source_drive; /**< Drive causing interrupt */
    float drive_urgency;            /**< Urgency of source drive */

    /* Recommended action */
    bool should_suspend_current;    /**< Suspend current goal? */
    hypo_goal_category_t inject_category;  /**< Category to prioritize */
    char recommended_goal[HYPO_EXEC_MAX_GOAL_DESC];  /**< Suggested goal */

    /* Timing */
    uint64_t timestamp_us;
    uint32_t duration_hint_ms;      /**< Estimated interrupt duration */
} hypo_exec_interrupt_t;

/*=============================================================================
 * PRIORITY UPDATE
 *===========================================================================*/

/**
 * @brief Goal priority update from drive states
 */
typedef struct {
    /* Per-category boosts from drives */
    float category_boosts[HYPO_GOAL_CAT_COUNT];

    /* Per-goal priority updates */
    uint32_t goal_count;
    uint32_t goal_ids[HYPO_EXEC_MAX_GOALS];
    float new_priorities[HYPO_EXEC_MAX_GOALS];

    /* Interrupt status */
    bool interrupt_active;
    hypo_exec_interrupt_t interrupt;

    /* Source drive summary */
    float drive_urgencies[HYPO_DRIVE_COUNT];
    hypo_drive_type_t dominant_drive;

    uint64_t timestamp_us;
} hypo_exec_priority_update_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Executive bridge configuration
 */
typedef struct {
    /* Interrupt thresholds */
    float survival_interrupt_threshold;  /**< Urgency to trigger interrupt */
    float critical_interrupt_threshold;  /**< Urgency for critical interrupt */

    /* Priority computation */
    float drive_boost_scale;        /**< Scale for drive -> priority boost */
    float category_weights[HYPO_GOAL_CAT_COUNT];  /**< Category base weights */

    /* Drive-category mapping weights */
    float drive_category_map[HYPO_DRIVE_COUNT][HYPO_GOAL_CAT_COUNT];

    /* Behavior flags */
    bool enable_survival_interrupts;  /**< Allow survival drive interrupts */
    bool enable_priority_updates;     /**< Enable priority broadcasts */
    bool block_growth_during_survival;  /**< Block growth goals during survival */

    /* Integration options */
    bool broadcast_enabled;         /**< Enable bio-async broadcasts */
} hypo_exec_bridge_config_t;

/*=============================================================================
 * BRIDGE CONTEXT
 *===========================================================================*/

/**
 * @brief Executive bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_exec_bridge_config_t config;

    /* Connected hypothalamus */
    hypo_drive_system_handle_t* drives;

    /* Goal tracking */
    hypo_exec_goal_t goals[HYPO_EXEC_MAX_GOALS];
    uint32_t goal_count;
    uint32_t active_goal_id;        /**< Currently active goal */

    /* Interrupt state */
    bool interrupt_active;
    hypo_exec_interrupt_t current_interrupt;
    uint64_t interrupt_start_us;

    /* Priority cache */
    hypo_exec_priority_update_t last_update;

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t priority_updates;
    uint64_t interrupts_triggered;
    uint64_t goals_blocked;
    uint64_t goals_boosted;

} hypo_exec_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default executive bridge configuration
 *
 * @return Default config with balanced drive-goal mapping
 */
hypo_exec_bridge_config_t hypo_exec_bridge_default_config(void);

/**
 * @brief Create executive bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_exec_bridge_t* hypo_exec_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_exec_bridge_config_t* config);

/**
 * @brief Destroy executive bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_exec_bridge_destroy(hypo_exec_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_exec_bridge_reset(hypo_exec_bridge_t* bridge);

/*=============================================================================
 * GOAL MANAGEMENT
 *===========================================================================*/

/**
 * @brief Register a goal with the bridge
 *
 * WHAT: Add a goal for drive-aware priority management
 * WHY:  Goals must be registered to receive priority updates
 * HOW:  Store goal, compute initial priority from drives
 *
 * @param bridge Bridge context
 * @param description Goal description
 * @param category Goal category
 * @param primary_drive Drive this goal serves
 * @param base_priority Base priority [0, 1]
 * @return Goal ID, or 0 on failure
 */
uint32_t hypo_exec_bridge_register_goal(
    hypo_exec_bridge_t* bridge,
    const char* description,
    hypo_goal_category_t category,
    hypo_drive_type_t primary_drive,
    float base_priority);

/**
 * @brief Unregister a goal
 *
 * @param bridge Bridge context
 * @param goal_id Goal to unregister
 * @return true on success
 */
bool hypo_exec_bridge_unregister_goal(
    hypo_exec_bridge_t* bridge,
    uint32_t goal_id);

/**
 * @brief Activate a goal
 *
 * @param bridge Bridge context
 * @param goal_id Goal to activate
 * @return true on success, false if blocked by interrupt
 */
bool hypo_exec_bridge_activate_goal(
    hypo_exec_bridge_t* bridge,
    uint32_t goal_id);

/**
 * @brief Complete a goal
 *
 * @param bridge Bridge context
 * @param goal_id Goal that was completed
 * @param satisfaction Drive satisfaction resulting [0, 1]
 * @return true on success
 */
bool hypo_exec_bridge_complete_goal(
    hypo_exec_bridge_t* bridge,
    uint32_t goal_id,
    float satisfaction);

/**
 * @brief Get goal by ID
 *
 * @param bridge Bridge context
 * @param goal_id Goal to retrieve
 * @param goal Output goal structure
 * @return true on success
 */
bool hypo_exec_bridge_get_goal(
    const hypo_exec_bridge_t* bridge,
    uint32_t goal_id,
    hypo_exec_goal_t* goal);

/*=============================================================================
 * PRIORITY COMPUTATION
 *===========================================================================*/

/**
 * @brief Update goal priorities from drive states
 *
 * WHAT: Recompute all goal priorities based on current drives
 * WHY:  Drive changes should affect goal priorities
 * HOW:  Map drive urgencies to category boosts, apply to goals
 *
 * @param bridge Bridge context
 * @return Priority update structure
 */
hypo_exec_priority_update_t hypo_exec_bridge_update_priorities(
    hypo_exec_bridge_t* bridge);

/**
 * @brief Get highest priority goal
 *
 * @param bridge Bridge context
 * @return Goal ID of highest priority goal, or 0 if none
 */
uint32_t hypo_exec_bridge_get_top_priority_goal(
    const hypo_exec_bridge_t* bridge);

/**
 * @brief Get goals sorted by priority
 *
 * @param bridge Bridge context
 * @param goal_ids Output array of goal IDs (size HYPO_EXEC_MAX_GOALS)
 * @param count Output: number of goals returned
 * @return true on success
 */
bool hypo_exec_bridge_get_priority_order(
    const hypo_exec_bridge_t* bridge,
    uint32_t* goal_ids,
    uint32_t* count);

/**
 * @brief Get effective priority for a goal
 *
 * @param bridge Bridge context
 * @param goal_id Goal to query
 * @return Effective priority [0, 1], or -1 on error
 */
float hypo_exec_bridge_get_goal_priority(
    const hypo_exec_bridge_t* bridge,
    uint32_t goal_id);

/*=============================================================================
 * SURVIVAL INTERRUPTS
 *===========================================================================*/

/**
 * @brief Check for pending survival interrupt
 *
 * WHAT: Check if survival drives require goal interruption
 * WHY:  Survival takes precedence over all other goals
 * HOW:  Check drive urgencies against thresholds
 *
 * @param bridge Bridge context
 * @param interrupt Output interrupt structure (if any)
 * @return true if interrupt pending
 */
bool hypo_exec_bridge_check_interrupt(
    hypo_exec_bridge_t* bridge,
    hypo_exec_interrupt_t* interrupt);

/**
 * @brief Acknowledge interrupt
 *
 * Call after handling the interrupt signal
 *
 * @param bridge Bridge context
 * @return true on success
 */
bool hypo_exec_bridge_acknowledge_interrupt(
    hypo_exec_bridge_t* bridge);

/**
 * @brief Clear interrupt
 *
 * Call when survival drive has been satisfied
 *
 * @param bridge Bridge context
 * @return true on success
 */
bool hypo_exec_bridge_clear_interrupt(
    hypo_exec_bridge_t* bridge);

/**
 * @brief Get current interrupt level
 *
 * @param bridge Bridge context
 * @return Current interrupt level
 */
hypo_interrupt_level_t hypo_exec_bridge_get_interrupt_level(
    const hypo_exec_bridge_t* bridge);

/*=============================================================================
 * DRIVE-GOAL QUERIES
 *===========================================================================*/

/**
 * @brief Get goals relevant to a drive
 *
 * @param bridge Bridge context
 * @param drive Drive to query
 * @param goal_ids Output array of relevant goal IDs
 * @param count Output: number of goals returned
 * @return true on success
 */
bool hypo_exec_bridge_get_goals_for_drive(
    const hypo_exec_bridge_t* bridge,
    hypo_drive_type_t drive,
    uint32_t* goal_ids,
    uint32_t* count);

/**
 * @brief Get category boost from drive
 *
 * @param bridge Bridge context
 * @param category Goal category
 * @return Current boost for this category from drives
 */
float hypo_exec_bridge_get_category_boost(
    const hypo_exec_bridge_t* bridge,
    hypo_goal_category_t category);

/**
 * @brief Check if category is currently blocked
 *
 * @param bridge Bridge context
 * @param category Category to check
 * @return true if blocked by survival interrupt
 */
bool hypo_exec_bridge_is_category_blocked(
    const hypo_exec_bridge_t* bridge,
    hypo_goal_category_t category);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return true on success
 */
bool hypo_exec_bridge_register_bio(
    hypo_exec_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_exec_bridge_process_bio(
    hypo_exec_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast priority update
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_exec_bridge_broadcast_priorities(
    hypo_exec_bridge_t* bridge);

/**
 * @brief Broadcast interrupt signal
 *
 * @param bridge Bridge context
 * @param interrupt Interrupt to broadcast
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_exec_bridge_broadcast_interrupt(
    hypo_exec_bridge_t* bridge,
    const hypo_exec_interrupt_t* interrupt);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param priority_updates Output: total priority updates
 * @param interrupts Output: total interrupts triggered
 * @param goals_blocked Output: total goals blocked
 * @param goals_boosted Output: total goals boosted
 */
void hypo_exec_bridge_get_stats(
    const hypo_exec_bridge_t* bridge,
    uint64_t* priority_updates,
    uint64_t* interrupts,
    uint64_t* goals_blocked,
    uint64_t* goals_boosted);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get goal category name
 *
 * @param category Category type
 * @return Human-readable name
 */
const char* hypo_goal_category_string(hypo_goal_category_t category);

/**
 * @brief Get interrupt level name
 *
 * @param level Interrupt level
 * @return Human-readable name
 */
const char* hypo_interrupt_level_string(hypo_interrupt_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_EXECUTIVE_BRIDGE_H */
