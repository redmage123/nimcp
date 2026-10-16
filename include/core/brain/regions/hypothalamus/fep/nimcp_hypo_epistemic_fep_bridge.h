/**
 * @file nimcp_hypo_epistemic_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Epistemic Integration
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for epistemic processing with hypothalamic drive modulation
 * WHY:  Drive state shapes confidence bounds; uncertainty maps to free energy
 * HOW:  Map drive state to epistemic parameters, use FEP for uncertainty quantification
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EPISTEMIC PROCESSING AS DRIVE-MODULATED CERTAINTY:
 * - CURIOSITY drive --> Information-seeking (reduces uncertainty)
 * - SAFETY drive --> Caution threshold (higher = more conservative bounds)
 * - COMPETENCE drive --> Confidence calibration
 * - Drive urgency --> Epistemic urgency (how quickly to resolve uncertainty)
 *
 * FEP INTEGRATION:
 * ```
 * Drive State (d) --> Epistemic Prior (confidence bounds)
 *         |
 *         v
 * Information (I) --> Belief Update (posterior)
 *         |
 *         v
 * Uncertainty: H = -sum(p log p)
 *         |
 *         v
 * Free Energy F = Expected Uncertainty + Complexity
 *         |
 *         v
 * Epistemic Action --> Information Gathering (active inference)
 * ```
 *
 * DRIVE-EPISTEMIC MAPPING:
 * - High CURIOSITY drive --> Active information seeking (low FE tolerance)
 * - High SAFETY drive --> Conservative confidence bounds (wider intervals)
 * - High COMPETENCE drive --> Tighter calibration (accurate credences)
 * - Low drive urgency --> Accept higher uncertainty (lazy evaluation)
 *
 * KEY MAPPINGS:
 * - Drive intensity --> Epistemic urgency (resolve uncertainty faster)
 * - Belief uncertainty --> Free energy (uncertain = high FE)
 * - Confidence miscalibration --> Prediction error
 * - Information gain --> Surprise reduction (drives learning)
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |   HYPOTHALAMUS EPISTEMIC - FEP BRIDGE (Drive-Modulated Uncertainty)     |
 * +=========================================================================+
 * |                                                                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                   FEP SYSTEM                                     |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Free Energy     |     | Surprise        |                    |   |
 * |   |   | (uncertainty)   |     | (info gain)     |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Precision       |     | Belief Update   |                    |   |
 * |   |   | (confidence)    |     | (learning)      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |              BIDIRECTIONAL EFFECTS                               |   |
 * |   |                                                                  |   |
 * |   |  FEP --> Epistemic:                                              |   |
 * |   |    * Free energy --> Uncertainty estimate                        |   |
 * |   |    * Surprise --> Information value                              |   |
 * |   |    * Precision --> Confidence level                              |   |
 * |   |    * Active inference --> Information seeking                    |   |
 * |   |                                                                  |   |
 * |   |  Epistemic --> FEP:                                              |   |
 * |   |    * Belief content --> Update world model                       |   |
 * |   |    * Confidence bounds --> Observation precision                 |   |
 * |   |    * Information gained --> Reduce expected FE                   |   |
 * |   |    * Knowledge gaps --> High-value observations                  |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |                 HYPOTHALAMUS DRIVE SYSTEM                        |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | CURIOSITY Drive |     | SAFETY Drive    |                    |   |
 * |   |   | (info seeking)  |     | (caution)       |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | COMPETENCE Drive|     | Drive Priority  |                    |   |
 * |   |   | (calibration)   |     | Resolution      |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_EPISTEMIC_FEP_BRIDGE_H
#define NIMCP_HYPO_EPISTEMIC_FEP_BRIDGE_H

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

/** Free energy thresholds for epistemic state */
#define HYPO_EPI_FEP_CERTAIN_THRESHOLD        1.0f    /**< FE below: high certainty */
#define HYPO_EPI_FEP_CONFIDENT_THRESHOLD      3.0f    /**< FE in range: confident */
#define HYPO_EPI_FEP_UNCERTAIN_THRESHOLD      7.0f    /**< FE in range: uncertain */
#define HYPO_EPI_FEP_IGNORANT_THRESHOLD       15.0f   /**< FE above: high uncertainty */

/** Precision bounds for confidence */
#define HYPO_EPI_FEP_MIN_PRECISION            0.1f    /**< Minimum precision */
#define HYPO_EPI_FEP_MAX_PRECISION            10.0f   /**< Maximum precision */
#define HYPO_EPI_FEP_DEFAULT_PRECISION        1.0f    /**< Default precision */

/** Learning rate defaults */
#define HYPO_EPI_FEP_DEFAULT_LEARNING_RATE    0.01f   /**< Belief update rate */
#define HYPO_EPI_FEP_PRECISION_LEARNING_RATE  0.05f   /**< Precision adaptation rate */

/** Feature dimensions */
#define HYPO_EPI_FEP_DRIVE_DIM                4       /**< Drive feature dimension */
#define HYPO_EPI_FEP_BELIEF_DIM               16      /**< Belief state dimension */
#define HYPO_EPI_FEP_EVIDENCE_DIM             16      /**< Evidence dimension */

/** Bio-async module ID */
#define BIO_MODULE_HYPO_EPI_FEP               0x0A12  /**< Hypothalamus epistemic FEP bridge */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Epistemic state classification based on drive-FEP integration
 *
 * WHAT: Classification of epistemic states based on uncertainty level
 * WHY:  Different uncertainty levels require different decision strategies
 * HOW:  Map free energy to epistemic categories
 */
typedef enum {
    HYPO_EPI_STATE_CERTAIN = 0,        /**< Very low FE: near-certain knowledge */
    HYPO_EPI_STATE_CONFIDENT,          /**< Low FE: confident belief */
    HYPO_EPI_STATE_PROBABLE,           /**< Moderate FE: probabilistic belief */
    HYPO_EPI_STATE_UNCERTAIN,          /**< High FE: significant uncertainty */
    HYPO_EPI_STATE_IGNORANT            /**< Very high FE: near-complete uncertainty */
} hypo_epi_state_t;

/**
 * @brief Epistemic action recommendation
 *
 * WHAT: Recommended action based on epistemic state
 * WHY:  Different epistemic states call for different responses
 * HOW:  Map uncertainty + drives to action
 */
typedef enum {
    HYPO_EPI_ACTION_ACT = 0,           /**< Sufficient certainty: proceed with action */
    HYPO_EPI_ACTION_SEEK_INFO,         /**< Moderate uncertainty: gather information */
    HYPO_EPI_ACTION_WAIT,              /**< High uncertainty: wait for more data */
    HYPO_EPI_ACTION_HEDGE,             /**< Uncertainty + stakes: hedge bets */
    HYPO_EPI_ACTION_EXPLORE            /**< High curiosity: explore actively */
} hypo_epi_action_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus epistemic FEP configuration
 *
 * WHAT: Configuration for drive-modulated epistemic processing via FEP
 * WHY:  Tune how drives influence epistemic parameters and uncertainty handling
 * HOW:  Adjustable weights, thresholds, and learning parameters
 */
typedef struct {
    /* Drive-FEP coupling weights */
    float drive_fe_weight;             /**< Weight of drive state on free energy */
    float prediction_error_gain;       /**< Gain for prediction error computation */
    float precision_modulation;        /**< Precision modulation factor */

    /* Epistemic parameters */
    float certainty_threshold;         /**< FE threshold for certainty */
    float ignorance_threshold;         /**< FE threshold for ignorance */
    float info_value_weight;           /**< Weight of information value */

    /* Drive influence weights */
    float curiosity_weight;            /**< CURIOSITY drive influence (info seeking) */
    float safety_weight;               /**< SAFETY drive influence (caution) */
    float competence_weight;           /**< COMPETENCE drive influence (calibration) */

    /* Confidence calibration */
    float overconfidence_penalty;      /**< Penalty for overconfidence */
    float underconfidence_penalty;     /**< Penalty for underconfidence */
    float calibration_target;          /**< Target calibration accuracy */

    /* Learning parameters */
    bool enable_online_learning;       /**< Enable belief learning */
    float learning_rate;               /**< Belief update rate */
    float precision_learning_rate;     /**< Precision adaptation rate */

    /* Active inference */
    bool enable_active_inference;      /**< Enable active information seeking */
    float action_temperature;          /**< Softmax temperature for actions */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */

    /* Sensitivity factors */
    float fep_sensitivity;             /**< FEP effect scaling [0.5-2.0] */
    float drive_sensitivity;           /**< Drive effect scaling [0.5-2.0] */
} hypo_epi_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on epistemic processing (FEP --> Epistemic)
 *
 * WHAT: How FEP analysis influences epistemic parameters
 * WHY:  Translate FEP metrics to epistemic parameters
 * HOW:  Map free energy, surprise, precision to confidence bounds
 */
typedef struct {
    /* Epistemic state */
    hypo_epi_state_t current_state;    /**< Current epistemic state */
    float uncertainty_level;           /**< Current uncertainty [0-1] */
    float confidence_level;            /**< Current confidence [0-1] */

    /* FEP metrics */
    float free_energy;                 /**< Current free energy level */
    float prediction_error;            /**< Current prediction error */
    float precision;                   /**< Current precision level */
    float active_inference_strength;   /**< Strength of active inference */

    /* Confidence bounds */
    float lower_bound;                 /**< Lower confidence bound */
    float upper_bound;                 /**< Upper confidence bound */
    float bound_width;                 /**< Width of confidence interval */

    /* Information value */
    float info_value;                  /**< Value of additional information */
    float info_urgency;                /**< Urgency of information seeking */

    /* Recommended action */
    hypo_epi_action_t recommended_action;  /**< Recommended epistemic action */

    /* Drive influence */
    float curiosity_influence;         /**< Current curiosity influence */
    float safety_influence;            /**< Current safety influence */
    float competence_influence;        /**< Current competence influence */
} fep_to_epi_effects_t;

/**
 * @brief Epistemic effects on FEP (Epistemic --> FEP)
 *
 * WHAT: How epistemic updates feed back to FEP
 * WHY:  Beliefs and evidence update the generative model
 * HOW:  Convert epistemic events to FEP observations
 */
typedef struct {
    /* Update counts */
    uint64_t beliefs_updated;          /**< Total beliefs updated */
    uint64_t evidence_received;        /**< Evidence instances received */
    uint64_t info_actions_taken;       /**< Information-seeking actions taken */
    uint64_t decisions_made;           /**< Decisions made under uncertainty */

    /* Outcome tracking */
    uint64_t correct_predictions;      /**< Predictions that proved correct */
    uint64_t incorrect_predictions;    /**< Predictions that proved wrong */
    uint64_t calibration_checks;       /**< Calibration assessment events */

    /* Aggregated metrics */
    float avg_uncertainty;             /**< Average uncertainty level */
    float avg_info_value;              /**< Average information value */
    float calibration_score;           /**< Calibration accuracy [0-1] */

    /* Drive updates */
    float curiosity_drive_update;      /**< Suggested CURIOSITY drive change */
    float competence_drive_update;     /**< Suggested COMPETENCE drive change */
} epi_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                       /**< Whether bridge is active */
    uint64_t update_count;             /**< Number of updates */
    uint64_t belief_count;             /**< Beliefs processed */
    float current_precision;           /**< Current precision level */
    float avg_free_energy;             /**< Running average free energy */
    float avg_prediction_error;        /**< Running average prediction error */
    hypo_epi_state_t current_state;    /**< Current epistemic state */
    hypo_epi_action_t last_action;     /**< Last recommended action */
    uint64_t last_update_time;         /**< Timestamp of last update */
} hypo_epi_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Epistemic state distribution */
    uint64_t state_certain;            /**< Time in certain state */
    uint64_t state_confident;          /**< Time in confident state */
    uint64_t state_probable;           /**< Time in probable state */
    uint64_t state_uncertain;          /**< Time in uncertain state */
    uint64_t state_ignorant;           /**< Time in ignorant state */

    /* Action distribution */
    uint64_t action_act;               /**< Act decisions */
    uint64_t action_seek_info;         /**< Information seeking */
    uint64_t action_wait;              /**< Wait decisions */
    uint64_t action_hedge;             /**< Hedge decisions */
    uint64_t action_explore;           /**< Explore decisions */

    /* FEP metrics */
    float avg_free_energy;             /**< Average free energy */
    float max_free_energy;             /**< Maximum free energy observed */
    float avg_prediction_error;        /**< Average prediction error */
    float avg_precision;               /**< Average precision */

    /* Quality metrics */
    float calibration_score;           /**< Overall calibration accuracy */
    float avg_bound_width;             /**< Average confidence interval width */
    float prediction_accuracy;         /**< Prediction accuracy */

    /* Performance */
    uint64_t bridge_updates;           /**< Total bridge updates */
    float avg_update_time_us;          /**< Average update time */
} hypo_epi_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus epistemic FEP bridge
 *
 * WHAT: Main bridge structure connecting hypothalamic drives to epistemic via FEP
 * WHY:  Enable drive-modulated uncertainty quantification with FEP
 * HOW:  Maintain FEP system, drive refs, bidirectional effects
 */
typedef struct {
    bridge_base_t base;                /**< MUST be first: base bridge */

    /* Configuration */
    hypo_epi_fep_config_t config;      /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;          /**< FEP system */
    hypo_drive_system_handle_t* drive_system;  /**< Hypothalamus drive system */

    /* Bidirectional effects */
    fep_to_epi_effects_t fep_effects;  /**< FEP --> Epistemic effects */
    epi_to_fep_effects_t epi_effects;  /**< Epistemic --> FEP effects */

    /* State */
    hypo_epi_fep_state_t state;        /**< Current state */

    /* Statistics */
    hypo_epi_fep_stats_t stats;        /**< Statistics */

    /* Feature buffers */
    float* drive_features;             /**< Drive feature buffer */
    float* belief_features;            /**< Belief state feature buffer */
    float* evidence_features;          /**< Evidence feature buffer */
} hypo_epi_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for drive-epistemic FEP integration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Populate config struct with tested default values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_default_config(hypo_epi_fep_config_t* config);

/**
 * @brief Create hypothalamus epistemic FEP bridge
 *
 * WHAT: Initialize FEP integration for drive-modulated epistemic processing
 * WHY:  Enable drive-based uncertainty quantification with FEP
 * HOW:  Connect FEP system to drive system, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Hypothalamus drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_epi_fep_bridge_t* hypo_epi_fep_create(
    const hypo_epi_fep_config_t* config,
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
void hypo_epi_fep_destroy(hypo_epi_fep_bridge_t* bridge);

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
int hypo_epi_fep_reset(hypo_epi_fep_bridge_t* bridge);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Update bridge from current drive state
 *
 * WHAT: Compute FEP-based epistemic parameters from drive state
 * WHY:  Translate drive priorities to confidence bounds
 * HOW:  Process drives through FEP, determine epistemic state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_update(hypo_epi_fep_bridge_t* bridge);

/**
 * @brief Compute free energy for current epistemic state
 *
 * WHAT: Calculate free energy representing uncertainty
 * WHY:  FE quantifies epistemic uncertainty
 * HOW:  Combine belief entropy with complexity
 *
 * @param bridge Bridge handle
 * @param free_energy Output free energy value
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_compute_fe(
    hypo_epi_fep_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Modulate precision based on calibration
 *
 * WHAT: Adjust precision based on prediction calibration
 * WHY:  Improve confidence accuracy over time
 * HOW:  Increase precision when well-calibrated, decrease otherwise
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_modulate_precision(hypo_epi_fep_bridge_t* bridge);

/**
 * @brief Get current FEP effects
 *
 * WHAT: Retrieve current FEP effects on epistemic processing
 * WHY:  Allow external systems to query uncertainty parameters
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_get_effects(
    const hypo_epi_fep_bridge_t* bridge,
    fep_to_epi_effects_t* effects
);

/* ============================================================================
 * Epistemic API
 * ============================================================================ */

/**
 * @brief Compute confidence bounds for a belief
 *
 * WHAT: Compute confidence interval based on FEP and drives
 * WHY:  Core function for uncertainty quantification
 * HOW:  Use FEP precision and drive state to set bounds
 *
 * @param bridge Bridge handle
 * @param belief_features Belief feature vector
 * @param belief_len Length of belief vector
 * @param lower_bound Output lower confidence bound
 * @param upper_bound Output upper confidence bound
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_compute_bounds(
    hypo_epi_fep_bridge_t* bridge,
    const float* belief_features,
    uint32_t belief_len,
    float* lower_bound,
    float* upper_bound
);

/**
 * @brief Compute information value
 *
 * WHAT: Estimate value of acquiring additional information
 * WHY:  Guide active information seeking
 * HOW:  Expected free energy reduction from information
 *
 * @param bridge Bridge handle
 * @param info_value Output information value
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_compute_info_value(
    hypo_epi_fep_bridge_t* bridge,
    float* info_value
);

/**
 * @brief Update from evidence
 *
 * WHAT: Process new evidence and update beliefs
 * WHY:  Enable Bayesian belief updating via FEP
 * HOW:  Process evidence through FEP, update precision
 *
 * @param bridge Bridge handle
 * @param evidence_features Evidence feature vector
 * @param evidence_len Length of evidence vector
 * @param evidence_weight Weight/reliability of evidence [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_update_from_evidence(
    hypo_epi_fep_bridge_t* bridge,
    const float* evidence_features,
    uint32_t evidence_len,
    float evidence_weight
);

/**
 * @brief Check calibration against outcome
 *
 * WHAT: Assess calibration based on prediction outcome
 * WHY:  Enable calibration learning
 * HOW:  Compare predicted confidence to actual outcome
 *
 * @param bridge Bridge handle
 * @param predicted_confidence Confidence at time of prediction
 * @param outcome_correct Whether prediction was correct
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_check_calibration(
    hypo_epi_fep_bridge_t* bridge,
    float predicted_confidence,
    bool outcome_correct
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
int hypo_epi_fep_get_state(
    const hypo_epi_fep_bridge_t* bridge,
    hypo_epi_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_get_stats(
    const hypo_epi_fep_bridge_t* bridge,
    hypo_epi_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module epistemic notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_connect_bio_async(hypo_epi_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_epi_fep_disconnect_bio_async(hypo_epi_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Handle incoming bio-async messages
 * WHY:  React to other modules' epistemic-relevant events
 * HOW:  Dequeue and process messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed, or -1 on error
 */
int hypo_epi_fep_process_messages(
    hypo_epi_fep_bridge_t* bridge,
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
void hypo_epi_fep_print_summary(const hypo_epi_fep_bridge_t* bridge);

/**
 * @brief Convert epistemic state to string
 *
 * @param state Epistemic state
 * @return Human-readable string
 */
const char* hypo_epi_state_to_string(hypo_epi_state_t state);

/**
 * @brief Convert epistemic action to string
 *
 * @param action Epistemic action
 * @return Human-readable string
 */
const char* hypo_epi_action_to_string(hypo_epi_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_EPISTEMIC_FEP_BRIDGE_H */
