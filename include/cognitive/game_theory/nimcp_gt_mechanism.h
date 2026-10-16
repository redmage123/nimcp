//=============================================================================
// nimcp_gt_mechanism.h - Mechanism Design and Incomplete Information
//=============================================================================
/**
 * @file nimcp_gt_mechanism.h
 * @brief Mechanism design primitives for incomplete information games
 *
 * WHAT: Private type spaces, signaling games, Bayesian Nash equilibrium
 * WHY:  Enable incentive-compatible resource allocation under uncertainty
 * HOW:  Revelation principle, incentive compatibility, signaling equilibria
 *
 * BIOLOGICAL INSPIRATION:
 * - Hormonal signaling (credible signals of internal state)
 * - Immune system recognition (self/non-self type revelation)
 * - Neural attention as mechanism for resource allocation
 * - Synaptic competition with private valuations
 *
 * INTEGRATION: Auction module (VCG mechanisms), Bargaining (revelation)
 *
 * ERROR CODES: Uses NIMCP_GT_ERROR_* from nimcp_game_theory.h
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_MECHANISM_H
#define NIMCP_GT_MECHANISM_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum types per player (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_TYPES 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_TYPES 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_TYPES 8
#else
    #define NIMCP_GT_MAX_TYPES 4
#endif

/** Maximum signals in signaling game (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_SIGNALS 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_SIGNALS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_SIGNALS 16
#else
    #define NIMCP_GT_MAX_SIGNALS 8
#endif

/** Maximum equilibrium iterations for Bayesian games */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_BAYESIAN_MAX_ITER 2000
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_BAYESIAN_MAX_ITER 1000
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_BAYESIAN_MAX_ITER 200
#else
    #define NIMCP_GT_BAYESIAN_MAX_ITER 100
#endif

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Mechanism types
 *
 * DIRECT: Players report types directly (revelation mechanism)
 * INDIRECT: Players take strategic actions (general mechanism)
 * SIGNALING: Sender-receiver game with type-dependent signals
 */
typedef enum {
    NIMCP_MECHANISM_DIRECT,           /**< Direct revelation mechanism */
    NIMCP_MECHANISM_INDIRECT,         /**< Indirect/general mechanism */
    NIMCP_MECHANISM_SIGNALING,        /**< Signaling game (sender-receiver) */
    NIMCP_MECHANISM_COUNT
} nimcp_mechanism_type_t;

/**
 * @brief Mechanism state
 */
typedef enum {
    NIMCP_MECHANISM_STATE_UNINITIALIZED,
    NIMCP_MECHANISM_STATE_READY,
    NIMCP_MECHANISM_STATE_RUNNING,
    NIMCP_MECHANISM_STATE_COMPLETED,
    NIMCP_MECHANISM_STATE_ERROR
} nimcp_mechanism_state_t;

/**
 * @brief Incentive compatibility level
 *
 * IC constraints determine when truth-telling is optimal:
 * - DOMINANT: Truth-telling optimal regardless of others' reports
 * - BAYES_NASH: Truth-telling optimal in expectation over types
 * - INTERIM: IC given own type (Bayesian interim IC)
 * - EX_POST: IC for all possible type realizations
 */
typedef enum {
    NIMCP_IC_NONE,                    /**< No IC guarantee */
    NIMCP_IC_DOMINANT_STRATEGY,       /**< Dominant strategy IC (DSIC) */
    NIMCP_IC_BAYES_NASH,              /**< Bayesian Nash IC (BNIC) */
    NIMCP_IC_INTERIM,                 /**< Interim IC */
    NIMCP_IC_EX_POST                  /**< Ex-post IC */
} nimcp_ic_level_t;

/**
 * @brief Signaling equilibrium type
 */
typedef enum {
    NIMCP_SIGNAL_EQUIL_SEPARATING,    /**< Different types send different signals */
    NIMCP_SIGNAL_EQUIL_POOLING,       /**< All types send same signal */
    NIMCP_SIGNAL_EQUIL_SEMI_SEPARATING, /**< Partial separation */
    NIMCP_SIGNAL_EQUIL_COUNT
} nimcp_signal_equilibrium_t;

//=============================================================================
// Type Space Structures
//=============================================================================

/**
 * @brief Private type for a player
 *
 * WHAT: Represents a single private type in the type space
 * WHY:  Encapsulates private information in Bayesian games
 * HOW:  Type determines payoffs and optimal behavior
 */
typedef struct {
    uint32_t type_id;                 /**< Type identifier (0-indexed) */
    float valuation;                  /**< Private valuation (for auctions) */
    float cost;                       /**< Private cost (for procurement) */
    float quality;                    /**< Quality/productivity parameter */
    void* custom_data;                /**< Application-specific type data */
    size_t custom_data_size;          /**< Size of custom data */
} nimcp_type_t;

/**
 * @brief Type space for a single player
 *
 * WHAT: Complete description of possible types and prior distribution
 * WHY:  Defines incomplete information structure
 * HOW:  Array of types with probability distribution
 */
typedef struct {
    nimcp_player_id_t player_id;      /**< Which player this type space belongs to */
    nimcp_type_t types[NIMCP_GT_MAX_TYPES]; /**< Array of possible types */
    float probabilities[NIMCP_GT_MAX_TYPES]; /**< Prior probability of each type */
    uint32_t num_types;               /**< Number of types in this space */
    uint32_t realized_type;           /**< Index of realized type (if known) */
    bool type_realized;               /**< Whether type has been realized */
} nimcp_type_space_t;

//=============================================================================
// Signaling Game Structures
//=============================================================================

/**
 * @brief Signal in a signaling game
 *
 * WHAT: Observable action by sender to convey information
 * WHY:  Enables communication of private information
 * HOW:  Signals may be costly (credible) or cheap-talk
 */
typedef struct {
    uint32_t signal_id;               /**< Signal identifier */
    char name[32];                    /**< Human-readable signal name */
    float cost;                       /**< Signal cost (0 = cheap talk) */
    float intensity;                  /**< Signal intensity/strength (0-1) */
    bool is_credible;                 /**< Whether signal is credible (costly) */
} nimcp_signal_t;

/**
 * @brief Signaling strategy (type -> signal mapping)
 */
typedef struct {
    uint32_t type_id;                 /**< Which type this strategy is for */
    float signal_probs[NIMCP_GT_MAX_SIGNALS]; /**< Probability of each signal */
    uint32_t num_signals;             /**< Number of available signals */
    bool is_pure;                     /**< Pure (single signal) vs mixed */
    uint32_t pure_signal;             /**< If pure, which signal */
} nimcp_signaling_strategy_t;

/**
 * @brief Receiver beliefs (signal -> type posterior)
 */
typedef struct {
    uint32_t signal_id;               /**< Which signal was observed */
    float posteriors[NIMCP_GT_MAX_TYPES]; /**< Posterior beliefs about sender type */
    uint32_t num_types;               /**< Number of sender types */
    bool on_equilibrium_path;         /**< Whether signal is on equilibrium path */
} nimcp_receiver_beliefs_t;

//=============================================================================
// Mechanism Structures
//=============================================================================

/**
 * @brief Allocation rule callback
 *
 * WHAT: Maps type reports to allocations
 * WHY:  Core component of mechanism design
 * HOW:  Called with reported types, returns allocation for each player
 *
 * @param reports Array of type indices reported by each player
 * @param num_players Number of players
 * @param allocations Output: allocation for each player
 * @param user_data User-provided context
 * @return NIMCP_SUCCESS or error code
 */
typedef nimcp_error_t (*nimcp_allocation_rule_fn)(
    const uint32_t* reports,
    uint32_t num_players,
    float* allocations,
    void* user_data
);

/**
 * @brief Payment rule callback
 *
 * WHAT: Maps type reports to payments
 * WHY:  Provides incentives for truthful revelation
 * HOW:  Called with reported types, returns payment for each player
 *
 * @param reports Array of type indices reported by each player
 * @param num_players Number of players
 * @param allocations Computed allocations
 * @param payments Output: payment for each player (negative = receives money)
 * @param user_data User-provided context
 * @return NIMCP_SUCCESS or error code
 */
typedef nimcp_error_t (*nimcp_payment_rule_fn)(
    const uint32_t* reports,
    uint32_t num_players,
    const float* allocations,
    float* payments,
    void* user_data
);

/**
 * @brief Mechanism configuration
 */
typedef struct {
    nimcp_mechanism_type_t type;      /**< Mechanism type */
    uint32_t num_players;             /**< Number of players */
    nimcp_ic_level_t target_ic;       /**< Target IC level */
    float convergence_epsilon;        /**< Convergence threshold */
    uint32_t max_iterations;          /**< Max equilibrium iterations */
    bool verify_ic;                   /**< Verify IC after solving? */
    bool verify_ir;                   /**< Verify IR after solving? */
    bool enable_statistics;           /**< Track statistics? */
} nimcp_mechanism_config_t;

/**
 * @brief Incentive compatibility verification result
 */
typedef struct {
    bool is_incentive_compatible;     /**< Overall IC status */
    nimcp_ic_level_t ic_level;        /**< Achieved IC level */
    float max_deviation_gain;         /**< Max gain from deviating */
    nimcp_player_id_t violator_id;    /**< Player who gains most from lying */
    uint32_t true_type;               /**< Type that gains from lying */
    uint32_t profitable_lie;          /**< Profitable false report */
    char explanation[256];            /**< Human-readable explanation */
} nimcp_ic_result_t;

/**
 * @brief Individual rationality verification result
 */
typedef struct {
    bool is_individually_rational;    /**< Overall IR status */
    float min_expected_utility;       /**< Minimum expected utility */
    nimcp_player_id_t violator_id;    /**< Player with IR violation */
    uint32_t violating_type;          /**< Type that violates IR */
    float utility_shortfall;          /**< How much below reservation */
    char explanation[256];            /**< Human-readable explanation */
} nimcp_ir_result_t;

/**
 * @brief Bayesian Nash equilibrium result
 */
typedef struct {
    bool equilibrium_found;           /**< Whether equilibrium was found */
    nimcp_signaling_strategy_t strategies[NIMCP_GT_MAX_PLAYERS * NIMCP_GT_MAX_TYPES];
    uint32_t num_strategies;          /**< Number of strategies in profile */
    float expected_social_welfare;    /**< Expected total welfare */
    float expected_revenue;           /**< Expected mechanism revenue */
    uint32_t iterations_taken;        /**< Iterations to converge */
    float convergence_error;          /**< Final convergence error */
    char explanation[256];            /**< Human-readable explanation */
} nimcp_bayesian_equilibrium_t;

/**
 * @brief Signaling equilibrium result
 */
typedef struct {
    nimcp_signal_equilibrium_t type;  /**< Equilibrium type */
    bool equilibrium_found;           /**< Whether equilibrium was found */
    nimcp_signaling_strategy_t sender_strategies[NIMCP_GT_MAX_TYPES];
    uint32_t num_sender_types;        /**< Number of sender types */
    nimcp_receiver_beliefs_t receiver_beliefs[NIMCP_GT_MAX_SIGNALS];
    uint32_t num_signals;             /**< Number of signals */
    float expected_sender_payoff;     /**< Expected sender payoff */
    float expected_receiver_payoff;   /**< Expected receiver payoff */
    float information_transmitted;    /**< Bits of information revealed */
    char explanation[256];            /**< Human-readable explanation */
} nimcp_signal_equilibrium_result_t;

/**
 * @brief Revelation principle verification result
 */
typedef struct {
    bool revelation_holds;            /**< Whether equivalent direct mechanism exists */
    nimcp_ic_level_t equivalent_ic;   /**< IC level of equivalent mechanism */
    float efficiency_loss;            /**< Efficiency loss in indirect mechanism */
    char explanation[256];            /**< Human-readable explanation */
} nimcp_revelation_result_t;

/**
 * @brief Mechanism outcome
 */
typedef struct {
    nimcp_mechanism_state_t state;    /**< Current mechanism state */
    float allocations[NIMCP_GT_MAX_PLAYERS]; /**< Final allocations */
    float payments[NIMCP_GT_MAX_PLAYERS];    /**< Final payments */
    float utilities[NIMCP_GT_MAX_PLAYERS];   /**< Final utilities */
    float social_welfare;             /**< Total welfare */
    float revenue;                    /**< Mechanism revenue */
    uint64_t timestamp_ms;            /**< Completion timestamp */
    bool is_efficient;                /**< Pareto efficient? */
} nimcp_mechanism_result_t;

/**
 * @brief Opaque mechanism handle
 */
typedef struct nimcp_mechanism_struct* nimcp_mechanism_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default mechanism configuration
 *
 * @return Default configuration with safe defaults
 */
nimcp_mechanism_config_t nimcp_mechanism_default_config(void);

/**
 * @brief Create mechanism context
 *
 * WHAT: Allocate and initialize mechanism design context
 * WHY:  Required for all mechanism operations
 * HOW:  Allocates internal state, initializes type spaces
 *
 * @param config Mechanism configuration
 * @return Mechanism handle or NULL on failure
 */
nimcp_mechanism_t nimcp_mechanism_create(const nimcp_mechanism_config_t* config);

/**
 * @brief Destroy mechanism context
 *
 * WHAT: Free all mechanism resources
 * WHY:  Prevent memory leaks
 * HOW:  Frees type spaces, callbacks, internal state
 *
 * @param ctx Mechanism context to destroy
 */
void nimcp_mechanism_destroy(nimcp_mechanism_t ctx);

//=============================================================================
// Type Space Configuration
//=============================================================================

/**
 * @brief Set type space for a player
 *
 * WHAT: Define the set of possible types and prior distribution
 * WHY:  Required for Bayesian game analysis
 * HOW:  Stores types and probabilities internally
 *
 * @param ctx Mechanism context
 * @param player Player ID
 * @param types Array of types
 * @param probabilities Prior probabilities (must sum to 1)
 * @param num_types Number of types
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_set_type_space(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    const nimcp_type_t* types,
    const float* probabilities,
    uint32_t num_types
);

/**
 * @brief Get type space for a player
 *
 * @param ctx Mechanism context
 * @param player Player ID
 * @param type_space Output: player's type space
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_get_type_space(
    const nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    nimcp_type_space_t* type_space
);

/**
 * @brief Realize a player's type (draw from prior)
 *
 * WHAT: Draw a type according to prior distribution
 * WHY:  Simulate Bayesian game play
 * HOW:  Random draw, stores realized type
 *
 * @param ctx Mechanism context
 * @param player Player ID
 * @param realized_type Output: index of realized type
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_realize_type(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    uint32_t* realized_type
);

//=============================================================================
// Mechanism Rules
//=============================================================================

/**
 * @brief Set allocation rule
 *
 * WHAT: Define how reports map to allocations
 * WHY:  Core component of mechanism
 * HOW:  Stores callback for later use
 *
 * @param ctx Mechanism context
 * @param callback Allocation rule function
 * @param user_data User context passed to callback
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_set_allocation_rule(
    nimcp_mechanism_t ctx,
    nimcp_allocation_rule_fn callback,
    void* user_data
);

/**
 * @brief Set payment rule
 *
 * WHAT: Define how reports map to payments
 * WHY:  Provides incentives for truthful revelation
 * HOW:  Stores callback for later use
 *
 * @param ctx Mechanism context
 * @param callback Payment rule function
 * @param user_data User context passed to callback
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_set_payment_rule(
    nimcp_mechanism_t ctx,
    nimcp_payment_rule_fn callback,
    void* user_data
);

//=============================================================================
// Verification Functions
//=============================================================================

/**
 * @brief Verify incentive compatibility
 *
 * WHAT: Check if truth-telling is optimal
 * WHY:  Core property for mechanism design
 * HOW:  Compare expected utility of truth vs lying for all type profiles
 *
 * DOMINANT STRATEGY IC: Truth optimal regardless of others
 * BAYESIAN IC: Truth optimal in expectation over other types
 *
 * @param ctx Mechanism context
 * @param result Output: IC verification result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_is_incentive_compatible(
    nimcp_mechanism_t ctx,
    nimcp_ic_result_t* result
);

/**
 * @brief Verify individual rationality
 *
 * WHAT: Check if participation is voluntary
 * WHY:  Players must prefer participating to opting out
 * HOW:  Compare expected utility vs outside option (typically 0)
 *
 * @param ctx Mechanism context
 * @param result Output: IR verification result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_is_individually_rational(
    nimcp_mechanism_t ctx,
    nimcp_ir_result_t* result
);

/**
 * @brief Verify revelation principle
 *
 * WHAT: Check if indirect mechanism can be replaced by direct mechanism
 * WHY:  Revelation principle simplifies mechanism design
 * HOW:  Construct equivalent direct mechanism, verify IC
 *
 * @param ctx Mechanism context (direct mechanism)
 * @param indirect_mechanism Indirect mechanism to compare
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_verify_revelation_principle(
    nimcp_mechanism_t ctx,
    const nimcp_mechanism_t indirect_mechanism,
    nimcp_revelation_result_t* result
);

//=============================================================================
// Equilibrium Computation
//=============================================================================

/**
 * @brief Compute Bayesian Nash equilibrium
 *
 * WHAT: Find strategy profile where each type best-responds
 * WHY:  Solution concept for incomplete information games
 * HOW:  Iterative best-response with belief updating
 *
 * @param ctx Mechanism context
 * @param result Output: Bayesian equilibrium result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_compute_bayesian_equilibrium(
    nimcp_mechanism_t ctx,
    nimcp_bayesian_equilibrium_t* result
);

/**
 * @brief Execute mechanism with type reports
 *
 * WHAT: Run mechanism with given type reports
 * WHY:  Compute allocations and payments
 * HOW:  Apply allocation rule, then payment rule
 *
 * @param ctx Mechanism context
 * @param reports Type reports from each player
 * @param result Output: mechanism outcome
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_mechanism_execute(
    nimcp_mechanism_t ctx,
    const uint32_t* reports,
    nimcp_mechanism_result_t* result
);

//=============================================================================
// Signaling Game Functions
//=============================================================================

/**
 * @brief Set sender types for signaling game
 *
 * WHAT: Configure sender's type space
 * WHY:  Define incomplete information for sender
 * HOW:  Stores types with prior distribution
 *
 * @param ctx Mechanism context (must be NIMCP_MECHANISM_SIGNALING)
 * @param types Array of sender types
 * @param num_types Number of types
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_signaling_set_sender_types(
    nimcp_mechanism_t ctx,
    const nimcp_type_t* types,
    uint32_t num_types
);

/**
 * @brief Set available signals for signaling game
 *
 * WHAT: Configure signal space
 * WHY:  Define sender's action space
 * HOW:  Stores signals with costs
 *
 * @param ctx Mechanism context
 * @param signals Array of signals
 * @param num_signals Number of signals
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_signaling_set_signals(
    nimcp_mechanism_t ctx,
    const nimcp_signal_t* signals,
    uint32_t num_signals
);

/**
 * @brief Compute separating equilibrium
 *
 * WHAT: Find equilibrium where types fully separate
 * WHY:  Full information revelation
 * HOW:  Each type sends distinct signal, receiver infers type perfectly
 *
 * SEPARATING EQUILIBRIUM CONDITIONS:
 * 1. Sender strategy is one-to-one (different types, different signals)
 * 2. Receiver correctly infers type from signal
 * 3. No type wants to imitate another
 *
 * @param ctx Mechanism context
 * @param result Output: separating equilibrium result
 * @return NIMCP_SUCCESS or NIMCP_GT_ERROR_NO_EQUILIBRIUM
 */
nimcp_error_t nimcp_signaling_compute_separating_equilibrium(
    nimcp_mechanism_t ctx,
    nimcp_signal_equilibrium_result_t* result
);

/**
 * @brief Compute pooling equilibrium
 *
 * WHAT: Find equilibrium where all types pool
 * WHY:  Minimal information revelation
 * HOW:  All types send same signal, receiver uses prior beliefs
 *
 * @param ctx Mechanism context
 * @param result Output: pooling equilibrium result
 * @return NIMCP_SUCCESS or NIMCP_GT_ERROR_NO_EQUILIBRIUM
 */
nimcp_error_t nimcp_signaling_compute_pooling_equilibrium(
    nimcp_mechanism_t ctx,
    nimcp_signal_equilibrium_result_t* result
);

/**
 * @brief Compute all equilibria
 *
 * WHAT: Find all signaling equilibria (separating, pooling, semi-separating)
 * WHY:  Complete equilibrium analysis
 * HOW:  Iterate over equilibrium types
 *
 * @param ctx Mechanism context
 * @param results Output: array of equilibrium results
 * @param max_results Maximum results to return
 * @param num_found Output: number of equilibria found
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_signaling_compute_all_equilibria(
    nimcp_mechanism_t ctx,
    nimcp_signal_equilibrium_result_t* results,
    uint32_t max_results,
    uint32_t* num_found
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get mechanism state
 */
nimcp_mechanism_state_t nimcp_mechanism_get_state(const nimcp_mechanism_t ctx);

/**
 * @brief Get mechanism type
 */
nimcp_mechanism_type_t nimcp_mechanism_get_type(const nimcp_mechanism_t ctx);

/**
 * @brief Get mechanism type name
 */
const char* nimcp_mechanism_type_name(nimcp_mechanism_type_t type);

/**
 * @brief Get IC level name
 */
const char* nimcp_ic_level_name(nimcp_ic_level_t level);

/**
 * @brief Get signaling equilibrium type name
 */
const char* nimcp_signal_equilibrium_name(nimcp_signal_equilibrium_t type);

/**
 * @brief Compute expected utility for a player-type
 *
 * WHAT: Calculate expected utility given beliefs about other types
 * WHY:  Core calculation for IC verification and equilibrium
 * HOW:  Sum over opponent type profiles weighted by probability
 *
 * @param ctx Mechanism context
 * @param player Player ID
 * @param type_idx Player's type index
 * @param report What the player reports
 * @return Expected utility
 */
float nimcp_mechanism_expected_utility(
    nimcp_mechanism_t ctx,
    nimcp_player_id_t player,
    uint32_t type_idx,
    uint32_t report
);

/**
 * @brief Compute information content of signaling
 *
 * WHAT: Measure how much information is revealed
 * WHY:  Quantify separation between types
 * HOW:  Mutual information between type and signal
 *
 * @param result Signaling equilibrium result
 * @return Information in bits
 */
float nimcp_signaling_information_content(
    const nimcp_signal_equilibrium_result_t* result
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Initialize type structure
 */
void nimcp_type_init(nimcp_type_t* type, uint32_t type_id, float valuation);

/**
 * @brief Initialize type space structure
 */
void nimcp_type_space_init(nimcp_type_space_t* space, nimcp_player_id_t player_id);

/**
 * @brief Initialize signal structure
 */
void nimcp_signal_init(nimcp_signal_t* signal, uint32_t signal_id,
                        const char* name, float cost);

/**
 * @brief Initialize mechanism result structure
 */
void nimcp_mechanism_result_init(nimcp_mechanism_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_MECHANISM_H
