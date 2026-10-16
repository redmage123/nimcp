//=============================================================================
// nimcp_gt_coalition.h - Coalition Formation Dynamics
//=============================================================================
/**
 * @file nimcp_gt_coalition.h
 * @brief Hedonic coalition formation games and stability analysis
 *
 * WHAT: Coalition formation, stability checking, and dynamics
 * WHY:  Enable neural coalition formation for binding and credit assignment
 * HOW:  Hedonic games, core stability, merge/split dynamics
 *
 * BIOLOGICAL INSPIRATION:
 * - Neural coalition formation (binding problem)
 * - Synaptic cluster formation for pattern encoding
 * - Hormonal group coordination
 * - Immune cell cooperative responses
 *
 * KEY CONCEPTS:
 * - Hedonic Games: Players care only about their own coalition
 * - Core Stability: No coalition can profitably deviate
 * - Individual Rationality: Each player prefers coalition over singleton
 * - Blocking Coalition: Coalition that would prefer to deviate
 * - Merge/Split Dynamics: Iterative coalition refinement
 *
 * INTEGRATION: Credit Assignment (for coalition value computation)
 *
 * ERROR CODE RANGE: 25000-25999 (shared with game_theory module)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_COALITION_H
#define NIMCP_GT_COALITION_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module-Specific Error Codes (within 25000-25999 range)
//=============================================================================

#define NIMCP_GT_ERROR_COALITION_EMPTY      (NIMCP_GT_ERROR_BASE + 60)
#define NIMCP_GT_ERROR_COALITION_INVALID    (NIMCP_GT_ERROR_BASE + 61)
#define NIMCP_GT_ERROR_PARTITION_INVALID    (NIMCP_GT_ERROR_BASE + 62)
#define NIMCP_GT_ERROR_NO_BLOCKING          (NIMCP_GT_ERROR_BASE + 63)
#define NIMCP_GT_ERROR_PREFERENCES_NOT_SET  (NIMCP_GT_ERROR_BASE + 64)
#define NIMCP_GT_ERROR_VALUE_FN_NOT_SET     (NIMCP_GT_ERROR_BASE + 65)

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum coalitions in a structure (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_COALITIONS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_COALITIONS 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_COALITIONS 8
#else
    #define NIMCP_GT_MAX_COALITIONS 4
#endif

/** Maximum preference orderings per player */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_PREFERENCES 64
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_PREFERENCES 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_PREFERENCES 16
#else
    #define NIMCP_GT_MAX_PREFERENCES 8
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Stability concept for coalition structures
 */
typedef enum {
    NIMCP_STABILITY_CORE,             /**< No blocking coalition exists */
    NIMCP_STABILITY_NASH,             /**< No single player wants to deviate */
    NIMCP_STABILITY_INDIVIDUAL,       /**< Each player prefers coalition over singleton */
    NIMCP_STABILITY_CONTRACTUAL,      /**< Members consent to deviations */
    NIMCP_STABILITY_STRICT_CORE,      /**< Strict core (no weakly blocking) */
    NIMCP_STABILITY_COUNT
} nimcp_stability_type_t;

/**
 * @brief Coalition formation algorithm
 */
typedef enum {
    NIMCP_FORMATION_GREEDY,           /**< Greedy sequential formation */
    NIMCP_FORMATION_OPTIMAL,          /**< Optimal partition (exponential) */
    NIMCP_FORMATION_MERGE_SPLIT,      /**< Merge/split dynamics */
    NIMCP_FORMATION_BOTTOM_UP,        /**< Start singletons, merge upward */
    NIMCP_FORMATION_TOP_DOWN,         /**< Start grand coalition, split */
    NIMCP_FORMATION_COUNT
} nimcp_formation_algorithm_t;

/**
 * @brief Coalition value function callback
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
typedef float (*nimcp_gt_coalition_value_fn)(
    uint32_t coalition,
    uint32_t num_players,
    void* user_data
);

/**
 * @brief Player preference callback for hedonic games
 *
 * WHAT: Compare two coalitions from a player's perspective
 * WHY:  Hedonic games require player preferences over coalitions
 * HOW:  Return comparison result: negative if coal1 < coal2, 0 if equal, positive if coal1 > coal2
 *
 * @param player Player making the comparison
 * @param coalition1 First coalition (bitmask, must contain player)
 * @param coalition2 Second coalition (bitmask, must contain player)
 * @param user_data Context
 * @return Comparison result (like strcmp)
 */
typedef int (*nimcp_gt_preference_fn)(
    uint32_t player,
    uint32_t coalition1,
    uint32_t coalition2,
    void* user_data
);

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Single coalition representation
 */
typedef struct {
    uint32_t members;                 /**< Bitmask of player membership */
    float value;                      /**< Coalition value v(S) */
    uint32_t size;                    /**< Number of members (popcount of members) */
} nimcp_coalition_t;

/**
 * @brief Coalition structure (partition of players)
 *
 * WHAT: A partition of the player set into disjoint coalitions
 * WHY:  Represents a complete coalition configuration
 * HOW:  Array of coalitions that together include all players exactly once
 */
typedef struct {
    nimcp_coalition_t coalitions[NIMCP_GT_MAX_COALITIONS]; /**< The coalitions */
    uint32_t num_coalitions;          /**< Number of coalitions in partition */
    float total_value;                /**< Sum of coalition values */
    uint32_t all_players;             /**< Bitmask of all players (for validation) */
} nimcp_coalition_structure_t;

/**
 * @brief Coalition formation configuration
 */
typedef struct {
    uint32_t num_players;             /**< Number of players in the game */
    nimcp_formation_algorithm_t algorithm; /**< Formation algorithm */
    uint32_t max_iterations;          /**< Max iterations for dynamics */
    float convergence_epsilon;        /**< Convergence threshold */
    bool cache_values;                /**< Cache coalition values? */
    bool use_preferences;             /**< Use preference-based formation? */
} nimcp_coalition_config_t;

/**
 * @brief Coalition formation result
 */
typedef struct {
    nimcp_coalition_structure_t structure; /**< Resulting coalition structure */
    bool is_stable[NIMCP_STABILITY_COUNT]; /**< Stability by type */
    uint32_t iterations;              /**< Iterations to converge */
    uint64_t coalitions_evaluated;    /**< Number of coalitions evaluated */
    float formation_time_ms;          /**< Time taken for formation */
    bool converged;                   /**< Did algorithm converge? */
} nimcp_coalition_result_t;

/**
 * @brief Opaque coalition game handle
 */
typedef struct nimcp_coalition_game_struct* nimcp_coalition_game_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default coalition configuration
 *
 * @param num_players Number of players
 * @return Default configuration
 */
nimcp_coalition_config_t nimcp_coalition_default_config(uint32_t num_players);

/**
 * @brief Create coalition game context
 *
 * WHAT: Allocate and initialize coalition game
 * WHY:  Encapsulate game state and algorithms
 * HOW:  Allocate struct, initialize caches
 *
 * @param config Configuration (NULL for defaults with 4 players)
 * @return Game handle or NULL on failure
 */
nimcp_coalition_game_t nimcp_coalition_create(const nimcp_coalition_config_t* config);

/**
 * @brief Destroy coalition game context
 *
 * @param game Game handle to destroy
 */
void nimcp_coalition_destroy(nimcp_coalition_game_t game);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Set coalition value function
 *
 * WHAT: Set the characteristic function for the game
 * WHY:  Value function defines the cooperative game
 * HOW:  Store callback and user data
 *
 * @param game Game handle
 * @param value_fn Value function callback
 * @param user_data Context for value function
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_set_value_function(
    nimcp_coalition_game_t game,
    nimcp_gt_coalition_value_fn value_fn,
    void* user_data
);

/**
 * @brief Set player preference function (for hedonic games)
 *
 * WHAT: Set preference comparison for hedonic formation
 * WHY:  Hedonic games use preferences over value functions
 * HOW:  Store preference callback
 *
 * @param game Game handle
 * @param pref_fn Preference comparison callback
 * @param user_data Context for preference function
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_set_preferences(
    nimcp_coalition_game_t game,
    nimcp_gt_preference_fn pref_fn,
    void* user_data
);

/**
 * @brief Set explicit preference ordering for a player
 *
 * WHAT: Set ordered list of preferred coalitions for a player
 * WHY:  Alternative to callback-based preferences
 * HOW:  Store preference array (most preferred first)
 *
 * @param game Game handle
 * @param player Player index
 * @param preferences Array of coalition bitmasks in preference order
 * @param num_preferences Number of coalitions in preference list
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_set_preference_order(
    nimcp_coalition_game_t game,
    uint32_t player,
    const uint32_t* preferences,
    uint32_t num_preferences
);

//=============================================================================
// Formation Algorithms
//=============================================================================

/**
 * @brief Form coalitions using greedy algorithm
 *
 * WHAT: Sequential greedy coalition formation
 * WHY:  Fast O(n^2) approximation
 * HOW:  Each player joins best available coalition
 *
 * ALGORITHM:
 * 1. Start with singletons
 * 2. For each player, consider joining existing coalitions
 * 3. Join coalition that maximizes player's payoff
 * 4. Continue until no beneficial moves
 *
 * COMPLEXITY: O(n^2 * k) where k = coalition evaluations
 *
 * @param game Game handle
 * @param result Output formation result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_form_greedy(
    nimcp_coalition_game_t game,
    nimcp_coalition_result_t* result
);

/**
 * @brief Form optimal coalition structure
 *
 * WHAT: Find partition that maximizes total value
 * WHY:  Optimal solution for small games
 * HOW:  Dynamic programming over partition space
 *
 * COMPLEXITY: O(3^n) - feasible for n <= 15
 *
 * @param game Game handle
 * @param result Output formation result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_form_optimal(
    nimcp_coalition_game_t game,
    nimcp_coalition_result_t* result
);

/**
 * @brief Form coalitions using merge/split dynamics
 *
 * WHAT: Iteratively merge and split coalitions
 * WHY:  Finds stable structures through local moves
 * HOW:  Alternate merge and split phases until stable
 *
 * MERGE: Combine coalitions if total value increases
 * SPLIT: Divide coalition if parts have higher total value
 *
 * @param game Game handle
 * @param result Output formation result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_form_merge_split(
    nimcp_coalition_game_t game,
    nimcp_coalition_result_t* result
);

//=============================================================================
// Stability Analysis
//=============================================================================

/**
 * @brief Check if coalition structure is stable
 *
 * WHAT: Verify stability under specified concept
 * WHY:  Stability indicates equilibrium configuration
 * HOW:  Check stability conditions for given type
 *
 * @param game Game handle
 * @param structure Coalition structure to check
 * @param stability_type Type of stability to check
 * @return true if stable under the given concept
 */
bool nimcp_coalition_is_stable(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    nimcp_stability_type_t stability_type
);

/**
 * @brief Check if structure is in the core
 *
 * WHAT: Verify no blocking coalition exists
 * WHY:  Core membership means no coalition can improve
 * HOW:  Check sum(payoff[S]) >= v(S) for all S
 *
 * @param game Game handle
 * @param structure Coalition structure to check
 * @return true if in core
 */
bool nimcp_coalition_is_in_core(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure
);

/**
 * @brief Check individual rationality
 *
 * WHAT: Verify each player prefers coalition over singleton
 * WHY:  Individual rationality is minimal stability requirement
 * HOW:  Check payoff >= v({i}) for each player
 *
 * @param game Game handle
 * @param structure Coalition structure to check
 * @return true if individually rational
 */
bool nimcp_coalition_is_individually_rational(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure
);

/**
 * @brief Find a blocking coalition if one exists
 *
 * WHAT: Find coalition that would prefer to deviate
 * WHY:  Blocking coalitions indicate instability
 * HOW:  Search for coalition where all members benefit from deviation
 *
 * @param game Game handle
 * @param structure Current coalition structure
 * @param blocking_coalition Output: blocking coalition (bitmask), or 0 if none
 * @return NIMCP_SUCCESS if found, NIMCP_GT_ERROR_NO_BLOCKING if none exists
 */
nimcp_error_t nimcp_coalition_find_blocking(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t* blocking_coalition
);

//=============================================================================
// Dynamics Operations
//=============================================================================

/**
 * @brief Merge two coalitions
 *
 * WHAT: Combine two coalitions into one
 * WHY:  Merge step of merge/split dynamics
 * HOW:  Create new structure with merged coalition
 *
 * @param game Game handle
 * @param structure Current structure
 * @param coal_idx1 Index of first coalition
 * @param coal_idx2 Index of second coalition
 * @param new_structure Output: new coalition structure
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_merge(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t coal_idx1,
    uint32_t coal_idx2,
    nimcp_coalition_structure_t* new_structure
);

/**
 * @brief Split a coalition into optimal parts
 *
 * WHAT: Divide coalition to maximize total value
 * WHY:  Split step of merge/split dynamics
 * HOW:  Find best binary partition of coalition
 *
 * @param game Game handle
 * @param structure Current structure
 * @param coal_idx Index of coalition to split
 * @param new_structure Output: new coalition structure
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_split(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t coal_idx,
    nimcp_coalition_structure_t* new_structure
);

/**
 * @brief Player deviation check
 *
 * WHAT: Check if player would benefit from joining another coalition
 * WHY:  Nash stability check for individual deviations
 * HOW:  Compare payoffs in current vs alternative coalition
 *
 * @param game Game handle
 * @param structure Current structure
 * @param player Player index
 * @param target_coal_idx Target coalition index (or -1 for singleton)
 * @return true if deviation is beneficial
 */
bool nimcp_coalition_player_would_deviate(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    uint32_t player,
    int32_t target_coal_idx
);

//=============================================================================
// Value and Payoff Computation
//=============================================================================

/**
 * @brief Compute coalition value
 *
 * WHAT: Get value of a coalition
 * WHY:  Core operation for all coalition algorithms
 * HOW:  Call value function with caching
 *
 * @param game Game handle
 * @param coalition Coalition bitmask
 * @param value Output: coalition value
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_compute_value(
    nimcp_coalition_game_t game,
    uint32_t coalition,
    float* value
);

/**
 * @brief Compute payoff for a player in a coalition
 *
 * WHAT: Get player's share of coalition value
 * WHY:  Payoffs determine stability and incentives
 * HOW:  Use equal split within coalition by default
 *
 * @param game Game handle
 * @param coalition Coalition bitmask
 * @param player Player index
 * @param payoff Output: player's payoff
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_compute_payoff(
    nimcp_coalition_game_t game,
    uint32_t coalition,
    uint32_t player,
    float* payoff
);

/**
 * @brief Compute all payoffs for a structure
 *
 * WHAT: Get payoff for each player in structure
 * WHY:  Needed for stability analysis
 * HOW:  Compute payoffs for each coalition
 *
 * @param game Game handle
 * @param structure Coalition structure
 * @param payoffs Output: array of payoffs (size = num_players)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_coalition_compute_payoffs(
    nimcp_coalition_game_t game,
    const nimcp_coalition_structure_t* structure,
    float* payoffs
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Initialize a coalition structure with singletons
 *
 * @param structure Structure to initialize
 * @param num_players Number of players
 */
void nimcp_coalition_structure_init_singletons(
    nimcp_coalition_structure_t* structure,
    uint32_t num_players
);

/**
 * @brief Initialize a coalition structure with grand coalition
 *
 * @param structure Structure to initialize
 * @param num_players Number of players
 */
void nimcp_coalition_structure_init_grand(
    nimcp_coalition_structure_t* structure,
    uint32_t num_players
);

/**
 * @brief Validate a coalition structure
 *
 * WHAT: Check that structure is a valid partition
 * WHY:  Ensure coalitions are disjoint and cover all players
 * HOW:  Check bitmask union and intersection
 *
 * @param structure Structure to validate
 * @param num_players Number of players
 * @return true if valid partition
 */
bool nimcp_coalition_structure_is_valid(
    const nimcp_coalition_structure_t* structure,
    uint32_t num_players
);

/**
 * @brief Find coalition containing a player
 *
 * @param structure Coalition structure
 * @param player Player index
 * @return Coalition index, or -1 if not found
 */
int32_t nimcp_coalition_find_player_coalition(
    const nimcp_coalition_structure_t* structure,
    uint32_t player
);

/**
 * @brief Get stability type name
 */
const char* nimcp_stability_type_name(nimcp_stability_type_t type);

/**
 * @brief Get formation algorithm name
 */
const char* nimcp_formation_algorithm_name(nimcp_formation_algorithm_t algorithm);

/**
 * @brief Get number of players in game
 */
uint32_t nimcp_coalition_get_num_players(const nimcp_coalition_game_t game);

/**
 * @brief Clear coalition value cache
 *
 * @param game Game handle
 */
void nimcp_coalition_clear_cache(nimcp_coalition_game_t game);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_COALITION_H
