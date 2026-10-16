/**
 * @file nimcp_centrality.h
 * @brief Network centrality measures for identifying important nodes
 *
 * WHAT: Header for centrality metric calculations
 * WHY: Identify hub nodes and important network positions
 * HOW: Implement multiple centrality measures
 */

#ifndef NIMCP_CENTRALITY_H
#define NIMCP_CENTRALITY_H

#include "utils/containers/nimcp_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Centrality scores structure
 */
typedef struct {
    double* scores;      /**< Centrality score for each vertex */
    uint32_t num_scores; /**< Number of scores (should equal vertex_count) */
} NimcpCentralityScores;

/**
 * @brief Calculate degree centrality
 *
 * WHAT: Measure based on vertex degree
 * WHY: Simple, fast measure of node importance
 * HOW: Normalize degree by (n-1)
 *
 * Degree centrality = degree(v) / (n-1)
 *
 * @param graph Input network
 * @return Centrality scores or NULL on failure
 */
NimcpCentralityScores* nimcp_degree_centrality(const NimcpGraph* graph);

/**
 * @brief Calculate betweenness centrality
 *
 * WHAT: Measure of control over paths between other nodes
 * WHY: Identify bridge nodes crucial for network connectivity
 * HOW: Count shortest paths passing through each vertex
 *
 * Betweenness centrality = sum of (shortest paths through v) / (total shortest paths)
 *
 * @param graph Input network
 * @return Centrality scores or NULL on failure
 */
NimcpCentralityScores* nimcp_betweenness_centrality(const NimcpGraph* graph);

/**
 * @brief Calculate closeness centrality
 *
 * WHAT: Measure of average distance to all other nodes
 * WHY: Identify central nodes with short paths to all others
 * HOW: Inverse of average shortest path length
 *
 * Closeness centrality = (n-1) / sum(distances)
 *
 * @param graph Input network
 * @return Centrality scores or NULL on failure
 */
NimcpCentralityScores* nimcp_closeness_centrality(const NimcpGraph* graph);

/**
 * @brief Calculate eigenvector centrality
 *
 * WHAT: Recursive measure based on neighbors' importance
 * WHY: Identify nodes connected to other important nodes
 * HOW: Iterative power method approximation
 *
 * @param graph Input network
 * @param max_iterations Maximum iterations for convergence
 * @return Centrality scores or NULL on failure
 */
NimcpCentralityScores* nimcp_eigenvector_centrality(const NimcpGraph* graph,
                                                     uint32_t max_iterations);

/**
 * @brief Detect hub nodes
 *
 * WHAT: Identify nodes with significantly high centrality
 * WHY: Find most important network nodes
 * HOW: Find nodes above mean + k*stdev in centrality distribution
 *
 * @param scores Centrality scores
 * @param threshold Standard deviations above mean (typically 1.0-2.0)
 * @param hubs Output array for hub indices
 * @param max_hubs Size of hubs array
 * @return Number of hubs found
 */
uint32_t nimcp_detect_hubs(const NimcpCentralityScores* scores, double threshold,
                            uint32_t* hubs, uint32_t max_hubs);

/**
 * @brief Destroy centrality scores
 *
 * @param scores Scores to free
 */
void nimcp_centrality_scores_destroy(NimcpCentralityScores* scores);

/**
 * @brief Get score for specific vertex
 *
 * @param scores Centrality scores
 * @param vertex_idx Vertex index
 * @return Centrality score or -1.0 on error
 */
double nimcp_get_centrality_score(const NimcpCentralityScores* scores, uint32_t vertex_idx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CENTRALITY_H */
