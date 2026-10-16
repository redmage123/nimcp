/**
 * @file nimcp_modularity.h
 * @brief Modularity calculations for community detection
 *
 * WHAT: Header for modularity metric calculations
 * WHY: Evaluate quality of network partitions
 * HOW: Compute modularity Q using standard formula
 */

#ifndef NIMCP_MODULARITY_H
#define NIMCP_MODULARITY_H

#include "utils/containers/nimcp_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate modularity of a partition
 *
 * WHAT: Compute modularity Q for a network partition
 * WHY: Quantify quality of community structure
 * HOW: Sum (ecc - ac^2) for each community c
 *
 * Q = 0 for random partition
 * Q > 0.3 indicates strong community structure
 * Q = 1 for perfect partitions (impossible in practice)
 *
 * @param graph Input network
 * @param assignments Community assignment for each vertex
 * @param num_vertices Number of vertices
 * @return Modularity score [-0.5, 1.0]
 */
double nimcp_calculate_modularity(const NimcpGraph* graph, const uint32_t* assignments,
                                  uint32_t num_vertices);

/**
 * @brief Calculate modularity with resolution parameter
 *
 * WHAT: Modularity with tunable resolution
 * WHY: Control hierarchical level detection
 * HOW: Modify modularity formula with resolution parameter
 *
 * @param graph Input network
 * @param assignments Community assignment for each vertex
 * @param num_vertices Number of vertices
 * @param resolution Resolution parameter (typically 0.5 to 2.0, default 1.0)
 * @return Modularity score
 */
double nimcp_calculate_modularity_with_resolution(const NimcpGraph* graph,
                                                   const uint32_t* assignments,
                                                   uint32_t num_vertices,
                                                   double resolution);

/**
 * @brief Validate partition integrity
 *
 * WHAT: Check if partition is valid
 * WHY: Ensure no vertices are unassigned
 * HOW: Verify all vertices have community assignments
 *
 * @param assignments Community assignments
 * @param num_vertices Number of vertices
 * @param num_communities Expected number of communities
 * @return true if partition is valid
 */
bool nimcp_validate_partition(const uint32_t* assignments, uint32_t num_vertices,
                              uint32_t num_communities);

/**
 * @brief Get number of unique communities
 *
 * WHAT: Count distinct communities in partition
 * WHY: Verify partition structure
 * HOW: Track unique community IDs
 *
 * @param assignments Community assignments
 * @param num_vertices Number of vertices
 * @return Number of unique communities
 */
uint32_t nimcp_count_communities(const uint32_t* assignments, uint32_t num_vertices);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MODULARITY_H */
