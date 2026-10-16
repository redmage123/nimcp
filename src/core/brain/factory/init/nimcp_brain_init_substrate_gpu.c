/**
 * @file nimcp_brain_init_substrate_gpu.c
 * @brief GPU Neural Substrate Initialization for Brain Factory
 *
 * WHAT: Initializes unified GPU substrate context during brain creation
 * WHY:  Enables GPU-accelerated substrate processing (axons, dendrites, myelin, etc.)
 * HOW:  Creates substrate GPU context if GPU is enabled, sizes based on brain config
 *
 * ARCHITECTURE:
 * - Depends on: GPU context (must be initialized first)
 * - Initializes: substrate_gpu_ctx with appropriately sized tensor buffers
 * - Integrates with: axon, dendrite, glial, neuromodulator, metabolic subsystems
 *
 * INITIALIZATION ORDER:
 * This subsystem must be initialized AFTER:
 * 1. GPU subsystem (requires gpu_ctx)
 * 2. Brain regions (provides neuron counts)
 * 3. Neuromodulator system (provides pool counts)
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#include "core/brain/factory/init/nimcp_brain_init.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/nimcp_brain_internal.h"
#include "gpu/substrate/nimcp_substrate_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_SUBSTRATE_GPU"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_substrate_gpu, MESH_ADAPTER_CATEGORY_SYSTEM)


/**
 * @brief Estimate neuron counts from brain size preset for substrate sizing
 *
 * Maps brain size enum to neuron estimates based on NIMCP size specifications
 */
static uint32_t estimate_total_neurons(brain_t brain)
{
    if (!brain) {
        return 10000;  // Default fallback
    }

    // Map brain size to neuron count (from brain_size_t documentation)
    // MICRO=25, TINY=100, SMALL=500, MEDIUM=1K, LARGE=5K, CUSTOM=default
    switch (brain->config.size) {
        case BRAIN_SIZE_MICRO:  return 25;
        case BRAIN_SIZE_TINY:   return 100;
        case BRAIN_SIZE_SMALL:  return 500;
        case BRAIN_SIZE_MEDIUM: return 1000;
        case BRAIN_SIZE_LARGE:  return 5000;
        case BRAIN_SIZE_CUSTOM:
        default:
            // Use num_inputs/outputs as a rough indicator
            return 1000 + brain->config.num_inputs + brain->config.num_outputs;
    }
}

/**
 * @brief Estimate axon count from network structure
 */
static uint32_t estimate_axon_count(brain_t brain)
{
    // Each neuron has one axon
    return estimate_total_neurons(brain);
}

/**
 * @brief Estimate dendrite count from network structure
 */
static uint32_t estimate_dendrite_count(brain_t brain)
{
    // Each neuron has ~5-10 primary dendrites on average
    return estimate_total_neurons(brain) * 6;
}

/**
 * @brief Estimate spine count from network structure
 */
static uint32_t estimate_spine_count(brain_t brain)
{
    // Each dendrite has ~100-1000 spines; use conservative estimate
    uint32_t dendrites = estimate_dendrite_count(brain);
    return dendrites * 50;  // 50 spines per dendrite average
}

/**
 * @brief Estimate neuromodulator pool count
 *
 * Number of pools scales with brain size
 */
static uint32_t estimate_neuromod_pools(brain_t brain)
{
    if (!brain) {
        return 10;
    }

    // Scale pools with brain size
    switch (brain->config.size) {
        case BRAIN_SIZE_MICRO:  return 3;
        case BRAIN_SIZE_TINY:   return 5;
        case BRAIN_SIZE_SMALL:  return 10;
        case BRAIN_SIZE_MEDIUM: return 20;
        case BRAIN_SIZE_LARGE:  return 50;
        case BRAIN_SIZE_CUSTOM:
        default:
            return 20;  // Reasonable default
    }
}

/**
 * @brief Estimate synapse count for neuromodulator receptor modeling
 */
static uint32_t estimate_synapse_count(brain_t brain)
{
    // Rough estimate: each neuron has ~1000 synapses
    return estimate_total_neurons(brain) * 100;  // Conservative: 100 per neuron
}

/**
 * @brief Estimate glial cell counts
 */
static void estimate_glial_counts(
    brain_t brain,
    uint32_t* astrocytes,
    uint32_t* microglia,
    uint32_t* opcs)
{
    uint32_t neurons = estimate_total_neurons(brain);

    // Glial to neuron ratio is roughly 1:1 in human brain
    // Astrocytes: ~50% of glia
    // Microglia: ~10% of glia
    // OPCs: ~5% of glia

    *astrocytes = neurons / 2;
    *microglia = neurons / 10;
    *opcs = neurons / 20;
}

/**
 * @brief Initialize GPU Neural Substrate subsystem
 *
 * WHAT: Creates substrate_gpu_ctx with appropriately sized tensor buffers
 * WHY:  Enables unified GPU acceleration for all substrate components
 * HOW:  Sizes tensors based on brain configuration and estimated neuron counts
 *
 * @param brain Brain instance to initialize substrate GPU for
 * @return true on success (including fallback to no substrate GPU), false on error
 *
 * COMPLEXITY: O(N) where N is total substrate element count
 *
 * THREAD SAFETY: NOT thread-safe (call during brain creation only)
 */
bool nimcp_brain_factory_init_substrate_gpu_subsystem(brain_t brain)
{
    if (!brain) {
        LOG_ERROR("NULL brain provided to substrate GPU init");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_substrate_gpu_subsystem: brain is NULL");
        return false;
    }

    // Initialize to NULL (safe default)
    brain->substrate_gpu_ctx = NULL;

    // Check if GPU is enabled
    if (!brain->gpu_enabled || !brain->gpu_ctx) {
        LOG_INFO("GPU not enabled, skipping substrate GPU initialization");
        return true;  // Success - substrate GPU is optional
    }

    LOG_INFO("Initializing GPU neural substrate subsystem...");

    // Create substrate GPU context with default config
    substrate_gpu_config_t config = substrate_gpu_default_config();

    // Adjust config based on brain size
    uint32_t n_neurons = estimate_total_neurons(brain);
    uint32_t n_axons = estimate_axon_count(brain);
    uint32_t n_dendrites = estimate_dendrite_count(brain);

    config.axon.max_axons = n_axons * 2;  // Allow growth
    config.dendrite.max_dendrites = n_dendrites * 2;
    config.neuromod.max_pools = estimate_neuromod_pools(brain) * 2;

    brain->substrate_gpu_ctx = substrate_gpu_create(brain->gpu_ctx, &config);
    if (!brain->substrate_gpu_ctx) {
        LOG_WARN("Failed to create substrate GPU context - continuing without GPU substrate");
        return true;  // Success - fallback to CPU substrate operations
    }

    // Initialize tensor buffers for each subsystem
    int result = 0;

    // Axon tensors
    result = substrate_gpu_init_axons(brain->substrate_gpu_ctx, n_axons);
    if (result != 0) {
        LOG_WARN("Failed to initialize axon GPU tensors");
    }

    // Dendrite tensors
    uint32_t n_segments = 20;  // Default segments per dendrite
    uint32_t n_spines = estimate_spine_count(brain);
    result = substrate_gpu_init_dendrites(brain->substrate_gpu_ctx, n_dendrites, n_segments, n_spines);
    if (result != 0) {
        LOG_WARN("Failed to initialize dendrite GPU tensors");
    }

    // Myelin tensors (assume ~30% of axons are myelinated)
    uint32_t n_myelinated = n_axons * 30 / 100;
    result = substrate_gpu_init_myelin(brain->substrate_gpu_ctx, n_myelinated);
    if (result != 0) {
        LOG_WARN("Failed to initialize myelin GPU tensors");
    }

    // Neuromodulator tensors
    uint32_t n_pools = estimate_neuromod_pools(brain);
    uint32_t n_types = 4;  // DA, 5HT, ACh, NE
    uint32_t n_synapses = estimate_synapse_count(brain);
    result = substrate_gpu_init_neuromod(brain->substrate_gpu_ctx, n_pools, n_types, n_synapses);
    if (result != 0) {
        LOG_WARN("Failed to initialize neuromodulator GPU tensors");
    }

    // Glial tensors
    uint32_t n_astrocytes, n_microglia, n_opcs;
    estimate_glial_counts(brain, &n_astrocytes, &n_microglia, &n_opcs);
    uint32_t n_neighbors = 6;  // Typical gap junction connectivity
    result = substrate_gpu_init_glial(brain->substrate_gpu_ctx, n_astrocytes, n_microglia, n_opcs, n_neighbors);
    if (result != 0) {
        LOG_WARN("Failed to initialize glial GPU tensors");
    }

    // Metabolic tensors (one region per brain region)
    uint32_t n_regions = estimate_neuromod_pools(brain);  // Same as neuromod pools
    result = substrate_gpu_init_metabolic(brain->substrate_gpu_ctx, n_regions);
    if (result != 0) {
        LOG_WARN("Failed to initialize metabolic GPU tensors");
    }

    LOG_INFO("GPU neural substrate initialized: %u neurons, %u axons, %u dendrites, %u synapses",
             n_neurons, n_axons, n_dendrites, n_synapses);

    return true;
}

/**
 * @brief Destroy GPU Neural Substrate subsystem
 *
 * WHAT: Cleans up substrate GPU context and all tensor buffers
 * WHY:  Proper resource cleanup during brain destruction
 * HOW:  Destroys substrate GPU context if it exists
 *
 * @param brain Brain instance to cleanup substrate GPU for
 *
 * COMPLEXITY: O(N) where N is total tensor count
 *
 * THREAD SAFETY: NOT thread-safe (call during brain destruction only)
 */
void nimcp_brain_factory_destroy_substrate_gpu_subsystem(brain_t brain)
{
    if (!brain) {
        return;
    }

    if (brain->substrate_gpu_ctx) {
        LOG_INFO("Destroying GPU neural substrate subsystem");
        substrate_gpu_destroy(brain->substrate_gpu_ctx);
        brain->substrate_gpu_ctx = NULL;
    }
}
