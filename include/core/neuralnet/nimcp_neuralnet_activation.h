//=============================================================================
// nimcp_neuralnet_activation.h - Activation Functions for Neural Networks
//=============================================================================
/**
 * @file nimcp_neuralnet_activation.h
 * @brief Activation function strategies for neural networks
 *
 * RESPONSIBILITY: Activation function computation and dispatch
 * PATTERN: Strategy Pattern - function pointer dispatch table
 *
 * This module provides:
 * - Sigmoid, tanh, ReLU, Leaky ReLU activation functions
 * - Adaptive threshold activation for spiking neurons
 * - Function pointer dispatch for O(1) activation
 */

#ifndef NIMCP_NEURALNET_ACTIVATION_H
#define NIMCP_NEURALNET_ACTIVATION_H

#include "core/neuralnet/nimcp_neuralnet.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Compute activation for a neuron
 *
 * PATTERN: Strategy Pattern - dispatches to correct activation function
 * COMPLEXITY: O(1)
 *
 * @param neuron Neuron to activate
 * @param input Raw input value
 * @return Activated output value
 */
NIMCP_EXPORT float neural_network_compute_activation(neuron_t* neuron, float input);

/**
 * @brief Clamp activation to valid range
 *
 * WHY: Prevents numerical overflow and ensures bounded outputs
 * COMPLEXITY: O(1)
 *
 * @param value Value to clamp
 * @return Clamped value in [MIN_ACTIVATION, MAX_ACTIVATION]
 */
NIMCP_EXPORT float neural_network_clamp_activation(float value);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURALNET_ACTIVATION_H
