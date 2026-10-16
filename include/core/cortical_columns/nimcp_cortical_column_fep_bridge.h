/**
 * @file nimcp_cortical_column_fep_bridge.h
 * @brief Free Energy Principle Bridge for Cortical Columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Integrates Free Energy Principle with cortical column architecture
 * WHY:  Cortical columns implement hierarchical predictive processing - the core
 *       of FEP. Minicolumns act as predictive units, hypercolumns implement
 *       hierarchical inference, and lateral inhibition performs prediction error minimization.
 * HOW:  Bridge FEP beliefs to column activations, prediction errors to lateral
 *       inhibition, and hierarchical processing to hypercolumn organization.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CORTICAL COLUMNS AS HIERARCHICAL PREDICTIVE UNITS:
 * --------------------------------------------------
 * 1. Minicolumns = Elementary Generative Models:
 *    - Each minicolumn represents a hypothesis about sensory input
 *    - Winner-take-all = selecting highest probability hypothesis
 *    - Receptive fields = likelihood function p(o|s)
 *    - Reference: Mountcastle (1997) "The columnar organization of the neocortex"
 *
 * 2. Hypercolumns = Feature Space Coverage:
 *    - Minicolumns tile feature space (e.g., orientation columns in V1)
 *    - Competition implements Bayesian model selection
 *    - Population code = posterior distribution q(s)
 *    - Reference: Hubel & Wiesel (1962) "Receptive fields of cells in striate cortex"
 *
 * 3. Lateral Inhibition = Precision Weighting:
 *    - Mexican hat profile implements precision-weighted prediction errors
 *    - Sharpening = increasing precision (reducing uncertainty)
 *    - Contrast enhancement = emphasizing reliable signals
 *    - Reference: Rao & Ballard (1999) "Predictive coding in the visual cortex"
 *
 * 4. Hierarchical Organization:
 *    - Lower columns: detailed sensory representations (high precision)
 *    - Higher columns: abstract invariant features (low precision, high level)
 *    - Feed-forward: prediction errors
 *    - Feed-back: predictions
 *    - Reference: Friston (2005) "A theory of cortical responses"
 *
 * FEP-COLUMN MAPPING:
 * -------------------
 * - FEP beliefs μ(s) ↔ Minicolumn activation distribution
 * - FEP precision Π ↔ Lateral inhibition strength
 * - Prediction error ε ↔ Mismatch between input and winning column
 * - Free energy F ↔ Entropy of hypercolumn distribution
 * - Model selection ↔ Winner-take-all competition
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_COLUMN_FEP_BRIDGE_H
#define NIMCP_CORTICAL_COLUMN_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for cortical column FEP bridge
 */
typedef struct {
    /* Belief mapping */
    float belief_to_activation_gain;    /**< Scaling from FEP belief to column activation */
    float activation_to_belief_gain;    /**< Scaling from activation to belief update */

    /* Precision control */
    float precision_to_inhibition_gain; /**< Scaling from FEP precision to lateral inhibition */
    bool enable_precision_learning;     /**< Learn optimal precision from prediction errors */

    /* Prediction error */
    float prediction_error_threshold;   /**< Threshold for significant prediction error */
    bool enable_error_backprop;         /**< Backpropagate errors to lower levels */

    /* Hierarchical parameters */
    uint32_t hierarchy_level;           /**< Level in cortical hierarchy (0=lowest) */
    float level_precision_scaling;      /**< Precision scaling by hierarchy level */

    /* Learning rates */
    float belief_learning_rate;         /**< Rate for belief updates from observations */
    float precision_learning_rate;      /**< Rate for precision adaptation */

    /* Thresholds */
    float surprise_threshold;           /**< Threshold for surprising observations */
    float convergence_threshold;        /**< Convergence criterion for belief updates */
} cortical_column_fep_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief FEP effects on cortical columns
 */
typedef struct {
    float activation_modulation;        /**< FEP-driven activation scaling [0-2] */
    float precision_weight;             /**< Current precision estimate [0-inf] */
    float prediction_error_magnitude;   /**< Current prediction error magnitude */
    float free_energy;                  /**< Local free energy estimate */
    float surprise;                     /**< Current surprise level */
    bool converged;                     /**< Whether beliefs have converged */
} cortical_column_fep_effects_t;

/**
 * @brief Column effects on FEP system
 */
typedef struct {
    float observation_likelihood;       /**< p(o|s) from column response */
    float posterior_belief;             /**< q(s) from column population code */
    float prediction;                   /**< Top-down prediction from beliefs */
    float confidence;                   /**< Confidence in current hypothesis */
} fep_cortical_column_effects_t;

/**
 * @brief Per-minicolumn FEP state
 */
typedef struct {
    float belief_mean;                  /**< Expected state μ */
    float belief_precision;             /**< Precision Π */
    float prediction_error;             /**< Local prediction error */
    float expected_input;               /**< Predicted input based on belief */
    uint64_t last_update;               /**< Last update timestamp */
} minicolumn_fep_state_t;

/**
 * @brief Cortical column FEP state
 */
typedef struct {
    /* Per-minicolumn states */
    minicolumn_fep_state_t* minicolumn_states;
    uint32_t num_minicolumns;

    /* Hypercolumn-level beliefs */
    float* belief_distribution;         /**< Posterior over minicolumns */
    float* prior_distribution;          /**< Prior preferences */
    float* prediction_errors;           /**< Prediction error per minicolumn */

    /* Free energy components */
    float complexity;                   /**< KL divergence term */
    float inaccuracy;                   /**< Negative log-likelihood */
    float free_energy_total;            /**< Total free energy F */

    /* Precision (inverse uncertainty) */
    float sensory_precision;            /**< Reliability of sensory input */
    float prior_precision;              /**< Strength of prior beliefs */

    /* Convergence tracking */
    uint32_t iteration_count;
    float belief_change;                /**< Magnitude of last belief update */
} cortical_column_fep_state_t;

/**
 * @brief Statistics for cortical column FEP bridge
 */
typedef struct {
    uint64_t total_updates;
    uint64_t belief_updates;
    uint64_t surprise_events;
    float avg_prediction_error;
    float avg_free_energy;
    float min_free_energy;
    float max_surprise;
    uint32_t convergence_failures;
    float avg_precision;
} cortical_column_fep_stats_t;

/**
 * @brief Complete cortical column FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    cortical_column_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;
    hypercolumn_t* hypercolumn;

    /* Bridge state */
    cortical_column_fep_state_t state;
    cortical_column_fep_effects_t fep_effects;
    fep_cortical_column_effects_t column_effects;

    /* Statistics */
    cortical_column_fep_stats_t stats;

    } cortical_column_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default cortical column FEP configuration
 *
 * WHAT: Provide sensible default parameters
 * WHY:  Easy initialization with biologically-plausible values
 * HOW:  Set defaults based on cortical physiology
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int cortical_column_fep_default_config(cortical_column_fep_config_t* config);

/**
 * @brief Create cortical column FEP bridge
 *
 * WHAT: Initialize FEP bridge for cortical columns
 * WHY:  Enable hierarchical predictive processing in columnar architecture
 * HOW:  Allocate bridge, connect systems, initialize beliefs
 *
 * @param config Configuration (NULL for defaults)
 * @param hypercolumn Hypercolumn to bridge
 * @param fep_system FEP system
 * @return New bridge or NULL on failure
 */
cortical_column_fep_bridge_t* cortical_column_fep_create(
    const cortical_column_fep_config_t* config,
    hypercolumn_t* hypercolumn,
    fep_system_t* fep_system
);

/**
 * @brief Destroy cortical column FEP bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void cortical_column_fep_destroy(cortical_column_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP bridge state
 *
 * WHAT: Synchronize FEP beliefs with column activations
 * WHY:  Maintain consistency between FEP and column representations
 * HOW:  Bidirectional mapping between beliefs and activations
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int cortical_column_fep_update(cortical_column_fep_bridge_t* bridge);

/**
 * @brief Process observation with FEP-guided column computation
 *
 * WHAT: Update column activations using FEP inference
 * WHY:  Minimize free energy through columnar competition
 * HOW:  Compute prediction errors, update beliefs, run competition
 *
 * @param bridge FEP bridge
 * @param observation Input observation vector
 * @param observation_dim Observation dimensionality
 * @return 0 on success
 */
int cortical_column_fep_process_observation(
    cortical_column_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim
);

/**
 * @brief Compute prediction from current beliefs
 *
 * WHAT: Generate top-down prediction from column beliefs
 * WHY:  Provide predictions for lower levels in hierarchy
 * HOW:  Weighted sum of minicolumn predictions
 *
 * @param bridge FEP bridge
 * @param prediction Output prediction vector
 * @param prediction_dim Max prediction dimension
 * @return Actual prediction dimension
 */
uint32_t cortical_column_fep_compute_prediction(
    const cortical_column_fep_bridge_t* bridge,
    float* prediction,
    uint32_t prediction_dim
);

/**
 * @brief Compute prediction error
 *
 * WHAT: Calculate mismatch between observation and prediction
 * WHY:  Prediction errors drive learning and inference
 * HOW:  Compare observation with winning column prediction
 *
 * @param bridge FEP bridge
 * @param observation Observed input
 * @param observation_dim Input dimensionality
 * @param error Output error magnitude
 * @return 0 on success
 */
int cortical_column_fep_compute_error(
    const cortical_column_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim,
    float* error
);

/* ============================================================================
 * Belief and Precision API
 * ============================================================================ */

/**
 * @brief Update beliefs from sensory evidence
 *
 * WHAT: Perform variational inference step
 * WHY:  Minimize free energy by updating beliefs
 * HOW:  Gradient descent on variational free energy
 *
 * @param bridge FEP bridge
 * @param observation Sensory input
 * @param observation_dim Input dimensionality
 * @return 0 on success
 */
int cortical_column_fep_update_beliefs(
    cortical_column_fep_bridge_t* bridge,
    const float* observation,
    uint32_t observation_dim
);

/**
 * @brief Update precision estimates
 *
 * WHAT: Adapt precision based on prediction error statistics
 * WHY:  Attention as precision optimization
 * HOW:  Increase precision for reliable signals, decrease for noisy ones
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int cortical_column_fep_update_precision(cortical_column_fep_bridge_t* bridge);

/**
 * @brief Set sensory precision
 *
 * @param bridge FEP bridge
 * @param precision New sensory precision [0-inf]
 * @return 0 on success
 */
int cortical_column_fep_set_precision(
    cortical_column_fep_bridge_t* bridge,
    float precision
);

/* ============================================================================
 * Competition and Selection API
 * ============================================================================ */

/**
 * @brief Run FEP-guided competition
 *
 * WHAT: Perform model selection via free energy minimization
 * WHY:  Select most probable hypothesis (minicolumn)
 * HOW:  Softmax over negative free energy
 *
 * @param bridge FEP bridge
 * @return Index of winning minicolumn
 */
uint32_t cortical_column_fep_select_hypothesis(
    cortical_column_fep_bridge_t* bridge
);

/**
 * @brief Apply precision-weighted lateral inhibition
 *
 * WHAT: Modulate lateral inhibition by FEP precision
 * WHY:  High precision → stronger competition (sharper tuning)
 * HOW:  Scale inhibition strength by precision estimate
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int cortical_column_fep_apply_lateral_inhibition(
    cortical_column_fep_bridge_t* bridge
);

/* ============================================================================
 * Free Energy and Surprise API
 * ============================================================================ */

/**
 * @brief Compute variational free energy
 *
 * WHAT: Calculate F = Complexity + Inaccuracy
 * WHY:  Objective function for inference
 * HOW:  F = KL[q||p] + E_q[-ln p(o|s)]
 *
 * @param bridge FEP bridge
 * @param free_energy Output free energy
 * @return 0 on success
 */
int cortical_column_fep_compute_free_energy(
    const cortical_column_fep_bridge_t* bridge,
    float* free_energy
);

/**
 * @brief Compute surprise
 *
 * WHAT: Estimate negative log evidence -ln p(o)
 * WHY:  Detect unexpected observations
 * HOW:  Free energy provides upper bound on surprise
 *
 * @param bridge FEP bridge
 * @return Surprise estimate
 */
float cortical_column_fep_compute_surprise(
    const cortical_column_fep_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current beliefs
 *
 * @param bridge FEP bridge
 * @param beliefs Output belief distribution (size = num_minicolumns)
 * @param size Buffer size
 * @return Number of beliefs returned
 */
uint32_t cortical_column_fep_get_beliefs(
    const cortical_column_fep_bridge_t* bridge,
    float* beliefs,
    uint32_t size
);

/**
 * @brief Get FEP effects on columns
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int cortical_column_fep_get_effects(
    const cortical_column_fep_bridge_t* bridge,
    cortical_column_fep_effects_t* effects
);

/**
 * @brief Get column effects on FEP
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int cortical_column_fep_get_column_effects(
    const cortical_column_fep_bridge_t* bridge,
    fep_cortical_column_effects_t* effects
);

/**
 * @brief Get statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int cortical_column_fep_get_stats(
    const cortical_column_fep_bridge_t* bridge,
    cortical_column_fep_stats_t* stats
);

/* ============================================================================
 * Bio-async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int cortical_column_fep_connect_bio_async(cortical_column_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int cortical_column_fep_disconnect_bio_async(cortical_column_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool cortical_column_fep_is_bio_async_connected(
    const cortical_column_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_COLUMN_FEP_BRIDGE_H */
