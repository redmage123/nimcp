/**
 * @file nimcp_neural_logic_factory.h
 * @brief MODULE 5: Neural Logic Factory - Create Pre-Configured Neural Logic Networks
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Factory for creating and configuring neural logic networks for brains
 * WHY:  Single Responsibility: Encapsulate network creation and configuration
 * HOW:  Provide pre-configured network constructors with sensible defaults
 *
 * SINGLE RESPONSIBILITY PRINCIPLE (SRP):
 * - SOLE RESPONSIBILITY: Create and configure neural logic networks
 * - DOES: Create networks, configure parameters, set defaults
 * - DOES NOT: Attach to brains (MODULE 1), evaluate (MODULE 2), modulate (MODULE 4)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_LOGIC_FACTORY_H
#define NIMCP_NEURAL_LOGIC_FACTORY_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Predefined Network Sizes
//=============================================================================

/** Small network: 100 gates, 26 variables (A-Z) */
#define NEURAL_LOGIC_SIZE_SMALL  100

/** Medium network: 1000 gates, 26 variables */
#define NEURAL_LOGIC_SIZE_MEDIUM 1000

/** Large network: 10000 gates, 26 variables */
#define NEURAL_LOGIC_SIZE_LARGE  10000

//=============================================================================
// MODULE 5: Factory API
//=============================================================================

/**
 * @brief Create default neural logic network for brain
 *
 * WHAT: Construct neural logic network with sensible defaults
 * WHY:  Simplify common case of basic logic network creation
 * HOW:  Use predefined config based on brain size, create network
 *
 * @param brain_size Brain size hint (BRAIN_SIZE_SMALL/MEDIUM/LARGE)
 * @return Neural logic network handle, or NULL on failure
 *
 * GUARD CLAUSES:
 * - None (all inputs have defaults)
 *
 * BEHAVIOR:
 * - Maps brain_size to logic network size:
 *   - SMALL → 100 gates
 *   - MEDIUM → 1000 gates
 *   - LARGE → 10000 gates
 * - Creates neural_logic_config_t with defaults:
 *   - max_variables: 26 (A-Z)
 *   - variable_pattern_dim: 64
 *   - use_gpu: true (if available)
 *   - timestep_us: 100
 *   - integration_window_ms: 10
 * - Calls neural_logic_create(config)
 * - Returns network handle
 *
 * DEFAULT CONFIGURATION:
 * - GPU acceleration: Enabled (if CUDA available)
 * - Threads per block: 256
 * - Timestep: 100 μs (10 kHz update rate)
 * - Integration window: 10 ms
 * - Learning: Disabled (static gates)
 *
 * COMPLEXITY: O(n) where n = max_logic_neurons
 * THREAD SAFETY: Thread-safe (each call creates independent network)
 *
 * EXAMPLE:
 * ```c
 * // Create small network (100 gates)
 * neural_logic_network_t net = create_default_neural_logic(BRAIN_SIZE_SMALL);
 *
 * if (net) {
 *     brain_attach_neural_logic(brain, net);
 * }
 * ```
 */
NIMCP_EXPORT neural_logic_network_t create_default_neural_logic(
    uint32_t brain_size
);

/**
 * @brief Create neural logic network with custom configuration
 *
 * WHAT: Construct neural logic network with user-specified parameters
 * WHY:  Enable advanced use cases requiring custom network topology
 * HOW:  Validate config, create network with neural_logic_create()
 *
 * @param config Custom neural logic configuration (must be non-NULL)
 * @return Neural logic network handle, or NULL on failure
 *
 * GUARD CLAUSES:
 * - NULL config → NULL + error log
 * - config->max_logic_neurons == 0 → NULL + error log
 * - config->max_variables == 0 → NULL + error log
 * - config->variable_pattern_dim == 0 → NULL + error log
 *
 * BEHAVIOR:
 * - Validates configuration parameters
 * - Calls neural_logic_create(config)
 * - Logs creation with network size
 * - Returns network handle
 *
 * VALIDATION RULES:
 * - max_logic_neurons: [1, UINT32_MAX]
 * - max_variables: [1, 26] (A-Z)
 * - variable_pattern_dim: [8, 1024] (practical limits)
 * - threads_per_block: [32, 1024] (CUDA limits)
 * - timestep_us: [10, 10000] (0.01-10 ms)
 * - integration_window_ms: [1, 1000] (1-1000 ms)
 *
 * COMPLEXITY: O(n) where n = max_logic_neurons
 * THREAD SAFETY: Thread-safe (each call creates independent network)
 *
 * EXAMPLE:
 * ```c
 * neural_logic_config_t config = {
 *     .max_logic_neurons = 5000,
 *     .max_variables = 26,
 *     .variable_pattern_dim = 128,
 *     .threads_per_block = 256,
 *     .use_gpu = true,
 *     .timestep_us = 100,
 *     .integration_window_ms = 20,
 *     .enable_learning = false,
 *     .learning_rate = 0.0f
 * };
 *
 * neural_logic_network_t net = create_neural_logic_with_config(&config);
 * ```
 */
NIMCP_EXPORT neural_logic_network_t create_neural_logic_with_config(
    const neural_logic_config_t* config
);

/**
 * @brief Create neural logic network and attach to brain (convenience)
 *
 * WHAT: One-step network creation + attachment
 * WHY:  Simplify most common use case
 * HOW:  Create network → attach to brain → return success
 *
 * @param brain Brain instance (must be non-NULL)
 * @param brain_size Brain size hint for network sizing
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic != NULL → false + warning log
 *
 * BEHAVIOR:
 * - Creates default network with create_default_neural_logic(brain_size)
 * - Calls brain_attach_neural_logic(brain, network)
 * - On failure: destroys network, returns false
 * - On success: returns true (brain owns network)
 *
 * OWNERSHIP:
 * - On success: brain owns network (will destroy on brain_destroy)
 * - On failure: no ownership transfer (network destroyed)
 *
 * COMPLEXITY: O(n) where n = max_logic_neurons
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * brain_t brain = brain_create("reasoner", BRAIN_SIZE_MEDIUM);
 *
 * if (create_and_attach_neural_logic(brain, BRAIN_SIZE_MEDIUM)) {
 *     // Brain now has logic capabilities
 *     uint32_t circuit = brain_build_logic_circuit(brain, "A AND B");
 * }
 * ```
 */
NIMCP_EXPORT bool create_and_attach_neural_logic(
    brain_t brain,
    uint32_t brain_size
);

/**
 * @brief Get default configuration for neural logic network
 *
 * WHAT: Return sensible default configuration structure
 * WHY:  Provide starting point for custom configurations
 * HOW:  Fill neural_logic_config_t with recommended defaults
 *
 * @param max_neurons Maximum logic neurons (typically 100-10000)
 * @return Default configuration structure
 *
 * GUARD CLAUSES:
 * - max_neurons == 0 → use 1000 as default
 *
 * DEFAULT VALUES:
 * - max_logic_neurons: max_neurons (parameter)
 * - max_variables: 26 (A-Z)
 * - variable_pattern_dim: 64
 * - threads_per_block: 256
 * - use_gpu: true (if neural_logic_gpu_available())
 * - pin_host_memory: true (faster GPU transfers)
 * - timestep_us: 100 (10 kHz)
 * - integration_window_ms: 10
 * - enable_learning: false (static gates)
 * - learning_rate: 0.0
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe (no shared state)
 *
 * EXAMPLE:
 * ```c
 * neural_logic_config_t config = get_default_neural_logic_config(500);
 *
 * // Customize as needed
 * config.use_gpu = false;
 * config.integration_window_ms = 20;
 *
 * neural_logic_network_t net = create_neural_logic_with_config(&config);
 * ```
 */
NIMCP_EXPORT neural_logic_config_t get_default_neural_logic_config(
    uint32_t max_neurons
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_FACTORY_H
