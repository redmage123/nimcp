//=============================================================================
// nimcp_gt_equilibrium.h - N-Player Nash Equilibrium Solver
//=============================================================================
/**
 * @file nimcp_gt_equilibrium.h
 * @brief N-Player Nash equilibrium computation for general-sum games
 *
 * WHAT: Algorithms for finding Nash equilibria in N-player games
 * WHY:  Enable strategic reasoning for multi-agent coordination
 * HOW:  Best-response iteration, support enumeration, Lemke-Howson
 *
 * BIOLOGICAL INSPIRATION:
 * - Neural population equilibria (attractor states)
 * - Competitive resource allocation in brain regions
 * - Hormonal feedback loops reaching homeostatic equilibrium
 * - Synaptic weight stabilization through competitive Hebbian learning
 *
 * INTEGRATION: Hemispheric Brain (HEMISPHERIC_MODE_COMPETITION)
 *
 * ALGORITHMS:
 * 1. Pure Strategy Nash: Best-response iteration (O(n * |S|^n))
 * 2. Mixed Strategy Nash: Support enumeration for small games
 * 3. Lemke-Howson: Efficient 2-player bimatrix game solver
 *
 * ERROR CODE RANGE: 25000-25999 (shared with game theory module)
 * BIO-ASYNC MODULE ID: BIO_MODULE_GT_EQUILIBRIUM (0x1503)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_EQUILIBRIUM_H
#define NIMCP_GT_EQUILIBRIUM_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum strategies per player (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_STRATEGIES 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_STRATEGIES 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_STRATEGIES 16
#else
    #define NIMCP_GT_MAX_STRATEGIES 8
#endif

/** Maximum equilibria to find (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_EQUILIBRIA 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_EQUILIBRIA 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_EQUILIBRIA 8
#else
    #define NIMCP_GT_MAX_EQUILIBRIA 4
#endif

/** Support enumeration threshold (max strategies for full enumeration) */
#define NIMCP_GT_SUPPORT_ENUM_THRESHOLD 6

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Algorithm type for equilibrium computation
 */
typedef enum {
    NIMCP_EQUILIBRIUM_ALGO_AUTO,            /**< Auto-select based on game structure */
    NIMCP_EQUILIBRIUM_ALGO_BEST_RESPONSE,   /**< Best-response iteration (pure NE) */
    NIMCP_EQUILIBRIUM_ALGO_SUPPORT_ENUM,    /**< Support enumeration (mixed NE) */
    NIMCP_EQUILIBRIUM_ALGO_LEMKE_HOWSON,    /**< Lemke-Howson (2-player only) */
    NIMCP_EQUILIBRIUM_ALGO_FICTITIOUS_PLAY, /**< Fictitious play (approximate) */
    NIMCP_EQUILIBRIUM_ALGO_COUNT
} nimcp_equilibrium_algo_t;

/**
 * @brief Equilibrium type classification
 */
typedef enum {
    NIMCP_EQUILIBRIUM_TYPE_PURE,            /**< Pure strategy Nash equilibrium */
    NIMCP_EQUILIBRIUM_TYPE_MIXED,           /**< Mixed strategy Nash equilibrium */
    NIMCP_EQUILIBRIUM_TYPE_CORRELATED,      /**< Correlated equilibrium */
    NIMCP_EQUILIBRIUM_TYPE_APPROXIMATE      /**< Approximate equilibrium (epsilon-NE) */
} nimcp_equilibrium_type_t;

/**
 * @brief Convergence status
 */
typedef enum {
    NIMCP_CONVERGENCE_NOT_STARTED,
    NIMCP_CONVERGENCE_IN_PROGRESS,
    NIMCP_CONVERGENCE_CONVERGED,
    NIMCP_CONVERGENCE_FAILED,
    NIMCP_CONVERGENCE_MAX_ITERATIONS
} nimcp_convergence_status_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Game matrix for payoff representation
 *
 * WHAT: N-dimensional payoff tensor represented as flat array
 * WHY:  Efficient storage and access for N-player games
 * HOW:  Flattened tensor with stride-based indexing
 *
 * INDEXING (2-player example):
 *   payoff[i][j] = data[i * strides[0] + j * strides[1]]
 *
 * N-PLAYER INDEXING:
 *   payoff[s0][s1]...[sN] = data[sum_k(s_k * strides[k])]
 */
typedef struct {
    float* data;                            /**< Flattened payoff data */
    uint32_t num_players;                   /**< Number of players */
    uint32_t num_strategies[NIMCP_GT_MAX_PLAYERS]; /**< Strategies per player */
    uint32_t strides[NIMCP_GT_MAX_PLAYERS]; /**< Index strides for each dimension */
    uint32_t total_cells;                   /**< Total number of cells */
} nimcp_game_matrix_t;

/**
 * @brief Strategy profile (strategy for each player)
 *
 * WHAT: A complete specification of strategies for all players
 * WHY:  Represent game state or equilibrium outcome
 * HOW:  Pure strategies as indices, mixed as probability distributions
 */
typedef struct {
    nimcp_strategy_type_t type;             /**< Pure or mixed */
    uint32_t num_players;                   /**< Number of players */

    // Pure strategy representation
    uint32_t pure_strategies[NIMCP_GT_MAX_PLAYERS]; /**< Strategy index per player */

    // Mixed strategy representation (probabilities over strategies)
    float* mixed_strategies[NIMCP_GT_MAX_PLAYERS];  /**< Probability distributions */
    uint32_t num_strategies[NIMCP_GT_MAX_PLAYERS];  /**< Number of strategies per player */
} nimcp_strategy_profile_t;

/**
 * @brief Equilibrium result
 *
 * WHAT: Complete description of a Nash equilibrium
 * WHY:  Return equilibrium strategies and associated payoffs
 * HOW:  Strategy profile plus expected payoffs and verification
 */
typedef struct {
    nimcp_equilibrium_type_t type;          /**< Type of equilibrium */
    nimcp_strategy_profile_t strategies;    /**< Equilibrium strategy profile */
    float payoffs[NIMCP_GT_MAX_PLAYERS];    /**< Expected payoffs at equilibrium */
    float epsilon;                          /**< Approximation error (0 for exact) */
    bool is_verified;                       /**< Has been verified as NE */
    bool is_pareto_optimal;                 /**< Is Pareto optimal? */
    float social_welfare;                   /**< Sum of payoffs */
    uint32_t support_sizes[NIMCP_GT_MAX_PLAYERS]; /**< Size of support for mixed NE */
} nimcp_equilibrium_result_t;

/**
 * @brief Equilibrium solver configuration
 */
typedef struct {
    nimcp_equilibrium_algo_t algorithm;     /**< Algorithm to use */
    uint32_t num_players;                   /**< Number of players in game */
    uint32_t num_strategies[NIMCP_GT_MAX_PLAYERS]; /**< Strategies per player */

    // Convergence parameters
    uint32_t max_iterations;                /**< Maximum iterations */
    float convergence_epsilon;              /**< Convergence threshold */
    float nash_epsilon;                     /**< Epsilon for epsilon-NE */

    // Algorithm-specific parameters
    float learning_rate;                    /**< Learning rate for iterative methods */
    bool find_all_equilibria;               /**< Find all NE (vs just one) */
    bool verify_equilibria;                 /**< Verify found equilibria */

    // Performance tuning
    uint64_t timeout_ms;                    /**< Computation timeout (0=none) */
    bool enable_early_termination;          /**< Stop on first equilibrium */
} nimcp_equilibrium_config_t;

/**
 * @brief Convergence statistics
 */
typedef struct {
    uint32_t iterations_completed;          /**< Iterations executed */
    float final_residual;                   /**< Final convergence residual */
    nimcp_convergence_status_t status;      /**< Convergence status */
    uint64_t compute_time_ms;               /**< Computation time */
    uint32_t equilibria_found;              /**< Number of equilibria found */
} nimcp_equilibrium_stats_t;

/**
 * @brief Opaque equilibrium solver handle
 */
typedef struct nimcp_equilibrium_struct* nimcp_equilibrium_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default equilibrium configuration
 *
 * @param num_players Number of players in the game
 * @param strategies_per_player Strategies for each player (NULL for uniform 2)
 * @return Default configuration
 */
nimcp_equilibrium_config_t nimcp_equilibrium_default_config(
    uint32_t num_players,
    const uint32_t* strategies_per_player
);

/**
 * @brief Create equilibrium solver context
 *
 * @param config Solver configuration
 * @return Solver handle or NULL on failure
 */
nimcp_equilibrium_t nimcp_equilibrium_create(const nimcp_equilibrium_config_t* config);

/**
 * @brief Destroy equilibrium solver context
 *
 * @param ctx Solver handle
 */
void nimcp_equilibrium_destroy(nimcp_equilibrium_t ctx);

//=============================================================================
// Game Setup Functions
//=============================================================================

/**
 * @brief Set payoff matrix for a player
 *
 * WHAT: Define payoffs for one player across all strategy combinations
 * WHY:  Build the game structure before solving
 * HOW:  Copy flattened payoff matrix to internal storage
 *
 * PAYOFF ORDERING (row-major):
 *   For 2 players with strategies (m, n):
 *   payoffs[i * n + j] = payoff when player 0 plays i, player 1 plays j
 *
 * @param ctx Solver handle
 * @param player Player index (0 to num_players-1)
 * @param payoffs Flattened payoff array
 * @param size Size of payoff array (must match total strategy combinations)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_set_payoffs(
    nimcp_equilibrium_t ctx,
    uint32_t player,
    const float* payoffs,
    uint32_t size
);

/**
 * @brief Set game matrix from bimatrix representation (2-player only)
 *
 * WHAT: Convenience function for 2-player games
 * WHY:  Common case deserves simple API
 * HOW:  Set both player payoffs from row/column matrices
 *
 * @param ctx Solver handle
 * @param row_payoffs Row player payoffs (m x n matrix, row-major)
 * @param col_payoffs Column player payoffs (m x n matrix, row-major)
 * @param rows Number of row strategies (m)
 * @param cols Number of column strategies (n)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_set_bimatrix(
    nimcp_equilibrium_t ctx,
    const float* row_payoffs,
    const float* col_payoffs,
    uint32_t rows,
    uint32_t cols
);

/**
 * @brief Create game matrix structure
 *
 * @param num_players Number of players
 * @param strategies_per_player Array of strategy counts
 * @return Allocated game matrix or NULL
 */
nimcp_game_matrix_t* nimcp_game_matrix_create(
    uint32_t num_players,
    const uint32_t* strategies_per_player
);

/**
 * @brief Destroy game matrix
 *
 * @param matrix Matrix to destroy
 */
void nimcp_game_matrix_destroy(nimcp_game_matrix_t* matrix);

/**
 * @brief Get payoff from game matrix
 *
 * @param matrix Game matrix
 * @param strategy_profile Strategy indices for each player
 * @return Payoff value
 */
float nimcp_game_matrix_get(
    const nimcp_game_matrix_t* matrix,
    const uint32_t* strategy_profile
);

/**
 * @brief Set payoff in game matrix
 *
 * @param matrix Game matrix
 * @param strategy_profile Strategy indices for each player
 * @param payoff Payoff value to set
 */
void nimcp_game_matrix_set(
    nimcp_game_matrix_t* matrix,
    const uint32_t* strategy_profile,
    float payoff
);

//=============================================================================
// Equilibrium Finding Functions
//=============================================================================

/**
 * @brief Find pure strategy Nash equilibrium
 *
 * WHAT: Search for pure strategy Nash equilibria via best-response iteration
 * WHY:  Pure equilibria are simpler and often more interpretable
 * HOW:  Iterate through strategy profiles, check if all players best-respond
 *
 * ALGORITHM:
 * 1. For each strategy profile s = (s_1, ..., s_n):
 *    a. For each player i, compute best response BR_i(s_{-i})
 *    b. If s_i = BR_i(s_{-i}) for all i, s is a Nash equilibrium
 *
 * TIME COMPLEXITY: O(n * |S|^n) where n = players, |S| = max strategies
 *
 * @param ctx Solver handle
 * @param result Output equilibrium result
 * @return NIMCP_SUCCESS if found, NIMCP_GT_ERROR_NO_EQUILIBRIUM if none exists
 */
nimcp_error_t nimcp_equilibrium_find_pure_nash(
    nimcp_equilibrium_t ctx,
    nimcp_equilibrium_result_t* result
);

/**
 * @brief Find mixed strategy Nash equilibrium
 *
 * WHAT: Compute mixed strategy Nash equilibrium
 * WHY:  Every finite game has at least one mixed NE (Nash's theorem)
 * HOW:  Support enumeration for small games, Lemke-Howson for 2-player
 *
 * ALGORITHM (Support Enumeration):
 * 1. For each support combination (S_1, ..., S_n):
 *    a. Solve indifference conditions for mixing probabilities
 *    b. Check if solution is valid (probabilities in [0,1], sum to 1)
 *    c. Verify no profitable deviation outside support
 *
 * @param ctx Solver handle
 * @param result Output equilibrium result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_find_mixed_nash(
    nimcp_equilibrium_t ctx,
    nimcp_equilibrium_result_t* result
);

/**
 * @brief Find all Nash equilibria
 *
 * WHAT: Exhaustive search for all equilibria (pure and mixed)
 * WHY:  Games may have multiple equilibria with different properties
 * HOW:  Enumerate all possible support combinations
 *
 * @param ctx Solver handle
 * @param results Output array for equilibria
 * @param max_results Maximum results to return
 * @param num_found Output: number of equilibria found
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_find_all(
    nimcp_equilibrium_t ctx,
    nimcp_equilibrium_result_t* results,
    uint32_t max_results,
    uint32_t* num_found
);

/**
 * @brief Use Lemke-Howson algorithm for 2-player bimatrix games
 *
 * WHAT: Efficient algorithm for 2-player games
 * WHY:  Polynomial path-following algorithm, finds one equilibrium
 * HOW:  Linear complementarity problem (LCP) formulation
 *
 * ALGORITHM:
 * 1. Convert bimatrix game to LCP
 * 2. Start from artificial equilibrium (pure strategy corner)
 * 3. Follow complementary pivot path until genuine equilibrium
 *
 * TIME COMPLEXITY: Exponential worst case, fast in practice
 *
 * NOTE: Only works for 2-player games. Returns error for N > 2.
 *
 * @param ctx Solver handle (must be 2-player game)
 * @param result Output equilibrium result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_lemke_howson(
    nimcp_equilibrium_t ctx,
    nimcp_equilibrium_result_t* result
);

//=============================================================================
// Best Response and Verification Functions
//=============================================================================

/**
 * @brief Compute best response for a player
 *
 * WHAT: Find optimal strategy given other players' strategies
 * WHY:  Core subroutine for equilibrium computation and verification
 * HOW:  Maximize expected payoff over own strategies
 *
 * @param ctx Solver handle
 * @param player Player index
 * @param opponent_strategies Strategy profile for other players
 * @param best_strategy Output: best response strategy index (pure)
 * @param best_payoff Output: expected payoff from best response (optional, can be NULL)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_best_response(
    nimcp_equilibrium_t ctx,
    uint32_t player,
    const nimcp_strategy_profile_t* opponent_strategies,
    uint32_t* best_strategy,
    float* best_payoff
);

/**
 * @brief Compute best response mixed strategy
 *
 * WHAT: Find optimal mixed strategy given opponents' mixed strategies
 * WHY:  Needed for mixed NE verification
 * HOW:  Find all strategies that achieve max expected payoff
 *
 * @param ctx Solver handle
 * @param player Player index
 * @param opponent_strategies Mixed strategy profile for other players
 * @param best_response Output: best response mixed strategy (caller allocates)
 * @param best_payoff Output: expected payoff
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_best_response_mixed(
    nimcp_equilibrium_t ctx,
    uint32_t player,
    const nimcp_strategy_profile_t* opponent_strategies,
    float* best_response,
    float* best_payoff
);

/**
 * @brief Verify if strategy profile is a Nash equilibrium
 *
 * WHAT: Check if no player has a profitable deviation
 * WHY:  Validate computed equilibria
 * HOW:  Compare current payoff with best response payoff for each player
 *
 * @param ctx Solver handle
 * @param strategies Strategy profile to verify
 * @param epsilon Tolerance for approximate equilibrium (0 for exact)
 * @return true if Nash equilibrium, false otherwise
 */
bool nimcp_equilibrium_is_nash(
    nimcp_equilibrium_t ctx,
    const nimcp_strategy_profile_t* strategies,
    float epsilon
);

/**
 * @brief Compute regret for each player
 *
 * WHAT: Measure how much each player could gain by deviating
 * WHY:  Quantify distance from equilibrium
 * HOW:  regret_i = max_{s_i} u_i(s_i, s_{-i}) - u_i(s)
 *
 * @param ctx Solver handle
 * @param strategies Strategy profile
 * @param regrets Output: regret for each player
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_compute_regret(
    nimcp_equilibrium_t ctx,
    const nimcp_strategy_profile_t* strategies,
    float* regrets
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute expected payoff for strategy profile
 *
 * @param ctx Solver handle
 * @param player Player index
 * @param strategies Strategy profile (pure or mixed)
 * @return Expected payoff
 */
float nimcp_equilibrium_expected_payoff(
    nimcp_equilibrium_t ctx,
    uint32_t player,
    const nimcp_strategy_profile_t* strategies
);

/**
 * @brief Get computation statistics
 *
 * @param ctx Solver handle
 * @param stats Output statistics
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_get_stats(
    const nimcp_equilibrium_t ctx,
    nimcp_equilibrium_stats_t* stats
);

/**
 * @brief Reset solver for new game
 *
 * @param ctx Solver handle
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_equilibrium_reset(nimcp_equilibrium_t ctx);

/**
 * @brief Get algorithm name
 *
 * @param algo Algorithm type
 * @return Algorithm name string
 */
const char* nimcp_equilibrium_algo_name(nimcp_equilibrium_algo_t algo);

/**
 * @brief Get equilibrium type name
 *
 * @param type Equilibrium type
 * @return Type name string
 */
const char* nimcp_equilibrium_type_name(nimcp_equilibrium_type_t type);

//=============================================================================
// Strategy Profile Helpers
//=============================================================================

/**
 * @brief Initialize strategy profile for pure strategies
 *
 * @param profile Profile to initialize
 * @param num_players Number of players
 * @param strategies Pure strategy index for each player
 */
void nimcp_strategy_profile_init_pure(
    nimcp_strategy_profile_t* profile,
    uint32_t num_players,
    const uint32_t* strategies
);

/**
 * @brief Initialize strategy profile for mixed strategies
 *
 * @param profile Profile to initialize
 * @param num_players Number of players
 * @param num_strategies Number of strategies per player
 * @return NIMCP_SUCCESS or error (allocates memory for mixed strategies)
 */
nimcp_error_t nimcp_strategy_profile_init_mixed(
    nimcp_strategy_profile_t* profile,
    uint32_t num_players,
    const uint32_t* num_strategies
);

/**
 * @brief Cleanup strategy profile (free mixed strategy arrays)
 *
 * @param profile Profile to cleanup
 */
void nimcp_strategy_profile_cleanup(nimcp_strategy_profile_t* profile);

/**
 * @brief Copy strategy profile
 *
 * @param dest Destination profile
 * @param src Source profile
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_strategy_profile_copy(
    nimcp_strategy_profile_t* dest,
    const nimcp_strategy_profile_t* src
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_EQUILIBRIUM_H
