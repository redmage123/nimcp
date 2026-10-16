//=============================================================================
// nimcp_credit_assignment.h - Shapley Value and Credit Assignment
//=============================================================================
/**
 * @file nimcp_credit_assignment.h
 * @brief Shapley value and credit assignment for cooperative games
 *
 * WHAT: Compute fair credit allocation for cooperative outcomes
 * WHY:  Enable principled attribution of contributions
 * HOW:  Shapley value, Banzhaf index, core membership
 *
 * BIOLOGICAL INSPIRATION:
 * - Synaptic credit assignment in learning
 * - Neural coalition contribution to decisions
 * - Hormonal attribution of reward/punishment
 *
 * INTEGRATION: Hemispheric Brain (credit for joint processing)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_CREDIT_ASSIGNMENT_H
#define NIMCP_CREDIT_ASSIGNMENT_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef enum {
    NIMCP_CREDIT_SHAPLEY,             /**< Shapley value (exact) */
    NIMCP_CREDIT_SHAPLEY_APPROX,      /**< Monte Carlo Shapley approximation */
    NIMCP_CREDIT_BANZHAF,             /**< Banzhaf power index */
    NIMCP_CREDIT_EQUAL_SPLIT,         /**< Equal division (baseline) */
    NIMCP_CREDIT_COUNT
} nimcp_credit_method_t;

/**
 * @brief Characteristic function callback
 *
 * WHAT: Returns value of a coalition
 * WHY:  Define cooperative game by coalition values
 * HOW:  Callback computes v(S) for any coalition S
 *
 * @param coalition Bitmask of players in coalition (bit i = player i)
 * @param num_players Total number of players
 * @param user_data Context for value computation
 * @return Value of coalition
 */
typedef float (*nimcp_coalition_value_fn)(
    uint32_t coalition,
    uint32_t num_players,
    void* user_data
);

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Credit assignment configuration
 */
typedef struct {
    nimcp_credit_method_t method;     /**< Credit computation method */
    uint32_t num_players;             /**< Number of players */
    uint32_t monte_carlo_samples;     /**< Samples for approximation */
    float convergence_epsilon;        /**< Convergence threshold */
    bool cache_coalitions;            /**< Cache coalition values? */
} nimcp_credit_config_t;

/**
 * @brief Credit assignment result
 */
typedef struct {
    float credits[NIMCP_GT_MAX_PLAYERS]; /**< Credit per player */
    float total_value;                /**< Grand coalition value */
    bool is_in_core;                  /**< Is allocation in the core? */
    float symmetry_error;             /**< Error from symmetry axiom */
    float efficiency_error;           /**< Error from efficiency axiom */
    uint64_t coalitions_evaluated;    /**< Number of coalitions computed */
} nimcp_credit_result_t;

/**
 * @brief Opaque credit system handle
 */
typedef struct nimcp_credit_system_struct* nimcp_credit_system_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default credit assignment configuration
 *
 * @param num_players Number of players
 * @return Default configuration
 */
nimcp_credit_config_t nimcp_credit_default_config(uint32_t num_players);

/**
 * @brief Create credit assignment system
 *
 * @param config Configuration
 * @return System handle or NULL
 */
nimcp_credit_system_t nimcp_credit_create(const nimcp_credit_config_t* config);

/**
 * @brief Destroy credit assignment system
 */
void nimcp_credit_destroy(nimcp_credit_system_t system);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Compute Shapley values (exact)
 *
 * WHAT: Compute fair credit allocation via Shapley value
 * WHY:  Unique solution satisfying efficiency, symmetry, additivity, null player
 * HOW:  Average marginal contributions over all orderings
 *
 * FORMULA:
 *   phi_i = Sum over S not containing i:
 *           [|S|!(n-|S|-1)!/n!] * [v(S + {i}) - v(S)]
 *
 * COMPLEXITY: O(2^n) - feasible for n <= 20
 *
 * @param system Credit system handle
 * @param value_fn Characteristic function
 * @param user_data Context for value function
 * @param result Output credit result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_credit_compute_shapley(
    nimcp_credit_system_t system,
    nimcp_coalition_value_fn value_fn,
    void* user_data,
    nimcp_credit_result_t* result
);

/**
 * @brief Compute Shapley value for single player
 *
 * @param system Credit system handle
 * @param player Player index
 * @param value_fn Characteristic function
 * @param user_data Context
 * @param credit Output credit value
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_credit_compute_shapley_single(
    nimcp_credit_system_t system,
    uint32_t player,
    nimcp_coalition_value_fn value_fn,
    void* user_data,
    float* credit
);

/**
 * @brief Approximate Shapley via Monte Carlo sampling
 *
 * WHAT: Approximate Shapley using random permutations
 * WHY:  Feasible for large n where exact is intractable
 * HOW:  Sample random orderings, average marginal contributions
 *
 * COMPLEXITY: O(samples * n)
 *
 * @param system Credit system handle
 * @param value_fn Characteristic function
 * @param user_data Context
 * @param num_samples Number of samples (0 = use config default)
 * @param result Output credit result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_credit_approximate_shapley(
    nimcp_credit_system_t system,
    nimcp_coalition_value_fn value_fn,
    void* user_data,
    uint32_t num_samples,
    nimcp_credit_result_t* result
);

/**
 * @brief Compute Banzhaf power index
 *
 * WHAT: Alternative credit measure based on swing votes
 * WHY:  Different fairness properties than Shapley
 * HOW:  Count coalitions where player is pivotal
 *
 * @param system Credit system handle
 * @param value_fn Characteristic function
 * @param user_data Context
 * @param result Output credit result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_credit_compute_banzhaf(
    nimcp_credit_system_t system,
    nimcp_coalition_value_fn value_fn,
    void* user_data,
    nimcp_credit_result_t* result
);

/**
 * @brief Check if allocation is in the core
 *
 * WHAT: Verify no coalition can profitably deviate
 * WHY:  Core stability means no group wants to break away
 * HOW:  Check sum(allocation[S]) >= v(S) for all S
 *
 * @param system Credit system handle
 * @param allocation Proposed allocation
 * @param value_fn Characteristic function
 * @param user_data Context
 * @return true if in core
 */
bool nimcp_credit_is_in_core(
    nimcp_credit_system_t system,
    const float* allocation,
    nimcp_coalition_value_fn value_fn,
    void* user_data
);

/**
 * @brief Compute equal split allocation
 *
 * @param system Credit system handle
 * @param value_fn Characteristic function
 * @param user_data Context
 * @param result Output result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_credit_compute_equal_split(
    nimcp_credit_system_t system,
    nimcp_coalition_value_fn value_fn,
    void* user_data,
    nimcp_credit_result_t* result
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get credit method name
 */
const char* nimcp_credit_method_name(nimcp_credit_method_t method);

/**
 * @brief Get number of players
 */
uint32_t nimcp_credit_get_num_players(const nimcp_credit_system_t system);

/**
 * @brief Verify Shapley axioms for result
 *
 * @param result Credit result to verify
 * @param value_fn Characteristic function
 * @param user_data Context
 * @param efficiency_error Output: efficiency axiom error
 * @param symmetry_error Output: symmetry axiom error
 * @return true if axioms satisfied within tolerance
 */
bool nimcp_credit_verify_axioms(
    const nimcp_credit_result_t* result,
    nimcp_coalition_value_fn value_fn,
    void* user_data,
    float* efficiency_error,
    float* symmetry_error
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CREDIT_ASSIGNMENT_H
