//=============================================================================
// nimcp_bargaining.h - Nash Bargaining and Negotiation Protocols
//=============================================================================
/**
 * @file nimcp_bargaining.h
 * @brief Nash bargaining and negotiation protocols
 *
 * WHAT: Implements bargaining solutions and negotiation algorithms
 * WHY:  Enable cooperative division of resources and surplus
 * HOW:  Nash bargaining solution, alternating offers, Rubinstein bargaining
 *
 * BIOLOGICAL INSPIRATION:
 * - Inter-hemispheric negotiation via corpus callosum
 * - Synaptic competition and cooperation
 * - Hormonal regulation of resource allocation
 *
 * INTEGRATION: Hemispheric Brain (HEMISPHERIC_MODE_BARGAINING)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_BARGAINING_H
#define NIMCP_BARGAINING_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef enum {
    NIMCP_BARGAINING_NASH,            /**< Nash bargaining solution */
    NIMCP_BARGAINING_KALAI_SMORODINSKY, /**< Kalai-Smorodinsky solution */
    NIMCP_BARGAINING_EGALITARIAN,     /**< Egalitarian solution */
    NIMCP_BARGAINING_RUBINSTEIN,      /**< Alternating offers (Rubinstein) */
    NIMCP_BARGAINING_COUNT
} nimcp_bargaining_type_t;

typedef enum {
    NIMCP_BARGAINING_STATE_INITIALIZED,
    NIMCP_BARGAINING_STATE_NEGOTIATING,
    NIMCP_BARGAINING_STATE_AGREED,
    NIMCP_BARGAINING_STATE_DISAGREEMENT,
    NIMCP_BARGAINING_STATE_TIMEOUT
} nimcp_bargaining_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bargaining configuration
 */
typedef struct {
    nimcp_bargaining_type_t type;     /**< Bargaining solution type */
    uint32_t num_players;             /**< Number of bargaining parties */
    float disagreement_payoffs[NIMCP_GT_MAX_PLAYERS]; /**< Payoff if no agreement */
    float bargaining_powers[NIMCP_GT_MAX_PLAYERS];    /**< Relative power (sum=1) */
    float discount_factor;            /**< Time discount (for Rubinstein) */
    uint32_t max_rounds;              /**< Maximum negotiation rounds */
    float convergence_threshold;      /**< Agreement threshold */
    uint64_t timeout_ms;              /**< Negotiation timeout (0=none) */
} nimcp_bargaining_config_t;

/**
 * @brief Offer in alternating offers protocol
 */
typedef struct {
    nimcp_player_id_t proposer;       /**< Who made the offer */
    float proposed_allocation[NIMCP_GT_MAX_PLAYERS]; /**< Proposed split */
    uint32_t round;                   /**< Round number */
    uint64_t timestamp_ms;            /**< When offer was made */
    bool is_final;                    /**< Final offer? */
} nimcp_offer_t;

/**
 * @brief Bargaining outcome
 */
typedef struct {
    nimcp_bargaining_state_t state;   /**< Final state */
    float allocations[NIMCP_GT_MAX_PLAYERS]; /**< Final allocation */
    float utilities[NIMCP_GT_MAX_PLAYERS];   /**< Achieved utilities */
    uint32_t rounds_taken;            /**< Rounds until agreement */
    float nash_product;               /**< Product of gains (Nash objective) */
    bool is_pareto_optimal;           /**< On Pareto frontier? */
    bool is_individually_rational;    /**< All players >= disagreement? */
} nimcp_bargaining_outcome_t;

/**
 * @brief Feasibility point (utility profile)
 */
typedef struct {
    float utilities[NIMCP_GT_MAX_PLAYERS]; /**< Utility for each player */
} nimcp_feasible_point_t;

/**
 * @brief Opaque bargaining handle
 */
typedef struct nimcp_bargaining_struct* nimcp_bargaining_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default bargaining configuration
 *
 * @param num_players Number of players
 * @return Default configuration
 */
nimcp_bargaining_config_t nimcp_bargaining_default_config(uint32_t num_players);

/**
 * @brief Create bargaining context
 *
 * @param config Bargaining configuration
 * @return Bargaining handle or NULL
 */
nimcp_bargaining_t nimcp_bargaining_create(const nimcp_bargaining_config_t* config);

/**
 * @brief Destroy bargaining context
 */
void nimcp_bargaining_destroy(nimcp_bargaining_t bargaining);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Set feasible set for bargaining
 *
 * @param bargaining Bargaining handle
 * @param points Feasible utility profiles
 * @param num_points Number of points
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_set_feasible_set(
    nimcp_bargaining_t bargaining,
    const nimcp_feasible_point_t* points,
    uint32_t num_points
);

/**
 * @brief Compute Nash bargaining solution analytically
 *
 * WHAT: Find allocation maximizing Nash product
 * WHY:  Optimal, fair division satisfying key axioms
 * HOW:  Optimize product of gains over disagreement
 *
 * FORMULA: argmax Product[(u_i - d_i)^alpha_i]
 * where u_i = utility, d_i = disagreement point, alpha_i = bargaining power
 *
 * PROPERTIES SATISFIED:
 * - Pareto optimality
 * - Individual rationality
 * - Independence of irrelevant alternatives
 * - Scale invariance
 * - Symmetry (when powers equal)
 *
 * @param bargaining Bargaining handle
 * @param outcome Output bargaining outcome
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_compute_nash_solution(
    nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
);

/**
 * @brief Compute Kalai-Smorodinsky solution
 *
 * WHAT: Find allocation on line from disagreement to utopia
 * WHY:  Monotonicity property (expanding feasible set helps all)
 * HOW:  Intersection of Pareto frontier with d-u line
 *
 * @param bargaining Bargaining handle
 * @param outcome Output outcome
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_compute_kalai_smorodinsky(
    nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
);

/**
 * @brief Compute egalitarian solution
 *
 * WHAT: Maximize minimum utility gain
 * WHY:  Fairness (Rawlsian maximin)
 * HOW:  max min_i (u_i - d_i)
 *
 * @param bargaining Bargaining handle
 * @param outcome Output outcome
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_compute_egalitarian(
    nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
);

/**
 * @brief Make offer in alternating offers protocol
 *
 * @param bargaining Bargaining handle
 * @param proposer Who is making the offer
 * @param proposed_allocation Proposed split
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_make_offer(
    nimcp_bargaining_t bargaining,
    nimcp_player_id_t proposer,
    const float* proposed_allocation
);

/**
 * @brief Respond to current offer
 *
 * @param bargaining Bargaining handle
 * @param responder Who is responding
 * @param accept true to accept, false to reject
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_respond(
    nimcp_bargaining_t bargaining,
    nimcp_player_id_t responder,
    bool accept
);

/**
 * @brief Advance one round of Rubinstein negotiation
 *
 * @param bargaining Bargaining handle
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_advance_round(
    nimcp_bargaining_t bargaining
);

/**
 * @brief Check if agreement has been reached
 */
bool nimcp_bargaining_has_agreement(const nimcp_bargaining_t bargaining);

/**
 * @brief Get current bargaining outcome
 */
nimcp_error_t nimcp_bargaining_get_outcome(
    const nimcp_bargaining_t bargaining,
    nimcp_bargaining_outcome_t* outcome
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get bargaining state
 */
nimcp_bargaining_state_t nimcp_bargaining_get_state(const nimcp_bargaining_t bargaining);

/**
 * @brief Get current round number
 */
uint32_t nimcp_bargaining_get_round(const nimcp_bargaining_t bargaining);

/**
 * @brief Get current offer (if any)
 */
nimcp_error_t nimcp_bargaining_get_current_offer(
    const nimcp_bargaining_t bargaining,
    nimcp_offer_t* offer
);

/**
 * @brief Get bargaining type name
 */
const char* nimcp_bargaining_type_name(nimcp_bargaining_type_t type);

/**
 * @brief Update bargaining powers
 *
 * @param bargaining Bargaining handle
 * @param powers New bargaining powers (must sum to 1.0)
 * @param num_players Number of players
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_set_powers(
    nimcp_bargaining_t bargaining,
    const float* powers,
    uint32_t num_players
);

/**
 * @brief Update disagreement payoffs
 *
 * @param bargaining Bargaining handle
 * @param disagreement New disagreement payoffs
 * @param num_players Number of players
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_bargaining_set_disagreement(
    nimcp_bargaining_t bargaining,
    const float* disagreement,
    uint32_t num_players
);

/**
 * @brief Compute Nash product for given allocation
 *
 * @param allocation Utility allocation
 * @param disagreement Disagreement point
 * @param powers Bargaining powers
 * @param n Number of players
 * @return Nash product value
 */
float nimcp_compute_nash_product(
    const float* allocation,
    const float* disagreement,
    const float* powers,
    uint32_t n
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BARGAINING_H
