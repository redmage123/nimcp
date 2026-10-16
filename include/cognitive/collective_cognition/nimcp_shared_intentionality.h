/**
 * @file nimcp_shared_intentionality.h
 * @brief Joint goals, shared attention, and collective intentions
 *
 * WHAT: Model joint goals, shared attention, and "we-mode" cognition
 * WHY: Enable collective action and coordination between brain instances
 * HOW: Track shared goals, commitments, roles, and joint attention
 *
 * THEORETICAL BASIS:
 * - Shared Intentionality (Tomasello, 2005, 2009, 2014)
 * - Joint attention and joint action (Sebanz et al., 2006)
 * - We-mode vs I-mode cognition (Tuomela, 2005)
 * - Collective intentionality (Searle, 1990; Bratman, 2014)
 *
 * KEY CONCEPTS:
 * - Joint Attention: Multiple agents attending to the same target
 * - Shared Goals: Goals that multiple agents commit to together
 * - Role Understanding: Knowing one's role in collective action
 * - Mutual Responsiveness: Adjusting to each other's actions
 * - We-Mode: Operating as "we" rather than parallel "I"s
 *
 * @author NIMCP Team
 * @date 2025-01-01
 */

#ifndef NIMCP_SHARED_INTENTIONALITY_H
#define NIMCP_SHARED_INTENTIONALITY_H

#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Maximum description/name length */
#define SI_MAX_DESCRIPTION_LEN          128

/** Maximum feature vector dimensions */
#define SI_MAX_FEATURE_DIM              16

/** Maximum roles per goal */
#define SI_MAX_ROLES                    8

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Role types for collective action
 */
typedef enum {
    ROLE_LEADER = 0,        /**< Initiates and coordinates */
    ROLE_FOLLOWER,          /**< Follows leader's direction */
    ROLE_OBSERVER,          /**< Monitors but doesn't act */
    ROLE_EXECUTOR,          /**< Performs assigned actions */
    ROLE_VERIFIER,          /**< Checks correctness */
    ROLE_COMMUNICATOR       /**< Handles external communication */
} role_type_t;

/**
 * @brief Commitment to a shared goal
 */
typedef struct {
    uint32_t instance_id;
    float strength;             /**< Commitment strength [0-1] */
    uint64_t committed_at_us;
    role_type_t assigned_role;
    bool has_role;
} goal_commitment_t;

/**
 * @brief Shared goal definition
 */
typedef struct {
    uint32_t goal_id;
    char description[SI_MAX_DESCRIPTION_LEN];
    float priority;             /**< Goal priority [0-1] */
    uint32_t proposer_id;       /**< Instance that proposed */
    shared_goal_state_t state;
    float progress;             /**< Completion progress [0-1] */
    uint64_t created_at_us;
    uint64_t deadline_us;       /**< 0 = no deadline */

    /* Commitments */
    goal_commitment_t commitments[COLLECTIVE_MAX_INSTANCES];
    uint32_t commitment_count;

    /* Aggregate commitment */
    float total_commitment;     /**< Sum of commitment strengths */
} shared_goal_t;

/**
 * @brief Joint attention target
 */
typedef struct {
    uint32_t attention_id;
    float feature_vector[SI_MAX_FEATURE_DIM];
    float salience;             /**< How salient the target is [0-1] */
    uint32_t proposer_id;
    uint64_t started_at_us;

    /* Attending instances */
    uint32_t attending_instances[COLLECTIVE_MAX_INSTANCES];
    uint32_t attending_count;
    float agreement_level;      /**< How much attendees agree [0-1] */
} joint_attention_t;

/**
 * @brief Role assignment for collective action
 */
typedef struct {
    uint32_t goal_id;
    uint32_t instance_id;
    role_type_t role;
    char role_name[32];
    float capability_match;     /**< How well suited for role [0-1] */
    bool accepted;
} role_assignment_t;

/**
 * @brief Shared intentionality statistics
 */
typedef struct {
    uint64_t goals_proposed;
    uint64_t goals_accepted;
    uint64_t goals_completed;
    uint64_t goals_failed;
    uint64_t goals_abandoned;
    uint64_t attentions_proposed;
    uint64_t attentions_joined;
    uint64_t role_negotiations;
    float avg_commitment;
    float avg_goal_completion_time_ms;
    float we_mode_time_ratio;   /**< Time in we-mode vs total */
} shared_intentionality_stats_t;

/*=============================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Callback for goal state changes
 */
typedef void (*goal_state_callback_fn)(
    const shared_goal_t* goal,
    shared_goal_state_t old_state,
    void* user_data
);

/**
 * @brief Callback for joint attention events
 */
typedef void (*attention_callback_fn)(
    const joint_attention_t* attention,
    bool is_new,
    void* user_data
);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create shared intentionality system
 *
 * @param config Configuration (NULL for defaults)
 * @return Shared intentionality handle or NULL on failure
 */
shared_intentionality_t* shared_intentionality_create(
    const shared_intentionality_config_t* config
);

/**
 * @brief Destroy shared intentionality system
 *
 * @param si Shared intentionality system to destroy
 */
void shared_intentionality_destroy(shared_intentionality_t* si);

/**
 * @brief Reset shared intentionality system
 *
 * @param si Shared intentionality system
 * @return 0 on success, -1 on error
 */
int shared_intentionality_reset(shared_intentionality_t* si);

/*=============================================================================
 * Instance Management API
 *===========================================================================*/

/**
 * @brief Register an instance
 *
 * @param si Shared intentionality system
 * @param instance_id Instance identifier
 * @return 0 on success, -1 on error
 */
int shared_intentionality_register_instance(
    shared_intentionality_t* si,
    uint32_t instance_id
);

/**
 * @brief Unregister an instance
 *
 * @param si Shared intentionality system
 * @param instance_id Instance to unregister
 * @return 0 on success, -1 on error
 */
int shared_intentionality_unregister_instance(
    shared_intentionality_t* si,
    uint32_t instance_id
);

/*=============================================================================
 * Shared Goal API
 *===========================================================================*/

/**
 * @brief Propose a new shared goal
 *
 * @param si Shared intentionality system
 * @param goal Goal to propose
 * @return Goal ID on success, 0 on failure
 */
uint32_t shared_intentionality_propose_goal(
    shared_intentionality_t* si,
    const shared_goal_t* goal
);

/**
 * @brief Commit to a shared goal
 *
 * @param si Shared intentionality system
 * @param goal_id Goal to commit to
 * @param instance_id Committing instance
 * @param commitment Commitment strength [0-1]
 * @return 0 on success, -1 on error
 */
int shared_intentionality_commit_to_goal(
    shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id,
    float commitment
);

/**
 * @brief Withdraw commitment from a goal
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @param instance_id Withdrawing instance
 * @return 0 on success, -1 on error
 */
int shared_intentionality_withdraw_from_goal(
    shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id
);

/**
 * @brief Update goal progress
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @param progress New progress [0-1]
 * @return 0 on success, -1 on error
 */
int shared_intentionality_update_goal_progress(
    shared_intentionality_t* si,
    uint32_t goal_id,
    float progress
);

/**
 * @brief Complete a goal
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @return 0 on success, -1 on error
 */
int shared_intentionality_complete_goal(
    shared_intentionality_t* si,
    uint32_t goal_id
);

/**
 * @brief Fail a goal
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @return 0 on success, -1 on error
 */
int shared_intentionality_fail_goal(
    shared_intentionality_t* si,
    uint32_t goal_id
);

/**
 * @brief Get a shared goal
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @param goal Output goal
 * @return 0 on success, -1 on error
 */
int shared_intentionality_get_goal(
    const shared_intentionality_t* si,
    uint32_t goal_id,
    shared_goal_t* goal
);

/**
 * @brief Get all active goals
 *
 * @param si Shared intentionality system
 * @param goals Output array
 * @param max_goals Maximum goals to return
 * @return Number of goals returned
 */
uint32_t shared_intentionality_get_active_goals(
    const shared_intentionality_t* si,
    shared_goal_t* goals,
    uint32_t max_goals
);

/*=============================================================================
 * Joint Attention API
 *===========================================================================*/

/**
 * @brief Propose joint attention to a target
 *
 * @param si Shared intentionality system
 * @param attention Attention target
 * @return Attention ID on success, 0 on failure
 */
uint32_t shared_intentionality_propose_attention(
    shared_intentionality_t* si,
    const joint_attention_t* attention
);

/**
 * @brief Join an existing joint attention
 *
 * @param si Shared intentionality system
 * @param attention_id Attention ID
 * @param instance_id Joining instance
 * @return 0 on success, -1 on error
 */
int shared_intentionality_join_attention(
    shared_intentionality_t* si,
    uint32_t attention_id,
    uint32_t instance_id
);

/**
 * @brief Leave a joint attention
 *
 * @param si Shared intentionality system
 * @param attention_id Attention ID
 * @param instance_id Leaving instance
 * @return 0 on success, -1 on error
 */
int shared_intentionality_leave_attention(
    shared_intentionality_t* si,
    uint32_t attention_id,
    uint32_t instance_id
);

/**
 * @brief Get joint attention info
 *
 * @param si Shared intentionality system
 * @param attention_id Attention ID
 * @param attention Output attention
 * @return 0 on success, -1 on error
 */
int shared_intentionality_get_attention(
    const shared_intentionality_t* si,
    uint32_t attention_id,
    joint_attention_t* attention
);

/**
 * @brief Get all active joint attentions
 *
 * @param si Shared intentionality system
 * @param attentions Output array
 * @param max_attentions Maximum to return
 * @return Number returned
 */
uint32_t shared_intentionality_get_active_attentions(
    const shared_intentionality_t* si,
    joint_attention_t* attentions,
    uint32_t max_attentions
);

/*=============================================================================
 * Role API
 *===========================================================================*/

/**
 * @brief Assign a role to an instance for a goal
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @param instance_id Instance ID
 * @param role Role to assign
 * @return 0 on success, -1 on error
 */
int shared_intentionality_assign_role(
    shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id,
    role_type_t role
);

/**
 * @brief Negotiate roles for a goal
 *
 * Automatically assigns roles based on capabilities.
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @return 0 on success, -1 on error
 */
int shared_intentionality_negotiate_roles(
    shared_intentionality_t* si,
    uint32_t goal_id
);

/**
 * @brief Get role assignment for an instance
 *
 * @param si Shared intentionality system
 * @param goal_id Goal ID
 * @param instance_id Instance ID
 * @param assignment Output assignment
 * @return 0 on success, -1 on error
 */
int shared_intentionality_get_role(
    const shared_intentionality_t* si,
    uint32_t goal_id,
    uint32_t instance_id,
    role_assignment_t* assignment
);

/*=============================================================================
 * We-Mode API
 *===========================================================================*/

/**
 * @brief Get we-mode state
 *
 * @param si Shared intentionality system
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int shared_intentionality_get_we_mode(
    const shared_intentionality_t* si,
    we_mode_state_t* state
);

/**
 * @brief Check if we-mode is active
 *
 * @param si Shared intentionality system
 * @return true if we-mode is active
 */
bool shared_intentionality_is_we_mode_active(
    const shared_intentionality_t* si
);

/**
 * @brief Force transition to we-mode
 *
 * @param si Shared intentionality system
 * @return 0 on success, -1 on error
 */
int shared_intentionality_enter_we_mode(shared_intentionality_t* si);

/**
 * @brief Force transition out of we-mode
 *
 * @param si Shared intentionality system
 * @return 0 on success, -1 on error
 */
int shared_intentionality_exit_we_mode(shared_intentionality_t* si);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update shared intentionality state
 *
 * @param si Shared intentionality system
 * @return 0 on success, -1 on error
 */
int shared_intentionality_update(shared_intentionality_t* si);

/*=============================================================================
 * Callback API
 *===========================================================================*/

/**
 * @brief Set goal state change callback
 *
 * @param si Shared intentionality system
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int shared_intentionality_set_goal_callback(
    shared_intentionality_t* si,
    goal_state_callback_fn callback,
    void* user_data
);

/**
 * @brief Set joint attention callback
 *
 * @param si Shared intentionality system
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int shared_intentionality_set_attention_callback(
    shared_intentionality_t* si,
    attention_callback_fn callback,
    void* user_data
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get shared intentionality statistics
 *
 * @param si Shared intentionality system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int shared_intentionality_get_stats(
    const shared_intentionality_t* si,
    shared_intentionality_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param si Shared intentionality system
 */
void shared_intentionality_reset_stats(shared_intentionality_t* si);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Get role type name
 *
 * @param role Role type
 * @return Human-readable name
 */
const char* role_type_name(role_type_t role);

/**
 * @brief Get goal state name
 *
 * @param state Goal state
 * @return Human-readable name
 */
const char* goal_state_name(shared_goal_state_t state);

/*=============================================================================
 * Debug API
 *===========================================================================*/

/**
 * @brief Dump shared intentionality state for debugging
 *
 * @param si Shared intentionality system
 */
void shared_intentionality_dump(const shared_intentionality_t* si);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHARED_INTENTIONALITY_H */
