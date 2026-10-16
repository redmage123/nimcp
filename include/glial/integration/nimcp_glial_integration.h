/**
 * @file nimcp_glial_integration.h
 * @brief Integration layer connecting glial cells with neural network
 *
 * DESIGN PHILOSOPHY:
 * - Non-invasive: Doesn't modify neuron_t or synapse_t structures
 * - Parallel tracking: Maps neuron/synapse IDs to assigned glial cells
 * - Callbacks: Glial cells notified of neural events
 * - Performance: O(1) lookup for glial cell assignments
 *
 * BIOLOGICAL MODEL:
 * - Tripartite synapse: Astrocyte + presynaptic + postsynaptic
 * - Myelinated axons: Oligodendrocyte wraps neuron axons
 * - Synaptic surveillance: Microglia monitor synapse activity
 *
 * INTEGRATION POINTS:
 * 1. Astrocyte-Synapse: Modulate synaptic weight (0.8x - 1.2x)
 * 2. Oligodendrocyte-Neuron: Reduce conduction delay (up to 50x)
 * 3. Microglia-Synapse: Monitor activity and prune weak connections
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create glial integration system
 * glial_integration_t* gi = glial_integration_create(network, 100);
 *
 * // Add glial cells
 * glial_integration_add_astrocyte(gi, astrocyte, synapse_id);
 * glial_integration_add_oligodendrocyte(gi, oligo, neuron_id);
 * glial_integration_add_microglia(gi, microglia, synapse_id);
 *
 * // During network step, notify glial cells
 * glial_integration_on_synapse_fired(gi, synapse_id, weight, timestamp);
 * glial_integration_on_neuron_fired(gi, neuron_id, timestamp);
 *
 * // Get glial modulation
 * float modulation = glial_integration_get_synaptic_modulation(gi, synapse_id);
 * float myelination = glial_integration_get_myelination_factor(gi, neuron_id);
 * ```
 *
 * TDD STATUS: Header-first design, tests to be written
 */

#ifndef NIMCP_GLIAL_INTEGRATION_H
#define NIMCP_GLIAL_INTEGRATION_H

#include "utils/validation/nimcp_common.h"
#include "utils/containers/nimcp_hash_table.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "glial/microglia/nimcp_microglia.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"  // Myelin sheath structural modeling
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"  // Part A2.1: Spatial diffusion
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Glial integration system
 *
 * Manages the mapping between neurons/synapses and glial cells.
 * Provides callbacks for glial-neuron communication.
 */
typedef struct {
    neural_network_t network;  /**< Neural network being integrated */

    // Glial cell networks
    astrocyte_network_t* astrocyte_network;
    oligodendrocyte_network_t* oligodendrocyte_network;
    microglia_network_t* microglia_network;
    myelin_sheath_network_t* myelin_sheath_network;  /**< Myelin sheath structural modeling */

    // Part A2.1: Spatial Neuromodulator Diffusion
    spatial_neuromod_system_t* spatial_neuromod;  /**< Spatial diffusion of DA, 5-HT, ACh, NE */

    // ID mappings (synapse/neuron ID → glial cell ID)
    hash_table_t* synapse_to_astrocyte;     /**< synapse_id → astrocyte_id */
    hash_table_t* neuron_to_oligodendrocyte; /**< neuron_id → oligo_id */
    hash_table_t* synapse_to_microglia;     /**< synapse_id → microglia_id */

    // Reverse mappings (for spatial queries)
    hash_table_t* astrocyte_to_synapses;     /**< astrocyte_id → synapse_ids[] */
    hash_table_t* oligodendrocyte_to_neurons; /**< oligo_id → neuron_ids[] */
    hash_table_t* microglia_to_synapses;     /**< microglia_id → synapse_ids[] */

    // Statistics
    uint64_t total_astrocyte_modulations;
    uint64_t total_oligodendrocyte_myelinations;
    uint64_t total_microglia_prunings;
    uint64_t total_neuromod_updates;         /**< Part A2.1: Spatial neuromod step count */

    // Configuration
    bool enable_astrocyte_modulation;
    bool enable_oligodendrocyte_myelination;
    bool enable_microglia_pruning;
    bool enable_spatial_neuromod;            /**< Part A2.1: Enable spatial diffusion */
    bool enable_myelin_sheath;               /**< Enable myelin sheath structural modeling */
    bool enable_bio_async;                   /**< Enable bio-async messaging */

    // Bio-Async Integration
    void* bio_ctx;                           /**< Bio-async module context */
    bool bio_async_enabled;                  /**< Bio-async integration active */

    // Timing (for proper dt calculation)
    uint64_t last_update_timestamp_us;       /**< Last update timestamp (µs) for dt computation */

    // Thread safety
    nimcp_mutex_t lock;
} glial_integration_t;

/**
 * @brief Statistics for glial integration
 */
typedef struct {
    uint32_t num_astrocytes;
    uint32_t num_oligodendrocytes;
    uint32_t num_microglia;

    uint32_t num_tripartite_synapses;  /**< Synapses with astrocyte */
    uint32_t num_myelinated_neurons;   /**< Neurons with oligodendrocyte */
    uint32_t num_monitored_synapses;   /**< Synapses monitored by microglia */

    uint64_t total_modulations;
    uint64_t total_myelinations;
    uint64_t total_prunings;

    float avg_synaptic_modulation;    /**< Average modulation factor */
    float avg_myelination_factor;     /**< Average myelination strength */
    float avg_pruning_rate;           /**< Synapses pruned per timestep */
} glial_integration_stats_t;

// ============================================================================
// CREATION & DESTRUCTION
// ============================================================================

/**
 * @brief Create glial integration system
 *
 * @param network Neural network to integrate with
 * @param max_mappings Initial capacity for ID mappings
 *
 * @return Glial integration handle or NULL on error
 */
glial_integration_t* glial_integration_create(neural_network_t network, uint32_t max_mappings);

/**
 * @brief Destroy glial integration system
 *
 * @param gi Glial integration system (NULL safe)
 *
 * NOTE: Does NOT destroy the neural network or glial cell networks
 *       (caller retains ownership)
 */
void glial_integration_destroy(glial_integration_t* gi);

// ============================================================================
// GLIAL CELL ASSIGNMENT
// ============================================================================

/**
 * @brief Assign astrocyte network to integration system
 *
 * @param gi Glial integration system
 * @param astrocyte_network Astrocyte network
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t glial_integration_set_astrocyte_network(glial_integration_t* gi,
                                                       astrocyte_network_t* astrocyte_network);

/**
 * @brief Assign oligodendrocyte network to integration system
 *
 * @param gi Glial integration system
 * @param oligo_network Oligodendrocyte network
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t glial_integration_set_oligodendrocyte_network(
    glial_integration_t* gi, oligodendrocyte_network_t* oligo_network);

/**
 * @brief Assign microglia network to integration system
 *
 * @param gi Glial integration system
 * @param microglia_network Microglia network
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t glial_integration_set_microglia_network(glial_integration_t* gi,
                                                       microglia_network_t* microglia_network);

/**
 * @brief Assign myelin sheath network to integration system
 *
 * @param gi Glial integration system
 * @param myelin_network Myelin sheath network
 *
 * @return NIMCP_SUCCESS on success
 *
 * NOTE: Myelin sheath provides detailed structural modeling of myelin
 *       and integrates with oligodendrocyte myelination decisions
 */
nimcp_result_t glial_integration_set_myelin_sheath_network(
    glial_integration_t* gi, myelin_sheath_network_t* myelin_network);

/**
 * @brief Assign spatial neuromodulator system to integration (Phase C2.1)
 *
 * @param gi Glial integration system
 * @param spatial_neuromod Spatial neuromodulator system
 *
 * @return NIMCP_SUCCESS on success
 *
 * NOTE: Glial integration takes ownership and will call destroy on cleanup
 */
nimcp_result_t glial_integration_set_spatial_neuromod_system(
    glial_integration_t* gi, spatial_neuromod_system_t* spatial_neuromod);

/**
 * @brief Assign astrocyte to monitor a synapse
 *
 * @param gi Glial integration system
 * @param astrocyte_id ID of astrocyte
 * @param synapse_id ID of synapse (G7: uint64, packed (pre<<32)|post)
 *
 * @return NIMCP_SUCCESS on success
 *
 * EFFECT: Astrocyte will modulate this synapse based on calcium levels
 */
nimcp_result_t glial_integration_assign_astrocyte_to_synapse(glial_integration_t* gi,
                                                             uint32_t astrocyte_id,
                                                             uint64_t synapse_id);

/**
 * @brief Assign oligodendrocyte to myelinate a neuron
 *
 * @param gi Glial integration system
 * @param oligo_id ID of oligodendrocyte
 * @param neuron_id ID of neuron
 *
 * @return NIMCP_SUCCESS on success
 *
 * EFFECT: Oligodendrocyte will myelinate neuron axon, reducing conduction delay
 */
nimcp_result_t glial_integration_assign_oligodendrocyte_to_neuron(glial_integration_t* gi,
                                                                  uint32_t oligo_id,
                                                                  uint32_t neuron_id);

/**
 * @brief Assign microglia to monitor a synapse
 *
 * @param gi Glial integration system
 * @param microglia_id ID of microglia
 * @param synapse_id ID of synapse
 *
 * @return NIMCP_SUCCESS on success
 *
 * EFFECT: Microglia will monitor synapse activity and may prune if weak
 */
nimcp_result_t glial_integration_assign_microglia_to_synapse(glial_integration_t* gi,
                                                             uint32_t microglia_id,
                                                             uint64_t synapse_id);

// ============================================================================
// AUTOMATIC SPATIAL ASSIGNMENT
// ============================================================================

/**
 * @brief Automatically assign glial cells based on spatial proximity
 *
 * Uses spatial coordinates to assign:
 * - Astrocytes to nearby synapses (within radius)
 * - Oligodendrocytes to nearby neurons (capacity-limited)
 * - Microglia to synapses within surveillance radius
 *
 * @param gi Glial integration system
 *
 * @return Number of assignments made
 *
 * REQUIRES: Neurons and glial cells must have spatial positions
 */
uint32_t glial_integration_auto_assign_spatial(glial_integration_t* gi);

// ============================================================================
// EVENT NOTIFICATIONS (Called by neural network during simulation)
// ============================================================================

/**
 * @brief Notify glial cells that a synapse fired
 *
 * @param gi Glial integration system
 * @param pre_neuron_id ID of presynaptic neuron
 * @param post_neuron_id ID of postsynaptic neuron
 * @param synaptic_weight Current synaptic weight
 * @param timestamp Current timestamp (µs)
 *
 * EFFECTS:
 * - Astrocyte: Increase calcium, may modulate synapse
 * - Microglia: Track activity for pruning decision
 */
void glial_integration_on_synapse_fired(glial_integration_t* gi, uint32_t pre_neuron_id,
                                        uint32_t post_neuron_id, float synaptic_weight,
                                        uint64_t timestamp);

/**
 * @brief Notify glial cells that a neuron fired
 *
 * @param gi Glial integration system
 * @param neuron_id ID of neuron that fired
 * @param timestamp Current timestamp (µs)
 *
 * EFFECTS:
 * - Oligodendrocyte: Adaptive myelination based on activity
 */
void glial_integration_on_neuron_fired(glial_integration_t* gi, uint32_t neuron_id,
                                       uint64_t timestamp);

// ============================================================================
// GLIAL MODULATION QUERIES (Called by neural network to get glial effects)
// ============================================================================

/**
 * @brief Get astrocyte modulation factor for synapse
 *
 * @param gi Glial integration system
 * @param pre_neuron_id ID of presynaptic neuron
 * @param post_neuron_id ID of postsynaptic neuron
 *
 * @return Modulation factor (0.8 - 1.2), or 1.0 if no astrocyte assigned
 *
 * USAGE: Multiply synaptic weight by this factor during transmission
 */
float glial_integration_get_synaptic_modulation(glial_integration_t* gi, uint32_t pre_neuron_id,
                                                 uint32_t post_neuron_id);

/**
 * @brief Get myelination factor for neuron
 *
 * @param gi Glial integration system
 * @param neuron_id ID of neuron
 *
 * @return Myelination factor (0.0 - 1.0), or 0.0 if no oligodendrocyte
 *
 * USAGE: Compute conduction delay = base_delay / (1 + myelination * 49)
 */
float glial_integration_get_myelination_factor(glial_integration_t* gi, uint32_t neuron_id);

/**
 * @brief Check if synapse should be pruned by microglia
 *
 * @param gi Glial integration system
 * @param pre_neuron_id ID of presynaptic neuron
 * @param post_neuron_id ID of postsynaptic neuron
 *
 * @return true if synapse is marked for pruning
 *
 * USAGE: Neural network should remove this synapse
 */
bool glial_integration_should_prune_synapse(glial_integration_t* gi, uint32_t pre_neuron_id,
                                            uint32_t post_neuron_id);

// ============================================================================
// SIMULATION STEP
// ============================================================================

/**
 * @brief Step all glial cells forward in time
 *
 * @param gi Glial integration system
 * @param timestamp Current timestamp (µs)
 *
 * OPERATIONS:
 * - Update astrocyte calcium dynamics
 * - Update oligodendrocyte myelination
 * - Update microglia activity scores and pruning
 *
 * SHOULD BE CALLED: Once per neural network timestep
 */
void glial_integration_step(glial_integration_t* gi, uint64_t timestamp);

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

/**
 * @brief Get glial integration statistics
 *
 * @param gi Glial integration system
 * @param stats Output statistics
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t glial_integration_get_stats(glial_integration_t* gi,
                                           glial_integration_stats_t* stats);

/**
 * @brief Get number of synapses assigned to an astrocyte
 *
 * @param gi Glial integration system
 * @param astrocyte_id ID of astrocyte
 *
 * @return Number of assigned synapses
 */
uint32_t glial_integration_get_astrocyte_synapse_count(glial_integration_t* gi,
                                                       uint32_t astrocyte_id);

/**
 * @brief Get number of neurons assigned to an oligodendrocyte
 *
 * @param gi Glial integration system
 * @param oligo_id ID of oligodendrocyte
 *
 * @return Number of assigned neurons
 */
uint32_t glial_integration_get_oligodendrocyte_neuron_count(glial_integration_t* gi,
                                                            uint32_t oligo_id);

// ============================================================================
// CONFIGURATION
// ============================================================================

/**
 * @brief Enable/disable astrocyte modulation
 *
 * @param gi Glial integration system
 * @param enable true to enable, false to disable
 */
void glial_integration_set_astrocyte_modulation_enabled(glial_integration_t* gi, bool enable);

/**
 * @brief Enable/disable oligodendrocyte myelination
 *
 * @param gi Glial integration system
 * @param enable true to enable, false to disable
 */
void glial_integration_set_oligodendrocyte_myelination_enabled(glial_integration_t* gi,
                                                               bool enable);

/**
 * @brief Enable/disable microglia pruning
 *
 * @param gi Glial integration system
 * @param enable true to enable, false to disable
 */
void glial_integration_set_microglia_pruning_enabled(glial_integration_t* gi, bool enable);

/**
 * @brief Enable/disable myelin sheath structural modeling
 *
 * @param gi Glial integration system
 * @param enable true to enable, false to disable
 */
void glial_integration_set_myelin_sheath_enabled(glial_integration_t* gi, bool enable);

// ============================================================================
// MYELIN SHEATH INTEGRATION
// ============================================================================

/**
 * @brief Get myelin sheath conduction velocity for axon
 *
 * @param gi Glial integration system
 * @param axon_id ID of axon
 *
 * @return Conduction velocity in m/s, or base velocity if no myelin
 *
 * USAGE: Use for accurate signal propagation timing
 */
float glial_integration_get_myelin_velocity(glial_integration_t* gi, uint32_t axon_id);

/**
 * @brief Get myelin sheath propagation delay for axon
 *
 * @param gi Glial integration system
 * @param axon_id ID of axon
 *
 * @return Propagation delay in ms, or 0 if no myelin
 */
float glial_integration_get_myelin_delay(glial_integration_t* gi, uint32_t axon_id);

/**
 * @brief Create myelin sheath for axon (coordinated with oligodendrocyte)
 *
 * @param gi Glial integration system
 * @param axon_id ID of axon
 * @param oligo_id ID of oligodendrocyte providing myelin
 * @param axon_length Length of axon in micrometers
 * @param axon_diameter Diameter of axon in micrometers
 *
 * @return Created myelin sheath or NULL on failure
 */
myelin_sheath_t* glial_integration_create_myelin_sheath(
    glial_integration_t* gi,
    uint32_t axon_id,
    uint32_t oligo_id,
    float axon_length,
    float axon_diameter);

/**
 * @brief Apply axon activity to myelin sheath (activity-dependent myelination)
 *
 * @param gi Glial integration system
 * @param axon_id ID of axon
 * @param activity_level Activity level (firing rate)
 * @param dt Time step in seconds
 */
void glial_integration_apply_axon_activity_to_myelin(
    glial_integration_t* gi,
    uint32_t axon_id,
    float activity_level,
    float dt);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GLIAL_INTEGRATION_H
