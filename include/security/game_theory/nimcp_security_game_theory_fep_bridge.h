/**
 * @file nimcp_security_game_theory_fep_bridge.h
 * @brief Free Energy Principle bridge for Security Game Theory
 * @version 1.0.0
 * @date 2025-01-10
 *
 * WHAT: FEP integration for game-theoretic security - strategy deviation as surprise
 * WHY:  Adversarial strategies are high-surprise observations in FEP framework
 * HOW:  Map strategy deviations to free energy, use prediction errors for detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * GAME THEORY AS PREDICTIVE PROCESSING:
 * - Expected behavior = low free energy (predicted Nash equilibrium)
 * - Manipulation = high free energy (surprising strategy deviations)
 * - Learned payoff matrix = generative model p(o,s)
 * - Detection threshold = surprise threshold
 *
 * FEP INTEGRATION:
 * ```
 * Strategy Observation (o) --> Feature Extraction (deviation analysis)
 *         |
 *         v
 * Expected Strategy mu (Nash equilibrium prediction)
 *         |
 *         v
 * Prediction Error: epsilon = o - g(mu)
 *         |
 *         v
 * Free Energy F = Complexity + Inaccuracy
 *         |
 *         v
 * Surprise = -ln p(o) <= F
 *         |
 *         v
 * Manipulation Score = F / F_threshold
 * ```
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0) --> Normal strategic behavior
 * - Medium FE (2-5) --> Suspicious (flag for review)
 * - High FE (5-10) --> Adversarial (alert)
 * - Very high FE (>10) --> Critical manipulation (quarantine)
 *
 * KEY MAPPINGS:
 * - Strategy deviation score --> Free energy
 * - Payoff manipulation --> Prediction error
 * - Coalition attack probability --> Surprise level
 * - Strategy normalization --> Active inference response
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |        SECURITY GAME THEORY - FEP BRIDGE (Adversarial Detection)        |
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
 * |   |  FEP --> Security Game Theory:                                   |   |
 * |   |    * Free energy --> Manipulation score                          |   |
 * |   |    * Surprise --> Adversarial threshold                          |   |
 * |   |    * Precision --> Detection sensitivity                         |   |
 * |   |    * Active inference --> Strategy normalization                 |   |
 * |   |                                                                  |   |
 * |   |  Security Game Theory --> FEP:                                   |   |
 * |   |    * Detected manipulations --> High-surprise observations       |   |
 * |   |    * Normal strategies --> Update generative model               |   |
 * |   |    * False positives --> Reduce precision                        |   |
 * |   |    * Coalition patterns --> Hierarchical prediction update       |   |
 * |   +------------------------------------------------------------------+   |
 * |                          ^                    |                          |
 * |                          |                    v                          |
 * |   +------------------------------------------------------------------+   |
 * |   |            SECURITY GAME THEORY SYSTEM                           |   |
 * |   |                                                                  |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Strategy        |     | Coalition       |                    |   |
 * |   |   | Monitor         |     | Detector        |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |          |                       |                               |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   |   | Payoff          |     | Mechanism       |                    |   |
 * |   |   | Validator       |     | Verifier        |                    |   |
 * |   |   +-----------------+     +-----------------+                    |   |
 * |   +------------------------------------------------------------------+   |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_GAME_THEORY_FEP_BRIDGE_H
#define NIMCP_SECURITY_GAME_THEORY_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "security/game_theory/nimcp_security_game_theory_bridge.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Strategy deviation thresholds based on free energy */
#define SGT_FEP_NORMAL_THRESHOLD          2.0f    /**< Normal strategic behavior */
#define SGT_FEP_SUSPICIOUS_THRESHOLD      5.0f    /**< Suspicious strategy */
#define SGT_FEP_ADVERSARIAL_THRESHOLD     10.0f   /**< Adversarial behavior */
#define SGT_FEP_CRITICAL_THRESHOLD        20.0f   /**< Critical manipulation */

/** Precision bounds for detection sensitivity */
#define SGT_FEP_MIN_PRECISION             0.1f    /**< Minimum precision */
#define SGT_FEP_MAX_PRECISION             10.0f   /**< Maximum precision */
#define SGT_FEP_DEFAULT_PRECISION         1.0f    /**< Default precision */

/** Learning rate defaults */
#define SGT_FEP_DEFAULT_LEARNING_RATE     0.01f   /**< Belief update rate */
#define SGT_FEP_PRECISION_LEARNING_RATE   0.05f   /**< Precision adaptation rate */

/** FEP observation dimensions */
#define SGT_FEP_STRATEGY_DIM              16      /**< Strategy feature dimension */
#define SGT_FEP_PAYOFF_DIM                16      /**< Payoff feature dimension */
#define SGT_FEP_COALITION_DIM             8       /**< Coalition feature dimension */

/** Bio-async module ID */
#define BIO_MODULE_SECURITY_GT_FEP        0x0630  /**< Security game theory FEP bridge */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Adversarial detection severity levels
 *
 * WHAT: Classification of adversarial behavior based on FEP analysis
 * WHY:  Different severities require different responses
 * HOW:  Map free energy levels to severity categories
 */
typedef enum {
    SGT_FEP_SEVERITY_NONE = 0,        /**< No adversarial behavior detected */
    SGT_FEP_SEVERITY_LOW,             /**< Low severity - minor deviation */
    SGT_FEP_SEVERITY_MEDIUM,          /**< Medium severity - suspicious pattern */
    SGT_FEP_SEVERITY_HIGH,            /**< High severity - likely adversarial */
    SGT_FEP_SEVERITY_CRITICAL         /**< Critical - active manipulation */
} sgt_fep_severity_t;

/**
 * @brief Manipulation type classification
 *
 * WHAT: Types of game-theoretic manipulation detected via FEP
 * WHY:  Different manipulation types need specific countermeasures
 * HOW:  Classify based on prediction error patterns
 */
typedef enum {
    SGT_FEP_MANIP_NONE = 0,           /**< No manipulation detected */
    SGT_FEP_MANIP_STRATEGY_SHIFT,     /**< Unexpected strategy shift */
    SGT_FEP_MANIP_PAYOFF_TAMPERING,   /**< Payoff matrix manipulation */
    SGT_FEP_MANIP_COALITION_ATTACK,   /**< Coordinated coalition attack */
    SGT_FEP_MANIP_EQUILIBRIUM_POISON, /**< Equilibrium computation poison */
    SGT_FEP_MANIP_TIMING_ATTACK,      /**< Timing-based manipulation */
    SGT_FEP_MANIP_SYBIL_DETECTED,     /**< Sybil attack pattern */
    SGT_FEP_MANIP_UNKNOWN             /**< Unknown manipulation type */
} sgt_fep_manipulation_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security game theory FEP configuration
 *
 * WHAT: Configuration parameters for FEP-based adversarial detection
 * WHY:  Tune detection sensitivity and learning behavior
 * HOW:  Adjustable thresholds, learning rates, and feature flags
 */
typedef struct {
    /* FEP parameters */
    float strategy_fe_threshold;          /**< Free energy threshold for strategy deviation */
    float payoff_fe_threshold;            /**< Free energy threshold for payoff manipulation */
    float coalition_fe_threshold;         /**< Free energy threshold for coalition attacks */
    float surprise_threshold;             /**< Surprise threshold for detection */
    float precision_learning_rate;        /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_scoring;                 /**< Use FEP for manipulation scoring */
    bool enable_precision_modulation;     /**< Adapt precision based on detections */
    float normal_fe_threshold;            /**< FE threshold for normal behavior */
    float critical_fe_threshold;          /**< FE threshold for critical alert */

    /* Learning parameters */
    bool enable_online_learning;          /**< Update FEP from detections */
    float learning_rate;                  /**< Belief update rate */
    bool learn_from_false_positives;      /**< Update on FP feedback */

    /* Feature extraction */
    uint32_t strategy_feature_dim;        /**< Strategy feature dimensionality */
    uint32_t payoff_feature_dim;          /**< Payoff feature dimensionality */
    uint32_t coalition_feature_dim;       /**< Coalition feature dimensionality */

    /* Active inference */
    bool enable_active_inference;         /**< Enable active inference response */
    float action_temperature;             /**< Softmax temperature for actions */

    /* Sensitivity factors */
    float fep_sensitivity;                /**< FEP effect scaling [0.5-2.0] */
    float security_sensitivity;           /**< Security effect scaling [0.5-2.0] */
} sgt_fep_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief FEP effects on Security Game Theory (FEP --> Security)
 *
 * WHAT: How FEP analysis influences security detection
 * WHY:  Translate FEP metrics to actionable security signals
 * HOW:  Map free energy, surprise, precision to security parameters
 */
typedef struct {
    /* Scoring */
    float fep_manipulation_score;         /**< Overall manipulation score from FEP [0-1] */
    float strategy_deviation_score;       /**< Strategy deviation score [0-1] */
    float payoff_manipulation_score;      /**< Payoff manipulation score [0-1] */
    float coalition_attack_score;         /**< Coalition attack probability [0-1] */

    /* FEP metrics */
    float current_free_energy;            /**< Current free energy level */
    float current_surprise;               /**< Current surprise level */
    float detection_sensitivity;          /**< Precision-based sensitivity */

    /* Classification */
    sgt_fep_severity_t severity;          /**< Detected severity level */
    sgt_fep_manipulation_t manipulation_type; /**< Type of manipulation */
    float confidence;                     /**< Detection confidence [0-1] */

    /* Active inference */
    float recommended_action[4];          /**< Active inference recommended response */
    bool action_recommended;              /**< Whether action is recommended */
} fep_to_sgt_effects_t;

/**
 * @brief Security Game Theory effects on FEP (Security --> FEP)
 *
 * WHAT: How security detections update FEP beliefs
 * WHY:  Feedback loop for online learning and adaptation
 * HOW:  Convert security events to FEP observations
 */
typedef struct {
    /* Detection counts */
    uint64_t manipulations_detected;      /**< Total manipulations detected */
    uint64_t normal_strategies;           /**< Normal strategies observed */
    uint64_t false_positives;             /**< Known false positives */
    uint64_t coalition_attacks;           /**< Coalition attacks detected */

    /* Aggregated scores */
    float avg_strategy_deviation;         /**< Average strategy deviation */
    float avg_payoff_error;               /**< Average payoff prediction error */
    float avg_coalition_surprise;         /**< Average coalition-related surprise */

    /* Validation results */
    uint32_t payoffs_validated;           /**< Payoff matrices validated */
    uint32_t payoffs_rejected;            /**< Payoff matrices rejected */
    uint32_t equilibria_verified;         /**< Equilibria verified */
    uint32_t equilibria_rejected;         /**< Equilibria rejected */
} sgt_to_fep_effects_t;

/* ============================================================================
 * Detection Result Structure
 * ============================================================================ */

/**
 * @brief FEP-enhanced detection result
 *
 * WHAT: Comprehensive result from FEP-based adversarial analysis
 * WHY:  Provide detailed information for security decisions
 * HOW:  Combine FEP metrics with security validation results
 */
typedef struct {
    /* Detection outcome */
    bool manipulation_detected;           /**< Whether manipulation was detected */
    sgt_fep_severity_t severity;          /**< Severity classification */
    sgt_fep_manipulation_t type;          /**< Type of manipulation */
    float confidence;                     /**< Detection confidence [0-1] */

    /* FEP analysis */
    float free_energy;                    /**< Computed free energy */
    float surprise;                       /**< Computed surprise */
    float prediction_error;               /**< Magnitude of prediction error */
    float complexity;                     /**< KL divergence (complexity term) */
    float inaccuracy;                     /**< Inaccuracy term */

    /* Component scores */
    float strategy_score;                 /**< Strategy deviation component */
    float payoff_score;                   /**< Payoff manipulation component */
    float coalition_score;                /**< Coalition attack component */
    float timing_score;                   /**< Timing anomaly component */

    /* Player information */
    uint32_t affected_player;             /**< Primary affected player ID */
    uint32_t num_suspicious_players;      /**< Count of suspicious players */

    /* Explanation */
    char explanation[256];                /**< Human-readable explanation */

    /* Timing */
    uint64_t detection_time_ns;           /**< Time to perform detection */
} sgt_fep_detection_result_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Current FEP bridge state
 */
typedef struct {
    bool active;                          /**< Whether bridge is active */
    uint64_t update_count;                /**< Number of updates */
    uint64_t detection_count;             /**< Detections processed */
    float current_precision;              /**< Current precision level */
    float avg_surprise;                   /**< Running average surprise */
    float avg_free_energy;                /**< Running average free energy */
    sgt_fep_severity_t last_severity;     /**< Last detected severity */
    uint64_t last_update_time;            /**< Timestamp of last update */
} sgt_fep_state_t;

/**
 * @brief FEP bridge statistics
 */
typedef struct {
    /* Detection statistics */
    uint64_t total_detections;            /**< Total detections run */
    uint64_t fep_based_detections;        /**< Detections using FEP */
    uint64_t manipulations_found;         /**< Manipulations detected */
    uint64_t false_positives;             /**< Known false positives */

    /* Severity distribution */
    uint64_t severity_none;               /**< Count of no severity */
    uint64_t severity_low;                /**< Count of low severity */
    uint64_t severity_medium;             /**< Count of medium severity */
    uint64_t severity_high;               /**< Count of high severity */
    uint64_t severity_critical;           /**< Count of critical severity */

    /* FEP metrics */
    float avg_free_energy;                /**< Average free energy */
    float max_free_energy;                /**< Maximum free energy observed */
    float avg_surprise;                   /**< Average surprise */
    float max_surprise;                   /**< Maximum surprise observed */
    float avg_prediction_error;           /**< Average prediction error */

    /* Precision tracking */
    uint64_t precision_adaptations;       /**< Precision updates */
    float min_precision;                  /**< Minimum precision reached */
    float max_precision;                  /**< Maximum precision reached */
    float current_precision;              /**< Current precision */

    /* Performance */
    float avg_detection_time_ns;          /**< Average detection time */
    uint64_t bridge_updates;              /**< Total bridge updates */
} sgt_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Game Theory FEP bridge
 *
 * WHAT: Main bridge structure connecting game theory security to FEP
 * WHY:  Enable FEP-based adversarial strategy detection
 * HOW:  Maintain FEP system, security bridge refs, bidirectional effects
 */
typedef struct {
    bridge_base_t base;                   /**< MUST be first: base bridge */

    /* Configuration */
    sgt_fep_config_t config;              /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;             /**< FEP system */
    security_game_theory_bridge_t* sgt_bridge; /**< Security game theory bridge */

    /* Bidirectional effects */
    fep_to_sgt_effects_t fep_effects;     /**< FEP --> Security effects */
    sgt_to_fep_effects_t sgt_effects;     /**< Security --> FEP effects */

    /* State */
    sgt_fep_state_t state;                /**< Current state */

    /* Statistics */
    sgt_fep_stats_t stats;                /**< Statistics */

    /* Feature buffers */
    float* strategy_features;             /**< Strategy feature buffer */
    float* payoff_features;               /**< Payoff feature buffer */
    float* coalition_features;            /**< Coalition feature buffer */
} sgt_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for game theory-FEP integration
 * WHY:  Simplify initialization with biologically-plausible defaults
 * HOW:  Populate config struct with tested default values
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sgt_fep_default_config(sgt_fep_config_t* config);

/**
 * @brief Create security game theory FEP bridge
 *
 * WHAT: Initialize FEP integration for game-theoretic security
 * WHY:  Enable surprise-based adversarial detection
 * HOW:  Connect FEP system to security bridge, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param sgt_bridge Security game theory bridge
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
sgt_fep_bridge_t* sgt_fep_create(
    const sgt_fep_config_t* config,
    security_game_theory_bridge_t* sgt_bridge,
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
void sgt_fep_destroy(sgt_fep_bridge_t* bridge);

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
int sgt_fep_reset(sgt_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sgt_fep_get_config(
    const sgt_fep_bridge_t* bridge,
    sgt_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime parameter tuning
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sgt_fep_set_config(
    sgt_fep_bridge_t* bridge,
    const sgt_fep_config_t* config
);

/* ============================================================================
 * Core Update API
 * ============================================================================ */

/**
 * @brief Update FEP effects on security game theory
 *
 * WHAT: Compute FEP-derived manipulation scores
 * WHY:  Use free energy for adversarial detection
 * HOW:  Process current FEP state, update effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sgt_fep_update(sgt_fep_bridge_t* bridge);

/**
 * @brief Compute FEP effects from current state
 *
 * WHAT: Calculate all FEP-based security metrics
 * WHY:  Provide comprehensive FEP analysis
 * HOW:  Compute free energy, surprise, precision effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sgt_fep_compute_effects(
    sgt_fep_bridge_t* bridge,
    fep_to_sgt_effects_t* effects
);

/**
 * @brief Update FEP from security detection
 *
 * WHAT: Feed security detection result back to FEP
 * WHY:  Enable online learning from detections
 * HOW:  Convert detection to FEP observation, update beliefs
 *
 * @param bridge Bridge handle
 * @param is_manipulation Whether manipulation was detected
 * @param type Type of manipulation (if detected)
 * @param confidence Detection confidence
 * @return 0 on success, -1 on error
 */
int sgt_fep_update_from_detection(
    sgt_fep_bridge_t* bridge,
    bool is_manipulation,
    sgt_fep_manipulation_t type,
    float confidence
);

/* ============================================================================
 * Detection API
 * ============================================================================ */

/**
 * @brief Analyze strategy for adversarial behavior
 *
 * WHAT: Detect adversarial strategy using FEP
 * WHY:  Identify manipulative strategy deviations
 * HOW:  Compute free energy of strategy observation
 *
 * @param bridge Bridge handle
 * @param strategy Strategy vector
 * @param strategy_len Strategy length
 * @param player_id Player ID
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sgt_fep_analyze_strategy(
    sgt_fep_bridge_t* bridge,
    const float* strategy,
    uint32_t strategy_len,
    uint32_t player_id,
    sgt_fep_detection_result_t* result
);

/**
 * @brief Analyze payoff matrix for manipulation
 *
 * WHAT: Detect payoff manipulation using FEP
 * WHY:  Identify tampered payoff values
 * HOW:  Compute prediction error against expected payoffs
 *
 * @param bridge Bridge handle
 * @param payoffs Payoff matrix (flattened)
 * @param rows Matrix rows
 * @param cols Matrix columns
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sgt_fep_analyze_payoff(
    sgt_fep_bridge_t* bridge,
    const float* payoffs,
    uint32_t rows,
    uint32_t cols,
    sgt_fep_detection_result_t* result
);

/**
 * @brief Analyze coalition for attack patterns
 *
 * WHAT: Detect coalition attacks using FEP
 * WHY:  Identify coordinated adversarial coalitions
 * HOW:  Compute surprise from coalition formation patterns
 *
 * @param bridge Bridge handle
 * @param coalition Coalition bitmask
 * @param player_ids Player IDs in coalition
 * @param num_players Number of players
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sgt_fep_analyze_coalition(
    sgt_fep_bridge_t* bridge,
    uint32_t coalition,
    const uint32_t* player_ids,
    uint32_t num_players,
    sgt_fep_detection_result_t* result
);

/**
 * @brief Comprehensive adversarial analysis
 *
 * WHAT: Perform full FEP-based adversarial analysis
 * WHY:  Combine strategy, payoff, and coalition analysis
 * HOW:  Fuse multiple FEP analyses for overall detection
 *
 * @param bridge Bridge handle
 * @param strategy Strategy observation (may be NULL)
 * @param strategy_len Strategy length
 * @param payoffs Payoff observation (may be NULL)
 * @param payoff_rows Payoff rows
 * @param payoff_cols Payoff cols
 * @param coalition Coalition bitmask (0 if none)
 * @param result Output comprehensive result
 * @return 0 on success, -1 on error
 */
int sgt_fep_full_analysis(
    sgt_fep_bridge_t* bridge,
    const float* strategy,
    uint32_t strategy_len,
    const float* payoffs,
    uint32_t payoff_rows,
    uint32_t payoff_cols,
    uint32_t coalition,
    sgt_fep_detection_result_t* result
);

/* ============================================================================
 * Precision Modulation API
 * ============================================================================ */

/**
 * @brief Apply FEP precision modulation
 *
 * WHAT: Adjust detection precision based on FEP state
 * WHY:  Adapt sensitivity to current threat levels
 * HOW:  Modulate precision based on detection performance
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sgt_fep_apply_precision_modulation(sgt_fep_bridge_t* bridge);

/**
 * @brief Report false positive for precision adjustment
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for this observation pattern
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sgt_fep_report_false_positive(sgt_fep_bridge_t* bridge);

/**
 * @brief Set precision directly
 *
 * WHAT: Override current precision value
 * WHY:  Allow external precision control
 * HOW:  Set and clamp to valid range
 *
 * @param bridge Bridge handle
 * @param precision New precision value
 * @return 0 on success, -1 on error
 */
int sgt_fep_set_precision(sgt_fep_bridge_t* bridge, float precision);

/**
 * @brief Get current precision
 *
 * @param bridge Bridge handle
 * @return Current precision or -1.0 on error
 */
float sgt_fep_get_precision(const sgt_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sgt_fep_get_fep_effects(
    const sgt_fep_bridge_t* bridge,
    fep_to_sgt_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sgt_fep_get_sgt_effects(
    const sgt_fep_bridge_t* bridge,
    sgt_to_fep_effects_t* effects
);

/**
 * @brief Get current state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int sgt_fep_get_state(
    const sgt_fep_bridge_t* bridge,
    sgt_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sgt_fep_get_stats(
    const sgt_fep_bridge_t* bridge,
    sgt_fep_stats_t* stats
);

/**
 * @brief Get current manipulation score
 *
 * @param bridge Bridge handle
 * @return Current manipulation score [0,1] or -1 on error
 */
float sgt_fep_get_manipulation_score(const sgt_fep_bridge_t* bridge);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1 on error
 */
float sgt_fep_get_free_energy(const sgt_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1 on error
 */
float sgt_fep_get_surprise(const sgt_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sgt_fep_connect_bio_async(sgt_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sgt_fep_disconnect_bio_async(sgt_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sgt_fep_is_bio_async_connected(const sgt_fep_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Print bridge summary to stdout
 *
 * WHAT: Output human-readable bridge state summary
 * WHY:  Debugging and monitoring
 * HOW:  Print config, state, effects, and stats
 *
 * @param bridge Bridge handle
 */
void sgt_fep_print_summary(const sgt_fep_bridge_t* bridge);

/**
 * @brief Convert severity to string
 *
 * @param severity Severity level
 * @return Human-readable string
 */
const char* sgt_fep_severity_to_string(sgt_fep_severity_t severity);

/**
 * @brief Convert manipulation type to string
 *
 * @param type Manipulation type
 * @return Human-readable string
 */
const char* sgt_fep_manipulation_to_string(sgt_fep_manipulation_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_GAME_THEORY_FEP_BRIDGE_H */
