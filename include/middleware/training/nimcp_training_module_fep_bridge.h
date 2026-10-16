/**
 * @file nimcp_training_module_fep_bridge.h
 * @brief Free Energy Principle bridge for Training Module
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional FEP integration for training module
 * WHY:  Model training as generative model optimization under FEP
 * HOW:  Training parameters → FEP beliefs, training dynamics → prediction errors
 *
 * BIOLOGICAL BASIS:
 * ==============================================================================
 * Training as Generative Model Optimization:
 * - Weight updates = belief updates that minimize variational free energy
 * - Loss function = prediction error (sensory PE)
 * - Optimizer = belief update rule (gradient descent on free energy)
 * - Learning rate = precision weighting on prediction errors
 * - Regularization = complexity cost (KL divergence from prior)
 *
 * FEP FORMULATION OF TRAINING:
 * - Training loss L = Free energy F = Complexity + Inaccuracy
 * - Complexity = KL[q(w)||p(w)] = Regularization penalty
 * - Inaccuracy = E[-log p(y|x,w)] = Reconstruction error
 * - Gradient = ∂F/∂w = Prediction error gradient
 * - Weight update = μ' = μ - lr * ∂F/∂μ
 *
 * INTEGRATION DIRECTIONS:
 * 1. Training → FEP:
 *    - Training loss → Observation
 *    - Gradient norm → Prediction error
 *    - Weight state → Hidden state beliefs
 *    - Learning dynamics → State transitions
 *
 * 2. FEP → Training:
 *    - Expected free energy → Learning rate modulation
 *    - Belief uncertainty → Exploration vs exploitation
 *    - Surprise → Early stopping signal
 *    - Complexity cost → Regularization strength
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRAINING_MODULE_FEP_BRIDGE_H
#define NIMCP_TRAINING_MODULE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "middleware/training/nimcp_training_module.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Training module FEP bridge configuration
 */
typedef struct {
    /* FEP parameters */
    float belief_learning_rate;          /**< Belief update rate (0.01-0.5) */
    float precision_learning_rate;        /**< Precision learning rate (0.001-0.1) */
    float initial_precision;              /**< Initial precision weight (0.1-10.0) */
    bool learn_precision;                 /**< Adapt precision based on errors */

    /* Prediction error thresholds */
    float surprise_threshold;             /**< High surprise threshold (3.0-10.0) */
    float convergence_threshold;          /**< Convergence threshold (0.001-0.1) */

    /* Modulation settings */
    bool modulate_learning_rate;          /**< Use FEP to modulate LR */
    float lr_modulation_strength;         /**< LR modulation strength (0.0-1.0) */
    bool enable_early_stopping;           /**< Stop on high surprise */

    /* Bio-async integration */
    bool enable_bio_async;                /**< Enable bio-async messaging */
} training_module_fep_config_t;

/* ============================================================================
 * Effects and State
 * ============================================================================ */

/**
 * @brief FEP effects on training module
 */
typedef struct {
    float learning_rate_modulation;       /**< LR scale factor [0.1, 2.0] */
    float regularization_strength;        /**< Regularization weight [0.0, 1.0] */
    float exploration_bonus;              /**< Exploration incentive [0.0, 1.0] */
    bool should_stop_early;               /**< Early stopping signal */
} training_module_fep_effects_t;

/**
 * @brief Training effects on FEP system
 */
typedef struct {
    float training_loss;                  /**< Current training loss */
    float gradient_norm;                  /**< L2 norm of gradients */
    float weight_variance;                /**< Variance of weight updates */
    uint64_t training_steps;              /**< Total training steps */
} fep_training_effects_t;

/**
 * @brief Training-FEP bridge state
 */
typedef struct {
    uint64_t update_count;                /**< Number of updates */
    float avg_free_energy;                /**< Running average free energy */
    float avg_prediction_error;           /**< Average prediction error */
    float max_surprise;                   /**< Maximum surprise seen */
    uint32_t convergence_count;           /**< Consecutive convergent steps */
    uint32_t surprise_count;              /**< High surprise event count */
} training_module_fep_state_t;

/**
 * @brief Training-FEP bridge statistics
 */
typedef struct {
    uint64_t total_updates;               /**< Total FEP updates */
    uint64_t total_modulations;           /**< Total LR modulations */
    uint64_t early_stop_triggers;         /**< Early stop signals */
    float min_free_energy;                /**< Minimum free energy achieved */
    float max_free_energy;                /**< Maximum free energy */
    float avg_belief_change;              /**< Average belief update magnitude */
} training_module_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Training module FEP bridge (opaque)
 */
typedef struct training_module_fep_bridge training_module_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide default parameters for training-FEP integration
 * WHY:  Sensible defaults based on standard training practices
 * HOW:  Return pre-configured struct
 *
 * @param config Output configuration
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on error
 */
int training_module_fep_default_config(training_module_fep_config_t* config);

/**
 * @brief Create training module FEP bridge
 *
 * WHAT: Initialize FEP integration for training module
 * WHY:  Enable generative model interpretation of training
 * HOW:  Create FEP system, allocate bridge state, connect module
 *
 * @param config Bridge configuration
 * @param training_module Training module to integrate
 * @param fep_system FEP system (created if NULL)
 * @return Bridge handle or NULL on failure
 */
training_module_fep_bridge_t* training_module_fep_create(
    const training_module_fep_config_t* config,
    nimcp_training_context_t* training_module,
    fep_system_t* fep_system
);

/**
 * @brief Destroy training module FEP bridge
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void training_module_fep_destroy(training_module_fep_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Update FEP system with training observations
 *
 * WHAT: Process training step results through FEP
 * WHY:  Compute prediction errors and free energy
 * HOW:  Map loss/gradients to observations, update beliefs
 *
 * @param bridge FEP bridge
 * @param loss Training loss value
 * @param gradient_norm Gradient L2 norm
 * @return 0 on success, negative on error
 */
int training_module_fep_update(
    training_module_fep_bridge_t* bridge,
    float loss,
    float gradient_norm
);

/**
 * @brief Compute FEP-based modulation effects
 *
 * WHAT: Calculate FEP effects on training parameters
 * WHY:  Use free energy to guide learning process
 * HOW:  Compute learning rate modulation from surprise/precision
 *
 * @param bridge FEP bridge
 * @return 0 on success, negative on error
 */
int training_module_fep_compute_effects(training_module_fep_bridge_t* bridge);

/**
 * @brief Apply FEP effects to training module
 *
 * WHAT: Modify training parameters based on FEP state
 * WHY:  Implement FEP-guided training adaptation
 * HOW:  Apply learning rate modulation, early stopping signals
 *
 * @param bridge FEP bridge
 * @return 0 on success, negative on error
 */
int training_module_fep_apply_effects(training_module_fep_bridge_t* bridge);

/**
 * @brief Bidirectional update (training → FEP → training)
 *
 * WHAT: Full update cycle with observation and modulation
 * WHY:  Convenient one-call interface
 * HOW:  Update FEP, compute effects, apply modulation
 *
 * @param bridge FEP bridge
 * @param loss Training loss
 * @param gradient_norm Gradient norm
 * @return 0 on success, negative on error
 */
int training_module_fep_step(
    training_module_fep_bridge_t* bridge,
    float loss,
    float gradient_norm
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP effects on training
 *
 * @param bridge FEP bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int training_module_fep_get_effects(
    const training_module_fep_bridge_t* bridge,
    training_module_fep_effects_t* effects
);

/**
 * @brief Get current free energy
 *
 * @param bridge FEP bridge
 * @return Current variational free energy
 */
float training_module_fep_get_free_energy(const training_module_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge FEP bridge
 * @return Current prediction error magnitude
 */
float training_module_fep_get_prediction_error(const training_module_fep_bridge_t* bridge);

/**
 * @brief Check if early stopping is recommended
 *
 * @param bridge FEP bridge
 * @return true if should stop, false otherwise
 */
bool training_module_fep_should_stop(const training_module_fep_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int training_module_fep_get_stats(
    const training_module_fep_bridge_t* bridge,
    training_module_fep_stats_t* stats
);

/**
 * @brief Get bridge state
 *
 * @param bridge FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int training_module_fep_get_state(
    const training_module_fep_bridge_t* bridge,
    training_module_fep_state_t* state
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging system
 * WHY:  Enable inter-module FEP communication
 * HOW:  Register as BIO_MODULE_FEP_TRAINING_MODULE
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int training_module_fep_connect_bio_async(training_module_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int training_module_fep_disconnect_bio_async(training_module_fep_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge FEP bridge
 * @return true if connected, false otherwise
 */
bool training_module_fep_is_bio_async_connected(const training_module_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_MODULE_FEP_BRIDGE_H */
