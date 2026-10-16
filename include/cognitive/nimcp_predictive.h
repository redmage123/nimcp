/**
 * @file nimcp_predictive.h
 * @brief Phase 10.9: Predictive Processing - Hierarchical Predictive Coding
 *
 * WHAT: Free Energy Principle and Active Inference framework
 * WHY:  Enable predictive coding, surprise minimization, and action selection
 * HOW:  Hierarchical layers with top-down predictions and bottom-up errors
 *
 * THEORY:
 * Based on Karl Friston's Free Energy Principle:
 * - Brain minimizes prediction error (surprise)
 * - Top-down: Generate predictions from internal models
 * - Bottom-up: Send prediction errors up the hierarchy
 * - Learning: Update models to minimize free energy
 * - Action: Select actions that minimize expected free energy
 *
 * ARCHITECTURE:
 * Layer N+1: P(x_{n+1})   ← Higher-level model
 *            ↓ prediction
 * Layer N:   P(x_n | x_{n+1}) + error(x_n - prediction)
 *            ↓ prediction
 * Layer N-1: ...
 *
 * FREE ENERGY: F = ∑_layers precision * ||error||²
 *
 * @author NIMCP Phase 10 Team
 * @date 2025-11-09
 */

#ifndef NIMCP_PREDICTIVE_H
#define NIMCP_PREDICTIVE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Named Constants (NIMCP Coding Standards)
//=============================================================================

#define PRED_MAX_LAYERS 10               /**< Maximum hierarchy depth */
#define PRED_DEFAULT_LAYERS 4            /**< Default hierarchy depth */
#define PRED_DEFAULT_PRECISION 1.0f      /**< Default confidence */
#define PRED_MIN_PRECISION 0.01f         /**< Minimum precision (avoid div/0) */
#define PRED_MAX_PRECISION 100.0f        /**< Maximum precision */
#define PRED_LEARNING_RATE 0.01f         /**< Prediction learning rate */
#define PRED_PRECISION_LR 0.001f         /**< Precision learning rate */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Opaque handle to predictive network
 *
 * WHAT: Complete hierarchical predictive coding system
 * WHY:  Encapsulation (hide implementation details)
 * HOW:  Opaque pointer pattern
 */
typedef struct predictive_network_s* predictive_network_t;

/**
 * @brief Predictive layer state
 *
 * WHAT: Single layer in predictive hierarchy
 * WHY:  Track predictions, errors, and precision
 * HOW:  Separate vectors for each quantity
 */
typedef struct {
    float* state;             /**< Current layer state [size] */
    float* prediction;        /**< Top-down prediction [size] */
    float* prediction_error;  /**< Bottom-up error [size] */
    float* precision;         /**< Confidence weights [size] */
    uint32_t size;            /**< Layer dimensionality */
    float free_energy;        /**< Layer's contribution to F */
} predictive_layer_t;

/**
 * @brief Predictive network configuration
 *
 * WHAT: Hyperparameters for predictive processing
 * WHY:  Flexible configuration for different scenarios
 * HOW:  Struct with learning rates and layer architecture
 */
typedef struct {
    uint32_t num_layers;              /**< Hierarchy depth */
    uint32_t* layer_sizes;            /**< Dimensions [num_layers] */
    float learning_rate;              /**< Prediction update rate */
    float precision_learning_rate;    /**< Confidence update rate */
    float initial_precision;          /**< Starting confidence */
    bool enable_active_inference;     /**< Enable action selection */
    bool enable_precision_learning;   /**< Learn precisions */
} predictive_config_t;

/**
 * @brief Action candidate for active inference
 *
 * WHAT: Possible action with expected free energy
 * WHY:  Select actions that minimize surprise
 * HOW:  Evaluate expected outcomes
 */
typedef struct {
    uint32_t action_id;        /**< Action identifier */
    char action_name[64];      /**< Human-readable name */
    float expected_free_energy; /**< Predicted surprise */
    float* predicted_state;    /**< Expected outcome [state_dim] */
    uint32_t state_dim;        /**< State dimensionality */
} predictive_action_t;

/**
 * @brief Predictive processing statistics
 *
 * WHAT: Performance metrics for monitoring
 * WHY:  Track convergence and surprise levels
 * HOW:  Aggregate statistics across inference steps
 */
typedef struct {
    float total_free_energy;        /**< Sum of all layer errors */
    float average_precision;        /**< Mean confidence */
    float max_prediction_error;     /**< Largest surprise */
    uint32_t num_updates;           /**< Inference iterations */
    uint64_t total_inference_time_us; /**< Total time spent */
} predictive_stats_t;

//=============================================================================
// Core API: Creation & Destruction
//=============================================================================

/**
 * @brief Get default predictive configuration
 *
 * WHAT: Return sensible defaults for predictive processing
 * WHY:  Simplify initialization
 * HOW:  Struct with proven hyperparameters
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 */
predictive_config_t predictive_default_config(void);

/**
 * @brief Create predictive network
 *
 * WHAT: Initialize hierarchical predictive coding system
 * WHY:  Enable free energy minimization
 * HOW:  Allocate layers with predictions and errors
 *
 * @param config Configuration (NULL for defaults)
 * @return Network handle or NULL on error
 *
 * COMPLEXITY: O(sum(layer_sizes))
 * MEMORY: O(sum(layer_sizes))
 */
predictive_network_t predictive_create(const predictive_config_t* config);

/**
 * @brief Destroy predictive network
 *
 * WHAT: Free all memory associated with network
 * WHY:  Prevent memory leaks
 * HOW:  Free each layer and the network structure
 *
 * @param net Network to destroy
 *
 * COMPLEXITY: O(num_layers)
 */
void predictive_destroy(predictive_network_t net);

//=============================================================================
// Inference API
//=============================================================================

/**
 * @brief Forward pass: Minimize free energy
 *
 * WHAT: Update predictions to minimize prediction error
 * WHY:  Core of predictive processing
 * HOW:  Iterative message passing between layers
 *
 * ALGORITHM:
 * 1. Bottom-up: Compute prediction errors
 * 2. Top-down: Generate predictions
 * 3. Update states to minimize precision-weighted error
 * 4. Repeat until convergence or max iterations
 *
 * @param net Predictive network
 * @param input Bottom layer input [layer_sizes[0]]
 * @param num_iterations Number of inference steps (default: 10)
 * @return Total free energy (lower = better)
 *
 * COMPLEXITY: O(num_iterations * sum(layer_sizes))
 */
float predictive_forward(predictive_network_t net, const float* input,
                        uint32_t num_iterations);

/**
 * @brief Get layer prediction
 *
 * WHAT: Extract prediction from specific layer
 * WHY:  Access internal representations
 * HOW:  Return prediction vector
 *
 * @param net Predictive network
 * @param layer_index Layer to query
 * @param output Output buffer [layer_size]
 * @return true on success
 *
 * COMPLEXITY: O(layer_size)
 */
bool predictive_get_layer_prediction(predictive_network_t net,
                                     uint32_t layer_index, float* output);

/**
 * @brief Get layer prediction error
 *
 * WHAT: Extract prediction error (surprise) from layer
 * WHY:  Measure mismatch between prediction and reality
 * HOW:  Return error vector
 *
 * @param net Predictive network
 * @param layer_index Layer to query
 * @param output Output buffer [layer_size]
 * @return true on success
 *
 * COMPLEXITY: O(layer_size)
 */
bool predictive_get_layer_error(predictive_network_t net,
                                uint32_t layer_index, float* output);

//=============================================================================
// Learning API
//=============================================================================

/**
 * @brief Update internal model (learning)
 *
 * WHAT: Adjust predictions to reduce future errors
 * WHY:  Learn better generative models
 * HOW:  Gradient descent on free energy
 *
 * @param net Predictive network
 * @return true on success
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
bool predictive_update_model(predictive_network_t net);

/**
 * @brief Update precision weights (attention)
 *
 * WHAT: Adjust confidence based on prediction reliability
 * WHY:  Allocate attention to informative signals
 * HOW:  Track prediction error variance
 *
 * @param net Predictive network
 * @param layer_index Layer to update
 * @return true on success
 *
 * COMPLEXITY: O(layer_size)
 */
bool predictive_update_precision(predictive_network_t net, uint32_t layer_index);

//=============================================================================
// Active Inference API
//=============================================================================

/**
 * @brief Select action via active inference
 *
 * WHAT: Choose action that minimizes expected free energy
 * WHY:  Goal-directed behavior under uncertainty
 * HOW:  Evaluate each action's predicted surprise
 *
 * ALGORITHM:
 * For each action:
 *   1. Simulate outcome (forward model)
 *   2. Compute expected prediction error
 *   3. Compute expected ambiguity
 *   4. Expected Free Energy = error + ambiguity
 * Return action with lowest EFE
 *
 * @param net Predictive network
 * @param actions Array of action candidates
 * @param num_actions Number of actions
 * @param selected_action Output: best action index
 * @return Expected free energy of selected action
 *
 * COMPLEXITY: O(num_actions * sum(layer_sizes))
 */
float predictive_active_inference(predictive_network_t net,
                                 predictive_action_t* actions,
                                 uint32_t num_actions,
                                 uint32_t* selected_action);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get network statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor convergence and surprise
 * HOW:  Return statistics structure
 *
 * @param net Predictive network
 * @param stats Output statistics
 * @return true on success
 *
 * COMPLEXITY: O(num_layers)
 */
bool predictive_get_statistics(predictive_network_t net, predictive_stats_t* stats);

/**
 * @brief Print network state (debug)
 *
 * WHAT: Human-readable network dump
 * WHY:  Debugging and visualization
 * HOW:  Print all layers and metrics
 *
 * @param net Predictive network
 *
 * COMPLEXITY: O(num_layers)
 */
void predictive_print_state(predictive_network_t net);

/**
 * @brief Reset network state
 *
 * WHAT: Clear all predictions and errors
 * WHY:  Start fresh inference
 * HOW:  Zero out all layer states
 *
 * @param net Predictive network
 * @return true on success
 *
 * COMPLEXITY: O(sum(layer_sizes))
 */
bool predictive_reset(predictive_network_t net);

/**
 * @brief Get number of layers
 *
 * WHAT: Query network depth
 * WHY:  Programmatic access to architecture
 * HOW:  Return layer count
 *
 * @param net Predictive network
 * @return Number of layers (0 if net is NULL)
 *
 * COMPLEXITY: O(1)
 */
uint32_t predictive_get_num_layers(predictive_network_t net);

/**
 * @brief Get layer size
 *
 * WHAT: Query layer dimensionality
 * WHY:  Programmatic access to architecture
 * HOW:  Return size of specified layer
 *
 * @param net Predictive network
 * @param layer_index Layer to query
 * @return Layer size (0 if invalid)
 *
 * COMPLEXITY: O(1)
 */
uint32_t predictive_get_layer_size(predictive_network_t net, uint32_t layer_index);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PREDICTIVE_H
