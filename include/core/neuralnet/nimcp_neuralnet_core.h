//=============================================================================
// nimcp_neuralnet_core.h - Core Network Lifecycle Functions
//=============================================================================
/**
 * @file nimcp_neuralnet_core.h
 * @brief Network creation, destruction, and state management
 *
 * RESPONSIBILITY: Network lifecycle and neuron state updates
 *
 * This module provides:
 * - Network creation/destruction
 * - Neuron state updates
 * - Connection management
 * - Network statistics
 * - Spike recording
 */

#ifndef NIMCP_NEURALNET_CORE_H
#define NIMCP_NEURALNET_CORE_H

#include "core/neuralnet/nimcp_neuralnet.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Network Lifecycle
//=============================================================================

/**
 * @brief Create a new neural network
 *
 * PATTERN: Factory Pattern
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param config Network configuration
 * @return Network handle or NULL on failure
 */
NIMCP_EXPORT neural_network_t neural_network_create(const network_config_t* config);

/**
 * @brief Destroy neural network and free resources
 *
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param network Network to destroy
 */
NIMCP_EXPORT void neural_network_destroy(neural_network_t network);

/**
 * @brief Reset network to initial state
 *
 * @param network Network to reset
 */
NIMCP_EXPORT void neural_network_reset(neural_network_t network);

//=============================================================================
// Neuron State Updates
//=============================================================================

/**
 * @brief Update a single neuron's state
 *
 * WHY: Core computational unit - integrates inputs, applies learning
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param network Neural network
 * @param neuron_id Neuron to update
 * @param input_current External input current
 * @param timestamp Current network time
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_update_neuron(neural_network_t network, uint32_t neuron_id,
                                               float input_current, uint64_t timestamp);

/**
 * @brief Forward pass through network
 *
 * @param network Neural network
 * @param inputs Input values
 * @param input_size Size of input array
 * @param outputs Output buffer
 * @param output_size Size of output buffer
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_forward(neural_network_t network, const float* inputs,
                                         uint32_t input_size, float* outputs, uint32_t output_size);

//=============================================================================
// Connection Management
//=============================================================================

/**
 * @brief Add connection between neurons
 *
 * @param network Neural network
 * @param from_id Source neuron ID
 * @param to_id Target neuron ID
 * @param weight Initial weight
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_add_connection(neural_network_t network, uint32_t from_id,
                                                uint32_t to_id, float weight);

/**
 * @brief Add typed connection with synapse type
 *
 * @param network Neural network
 * @param from_id Source neuron ID
 * @param to_id Target neuron ID
 * @param weight Initial weight
 * @param type Synapse type (AMPA, NMDA, etc.)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_add_connection_typed(neural_network_t network, uint32_t from_id,
                                                      uint32_t to_id, float weight,
                                                      synapse_type_t type);

//=============================================================================
// State Queries
//=============================================================================

/**
 * @brief Get neuron state
 *
 * @param network Neural network
 * @param neuron_id Neuron ID
 * @param state Output: neuron state
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_get_neuron_state(neural_network_t network, uint32_t neuron_id,
                                                  float* state);

/**
 * @brief Get pointer to neuron structure
 *
 * @param network Neural network
 * @param neuron_id Neuron ID
 * @return Pointer to neuron or NULL
 */
NIMCP_EXPORT neuron_t* neural_network_get_neuron(neural_network_t network, uint32_t neuron_id);

/**
 * @brief Get network statistics
 *
 * @param network Neural network
 * @param stats Output: network statistics
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_get_stats(neural_network_t network, network_stats_t* stats);

/**
 * @brief Get average activity for a neuron
 *
 * @param network Neural network
 * @param neuron_id Neuron ID
 * @return Average activity level
 */
NIMCP_EXPORT float neural_network_get_average_activity(neural_network_t network, uint32_t neuron_id);

//=============================================================================
// Spike Recording
//=============================================================================

/**
 * @brief Record a spike event
 *
 * @param network Neural network
 * @param neuron_id Neuron that spiked
 * @param magnitude Spike magnitude
 * @param timestamp Spike time
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_record_spike(neural_network_t network, uint32_t neuron_id,
                                              float magnitude, uint64_t timestamp);

//=============================================================================
// Time Management
//=============================================================================

/**
 * @brief Set network time
 *
 * @param network Neural network
 * @param timestamp New network time
 */
NIMCP_EXPORT void neural_network_set_time(neural_network_t network, uint64_t timestamp);

//=============================================================================
// Subsystem Integration
//=============================================================================

/**
 * @brief Set global state for synapse computation
 *
 * @param network Neural network
 * @param global_state Global state buffer
 * @param size Size of state buffer
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_set_global_state(neural_network_t network, float* global_state,
                                                  uint32_t size);

/**
 * @brief Set neuromodulator system
 *
 * @param network Neural network
 * @param neuromod_system Neuromodulator system handle
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_set_neuromodulator_system(neural_network_t network,
                                                           void* neuromod_system);

/**
 * @brief Set glial integration system
 *
 * @param network Neural network
 * @param glial_system Glial integration handle
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_set_glial_integration(neural_network_t network, void* glial_system);

/**
 * @brief Get neuromodulation level
 *
 * @param network Neural network
 * @return Current neuromodulation level
 */
NIMCP_EXPORT float neural_network_get_neuromodulation(neural_network_t network);

/**
 * @brief Set neuron model type
 *
 * @param network Neural network
 * @param neuron_id Neuron ID
 * @param model_type Model type (LIF, Izhikevich, etc.)
 * @param params Model-specific parameters
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_set_neuron_model(neural_network_t network, uint32_t neuron_id,
                                                  neuron_model_type_t model_type, const void* params);

//=============================================================================
// Debugging
//=============================================================================

/**
 * @brief Dump neuron state for debugging
 *
 * @param network Neural network
 * @param neuron_id Neuron to dump
 */
NIMCP_EXPORT void neural_network_dump_neuron(neural_network_t network, uint32_t neuron_id);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURALNET_CORE_H
