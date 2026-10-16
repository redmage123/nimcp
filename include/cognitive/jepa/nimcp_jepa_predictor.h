/**
 * @file nimcp_jepa_predictor.h
 * @brief JEPA Predictor Module - Latent Space Prediction
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Predictor network that predicts latent embeddings from context
 * WHY:  Core of JEPA - predict representations rather than raw data
 * HOW:  MLP-based predictor with FEP integration for precision weighting
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * JEPA PREDICTION (LeCun, 2022):
 * ------------------------------
 * Given a context embedding z_ctx and a target mask, the predictor
 * generates a predicted target embedding:
 *
 *   z_pred = Predictor(z_ctx, mask)
 *
 * The loss is computed in embedding space, not pixel/token space:
 *
 *   L = ||z_pred - z_target||²
 *
 * This allows learning abstract representations focused on semantics.
 *
 * FEP INTEGRATION:
 * ----------------
 * Prediction errors become precision-weighted free energy terms:
 *
 *   ε = z_pred - z_target
 *   F += π × ||ε||²
 *
 * Precision (attention) modulates how much each prediction error
 * contributes to learning.
 *
 * MLP ARCHITECTURE:
 * -----------------
 * Default 2-layer MLP with GELU activation:
 *
 *   h = GELU(W1 @ z_ctx + b1)
 *   z_pred = W2 @ h + b2
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * - Predictor models cortical association areas
 * - Top-down predictions from higher to lower regions
 * - Prediction errors drive synaptic plasticity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_PREDICTOR_H
#define NIMCP_JEPA_PREDICTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for JEPA predictor */
#define BIO_MODULE_JEPA_PREDICTOR               0x0E01

/** @brief Maximum hidden layer size */
#define JEPA_PREDICTOR_MAX_HIDDEN               2048

/** @brief Default hidden layer size (tier-dependent) */
#ifndef NIMCP_JEPA_PREDICTOR_HIDDEN
    #define NIMCP_JEPA_PREDICTOR_HIDDEN         256
#endif

/** @brief Maximum number of MLP layers */
#define JEPA_PREDICTOR_MAX_LAYERS               8

/** @brief Default number of MLP layers */
#define JEPA_PREDICTOR_DEFAULT_LAYERS           2

/** @brief Default learning rate */
#define JEPA_PREDICTOR_DEFAULT_LR               0.001f

/** @brief Default weight decay */
#define JEPA_PREDICTOR_DEFAULT_WEIGHT_DECAY     0.01f

/** @brief Gradient clipping threshold */
#define JEPA_PREDICTOR_GRAD_CLIP                1.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Predictor architecture types
 */
typedef enum {
    JEPA_PREDICTOR_LINEAR = 0,      /**< Single linear projection */
    JEPA_PREDICTOR_MLP,             /**< Multi-layer perceptron (default) */
    JEPA_PREDICTOR_TRANSFORMER,     /**< Self-attention based */
    JEPA_PREDICTOR_RECURRENT        /**< GRU/LSTM for sequences */
} jepa_predictor_type_t;

/**
 * @brief Activation function types
 */
typedef enum {
    JEPA_ACT_NONE = 0,              /**< No activation (linear) */
    JEPA_ACT_RELU,                  /**< ReLU: max(0, x) */
    JEPA_ACT_GELU,                  /**< GELU: x * Φ(x) (default) */
    JEPA_ACT_TANH,                  /**< Tanh: hyperbolic tangent */
    JEPA_ACT_SIGMOID                /**< Sigmoid: 1/(1+exp(-x)) */
} jepa_activation_t;

/**
 * @brief Loss function types
 */
typedef enum {
    JEPA_LOSS_MSE = 0,              /**< Mean Squared Error (default) */
    JEPA_LOSS_COSINE,               /**< Cosine similarity loss */
    JEPA_LOSS_SMOOTH_L1,            /**< Smooth L1 (Huber) loss */
    JEPA_LOSS_PRECISION_WEIGHTED    /**< Precision-weighted MSE */
} jepa_loss_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Single MLP layer parameters
 */
typedef struct {
    float* weights;                 /**< Weight matrix [out_dim × in_dim] */
    float* bias;                    /**< Bias vector [out_dim] */
    uint32_t in_dim;                /**< Input dimension */
    uint32_t out_dim;               /**< Output dimension */
    jepa_activation_t activation;   /**< Activation function */

    /* Gradient storage (for training) */
    float* grad_weights;            /**< Weight gradients */
    float* grad_bias;               /**< Bias gradients */
} jepa_mlp_layer_t;

/**
 * @brief MLP predictor parameters
 */
typedef struct {
    jepa_mlp_layer_t* layers;       /**< Array of layers */
    uint32_t num_layers;            /**< Number of layers */

    /* Intermediate activations (for backprop) */
    float** activations;            /**< Per-layer activations */
    float** pre_activations;        /**< Pre-activation values */
} jepa_mlp_t;

/**
 * @brief Prediction error with FEP integration
 */
typedef struct {
    float* error;                   /**< Raw error z_pred - z_target */
    float* weighted_error;          /**< Precision-weighted error */
    float loss;                     /**< Scalar loss value */
    float precision;                /**< Applied precision weight */
    uint32_t dim;                   /**< Error dimension */
} jepa_prediction_error_t;

/**
 * @brief Predictor configuration
 */
typedef struct {
    jepa_predictor_type_t type;     /**< Predictor architecture */
    uint32_t input_dim;             /**< Context embedding dimension */
    uint32_t output_dim;            /**< Target embedding dimension */
    uint32_t hidden_dim;            /**< Hidden layer dimension */
    uint32_t num_layers;            /**< Number of hidden layers */
    jepa_activation_t activation;   /**< Hidden layer activation */
    jepa_loss_t loss_type;          /**< Loss function */

    /* Training parameters */
    float learning_rate;            /**< Learning rate */
    float weight_decay;             /**< L2 regularization */
    float dropout_rate;             /**< Dropout probability */
    bool enable_layer_norm;         /**< Apply layer normalization */

    /* FEP integration */
    bool enable_fep;                /**< Enable FEP precision weighting */
    float initial_precision;        /**< Initial prediction precision */
} jepa_predictor_config_t;

/**
 * @brief Predictor statistics
 */
typedef struct {
    uint64_t predictions_made;      /**< Total predictions */
    uint64_t updates_applied;       /**< Weight updates */
    float avg_loss;                 /**< Running average loss */
    float avg_precision;            /**< Running average precision */
    float min_loss;                 /**< Minimum loss seen */
    float max_loss;                 /**< Maximum loss seen */
} jepa_predictor_stats_t;

/**
 * @brief JEPA Predictor state
 *
 * Uses bridge pattern for FEP integration
 */
typedef struct jepa_predictor {
    bridge_base_t base;             /**< MUST be first - bridge pattern */

    /* Configuration */
    jepa_predictor_config_t config;

    /* Predictor network */
    jepa_predictor_type_t type;
    union {
        jepa_mlp_t mlp;             /**< MLP parameters */
        /* Future: transformer, recurrent */
    } network;

    /* FEP integration */
    fep_belief_t* belief_state;     /**< FEP belief about predictions */
    float prediction_precision;     /**< Current precision estimate */

    /* Training state */
    bool training_mode;             /**< Training vs inference mode */
    uint64_t step_count;            /**< Training step counter */

    /* Statistics */
    jepa_predictor_stats_t stats;

    /* Temporary buffers */
    float* temp_hidden;             /**< Temporary hidden activation */
    float* temp_output;             /**< Temporary output buffer */
} jepa_predictor_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default predictor configuration
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_default_config(jepa_predictor_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a JEPA predictor
 *
 * WHAT: Initialize predictor network with given architecture
 * WHY:  Core component for JEPA embedding prediction
 * HOW:  Allocate weights, initialize randomly, setup FEP if enabled
 *
 * @param config Configuration (NULL for defaults)
 * @return New predictor or NULL on failure
 */
jepa_predictor_t* jepa_predictor_create(const jepa_predictor_config_t* config);

/**
 * @brief Destroy predictor and free resources
 *
 * @param predictor Predictor to destroy (NULL safe)
 */
void jepa_predictor_destroy(jepa_predictor_t* predictor);

/**
 * @brief Reset predictor to initial state
 *
 * WHAT: Reinitialize weights and reset statistics
 * WHY:  Allow retraining from scratch
 *
 * @param predictor Predictor to reset
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_reset(jepa_predictor_t* predictor);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict target embedding from context
 *
 * WHAT: Forward pass through predictor network
 * WHY:  Core JEPA operation - predict in latent space
 * HOW:  z_pred = MLP(z_ctx)
 *
 * @param predictor Predictor network
 * @param context Context embedding
 * @param prediction Output predicted embedding (must be pre-allocated)
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_predict(jepa_predictor_t* predictor,
                            const jepa_latent_t* context,
                            jepa_latent_t* prediction);

/**
 * @brief Predict with mask (for masked prediction tasks)
 *
 * WHAT: Predict only masked positions
 * WHY:  JEPA training masks parts of input for prediction
 *
 * @param predictor Predictor network
 * @param context Context embedding
 * @param mask Binary mask [dim], 1 = predict this position
 * @param prediction Output prediction
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_predict_masked(jepa_predictor_t* predictor,
                                   const jepa_latent_t* context,
                                   const float* mask,
                                   jepa_latent_t* prediction);

/* ============================================================================
 * Loss Computation API
 * ============================================================================ */

/**
 * @brief Compute prediction error
 *
 * WHAT: Calculate difference between prediction and target
 * WHY:  Error signal for learning
 * HOW:  ε = z_pred - z_target
 *
 * @param predictor Predictor (for precision info)
 * @param prediction Predicted embedding
 * @param target Target embedding
 * @param error Output error structure
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_compute_error(jepa_predictor_t* predictor,
                                  const jepa_latent_t* prediction,
                                  const jepa_latent_t* target,
                                  jepa_prediction_error_t* error);

/**
 * @brief Compute loss value
 *
 * WHAT: Scalar loss from prediction error
 * WHY:  Objective function for training
 *
 * @param predictor Predictor
 * @param prediction Predicted embedding
 * @param target Target embedding
 * @return Loss value, NAN on error
 */
float jepa_predictor_compute_loss(jepa_predictor_t* predictor,
                                   const jepa_latent_t* prediction,
                                   const jepa_latent_t* target);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Set training mode
 *
 * @param predictor Predictor
 * @param training true for training, false for inference
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_set_training(jepa_predictor_t* predictor, bool training);

/**
 * @brief Backward pass - compute gradients
 *
 * WHAT: Backpropagate error through network
 * WHY:  Compute weight gradients for learning
 * HOW:  Standard backpropagation with chain rule
 *
 * @param predictor Predictor
 * @param error Prediction error from forward pass
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_backward(jepa_predictor_t* predictor,
                             const jepa_prediction_error_t* error);

/**
 * @brief Update weights from gradients
 *
 * WHAT: Apply gradient descent step
 * WHY:  Learn to make better predictions
 * HOW:  W -= lr * grad_W + weight_decay * W
 *
 * @param predictor Predictor
 * @param learning_rate Learning rate (0 = use config)
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_update_weights(jepa_predictor_t* predictor,
                                   float learning_rate);

/**
 * @brief Complete training step (forward + backward + update)
 *
 * WHAT: Full training iteration
 * WHY:  Convenience function for common case
 *
 * @param predictor Predictor
 * @param context Context embedding
 * @param target Target embedding
 * @param loss Output loss value (can be NULL)
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_train_step(jepa_predictor_t* predictor,
                               const jepa_latent_t* context,
                               const jepa_latent_t* target,
                               float* loss);

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

/**
 * @brief Update precision from prediction errors
 *
 * WHAT: Learn precision (attention) from error statistics
 * WHY:  FEP treats precision as inverse variance of errors
 * HOW:  π = 1 / E[ε²]
 *
 * @param predictor Predictor
 * @param error Recent prediction error
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_update_precision(jepa_predictor_t* predictor,
                                     const jepa_prediction_error_t* error);

/**
 * @brief Get FEP prediction error for external integration
 *
 * WHAT: Convert predictor error to FEP format
 * WHY:  Allow integration with FEP system
 *
 * @param predictor Predictor
 * @param internal_error Predictor error
 * @param fep_error Output FEP error structure
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_to_fep_error(jepa_predictor_t* predictor,
                                 const jepa_prediction_error_t* internal_error,
                                 fep_prediction_error_t* fep_error);

/* ============================================================================
 * Weight Access API
 * ============================================================================ */

/**
 * @brief Get total number of trainable parameters
 *
 * @param predictor Predictor
 * @return Number of parameters, 0 on error
 */
uint32_t jepa_predictor_num_params(const jepa_predictor_t* predictor);

/**
 * @brief Get weight tensor for layer
 *
 * @param predictor Predictor
 * @param layer_idx Layer index
 * @param weights Output weights pointer
 * @param dims Output dimensions [out_dim, in_dim]
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_get_weights(const jepa_predictor_t* predictor,
                                uint32_t layer_idx,
                                float** weights,
                                uint32_t dims[2]);

/**
 * @brief Set weight tensor for layer
 *
 * @param predictor Predictor
 * @param layer_idx Layer index
 * @param weights Weight values
 * @param in_dim Input dimension
 * @param out_dim Output dimension
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_set_weights(jepa_predictor_t* predictor,
                                uint32_t layer_idx,
                                const float* weights,
                                uint32_t in_dim,
                                uint32_t out_dim);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get predictor statistics
 *
 * @param predictor Predictor
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_get_stats(const jepa_predictor_t* predictor,
                              jepa_predictor_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param predictor Predictor
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_reset_stats(jepa_predictor_t* predictor);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect predictor to bio-async router
 *
 * @param predictor Predictor
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_connect_bio_async(jepa_predictor_t* predictor);

/**
 * @brief Disconnect from bio-async router
 *
 * @param predictor Predictor
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_disconnect_bio_async(jepa_predictor_t* predictor);

/**
 * @brief Check bio-async connection status
 *
 * @param predictor Predictor
 * @return true if connected
 */
bool jepa_predictor_is_bio_async_connected(const jepa_predictor_t* predictor);

/* ============================================================================
 * Prediction Error Management
 * ============================================================================ */

/**
 * @brief Create prediction error structure
 *
 * @param dim Error dimension
 * @return New error structure or NULL
 */
jepa_prediction_error_t* jepa_prediction_error_create(uint32_t dim);

/**
 * @brief Destroy prediction error structure
 *
 * @param error Error to destroy (NULL safe)
 */
void jepa_prediction_error_destroy(jepa_prediction_error_t* error);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert predictor type to string
 *
 * @param type Predictor type
 * @return Human-readable string
 */
const char* jepa_predictor_type_to_string(jepa_predictor_type_t type);

/**
 * @brief Convert activation to string
 *
 * @param activation Activation type
 * @return Human-readable string
 */
const char* jepa_activation_to_string(jepa_activation_t activation);

/**
 * @brief Convert loss type to string
 *
 * @param loss Loss type
 * @return Human-readable string
 */
const char* jepa_loss_to_string(jepa_loss_t loss);

/* ============================================================================
 * Monte Carlo Integration API
 * ============================================================================ */

/**
 * @brief Predict with uncertainty estimation via MC sampling
 *
 * WHAT: Make prediction with uncertainty quantification
 * WHY:  Enable confidence-aware predictions in JEPA
 * HOW:  Sample predictions with noise, compute mean and variance
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param prediction Output prediction (mean)
 * @param uncertainty Output uncertainty (std per dimension, size = output_dim)
 * @param num_samples Number of MC samples
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_predict_with_uncertainty_mc(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    jepa_latent_t* prediction,
    float* uncertainty,
    uint32_t num_samples);

/**
 * @brief Apply MC dropout during training
 *
 * WHAT: Apply stochastic dropout using MC sampling
 * WHY:  Regularization and uncertainty estimation
 * HOW:  Randomly zero elements with dropout rate (inverted dropout)
 *
 * @param activations Activation array to modify in-place
 * @param size Array size
 * @param dropout_rate Probability of dropping [0,1]
 */
void jepa_predictor_apply_dropout_mc(float* activations, uint32_t size, float dropout_rate);

/**
 * @brief Estimate prediction confidence via MC sampling
 *
 * WHAT: Estimate confidence in prediction
 * WHY:  Support uncertainty-aware decision making
 * HOW:  Compute prediction variance over samples
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param num_samples Number of MC samples
 * @return Confidence score [0,1] (inverse of normalized variance)
 */
float jepa_predictor_estimate_confidence_mc(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    uint32_t num_samples);

/**
 * @brief Get thread-local MC seed for JEPA predictor
 *
 * @return Pointer to thread-local seed
 */
uint32_t* jepa_predictor_get_mc_seed(void);

/* ============================================================================
 * Quantum Monte Carlo Integration API
 * ============================================================================ */

/**
 * @brief Configuration for JEPA Quantum Monte Carlo
 */
typedef struct jepa_qmc_config {
    uint32_t num_samples;           /**< Number of QMC samples */
    uint32_t num_iterations;        /**< Annealing iterations */
    float initial_temp;             /**< Initial temperature for annealing */
    float final_temp;               /**< Final temperature for annealing */
    float exploration_constant;     /**< UCB exploration parameter */
    float quantum_strength;         /**< Tunneling probability [0,1] */
    bool use_importance_sampling;   /**< Use importance sampling */
    bool use_adaptive_proposal;     /**< Adapt proposal distribution */
    uint32_t seed;                  /**< RNG seed (0 = time-based) */
} jepa_qmc_config_t;

/**
 * @brief Statistics from JEPA QMC operations
 */
typedef struct jepa_qmc_stats {
    uint32_t samples_taken;         /**< Total samples taken */
    uint32_t tunneling_events;      /**< Quantum tunneling events */
    float acceptance_rate;          /**< M-H acceptance rate */
    float effective_sample_size;    /**< ESS for importance sampling */
    float mean_energy;              /**< Mean energy in annealing */
    float final_energy;             /**< Final energy after annealing */
    float entropy_estimate;         /**< Estimated latent entropy */
    float fidelity_estimate;        /**< Estimated state fidelity */
    float computation_time_ms;      /**< Time spent in QMC */
} jepa_qmc_stats_t;

/**
 * @brief Initialize default QMC configuration
 *
 * @param config Configuration to initialize
 * @return NIMCP_SUCCESS on success
 */
int jepa_qmc_config_init(jepa_qmc_config_t* config);

/**
 * @brief Estimate latent space amplitude uncertainty via QMC
 *
 * WHAT: Quantum-inspired amplitude estimation for latent space
 * WHY:  More accurate uncertainty estimation than standard MC
 * HOW:  Use QMC importance sampling with amplitude weighting
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param target_dim Target dimension for amplitude estimation
 * @param config QMC configuration (NULL = defaults)
 * @param amplitude Output amplitude estimate
 * @param variance Output variance estimate
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_qmc_amplitude_estimate(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    uint32_t target_dim,
    const jepa_qmc_config_t* config,
    float* amplitude,
    float* variance);

/**
 * @brief Optimize predictor weights via quantum annealing
 *
 * WHAT: Use adaptive quantum annealing for weight optimization
 * WHY:  Can escape local minima better than gradient descent
 * HOW:  Minimize prediction error with quantum tunneling transitions
 *
 * @param predictor JEPA predictor
 * @param contexts Array of training contexts
 * @param targets Array of training targets
 * @param num_samples Number of training samples
 * @param config QMC configuration (NULL = defaults)
 * @param stats Output statistics (optional)
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_qmc_adaptive_anneal(
    jepa_predictor_t* predictor,
    const jepa_latent_t** contexts,
    const jepa_latent_t** targets,
    uint32_t num_samples,
    const jepa_qmc_config_t* config,
    jepa_qmc_stats_t* stats);

/**
 * @brief Estimate Shannon entropy of latent space distribution
 *
 * WHAT: Estimate entropy of prediction distribution via QMC
 * WHY:  Entropy measures uncertainty/diversity in latent space
 * HOW:  Sample-based entropy estimation with stratification
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param config QMC configuration (NULL = defaults)
 * @param entropy Output entropy estimate
 * @param std_error Output standard error
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_qmc_entropy(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    const jepa_qmc_config_t* config,
    float* entropy,
    float* std_error);

/**
 * @brief Compute fidelity between two latent states via QMC
 *
 * WHAT: Measure similarity between latent representations
 * WHY:  Fidelity captures quantum-like overlap in latent space
 * HOW:  Compute |<psi1|psi2>|^2 with normalization
 *
 * @param predictor JEPA predictor
 * @param latent1 First latent state
 * @param latent2 Second latent state
 * @return Fidelity value [0,1]
 */
float jepa_predictor_qmc_fidelity(
    jepa_predictor_t* predictor,
    const jepa_latent_t* latent1,
    const jepa_latent_t* latent2);

/**
 * @brief Sample from latent distribution via QMC
 *
 * WHAT: Generate samples from latent space distribution
 * WHY:  Enable generative modeling and exploration
 * HOW:  Use finite-shot QMC measurement simulation
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param samples Output array of samples
 * @param num_samples Number of samples to generate
 * @param config QMC configuration (NULL = defaults)
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_qmc_sample_latent(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    jepa_latent_t** samples,
    uint32_t num_samples,
    const jepa_qmc_config_t* config);

/**
 * @brief Estimate prediction free energy via QMC
 *
 * WHAT: Estimate thermodynamic free energy of prediction
 * WHY:  Free energy connects to FEP and belief optimization
 * HOW:  Partition function estimation via MCMC
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param target Target latent state
 * @param temperature Thermodynamic temperature
 * @param config QMC configuration (NULL = defaults)
 * @param free_energy Output free energy estimate
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_qmc_free_energy(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    const jepa_latent_t* target,
    float temperature,
    const jepa_qmc_config_t* config,
    float* free_energy);

/**
 * @brief Predict with QMC-enhanced uncertainty
 *
 * WHAT: Make prediction with quantum-enhanced uncertainty estimation
 * WHY:  Better uncertainty quantification than standard MC
 * HOW:  Combine amplitude estimation with entropy bounds
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param prediction Output prediction (mean)
 * @param uncertainty Output uncertainty per dimension
 * @param config QMC configuration (NULL = defaults)
 * @param stats Output statistics (optional)
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_predict_qmc(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    jepa_latent_t* prediction,
    float* uncertainty,
    const jepa_qmc_config_t* config,
    jepa_qmc_stats_t* stats);

/**
 * @brief Run MCTS-guided latent space exploration
 *
 * WHAT: Use MCTS to explore latent space for optimal prediction
 * WHY:  Structured exploration can find better predictions
 * HOW:  MCTS with latent states as nodes, transformations as actions
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param exploration_depth Maximum MCTS depth
 * @param config QMC configuration (NULL = defaults)
 * @param best_latent Output best latent state found
 * @param value Output value of best state
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_qmc_mcts_explore(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    uint32_t exploration_depth,
    const jepa_qmc_config_t* config,
    jepa_latent_t* best_latent,
    float* value);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_PREDICTOR_H */
