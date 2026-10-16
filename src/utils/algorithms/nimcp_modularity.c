/**
 * @file nimcp_modularity.c
 * @brief Modularity calculation implementation
 *
 * WHAT: Implements modularity metric for community detection
 * WHY: Evaluate partition quality objectively
 * HOW: Apply standard modularity formula to partitions
 */

#include "utils/algorithms/nimcp_modularity.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(modularity)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Calculate edge count and degrees within/between communities
 * WHY: Compute modularity components
 * HOW: Scan edges and classify by community membership
 */
static void calculate_community_stats(const NimcpGraph* graph, const uint32_t* assignments,
                                      uint32_t num_vertices, double* ecc, double* ac)
{
    if (!graph || !assignments || !ecc || !ac) return;

    memset(ecc, 0, sizeof(double) * num_vertices);
    memset(ac, 0, sizeof(double) * num_vertices);

    double total_weight = 0.0;

    // Count edges within and degree sums for each community
    for (uint32_t v = 0; v < num_vertices && v < graph->vertex_count; v++) {
        const NimcpVertex* vertex = &graph->vertices[v];
        uint32_t v_comm = assignments[v];

        for (NimcpEdgeNode* edge = vertex->edges; edge; edge = edge->next) {
            uint32_t u = edge->dest;
            if (u < num_vertices) {
                uint32_t u_comm = assignments[u];
                total_weight += edge->weight;

                if (v_comm == u_comm) {
                    ecc[v_comm] += edge->weight;
                }

                // Accumulate degree for modularity calculation
                ac[v_comm] += edge->weight;
            }
        }
    }

    if (total_weight > 0.0) {
        // Normalize
        for (uint32_t i = 0; i < num_vertices; i++) {
            ecc[i] /= total_weight;
            ac[i] /= (2.0 * total_weight);  // Factor of 2 for undirected graphs
        }
    }
}

/**
 * WHAT: Get unique community count
 * WHY: Know number of communities in partition
 * HOW: Track unique community IDs
 */
static uint32_t count_unique_communities(const uint32_t* assignments, uint32_t num_vertices)
{
    if (!assignments) return 0;

    bool* seen = (bool*)nimcp_malloc(sizeof(bool) * num_vertices);
    if (!seen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "count_unique_communities: seen is NULL");

            return 0;
    }

    memset(seen, false, sizeof(bool) * num_vertices);

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_vertices; i++) {
        uint32_t comm = assignments[i];
        if (comm < num_vertices && !seen[comm]) {
            seen[comm] = true;
            count++;
        }
    }

    nimcp_free(seen);
    return count;
}

//=============================================================================
// Public API Functions
//=============================================================================

double nimcp_calculate_modularity(const NimcpGraph* graph, const uint32_t* assignments,
                                  uint32_t num_vertices)
{
    return nimcp_calculate_modularity_with_resolution(graph, assignments, num_vertices, 1.0);
}

double nimcp_calculate_modularity_with_resolution(const NimcpGraph* graph,
                                                   const uint32_t* assignments,
                                                   uint32_t num_vertices,
                                                   double resolution)
{
    if (!graph || !assignments) return 0.0;

    if (num_vertices == 0 || graph->vertex_count == 0) {
        return 0.0;
    }

    uint32_t num_communities = count_unique_communities(assignments, num_vertices);
    if (num_communities == 0) {
        return 0.0;
    }

    // Allocate temporary arrays
    double* ecc = (double*)nimcp_malloc(sizeof(double) * num_communities);
    double* ac = (double*)nimcp_malloc(sizeof(double) * num_communities);

    if (!ecc || !ac) {
        nimcp_free(ecc);
        nimcp_free(ac);
        return 0.0;
    }

    calculate_community_stats(graph, assignments, num_vertices, ecc, ac);

    // Calculate modularity: Q = sum(ecc[c] - resolution * ac[c]^2)
    double modularity = 0.0;
    for (uint32_t c = 0; c < num_communities; c++) {
        modularity += ecc[c] - resolution * ac[c] * ac[c];
    }

    nimcp_free(ecc);
    nimcp_free(ac);

    return modularity;
}

bool nimcp_validate_partition(const uint32_t* assignments, uint32_t num_vertices,
                              uint32_t num_communities)
{
    if (!assignments) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_validate_partition: assignments is NULL");

            return false;

        }

    if (num_vertices == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_partition: num_vertices is zero");
        return false;
    }

    // Check all vertices are assigned
    for (uint32_t i = 0; i < num_vertices; i++) {
        if (assignments[i] >= num_communities) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_partition: capacity exceeded");
            return false;
        }
    }

    // Check all communities have at least one member
    bool* has_member = (bool*)nimcp_malloc(sizeof(bool) * num_communities);
    if (!has_member) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "if: has_member is NULL");

            return false;
    }

    memset(has_member, false, sizeof(bool) * num_communities);

    for (uint32_t i = 0; i < num_vertices; i++) {
        has_member[assignments[i]] = true;
    }

    bool valid = true;
    for (uint32_t c = 0; c < num_communities; c++) {
        if (!has_member[c]) {
            valid = false;
            break;
        }
    }

    nimcp_free(has_member);
    return valid;
}

uint32_t nimcp_count_communities(const uint32_t* assignments, uint32_t num_vertices)
{
    LOG_TRACE("Entering nimcp_count_communities");
    if (!assignments) return 0;

    return count_unique_communities(assignments, num_vertices);
}
