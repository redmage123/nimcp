/**
 * @file nimcp_brain_region_predictive.h
 * @brief Integration of Predictive Coding with Brain Regions and Hierarchical Processing
 *
 * WHAT: Extends brain regions with hierarchical predictive coding capabilities
 * WHY:  Implement Friston's Free Energy Principle across cortical hierarchy
 * HOW:  Brain regions predict lower regions' activity; errors flow up, predictions flow down
 *
 * ARCHITECTURAL OVERVIEW:
 * - Chain of Responsibility: Hierarchical error propagation between regions
 * - Strategy Pattern: Different prediction strategies per region type
 * - Observer Pattern: Bio-async notifications for prediction updates
 * - Facade Pattern: Simplified interface over complex predictive machinery
 *
 * BIOLOGICAL BASIS:
 * - Free Energy Principle (Friston 2010): Brain minimizes prediction error
 * - Predictive Processing: Higher cortical areas predict lower areas
 * - Superficial Layers (2/3): Encode prediction errors (ε)
 * - Deep Layers (5/6): Encode predictions (μ̂) and send to lower areas
 * - Precision Weighting: Attention modulates prediction error gain
 *
 * HIERARCHICAL FLOW:
 * ```
 * Higher Region (e.g., V4)
 *   ↓ Top-Down Prediction: μ̂_V2 = f(μ_V4)
 * Middle Region (e.g., V2)
 *   ↓ Prediction Error: ε_V2 = x_V2 - μ̂_V2
 *   ↑ Error Propagation to V4
 *   ↓ Top-Down Prediction: μ̂_V1 = g(μ_V2)
 * Lower Region (e.g., V1)
 *   ↓ Prediction Error: ε_V1 = sensory_input - μ̂_V1
 *   ↑ Error Propagation to V2
 * ```
 *
 * BIO-ASYNC MESSAGING:
 * - BIO_MSG_PREDICTION_ERROR: Bottom-up error signals (norepinephrine channel)
 * - BIO_MSG_PREDICTION_UPDATE: Top-down predictions (serotonin channel)
 * - BIO_MSG_PREDICTIVE_CODING_UPDATE: General state updates (dopamine channel)
 * - BIO_MSG_ATTENTION_SHIFT: Precision modulation (acetylcholine channel)
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_REGION_PREDICTIVE_H
#define NIMCP_BRAIN_REGION_PREDICTIVE_H

#include "core/brain_regions/nimcp_brain_regions.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "utils/validation/nimcp_common.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define PC_REGION_MAX_PREDICTIONS 64       /**< Max concurrent predictions */
#define PC_REGION_CONVERGENCE_ITERATIONS 20 /**< Default convergence iterations */
#define PC_REGION_CONVERGENCE_TOLERANCE 0.001f /**< Convergence threshold */

//=============================================================================
// Predictive Region Configuration
//=============================================================================

/**
 * @brief Predictive processing configuration for brain region
 *
 * WHAT: Specifies how region participates in hierarchical prediction
 * WHY:  Different regions have different roles in hierarchy
 */
typedef struct {
    bool enable_predictive_processing;  /**< Enable predictive coding */
    bool generate_predictions;          /**< Generate top-down predictions */
    bool compute_prediction_errors;     /**< Compute prediction errors */
    bool learn_precisions;              /**< Learn precision (attention) weights */
    bool broadcast_predictions;         /**< Broadcast via bio-async */
    bool broadcast_errors;              /**< Broadcast errors via bio-async */

    /* Hierarchical configuration */
    uint32_t hierarchy_level;           /**< Level in hierarchy (0=lowest) */
    uint32_t num_levels_below;          /**< Regions below this one */
    uint32_t num_levels_above;          /**< Regions above this one */

    /* Learning rates */
    float prediction_learning_rate;     /**< Learning rate for predictions */
    float precision_learning_rate;      /**< Learning rate for precisions */
    float error_correction_rate;        /**< How fast to correct errors */

    /* Convergence */
    uint32_t max_iterations;            /**< Max inference iterations */
    float convergence_tolerance;        /**< Convergence threshold */

    /* Bio-async channels */
    nimcp_bio_channel_type_t prediction_channel;  /**< Channel for predictions */
    nimcp_bio_channel_type_t error_channel;       /**< Channel for errors */

    /* Positional encoding configuration */
    bool enable_hierarchy_pe;           /**< Enable hierarchy level positional encoding */
    bool enable_temporal_pe;            /**< Enable temporal sequence positional encoding */
    uint32_t pe_embedding_dim;          /**< Dimension for positional embeddings */
    uint32_t max_prediction_sequence;   /**< Max length of prediction sequences */
} brain_region_predictive_config_t;

//=============================================================================
// Predictive Region Extension (Extends brain_region_t)
//=============================================================================

/**
 * @brief Predictive coding state for brain region
 *
 * WHAT: Extension to brain_region_t for predictive processing
 * WHY:  Add hierarchical prediction capabilities without breaking existing code
 * HOW:  Attached to brain_region_t->predictive_extension field
 */
typedef struct {
    /* Configuration */
    brain_region_predictive_config_t config;

    /* Predictive hierarchy */
    pc_hierarchy_t hierarchy;           /**< Predictive coding hierarchy (NULL if disabled) */

    /* Prediction state */
    float* current_prediction;          /**< Current top-down prediction [num_neurons] */
    float* prediction_error;            /**< Current prediction error [num_neurons] */
    float* precision_weights;           /**< Precision (attention) weights [num_neurons] */

    /* Hierarchical connections */
    uint32_t* input_region_ids;         /**< Regions providing bottom-up input */
    uint32_t num_input_regions;
    uint32_t* output_region_ids;        /**< Regions receiving predictions */
    uint32_t num_output_regions;

    /* Statistics */
    uint64_t total_predictions;         /**< Total predictions generated */
    uint64_t total_errors_computed;     /**< Total errors computed */
    float mean_prediction_error;        /**< Mean absolute prediction error */
    float mean_precision;               /**< Mean precision weight */
    float total_free_energy;            /**< Current free energy */

    /* Bio-async */
    bool bio_async_registered;          /**< Registered with bio-async */
    uint32_t bio_async_module_id;       /**< Bio-async module ID */

    /* Positional encoding state */
    nimcp_pos_encoder_t* hierarchy_pe_encoder;  /**< Learned PE for hierarchy levels */
    nimcp_pos_encoder_t* temporal_pe_encoder;   /**< Sinusoidal PE for temporal sequences */
    float* hierarchy_level_embedding;           /**< Current hierarchy level embedding */
    float* temporal_sequence_buffer;            /**< Buffer for temporal PE sequences */

    /* Thread safety */
    nimcp_mutex_t lock;
} brain_region_predictive_t;

//=============================================================================
// Initialization and Destruction
//=============================================================================

/**
 * @brief Enable predictive processing for brain region
 *
 * WHAT: Add predictive coding capabilities to existing region
 * WHY:  Integrate region into hierarchical predictive network
 * HOW:  Create predictive hierarchy, register with bio-async
 *
 * @param region Brain region to extend
 * @param config Predictive processing configuration
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * COMPLEXITY: O(N) where N = region neuron count
 * ALLOCATES: Predictive hierarchy, prediction buffers
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_enable_predictive(brain_region_t* region,
                                                const brain_region_predictive_config_t* config);

/**
 * @brief Disable predictive processing for brain region
 *
 * WHAT: Remove predictive coding capabilities
 * WHY:  Clean up resources, disable hierarchical processing
 *
 * @param region Brain region
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_disable_predictive(brain_region_t* region);

/**
 * @brief Get predictive extension from region
 *
 * WHAT: Access predictive coding state
 * WHY:  Query/modify predictive processing
 *
 * @param region Brain region
 * @return Predictive extension or NULL if disabled
 *
 * THREAD-SAFE: No (caller must lock)
 */
brain_region_predictive_t* brain_region_get_predictive(brain_region_t* region);

//=============================================================================
// Hierarchical Prediction API
//=============================================================================

/**
 * @brief Generate top-down prediction for lower region
 *
 * WHAT: Higher region predicts lower region activity
 * WHY:  Core of predictive coding - predictions flow down hierarchy
 * HOW:  Use predictive hierarchy to generate prediction from representations
 *
 * FORMULA: μ̂_lower = f(W × μ_higher + b)
 *
 * @param region Higher region (generates prediction)
 * @param lower_region_id ID of lower region to predict
 * @param prediction Output prediction buffer [lower_region_neurons]
 * @param prediction_size Size of prediction buffer
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates region->predictive_extension->current_prediction
 * - Broadcasts BIO_MSG_PREDICTION_UPDATE if configured
 * - Logs to security audit trail
 *
 * COMPLEXITY: O(N × M) where N=higher units, M=lower units
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_predict_lower(brain_region_t* region,
                                           uint32_t lower_region_id,
                                           float* prediction,
                                           uint32_t prediction_size);

/**
 * @brief Compute prediction error against top-down prediction
 *
 * WHAT: Compare actual activity to predicted activity
 * WHY:  Prediction errors drive learning and propagate up hierarchy
 * HOW:  ε = actual - predicted, weighted by precision
 *
 * FORMULA: ε_i = π_i × (x_i - μ̂_i)
 *
 * @param region Region computing error
 * @param actual Actual activity [num_neurons]
 * @param predicted Prediction from higher region [num_neurons]
 * @param error Output error buffer [num_neurons]
 * @param error_size Size of error buffer
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates region->predictive_extension->prediction_error
 * - Broadcasts BIO_MSG_PREDICTION_ERROR if configured
 * - Updates error statistics
 *
 * COMPLEXITY: O(N)
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_compute_error(brain_region_t* region,
                                           const float* actual,
                                           const float* predicted,
                                           float* error,
                                           uint32_t error_size);

/**
 * @brief Update region based on precision-weighted prediction error
 *
 * WHAT: Adjust representations to minimize prediction error
 * WHY:  Core inference step - minimize free energy
 * HOW:  Gradient descent on free energy using precision-weighted errors
 *
 * FORMULA: Δμ = -α × π × ε
 *
 * @param region Region to update
 * @param error Prediction error [num_neurons]
 * @param precision Precision weights [num_neurons] (NULL = use learned)
 * @param dt Time step (ms)
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates neural activity in region
 * - Updates predictive hierarchy state
 * - May trigger weight updates if learning enabled
 *
 * COMPLEXITY: O(N)
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_update_from_error(brain_region_t* region,
                                               const float* error,
                                               const float* precision,
                                               float dt);

//=============================================================================
// Hierarchical Processing Flow
//=============================================================================

/**
 * @brief Execute one hierarchical prediction cycle
 *
 * WHAT: Run complete prediction → error → update cycle
 * WHY:  Implements full predictive processing loop
 * HOW:
 *   1. Generate top-down predictions for lower regions
 *   2. Compute prediction errors against predictions
 *   3. Update representations based on errors
 *   4. Propagate errors up to higher regions
 *
 * @param region Brain region
 * @param sensory_input Bottom-up sensory input (NULL for higher regions)
 * @param input_size Size of input (0 if NULL)
 * @param dt Time step (ms)
 * @return NIMCP_SUCCESS on success
 *
 * ALGORITHM:
 * ```
 * if has_higher_regions:
 *     receive_top_down_prediction()
 *     compute_prediction_error()
 *     update_from_error()
 *
 * if has_lower_regions:
 *     generate_predictions_for_lower()
 *     broadcast_predictions()
 *
 * update_precisions()
 * compute_free_energy()
 * ```
 *
 * COMPLEXITY: O(N × H) where N=neurons, H=hierarchy depth
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_hierarchical_step(brain_region_t* region,
                                                const float* sensory_input,
                                                uint32_t input_size,
                                                float dt);

/**
 * @brief Run hierarchical prediction to convergence
 *
 * WHAT: Iterate prediction cycles until free energy stabilizes
 * WHY:  Find optimal representations that minimize surprise
 * HOW:  Repeat hierarchical_step until convergence or max iterations
 *
 * @param region Brain region
 * @param sensory_input Bottom-up input
 * @param input_size Size of input
 * @param max_iterations Maximum iterations (0 = use config default)
 * @param tolerance Convergence threshold (0 = use config default)
 * @return Number of iterations taken
 *
 * CONVERGENCE: Stops when |ΔF| / F < tolerance
 *
 * COMPLEXITY: O(I × N × H) where I=iterations
 * THREAD-SAFE: Yes (locks region)
 */
uint32_t brain_region_hierarchical_converge(brain_region_t* region,
                                             const float* sensory_input,
                                             uint32_t input_size,
                                             uint32_t max_iterations,
                                             float tolerance);

//=============================================================================
// Precision (Attention) Modulation
//=============================================================================

/**
 * @brief Set precision weights for region
 *
 * WHAT: Modulate prediction error gain via precision (attention)
 * WHY:  Attention controls which errors drive inference
 * HOW:  Precision weights scale prediction errors
 *
 * FORMULA: ε_attended = π_i × ε_i
 *
 * @param region Brain region
 * @param precisions Precision weights [num_neurons] (NULL = uniform)
 * @param precision_size Size of precision array
 * @return NIMCP_SUCCESS on success
 *
 * USAGE:
 * - High precision (π >> 1): Attend to errors, strong correction
 * - Low precision (π << 1): Ignore errors, weak correction
 * - Can be modulated by top-down attention signals
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_set_precision(brain_region_t* region,
                                           const float* precisions,
                                           uint32_t precision_size);

/**
 * @brief Get current precision weights
 *
 * WHAT: Query current precision (attention) state
 * WHY:  Monitor attention allocation
 *
 * @param region Brain region
 * @param precisions Output buffer [num_neurons]
 * @param precision_size Size of output buffer
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_get_precision(brain_region_t* region,
                                           float* precisions,
                                           uint32_t precision_size);

/**
 * @brief Learn precisions from prediction error statistics
 *
 * WHAT: Adapt precision weights based on error variance
 * WHY:  Automatic attention allocation to reliable signals
 * HOW:  High precision where errors are small and consistent
 *
 * FORMULA: π_i = 1 / <ε_i²>
 *
 * @param region Brain region
 * @param dt Time step for learning (ms)
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Updates region->predictive_extension->precision_weights
 * - May broadcast attention shift messages
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_learn_precisions(brain_region_t* region, float dt);

//=============================================================================
// Inter-Region Hierarchical Connections
//=============================================================================

/**
 * @brief Connect regions hierarchically for predictive processing
 *
 * WHAT: Establish prediction flow between regions
 * WHY:  Build hierarchical predictive network
 * HOW:  Configure higher region to predict lower region
 *
 * @param higher_region Higher region (generates predictions)
 * @param lower_region Lower region (receives predictions)
 * @param connection_strength Strength of predictive connection (0-1)
 * @return NIMCP_SUCCESS on success
 *
 * SIDE EFFECTS:
 * - Adds lower_region to higher_region's output_region_ids
 * - Adds higher_region to lower_region's input_region_ids
 * - Initializes prediction weights between regions
 *
 * THREAD-SAFE: Yes (locks both regions)
 */
nimcp_result_t brain_region_connect_predictive(brain_region_t* higher_region,
                                                 brain_region_t* lower_region,
                                                 float connection_strength);

/**
 * @brief Disconnect predictive connection between regions
 *
 * WHAT: Remove hierarchical prediction flow
 * WHY:  Reconfigure hierarchy, remove connections
 *
 * @param higher_region Higher region
 * @param lower_region Lower region
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks both regions)
 */
nimcp_result_t brain_region_disconnect_predictive(brain_region_t* higher_region,
                                                    brain_region_t* lower_region);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register region with bio-async for predictive messaging
 *
 * WHAT: Enable bio-async message handling for predictions/errors
 * WHY:  Loosely coupled hierarchical communication
 * HOW:  Register callbacks for prediction messages
 *
 * @param region Brain region
 * @param module_id Bio-async module ID for this region
 * @return NIMCP_SUCCESS on success
 *
 * SUBSCRIBES TO:
 * - BIO_MSG_PREDICTION_UPDATE: Receive top-down predictions
 * - BIO_MSG_PREDICTION_ERROR: Receive bottom-up errors
 * - BIO_MSG_ATTENTION_SHIFT: Modulate precisions
 *
 * PUBLISHES:
 * - BIO_MSG_PREDICTION_UPDATE: Send predictions to lower regions
 * - BIO_MSG_PREDICTION_ERROR: Send errors to higher regions
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_register_predictive_bio_async(brain_region_t* region,
                                                            bio_module_id_t module_id);

/**
 * @brief Unregister region from bio-async
 *
 * WHAT: Disable bio-async messaging
 * WHY:  Clean up, disable hierarchical communication
 *
 * @param region Brain region
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_unregister_predictive_bio_async(brain_region_t* region);

/**
 * @brief Broadcast prediction update via bio-async
 *
 * WHAT: Send top-down prediction to lower regions
 * WHY:  Hierarchical communication via message passing
 * HOW:  Package prediction as BIO_MSG_PREDICTION_UPDATE
 *
 * @param region Source region
 * @param target_region_id Target region ID (0 = broadcast)
 * @param prediction Prediction data [size]
 * @param size Size of prediction
 * @return NIMCP_SUCCESS on success
 *
 * MESSAGE CONTENT:
 * - Source: region module ID
 * - Target: target_region_id
 * - Channel: Serotonin (slow, modulatory)
 * - Payload: prediction array
 *
 * THREAD-SAFE: Yes
 */
nimcp_result_t brain_region_broadcast_prediction(brain_region_t* region,
                                                   uint32_t target_region_id,
                                                   const float* prediction,
                                                   uint32_t size);

/**
 * @brief Broadcast prediction error via bio-async
 *
 * WHAT: Send bottom-up error to higher regions
 * WHY:  Errors drive learning and inference in higher regions
 * HOW:  Package error as BIO_MSG_PREDICTION_ERROR
 *
 * @param region Source region
 * @param target_region_id Target region ID (0 = broadcast)
 * @param error Error data [size]
 * @param size Size of error
 * @return NIMCP_SUCCESS on success
 *
 * MESSAGE CONTENT:
 * - Source: region module ID
 * - Target: target_region_id
 * - Channel: Norepinephrine (alerting, salient)
 * - Payload: error array
 *
 * THREAD-SAFE: Yes
 */
nimcp_result_t brain_region_broadcast_error(brain_region_t* region,
                                              uint32_t target_region_id,
                                              const float* error,
                                              uint32_t size);

//=============================================================================
// Query and Statistics
//=============================================================================

/**
 * @brief Get current prediction for region
 *
 * WHAT: Query top-down prediction
 * WHY:  Monitor predictive processing
 *
 * @param region Brain region
 * @param prediction Output buffer [num_neurons]
 * @param size Buffer size
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_get_prediction(brain_region_t* region,
                                             float* prediction,
                                             uint32_t size);

/**
 * @brief Get current prediction error for region
 *
 * WHAT: Query prediction error
 * WHY:  Monitor surprise, novelty detection
 *
 * @param region Brain region
 * @param error Output buffer [num_neurons]
 * @param size Buffer size
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_get_prediction_error(brain_region_t* region,
                                                   float* error,
                                                   uint32_t size);

/**
 * @brief Get free energy for region
 *
 * WHAT: Query variational free energy
 * WHY:  Free energy quantifies surprise/model quality
 * HOW:  F = Σ π × ε² + complexity terms
 *
 * @param region Brain region
 * @return Free energy value
 *
 * INTERPRETATION:
 * - Low F: Good predictions, low surprise
 * - High F: Poor predictions, high surprise
 * - Decreasing F: Learning/adaptation occurring
 *
 * THREAD-SAFE: Yes (locks region)
 */
float brain_region_get_free_energy(brain_region_t* region);

/**
 * @brief Predictive processing statistics
 */
typedef struct {
    uint64_t total_predictions;      /**< Total predictions generated */
    uint64_t total_errors_computed;  /**< Total errors computed */
    float mean_prediction_error;     /**< Mean absolute error */
    float mean_precision;            /**< Mean precision weight */
    float total_free_energy;         /**< Current free energy */
    uint32_t num_input_regions;      /**< Connected input regions */
    uint32_t num_output_regions;     /**< Connected output regions */
    uint32_t hierarchy_level;        /**< Level in hierarchy */
    bool is_converged;               /**< Inference converged */
} brain_region_predictive_stats_t;

/**
 * @brief Get predictive processing statistics
 *
 * WHAT: Query statistics for monitoring
 * WHY:  Track performance, convergence, connectivity
 *
 * @param region Brain region
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_get_predictive_stats(brain_region_t* region,
                                                   brain_region_predictive_stats_t* stats);

//=============================================================================
// Security Integration
//=============================================================================

/**
 * @brief Register predictive updates with Blood-Brain Barrier (BBB)
 *
 * WHAT: Integrate predictive processing with security audit
 * WHY:  Monitor prediction errors for anomalies, attacks
 * HOW:  Log predictions/errors to security system
 *
 * @param region Brain region
 * @param enable Enable BBB integration
 * @return NIMCP_SUCCESS on success
 *
 * SECURITY MONITORING:
 * - Large prediction errors may indicate adversarial input
 * - Sudden precision changes may indicate attention hijacking
 * - Free energy spikes may indicate model confusion
 *
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_predictive_enable_security(brain_region_t* region,
                                                         bool enable);

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Create default predictive configuration
 *
 * WHAT: Factory method for standard configuration
 * WHY:  Reasonable defaults for most use cases
 *
 * @param hierarchy_level Level in hierarchy (0=lowest/sensory)
 * @return Default configuration
 */
brain_region_predictive_config_t brain_region_predictive_config_default(uint32_t hierarchy_level);

/**
 * @brief Create sensory region predictive configuration
 *
 * WHAT: Configuration for sensory (bottom-level) regions
 * WHY:  Sensory regions have different properties than higher regions
 * HOW:  High error sensitivity, fast learning, no predictions downward
 *
 * @return Sensory configuration
 *
 * PROPERTIES:
 * - compute_prediction_errors = true
 * - generate_predictions = false (no regions below)
 * - high precision_learning_rate
 * - broadcast_errors = true
 */
brain_region_predictive_config_t brain_region_predictive_config_sensory(void);

/**
 * @brief Create association region predictive configuration
 *
 * WHAT: Configuration for association (high-level) regions
 * WHY:  Association regions integrate information across modalities
 * HOW:  Bidirectional predictions, slower learning, high-level priors
 *
 * @return Association configuration
 *
 * PROPERTIES:
 * - compute_prediction_errors = true
 * - generate_predictions = true
 * - learn_precisions = true (context-dependent attention)
 * - lower error_correction_rate (slower, more stable)
 */
brain_region_predictive_config_t brain_region_predictive_config_association(void);

//=============================================================================
// Positional Encoding Integration
//=============================================================================

/**
 * @brief Set hierarchy level positional encoding for region
 *
 * WHAT: Apply learned positional encoding to represent hierarchy level
 * WHY:  Cortical hierarchy has distinct levels (V1→V2→V4→IT); explicit encoding needed
 * HOW:  Use learned PE to encode absolute hierarchy position in Free Energy framework
 *
 * BIOLOGICAL BASIS:
 * - Different cortical regions occupy distinct levels in processing hierarchy
 * - V1 (primary visual) → V2 → V4 → IT (inferotemporal cortex)
 * - Position in hierarchy affects prediction dynamics and error propagation
 * - Learned encoding captures task-specific hierarchical structure
 *
 * @param region Brain region to configure
 * @param hierarchy_level Hierarchy level (0=lowest/sensory, higher=abstract)
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * SIDE EFFECTS:
 * - Initializes region->predictive_extension->hierarchy_pe_encoder (LEARNED type)
 * - Computes and stores hierarchy_level_embedding
 * - May broadcast BIO_MSG_ENCODING_COMPUTE if bio-async enabled
 *
 * COMPLEXITY: O(D) where D = embedding dimension
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_set_hierarchy_pe(brain_region_t* region,
                                               uint32_t hierarchy_level);

/**
 * @brief Apply positional encoding to prediction sequence
 *
 * WHAT: Add sinusoidal PE to temporal prediction sequences
 * WHY:  Temporal predictions need sequence order information
 * HOW:  Apply sin/cos PE to prediction vectors for each timestep
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding unfolds over time with sequential predictions
 * - Temporal order critical for prediction accuracy
 * - Sinusoidal encoding provides fixed, extrapolatable position information
 * - Different frequencies capture different timescales
 *
 * FORMULA:
 * - PE(pos, 2i) = sin(pos / 10000^(2i/d))
 * - PE(pos, 2i+1) = cos(pos / 10000^(2i/d))
 * - prediction_encoded[t] = prediction[t] + PE(t)
 *
 * @param region Brain region
 * @param prediction_sequence Sequence of predictions [seq_length * num_neurons]
 * @param seq_length Number of timesteps in sequence
 * @param output Output buffer with PE applied [seq_length * num_neurons]
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * SIDE EFFECTS:
 * - Uses region->predictive_extension->temporal_pe_encoder (SINUSOIDAL type)
 * - Updates temporal_sequence_buffer cache
 * - Broadcasts BIO_MSG_PREDICTION_UPDATE with PE-enhanced predictions
 *
 * COMPLEXITY: O(T × N) where T=seq_length, N=num_neurons
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_encode_prediction_sequence(brain_region_t* region,
                                                         const float* prediction_sequence,
                                                         uint32_t seq_length,
                                                         float* output);

/**
 * @brief Get positional embedding for specific hierarchy level
 *
 * WHAT: Query learned embedding vector for a hierarchy level
 * WHY:  Access position encoding for analysis or injection into representations
 * HOW:  Retrieve learned embedding from hierarchy PE encoder
 *
 * BIOLOGICAL BASIS:
 * - Each cortical level has characteristic processing properties
 * - Embedding captures learned relationship between level and function
 * - Used to modulate predictions based on hierarchical position
 * - Supports context-dependent prediction generation
 *
 * @param region Brain region
 * @param hierarchy_level Level to query (must be < num_levels in hierarchy)
 * @param embedding Output embedding vector [pe_embedding_dim]
 * @param embedding_size Size of output buffer
 * @return NIMCP_SUCCESS on success, error code otherwise
 *
 * USAGE:
 * - Query embeddings for all levels to visualize hierarchy
 * - Compare embeddings to measure hierarchical distance
 * - Inject into neural representations for position-aware processing
 *
 * COMPLEXITY: O(D) where D = embedding dimension
 * THREAD-SAFE: Yes (locks region)
 */
nimcp_result_t brain_region_get_level_embedding(brain_region_t* region,
                                                  uint32_t hierarchy_level,
                                                  float* embedding,
                                                  uint32_t embedding_size);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_BRAIN_REGION_PREDICTIVE_H
