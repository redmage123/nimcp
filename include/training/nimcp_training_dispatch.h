/**
 * @file nimcp_training_dispatch.h
 * @brief Training Dispatcher - Routes training to SNN/LNN/CNN/Adaptive trainers
 *
 * WHAT: Unified training dispatch based on network type
 * WHY:  Different network architectures require different training algorithms:
 *       - Adaptive: Standard backpropagation
 *       - SNN: STDP, R-STDP, eProp, surrogate gradients
 *       - LNN: Adjoint method, BPTT for ODE networks
 *       - CNN: Convolutional backpropagation
 * HOW:  Checks brain's network_type and dispatches to appropriate trainer
 *
 * @author NIMCP Team
 * @date 2025-01-16
 */

#ifndef NIMCP_TRAINING_DISPATCH_H
#define NIMCP_TRAINING_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declarations
struct brain_struct;
typedef struct brain_struct* brain_t;

// Include nimcp.h for nimcp_training_config_t
#include "nimcp.h"

//=============================================================================
// Training Dispatch Result
//=============================================================================

/**
 * @brief Result of a training dispatch step
 */
typedef struct {
    float loss;              /**< Computed loss value */
    float learning_rate;     /**< Current learning rate */
    uint32_t step;           /**< Training step number */
    bool early_stopped;      /**< Whether early stopping triggered */
    float gradient_norm;     /**< Gradient norm (if applicable) */

    // Network-type specific results
    union {
        struct {
            uint32_t ltp_events;   /**< Long-term potentiation events */
            uint32_t ltd_events;   /**< Long-term depression events */
            float spike_rate;      /**< Average spike rate */
        } snn;

        struct {
            float ode_error;       /**< ODE integration error */
            float tau_mean;        /**< Mean time constant */
        } lnn;

        struct {
            float conv_grad_norm;  /**< Convolutional gradient norm */
            float dense_grad_norm; /**< Dense layer gradient norm */
        } cnn;
    } type_specific;
} training_dispatch_result_t;

//=============================================================================
// Training Dispatch API
//=============================================================================

/**
 * @brief Initialize training dispatcher for a brain
 *
 * WHAT: Sets up the appropriate training context based on network type
 * WHY:  Each network type needs specialized training infrastructure
 * HOW:  Creates SNN/LNN/CNN training contexts as needed
 *
 * @param brain Brain to initialize training for
 * @param config Training configuration
 * @return 0 on success, negative on error
 */
int training_dispatch_init(brain_t brain, const nimcp_training_config_t* config);

/**
 * @brief Execute one training step with appropriate dispatcher
 *
 * WHAT: Runs forward pass, loss computation, backward pass, weight update
 * WHY:  Unified interface for all network types
 * HOW:  Dispatches to SNN/LNN/CNN/Adaptive trainer based on brain's type
 *
 * @param brain Brain to train
 * @param inputs Input features array
 * @param num_inputs Number of input features
 * @param targets Target values array
 * @param num_targets Number of target values
 * @param result Output training result (may be NULL)
 * @return 0 on success, negative on error
 *
 * DISPATCH LOGIC:
 * - NIMCP_NETWORK_ADAPTIVE: Uses backprop_backward() + optimizer
 * - NIMCP_NETWORK_SNN: Uses snn_training_step() (STDP/eProp/surrogate)
 * - NIMCP_NETWORK_LNN: Uses lnn_training_step() (adjoint/BPTT)
 * - NIMCP_NETWORK_CNN: Uses cnn_trainer_forward/backward/step()
 */
int training_dispatch_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result
);

/**
 * @brief Set reward signal for reward-modulated training (SNN R-STDP)
 *
 * WHAT: Provides dopaminergic reward signal to SNN training
 * WHY:  R-STDP requires reward signal for eligibility trace modulation
 * HOW:  Updates snn_training_ctx reward field
 *
 * @param brain Brain with SNN training
 * @param reward Reward signal (-1 to +1, 0 = neutral)
 * @return 0 on success, -1 if not SNN or no training context
 */
int training_dispatch_set_reward(brain_t brain, float reward);

/**
 * @brief Get training statistics for the current network type
 *
 * @param brain Brain to query
 * @param total_steps Output: total training steps
 * @param total_loss Output: accumulated loss
 * @param current_lr Output: current learning rate
 * @return 0 on success, negative on error
 */
int training_dispatch_get_stats(
    brain_t brain,
    uint64_t* total_steps,
    float* total_loss,
    float* current_lr
);

/**
 * @brief Reset training state for the dispatcher
 *
 * @param brain Brain to reset
 * @return 0 on success, negative on error
 */
int training_dispatch_reset(brain_t brain);

/**
 * @brief Cleanup training dispatcher resources
 *
 * @param brain Brain to cleanup
 */
void training_dispatch_destroy(brain_t brain);

/**
 * @brief Get the network type name as a string
 *
 * @param network_type Network type enum value
 * @return String name (e.g., "SNN", "LNN", "CNN", "ADAPTIVE")
 */
const char* training_dispatch_type_name(uint8_t network_type);

/**
 * @brief Check if a network type is supported for training
 *
 * @param network_type Network type to check
 * @return true if supported, false otherwise
 */
bool training_dispatch_is_supported(uint8_t network_type);

/**
 * @brief Execute one SNN training step (STDP/R-STDP/eProp/surrogate)
 *
 * @param brain Brain with SNN network and training context
 * @param inputs Input features
 * @param num_inputs Number of inputs
 * @param targets Target values
 * @param num_targets Number of targets
 * @param result Output result (may be NULL)
 * @return 0 on success, negative on error
 */
int training_dispatch_snn_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result
);

/**
 * @brief Execute one LNN training step (adjoint ODE)
 *
 * @param brain Brain with LNN network and training context
 * @param inputs Input features
 * @param num_inputs Number of inputs
 * @param targets Target values
 * @param num_targets Number of targets
 * @param result Output result (may be NULL)
 * @return 0 on success, negative on error
 */
int training_dispatch_lnn_step(
    brain_t brain,
    const float* inputs,
    uint32_t num_inputs,
    const float* targets,
    uint32_t num_targets,
    training_dispatch_result_t* result
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_DISPATCH_H */
