/**
 * @file nimcp_cortical_predictive_coding.h
 * @brief Hierarchical Predictive Coding for Cortical Columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Implements hierarchical predictive coding with separate prediction and error populations
 * WHY:  Cortical columns perform predictive processing - deep layers generate predictions,
 *       superficial layers compute precision-weighted errors. This is the computational
 *       substrate of Free Energy Principle in cortex.
 * HOW:  Separate prediction units (L5/6) and error units (L2/3) per hierarchy level,
 *       with precision-weighted message passing between levels.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LAMINAR ORGANIZATION OF PREDICTIVE CODING:
 * ------------------------------------------
 * 1. Deep Layers (L5/6) = Prediction Units:
 *    - Generate top-down predictions μ(s)
 *    - Send predictions to lower levels via feedback connections
 *    - Updated by minimizing prediction error
 *    - Reference: Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * 2. Superficial Layers (L2/3) = Error Units:
 *    - Compute prediction error ε = observation - prediction
 *    - Send errors to higher levels via feedforward connections
 *    - Weight errors by precision (inverse variance)
 *    - Reference: Friston (2005) "A theory of cortical responses"
 *
 * 3. Precision Weighting = Attention:
 *    - High precision (Π) → stronger error signals → attentional enhancement
 *    - Low precision → weaker error signals → ignored inputs
 *    - Precision learned from error statistics
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * 4. Hierarchical Message Passing:
 *    - Feedforward: Precision-weighted errors ascend hierarchy
 *    - Feedback: Predictions descend hierarchy
 *    - Lateral: Within-level predictions for spatial coherence
 *    - Reference: Rao & Ballard (1999) "Predictive coding in the visual cortex"
 *
 * PREDICTIVE CODING MATHEMATICS:
 * -------------------------------
 * Prediction Error:
 *   ε_i = x_i - g(μ_{i+1})
 *   where x_i = input at level i, μ_{i+1} = beliefs at level i+1, g = generative model
 *
 * Precision-Weighted Error:
 *   ε̃_i = Π_i * ε_i
 *   where Π_i = precision (inverse variance) at level i
 *
 * Belief Update (Prediction Learning):
 *   Δμ_i = η * ∂F/∂μ_i = η * (ε_{i-1} - g'(μ_i)^T Π_i ε_i)
 *   where η = learning rate, F = free energy
 *
 * Precision Update:
 *   Δln(Π_i) = α * (ε_i^2 - 1/Π_i)
 *   where α = precision learning rate
 *
 * Free Energy:
 *   F = Σ_i [½ε_i^T Π_i ε_i + ½ln|Π_i^{-1}|]
 *   (sum of precision-weighted squared errors + uncertainty term)
 *
 * CORTICAL HIERARCHY:
 * -------------------
 *   Level 3 (Abstract) ────┐
 *      L5/6: μ₃ (predictions)  │
 *      L2/3: ε₃ (errors)       │ High-level
 *                              │ features
 *   Level 2 (Intermediate) ───┤
 *      L5/6: μ₂ (predictions)  │
 *      L2/3: ε₂ (errors)       │ Mid-level
 *                              │ features
 *   Level 1 (Sensory) ────────┘
 *      L5/6: μ₁ (predictions)
 *      L2/3: ε₁ (errors)       Low-level
 *                              features
 *   Level 0 (Input)
 *      Sensory data
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_PREDICTIVE_CODING_H
#define NIMCP_CORTICAL_PREDICTIVE_CODING_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Population type in predictive coding hierarchy
 *
 * WHAT: Distinguishes prediction units (deep layers) from error units (superficial)
 * WHY:  Different populations have different update rules and connectivity
 */
typedef enum {
    PC_POPULATION_PREDICTION,    /**< Deep layers (L5/6) - top-down predictions */
    PC_POPULATION_ERROR          /**< Superficial layers (L2/3) - bottom-up errors */
} pc_population_type_t;

/**
 * @brief Predictive layer containing both prediction and error populations
 *
 * WHAT: Single computational layer in predictive hierarchy
 * WHY:  Each layer computes predictions and errors for its level
 * HOW:  Separate arrays for predictions, errors, and precision weights
 */
typedef struct {
    float* predictions;          /**< Top-down predictions from this layer */
    float* errors;               /**< Prediction errors at this layer */
    float* precisions;           /**< Precision weights (inverse variance) */
    uint32_t num_units;          /**< Number of units in this layer */
    pc_population_type_t type;   /**< Layer type (not really used, layers have both) */
} predictive_layer_t;

/**
 * @brief Configuration for predictive coding hierarchy
 *
 * WHAT: Parameters controlling learning and precision
 * WHY:  Allow tuning of prediction vs error learning rates
 * HOW:  Separate learning rates for predictions and precisions
 */
typedef struct {
    float prediction_learning_rate;     /**< Learning rate for prediction updates */
    float precision_learning_rate;      /**< Learning rate for precision updates */
    float error_gain;                    /**< Amplification factor for error signals */
    float prediction_decay;              /**< Decay factor for predictions (regularization) */
    bool enable_precision_weighting;     /**< Enable precision-weighted errors */
    bool enable_lateral_predictions;     /**< Enable within-level predictions */
    uint32_t hierarchy_depth;            /**< Number of levels in hierarchy */
} predictive_config_t;

/**
 * @brief Single level in predictive hierarchy
 *
 * WHAT: Complete representation of one hierarchy level
 * WHY:  Each level has prediction/error populations plus metadata
 * HOW:  Combines prediction layer, error layer, lateral connections, precision
 */
typedef struct {
    predictive_layer_t prediction_pop;  /**< Prediction population (L5/6) */
    predictive_layer_t error_pop;       /**< Error population (L2/3) */
    float* lateral_predictions;         /**< Within-level lateral predictions */
    float level_precision;              /**< Overall precision for this level */
    uint32_t level_index;               /**< Index in hierarchy (0=lowest) */
} predictive_level_t;

/**
 * @brief Complete predictive coding hierarchy
 *
 * WHAT: Multi-level predictive processing system
 * WHY:  Hierarchical abstraction through predictive coding
 * HOW:  Stack of levels with inter-level connections and free energy tracking
 */
typedef struct {
    predictive_level_t* levels;         /**< Array of hierarchy levels */
    uint32_t num_levels;                /**< Number of levels in hierarchy */
    float total_free_energy;            /**< Summed free energy across levels */
    float total_prediction_error;       /**< Total prediction error magnitude */
    float* inter_level_weights;         /**< Feedforward and feedback weights */
    uint32_t weights_size;              /**< Size of weights array */
} predictive_hierarchy_t;

/**
 * @brief Message passed between levels
 *
 * WHAT: Encapsulates prediction or error signal
 * WHY:  Standardized format for inter-level communication
 * HOW:  Contains data, metadata, and routing information
 */
typedef struct {
    float* content;                     /**< Message data (prediction or error) */
    uint32_t size;                      /**< Number of elements in content */
    bool is_prediction;                 /**< true=top-down, false=bottom-up error */
    float precision;                    /**< Associated precision weight */
    uint32_t source_level;              /**< Originating level index */
    uint32_t target_level;              /**< Destination level index */
} predictive_message_t;

/**
 * @brief Statistics for predictive coding system
 *
 * WHAT: Performance and state metrics
 * WHY:  Monitor convergence and error dynamics
 */
typedef struct {
    uint64_t total_updates;             /**< Total update iterations */
    uint64_t prediction_updates;        /**< Prediction learning steps */
    uint64_t precision_updates;         /**< Precision learning steps */
    float avg_free_energy;              /**< Average free energy */
    float avg_prediction_error;         /**< Average prediction error */
    float avg_precision;                /**< Average precision across levels */
    float min_free_energy;              /**< Minimum free energy achieved */
    float convergence_rate;             /**< Rate of free energy decrease */
} predictive_stats_t;

/**
 * @brief Complete predictive coding system
 *
 * WHAT: Main structure integrating hierarchy, config, and bio-async
 * WHY:  Single point of access for predictive coding operations
 * HOW:  Combines hierarchy, configuration, statistics, and threading
 */
typedef struct {
    predictive_config_t config;         /**< Configuration parameters */
    predictive_hierarchy_t hierarchy;   /**< Hierarchical structure */
    predictive_stats_t stats;           /**< Performance statistics */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;       /**< Bio-async module context */
    bool bio_async_enabled;             /**< Whether bio-async is active */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Mutex for thread safety */
} cortical_predictive_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default predictive coding configuration
 *
 * WHAT: Initialize configuration with biologically-plausible defaults
 * WHY:  Provide sensible starting parameters
 * HOW:  Set learning rates, gains based on cortical physiology
 *
 * @param config Output configuration structure
 * @return 0 on success, negative on error
 */
int cortical_predictive_default_config(predictive_config_t* config);

/**
 * @brief Create predictive coding system
 *
 * WHAT: Allocate and initialize predictive hierarchy
 * WHY:  Set up multi-level predictive processing
 * HOW:  Allocate levels, initialize weights and precisions
 *
 * @param config Configuration (NULL for defaults)
 * @return New predictive coding system or NULL on failure
 */
cortical_predictive_t* cortical_predictive_create(const predictive_config_t* config);

/**
 * @brief Destroy predictive coding system
 *
 * WHAT: Free all allocated memory
 * WHY:  Clean shutdown and leak prevention
 * HOW:  Free levels, weights, and main structure
 *
 * @param pc Predictive system to destroy (NULL safe)
 */
void cortical_predictive_destroy(cortical_predictive_t* pc);

/* ============================================================================
 * Hierarchy Construction API
 * ============================================================================ */

/**
 * @brief Add level to predictive hierarchy
 *
 * WHAT: Extend hierarchy by one level
 * WHY:  Build hierarchy incrementally with custom sizes
 * HOW:  Allocate new level, initialize populations, connect to existing
 *
 * @param pc Predictive coding system
 * @param num_prediction_units Size of prediction population
 * @param num_error_units Size of error population
 * @return 0 on success, negative on error
 */
int cortical_predictive_add_level(
    cortical_predictive_t* pc,
    uint32_t num_prediction_units,
    uint32_t num_error_units
);

/* ============================================================================
 * Prediction and Error Computation API
 * ============================================================================ */

/**
 * @brief Compute top-down predictions at a level
 *
 * WHAT: Generate predictions from current beliefs
 * WHY:  Predictions are compared to input to compute errors
 * HOW:  Apply generative model (weighted sum from higher level)
 *
 * @param pc Predictive coding system
 * @param level_idx Level index to compute predictions for
 * @param output Output buffer for predictions
 * @param output_size Size of output buffer
 * @return Number of predictions written, negative on error
 */
int cortical_predictive_compute_prediction(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* output,
    uint32_t output_size
);

/**
 * @brief Compute prediction errors at a level
 *
 * WHAT: Calculate mismatch between input and prediction
 * WHY:  Errors drive learning and are sent up the hierarchy
 * HOW:  Subtract prediction from observation
 *
 * @param pc Predictive coding system
 * @param level_idx Level index to compute errors for
 * @param observation Input observation
 * @param obs_size Size of observation
 * @param output Output buffer for errors
 * @param output_size Size of output buffer
 * @return Number of errors written, negative on error
 */
int cortical_predictive_compute_error(
    cortical_predictive_t* pc,
    uint32_t level_idx,
    const float* observation,
    uint32_t obs_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Apply precision weighting to errors
 *
 * WHAT: Multiply errors by precision (attention)
 * WHY:  High precision errors have more influence
 * HOW:  Element-wise multiplication by precision weights
 *
 * @param pc Predictive coding system
 * @param level_idx Level index
 * @param errors Input errors
 * @param error_size Size of error vector
 * @param weighted_errors Output weighted errors
 * @param output_size Size of output buffer
 * @return 0 on success, negative on error
 */
int cortical_predictive_weight_by_precision(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    const float* errors,
    uint32_t error_size,
    float* weighted_errors,
    uint32_t output_size
);

/* ============================================================================
 * Message Passing API
 * ============================================================================ */

/**
 * @brief Propagate errors up the hierarchy
 *
 * WHAT: Send precision-weighted errors from L2/3 to higher level
 * WHY:  Bottom-up error signals drive inference
 * HOW:  Apply inter-level weights to weighted errors
 *
 * @param pc Predictive coding system
 * @param source_level Source level index
 * @return 0 on success, negative on error
 */
int cortical_predictive_propagate_up(
    cortical_predictive_t* pc,
    uint32_t source_level
);

/**
 * @brief Propagate predictions down the hierarchy
 *
 * WHAT: Send predictions from L5/6 to lower level
 * WHY:  Top-down predictions set expectations
 * HOW:  Apply inter-level weights to prediction units
 *
 * @param pc Predictive coding system
 * @param source_level Source level index
 * @return 0 on success, negative on error
 */
int cortical_predictive_propagate_down(
    cortical_predictive_t* pc,
    uint32_t source_level
);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Update predictions via gradient descent on free energy
 *
 * WHAT: Adjust predictions to minimize prediction error
 * WHY:  Prediction learning = perceptual inference
 * HOW:  Gradient descent: Δμ = η * ∂F/∂μ
 *
 * @param pc Predictive coding system
 * @param level_idx Level to update
 * @return 0 on success, negative on error
 */
int cortical_predictive_update_predictions(
    cortical_predictive_t* pc,
    uint32_t level_idx
);

/**
 * @brief Update precision estimates from error statistics
 *
 * WHAT: Adapt precision based on error variance
 * WHY:  Precision learning = attentional optimization
 * HOW:  Increase precision for consistent errors, decrease for noisy ones
 *
 * @param pc Predictive coding system
 * @param level_idx Level to update
 * @return 0 on success, negative on error
 */
int cortical_predictive_update_precisions(
    cortical_predictive_t* pc,
    uint32_t level_idx
);

/* ============================================================================
 * Free Energy API
 * ============================================================================ */

/**
 * @brief Compute variational free energy
 *
 * WHAT: Calculate objective function for inference
 * WHY:  Free energy = upper bound on surprise
 * HOW:  F = Σ [½ε^T Π ε + ½ln|Π^{-1}|]
 *
 * @param pc Predictive coding system
 * @param free_energy Output free energy value
 * @return 0 on success, negative on error
 */
int cortical_predictive_compute_free_energy(
    const cortical_predictive_t* pc,
    float* free_energy
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor convergence and system health
 *
 * @param pc Predictive coding system
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int cortical_predictive_get_stats(
    const cortical_predictive_t* pc,
    predictive_stats_t* stats
);

/**
 * @brief Get predictions from a level
 *
 * WHAT: Retrieve current prediction values
 * WHY:  Access internal state for analysis or integration
 *
 * @param pc Predictive coding system
 * @param level_idx Level index
 * @param predictions Output buffer
 * @param size Buffer size
 * @return Number of predictions copied, negative on error
 */
int cortical_predictive_get_predictions(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* predictions,
    uint32_t size
);

/**
 * @brief Get errors from a level
 *
 * WHAT: Retrieve current error values
 * WHY:  Access error signals for analysis or routing
 *
 * @param pc Predictive coding system
 * @param level_idx Level index
 * @param errors Output buffer
 * @param size Buffer size
 * @return Number of errors copied, negative on error
 */
int cortical_predictive_get_errors(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* errors,
    uint32_t size
);

/**
 * @brief Get precisions from a level
 *
 * WHAT: Retrieve current precision values
 * WHY:  Access attention weights for analysis
 *
 * @param pc Predictive coding system
 * @param level_idx Level index
 * @param precisions Output buffer
 * @param size Buffer size
 * @return Number of precisions copied, negative on error
 */
int cortical_predictive_get_precisions(
    const cortical_predictive_t* pc,
    uint32_t level_idx,
    float* precisions,
    uint32_t size
);

/* ============================================================================
 * Bio-async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module communication
 * HOW:  Register as BIO_MODULE_CORTICAL_PREDICTIVE
 *
 * @param pc Predictive coding system
 * @return 0 on success, negative on error
 */
int cortical_predictive_connect_bio_async(cortical_predictive_t* pc);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown
 *
 * @param pc Predictive coding system
 * @return 0 on success, negative on error
 */
int cortical_predictive_disconnect_bio_async(cortical_predictive_t* pc);

/**
 * @brief Check if bio-async is connected
 *
 * @param pc Predictive coding system
 * @return true if connected, false otherwise
 */
bool cortical_predictive_is_bio_async_connected(const cortical_predictive_t* pc);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_PREDICTIVE_CODING_H */
