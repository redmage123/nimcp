/**
 * @file nimcp_graph_metrics.h
 * @brief Network analysis metrics for brain topology validation
 *
 * WHAT: Graph-theoretic measures for analyzing brain network topology
 * WHY: Validate brain structure against biological constraints (modularity, small-world)
 * HOW: Newman's modularity Q, clustering coefficient, path length, assortativity
 *
 * BIOLOGY:
 * - Real brains: Q ≈ 0.3-0.5 (highly modular)
 * - Real brains: Small-world (high clustering, low path length)
 * - σ = (C/C_random) / (L/L_random) > 1 indicates small-world property
 * - Assortativity: r > 0 means hubs connect to hubs (not typical in brain)
 *
 * REFERENCES:
 * - Newman & Girvan (2004): Finding and evaluating community structure
 * - Watts & Strogatz (1998): Collective dynamics of small-world networks
 * - Bullmore & Sporns (2009): Complex brain networks
 */

#ifndef NIMCP_GRAPH_METRICS_H
#define NIMCP_GRAPH_METRICS_H

#include "utils/containers/nimcp_graph.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Comprehensive graph metrics for network analysis
 *
 * WHAT: Collection of topology metrics describing graph structure
 * WHY: Quantify network properties for validation and optimization
 */
typedef struct {
    float modularity;                  /**< Newman's Q: -0.5 to 1.0, brain ~0.3-0.5 */
    float clustering_coefficient;      /**< Average C: 0 to 1.0, brain ~0.4-0.6 */
    float characteristic_path_length;  /**< Average L: >1, brain ~2-4 */
    float small_world_coefficient;     /**< σ = (C/C_rand) / (L/L_rand), brain >1 */
    uint32_t diameter;                 /**< Longest shortest path in graph */
    float assortativity;               /**< Degree correlation: -1 to 1, brain ~0 */
} graph_metrics_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Compute all graph metrics in one pass
 *
 * WHAT: Calculates comprehensive set of network topology metrics
 * WHY: Single call for complete graph characterization
 * HOW: Computes modularity, clustering, path length, diameter, assortativity
 *
 * @param graph Network topology to analyze (must be non-NULL)
 * @return Allocated metrics structure or NULL on error
 *
 * USAGE:
 *   graph_metrics_t* metrics = compute_graph_metrics(graph);
 *   if (metrics) {
 *       printf("Modularity: %.3f\\n", metrics->modularity);
 *       graph_metrics_destroy(metrics);
 *   }
 *
 * NOTE: Caller must free returned metrics with graph_metrics_destroy()
 */
graph_metrics_t* compute_graph_metrics(NimcpGraph* graph);

/**
 * @brief Free graph metrics structure
 *
 * WHAT: Deallocates metrics structure
 * WHY: Clean memory management
 *
 * @param metrics Structure to free (NULL-safe)
 */
void graph_metrics_destroy(graph_metrics_t* metrics);

//=============================================================================
// Individual Metric Functions
//=============================================================================

/**
 * @brief Compute Newman's modularity Q
 *
 * WHAT: Measures strength of community structure in network
 * WHY: Quantify how well network divides into modules/communities
 * HOW: Q = (1/2m) * Σ[A_ij - (k_i*k_j)/(2m)] * δ(c_i, c_j)
 *      where m = edges, A_ij = adjacency, k_i = degree, c_i = community
 *
 * BIOLOGY: Real brains have Q ≈ 0.3-0.5 (highly modular organization)
 *
 * @param graph Network to analyze
 * @param communities Array of community labels (one per vertex)
 * @return Modularity Q in range [-0.5, 1.0], or -1.0 on error
 *
 * INTERPRETATION:
 * - Q > 0.3: Strong community structure (typical for brain)
 * - Q ≈ 0.0: Random network (no communities)
 * - Q < 0.0: Anti-modular (shouldn't happen in brain)
 */
float compute_modularity_q(NimcpGraph* graph, uint32_t* communities);

/**
 * @brief Compute average clustering coefficient
 *
 * WHAT: Measures local density of connections (how connected neighbors are)
 * WHY: Quantify local network cohesion
 * HOW: C = (1/n) * Σ[2*T_i / (k_i*(k_i-1))]
 *      where T_i = triangles involving node i, k_i = degree
 *
 * BIOLOGY: Real brains have C ≈ 0.4-0.6 (high local clustering)
 *
 * @param graph Network to analyze
 * @return Clustering coefficient in range [0, 1], or -1.0 on error
 *
 * INTERPRETATION:
 * - C = 1.0: Complete graph (fully connected)
 * - C ≈ 0.5: Brain-like (neighbors tend to connect)
 * - C = 0.0: Tree/star (no triangles)
 */
float compute_clustering_coefficient(NimcpGraph* graph);

/**
 * @brief Compute average shortest path length
 *
 * WHAT: Measures typical distance between any two nodes
 * WHY: Quantify global communication efficiency
 * HOW: L = (1/(n*(n-1))) * Σ_i Σ_j d(i,j)
 *      where d(i,j) = shortest path from i to j
 *
 * BIOLOGY: Real brains have L ≈ 2-4 (short paths despite size)
 *
 * @param graph Network to analyze
 * @return Average path length, or -1.0 on error
 *
 * INTERPRETATION:
 * - L ≈ 2: Very efficient (brain-like)
 * - L ≈ log(N): Random network
 * - L ≈ N: Lattice/ring (poor communication)
 *
 * COMPLEXITY: O(V^3) using Floyd-Warshall
 */
float compute_characteristic_path_length(NimcpGraph* graph);

/**
 * @brief Compute degree assortativity coefficient
 *
 * WHAT: Measures tendency of high-degree nodes to connect to each other
 * WHY: Identify hub organization patterns
 * HOW: r = Pearson correlation of degrees at edge endpoints
 *
 * BIOLOGY: Real brains have r ≈ 0 (hubs don't preferentially connect)
 *
 * @param graph Network to analyze
 * @return Assortativity in range [-1, 1], or -2.0 on error
 *
 * INTERPRETATION:
 * - r > 0: Assortative (hubs connect to hubs) - NOT brain-like
 * - r ≈ 0: Neutral mixing - typical for brain
 * - r < 0: Disassortative (hubs avoid hubs)
 */
float compute_assortativity(NimcpGraph* graph);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPH_METRICS_H */
