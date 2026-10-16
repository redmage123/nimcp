/**
 * @file nimcp_louvain.h
 * @brief Louvain algorithm for community detection in graphs
 *
 * WHAT: Header for Louvain community detection algorithm
 * WHY: Efficiently identify modular structure in networks
 * HOW: Multi-phase greedy optimization maximizing modularity
 */

#ifndef NIMCP_LOUVAIN_H
#define NIMCP_LOUVAIN_H

#include "utils/containers/nimcp_graph.h"
#include "utils/containers/nimcp_vector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Community assignment structure
 */
typedef struct {
    uint32_t* assignments;      /**< Community ID for each vertex */
    uint32_t num_communities;   /**< Total number of communities found */
    double modularity;          /**< Modularity score of the partition */
    uint32_t iterations;        /**< Number of iterations to converge */
} NimcpCommunityPartition;

/**
 * @brief Detect communities using Louvain algorithm
 *
 * WHAT: Identifies modular structure in network
 * WHY: Maximize modularity through greedy optimization
 * HOW: Multi-phase approach with local optimization and network aggregation
 *
 * @param graph Input network
 * @param resolution Resolution parameter (typically 1.0)
 * @param seed Random seed for deterministic results
 * @return Community partition or NULL on failure
 */
NimcpCommunityPartition* nimcp_louvain_detect(const NimcpGraph* graph, double resolution, uint32_t seed);

/**
 * @brief Destroy community partition
 *
 * @param partition Partition to free
 */
void nimcp_community_partition_destroy(NimcpCommunityPartition* partition);

/**
 * @brief Get community ID for a vertex
 *
 * @param partition Community partition
 * @param vertex_idx Vertex index
 * @return Community ID or UINT32_MAX on error
 */
uint32_t nimcp_get_community_id(const NimcpCommunityPartition* partition, uint32_t vertex_idx);

/**
 * @brief Get members of a community
 *
 * WHAT: Extract all vertices in a specific community
 * WHY: Analyze community membership
 * HOW: Linear scan of assignments array
 *
 * @param partition Community partition
 * @param community_id Community to query
 * @param members Output array for member indices
 * @param max_members Size of members array
 * @return Number of members in community
 */
uint32_t nimcp_get_community_members(const NimcpCommunityPartition* partition,
                                      uint32_t community_id, uint32_t* members,
                                      uint32_t max_members);

/**
 * @brief Refine partition through additional iterations
 *
 * WHAT: Improve partition quality
 * WHY: Get closer to optimal solution
 * HOW: Continue optimization iterations on existing partition
 *
 * @param graph Input network
 * @param partition Existing partition to refine
 * @param additional_iterations Iterations to run
 * @return Updated partition or NULL on failure
 */
NimcpCommunityPartition* nimcp_louvain_refine(const NimcpGraph* graph,
                                              const NimcpCommunityPartition* partition,
                                              uint32_t additional_iterations);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOUVAIN_H */
