/**
 * @file nimcp_physics_nn.h
 * @brief Physics-informed neural network for dynamics prediction
 *
 * WHAT: Neural network for learning and predicting physical dynamics
 * WHY:  Enable accurate physics simulation with learned dynamics
 * HOW:  Full backpropagation, configurable architecture, Hamiltonian constraints
 *
 * BIOLOGICAL BASIS:
 * The parietal cortex develops intuitive physics models through experience.
 * This module provides a learnable physics model that can:
 * - Learn dynamics from observed trajectories
 * - Predict future states using RK4/symplectic integration
 * - Enforce energy conservation (Hamiltonian constraint)
 *
 * KEY FEATURES:
 * - Full backpropagation through all layers (not just last layer)
 * - Configurable architecture (variable depth, layer sizes)
 * - Multiple integration methods (Euler, RK4, Symplectic)
 * - Hamiltonian/Lagrangian constraints for energy conservation
 * - Softplus activation for smooth gradients
 *
 * USAGE:
 * ```c
 * physics_nn_config_t config = physics_nn_default_config();
 * config.state_dim = 4;  // 2 positions + 2 momenta
 * config.integrator = PHYSICS_NN_INTEGRATOR_RK4;
 * config.use_hamiltonian = true;
 *
 * physics_nn_t* nn = physics_nn_create(&config);
 *
 * // Train on observed data
 * physics_nn_train_step(nn, state, target_derivative);
 *
 * // Predict trajectory
 * physics_nn_predict(nn, initial_state, dt, num_steps, trajectory);
 *
 * physics_nn_destroy(nn);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 2.0.0
 */

#ifndef NIMCP_PHYSICS_NN_H
#define NIMCP_PHYSICS_NN_H

#include "utils/validation/nimcp_common.h"
#include "utils/numerical/nimcp_integration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum number of layers in physics NN */
#define PHYSICS_NN_MAX_LAYERS           16

/** Maximum neurons per layer */
#define PHYSICS_NN_MAX_NEURONS          1024

/** Default hidden layer size */
#define PHYSICS_NN_DEFAULT_HIDDEN       256

/** Default number of layers */
#define PHYSICS_NN_DEFAULT_NUM_LAYERS   3

/** Default learning rate */
#define PHYSICS_NN_DEFAULT_LR           0.001f

/** Default state dimension */
#define PHYSICS_NN_DEFAULT_STATE_DIM    8

/** Bio-async module ID */
#define BIO_MODULE_PHYSICS_NN           0x038C

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for physics neural network */
typedef struct physics_nn physics_nn_t;

/**
 * @brief Integration method for physics NN
 */
typedef enum {
    PHYSICS_NN_INTEGRATOR_EULER,        /**< Explicit Euler (fast, less accurate) */
    PHYSICS_NN_INTEGRATOR_RK4,          /**< Runge-Kutta 4 (accurate, 4x slower) */
    PHYSICS_NN_INTEGRATOR_SYMPLECTIC,   /**< Symplectic (preserves energy) */
    PHYSICS_NN_INTEGRATOR_ADAPTIVE      /**< Adaptive timestep */
} physics_nn_integrator_t;

/**
 * @brief Activation function type
 */
typedef enum {
    PHYSICS_NN_ACTIVATION_SOFTPLUS,     /**< log(1 + exp(x)) - smooth, default */
    PHYSICS_NN_ACTIVATION_TANH,         /**< Hyperbolic tangent */
    PHYSICS_NN_ACTIVATION_RELU,         /**< ReLU (may cause issues in backprop) */
    PHYSICS_NN_ACTIVATION_SWISH         /**< x * sigmoid(x) - smooth like softplus */
} physics_nn_activation_t;

/**
 * @brief Optimizer type for training
 */
typedef enum {
    PHYSICS_NN_OPTIMIZER_SGD,           /**< Stochastic gradient descent */
    PHYSICS_NN_OPTIMIZER_SGD_MOMENTUM,  /**< SGD with momentum */
    PHYSICS_NN_OPTIMIZER_ADAM           /**< Adam optimizer (default) */
} physics_nn_optimizer_t;

/**
 * @brief Configuration for physics NN
 */
typedef struct {
    /* Architecture */
    uint32_t state_dim;                 /**< State vector dimension (default: 8) */
    uint32_t num_layers;                /**< Number of layers including output (default: 3) */
    uint32_t* layer_sizes;              /**< Size of each hidden layer (NULL = all same) */
    uint32_t hidden_size;               /**< Default hidden layer size (default: 256) */

    /* Activation and training */
    physics_nn_activation_t activation; /**< Activation function (default: softplus) */
    physics_nn_optimizer_t optimizer;   /**< Optimizer type (default: Adam) */
    float learning_rate;                /**< Learning rate (default: 0.001) */
    float momentum;                     /**< Momentum for SGD (default: 0.9) */
    float beta1;                        /**< Adam beta1 (default: 0.9) */
    float beta2;                        /**< Adam beta2 (default: 0.999) */
    float epsilon;                      /**< Adam epsilon (default: 1e-8) */
    float weight_decay;                 /**< L2 regularization (default: 0.0) */
    float gradient_clip;                /**< Max gradient norm, 0=disabled (default: 1.0) */

    /* Physics constraints */
    bool use_hamiltonian;               /**< Enforce Hamiltonian conservation (default: true) */
    bool use_lagrangian;                /**< Use Lagrangian formulation (default: false) */
    float hamiltonian_weight;           /**< Weight for Hamiltonian loss term (default: 0.1) */

    /* Integration */
    physics_nn_integrator_t integrator; /**< Integration method (default: RK4) */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging (default: false) */

    /* Modulation */
    float inflammation_sensitivity;     /**< How inflammation affects precision (default: 0.5) */
    float fatigue_sensitivity;          /**< How fatigue affects precision (default: 0.5) */
} physics_nn_config_t;

/**
 * @brief Training result from single step
 */
typedef struct {
    float loss;                         /**< Total loss */
    float mse_loss;                     /**< MSE component of loss */
    float hamiltonian_loss;             /**< Hamiltonian conservation loss */
    float gradient_norm;                /**< L2 norm of gradients */
    bool gradient_clipped;              /**< Whether gradients were clipped */
} physics_nn_train_result_t;

/**
 * @brief Prediction result
 */
typedef struct {
    float** trajectory;                 /**< Predicted trajectory [num_steps][state_dim] */
    uint32_t num_steps;                 /**< Number of steps predicted */
    float* hamiltonians;                /**< Hamiltonian at each step (if computed) */
    float energy_drift;                 /**< Total energy drift (H_final - H_initial) */
    float confidence;                   /**< Confidence in prediction [0,1] */
} physics_nn_prediction_t;

/**
 * @brief Statistics for physics NN
 */
typedef struct {
    uint64_t training_steps;            /**< Total training steps */
    uint64_t prediction_steps;          /**< Total prediction steps */
    float total_loss;                   /**< Cumulative training loss */
    float avg_loss;                     /**< Average loss */
    float avg_hamiltonian_drift;        /**< Average energy drift */
    float min_loss;                     /**< Minimum loss achieved */
    float max_gradient_norm;            /**< Maximum gradient norm seen */
    uint64_t gradient_clips;            /**< Number of gradient clips */
    float current_learning_rate;        /**< Current (potentially adapted) LR */
} physics_nn_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default physics NN configuration
 * @return Default configuration
 */
physics_nn_config_t physics_nn_default_config(void);

/**
 * @brief Validate physics NN configuration
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool physics_nn_validate_config(const physics_nn_config_t* config);

/**
 * @brief Create physics NN with default configuration
 * @return Physics NN handle, or NULL on failure
 */
physics_nn_t* physics_nn_create(void);

/**
 * @brief Create physics NN with custom configuration
 * @param config Configuration (NULL for defaults)
 * @return Physics NN handle, or NULL on failure
 */
physics_nn_t* physics_nn_create_custom(const physics_nn_config_t* config);

/**
 * @brief Destroy physics NN and free resources
 * @param nn Physics NN handle
 */
void physics_nn_destroy(physics_nn_t* nn);

/**
 * @brief Reset physics NN weights to random initialization
 * @param nn Physics NN handle
 * @return 0 on success, -1 on error
 */
int physics_nn_reset(physics_nn_t* nn);

/* ============================================================================
 * FORWARD PASS API
 * ============================================================================ */

/**
 * @brief Compute derivative prediction (forward pass)
 *
 * Given current state y, predicts dy/dt = f(y)
 *
 * @param nn Physics NN handle
 * @param state Current state vector [state_dim]
 * @param derivative Output derivative vector [state_dim]
 * @return 0 on success, -1 on error
 */
int physics_nn_forward(physics_nn_t* nn, const float* state, float* derivative);

/**
 * @brief Compute Hamiltonian for given state
 *
 * For state [q1...qn, p1...pn], computes H = T(p) + V(q)
 *
 * @param nn Physics NN handle
 * @param state State vector [state_dim]
 * @return Hamiltonian value, or NaN on error
 */
float physics_nn_compute_hamiltonian(physics_nn_t* nn, const float* state);

/* ============================================================================
 * TRAINING API
 * ============================================================================ */

/**
 * @brief Perform single training step with full backpropagation
 *
 * Trains the network to predict target_derivative from state.
 * Uses full backpropagation through all layers.
 *
 * @param nn Physics NN handle
 * @param state Input state vector [state_dim]
 * @param target_derivative Target dy/dt [state_dim]
 * @param result Optional output training result (can be NULL)
 * @return 0 on success, -1 on error
 */
int physics_nn_train_step(
    physics_nn_t* nn,
    const float* state,
    const float* target_derivative,
    physics_nn_train_result_t* result
);

/**
 * @brief Train on batch of state-derivative pairs
 *
 * @param nn Physics NN handle
 * @param states Batch of states [batch_size][state_dim]
 * @param derivatives Batch of derivatives [batch_size][state_dim]
 * @param batch_size Number of samples
 * @param result Optional aggregate result
 * @return 0 on success, -1 on error
 */
int physics_nn_train_batch(
    physics_nn_t* nn,
    const float** states,
    const float** derivatives,
    uint32_t batch_size,
    physics_nn_train_result_t* result
);

/**
 * @brief Train from observed trajectory
 *
 * Extracts state-derivative pairs from trajectory and trains.
 *
 * @param nn Physics NN handle
 * @param trajectory Observed trajectory [num_points][state_dim]
 * @param num_points Number of trajectory points
 * @param dt Time step between points
 * @param result Optional aggregate result
 * @return 0 on success, -1 on error
 */
int physics_nn_train_from_trajectory(
    physics_nn_t* nn,
    const float** trajectory,
    uint32_t num_points,
    float dt,
    physics_nn_train_result_t* result
);

/* ============================================================================
 * PREDICTION API
 * ============================================================================ */

/**
 * @brief Predict future trajectory using configured integrator
 *
 * @param nn Physics NN handle
 * @param initial_state Initial state [state_dim]
 * @param dt Time step
 * @param num_steps Number of steps to predict
 * @param prediction Output prediction result (caller must free trajectory)
 * @return 0 on success, -1 on error
 */
int physics_nn_predict(
    physics_nn_t* nn,
    const float* initial_state,
    float dt,
    uint32_t num_steps,
    physics_nn_prediction_t* prediction
);

/**
 * @brief Predict single step using configured integrator
 *
 * @param nn Physics NN handle
 * @param state Current state [state_dim] (modified in place)
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
int physics_nn_step(physics_nn_t* nn, float* state, float dt);

/**
 * @brief Free prediction result resources
 *
 * @param prediction Prediction to free
 */
void physics_nn_free_prediction(physics_nn_prediction_t* prediction);

/* ============================================================================
 * SYMPLECTIC INTEGRATOR API
 * ============================================================================ */

/**
 * @brief Symplectic Euler step (1st order, energy-preserving)
 *
 * For Hamiltonian systems: dq/dt = dH/dp, dp/dt = -dH/dq
 *
 * @param nn Physics NN handle
 * @param q Position coordinates [state_dim/2]
 * @param p Momentum coordinates [state_dim/2]
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
int physics_nn_symplectic_euler(physics_nn_t* nn, float* q, float* p, float dt);

/**
 * @brief Leapfrog/Stormer-Verlet step (2nd order, energy-preserving)
 *
 * @param nn Physics NN handle
 * @param q Position coordinates [state_dim/2]
 * @param p Momentum coordinates [state_dim/2]
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
int physics_nn_leapfrog(physics_nn_t* nn, float* q, float* p, float dt);

/**
 * @brief Velocity Verlet step (2nd order, energy-preserving)
 *
 * More accurate than leapfrog for position-dependent forces.
 *
 * @param nn Physics NN handle
 * @param q Position coordinates [state_dim/2]
 * @param p Momentum coordinates [state_dim/2]
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
int physics_nn_velocity_verlet(physics_nn_t* nn, float* q, float* p, float dt);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level (affects precision)
 *
 * @param nn Physics NN handle
 * @param level Inflammation level [0,1]
 * @return 0 on success, -1 on error
 */
int physics_nn_set_inflammation(physics_nn_t* nn, float level);

/**
 * @brief Set fatigue level (affects precision)
 *
 * @param nn Physics NN handle
 * @param level Fatigue level [0,1]
 * @return 0 on success, -1 on error
 */
int physics_nn_set_fatigue(physics_nn_t* nn, float level);

/**
 * @brief Set learning rate (runtime adjustment)
 *
 * @param nn Physics NN handle
 * @param lr New learning rate
 * @return 0 on success, -1 on error
 */
int physics_nn_set_learning_rate(physics_nn_t* nn, float lr);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get physics NN statistics
 *
 * @param nn Physics NN handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int physics_nn_get_stats(const physics_nn_t* nn, physics_nn_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param nn Physics NN handle
 */
void physics_nn_reset_stats(physics_nn_t* nn);

/**
 * @brief Get last error message
 *
 * @return Error message (static, do not free)
 */
const char* physics_nn_get_last_error(void);

/* ============================================================================
 * SERIALIZATION API
 * ============================================================================ */

/**
 * @brief Save physics NN weights to file
 *
 * @param nn Physics NN handle
 * @param filename Output filename
 * @return 0 on success, -1 on error
 */
int physics_nn_save(const physics_nn_t* nn, const char* filename);

/**
 * @brief Load physics NN weights from file
 *
 * @param nn Physics NN handle
 * @param filename Input filename
 * @return 0 on success, -1 on error
 */
int physics_nn_load(physics_nn_t* nn, const char* filename);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_NN_H */
