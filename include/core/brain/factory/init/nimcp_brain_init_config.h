//=============================================================================
// nimcp_brain_init_config.h - Brain Configuration Functions
//=============================================================================
/**
 * @file nimcp_brain_init_config.h
 * @brief Brain configuration and statistics initialization functions
 *
 * WHAT: Configuration building and parameter setup for brain initialization
 * WHY:  Separates configuration logic from brain creation
 * HOW:  Provides builder functions for network config and brain stats
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_BRAIN_INIT_CONFIG_H
#define NIMCP_BRAIN_INIT_CONFIG_H

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Neuron Count and Sparsity Functions
//=============================================================================

/**
 * @brief Get neuron count for brain size preset
 *
 * @param size Brain size preset
 * @return Number of neurons for size
 */
uint32_t nimcp_brain_factory_get_neuron_count(brain_size_t size);

/**
 * @brief Get default sparsity target for size
 *
 * @param size Brain size preset
 * @return Sparsity target (0.0-1.0)
 */
float nimcp_brain_factory_get_default_sparsity(brain_size_t size);

//=============================================================================
// Configuration Builders
//=============================================================================

/**
 * @brief Build spike parameters for brain configuration
 *
 * @param sparsity_target Target sparsity level
 * @return Spike parameters structure
 */
adaptive_spike_params_t nimcp_brain_factory_build_spike_params(float sparsity_target);

/**
 * @brief Build base network configuration
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Total neuron count
 * @param integration_method ODE integration method
 * @return Base network config (caller must free layer_sizes)
 */
network_config_t nimcp_brain_factory_build_base_network_config(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_neurons,
    ode_integration_method_t integration_method);

/**
 * @brief Build complete adaptive network configuration
 *
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param num_neurons Neuron count
 * @param sparsity_target Target sparsity
 * @param integration_method ODE integration method
 * @return Complete adaptive network config
 */
adaptive_network_config_t nimcp_brain_factory_build_network_config(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_neurons,
    float sparsity_target,
    ode_integration_method_t integration_method);

/**
 * @brief Initialize brain configuration structure
 *
 * @param config Brain configuration to initialize
 * @param task_name Task name
 * @param size Brain size preset
 * @param task Task type
 * @param num_inputs Input dimension
 * @param num_outputs Output dimension
 * @param strategy Task strategy
 */
void nimcp_brain_factory_init_brain_config(
    brain_config_t* config,
    const char* task_name,
    brain_size_t size,
    brain_task_t task,
    uint32_t num_inputs,
    uint32_t num_outputs,
    task_strategy_t* strategy);

/**
 * @brief Initialize brain statistics structure
 *
 * @param stats Brain statistics to initialize
 * @param task_name Task name
 * @param size Brain size
 * @param num_inputs Input dimension
 * @param learning_rate Learning rate
 */
void nimcp_brain_factory_init_brain_stats(
    brain_stats_t* stats,
    const char* task_name,
    brain_size_t size,
    uint32_t num_inputs,
    float learning_rate);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INIT_CONFIG_H
