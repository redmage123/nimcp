//=============================================================================
// nimcp_neuralnet_learning.h - Learning Rules for Neural Networks
//=============================================================================
/**
 * @file nimcp_neuralnet_learning.h
 * @brief Learning algorithms (STDP, Oja's rule, etc.)
 *
 * RESPONSIBILITY: Synaptic plasticity and weight updates
 *
 * This module provides:
 * - STDP (Spike-Timing Dependent Plasticity)
 * - Oja's learning rule for PCA
 * - Hybrid learning rules
 * - Synaptic trace updates
 * - Weight normalization
 */

#ifndef NIMCP_NEURALNET_LEARNING_H
#define NIMCP_NEURALNET_LEARNING_H

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
 * @brief Normalize synaptic weights for a neuron
 *
 * WHY: Prevents unbounded weight growth
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param network Neural network
 * @param neuron_id Neuron whose weights to normalize
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_normalize_weights(neural_network_t network, uint32_t neuron_id);

/**
 * @brief Update synaptic traces for STDP
 *
 * WHY: Decay eligibility traces over time
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param network Neural network
 * @param neuron_id Neuron to update
 * @param timestamp Current network time
 */
NIMCP_EXPORT void neural_network_update_traces(neural_network_t network, uint32_t neuron_id,
                                               uint64_t timestamp);

/**
 * @brief Get weight norm for a neuron
 *
 * @param network Neural network
 * @param neuron_id Neuron ID
 * @return L2 norm of synaptic weights
 */
NIMCP_EXPORT float neural_network_get_weight_norm(neural_network_t network, uint32_t neuron_id);

/**
 * @brief Get extended weight statistics for a neuron
 *
 * @param network Neural network
 * @param neuron_id Neuron ID
 * @param mean Output: mean weight
 * @param std_dev Output: standard deviation
 * @param min_weight Output: minimum weight
 * @param max_weight Output: maximum weight
 */
NIMCP_EXPORT void neural_network_get_weight_statistics_ext(neural_network_t network, uint32_t neuron_id,
                                                            float* mean, float* std_dev,
                                                            float* min_weight, float* max_weight);

/**
 * @brief Apply Oja's learning rule to a neuron
 *
 * WHY: PCA-like unsupervised learning with automatic weight normalization
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param network Neural network
 * @param neuron_id Neuron to update
 * @param timestamp Current network time
 * @return Number of synapses modified
 */
NIMCP_EXPORT uint32_t neural_network_apply_oja(neural_network_t network, uint32_t neuron_id,
                                               uint64_t timestamp);

/**
 * @brief Apply STDP (Spike-Timing Dependent Plasticity) learning rule
 *
 * WHY: Biologically realistic causality-based learning
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param network Neural network
 * @param neuron_id Neuron to update
 * @param timestamp Current network time
 * @return Number of synapses modified
 */
NIMCP_EXPORT uint32_t neural_network_apply_stdp(neural_network_t network, uint32_t neuron_id,
                                                uint64_t timestamp);

/**
 * @brief Apply reward-modulated learning (RL with eligibility traces)
 *
 * WHY: Enable supervised/RL learning using biological plasticity rules
 * COMPLEXITY: O(n * s) where n = neurons, s = synapses
 *
 * @param network Neural network
 * @param reward Reward signal [0, 1]
 * @param learning_rate Learning rate multiplier
 * @param current_time Current network time
 * @return Number of synapses modified
 */
NIMCP_EXPORT uint32_t neural_network_apply_reward_learning(neural_network_t network, float reward,
                                                           float learning_rate, uint64_t current_time);

/**
 * @brief Apply reward-modulated bio-plasticity to ACTIVE neurons only.
 *
 * Like neural_network_apply_reward_learning() but only processes neurons
 * with |state| > activity_threshold. O(active) instead of O(N), making
 * STDP/BCM/eligibility feasible at 1.5M+ neurons.
 *
 * @param activity_threshold Minimum |state| to consider a neuron active (default 0.01)
 */
NIMCP_EXPORT uint32_t neural_network_apply_reward_learning_active(neural_network_t network, float reward,
                                                                   float learning_rate, uint64_t current_time,
                                                                   float activity_threshold);

/**
 * @brief Apply lateral inhibition (winner-take-all) to output layer
 *
 * WHAT: Suppress non-winning output neurons to sharpen classification
 * WHY:  Lateral inhibition creates competition between output neurons,
 *        producing sharper class boundaries and faster convergence
 * HOW:  Find maximum output, suppress others by inhibition_strength
 *
 * BIOLOGICAL BASIS:
 * - Cortical lateral inhibition via GABAergic interneurons
 * - Competitive learning in self-organizing maps
 * - Winner-take-all circuits in basal ganglia
 *
 * @param network Neural network
 * @param output_start First output neuron index
 * @param output_count Number of output neurons
 * @param inhibition_strength How much to suppress losers (0.0-1.0)
 * @return Number of neurons modified
 */
NIMCP_EXPORT uint32_t neural_network_apply_lateral_inhibition(
    neural_network_t network,
    uint32_t output_start,
    uint32_t output_count,
    float inhibition_strength);

/**
 * @brief Synaptogenesis: grow new connections from active neurons to dormant neighbors.
 *
 * WHY: Networks with sparse backbone wiring have millions of dormant neurons
 *      with zero connections. This gradually activates them by sprouting
 *      connections from active neighbors, expanding network capacity over time.
 *
 * @param network Neural network
 * @param max_new_connections Maximum connections to create per call
 * @param activity_threshold Minimum |state| to consider a neuron active
 * @return Number of new connections created
 */
NIMCP_EXPORT uint32_t neural_network_sprout_connections(neural_network_t network,
                                                        uint32_t max_new_connections,
                                                        float activity_threshold);

/**
 * @brief Apply generalized Oja's rule
 *
 * @param network Neural network
 * @param neuron_id Neuron to update
 * @param timestamp Current network time
 * @return Number of synapses modified
 */
NIMCP_EXPORT uint32_t neural_network_apply_generalized_oja(neural_network_t network, uint32_t neuron_id,
                                                           uint64_t timestamp);

/**
 * @brief Update plasticity mechanisms for a neuron
 *
 * @param network Neural network
 * @param neuron_id Neuron to update
 * @param timestamp Current network time
 * @return Number of updates applied
 */
NIMCP_EXPORT uint32_t neural_network_update_plasticity(neural_network_t network, uint32_t neuron_id,
                                                       uint64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURALNET_LEARNING_H
