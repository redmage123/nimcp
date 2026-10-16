//=============================================================================
// nimcp_fractal_cognitive.h - Fractal Topology Integration with Cognitive Modules
//=============================================================================
/**
 * @file nimcp_fractal_cognitive.h
 * @brief ENHANCEMENT 2: Cognitive module integration with fractal topology
 *
 * WHAT: High-level API for cognitive modules to leverage fractal network properties
 * WHY: Hub neurons and scale-free structure enable efficient exploration/reasoning
 * HOW: Expose hub neuron data, centrality scores, and hierarchical structure
 *
 * USE CASES:
 * 1. **Curiosity:** Explore via hub neurons (information bottlenecks)
 * 2. **Knowledge:** Use hubs as concept anchors in semantic space
 * 3. **Attention:** Weight salience by degree centrality
 * 4. **Ethics:** Fundamental principles = high-centrality nodes
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#ifndef NIMCP_FRACTAL_COGNITIVE_H
#define NIMCP_FRACTAL_COGNITIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/topology/nimcp_fractal_topology.h"
#include "core/neuralnet/nimcp_neuralnet.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Fractal Properties for Cognitive Modules
//=============================================================================

/**
 * @brief Cached fractal properties for fast cognitive access
 *
 * WHAT: Pre-computed network metrics for cognitive decision-making
 * WHY: Avoid recomputing during time-critical operations
 * HOW: Cached on network initialization, refreshed on topology change
 */
typedef struct {
    uint32_t *hub_indices;          /**< Array of hub neuron indices */
    uint32_t num_hubs;              /**< Count of hub neurons */
    float *centrality_scores;       /**< Betweenness centrality [N] */
    float *degree_normalized;       /**< Degree / max_degree [N] */
    topology_stats_t stats;         /**< Overall topology statistics */
    bool valid;                     /**< True if cache is valid */
} fractal_cognitive_cache_t;

//=============================================================================
// Initialization and Caching
//=============================================================================

/**
 * @brief Initialize fractal cognitive integration
 *
 * WHAT: Computes and caches fractal properties for cognitive use
 * WHY: Enables fast queries during cognitive operations
 * HOW: Identifies hubs, computes centrality, normalizes degrees
 *
 * @param network Neural network to analyze
 * @param cache Output cache structure (allocated by caller)
 * @return true on success
 */
bool fractal_cognitive_init(neural_network_t network, fractal_cognitive_cache_t *cache);

/**
 * @brief Free fractal cognitive cache
 *
 * @param cache Cache to free
 */
void fractal_cognitive_free(fractal_cognitive_cache_t *cache);

/**
 * @brief Refresh cache after topology changes
 *
 * @param network Neural network
 * @param cache Cache to refresh
 * @return true on success
 */
bool fractal_cognitive_refresh(neural_network_t network, fractal_cognitive_cache_t *cache);

//=============================================================================
// Hub Neuron Queries (for Curiosity, Knowledge)
//=============================================================================

/**
 * @brief Check if neuron is a hub
 *
 * WHAT: Tests if neuron index is in hub set
 * WHY: Quick hub membership test for cognitive decisions
 * HOW: Binary search in sorted hub_indices array
 *
 * @param cache Fractal cache
 * @param neuron_index Neuron to test
 * @return true if hub, false otherwise
 */
bool fractal_is_hub_neuron(const fractal_cognitive_cache_t *cache, uint32_t neuron_index);

/**
 * @brief Get nearest hub neuron to given neuron
 *
 * WHAT: Finds closest hub in network topology
 * WHY: Curiosity can route exploration through hubs
 * HOW: Breadth-first search from neuron to nearest hub
 *
 * @param network Neural network
 * @param cache Fractal cache
 * @param neuron_index Starting neuron
 * @param distance_out Output distance (can be NULL)
 * @return Hub neuron index, or UINT32_MAX if none found
 */
uint32_t fractal_nearest_hub(neural_network_t network,
                              const fractal_cognitive_cache_t *cache,
                              uint32_t neuron_index,
                              uint32_t *distance_out);

/**
 * @brief Get k most central neurons near a given neuron
 *
 * WHAT: Returns k neurons with highest centrality within radius
 * WHY: Knowledge module can anchor concepts to central neurons
 * HOW: Filter by distance, sort by centrality, return top k
 *
 * @param network Neural network
 * @param cache Fractal cache
 * @param neuron_index Center neuron
 * @param radius Search radius (hops)
 * @param k Number of central neurons to return
 * @param central_out Output array [k] (caller allocates)
 * @return Number of central neurons found
 */
uint32_t fractal_get_central_neighbors(neural_network_t network,
                                        const fractal_cognitive_cache_t *cache,
                                        uint32_t neuron_index,
                                        uint32_t radius,
                                        uint32_t k,
                                        uint32_t *central_out);

//=============================================================================
// Centrality Queries (for Attention, Ethics)
//=============================================================================

/**
 * @brief Get betweenness centrality for neuron
 *
 * WHAT: Returns cached centrality score
 * WHY: Attention can weight salience by centrality
 * HOW: Direct array lookup
 *
 * @param cache Fractal cache
 * @param neuron_index Neuron to query
 * @return Centrality score (0-1), or 0.0 if invalid
 */
float fractal_get_centrality(const fractal_cognitive_cache_t *cache, uint32_t neuron_index);

/**
 * @brief Get normalized degree for neuron
 *
 * WHAT: Returns degree / max_degree
 * WHY: Attention/Ethics can use as importance weight
 * HOW: Direct array lookup
 *
 * @param cache Fractal cache
 * @param neuron_index Neuron to query
 * @return Normalized degree (0-1), or 0.0 if invalid
 */
float fractal_get_degree_normalized(const fractal_cognitive_cache_t *cache, uint32_t neuron_index);

//=============================================================================
// Hierarchical Structure Queries (for Knowledge, Ethics)
//=============================================================================

/**
 * @brief Estimate hierarchical level of neuron
 *
 * WHAT: Returns approximate position in hierarchy (0=root, 1=leaf)
 * WHY: Knowledge can map hierarchical level to abstraction level
 * HOW: Centrality + degree heuristic
 *
 * FORMULA: level = 1 - sqrt(centrality * degree_normalized)
 *
 * @param cache Fractal cache
 * @param neuron_index Neuron to query
 * @return Hierarchical level (0-1)
 */
float fractal_get_hierarchical_level(const fractal_cognitive_cache_t *cache, uint32_t neuron_index);

/**
 * @brief Get neurons at specific hierarchical level
 *
 * WHAT: Returns all neurons within [level - tolerance, level + tolerance]
 * WHY: Curiosity can explore horizontally at same abstraction level
 * HOW: Filter by hierarchical_level estimate
 *
 * @param cache Fractal cache
 * @param level Target level (0-1)
 * @param tolerance Level tolerance (typical: 0.1)
 * @param neurons_out Output array (caller must free)
 * @param count_out Output count
 * @return true on success
 */
bool fractal_get_neurons_at_level(const fractal_cognitive_cache_t *cache,
                                   float level,
                                   float tolerance,
                                   uint32_t **neurons_out,
                                   uint32_t *count_out);

//=============================================================================
// Debug/Visualization
//=============================================================================

/**
 * @brief Print fractal cognitive properties summary
 *
 * @param cache Fractal cache
 */
void fractal_cognitive_print_summary(const fractal_cognitive_cache_t *cache);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FRACTAL_COGNITIVE_H
