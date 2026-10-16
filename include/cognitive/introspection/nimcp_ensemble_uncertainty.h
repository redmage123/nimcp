/**
 * @file nimcp_ensemble_uncertainty.h
 * @brief Real ensemble-based uncertainty quantification for introspection
 *
 * WHAT: Implements true ensemble uncertainty estimation using multiple neural
 * network snapshots with diverse initializations and training trajectories.
 *
 * WHY: Accurate uncertainty quantification is essential for metacognition and
 * safe decision-making. Simulated uncertainty using random noise does not
 * capture true model uncertainty and data uncertainty.
 *
 * HOW: Create ensemble of N models with different:
 * - Weight initializations (perturbed from base model)
 * - Dropout patterns during training
 * - Training epochs/snapshots
 * Then aggregate predictions to compute:
 * - Epistemic uncertainty (variance across models = model doesn't know)
 * - Aleatoric uncertainty (entropy of predictions = data is noisy)
 *
 * BIOLOGICAL INSPIRATION:
 * - Multiple cortical pathways process same input in parallel
 * - Disagreement between pathways signals uncertainty
 * - Brainstem arousal systems modulate confidence based on pathway consensus
 *
 * MATHEMATICAL FOUNDATION:
 * - Epistemic uncertainty: Var(p_1, p_2, ..., p_N) across ensemble
 * - Aleatoric uncertainty: E[H(p_i)] = average entropy of individual predictions
 * - Total uncertainty: epistemic + aleatoric
 *
 * PERFORMANCE:
 * - Ensemble creation: O(N * M) where N = models, M = network size
 * - Prediction: O(N * F) where F = forward pass cost
 * - Uncertainty computation: O(N)
 * - Typical: 5-10 models, ~2-10ms per prediction
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_ENSEMBLE_UNCERTAINTY_H
#define NIMCP_ENSEMBLE_UNCERTAINTY_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/introspection/nimcp_introspection.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Single model in ensemble
 * WHY: Track individual model state and predictions
 * HOW: Store network snapshot and metadata
 */
typedef struct ensemble_model_struct {
    adaptive_network_t network;     /* Neural network snapshot */
    uint32_t model_id;              /* Unique identifier */
    float weight_perturbation;      /* Weight noise magnitude used */
    uint32_t training_epoch;        /* Epoch this snapshot was taken */
    uint64_t creation_time;         /* When model was created */
    float* last_prediction;         /* Cache of last prediction */
    uint32_t prediction_size;       /* Size of prediction vector */
} ensemble_model_t;

/**
 * WHAT: Configuration for ensemble creation
 * WHY: Control ensemble diversity and size
 * HOW: Parameters for model initialization and training
 */
typedef struct {
    uint32_t num_models;            /* Number of models in ensemble (default 5) */
    float weight_noise_sigma;       /* Std dev for weight perturbation (default 0.1) */
    float dropout_rate;             /* Dropout rate during training (default 0.2) */
    bool use_bootstrap;             /* Bootstrap sampling of training data */
    bool use_snapshot_ensemble;     /* Use snapshots from training trajectory */
    uint32_t snapshot_interval;     /* Epochs between snapshots */
    uint32_t max_models;            /* Maximum models to keep */
} ensemble_config_t;

/**
 * WHAT: Single model's prediction
 * WHY: Track individual model outputs for aggregation
 * HOW: Store prediction vector and metadata
 */
typedef struct {
    float* prediction;              /* Prediction vector */
    uint32_t size;                  /* Vector dimension */
    float confidence;               /* Model's internal confidence (0-1) */
    float entropy;                  /* Entropy of this prediction */
} ensemble_prediction_t;

/**
 * WHAT: Aggregated uncertainty from ensemble
 * WHY: Comprehensive uncertainty quantification
 * HOW: Combine predictions from all models
 */
typedef struct {
    float epistemic;                /* Model uncertainty (variance) */
    float aleatoric;                /* Data uncertainty (entropy) */
    float total;                    /* Combined uncertainty */
    float confidence;               /* 1.0 - total uncertainty */
    uint32_t num_models;            /* Number of models used */
    float* mean_prediction;         /* Mean across ensemble */
    float* prediction_variance;     /* Variance per output dimension */
    uint32_t prediction_size;       /* Dimension of predictions */
    ensemble_prediction_t* individual_predictions; /* Individual model outputs */
} ensemble_uncertainty_result_t;

/**
 * WHAT: Opaque handle to ensemble
 * WHY: Encapsulation and thread safety
 * HOW: Pimpl idiom
 */
typedef struct ensemble_context_struct* ensemble_context_t;

/* ========================================================================
 * ENSEMBLE LIFECYCLE
 * ======================================================================== */

/**
 * WHAT: Get default ensemble configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULTS:
 * - num_models: 5 (good balance of accuracy vs speed)
 * - weight_noise_sigma: 0.1 (10% weight perturbation)
 * - dropout_rate: 0.2 (20% dropout)
 * - use_bootstrap: false
 * - use_snapshot_ensemble: false
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
ensemble_config_t ensemble_default_config(void);

/**
 * WHAT: Create ensemble from base network
 * WHY: Initialize ensemble for uncertainty estimation
 * HOW: Create N perturbed copies of base network
 *
 * ALGORITHM:
 * 1. Copy base network N times
 * 2. For each copy, perturb weights with Gaussian noise
 * 3. Optionally apply different dropout masks
 * 4. Store models in ensemble context
 *
 * @param base_network Base neural network to ensemble
 * @param config Ensemble configuration (NULL for defaults)
 * @return Ensemble context or NULL on error
 *
 * ERRORS:
 * - Returns NULL if base_network is NULL
 * - Returns NULL if allocation fails
 *
 * MEMORY: Caller must call ensemble_destroy() when done
 *
 * COMPLEXITY: O(N * M) where N = num_models, M = network size
 * THREAD-SAFE: Yes
 */
ensemble_context_t ensemble_create(adaptive_network_t base_network,
                                   const ensemble_config_t* config);

/**
 * WHAT: Destroy ensemble and free resources
 * WHY: Prevent memory leaks
 * HOW: Free all models and context
 *
 * @param ensemble Ensemble to destroy
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(N) where N = num_models
 * THREAD-SAFE: Yes (caller must ensure no concurrent access)
 */
void ensemble_destroy(ensemble_context_t ensemble);

/**
 * WHAT: Add trained model to ensemble
 * WHY: Incrementally build ensemble from training checkpoints
 * HOW: Add model snapshot to ensemble
 *
 * USAGE:
 * - During training, periodically add snapshots
 * - After training different initializations
 * - From different training algorithms
 *
 * @param ensemble Ensemble context
 * @param network Network snapshot to add
 * @param training_epoch Epoch number for tracking
 * @return true on success, false on error
 *
 * ERRORS: Returns false if ensemble is full (reached max_models)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (with internal locking)
 */
bool ensemble_add_model(ensemble_context_t ensemble,
                       adaptive_network_t network,
                       uint32_t training_epoch);

/**
 * WHAT: Get number of models in ensemble
 * WHY: Check ensemble size
 * HOW: Return model count
 *
 * @param ensemble Ensemble context
 * @return Number of models, or 0 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t ensemble_get_size(ensemble_context_t ensemble);

/* ========================================================================
 * PREDICTION AND UNCERTAINTY
 * ======================================================================== */

/**
 * WHAT: Get predictions from all ensemble models
 * WHY: Basis for uncertainty quantification
 * HOW: Run input through each model, collect outputs
 *
 * ALGORITHM:
 * 1. For each model in ensemble:
 *    - Run forward pass with input
 *    - Store prediction
 *    - Compute prediction entropy
 * 2. Return array of predictions
 *
 * @param ensemble Ensemble context
 * @param features Input features
 * @param num_features Number of input features
 * @param predictions Output: array of predictions (allocated by function)
 * @return Number of predictions (should equal num_models)
 *
 * MEMORY: Caller must free predictions array and each prediction's data
 *
 * COMPLEXITY: O(N * F) where N = models, F = forward pass cost
 * THREAD-SAFE: Yes
 */
uint32_t ensemble_predict(ensemble_context_t ensemble,
                          const float* features,
                          uint32_t num_features,
                          ensemble_prediction_t** predictions);

/**
 * WHAT: Compute uncertainty from ensemble predictions
 * WHY: Quantify epistemic and aleatoric uncertainty
 * HOW: Compute variance and entropy from predictions
 *
 * ALGORITHM:
 * 1. Compute mean prediction: μ = (1/N) Σ p_i
 * 2. Epistemic = Var(p_1, ..., p_N) = (1/N) Σ (p_i - μ)²
 * 3. Aleatoric = E[H(p_i)] = (1/N) Σ H(p_i)
 * 4. Total = epistemic + aleatoric
 *
 * INTERPRETATION:
 * - High epistemic → models disagree (lack of data/training)
 * - High aleatoric → inherent data noise
 * - Low total → high confidence
 *
 * @param ensemble Ensemble context
 * @param features Input features
 * @param num_features Number of features
 * @return Uncertainty result (must be freed with ensemble_uncertainty_free)
 *
 * ERRORS: Returns zeroed struct on error
 *
 * COMPLEXITY: O(N * F + N * D) where D = output dimension
 * THREAD-SAFE: Yes
 */
ensemble_uncertainty_result_t ensemble_compute_uncertainty(
    ensemble_context_t ensemble,
    const float* features,
    uint32_t num_features);

/**
 * WHAT: Free uncertainty result
 * WHY: Release allocated memory
 * HOW: Free all arrays in result
 *
 * @param result Uncertainty result to free
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(N) where N = num_models
 * THREAD-SAFE: Yes
 */
void ensemble_uncertainty_free(ensemble_uncertainty_result_t* result);

/**
 * WHAT: Free array of predictions
 * WHY: Release memory from ensemble_predict
 * HOW: Free each prediction and array
 *
 * @param predictions Array of predictions
 * @param num_predictions Number of predictions
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(N)
 * THREAD-SAFE: Yes
 */
void ensemble_predictions_free(ensemble_prediction_t* predictions,
                               uint32_t num_predictions);

/* ========================================================================
 * INTROSPECTION INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Attach ensemble to introspection context
 * WHY: Enable real uncertainty estimation in brain_get_uncertainty()
 * HOW: Store ensemble reference in introspection context
 *
 * USAGE:
 * ```c
 * // Create ensemble from brain's network
 * adaptive_network_t network = brain_get_network(brain);
 * ensemble_context_t ensemble = ensemble_create(network, NULL);
 *
 * // Attach to introspection
 * introspection_context_t intro = brain_get_introspection(brain);
 * introspection_set_ensemble(intro, ensemble);
 *
 * // Now brain_get_uncertainty() uses real ensemble
 * brain_uncertainty_t unc = brain_get_uncertainty(intro, features, n);
 * ```
 *
 * @param context Introspection context
 * @param ensemble Ensemble to attach
 * @return true on success, false on error
 *
 * NOTE: Context does NOT take ownership - caller must destroy ensemble
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool introspection_set_ensemble(introspection_context_t context,
                                ensemble_context_t ensemble);

/**
 * WHAT: Get ensemble from introspection context
 * WHY: Access ensemble for manual uncertainty computation
 * HOW: Return ensemble reference
 *
 * @param context Introspection context
 * @return Ensemble context or NULL if none attached
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
ensemble_context_t introspection_get_ensemble(introspection_context_t context);

/**
 * WHAT: Create ensemble from brain snapshots
 * WHY: Build ensemble from training checkpoints
 * HOW: Load multiple brain states and create ensemble
 *
 * USAGE:
 * - Save brain at different training epochs
 * - Load all snapshots into ensemble
 * - Use for uncertainty estimation
 *
 * @param brain Brain instance
 * @param checkpoint_paths Array of paths to brain checkpoints
 * @param num_checkpoints Number of checkpoints
 * @param config Ensemble configuration (NULL for defaults)
 * @return Ensemble context or NULL on error
 *
 * COMPLEXITY: O(N * L) where N = checkpoints, L = load time
 * THREAD-SAFE: Yes
 */
ensemble_context_t ensemble_train_from_brain(
    brain_t brain,
    const char** checkpoint_paths,
    uint32_t num_checkpoints,
    const ensemble_config_t* config);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Compute entropy of probability distribution
 * WHY: Measure uncertainty of single prediction
 * HOW: H(p) = -Σ p_i log2(p_i)
 *
 * @param probabilities Probability distribution
 * @param size Number of elements
 * @return Entropy in bits
 *
 * COMPLEXITY: O(N)
 * THREAD-SAFE: Yes
 */
float ensemble_compute_entropy(const float* probabilities, uint32_t size);

/**
 * WHAT: Compute variance across ensemble predictions
 * WHY: Measure disagreement between models
 * HOW: Var = (1/N) Σ (p_i - μ)²
 *
 * @param predictions Array of prediction vectors
 * @param num_predictions Number of predictions
 * @param dimension Dimension of each prediction
 * @param variance Output: variance per dimension (allocated by caller)
 * @return Overall variance (mean across dimensions)
 *
 * COMPLEXITY: O(N * D)
 * THREAD-SAFE: Yes
 */
float ensemble_compute_variance(const float** predictions,
                                uint32_t num_predictions,
                                uint32_t dimension,
                                float* variance);

/**
 * WHAT: Compute mean prediction across ensemble
 * WHY: Get consensus prediction
 * HOW: μ = (1/N) Σ p_i
 *
 * @param predictions Array of prediction vectors
 * @param num_predictions Number of predictions
 * @param dimension Dimension of each prediction
 * @param mean Output: mean prediction (allocated by caller)
 *
 * COMPLEXITY: O(N * D)
 * THREAD-SAFE: Yes
 */
void ensemble_compute_mean(const float** predictions,
                          uint32_t num_predictions,
                          uint32_t dimension,
                          float* mean);

/**
 * WHAT: Get ensemble statistics
 * WHY: Monitor ensemble health
 * HOW: Return metadata about ensemble
 */
typedef struct {
    uint32_t num_models;            /* Current models in ensemble */
    uint32_t max_models;            /* Maximum capacity */
    float avg_prediction_time_ms;   /* Average prediction time */
    uint64_t total_predictions;     /* Total predictions made */
    float avg_epistemic;            /* Average epistemic uncertainty */
    float avg_aleatoric;            /* Average aleatoric uncertainty */
    size_t memory_used_bytes;       /* Memory usage estimate */
} ensemble_stats_t;

/**
 * WHAT: Get ensemble statistics
 * WHY: Monitor performance and health
 * HOW: Return statistics structure
 *
 * @param ensemble Ensemble context
 * @param stats Output statistics
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool ensemble_get_stats(ensemble_context_t ensemble, ensemble_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENSEMBLE_UNCERTAINTY_H */
