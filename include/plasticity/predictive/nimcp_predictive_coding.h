/**
 * @file nimcp_predictive_coding.h
 * @brief Predictive Coding - Hierarchical Error Minimization
 *
 * WHAT: Brain-inspired inference through prediction error minimization
 * WHY:  Unifies perception, action, and learning in single framework
 *
 * BIOLOGICAL BASIS:
 * - Free Energy Principle (Friston 2010): Brain minimizes surprise
 * - Predictive Processing (Rao & Ballard 1999): Cortical hierarchy
 * - Error Neurons: Superficial layers encode prediction errors
 * - Representation Neurons: Deep layers encode predictions
 *
 * MATHEMATICAL FORMULATION:
 *
 * 1. Prediction Error:
 *    ε = x - μ̂
 *    Where x = input, μ̂ = prediction
 *
 * 2. Precision-Weighted Error:
 *    ε_π = π × (x - μ̂)
 *    Where π = precision (inverse variance)
 *
 * 3. Free Energy (Variational):
 *    F = Σ π_i × ε_i² + ln|Σ|
 *    Minimize F by updating predictions and precisions
 *
 * 4. Hierarchical Update:
 *    μ̂_L = f(μ̂_{L+1})  (top-down prediction)
 *    ε_L = x_L - μ̂_L    (prediction error)
 *    μ_{L+1} += κ × g(ε_L)  (bottom-up error correction)
 *
 * 5. Precision Learning:
 *    π_L = 1 / <ε_L²>    (learn from error statistics)
 *
 * DESIGN PATTERNS:
 * - Chain of Responsibility: Hierarchical error propagation
 * - Strategy Pattern: Different prediction functions
 * - Observer Pattern: Notify on prediction updates
 *
 * PERFORMANCE:
 * - O(n) per layer for prediction/error
 * - O(L) for hierarchy (L = levels)
 * - Parallelizable across layers
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 */

#ifndef NIMCP_PREDICTIVE_CODING_H
#define NIMCP_PREDICTIVE_CODING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define PC_EPSILON 1e-8f               /**< Numerical stability */
#define PC_MAX_HIERARCHY_LEVELS 16     /**< Maximum hierarchy depth */
#define PC_DEFAULT_PRECISION 1.0f      /**< Default precision (unit variance) */

//=============================================================================
// Prediction Function Types
//=============================================================================

/**
 * @brief Prediction function type (top-down)
 *
 * WHAT: How higher levels predict lower level states
 * WHY:  Different functional forms for different cortical areas
 */
typedef enum {
    PC_PREDICT_LINEAR,       /**< μ̂ = W × μ + b */
    PC_PREDICT_NONLINEAR,    /**< μ̂ = f(W × μ + b) */
    PC_PREDICT_GAUSSIAN,     /**< μ̂ ~ N(μ_prior, Σ_prior) */
    PC_PREDICT_SPARSE,       /**< μ̂ with sparsity constraint */
    PC_PREDICT_IDENTITY      /**< μ̂ = μ (for testing) */
} pc_prediction_type_t;

/**
 * @brief Error type for different pathways
 *
 * WHAT: Type of prediction error computation
 * WHY:  Different error signals for different purposes
 */
typedef enum {
    PC_ERROR_STANDARD,       /**< ε = x - μ̂ */
    PC_ERROR_PRECISION_WEIGHTED, /**< ε = π × (x - μ̂) */
    PC_ERROR_LOG_PRECISION,  /**< ε = log(π) × (x - μ̂)² */
    PC_ERROR_TEMPORAL        /**< ε = x_t - f(x_{t-1}) */
} pc_error_type_t;

//=============================================================================
// Layer Structures
//=============================================================================

/**
 * @brief Predictive coding layer parameters
 *
 * WHAT: Configuration for a single layer in hierarchy
 * WHY:  Different layers have different properties
 */
typedef struct {
    uint32_t num_units;           /**< Number of units in layer */
    pc_prediction_type_t pred_type; /**< Prediction function type */
    pc_error_type_t error_type;   /**< Error computation type */
    float learning_rate_mu;       /**< Learning rate for representations */
    float learning_rate_precision; /**< Learning rate for precisions */
    float learning_rate_weights;  /**< Learning rate for prediction weights */
    float prediction_tau;         /**< Time constant for predictions (ms) */
    float error_tau;              /**< Time constant for errors (ms) */
    float min_precision;          /**< Minimum precision value */
    float max_precision;          /**< Maximum precision value */
} pc_layer_params_t;

/**
 * @brief Predictive coding layer state
 *
 * WHAT: Dynamic state of a layer
 * WHY:  Track predictions, errors, and precisions
 */
typedef struct {
    float* mu;                    /**< Representations/predictions [num_units] */
    float* mu_prior;              /**< Prior predictions from above [num_units] */
    float* error;                 /**< Prediction errors [num_units] */
    float* precision;             /**< Precisions (inverse variance) [num_units] */
    float* precision_log;         /**< Log precisions for stable learning */
    float* error_variance;        /**< Running variance of errors [num_units] */
    float free_energy;            /**< Layer contribution to free energy */
    uint32_t num_units;
} pc_layer_state_t;

/**
 * @brief Prediction weights between layers
 *
 * WHAT: Top-down prediction mapping
 * WHY:  Higher layers predict lower layer states
 *
 * Maps from layer L+1 (num_higher units) to layer L (num_lower units)
 */
typedef struct {
    float* weights;               /**< Weight matrix [num_lower × num_higher] */
    float* bias;                  /**< Bias vector [num_lower] */
    uint32_t num_lower;           /**< Units in lower layer */
    uint32_t num_higher;          /**< Units in higher layer */
} pc_prediction_weights_t;

//=============================================================================
// Hierarchy Structures
//=============================================================================

/**
 * @brief Predictive coding hierarchy configuration
 *
 * WHAT: Full configuration for predictive hierarchy
 * WHY:  Specify structure and parameters
 */
typedef struct {
    uint32_t num_levels;          /**< Number of hierarchical levels */
    uint32_t* units_per_level;    /**< Array of unit counts [num_levels] */
    pc_prediction_type_t pred_type; /**< Default prediction type */
    pc_error_type_t error_type;   /**< Default error type */
    float learning_rate;          /**< Global learning rate multiplier */
    float precision_learning_rate; /**< Learning rate for precisions */
    bool learn_precisions;        /**< Enable precision learning */
    bool use_lateral_connections; /**< Enable lateral (within-layer) connections */
    float dt;                     /**< Default timestep (ms) */
} pc_hierarchy_config_t;

/**
 * @brief Predictive coding hierarchy statistics
 *
 * WHAT: Monitoring metrics for predictive coding
 * WHY:  Track convergence and performance
 */
typedef struct {
    uint64_t total_updates;
    float total_free_energy;      /**< Sum of free energy across layers */
    float mean_error;             /**< Mean absolute prediction error */
    float mean_precision;         /**< Mean precision across layers */
    float precision_variance;     /**< Variance in precisions */
    float* layer_free_energies;   /**< Per-layer free energies */
    float* layer_mean_errors;     /**< Per-layer mean errors */
    float convergence_rate;       /**< Rate of free energy decrease */
    bool is_converged;            /**< True if below threshold */
} pc_hierarchy_stats_t;

/**
 * @brief Opaque handle to predictive coding hierarchy
 */
typedef struct pc_hierarchy_struct* pc_hierarchy_t;

//=============================================================================
// Factory Functions
//=============================================================================

/**
 * @brief Create default layer parameters
 *
 * WHAT: Factory method for layer parameters
 * WHY:  Provides reasonable defaults
 *
 * @param num_units Number of units in layer
 * @return Layer parameters
 */
pc_layer_params_t pc_layer_params_default(uint32_t num_units);

/**
 * @brief Create default hierarchy configuration
 *
 * WHAT: Factory method for hierarchy configuration
 * WHY:  Standard configuration with defaults
 *
 * @param num_levels Number of hierarchical levels
 * @param units_per_level Array of unit counts
 * @return Hierarchy configuration
 */
pc_hierarchy_config_t pc_hierarchy_config_default(uint32_t num_levels,
                                                   const uint32_t* units_per_level);

/**
 * @brief Create sensory hierarchy configuration
 *
 * WHAT: Configuration for sensory processing
 * WHY:  Hierarchical feature extraction
 *
 * @param input_dim Input dimension
 * @param num_levels Number of levels
 * @return Sensory hierarchy configuration
 */
pc_hierarchy_config_t pc_hierarchy_config_sensory(uint32_t input_dim,
                                                   uint32_t num_levels);

/**
 * @brief Create motor hierarchy configuration
 *
 * WHAT: Configuration for motor control
 * WHY:  Hierarchical action generation
 *
 * @param output_dim Output dimension
 * @param num_levels Number of levels
 * @return Motor hierarchy configuration
 */
pc_hierarchy_config_t pc_hierarchy_config_motor(uint32_t output_dim,
                                                 uint32_t num_levels);

//=============================================================================
// Layer Functions
//=============================================================================

/**
 * @brief Initialize layer state
 *
 * WHAT: Allocate and initialize layer state
 * WHY:  Factory method ensures valid initial state
 *
 * @param params Layer parameters
 * @return Initialized layer state (caller must free)
 */
pc_layer_state_t* pc_layer_state_create(const pc_layer_params_t* params);

/**
 * @brief Free layer state
 *
 * WHAT: Free layer state memory
 * WHY:  Prevent memory leaks
 *
 * @param state Layer state to free
 */
void pc_layer_state_destroy(pc_layer_state_t* state);

/**
 * @brief Compute prediction error
 *
 * WHAT: Calculate ε = x - μ̂
 * WHY:  Core computation of predictive coding
 *
 * FORMULA: ε_i = x_i - μ̂_i
 *          For precision-weighted: ε_i = π_i × (x_i - μ̂_i)
 *
 * COMPLEXITY: O(n) where n = num_units
 *
 * @param state Layer state
 * @param input Sensory input [num_units]
 * @param params Layer parameters
 */
void pc_layer_compute_error(pc_layer_state_t* state,
                            const float* input,
                            const pc_layer_params_t* params);

/**
 * @brief Update representations from error
 *
 * WHAT: Adjust μ to reduce prediction error
 * WHY:  Inference step of predictive coding
 *
 * FORMULA: μ += κ × ε
 *          Or: dμ/dt = -κ × ∂F/∂μ
 *
 * COMPLEXITY: O(n)
 *
 * @param state Layer state
 * @param dt Time step (ms)
 * @param params Layer parameters
 */
void pc_layer_update_representations(pc_layer_state_t* state,
                                     float dt,
                                     const pc_layer_params_t* params);

/**
 * @brief Update precisions from error statistics
 *
 * WHAT: Learn precisions from prediction errors
 * WHY:  Precision weighting is crucial for robust inference
 *
 * FORMULA: π_i = 1 / <ε_i²>
 *          Or: d(log π)/dt = 1 - π × <ε²>
 *
 * COMPLEXITY: O(n)
 *
 * @param state Layer state
 * @param dt Time step (ms)
 * @param params Layer parameters
 */
void pc_layer_update_precisions(pc_layer_state_t* state,
                                float dt,
                                const pc_layer_params_t* params);

/**
 * @brief Compute layer free energy
 *
 * WHAT: Calculate free energy contribution
 * WHY:  Free energy quantifies surprise/prediction quality
 *
 * FORMULA: F = Σ π_i × ε_i² + ln|Σ|
 *
 * COMPLEXITY: O(n)
 *
 * @param state Layer state
 * @return Free energy value
 */
float pc_layer_compute_free_energy(const pc_layer_state_t* state);

//=============================================================================
// Prediction Weight Functions
//=============================================================================

/**
 * @brief Create prediction weights
 *
 * WHAT: Allocate and initialize prediction weights
 * WHY:  Connect layers in hierarchy
 *
 * @param num_lower Units in lower layer
 * @param num_higher Units in higher layer
 * @return Initialized weights (caller must free)
 */
pc_prediction_weights_t* pc_prediction_weights_create(uint32_t num_lower,
                                                       uint32_t num_higher);

/**
 * @brief Free prediction weights
 *
 * @param weights Weights to free
 */
void pc_prediction_weights_destroy(pc_prediction_weights_t* weights);

/**
 * @brief Generate top-down prediction
 *
 * WHAT: Compute prediction from higher layer
 * WHY:  Top-down predictions drive inference
 *
 * FORMULA: μ̂_lower = f(W × μ_higher + b)
 *
 * COMPLEXITY: O(n_lower × n_higher)
 *
 * @param weights Prediction weights
 * @param higher_mu Higher layer representations [num_higher]
 * @param prediction Output prediction [num_lower]
 * @param pred_type Prediction function type
 */
void pc_generate_prediction(const pc_prediction_weights_t* weights,
                            const float* higher_mu,
                            float* prediction,
                            pc_prediction_type_t pred_type);

/**
 * @brief Update prediction weights from errors
 *
 * WHAT: Learn prediction weights via gradient descent
 * WHY:  Improve predictions over time
 *
 * FORMULA: ΔW = -κ × ε_lower × μ_higher^T
 *
 * COMPLEXITY: O(n_lower × n_higher)
 *
 * @param weights Prediction weights to update
 * @param lower_error Lower layer errors [num_lower]
 * @param higher_mu Higher layer representations [num_higher]
 * @param learning_rate Learning rate
 */
void pc_update_prediction_weights(pc_prediction_weights_t* weights,
                                  const float* lower_error,
                                  const float* higher_mu,
                                  float learning_rate);

//=============================================================================
// Hierarchy Functions
//=============================================================================

/**
 * @brief Create predictive coding hierarchy
 *
 * WHAT: Factory method for complete hierarchy
 * WHY:  Unified management of hierarchical inference
 *
 * @param config Hierarchy configuration
 * @return Hierarchy handle or NULL on failure
 */
pc_hierarchy_t pc_hierarchy_create(const pc_hierarchy_config_t* config);

/**
 * @brief Destroy predictive coding hierarchy
 *
 * WHAT: Free hierarchy resources
 * WHY:  Prevent memory leaks
 *
 * @param hierarchy Hierarchy to destroy
 */
void pc_hierarchy_destroy(pc_hierarchy_t hierarchy);

/**
 * @brief Present input to hierarchy
 *
 * WHAT: Set sensory input at bottom level
 * WHY:  Drive inference from bottom up
 *
 * @param hierarchy Hierarchy handle
 * @param input Sensory input [input_dim]
 */
void pc_hierarchy_set_input(pc_hierarchy_t hierarchy,
                            const float* input);

/**
 * @brief Set prior at top level
 *
 * WHAT: Set top-down prior beliefs
 * WHY:  Incorporate context/expectations
 *
 * @param hierarchy Hierarchy handle
 * @param prior Prior representations [top_level_dim]
 */
void pc_hierarchy_set_prior(pc_hierarchy_t hierarchy,
                            const float* prior);

/**
 * @brief Run one inference step
 *
 * WHAT: Update all levels for one timestep
 * WHY:  Iterative inference via message passing
 *
 * ALGORITHM:
 * 1. Generate top-down predictions
 * 2. Compute prediction errors
 * 3. Update representations
 * 4. Update precisions (optional)
 * 5. Update weights (optional)
 *
 * @param hierarchy Hierarchy handle
 * @param dt Time step (ms)
 * @param learn Enable weight learning
 */
void pc_hierarchy_inference_step(pc_hierarchy_t hierarchy,
                                 float dt,
                                 bool learn);

/**
 * @brief Run inference to convergence
 *
 * WHAT: Iterate until free energy stabilizes
 * WHY:  Find best representation for input
 *
 * @param hierarchy Hierarchy handle
 * @param max_iterations Maximum iterations
 * @param tolerance Convergence threshold
 * @param learn Enable weight learning
 * @return Number of iterations taken
 */
uint32_t pc_hierarchy_inference_converge(pc_hierarchy_t hierarchy,
                                          uint32_t max_iterations,
                                          float tolerance,
                                          bool learn);

/**
 * @brief Get representations at level
 *
 * WHAT: Read inferred representations
 * WHY:  Access internal states
 *
 * @param hierarchy Hierarchy handle
 * @param level Hierarchy level (0 = bottom)
 * @param output Output buffer [units_at_level]
 * @return true on success
 */
bool pc_hierarchy_get_representations(pc_hierarchy_t hierarchy,
                                       uint32_t level,
                                       float* output);

/**
 * @brief Get prediction errors at level
 *
 * WHAT: Read prediction errors
 * WHY:  Errors indicate surprise/novelty
 *
 * @param hierarchy Hierarchy handle
 * @param level Hierarchy level
 * @param output Output buffer
 * @return true on success
 */
bool pc_hierarchy_get_errors(pc_hierarchy_t hierarchy,
                              uint32_t level,
                              float* output);

/**
 * @brief Get total free energy
 *
 * WHAT: Sum of free energy across levels
 * WHY:  Global measure of model quality
 *
 * @param hierarchy Hierarchy handle
 * @return Total free energy
 */
float pc_hierarchy_get_free_energy(pc_hierarchy_t hierarchy);

/**
 * @brief Get hierarchy statistics
 *
 * WHAT: Retrieve monitoring metrics
 * WHY:  Track convergence and learning
 *
 * @param hierarchy Hierarchy handle
 * @param stats Output statistics
 * @return true on success
 */
bool pc_hierarchy_get_stats(pc_hierarchy_t hierarchy,
                            pc_hierarchy_stats_t* stats);

/**
 * @brief Reset hierarchy to initial state
 *
 * WHAT: Reset all representations and errors
 * WHY:  Clear for new input
 *
 * @param hierarchy Hierarchy handle
 */
void pc_hierarchy_reset(pc_hierarchy_t hierarchy);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute KL divergence between distributions
 *
 * WHAT: Calculate KL(q||p) for Gaussian distributions
 * WHY:  Component of variational free energy
 *
 * FORMULA: KL = 0.5 × (tr(Σ_p^{-1} Σ_q) + (μ_p - μ_q)^T Σ_p^{-1} (μ_p - μ_q) - k + ln(|Σ_p|/|Σ_q|))
 *
 * @param mu_q Mean of q [dim]
 * @param precision_q Precision of q [dim]
 * @param mu_p Mean of p (prior) [dim]
 * @param precision_p Precision of p (prior) [dim]
 * @param dim Dimension
 * @return KL divergence
 */
float pc_kl_divergence_gaussian(const float* mu_q,
                                const float* precision_q,
                                const float* mu_p,
                                const float* precision_p,
                                uint32_t dim);

/**
 * @brief Softmax function for precision normalization
 *
 * WHAT: Normalize precisions to sum to 1
 * WHY:  Attention-like precision weighting
 *
 * @param precisions Input precisions [dim]
 * @param output Normalized precisions [dim]
 * @param dim Dimension
 * @param temperature Softmax temperature
 */
void pc_softmax_precision(const float* precisions,
                          float* output,
                          uint32_t dim,
                          float temperature);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_PREDICTIVE_CODING_H
