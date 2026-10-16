/**
 * @file nimcp_hypo_imagination_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Imagination Integration
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for imagination with hypothalamic drive modulation
 * WHY:  Drives shape scenario priority; fantasy vs reality as prediction error
 * HOW:  Map drive state to imagination scenarios, use FEP for reality grounding
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMAGINATION AS DRIVE-MODULATED SIMULATION:
 * - CURIOSITY drive --> Novel scenario generation
 * - SAFETY drive --> Threat scenario simulation (anxiety-related)
 * - SOCIAL drive --> Social scenario imagination
 * - Drive intensity --> Scenario vividness and priority
 *
 * FEP INTEGRATION:
 * ```
 * Drive State (d) --> Scenario Prior (which scenarios to imagine)
 *         |
 *         v
 * Imagined Scenario (s) --> Reality Check (sensory prediction)
 *         |
 *         v
 * Prediction Error: epsilon = s - reality
 *         |
 *         v
 * Free Energy F = Fantasy-Reality Gap
 *         |
 *         v
 * Scenario Adjustment --> Constrain imagination to plausible
 *         |
 *         v
 * Active Inference --> Goal-directed imagination (problem solving)
 * ```
 *
 * DRIVE-IMAGINATION MAPPING:
 * - High CURIOSITY drive --> Exploratory/novel scenarios (high creativity)
 * - High SAFETY drive --> Risk/threat scenarios (worst-case planning)
 * - High SOCIAL drive --> Interpersonal scenarios (theory of mind)
 * - Low drive urgency --> Reality-constrained imagination (practical)
 *
 * KEY MAPPINGS:
 * - Drive intensity --> Scenario generation priority
 * - Fantasy-reality gap --> Prediction error (triggers grounding)
 * - Scenario plausibility --> Free energy (implausible = high FE)
 * - Goal-directed imagination --> Active inference (policy optimization)
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |   HYPOTHALAMUS IMAGINATION - FEP BRIDGE (Drive-Modulated Simulation)    |
 * +=========================================================================+
 * |                                                                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                   FEP SYSTEM                                     |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Free Energy     |     | Surprise        |                    |   |
 * |   |   | (reality gap)   |     | (novelty)       |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Precision       |     | Belief Update   |                    |   |
 * |   |   | (confidence)    |     | (plausibility)  |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |              BIDIRECTIONAL EFFECTS                               |   |
 * |   |                                                                  |   |
 * |   |  FEP --> Imagination:                                            |   |
 * |   |    * Free energy --> Scenario plausibility constraint            |   |
 * |   |    * Surprise --> Novelty bonus/penalty                          |   |
 * |   |    * Precision --> Scenario detail level                         |   |
 * |   |    * Active inference --> Goal-directed imagination              |   |
 * |   |                                                                  |   |
 * |   |  Imagination --> FEP:                                            |   |
 * |   |    * Scenario content --> Update world model                     |   |
 * |   |    * Predicted outcomes --> Future state priors                  |   |
 * |   |    * Fantasy elements --> Observation uncertainty                |   |
 * |   |    * Problem solutions --> Policy candidates                     |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                 HYPOTHALAMUS DRIVE SYSTEM                        |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | CURIOSITY Drive |     | SAFETY Drive    |                    |   |
 * |   |   | (exploration)   |     | (threat sim)    |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | SOCIAL Drive    |     | Drive Priority  |                    |   |
 * |   |   | (social sim)    |     | Resolution      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_IMAGINATION_FEP_BRIDGE_H
#define NIMCP_HYPO_IMAGINATION_FEP_BRIDGE_H

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

/** Free energy thresholds for imagination mode */
#define HYPO_IMG_FEP_REALISTIC_THRESHOLD      2.0f    /**< FE below: reality-constrained */
#define HYPO_IMG_FEP_CREATIVE_THRESHOLD       5.0f    /**< FE in range: creative but plausible */
#define HYPO_IMG_FEP_FANTASY_THRESHOLD        10.0f   /**< FE above: unconstrained fantasy */

/** Precision bounds for scenario confidence */
#define HYPO_IMG_FEP_MIN_PRECISION            0.1f    /**< Minimum precision */
#define HYPO_IMG_FEP_MAX_PRECISION            10.0f   /**< Maximum precision */
#define HYPO_IMG_FEP_DEFAULT_PRECISION        1.0f    /**< Default precision */

/** Learning rate defaults */
#define HYPO_IMG_FEP_DEFAULT_LEARNING_RATE    0.01f   /**< Belief update rate */
#define HYPO_IMG_FEP_PRECISION_LEARNING_RATE  0.05f   /**< Precision adaptation rate */

/** Feature dimensions */
#define HYPO_IMG_FEP_DRIVE_DIM                4       /**< Drive feature dimension */
#define HYPO_IMG_FEP_SCENARIO_DIM             16      /**< Scenario feature dimension */
#define HYPO_IMG_FEP_REALITY_DIM              16      /**< Reality model dimension */

/** Bio-async module ID */
#define BIO_MODULE_HYPO_IMG_FEP               0x0A11  /**< Hypothalamus imagination FEP bridge */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Imagination mode classification based on drive-FEP integration
 *
 * WHAT: Classification of imagination modes based on hypothalamic drive state
 * WHY:  Different drive configurations lead to different imagination styles
 * HOW:  Map drive priorities to imagination categories
 */
typedef enum {
    HYPO_IMG_MODE_REALISTIC = 0,       /**< Low FE: reality-constrained simulation */
    HYPO_IMG_MODE_CREATIVE,            /**< Moderate FE: creative but plausible */
    HYPO_IMG_MODE_EXPLORATORY,         /**< High curiosity: novel scenario exploration */
    HYPO_IMG_MODE_THREAT_SIM,          /**< High safety drive: threat scenario simulation */
    HYPO_IMG_MODE_SOCIAL_SIM,          /**< High social drive: interpersonal simulation */
    HYPO_IMG_MODE_FANTASY              /**< Very high FE: unconstrained imagination */
} hypo_img_mode_t;

/**
 * @brief Scenario plausibility classification
 *
 * WHAT: Classification of imagined scenario plausibility
 * WHY:  Plausibility determines how to use scenario in decision-making
 * HOW:  Classify based on prediction error patterns
 */
typedef enum {
    HYPO_IMG_PLAUS_CERTAIN = 0,        /**< Very likely to occur */
    HYPO_IMG_PLAUS_PROBABLE,           /**< Reasonably likely */
    HYPO_IMG_PLAUS_POSSIBLE,           /**< Could happen */
    HYPO_IMG_PLAUS_UNLIKELY,           /**< Improbable but not impossible */
    HYPO_IMG_PLAUS_FANTASY             /**< Impossible/purely imaginative */
} hypo_img_plausibility_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus imagination FEP configuration
 *
 * WHAT: Configuration for drive-modulated imagination via FEP
 * WHY:  Tune how drives influence imagination and how FEP constrains it
 * HOW:  Adjustable weights, thresholds, and learning parameters
 */
typedef struct {
    /* Drive-FEP coupling weights */
    float drive_fe_weight;             /**< Weight of drive state on free energy */
    float prediction_error_gain;       /**< Gain for prediction error computation */
    float precision_modulation;        /**< Precision modulation factor */

    /* Imagination parameters */
    float realistic_threshold;         /**< FE threshold for realistic mode */
    float fantasy_threshold;           /**< FE threshold for fantasy mode */
    float creativity_factor;           /**< Creativity scaling factor */

    /* Drive influence weights */
    float curiosity_weight;            /**< CURIOSITY drive influence */
    float safety_weight;               /**< SAFETY drive influence */
    float social_weight;               /**< SOCIAL drive influence */

    /* Reality grounding */
    float reality_anchor;              /**< Strength of reality constraint [0-1] */
    float plausibility_threshold;      /**< Min plausibility for practical use */

    /* Learning parameters */
    bool enable_online_learning;       /**< Enable model learning */
    float learning_rate;               /**< Model update rate */
    float precision_learning_rate;     /**< Precision adaptation rate */

    /* Active inference */
    bool enable_active_inference;      /**< Enable goal-directed imagination */
    float action_temperature;          /**< Softmax temperature for actions */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */

    /* Sensitivity factors */
    float fep_sensitivity;             /**< FEP effect scaling [0.5-2.0] */
    float drive_sensitivity;           /**< Drive effect scaling [0.5-2.0] */
} hypo_img_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on imagination (FEP --> Imagination)
 *
 * WHAT: How FEP analysis influences imagination
 * WHY:  Translate FEP metrics to imagination parameters
 * HOW:  Map free energy, surprise, precision to imagination mode
 */
typedef struct {
    /* Mode and creativity */
    hypo_img_mode_t current_mode;      /**< Current imagination mode */
    float creativity_level;            /**< Current creativity [0-1] */
    float reality_anchoring;           /**< How constrained to reality [0-1] */

    /* FEP metrics */
    float free_energy;                 /**< Current free energy level */
    float prediction_error;            /**< Current prediction error */
    float precision;                   /**< Current precision level */
    float active_inference_strength;   /**< Strength of active inference */

    /* Scenario evaluation */
    hypo_img_plausibility_t plausibility;  /**< Current scenario plausibility */
    float plausibility_score;          /**< Numeric plausibility [0-1] */
    float novelty_score;               /**< Scenario novelty [0-1] */
    float utility_score;               /**< Practical utility [0-1] */

    /* Drive influence */
    float curiosity_influence;         /**< Current curiosity influence */
    float safety_influence;            /**< Current safety influence */
    float social_influence;            /**< Current social influence */
} fep_to_img_effects_t;

/**
 * @brief Imagination effects on FEP (Imagination --> FEP)
 *
 * WHAT: How imagination updates FEP beliefs
 * WHY:  Imagination provides hypothetical observations for learning
 * HOW:  Convert imagined scenarios to FEP observations
 */
typedef struct {
    /* Scenario counts */
    uint64_t scenarios_generated;      /**< Total scenarios generated */
    uint64_t realistic_scenarios;      /**< Reality-constrained scenarios */
    uint64_t creative_scenarios;       /**< Creative scenarios */
    uint64_t fantasy_scenarios;        /**< Fantasy/impossible scenarios */

    /* Outcome tracking */
    uint64_t scenarios_validated;      /**< Scenarios later validated */
    uint64_t scenarios_invalidated;    /**< Scenarios proven wrong */
    uint64_t useful_predictions;       /**< Scenarios that aided decisions */

    /* Aggregated metrics */
    float avg_plausibility;            /**< Average scenario plausibility */
    float avg_novelty;                 /**< Average scenario novelty */
    float avg_utility;                 /**< Average practical utility */

    /* Drive updates */
    float curiosity_drive_update;      /**< Suggested CURIOSITY drive change */
    float safety_drive_update;         /**< Suggested SAFETY drive change */
} img_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                       /**< Whether bridge is active */
    uint64_t update_count;             /**< Number of updates */
    uint64_t scenario_count;           /**< Scenarios processed */
    float current_precision;           /**< Current precision level */
    float avg_free_energy;             /**< Running average free energy */
    float avg_prediction_error;        /**< Running average prediction error */
    hypo_img_mode_t current_mode;      /**< Current imagination mode */
    hypo_img_plausibility_t last_plausibility;  /**< Last plausibility rating */
    uint64_t last_update_time;         /**< Timestamp of last update */
} hypo_img_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Scenario statistics */
    uint64_t total_scenarios;          /**< Total scenarios processed */
    uint64_t realistic_count;          /**< Realistic mode count */
    uint64_t creative_count;           /**< Creative mode count */
    uint64_t exploratory_count;        /**< Exploratory mode count */
    uint64_t threat_sim_count;         /**< Threat simulation count */
    uint64_t social_sim_count;         /**< Social simulation count */
    uint64_t fantasy_count;            /**< Fantasy mode count */

    /* FEP metrics */
    float avg_free_energy;             /**< Average free energy */
    float max_free_energy;             /**< Maximum free energy observed */
    float avg_prediction_error;        /**< Average prediction error */
    float avg_precision;               /**< Average precision */

    /* Quality metrics */
    float avg_plausibility;            /**< Average plausibility */
    float avg_novelty;                 /**< Average novelty */
    float avg_utility;                 /**< Average utility */
    float prediction_accuracy;         /**< Accuracy of predictions */

    /* Performance */
    uint64_t bridge_updates;           /**< Total bridge updates */
    float avg_update_time_us;          /**< Average update time */
} hypo_img_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus imagination FEP bridge
 *
 * WHAT: Main bridge structure connecting hypothalamic drives to imagination via FEP
 * WHY:  Enable drive-modulated imagination with FEP-based reality grounding
 * HOW:  Maintain FEP system, drive refs, bidirectional effects
 */
typedef struct {
    bridge_base_t base;                /**< MUST be first: base bridge */

    /* Configuration */
    hypo_img_fep_config_t config;      /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;          /**< FEP system */
    hypo_drive_system_handle_t* drive_system;  /**< Hypothalamus drive system */

    /* Bidirectional effects */
    fep_to_img_effects_t fep_effects;  /**< FEP --> Imagination effects */
    img_to_fep_effects_t img_effects;  /**< Imagination --> FEP effects */

    /* State */
    hypo_img_fep_state_t state;        /**< Current state */

    /* Statistics */
    hypo_img_fep_stats_t stats;        /**< Statistics */

    /* Feature buffers */
    float* drive_features;             /**< Drive feature buffer */
    float* scenario_features;          /**< Scenario feature buffer */
    float* reality_features;           /**< Reality model feature buffer */
} hypo_img_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for drive-imagination FEP integration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Populate config struct with tested default values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_default_config(hypo_img_fep_config_t* config);

/**
 * @brief Create hypothalamus imagination FEP bridge
 *
 * WHAT: Initialize FEP integration for drive-modulated imagination
 * WHY:  Enable drive-based scenario generation with FEP reality grounding
 * HOW:  Connect FEP system to drive system, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Hypothalamus drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_img_fep_bridge_t* hypo_img_fep_create(
    const hypo_img_fep_config_t* config,
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
void hypo_img_fep_destroy(hypo_img_fep_bridge_t* bridge);

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
int hypo_img_fep_reset(hypo_img_fep_bridge_t* bridge);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Update bridge from current drive state
 *
 * WHAT: Compute FEP-based imagination mode from drive state
 * WHY:  Translate drive priorities to imagination parameters
 * HOW:  Process drives through FEP, determine imagination mode
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_update(hypo_img_fep_bridge_t* bridge);

/**
 * @brief Compute free energy for current imagination state
 *
 * WHAT: Calculate free energy from scenario-reality gap
 * WHY:  FE quantifies how fantastical vs realistic imagination is
 * HOW:  Compare imagined scenario to world model
 *
 * @param bridge Bridge handle
 * @param free_energy Output free energy value
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_compute_fe(
    hypo_img_fep_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Modulate precision based on prediction outcomes
 *
 * WHAT: Adjust precision based on imagination accuracy
 * WHY:  Improve imagination quality over time
 * HOW:  Increase precision when predictions validated, decrease otherwise
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_modulate_precision(hypo_img_fep_bridge_t* bridge);

/**
 * @brief Get current FEP effects
 *
 * WHAT: Retrieve current FEP effects on imagination
 * WHY:  Allow external systems to query imagination parameters
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_get_effects(
    const hypo_img_fep_bridge_t* bridge,
    fep_to_img_effects_t* effects
);

/* ============================================================================
 * Imagination API
 * ============================================================================ */

/**
 * @brief Evaluate scenario plausibility
 *
 * WHAT: Assess how plausible an imagined scenario is
 * WHY:  Core function for reality-grounding imagination
 * HOW:  Compute prediction error against world model
 *
 * @param bridge Bridge handle
 * @param scenario_features Scenario feature vector
 * @param scenario_len Length of feature vector
 * @param plausibility Output plausibility classification
 * @param score Output numeric plausibility score [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_evaluate_scenario(
    hypo_img_fep_bridge_t* bridge,
    const float* scenario_features,
    uint32_t scenario_len,
    hypo_img_plausibility_t* plausibility,
    float* score
);

/**
 * @brief Get current imagination mode
 *
 * WHAT: Determine current imagination mode based on drive-FEP state
 * WHY:  External systems may need to know current mode
 * HOW:  Return cached mode from last update
 *
 * @param bridge Bridge handle
 * @param mode Output imagination mode
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_get_mode(
    const hypo_img_fep_bridge_t* bridge,
    hypo_img_mode_t* mode
);

/**
 * @brief Update from scenario validation
 *
 * WHAT: Process validation/invalidation of imagined scenario
 * WHY:  Enable FEP belief updates from real-world feedback
 * HOW:  Compute prediction error, update world model
 *
 * @param bridge Bridge handle
 * @param was_validated Whether scenario was validated
 * @param actual_features Actual outcome features (may be NULL)
 * @param actual_len Length of actual features
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_update_from_validation(
    hypo_img_fep_bridge_t* bridge,
    bool was_validated,
    const float* actual_features,
    uint32_t actual_len
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
int hypo_img_fep_get_state(
    const hypo_img_fep_bridge_t* bridge,
    hypo_img_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_get_stats(
    const hypo_img_fep_bridge_t* bridge,
    hypo_img_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module imagination notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_connect_bio_async(hypo_img_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_img_fep_disconnect_bio_async(hypo_img_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handle incoming bio-async messages
 * WHY:  React to other modules' imagination-relevant events
 * HOW:  Dequeue and process messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int hypo_img_fep_process_messages(
    hypo_img_fep_bridge_t* bridge,
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
void hypo_img_fep_print_summary(const hypo_img_fep_bridge_t* bridge);

/**
 * @brief Convert imagination mode to string
 *
 * @param mode Imagination mode
 * @return Human-readable string
 */
const char* hypo_img_mode_to_string(hypo_img_mode_t mode);

/**
 * @brief Convert plausibility to string
 *
 * @param plausibility Plausibility level
 * @return Human-readable string
 */
const char* hypo_img_plausibility_to_string(hypo_img_plausibility_t plausibility);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_IMAGINATION_FEP_BRIDGE_H */
