/**
 * @file nimcp_learning_signal_adapter.h
 * @brief Middleware adapter for learning signal generation
 *
 * WHAT: Connects middleware features to learning signal computation
 * WHY:  Enable learning from neural activity patterns
 * HOW:  Extract features, compute prediction errors, generate learning signals
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#ifndef NIMCP_LEARNING_SIGNAL_ADAPTER_H
#define NIMCP_LEARNING_SIGNAL_ADAPTER_H

#include "middleware/brain_integration.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Learning signal adapter instance
 * WHY:  Manage middleware-to-learning connection
 * HOW:  Maintain prediction tracking and error computation
 */
typedef struct learning_signal_adapter_struct* learning_signal_adapter_t;

/**
 * WHAT: Learning signal type
 * WHY:  Different learning signals for different contexts
 * HOW:  Categorize by computational mechanism
 */
typedef enum {
    LEARNING_SIGNAL_PREDICTION_ERROR,  /**< Supervised learning signal */
    LEARNING_SIGNAL_REWARD,            /**< Reinforcement learning signal */
    LEARNING_SIGNAL_NOVELTY,           /**< Unsupervised novelty signal */
    LEARNING_SIGNAL_SYNCHRONY,         /**< Hebbian synchrony signal */
    LEARNING_SIGNAL_COMBINED           /**< Combined multi-signal */
} learning_signal_type_t;

/**
 * WHAT: Learning signal adapter configuration
 * WHY:  Customize learning signal computation
 * HOW:  Specify signal types, learning rates, decay
 */
typedef struct {
    uint32_t num_channels;           /**< Number of input channels */
    brain_buffer_size_t buffer_size; /**< Temporal buffer size */
    learning_signal_type_t signal_type; /**< Type of learning signal */
    float learning_rate;             /**< Base learning rate [0-1] */
    float decay_rate;                /**< Signal decay rate [0-1] */
    bool enable_eligibility_traces;  /**< Enable temporal credit assignment */
    bool enable_modulation;          /**< Enable neuromodulatory scaling */
} learning_signal_adapter_config_t;

/**
 * WHAT: Learning signal output
 * WHY:  Provide detailed learning information
 * HOW:  Combine error, strength, eligibility
 */
typedef struct {
    float prediction_error;  /**< Prediction error magnitude */
    float reward_signal;     /**< Reward/punishment signal */
    float novelty_signal;    /**< Novelty/surprise signal */
    float synchrony_signal;  /**< Synchrony-based signal */
    float combined_signal;   /**< Combined learning signal */
    float eligibility;       /**< Eligibility trace value [0-1] */
    uint64_t timestamp;      /**< When signal was generated */
} learning_signal_t;

/**
 * WHAT: Create learning signal adapter
 * WHY:  Initialize middleware connection
 * HOW:  Allocate prediction trackers and signal generators
 *
 * @param config Adapter configuration
 * @return Adapter handle or NULL on error
 */
learning_signal_adapter_t learning_signal_adapter_create(
    const learning_signal_adapter_config_t* config
);

/**
 * WHAT: Destroy learning signal adapter
 * WHY:  Clean memory cleanup
 * HOW:  Free all resources
 *
 * @param adapter Adapter to destroy (NULL is safe)
 */
void learning_signal_adapter_destroy(learning_signal_adapter_t adapter);

/**
 * WHAT: Compute learning signal from neural activity
 * WHY:  Generate training signal for synaptic update
 * HOW:  Compare predicted vs actual activity, compute error
 *
 * @param adapter Adapter instance
 * @param activity_predicted Predicted neural activity
 * @param activity_actual Actual neural activity
 * @param num_channels Number of channels
 * @param timestamp Current timestamp
 * @param signal_out Output learning signal
 * @return true on success
 */
bool learning_signal_adapter_compute(
    learning_signal_adapter_t adapter,
    const float* activity_predicted,
    const float* activity_actual,
    uint32_t num_channels,
    uint64_t timestamp,
    learning_signal_t* signal_out
);

/**
 * WHAT: Update eligibility traces
 * WHY:  Maintain temporal credit assignment
 * HOW:  Decay existing traces, add new activity
 *
 * @param adapter Adapter instance
 * @param activity Neural activity vector
 * @param num_channels Number of channels
 * @param dt Time delta (ms)
 * @return true on success
 */
bool learning_signal_adapter_update_eligibility(
    learning_signal_adapter_t adapter,
    const float* activity,
    uint32_t num_channels,
    float dt
);

/**
 * WHAT: Get default learning signal adapter configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 */
learning_signal_adapter_config_t learning_signal_adapter_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_LEARNING_SIGNAL_ADAPTER_H
