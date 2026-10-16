//=============================================================================
// nimcp_gt_repeated.h - Repeated Games and Folk Theorem Applications
//=============================================================================
/**
 * @file nimcp_gt_repeated.h
 * @brief Repeated games, trigger strategies, and cooperation sustainability
 *
 * WHAT: Implements repeated game dynamics with history tracking
 * WHY:  Enable cooperation through repeated interactions and reputation
 * HOW:  Discount factors, trigger strategies, folk theorem feasibility
 *
 * BIOLOGICAL INSPIRATION:
 * - Reciprocal altruism in social species
 * - Neural memory for past interactions (hippocampus)
 * - Trust building through repeated synaptic activation
 * - Immune system memory for pathogen recognition
 *
 * FOLK THEOREM: Any feasible, individually rational payoff can be sustained
 * in equilibrium when players are sufficiently patient (high discount factor).
 *
 * INTEGRATION: Hemispheric Brain (repeated cooperation patterns)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_REPEATED_H
#define NIMCP_GT_REPEATED_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum history length (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_REPEATED_MAX_HISTORY 1024
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_REPEATED_MAX_HISTORY 512
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_REPEATED_MAX_HISTORY 128
#else
    #define NIMCP_REPEATED_MAX_HISTORY 64
#endif

/** Maximum actions per player in stage game */
#define NIMCP_REPEATED_MAX_ACTIONS 16

/** Maximum vertices in feasibility set polygon */
#define NIMCP_REPEATED_MAX_VERTICES 64

/** Default discount factor */
#define NIMCP_REPEATED_DEFAULT_DISCOUNT 0.9f

/** Cooperation detection threshold */
#define NIMCP_REPEATED_COOP_THRESHOLD 0.7f

//=============================================================================
// Strategy Types
//=============================================================================

/**
 * @brief Repeated game strategy types
 *
 * BIOLOGICAL MAPPING:
 * - ALWAYS_COOPERATE: Unconditional altruism
 * - ALWAYS_DEFECT: Parasitic behavior
 * - TIT_FOR_TAT: Reciprocal altruism
 * - GRIM_TRIGGER: Immune memory (permanent response)
 * - GENEROUS_TFT: Forgiveness mechanism
 * - PAVLOV: Win-stay-lose-shift (reward-based learning)
 */
typedef enum {
    NIMCP_STRATEGY_ALWAYS_COOPERATE,   /**< Always play cooperate */
    NIMCP_STRATEGY_ALWAYS_DEFECT,      /**< Always play defect */
    NIMCP_STRATEGY_TIT_FOR_TAT,        /**< Copy opponent's last move */
    NIMCP_STRATEGY_GRIM_TRIGGER,       /**< Defect forever after first defection */
    NIMCP_STRATEGY_GENEROUS_TFT,       /**< TFT with probabilistic forgiveness */
    NIMCP_STRATEGY_PAVLOV,             /**< Win-stay, lose-shift */
    NIMCP_STRATEGY_CUSTOM,             /**< User-defined strategy callback */
    NIMCP_REPEATED_STRATEGY_COUNT
} nimcp_repeated_strategy_type_t;

/**
 * @brief Cooperation level classification
 */
typedef enum {
    NIMCP_COOP_LEVEL_NONE,             /**< No cooperation detected */
    NIMCP_COOP_LEVEL_LOW,              /**< Sporadic cooperation */
    NIMCP_COOP_LEVEL_MEDIUM,           /**< Moderate cooperation */
    NIMCP_COOP_LEVEL_HIGH,             /**< Strong cooperation */
    NIMCP_COOP_LEVEL_FULL              /**< Complete mutual cooperation */
} nimcp_cooperation_level_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Action pair in stage game history
 */
typedef struct {
    uint32_t actions[NIMCP_GT_MAX_PLAYERS];  /**< Action indices per player */
    float payoffs[NIMCP_GT_MAX_PLAYERS];     /**< Realized payoffs */
    uint64_t timestamp_ms;                    /**< When round was played */
} nimcp_round_record_t;

/**
 * @brief History of repeated game play
 */
typedef struct {
    nimcp_round_record_t* rounds;      /**< Array of round records */
    uint32_t num_rounds;               /**< Number of rounds played */
    uint32_t max_rounds;               /**< History capacity */
    uint32_t head;                     /**< Circular buffer head */
} nimcp_repeated_history_t;

/**
 * @brief Stage game payoff matrix
 *
 * Row player chooses row, column player chooses column.
 * payoffs[row][col] gives (row_payoff, col_payoff) pair.
 */
typedef struct {
    uint32_t num_players;              /**< Number of players (typically 2) */
    uint32_t num_actions[NIMCP_GT_MAX_PLAYERS]; /**< Actions per player */
    float* payoff_matrix;              /**< Flattened payoff tensor */
    size_t matrix_size;                /**< Size of payoff matrix */
} nimcp_stage_game_t;

/**
 * @brief Strategy parameters for repeated play
 */
typedef struct {
    nimcp_repeated_strategy_type_t type; /**< Strategy type */
    float forgiveness_prob;            /**< For generous TFT [0,1] */
    float exploration_rate;            /**< Random exploration probability */
    void* custom_data;                 /**< Custom strategy context */

    /**
     * @brief Custom strategy callback
     *
     * @param history Past play history
     * @param player_id Which player is choosing
     * @param custom_data User context
     * @return Action index to play
     */
    uint32_t (*custom_strategy)(
        const nimcp_repeated_history_t* history,
        uint32_t player_id,
        void* custom_data
    );
} nimcp_repeated_strategy_t;

/**
 * @brief Configuration for repeated game
 */
typedef struct {
    float discount_factor;             /**< Future payoff discount delta [0,1] */
    uint32_t history_length;           /**< Maximum history to track */
    uint32_t num_players;              /**< Number of players */
    bool track_cooperation;            /**< Track cooperation metrics? */
    bool enable_statistics;            /**< Collect detailed statistics? */
} nimcp_repeated_config_t;

/**
 * @brief Point in payoff space (for feasibility set)
 */
typedef struct {
    float payoffs[NIMCP_GT_MAX_PLAYERS]; /**< Payoff for each player */
} nimcp_payoff_point_t;

/**
 * @brief Repeated game simulation result
 */
typedef struct {
    float avg_payoffs[NIMCP_GT_MAX_PLAYERS];    /**< Average payoff per player */
    float discounted_payoffs[NIMCP_GT_MAX_PLAYERS]; /**< Discounted sum */
    float total_payoffs[NIMCP_GT_MAX_PLAYERS];  /**< Total undiscounted sum */
    uint32_t rounds_played;            /**< Number of rounds simulated */
    float cooperation_rate;            /**< Overall cooperation rate */
    nimcp_cooperation_level_t coop_level; /**< Cooperation classification */
    bool reached_equilibrium;          /**< Did strategies stabilize? */
    uint32_t equilibrium_round;        /**< Round where equilibrium reached */
} nimcp_repeated_result_t;

/**
 * @brief Opaque repeated game context handle
 */
typedef struct nimcp_repeated_game_struct* nimcp_repeated_game_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration for repeated games
 *
 * @return Default configuration with reasonable values
 */
nimcp_repeated_config_t nimcp_repeated_default_config(void);

/**
 * @brief Create repeated game context
 *
 * WHAT: Initialize repeated game with stage game payoffs
 * WHY:  Set up context for repeated play simulation
 * HOW:  Allocate history, copy stage game, initialize strategies
 *
 * @param config Configuration (NULL for defaults)
 * @param stage_payoffs Stage game payoff matrix (row-major, flattened)
 * @param num_actions Array of action counts per player
 * @param num_players Number of players
 * @return Game context or NULL on failure
 */
nimcp_repeated_game_t nimcp_repeated_create(
    const nimcp_repeated_config_t* config,
    const float* stage_payoffs,
    const uint32_t* num_actions,
    uint32_t num_players
);

/**
 * @brief Destroy repeated game context
 *
 * @param ctx Game context to destroy (NULL safe)
 */
void nimcp_repeated_destroy(nimcp_repeated_game_t ctx);

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Set discount factor
 *
 * WHAT: Update future payoff discount
 * WHY:  Higher discount -> more patient -> more cooperation possible
 * HOW:  delta close to 1 = patient, close to 0 = myopic
 *
 * FOLK THEOREM: For any feasible, individually rational payoff profile,
 * there exists delta* < 1 such that for all delta > delta*, that payoff
 * can be sustained as a subgame perfect equilibrium.
 *
 * @param ctx Game context
 * @param discount_factor Discount factor in (0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_set_discount(
    nimcp_repeated_game_t ctx,
    float discount_factor
);

/**
 * @brief Set strategy for a player
 *
 * @param ctx Game context
 * @param player Player index
 * @param strategy Strategy configuration
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_set_strategy(
    nimcp_repeated_game_t ctx,
    uint32_t player,
    const nimcp_repeated_strategy_t* strategy
);

//=============================================================================
// Core Game Operations
//=============================================================================

/**
 * @brief Play one round of the stage game
 *
 * WHAT: Execute one round with specified actions
 * WHY:  Manual control over repeated game play
 * HOW:  Record actions, compute payoffs, update history
 *
 * @param ctx Game context
 * @param actions Action indices for each player
 * @param payoffs_out Output payoffs (may be NULL)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_play_round(
    nimcp_repeated_game_t ctx,
    const uint32_t* actions,
    float* payoffs_out
);

/**
 * @brief Get current game history
 *
 * @param ctx Game context
 * @param history_out Output history structure
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_get_history(
    const nimcp_repeated_game_t ctx,
    nimcp_repeated_history_t* history_out
);

/**
 * @brief Compute average payoff for a player
 *
 * WHAT: Calculate mean payoff across all rounds
 * WHY:  Key metric for long-run behavior
 * HOW:  Sum payoffs / number of rounds
 *
 * @param ctx Game context
 * @param player Player index
 * @return Average payoff (NaN if no rounds played)
 */
float nimcp_repeated_compute_avg_payoff(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

/**
 * @brief Compute discounted payoff for a player
 *
 * WHAT: Calculate discounted sum of payoffs
 * WHY:  The objective players maximize in repeated games
 * HOW:  Sum[delta^t * payoff_t] normalized by (1-delta)
 *
 * @param ctx Game context
 * @param player Player index
 * @return Discounted average payoff
 */
float nimcp_repeated_compute_discounted_payoff(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

//=============================================================================
// Folk Theorem and Feasibility
//=============================================================================

/**
 * @brief Check if target payoffs are sustainable
 *
 * WHAT: Test folk theorem conditions for target payoff profile
 * WHY:  Determine if cooperation at given payoffs is equilibrium
 * HOW:  Check individual rationality and patience condition
 *
 * FOLK THEOREM CONDITIONS:
 * 1. Feasible: payoffs achievable by some action profile mix
 * 2. Individually rational: payoff >= minmax for each player
 * 3. Patient enough: discount factor > critical threshold
 *
 * @param ctx Game context
 * @param target_payoffs Desired payoff for each player
 * @return true if sustainable, false otherwise
 */
bool nimcp_repeated_is_sustainable(
    const nimcp_repeated_game_t ctx,
    const float* target_payoffs
);

/**
 * @brief Compute feasibility set vertices
 *
 * WHAT: Find convex hull of achievable payoffs
 * WHY:  Folk theorem applies to this set
 * HOW:  Enumerate pure strategy profiles, compute payoffs
 *
 * @param ctx Game context
 * @param vertices Output array of payoff vertices
 * @param num_vertices Output number of vertices (also max capacity input)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_feasibility_set(
    const nimcp_repeated_game_t ctx,
    nimcp_payoff_point_t* vertices,
    uint32_t* num_vertices
);

/**
 * @brief Compute minmax payoff for a player
 *
 * WHAT: Find worst-case payoff player can guarantee
 * WHY:  Lower bound for folk theorem individual rationality
 * HOW:  max_own min_others payoff(own, others)
 *
 * The minmax payoff is the security level - what a player can
 * guarantee regardless of other players' actions.
 *
 * @param ctx Game context
 * @param player Player index
 * @return Minmax payoff value
 */
float nimcp_repeated_minmax_payoff(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

/**
 * @brief Compute critical discount factor for target payoffs
 *
 * WHAT: Find minimum discount needed to sustain target payoffs
 * WHY:  Determine patience requirement for cooperation
 * HOW:  Solve for delta where deviation is unprofitable
 *
 * @param ctx Game context
 * @param target_payoffs Target payoff profile
 * @return Critical discount factor (returns 1.0 if unsustainable)
 */
float nimcp_repeated_critical_discount(
    const nimcp_repeated_game_t ctx,
    const float* target_payoffs
);

//=============================================================================
// Trigger Strategy Functions
//=============================================================================

/**
 * @brief Get next action according to strategy
 *
 * WHAT: Determine action based on strategy and history
 * WHY:  Automated strategy execution
 * HOW:  Apply strategy logic to history
 *
 * @param ctx Game context
 * @param player Player index
 * @return Action index to play
 */
uint32_t nimcp_repeated_get_strategy_action(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

/**
 * @brief Check if trigger strategy has been activated
 *
 * WHAT: Detect if punishment phase is active
 * WHY:  Monitor strategy state
 * HOW:  Check history for trigger conditions
 *
 * @param ctx Game context
 * @param player Player index
 * @return true if trigger activated (in punishment phase)
 */
bool nimcp_repeated_trigger_activated(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

/**
 * @brief Get round when trigger was activated
 *
 * WHAT: Find first defection that triggered punishment
 * WHY:  Analyze cooperation breakdown
 * HOW:  Scan history for trigger event
 *
 * @param ctx Game context
 * @param player Player index
 * @return Round number of trigger (-1 if not triggered)
 */
int32_t nimcp_repeated_trigger_threshold(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Simulate repeated game for multiple rounds
 *
 * WHAT: Run repeated game with current strategies
 * WHY:  Test strategy profiles, analyze outcomes
 * HOW:  Loop: get actions from strategies, play round, record
 *
 * @param ctx Game context
 * @param num_rounds Number of rounds to simulate
 * @param result Output simulation result
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_simulate(
    nimcp_repeated_game_t ctx,
    uint32_t num_rounds,
    nimcp_repeated_result_t* result
);

/**
 * @brief Reset game to initial state
 *
 * WHAT: Clear history and reset strategies
 * WHY:  Run multiple simulations from clean state
 * HOW:  Zero history, reset trigger states
 *
 * @param ctx Game context
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_repeated_reset(nimcp_repeated_game_t ctx);

//=============================================================================
// Cooperation Analysis
//=============================================================================

/**
 * @brief Detect cooperation level from history
 *
 * WHAT: Analyze history to classify cooperation
 * WHY:  Quantify cooperation emergence
 * HOW:  Compute cooperation rate over history window
 *
 * @param ctx Game context
 * @return Cooperation level classification
 */
nimcp_cooperation_level_t nimcp_repeated_detect_cooperation(
    const nimcp_repeated_game_t ctx
);

/**
 * @brief Get cooperation rate
 *
 * WHAT: Compute fraction of cooperative outcomes
 * WHY:  Quantitative cooperation metric
 * HOW:  Count mutual cooperation / total rounds
 *
 * @param ctx Game context
 * @return Cooperation rate [0, 1]
 */
float nimcp_repeated_cooperation_rate(const nimcp_repeated_game_t ctx);

/**
 * @brief Analyze strategy stability
 *
 * WHAT: Check if current play pattern is stable
 * WHY:  Detect equilibrium convergence
 * HOW:  Analyze recent history for pattern repetition
 *
 * @param ctx Game context
 * @param window_size Number of recent rounds to analyze
 * @return true if pattern is stable
 */
bool nimcp_repeated_is_stable(
    const nimcp_repeated_game_t ctx,
    uint32_t window_size
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get strategy type name
 */
const char* nimcp_repeated_strategy_name(nimcp_repeated_strategy_type_t type);

/**
 * @brief Get cooperation level name
 */
const char* nimcp_repeated_coop_level_name(nimcp_cooperation_level_t level);

/**
 * @brief Get current discount factor
 */
float nimcp_repeated_get_discount(const nimcp_repeated_game_t ctx);

/**
 * @brief Get number of rounds played
 */
uint32_t nimcp_repeated_get_num_rounds(const nimcp_repeated_game_t ctx);

/**
 * @brief Get number of players
 */
uint32_t nimcp_repeated_get_num_players(const nimcp_repeated_game_t ctx);

/**
 * @brief Get player's current strategy type
 */
nimcp_repeated_strategy_type_t nimcp_repeated_get_strategy_type(
    const nimcp_repeated_game_t ctx,
    uint32_t player
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_REPEATED_H
