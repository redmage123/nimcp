//=============================================================================
// nimcp_gt_neuromod.h - Game Payoff to Neuromodulator Mapping
//=============================================================================
/**
 * @file nimcp_gt_neuromod.h
 * @brief Map game-theoretic outcomes to neuromodulator release
 *
 * WHAT: Payoff-based neuromodulator release
 * WHY:  Game outcomes should affect learning and motivation
 * HOW:  Win -> DA, Loss -> 5-HT, Unfair -> NE
 *
 * BIOLOGICAL INSPIRATION:
 * - Dopamine encodes reward prediction error
 * - Serotonin modulates patience and impulse control
 * - Norepinephrine signals threat and arousal
 * - Acetylcholine drives attention and encoding
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_NEUROMOD_H
#define NIMCP_GT_NEUROMOD_H

#include "cognitive/game_theory/nimcp_game_theory.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Payoff-to-neuromodulator mapping configuration
 */
typedef struct {
    // Dopamine (reward/motivation)
    float payoff_dopamine_gain;       /**< DA per unit payoff */
    float win_dopamine_bonus;         /**< Bonus DA for winning */
    float cooperation_dopamine_bonus; /**< DA for successful cooperation */

    // Serotonin (patience/inhibition)
    float loss_serotonin_gain;        /**< 5-HT per unit loss */
    float unfair_serotonin_boost;     /**< 5-HT for unfair outcome */
    float timeout_serotonin_gain;     /**< 5-HT for timeout (patience) */

    // Norepinephrine (arousal/threat)
    float competition_ne_baseline;    /**< NE during competition */
    float fairness_violation_ne_gain; /**< NE for fairness violations */
    float uncertainty_ne_gain;        /**< NE for high uncertainty */

    // Acetylcholine (attention/salience)
    float strategic_ach_gain;         /**< ACh for strategic decisions */
    float novel_opponent_ach_boost;   /**< ACh for new opponents */

    // Thresholds
    float payoff_threshold;           /**< Minimum payoff for DA release */
    float fairness_threshold;         /**< Below this = unfair */
    float uncertainty_threshold;      /**< Above this = uncertain */
} gt_neuromod_config_t;

/**
 * @brief Neuromodulator release from game outcome
 */
typedef struct {
    float dopamine_released;          /**< DA amount released */
    float serotonin_released;         /**< 5-HT amount released */
    float norepinephrine_released;    /**< NE amount released */
    float acetylcholine_released;     /**< ACh amount released */
    float net_valence;                /**< Overall valence (-1 to +1) */
} gt_neuromod_release_t;

/**
 * @brief Opaque handle to neuromodulator bridge
 */
typedef struct gt_neuromod_bridge_struct* gt_neuromod_bridge_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default neuromodulator bridge configuration
 */
gt_neuromod_config_t gt_neuromod_default_config(void);

/**
 * @brief Create neuromodulator bridge
 *
 * @param neuromod Neuromodulator system to bridge
 * @param config Bridge configuration
 * @return Bridge handle or NULL
 */
gt_neuromod_bridge_t gt_neuromod_bridge_create(
    neuromodulator_system_t neuromod,
    const gt_neuromod_config_t* config
);

/**
 * @brief Destroy neuromodulator bridge
 */
void gt_neuromod_bridge_destroy(gt_neuromod_bridge_t bridge);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Process game outcome and release neuromodulators
 *
 * WHAT: Map game payoffs to neurotransmitter release
 * WHY:  Game outcomes affect learning via neuromodulation
 * HOW:  Winning -> DA, Losing -> 5-HT, Unfair -> NE
 *
 * @param bridge Bridge handle
 * @param player Player whose perspective to use
 * @param outcome Game outcome
 * @param release Output neuromodulator release
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t gt_neuromod_process_outcome(
    gt_neuromod_bridge_t bridge,
    nimcp_player_id_t player,
    const nimcp_game_outcome_t* outcome,
    gt_neuromod_release_t* release
);

/**
 * @brief Release dopamine for auction win
 *
 * @param bridge Bridge handle
 * @param winning_bid Winning bid amount
 * @param payment Actual payment
 * @return Amount of dopamine released
 */
float gt_neuromod_auction_win(
    gt_neuromod_bridge_t bridge,
    float winning_bid,
    float payment
);

/**
 * @brief Release dopamine for successful bargaining
 *
 * @param bridge Bridge handle
 * @param agreement_value Value of agreement
 * @param fairness_index Fairness of outcome (0-1)
 * @return Amount of dopamine released
 */
float gt_neuromod_bargaining_success(
    gt_neuromod_bridge_t bridge,
    float agreement_value,
    float fairness_index
);

/**
 * @brief Release serotonin for bargaining failure
 *
 * @param bridge Bridge handle
 * @param rounds_taken Rounds before failure
 * @param max_rounds Maximum allowed rounds
 * @return Amount of serotonin released
 */
float gt_neuromod_bargaining_failure(
    gt_neuromod_bridge_t bridge,
    uint32_t rounds_taken,
    uint32_t max_rounds
);

/**
 * @brief Release norepinephrine for unfairness
 *
 * @param bridge Bridge handle
 * @param unfairness_magnitude How unfair (0-1)
 * @return Amount of norepinephrine released
 */
float gt_neuromod_signal_unfairness(
    gt_neuromod_bridge_t bridge,
    float unfairness_magnitude
);

/**
 * @brief Release acetylcholine for strategic situation
 *
 * @param bridge Bridge handle
 * @param num_options Number of strategic options
 * @param uncertainty Decision uncertainty (0-1)
 * @return Amount of acetylcholine released
 */
float gt_neuromod_strategic_attention(
    gt_neuromod_bridge_t bridge,
    uint32_t num_options,
    float uncertainty
);

/**
 * @brief Process Shapley credit and release proportional DA
 *
 * @param bridge Bridge handle
 * @param credit Player's Shapley credit
 * @param total_value Grand coalition value
 * @return Amount of dopamine released
 */
float gt_neuromod_shapley_reward(
    gt_neuromod_bridge_t bridge,
    float credit,
    float total_value
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get underlying neuromodulator system
 */
neuromodulator_system_t gt_neuromod_get_system(const gt_neuromod_bridge_t bridge);

/**
 * @brief Get cumulative release statistics
 */
nimcp_error_t gt_neuromod_get_cumulative_release(
    const gt_neuromod_bridge_t bridge,
    gt_neuromod_release_t* cumulative
);

/**
 * @brief Reset release statistics
 */
void gt_neuromod_reset_stats(gt_neuromod_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_NEUROMOD_H
