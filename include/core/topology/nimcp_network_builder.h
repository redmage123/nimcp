//=============================================================================
// nimcp_network_builder.h - High-Level Network Builder API (NIMCP 2.7 Phase 2)
//=============================================================================
/**
 * @file nimcp_network_builder.h
 * @brief Simplified API for creating neural networks with fractal topologies
 *
 * WHAT: High-level builder pattern for network+topology creation
 * WHY: Creating a network with fractal topology requires multiple steps
 * HOW: Fluent API that configures and builds in one call
 *
 * EXAMPLE:
 * ```c
 * neural_network_t net = network_builder()
 *     .with_neurons(1000)
 *     .with_scale_free_topology()
 *     .with_pink_noise_weights(0.5f)
 *     .build();
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 */

#ifndef NIMCP_NETWORK_BUILDER_H
#define NIMCP_NETWORK_BUILDER_H

#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/topology/nimcp_fractal_topology.h"
#include "plasticity/noise/nimcp_pink_noise.h"

//=============================================================================
// Builder Configuration Structure
//=============================================================================

/**
 * @brief Configuration for network builder
 *
 * WHAT: Accumulates all settings before building network
 * WHY: Allows fluent API and validation before creation
 * HOW: Filled via builder methods, consumed by build()
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Basic network config
    uint32_t num_neurons;
    float ei_ratio;
    bool enable_stdp;
    bool enable_homeostasis;

    // Topology config (optional)
    bool use_topology;
    topology_config_t topology_config;

    // Weight initialization (optional)
    bool use_pink_noise_weights;
    float noise_amplitude;
    float base_weight;

    // Advanced options
    uint32_t random_seed;
    bool verbose;
} network_builder_config_t;

//=============================================================================
// Builder API Functions
//=============================================================================

/**
 * @brief Create default builder configuration
 *
 * WHAT: Returns sensible defaults for all parameters
 * WHY: Users can customize only what they need
 * HOW: Memset to zero, then set non-zero defaults
 *
 * @return Default builder configuration
 */
network_builder_config_t network_builder_default(void);

/**
 * @brief Build neural network from configuration
 *
 * WHAT: Creates network, applies topology, initializes weights
 * WHY: One-stop function to create complete network
 * HOW:
 *   1. Create base network with neural_network_create()
 *   2. Apply topology if configured
 *   3. Initialize weights with pink noise if configured
 *   4. Return fully-configured network
 *
 * @param config Builder configuration
 * @return Created network, or NULL on error
 */
neural_network_t network_builder_build(const network_builder_config_t* config);

/**
 * @brief Create scale-free network (shorthand)
 *
 * WHAT: Quick helper for common use case
 * WHY: Most users want scale-free topology
 * HOW: Calls network_builder_build with scale-free config
 *
 * @param num_neurons Number of neurons
 * @param gamma Power-law exponent (typically -2.1)
 * @return Created network with scale-free topology
 */
neural_network_t network_create_scale_free(uint32_t num_neurons, float gamma);

/**
 * @brief Create fractal network (shorthand)
 *
 * WHAT: Quick helper for fractal topology
 * WHY: Fractal networks are common for hierarchical tasks
 * HOW: Calls network_builder_build with fractal config
 *
 * @param num_neurons Number of neurons
 * @param fractal_dimension Fractal dimension (typically 2.5)
 * @return Created network with fractal topology
 */
neural_network_t network_create_fractal(uint32_t num_neurons, float fractal_dimension);

/**
 * @brief Initialize network weights with pink noise
 *
 * WHAT: Set all synapse weights using 1/f noise distribution
 * WHY: Pink noise matches biological synaptic distributions
 * HOW: Generate pink noise samples, map to [-amplitude, amplitude]
 *
 * @param network Network to initialize
 * @param amplitude Weight amplitude (e.g., 0.5f)
 * @param base_weight Base weight to add to noise (e.g., 0.0f)
 * @return true on success, false on error
 */
bool network_init_weights_pink_noise(
    neural_network_t network,
    float amplitude,
    float base_weight
);

//=============================================================================
// Fluent API Macros (Optional - C99 Compound Literals)
//=============================================================================

#if __STDC_VERSION__ >= 199901L

/**
 * @brief Start builder chain
 * USAGE: network_builder().with_neurons(1000).build()
 */
#define network_builder() \
    network_builder_default()

#endif // C99


#ifdef __cplusplus
}
#endif
#endif // NIMCP_NETWORK_BUILDER_H
