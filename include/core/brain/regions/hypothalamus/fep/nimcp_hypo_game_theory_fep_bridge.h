/**
 * @file nimcp_hypo_game_theory_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Game Theory Integration
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for game-theoretic strategy selection via hypothalamic drives
 * WHY:  SOCIAL drive shapes strategy selection; cooperation vs competition as free energy
 * HOW:  Map drive state to game strategies, use prediction errors for strategy adaptation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * GAME THEORY AS DRIVE-MODULATED BEHAVIOR:
 * - SOCIAL drive (paraventricular nucleus) --> Cooperation tendency
 * - SAFETY drive (ventromedial hypothalamus) --> Risk aversion
 * - AUTONOMY drive --> Competitive independence
 * - Drive balance --> Nash equilibrium approximation
 *
 * FEP INTEGRATION:
 * ```
 * Drive State (d) --> Strategy Prior (pi)
 *         |
 *         v
 * Game Outcome (o) --> Prediction Error (epsilon = o - E[o|d])
 *         |
 *         v
 * Free Energy F = Complexity + Inaccuracy
 *         |
 *         v
 * Drive Update --> Strategy Adaptation
 *         |
 *         v
 * Active Inference --> Cooperation/Competition Balance
 * ```
 *
 * DRIVE-STRATEGY MAPPING:
 * - High SOCIAL drive --> Cooperative strategies (low FE for mutual benefit)
 * - High SAFETY drive --> Risk-averse strategies (minimize variance)
 * - High AUTONOMY drive --> Competitive strategies (maximize individual payoff)
 * - Drive conflict --> Mixed strategies (exploration under uncertainty)
 *
 * KEY MAPPINGS:
 * - SOCIAL drive intensity --> Cooperation weight in strategy
 * - Strategy outcome deviation --> Prediction error (drives learning)
 * - Partner behavior mismatch --> Surprise (triggers drive adjustment)
 * - Reciprocal cooperation --> Free energy minimization
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |     HYPOTHALAMUS GAME THEORY - FEP BRIDGE (Drive-Modulated Strategy)    |
 * +=========================================================================+
 * |                                                                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                   FEP SYSTEM                                     |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Free Energy     |     | Surprise        |                    |   |
 * |   |   | Computation     |     | Estimation      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Precision       |     | Belief Update   |                    |   |
 * |   |   | Modulation      |     | (learning)      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |              BIDIRECTIONAL EFFECTS                               |   |
 * |   |                                                                  |   |
 * |   |  FEP --> Game Theory:                                            |   |
 * |   |    * Free energy --> Strategy uncertainty                        |   |
 * |   |    * Surprise --> Partner model update                           |   |
 * |   |    * Precision --> Strategy confidence                           |   |
 * |   |    * Active inference --> Cooperation/defection choice           |   |
 * |   |                                                                  |   |
 * |   |  Game Theory --> FEP:                                            |   |
 * |   |    * Strategy outcomes --> Update generative model               |   |
 * |   |    * Partner behavior --> Observation likelihood                 |   |
 * |   |    * Repeated games --> Temporal precision                       |   |
 * |   |    * Nash deviation --> Prediction error                         |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                 HYPOTHALAMUS DRIVE SYSTEM                        |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | SOCIAL Drive    |     | SAFETY Drive    |                    |   |
 * |   |   | (cooperation)   |     | (risk aversion) |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | AUTONOMY Drive  |     | Drive Priority  |                    |   |
 * |   |   | (competition)   |     | Resolution      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_GAME_THEORY_FEP_BRIDGE_H
#define NIMCP_HYPO_GAME_THEORY_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Free energy thresholds for strategy selection */
#define HYPO_GT_FEP_COOPERATIVE_THRESHOLD     2.0f    /**< FE below this favors cooperation */
#define HYPO_GT_FEP_MIXED_THRESHOLD           5.0f    /**< FE in range favors mixed strategy */
#define HYPO_GT_FEP_COMPETITIVE_THRESHOLD     10.0f   /**< FE above this favors competition */

/** Precision bounds for strategy confidence */
#define HYPO_GT_FEP_MIN_PRECISION             0.1f    /**< Minimum precision */
#define HYPO_GT_FEP_MAX_PRECISION             10.0f   /**< Maximum precision */
#define HYPO_GT_FEP_DEFAULT_PRECISION         1.0f    /**< Default precision */

/** Learning rate defaults */
#define HYPO_GT_FEP_DEFAULT_LEARNING_RATE     0.01f   /**< Belief update rate */
#define HYPO_GT_FEP_PRECISION_LEARNING_RATE   0.05f   /**< Precision adaptation rate */

/** Feature dimensions */
#define HYPO_GT_FEP_DRIVE_DIM                 4       /**< Drive feature dimension (SOCIAL, SAFETY, AUTONOMY, COMPETENCE) */
#define HYPO_GT_FEP_STRATEGY_DIM              8       /**< Strategy feature dimension */
#define HYPO_GT_FEP_PARTNER_DIM               8       /**< Partner model dimension */

/** Bio-async module ID */
#define BIO_MODULE_HYPO_GT_FEP                0x0A10  /**< Hypothalamus game theory FEP bridge */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Strategy type classification based on drive-FEP integration
 *
 * WHAT: Classification of strategy types based on hypothalamic drive state
 * WHY:  Different drive configurations lead to different game strategies
 * HOW:  Map drive priorities to strategy categories
 */
typedef enum {
    HYPO_GT_STRATEGY_COOPERATIVE = 0,  /**< High SOCIAL drive: maximize mutual benefit */
    HYPO_GT_STRATEGY_COMPETITIVE,      /**< High AUTONOMY drive: maximize individual payoff */
    HYPO_GT_STRATEGY_CAUTIOUS,         /**< High SAFETY drive: minimize risk */
    HYPO_GT_STRATEGY_MIXED,            /**< Drive conflict: exploration mode */
    HYPO_GT_STRATEGY_RECIPROCAL        /**< Tit-for-tat: mirror partner behavior */
} hypo_gt_strategy_type_t;

/**
 * @brief Partner behavior classification
 *
 * WHAT: Classification of observed partner behavior
 * WHY:  Partner model is key for strategy selection
 * HOW:  Classify based on prediction error patterns
 */
typedef enum {
    HYPO_GT_PARTNER_UNKNOWN = 0,       /**< Insufficient data */
    HYPO_GT_PARTNER_COOPERATIVE,       /**< Consistently cooperative */
    HYPO_GT_PARTNER_COMPETITIVE,       /**< Consistently competitive */
    HYPO_GT_PARTNER_MIXED,             /**< Variable behavior */
    HYPO_GT_PARTNER_EXPLOITATIVE       /**< Exploits cooperation */
} hypo_gt_partner_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus game theory FEP configuration
 *
 * WHAT: Configuration for drive-modulated game-theoretic strategy selection
 * WHY:  Tune how drives influence strategy and how FEP guides learning
 * HOW:  Adjustable weights, thresholds, and learning parameters
 */
typedef struct {
    /* Drive-FEP coupling weights */
    float drive_fe_weight;             /**< Weight of drive state on free energy */
    float prediction_error_gain;       /**< Gain for prediction error computation */
    float precision_modulation;        /**< Precision modulation factor */

    /* Strategy parameters */
    float cooperation_threshold;       /**< FE threshold for cooperation */
    float competition_threshold;       /**< FE threshold for competition */
    float exploration_rate;            /**< Rate of strategy exploration */

    /* Drive influence weights */
    float social_weight;               /**< SOCIAL drive influence on strategy */
    float safety_weight;               /**< SAFETY drive influence on strategy */
    float autonomy_weight;             /**< AUTONOMY drive influence on strategy */
    float competence_weight;           /**< COMPETENCE drive influence on strategy */

    /* Learning parameters */
    bool enable_online_learning;       /**< Enable partner model learning */
    float learning_rate;               /**< Partner model update rate */
    float precision_learning_rate;     /**< Precision adaptation rate */

    /* Active inference */
    bool enable_active_inference;      /**< Enable active strategy selection */
    float action_temperature;          /**< Softmax temperature for action selection */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */

    /* Sensitivity factors */
    float fep_sensitivity;             /**< FEP effect scaling [0.5-2.0] */
    float drive_sensitivity;           /**< Drive effect scaling [0.5-2.0] */
} hypo_gt_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on game theory strategy (FEP --> Strategy)
 *
 * WHAT: How FEP analysis influences strategy selection
 * WHY:  Translate FEP metrics to actionable strategy parameters
 * HOW:  Map free energy, surprise, precision to strategy weights
 */
typedef struct {
    /* Strategy scores */
    float cooperation_score;           /**< Drive-weighted cooperation tendency [0-1] */
    float competition_score;           /**< Drive-weighted competition tendency [0-1] */
    float exploration_score;           /**< Uncertainty-driven exploration [0-1] */

    /* FEP metrics */
    float free_energy;                 /**< Current free energy level */
    float prediction_error;            /**< Current prediction error */
    float precision;                   /**< Current precision level */
    float active_inference_strength;   /**< Strength of active inference */

    /* Strategy recommendation */
    hypo_gt_strategy_type_t recommended_strategy;  /**< Recommended strategy type */
    float strategy_confidence;         /**< Confidence in recommendation [0-1] */

    /* Partner model */
    hypo_gt_partner_type_t partner_type;  /**< Inferred partner type */
    float partner_model_confidence;    /**< Confidence in partner model [0-1] */
} fep_to_gt_effects_t;

/**
 * @brief Game theory effects on FEP (Strategy --> FEP)
 *
 * WHAT: How game outcomes update FEP beliefs
 * WHY:  Feedback loop for online learning and adaptation
 * HOW:  Convert strategy outcomes to FEP observations
 */
typedef struct {
    /* Outcome counts */
    uint64_t cooperative_outcomes;     /**< Mutual cooperation count */
    uint64_t competitive_outcomes;     /**< Competitive outcome count */
    uint64_t exploited_count;          /**< Times exploited by partner */
    uint64_t successful_exploits;      /**< Successful exploitation count */

    /* Aggregated metrics */
    float avg_payoff;                  /**< Average payoff received */
    float avg_partner_payoff;          /**< Average partner payoff */
    float reciprocity_score;           /**< Measure of reciprocal behavior */

    /* Drive updates */
    float social_drive_update;         /**< Suggested SOCIAL drive change */
    float safety_drive_update;         /**< Suggested SAFETY drive change */
    float autonomy_drive_update;       /**< Suggested AUTONOMY drive change */
} gt_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                       /**< Whether bridge is active */
    uint64_t update_count;             /**< Number of updates */
    uint64_t game_count;               /**< Games processed */
    float current_precision;           /**< Current precision level */
    float avg_free_energy;             /**< Running average free energy */
    float avg_prediction_error;        /**< Running average prediction error */
    hypo_gt_strategy_type_t last_strategy;  /**< Last selected strategy */
    hypo_gt_partner_type_t last_partner;    /**< Last inferred partner type */
    uint64_t last_update_time;         /**< Timestamp of last update */
} hypo_gt_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Game statistics */
    uint64_t total_games;              /**< Total games processed */
    uint64_t cooperative_games;        /**< Games with cooperation */
    uint64_t competitive_games;        /**< Games with competition */

    /* Strategy distribution */
    uint64_t strategy_cooperative;     /**< Times cooperative strategy used */
    uint64_t strategy_competitive;     /**< Times competitive strategy used */
    uint64_t strategy_cautious;        /**< Times cautious strategy used */
    uint64_t strategy_mixed;           /**< Times mixed strategy used */
    uint64_t strategy_reciprocal;      /**< Times reciprocal strategy used */

    /* FEP metrics */
    float avg_free_energy;             /**< Average free energy */
    float max_free_energy;             /**< Maximum free energy observed */
    float avg_prediction_error;        /**< Average prediction error */
    float avg_precision;               /**< Average precision */

    /* Performance */
    float avg_payoff;                  /**< Average payoff achieved */
    float cooperation_rate;            /**< Rate of cooperative outcomes */
    uint64_t bridge_updates;           /**< Total bridge updates */
    float avg_update_time_us;          /**< Average update time */
} hypo_gt_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus game theory FEP bridge
 *
 * WHAT: Main bridge structure connecting hypothalamic drives to game theory via FEP
 * WHY:  Enable drive-modulated strategy selection with FEP-based learning
 * HOW:  Maintain FEP system, drive refs, bidirectional effects
 */
typedef struct {
    bridge_base_t base;                /**< MUST be first: base bridge */

    /* Configuration */
    hypo_gt_fep_config_t config;       /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;          /**< FEP system */
    hypo_drive_system_handle_t* drive_system;  /**< Hypothalamus drive system */

    /* Bidirectional effects */
    fep_to_gt_effects_t fep_effects;   /**< FEP --> Game Theory effects */
    gt_to_fep_effects_t gt_effects;    /**< Game Theory --> FEP effects */

    /* State */
    hypo_gt_fep_state_t state;         /**< Current state */

    /* Statistics */
    hypo_gt_fep_stats_t stats;         /**< Statistics */

    /* Feature buffers */
    float* drive_features;             /**< Drive feature buffer */
    float* strategy_features;          /**< Strategy feature buffer */
    float* partner_features;           /**< Partner model feature buffer */
} hypo_gt_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for drive-game theory FEP integration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Populate config struct with tested default values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_default_config(hypo_gt_fep_config_t* config);

/**
 * @brief Create hypothalamus game theory FEP bridge
 *
 * WHAT: Initialize FEP integration for drive-modulated game theory
 * WHY:  Enable drive-based strategy selection with FEP learning
 * HOW:  Connect FEP system to drive system, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Hypothalamus drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_gt_fep_bridge_t* hypo_gt_fep_create(
    const hypo_gt_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_gt_fep_destroy(hypo_gt_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Reset all state and statistics
 * WHY:  Allow clean restart without reallocation
 * HOW:  Zero state/stats, reset precision to default
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_reset(hypo_gt_fep_bridge_t* bridge);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Update bridge from current drive state
 *
 * WHAT: Compute FEP-based strategy recommendations from drive state
 * WHY:  Translate drive priorities to game-theoretic strategies
 * HOW:  Process drives through FEP, compute strategy scores
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_update(hypo_gt_fep_bridge_t* bridge);

/**
 * @brief Compute free energy for current drive-strategy state
 *
 * WHAT: Calculate free energy from drive-strategy alignment
 * WHY:  FE quantifies deviation from expected drive-strategy mapping
 * HOW:  Compare current strategy to drive-predicted strategy
 *
 * @param bridge Bridge handle
 * @param free_energy Output free energy value
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_compute_fe(
    hypo_gt_fep_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Modulate precision based on game outcomes
 *
 * WHAT: Adjust precision based on prediction accuracy
 * WHY:  Improve strategy selection over time
 * HOW:  Increase precision when predictions accurate, decrease otherwise
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_modulate_precision(hypo_gt_fep_bridge_t* bridge);

/**
 * @brief Get current FEP effects
 *
 * WHAT: Retrieve current FEP effects on game theory
 * WHY:  Allow external systems to query strategy recommendations
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_get_effects(
    const hypo_gt_fep_bridge_t* bridge,
    fep_to_gt_effects_t* effects
);

/* ============================================================================
 * Strategy Selection API
 * ============================================================================ */

/**
 * @brief Select strategy based on current drive state and FEP
 *
 * WHAT: Choose game strategy based on drive priorities and FEP analysis
 * WHY:  Core function for drive-modulated game-theoretic behavior
 * HOW:  Combine drive weights with FEP uncertainty for strategy selection
 *
 * @param bridge Bridge handle
 * @param strategy Output selected strategy type
 * @param confidence Output confidence in selection [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_select_strategy(
    hypo_gt_fep_bridge_t* bridge,
    hypo_gt_strategy_type_t* strategy,
    float* confidence
);

/**
 * @brief Update from game outcome
 *
 * WHAT: Process game outcome for learning
 * WHY:  Enable FEP belief updates from experience
 * HOW:  Compute prediction error, update partner model and drives
 *
 * @param bridge Bridge handle
 * @param own_payoff Payoff received
 * @param partner_payoff Partner's payoff
 * @param partner_cooperated Whether partner cooperated
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_update_from_outcome(
    hypo_gt_fep_bridge_t* bridge,
    float own_payoff,
    float partner_payoff,
    bool partner_cooperated
);

/**
 * @brief Infer partner type from observation history
 *
 * WHAT: Classify partner based on observed behavior
 * WHY:  Partner model crucial for strategy selection
 * HOW:  Analyze observation patterns via FEP
 *
 * @param bridge Bridge handle
 * @param partner_type Output inferred partner type
 * @param confidence Output confidence in inference [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_infer_partner(
    hypo_gt_fep_bridge_t* bridge,
    hypo_gt_partner_type_t* partner_type,
    float* confidence
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_get_state(
    const hypo_gt_fep_bridge_t* bridge,
    hypo_gt_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_get_stats(
    const hypo_gt_fep_bridge_t* bridge,
    hypo_gt_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module game theory notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_connect_bio_async(hypo_gt_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_gt_fep_disconnect_bio_async(hypo_gt_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handle incoming bio-async messages
 * WHY:  React to other modules' game-theory-relevant events
 * HOW:  Dequeue and process messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int hypo_gt_fep_process_messages(
    hypo_gt_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge handle
 */
void hypo_gt_fep_print_summary(const hypo_gt_fep_bridge_t* bridge);

/**
 * @brief Convert strategy type to string
 *
 * @param strategy Strategy type
 * @return Human-readable string
 */
const char* hypo_gt_strategy_to_string(hypo_gt_strategy_type_t strategy);

/**
 * @brief Convert partner type to string
 *
 * @param partner Partner type
 * @return Human-readable string
 */
const char* hypo_gt_partner_to_string(hypo_gt_partner_type_t partner);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_GAME_THEORY_FEP_BRIDGE_H */
