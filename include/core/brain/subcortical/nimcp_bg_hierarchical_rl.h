//=============================================================================
// nimcp_bg_hierarchical_rl.h - Hierarchical Reinforcement Learning / Options
//=============================================================================
/**
 * @file nimcp_bg_hierarchical_rl.h
 * @brief Hierarchical RL with options framework for basal ganglia
 *
 * BIOLOGICAL BASIS:
 * The basal ganglia supports hierarchical action organization:
 * - Primitive actions: Simple motor commands
 * - Options/Macros: Temporally extended action sequences
 * - Goals: High-level objectives that guide option selection
 *
 * KEY CONCEPTS:
 * - Options: (Initiation set, Policy, Termination condition)
 * - Intra-option learning: Learning within an option
 * - Option discovery: Automatic chunking of action sequences
 * - Semi-MDP: Markov Decision Process with temporal abstraction
 *
 * INTEGRATION:
 * - Striatum: Option initiation and selection
 * - GPi/SNr: Option gating
 * - PFC: Goal maintenance and option monitoring
 * - Dopamine: Option-level reward prediction errors
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_HIERARCHICAL_RL_H
#define NIMCP_BG_HIERARCHICAL_RL_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define BG_HRL_MAX_OPTIONS          64      /**< Maximum options */
#define BG_HRL_MAX_PRIMITIVES       32      /**< Maximum primitive actions */
#define BG_HRL_MAX_GOALS            16      /**< Maximum concurrent goals */
#define BG_HRL_MAX_SEQUENCE_LEN     32      /**< Max actions in sequence */
#define BG_HRL_MAX_HIERARCHY_DEPTH  4       /**< Max hierarchy levels */

/** Learning parameters */
#define BG_HRL_OPTION_LEARNING_RATE     0.1f
#define BG_HRL_INTRA_OPTION_LR          0.05f
#define BG_HRL_TERMINATION_LR           0.02f
#define BG_HRL_DISCOVERY_THRESHOLD      0.7f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Option state
 */
typedef enum {
    BG_OPTION_STATE_INACTIVE,       /**< Option not running */
    BG_OPTION_STATE_INITIATING,     /**< Starting option */
    BG_OPTION_STATE_EXECUTING,      /**< Option in progress */
    BG_OPTION_STATE_TERMINATING,    /**< Option ending */
    BG_OPTION_STATE_COMPLETED,      /**< Option finished successfully */
    BG_OPTION_STATE_ABORTED,        /**< Option terminated early */
    BG_OPTION_STATE_COUNT
} bg_option_state_t;

/**
 * @brief Option type
 */
typedef enum {
    BG_OPTION_TYPE_PRIMITIVE,       /**< Single action (degenerate) */
    BG_OPTION_TYPE_SEQUENCE,        /**< Fixed action sequence */
    BG_OPTION_TYPE_POLICY,          /**< State-dependent policy */
    BG_OPTION_TYPE_HIERARCHICAL,    /**< Contains sub-options */
    BG_OPTION_TYPE_COUNT
} bg_option_type_t;

/**
 * @brief Goal state
 */
typedef enum {
    BG_GOAL_STATE_INACTIVE,
    BG_GOAL_STATE_ACTIVE,
    BG_GOAL_STATE_ACHIEVED,
    BG_GOAL_STATE_FAILED,
    BG_GOAL_STATE_ABANDONED,
    BG_GOAL_STATE_COUNT
} bg_goal_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Primitive action
 */
typedef struct {
    uint32_t id;
    char name[64];
    float duration_ms;              /**< Expected execution time */
    float effort_cost;              /**< Effort required */
    float* state_requirements;      /**< Required state features */
    uint32_t num_requirements;
} bg_primitive_t;

/**
 * @brief Option termination condition
 */
typedef struct {
    float* termination_states;      /**< States where option can terminate */
    uint32_t num_term_states;
    float min_probability;          /**< Minimum termination probability */
    float learned_probability;      /**< Learned termination probability */
    uint32_t max_steps;             /**< Maximum steps before forced termination */
} bg_termination_t;

/**
 * @brief Option definition
 */
typedef struct {
    uint32_t id;
    char name[64];
    bg_option_type_t type;
    bg_option_state_t state;

    /* Initiation set */
    float* initiation_states;       /**< States where option can start */
    uint32_t num_init_states;
    float initiation_threshold;

    /* Policy (for POLICY type) */
    float** policy_weights;         /**< State x Action weights */
    uint32_t policy_state_dim;
    uint32_t policy_action_dim;

    /* Sequence (for SEQUENCE type) */
    uint32_t* action_sequence;
    uint32_t sequence_length;
    uint32_t current_step;

    /* Sub-options (for HIERARCHICAL type) */
    uint32_t* sub_option_ids;
    uint32_t num_sub_options;

    /* Termination */
    bg_termination_t termination;

    /* Value estimates */
    float value;                    /**< Expected return from option */
    float* q_values;                /**< Q(s, o) for each initiation state */

    /* Statistics */
    uint32_t execution_count;
    uint32_t success_count;
    float avg_duration;
    float avg_reward;
} bg_option_t;

/**
 * @brief Goal representation
 */
typedef struct {
    uint32_t id;
    char name[64];
    bg_goal_state_t state;

    float* goal_state;              /**< Target state features */
    uint32_t goal_dim;
    float achievement_threshold;

    float priority;                 /**< Goal priority [0,1] */
    float urgency;                  /**< Time pressure */
    float value;                    /**< Goal value */

    uint32_t* preferred_options;    /**< Options that serve this goal */
    uint32_t num_preferred;

    float progress;                 /**< Estimated progress [0,1] */
    uint32_t attempts;
} bg_goal_t;

/**
 * @brief Eligibility trace for options
 */
typedef struct {
    uint32_t option_id;
    float trace;                    /**< Eligibility value */
    float* state_trace;             /**< State-dependent traces */
    uint32_t trace_dim;
} bg_option_trace_t;

/**
 * @brief HRL system configuration
 */
typedef struct {
    uint32_t max_options;
    uint32_t max_primitives;
    uint32_t max_goals;
    uint32_t state_dim;

    float option_learning_rate;
    float intra_option_lr;
    float termination_lr;
    float discount_factor;
    float trace_decay;

    bool enable_option_discovery;
    float discovery_threshold;
    uint32_t min_sequence_for_discovery;

    bool enable_interruption;       /**< Allow option interruption */
    float interruption_cost;
} bg_hrl_config_t;

/**
 * @brief HRL system statistics
 */
typedef struct {
    uint32_t total_options;
    uint32_t active_options;
    uint32_t options_discovered;
    uint32_t total_goals;
    uint32_t goals_achieved;
    float avg_option_duration;
    float avg_hierarchy_depth;
    float option_reuse_rate;
} bg_hrl_stats_t;

/**
 * @brief Main HRL system handle
 */
typedef struct bg_hrl_system bg_hrl_system_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void bg_hrl_default_config(bg_hrl_config_t* config);
bg_hrl_system_t* bg_hrl_create(const bg_hrl_config_t* config);
void bg_hrl_destroy(bg_hrl_system_t* system);
int bg_hrl_reset(bg_hrl_system_t* system);

/* ============================================================================
 * OPTION MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Register a primitive action
 */
int bg_hrl_register_primitive(bg_hrl_system_t* system,
                               const bg_primitive_t* primitive);

/**
 * @brief Create a new option
 */
int bg_hrl_create_option(bg_hrl_system_t* system,
                          const bg_option_t* option_def,
                          uint32_t* out_id);

/**
 * @brief Create option from action sequence
 */
int bg_hrl_create_sequence_option(bg_hrl_system_t* system,
                                   const char* name,
                                   const uint32_t* actions,
                                   uint32_t num_actions,
                                   uint32_t* out_id);

/**
 * @brief Create hierarchical option from sub-options
 */
int bg_hrl_create_hierarchical_option(bg_hrl_system_t* system,
                                       const char* name,
                                       const uint32_t* sub_options,
                                       uint32_t num_sub,
                                       uint32_t* out_id);

/**
 * @brief Get option by ID
 */
const bg_option_t* bg_hrl_get_option(const bg_hrl_system_t* system,
                                      uint32_t option_id);

/* ============================================================================
 * GOAL MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Register a goal
 */
int bg_hrl_register_goal(bg_hrl_system_t* system,
                          const bg_goal_t* goal,
                          uint32_t* out_id);

/**
 * @brief Activate a goal
 */
int bg_hrl_activate_goal(bg_hrl_system_t* system, uint32_t goal_id);

/**
 * @brief Deactivate a goal
 */
int bg_hrl_deactivate_goal(bg_hrl_system_t* system, uint32_t goal_id);

/**
 * @brief Update goal progress
 */
int bg_hrl_update_goal_progress(bg_hrl_system_t* system,
                                 uint32_t goal_id,
                                 const float* current_state);

/* ============================================================================
 * EXECUTION API
 * ============================================================================ */

/**
 * @brief Select option given current state and goals
 */
int bg_hrl_select_option(bg_hrl_system_t* system,
                          const float* state,
                          uint32_t* out_option_id);

/**
 * @brief Get next primitive action from current option
 */
int bg_hrl_get_action(bg_hrl_system_t* system,
                       const float* state,
                       uint32_t* out_action_id);

/**
 * @brief Check if current option should terminate
 */
bool bg_hrl_should_terminate(bg_hrl_system_t* system,
                              const float* state);

/**
 * @brief Interrupt current option
 */
int bg_hrl_interrupt_option(bg_hrl_system_t* system);

/**
 * @brief Step execution forward
 */
int bg_hrl_step(bg_hrl_system_t* system,
                 const float* state,
                 float reward,
                 float dt_ms);

/* ============================================================================
 * LEARNING API
 * ============================================================================ */

/**
 * @brief Update option values with TD error
 */
int bg_hrl_update_values(bg_hrl_system_t* system,
                          const float* state,
                          float reward,
                          const float* next_state,
                          bool terminal);

/**
 * @brief Update intra-option policy
 */
int bg_hrl_update_intra_option(bg_hrl_system_t* system,
                                uint32_t option_id,
                                const float* state,
                                uint32_t action,
                                float reward);

/**
 * @brief Update termination function
 */
int bg_hrl_update_termination(bg_hrl_system_t* system,
                               uint32_t option_id,
                               const float* state,
                               bool did_terminate);

/**
 * @brief Attempt to discover new options from recent history
 */
int bg_hrl_discover_options(bg_hrl_system_t* system);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Get current active option
 */
uint32_t bg_hrl_get_active_option(const bg_hrl_system_t* system);

/**
 * @brief Get option hierarchy depth
 */
uint32_t bg_hrl_get_hierarchy_depth(const bg_hrl_system_t* system);

/**
 * @brief Get option value
 */
float bg_hrl_get_option_value(const bg_hrl_system_t* system,
                               uint32_t option_id,
                               const float* state);

/**
 * @brief Get all option Q-values for state
 */
int bg_hrl_get_option_q_values(const bg_hrl_system_t* system,
                                const float* state,
                                float* q_values);

/**
 * @brief Get system statistics
 */
int bg_hrl_get_stats(const bg_hrl_system_t* system, bg_hrl_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_HIERARCHICAL_RL_H */
