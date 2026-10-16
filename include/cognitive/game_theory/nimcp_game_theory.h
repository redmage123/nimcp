//=============================================================================
// nimcp_game_theory.h - Game Theory Primitives for Strategic Interactions
//=============================================================================
/**
 * @file nimcp_game_theory.h
 * @brief Game theory primitives for strategic interactions in NIMCP
 *
 * WHAT: Core game-theoretic structures and common types
 * WHY:  Enable strategic reasoning for resource allocation and coordination
 * HOW:  Unified framework for auctions, bargaining, mechanism design
 *
 * BIOLOGICAL INSPIRATION:
 * - Competitive foraging as resource auction
 * - Neural coalition formation (binding problem)
 * - Synaptic competition for resources
 * - Hormonal signaling as incentive mechanisms
 *
 * ERROR CODE RANGE: 25000-25999 (Module-specific)
 * BIO-ASYNC MODULE ID: 0x1500-0x150F
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GAME_THEORY_H
#define NIMCP_GAME_THEORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_tier_optimization.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Codes (Range: 25000-25999)
//=============================================================================

#define NIMCP_GT_ERROR_BASE                 25000
#define NIMCP_GT_ERROR_NULL_POINTER         (NIMCP_GT_ERROR_BASE + 1)
#define NIMCP_GT_ERROR_INVALID_PARAMETER    (NIMCP_GT_ERROR_BASE + 2)
#define NIMCP_GT_ERROR_NO_MEMORY            (NIMCP_GT_ERROR_BASE + 3)
#define NIMCP_GT_ERROR_NOT_INITIALIZED      (NIMCP_GT_ERROR_BASE + 4)
#define NIMCP_GT_ERROR_INVALID_STATE        (NIMCP_GT_ERROR_BASE + 5)
#define NIMCP_GT_ERROR_NO_EQUILIBRIUM       (NIMCP_GT_ERROR_BASE + 10)
#define NIMCP_GT_ERROR_CONVERGENCE_FAILED   (NIMCP_GT_ERROR_BASE + 11)
#define NIMCP_GT_ERROR_AUCTION_CLOSED       (NIMCP_GT_ERROR_BASE + 20)
#define NIMCP_GT_ERROR_INVALID_BID          (NIMCP_GT_ERROR_BASE + 21)
#define NIMCP_GT_ERROR_RESERVE_NOT_MET      (NIMCP_GT_ERROR_BASE + 22)
#define NIMCP_GT_ERROR_BARGAINING_FAILED    (NIMCP_GT_ERROR_BASE + 30)
#define NIMCP_GT_ERROR_DISAGREEMENT         (NIMCP_GT_ERROR_BASE + 31)
#define NIMCP_GT_ERROR_TIMEOUT              (NIMCP_GT_ERROR_BASE + 32)
#define NIMCP_GT_ERROR_MECHANISM_VIOLATION  (NIMCP_GT_ERROR_BASE + 40)
#define NIMCP_GT_ERROR_NOT_STRATEGYPROOF    (NIMCP_GT_ERROR_BASE + 41)
#define NIMCP_GT_ERROR_CAPACITY             (NIMCP_GT_ERROR_BASE + 50)
#define NIMCP_GT_ERROR_GAME_OVER            (NIMCP_GT_ERROR_BASE + 51)
#define NIMCP_GT_ERROR_BID_TOO_LOW          (NIMCP_GT_ERROR_BASE + 52)
#define NIMCP_GT_ERROR_BUDGET               (NIMCP_GT_ERROR_BASE + 53)
#define NIMCP_GT_ERROR_PLAYER_NOT_FOUND     (NIMCP_GT_ERROR_BASE + 54)

//=============================================================================
// Bio-Async Module IDs (Range: 0x1500-0x150F)
//=============================================================================

#define BIO_MODULE_GAME_THEORY_BASE         0x1500
#define BIO_MODULE_GT_AUCTION               (BIO_MODULE_GAME_THEORY_BASE + 0)
#define BIO_MODULE_GT_BARGAINING            (BIO_MODULE_GAME_THEORY_BASE + 1)
#define BIO_MODULE_GT_MECHANISM             (BIO_MODULE_GAME_THEORY_BASE + 2)
#define BIO_MODULE_GT_EQUILIBRIUM           (BIO_MODULE_GAME_THEORY_BASE + 3)
#define BIO_MODULE_GT_CREDIT                (BIO_MODULE_GAME_THEORY_BASE + 4)
#define BIO_MODULE_GT_GW_INTEGRATION        (BIO_MODULE_GAME_THEORY_BASE + 5)
#define BIO_MODULE_GT_HEMISPHERIC           (BIO_MODULE_GAME_THEORY_BASE + 6)
#define BIO_MODULE_GT_SWARM                 (BIO_MODULE_GAME_THEORY_BASE + 7)
#define BIO_MODULE_GT_NEUROMOD              (BIO_MODULE_GAME_THEORY_BASE + 8)
#define BIO_MODULE_GT_WM                    (BIO_MODULE_GAME_THEORY_BASE + 9)

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum players in a game (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_PLAYERS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_PLAYERS 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_PLAYERS 8
#else
    #define NIMCP_GT_MAX_PLAYERS 4
#endif

/** Maximum items in multi-item auction (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_AUCTION_ITEMS 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_AUCTION_ITEMS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_AUCTION_ITEMS 16
#else
    #define NIMCP_GT_MAX_AUCTION_ITEMS 8
#endif

/** Maximum iterations for equilibrium computation */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_ITERATIONS 1000
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_ITERATIONS 500
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_ITERATIONS 100
#else
    #define NIMCP_GT_MAX_ITERATIONS 50
#endif

/** History depth for game outcomes */
#define NIMCP_GT_HISTORY_DEPTH 64

/** Invalid player ID sentinel */
#define NIMCP_GT_INVALID_PLAYER ((nimcp_player_id_t)0xFFFFFFFF)

//=============================================================================
// Core Enumerations
//=============================================================================

/**
 * @brief Game types
 */
typedef enum {
    NIMCP_GAME_ZERO_SUM,              /**< Zero-sum competitive game */
    NIMCP_GAME_COOPERATIVE,           /**< Cooperative game (coalitions) */
    NIMCP_GAME_GENERAL_SUM,           /**< General-sum game */
    NIMCP_GAME_POTENTIAL,             /**< Potential game (has pure NE) */
    NIMCP_GAME_CONGESTION,            /**< Congestion game */
    NIMCP_GAME_AUCTION,               /**< Auction mechanism */
    NIMCP_GAME_BARGAINING,            /**< Bargaining game */
    NIMCP_GAME_COUNT
} nimcp_game_type_t;

/**
 * @brief Player/agent identifier (opaque, integrates with cognitive_module_t)
 */
typedef uint32_t nimcp_player_id_t;

/**
 * @brief Strategy type
 */
typedef enum {
    NIMCP_STRATEGY_PURE,              /**< Single action */
    NIMCP_STRATEGY_MIXED,             /**< Probability distribution */
    NIMCP_STRATEGY_DOMINANT,          /**< Dominant strategy */
    NIMCP_STRATEGY_ADAPTIVE           /**< Learning-based strategy */
} nimcp_strategy_type_t;

/**
 * @brief Solution concept
 */
typedef enum {
    NIMCP_SOLUTION_NASH,              /**< Nash equilibrium */
    NIMCP_SOLUTION_CORRELATED,        /**< Correlated equilibrium */
    NIMCP_SOLUTION_PARETO_OPTIMAL,    /**< Pareto optimality */
    NIMCP_SOLUTION_CORE,              /**< Core (cooperative) */
    NIMCP_SOLUTION_SHAPLEY,           /**< Shapley value */
    NIMCP_SOLUTION_NUCLEOLUS          /**< Nucleolus */
} nimcp_solution_concept_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Player/agent structure
 */
typedef struct {
    nimcp_player_id_t id;             /**< Unique identifier */
    char name[32];                    /**< Human-readable name */
    float* strategy;                  /**< Current strategy (mixed or pure) */
    uint32_t num_actions;             /**< Number of available actions */
    float payoff;                     /**< Current/expected payoff */
    float budget;                     /**< Resource budget (for auctions) */
    void* private_info;               /**< Private information (mechanism design) */
    size_t private_info_size;         /**< Size of private info */
} nimcp_player_t;

/**
 * @brief Game outcome
 */
typedef struct {
    nimcp_player_id_t winners[NIMCP_GT_MAX_PLAYERS]; /**< Winning player(s) */
    uint32_t num_winners;             /**< Number of winners */
    float allocations[NIMCP_GT_MAX_PLAYERS]; /**< Resource allocations per player */
    float payments[NIMCP_GT_MAX_PLAYERS];    /**< Payments per player */
    float social_welfare;             /**< Total social welfare */
    uint64_t timestamp_ms;            /**< When outcome was determined */
    bool is_efficient;                /**< Pareto efficient allocation? */
    bool is_fair;                     /**< Fair by fairness index? */
} nimcp_game_outcome_t;

/**
 * @brief Game statistics
 */
typedef struct {
    uint64_t games_played;
    uint64_t equilibria_found;
    uint64_t bargaining_successes;
    uint64_t bargaining_failures;
    uint64_t auctions_completed;
    float avg_convergence_iterations;
    float avg_social_welfare;
    float avg_fairness_index;         /**< Jain's fairness index */
    float avg_efficiency;             /**< Allocative efficiency */
} nimcp_game_stats_t;

/**
 * @brief Game theory system configuration
 */
typedef struct {
    uint32_t max_players;             /**< Maximum concurrent players */
    uint32_t max_iterations;          /**< Max iterations for solvers */
    float convergence_epsilon;        /**< Convergence threshold */
    bool enable_statistics;           /**< Track detailed statistics? */
    bool enable_history;              /**< Track game history? */
    uint32_t history_depth;           /**< How many games to remember */
} nimcp_gt_config_t;

/**
 * @brief Opaque game theory system handle
 */
typedef struct nimcp_gt_system_struct* nimcp_gt_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
nimcp_gt_config_t nimcp_gt_default_config(void);

/**
 * @brief Initialize game theory system
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 */
nimcp_gt_system_t nimcp_gt_create(const nimcp_gt_config_t* config);

/**
 * @brief Shutdown game theory system
 */
void nimcp_gt_destroy(nimcp_gt_system_t system);

/**
 * @brief Check if game theory system is initialized
 */
bool nimcp_gt_is_initialized(const nimcp_gt_system_t system);

/**
 * @brief Get game theory system statistics
 */
nimcp_error_t nimcp_gt_get_stats(const nimcp_gt_system_t system,
                                  nimcp_game_stats_t* stats);

/**
 * @brief Reset statistics
 */
void nimcp_gt_reset_stats(nimcp_gt_system_t system);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get game type name
 */
const char* nimcp_game_type_name(nimcp_game_type_t type);

/**
 * @brief Get solution concept name
 */
const char* nimcp_solution_concept_name(nimcp_solution_concept_t solution_concept);

/**
 * @brief Compute Jain's fairness index
 *
 * WHAT: Measure fairness of allocation
 * WHY:  Quantify how evenly resources are distributed
 * HOW:  J = (sum x_i)^2 / (n * sum x_i^2)
 *
 * @param allocations Array of allocations
 * @param n Number of allocations
 * @return Fairness index [0, 1] where 1 = perfectly fair
 */
float nimcp_compute_fairness_index(const float* allocations, uint32_t n);

/**
 * @brief Check if allocation is Pareto optimal
 *
 * @param utilities Current utilities
 * @param n Number of players
 * @param feasible_utilities Feasible utility profiles (flattened)
 * @param num_feasible Number of feasible profiles
 * @return true if Pareto optimal
 */
bool nimcp_is_pareto_optimal(const float* utilities, uint32_t n,
                              const float* feasible_utilities,
                              uint32_t num_feasible);

/**
 * @brief Initialize a player structure
 */
void nimcp_player_init(nimcp_player_t* player, nimcp_player_id_t id,
                        const char* name, uint32_t num_actions);

/**
 * @brief Cleanup a player structure
 */
void nimcp_player_cleanup(nimcp_player_t* player);

/**
 * @brief Initialize game outcome structure
 */
void nimcp_game_outcome_init(nimcp_game_outcome_t* outcome);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/**
 * @brief Select action according to mixed strategy using MC sampling
 *
 * WHAT: Sample action from probability distribution
 * WHY:  Realize mixed strategies in actual play
 * HOW:  Use MC importance sampling
 *
 * @param player Player with mixed strategy
 * @return Selected action index
 */
uint32_t nimcp_gt_sample_action_mc(const nimcp_player_t* player);

/**
 * @brief Estimate expected utility via Monte Carlo sampling
 *
 * WHAT: Compute expected payoff under mixed strategies
 * WHY:  Evaluate strategy quality when payoff matrix is stochastic
 * HOW:  Sample action profiles, average payoffs
 *
 * @param system Game theory system
 * @param players Array of players with strategies
 * @param num_players Number of players
 * @param payoff_fn Payoff function (action profile -> utilities)
 * @param num_samples Number of MC samples
 * @param expected_utilities Output: expected utility per player
 * @param user_data User data for payoff function
 * @return 0 on success
 */
int nimcp_gt_expected_utility_mc(
    nimcp_gt_system_t system,
    const nimcp_player_t* players,
    uint32_t num_players,
    float (*payoff_fn)(const uint32_t* actions, uint32_t num_players, void* user_data),
    uint32_t num_samples,
    float* expected_utilities,
    void* user_data
);

/**
 * @brief Update strategy via fictitious play with MC sampling
 *
 * WHAT: Update strategy using best response to opponent history
 * WHY:  Simple learning algorithm that converges to Nash in some games
 * HOW:  Track opponent action frequencies, compute best response with exploration noise
 *
 * @param player Player to update
 * @param opponent_frequencies Observed frequencies of opponent actions
 * @param num_opponent_actions Number of opponent actions
 * @param payoff_matrix Player's payoff matrix [my_action][opp_action]
 * @param learning_rate How fast to update (0-1)
 * @return 0 on success
 */
int nimcp_gt_fictitious_play_update_mc(
    nimcp_player_t* player,
    const float* opponent_frequencies,
    uint32_t num_opponent_actions,
    const float* payoff_matrix,
    float learning_rate
);

/**
 * @brief Get thread-local MC seed for game theory
 *
 * @return Pointer to thread-local seed
 */
uint32_t* nimcp_gt_get_mc_seed(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GAME_THEORY_H
