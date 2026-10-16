//=============================================================================
// nimcp_brain_topology.h - Brain Topology/Graph Analysis Module
//=============================================================================
/**
 * @file nimcp_brain_topology.h
 * @brief Network topology analysis functions for brain module
 *
 * WHAT: Graph-theoretic analysis of brain network topology
 * WHY:  Understand functional organization, community structure, and hub neurons
 * HOW:  Community detection (Louvain), hub analysis, topology metrics
 *
 * EXTRACTED FROM: nimcp_brain.c (lines 7752-8150)
 *
 * FUNCTIONS:
 * - brain_detect_communities: Run community detection on brain network
 * - brain_get_neuron_community: Get community assignment for a neuron
 * - brain_detect_hubs: Identify hub neurons (high connectivity)
 * - brain_is_hub_neuron: Check if a neuron is a hub
 * - brain_compute_topology_metrics: Compute Q, C, L, σ metrics
 * - brain_validate_topology: Validate network health
 * - brain_get_network_analyzer: Get/create network analyzer for real-time analysis
 *
 * INTEGRATION:
 * - Uses core/topology/nimcp_community_detection.h for algorithms
 * - Uses utils/algorithms/nimcp_graph_metrics.h for metrics
 * - Uses utils/containers/nimcp_graph.h for graph data structures
 * - Works with consolidation, curiosity, meta-learning modules
 *
 * COMPLEXITY:
 * - Community detection: O(N log N) where N = number of neurons (Louvain)
 * - Hub detection: O(N + E) where E = number of synapses
 * - Topology metrics: O(N^2) for path length calculation
 */

#ifndef NIMCP_BRAIN_TOPOLOGY_H
#define NIMCP_BRAIN_TOPOLOGY_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Community Detection
//=============================================================================

/**
 * @brief Detect functional communities in brain network
 *
 * WHAT: Run Louvain algorithm to detect functional modules
 * WHY:  Understand how neurons organize into specialized groups
 * HOW:  Build graph from synapses, run community detection, store results
 *
 * ALGORITHM: Louvain method (greedy modularity optimization)
 * - Phase 1: Local optimization (move neurons to improve Q)
 * - Phase 2: Network aggregation (merge communities)
 * - Iterate until Q converges
 *
 * COMPLEXITY: O(N log N) where N = number of neurons
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_detect_communities(brain_t brain);

/**
 * @brief Get community assignment for a neuron
 *
 * WHAT: Query which functional module a neuron belongs to
 * WHY:  Understand neuron's functional role
 * HOW:  Lookup in community structure (must call brain_detect_communities first)
 *
 * COMPLEXITY: O(1)
 *
 * @param brain Brain instance
 * @param neuron_id Neuron index
 * @return Community ID (0 to num_communities-1), or UINT32_MAX on error
 */
NIMCP_EXPORT uint32_t brain_get_neuron_community(brain_t brain, uint32_t neuron_id);

//=============================================================================
// Hub Detection
//=============================================================================

/**
 * @brief Detect hub neurons (high connectivity)
 *
 * WHAT: Identify neurons with high degree centrality
 * WHY:  Find neurons critical for information flow
 * HOW:  Compute degree centrality, select top neurons above threshold
 *
 * ALGORITHM:
 * - Compute degree centrality for each neuron
 * - Normalize by max possible degree
 * - Select neurons with centrality > (mean + threshold*stddev)
 *
 * COMPLEXITY: O(N + E) where E = number of synapses
 *
 * @param brain Brain instance
 * @param threshold Standard deviations above mean (e.g., 0.7 = top ~25%)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_detect_hubs(brain_t brain, float threshold);

/**
 * @brief Check if a neuron is a hub
 *
 * WHAT: Query if neuron was identified as hub
 * WHY:  Fast lookup for hub status
 * HOW:  Linear search in hub array (must call brain_detect_hubs first)
 *
 * COMPLEXITY: O(num_hubs) where num_hubs << N
 *
 * @param brain Brain instance
 * @param neuron_id Neuron index
 * @return true if neuron is hub, false otherwise
 */
NIMCP_EXPORT bool brain_is_hub_neuron(brain_t brain, uint32_t neuron_id);

//=============================================================================
// Topology Metrics
//=============================================================================

/**
 * @brief Compute network topology metrics
 *
 * WHAT: Calculate modularity Q, clustering C, path length L, small-world σ
 * WHY:  Quantify network organization and efficiency
 * HOW:  Run topology validation with metrics computation
 *
 * METRICS:
 * - Modularity Q: How well network splits into communities [0, 1]
 * - Clustering C: Fraction of triangles in network [0, 1]
 * - Path length L: Average shortest path between neurons
 * - Small-world σ: Ratio (C/Crandom) / (L/Lrandom) > 1 for small-world
 *
 * COMPLEXITY: O(N^2) for all-pairs shortest paths
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_compute_topology_metrics(brain_t brain);

/**
 * @brief Validate network topology health
 *
 * WHAT: Check if network has healthy organization
 * WHY:  Detect pathological configurations (e.g., too fragmented)
 * HOW:  Run validation checks on metrics and structure
 *
 * VALIDATION CHECKS:
 * - Modularity Q > 0.25 (reasonable community structure)
 * - No isolated neurons
 * - Path length < infinity (network is connected)
 * - Hubs preserved (if previously detected)
 *
 * COMPLEXITY: O(N^2)
 *
 * @param brain Brain instance
 * @return true if topology is healthy, false if damaged
 */
NIMCP_EXPORT bool brain_validate_topology(brain_t brain);

//=============================================================================
// Network Analyzer API
//=============================================================================

/**
 * @brief Get or create network analyzer for real-time topology analysis
 *
 * WHAT: Lazy initialization of network analyzer
 * WHY:  Enable continuous monitoring of network topology during inference
 * HOW:  Create analyzer on first access, cache for reuse
 *
 * ALGORITHM:
 * 1. Validate brain handle and network existence
 * 2. Check if analyzer already exists (cached)
 * 3. If not cached, create new analyzer with network_analyzer_create()
 * 4. Configure analyzer for real-time monitoring:
 *    - Auto-analyze enabled with 10-iteration interval
 *    - Hub detection threshold at 0.7 (top 30% centrality)
 * 5. Run initial analysis to populate topology metrics
 * 6. Cache analyzer in brain structure for reuse
 * 7. Return analyzer pointer
 *
 * INTEGRATION:
 * - Used by consolidation (memory optimization based on communities)
 * - Used by curiosity (novelty from community emergence)
 * - Used by meta-learning (architecture search using topology)
 * - Used by quantum-Shannon (adaptive routing based on hubs)
 *
 * PERFORMANCE:
 * - O(1) after first creation (cached lookup)
 * - Initial creation: O(N + E) where N=neurons, E=synapses
 * - Initial analysis: O(N log N) for Louvain algorithm
 * - Typical overhead: 50-200ms for first call, <1μs for subsequent calls
 *
 * COMPLEXITY: O(1) cached, O(N + E) first call
 *
 * @param brain Brain instance
 * @return Network analyzer pointer (opaque), or NULL on error
 */
NIMCP_EXPORT void* brain_get_network_analyzer(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_TOPOLOGY_H
