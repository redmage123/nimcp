/**
 * @file nimcp_louvain.c
 * @brief Louvain algorithm implementation for community detection
 *
 * WHAT: Implements Louvain community detection algorithm
 * WHY: Efficiently find modular structure in networks
 * HOW: Greedy optimization of modularity in two phases
 */

#include "utils/algorithms/nimcp_louvain.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(louvain)

//=============================================================================
// Internal Types
//=============================================================================

/**
 * WHAT: Intermediate data structure for Louvain algorithm
 * WHY: Track community assignments and modularity gains
 * HOW: Store current assignments and edge weights per community
 */
typedef struct {
    uint32_t* assignments;           /**< Current community assignments */
    double* community_weights;       /**< Total weight in each community */
    uint32_t* community_sizes;       /**< Size of each community */
    uint32_t num_communities;        /**< Current number of communities */
    uint32_t max_vertex;             /**< Maximum vertex index */
    double total_weight;             /**< Sum of all edge weights */
    uint32_t* neighbor_communities;  /**< Temporary array for neighbor communities */
    double* neighbor_weights;        /**< Weights to neighbor communities */
} LouvainContext;

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Calculate modularity gain for moving vertex
 * WHY: Evaluate community reassignment quality
 * HOW: Compute change in modularity Q after move
 */
static inline double calculate_modularity_gain(LouvainContext* ctx, uint32_t vertex,
                                               uint32_t new_comm, uint32_t old_comm,
                                               const NimcpGraph* graph, double resolution)
{
    if (!ctx || !graph) return 0.0;

    if (vertex >= ctx->max_vertex) {
        return 0.0;
    }

    const NimcpVertex* v = &graph->vertices[vertex];
    if (!v) return 0.0;

    // Get neighbors and their communities
    double internal_edges = 0.0;
    for (NimcpEdgeNode* edge = v->edges; edge; edge = edge->next) {
        uint32_t neighbor = edge->dest;
        if (neighbor < ctx->max_vertex && ctx->assignments[neighbor] == new_comm) {
            internal_edges += edge->weight;
        }
    }

    // Modularity gain formula
    if (ctx->community_weights[new_comm] <= 0.0) {
        return 0.0;
    }

    double gain = (internal_edges / ctx->total_weight) -
                  resolution * (ctx->community_weights[new_comm] * v->edge_count) /
                      (ctx->total_weight * ctx->total_weight);

    return gain;
}

/**
 * WHAT: Initialize community assignments (each vertex is its own community)
 * WHY: Provide starting point for optimization
 * HOW: Set assignment[i] = i for all vertices
 */
static LouvainContext* louvain_context_create(const NimcpGraph* graph)
{
    if (!graph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;

    }
    if (!graph->vertices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "louvain_context_create: graph->vertices is NULL");
        return NULL;
    }

    if (graph->vertex_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "louvain_context_create: graph->vertex_count is zero");
        return NULL;
    }

    LouvainContext* ctx = (LouvainContext*)nimcp_malloc(sizeof(LouvainContext));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    ctx->max_vertex = graph->vertex_count;
    ctx->num_communities = graph->vertex_count;
    ctx->total_weight = 0.0;

    // Allocate arrays
    ctx->assignments = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * ctx->max_vertex);
    ctx->community_weights = (double*)nimcp_malloc(sizeof(double) * ctx->max_vertex);
    ctx->community_sizes = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * ctx->max_vertex);
    ctx->neighbor_communities = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * ctx->max_vertex);
    ctx->neighbor_weights = (double*)nimcp_malloc(sizeof(double) * ctx->max_vertex);

    if (!ctx->assignments || !ctx->community_weights || !ctx->community_sizes ||
        !ctx->neighbor_communities || !ctx->neighbor_weights) {
        nimcp_free(ctx->assignments);
        nimcp_free(ctx->community_weights);
        nimcp_free(ctx->community_sizes);
        nimcp_free(ctx->neighbor_communities);
        nimcp_free(ctx->neighbor_weights);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "louvain_context_create: operation failed");
        return NULL;
    }

    // Initialize each vertex as its own community
    memset(ctx->community_weights, 0, sizeof(double) * ctx->max_vertex);
    memset(ctx->community_sizes, 0, sizeof(uint32_t) * ctx->max_vertex);

    for (uint32_t i = 0; i < ctx->max_vertex; i++) {
        ctx->assignments[i] = i;
        ctx->community_sizes[i] = 1;
        ctx->community_weights[i] = (double)graph->vertices[i].edge_count;
        ctx->total_weight += graph->vertices[i].edge_count;
    }

    // Calculate total weight (sum of degrees)
    if (ctx->total_weight == 0.0) {
        ctx->total_weight = 1.0;  // Avoid division by zero
    }

    return ctx;
}

/**
 * WHAT: Free Louvain context
 * WHY: Cleanup intermediate structures
 * HOW: Free all allocated arrays and structure
 */
static void louvain_context_destroy(LouvainContext* ctx)
{
    if (!ctx) {
        return;
    }

    nimcp_free(ctx->assignments);
    nimcp_free(ctx->community_weights);
    nimcp_free(ctx->community_sizes);
    nimcp_free(ctx->neighbor_communities);
    nimcp_free(ctx->neighbor_weights);
    nimcp_free(ctx);
}

/**
 * WHAT: Execute one optimization phase
 * WHY: Greedily improve modularity
 * HOW: For each vertex, find best community to move to
 */
static bool louvain_optimization_phase(LouvainContext* ctx, const NimcpGraph* graph,
                                       double resolution, uint32_t* improvements)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "louvain_optimization_phase: ctx is NULL");
        return false;
    }
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "louvain_optimization_phase: graph is NULL");
        return false;
    }
    if (!improvements) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "louvain_optimization_phase: improvements is NULL");
        return false;
    }

    *improvements = 0;

    for (uint32_t v = 0; v < ctx->max_vertex; v++) {
        if (v >= graph->vertex_count || !graph->vertices) {
            continue;
        }

        uint32_t old_comm = ctx->assignments[v];

        // Find best community for this vertex
        double best_gain = 0.0;
        uint32_t best_comm = old_comm;

        // Try moving to each neighbor's community
        memset(ctx->neighbor_communities, 0xff, sizeof(uint32_t) * ctx->max_vertex);
        memset(ctx->neighbor_weights, 0, sizeof(double) * ctx->max_vertex);

        const NimcpVertex* vertex = &graph->vertices[v];
        for (NimcpEdgeNode* edge = vertex->edges; edge; edge = edge->next) {
            uint32_t neighbor = edge->dest;
            if (neighbor < ctx->max_vertex) {
                uint32_t neighbor_comm = ctx->assignments[neighbor];
                if (neighbor_comm < ctx->max_vertex) {
                    ctx->neighbor_weights[neighbor_comm] += edge->weight;
                }
            }
        }

        // Evaluate each neighbor community
        for (uint32_t c = 0; c < ctx->max_vertex; c++) {
            if (ctx->community_weights[c] <= 0.0) {
                continue;
            }

            double gain = calculate_modularity_gain(ctx, v, c, old_comm, graph, resolution);
            if (gain > best_gain) {
                best_gain = gain;
                best_comm = c;
            }
        }

        // Move vertex if improvement found
        if (best_comm != old_comm) {
            ctx->assignments[v] = best_comm;
            ctx->community_sizes[old_comm]--;
            ctx->community_sizes[best_comm]++;
            (*improvements)++;
        }
    }

    return *improvements > 0;
}

/**
 * WHAT: Compact communities (renumber to remove gaps)
 * WHY: Create standard numbering for result
 * HOW: Map old community IDs to new sequential IDs
 */
static uint32_t louvain_compact_communities(LouvainContext* ctx)
{
    NIMCP_VALIDATE(ctx, 0);

    // Find used communities
    uint32_t* community_map = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * ctx->max_vertex);
    if (!community_map) {
        return 0;
    }

    memset(community_map, 0xff, sizeof(uint32_t) * ctx->max_vertex);

    uint32_t new_id = 0;
    for (uint32_t i = 0; i < ctx->max_vertex; i++) {
        if (ctx->community_sizes[i] > 0) {
            community_map[i] = new_id++;
        }
    }

    // Remap assignments
    for (uint32_t i = 0; i < ctx->max_vertex; i++) {
        uint32_t old_comm = ctx->assignments[i];
        if (old_comm < ctx->max_vertex) {
            ctx->assignments[i] = community_map[old_comm];
        }
    }

    nimcp_free(community_map);
    ctx->num_communities = new_id;
    return new_id;
}

/**
 * WHAT: Calculate modularity of current partition
 * WHY: Measure quality of community structure
 * HOW: Sum modularity contributions of all edges
 */
static double calculate_modularity(LouvainContext* ctx, const NimcpGraph* graph)
{
    NIMCP_VALIDATE(ctx, 0.0);
    NIMCP_VALIDATE(graph, 0.0);

    double modularity = 0.0;

    if (ctx->total_weight <= 0.0) {
        return 0.0;
    }

    for (uint32_t v = 0; v < ctx->max_vertex && v < graph->vertex_count; v++) {
        const NimcpVertex* vertex = &graph->vertices[v];
        uint32_t v_comm = ctx->assignments[v];

        for (NimcpEdgeNode* edge = vertex->edges; edge; edge = edge->next) {
            uint32_t u = edge->dest;
            if (u < ctx->max_vertex) {
                uint32_t u_comm = ctx->assignments[u];

                if (v_comm == u_comm) {
                    double edge_weight = edge->weight / ctx->total_weight;
                    modularity += edge_weight;
                }
            }
        }
    }

    return modularity;
}

//=============================================================================
// Public API Functions
//=============================================================================

NimcpCommunityPartition* nimcp_louvain_detect(const NimcpGraph* graph, double resolution,
                                              uint32_t seed)
{
    if (!graph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;

    }
    if (!graph->vertices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_louvain_detect: graph->vertices is NULL");
        return NULL;
    }

    if (graph->vertex_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_louvain_detect: graph->vertex_count is zero");
        return NULL;
    }

    // Create context
    LouvainContext* ctx = louvain_context_create(graph);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    // Multi-phase optimization
    uint32_t phase = 0;
    uint32_t improvements = 0;
    const uint32_t MAX_PHASES = 10;

    while (phase < MAX_PHASES) {
        improvements = 0;

        if (!louvain_optimization_phase(ctx, graph, resolution, &improvements)) {
            break;
        }

        phase++;
    }

    // Compact communities
    louvain_compact_communities(ctx);

    // Create result structure
    NimcpCommunityPartition* result =
        (NimcpCommunityPartition*)nimcp_malloc(sizeof(NimcpCommunityPartition));
    if (!result) {
        louvain_context_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_louvain_detect: result is NULL");
        return NULL;
    }

    result->assignments = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * ctx->max_vertex);
    if (!result->assignments) {
        nimcp_free(result);
        louvain_context_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_louvain_detect: result->assignments is NULL");
        return NULL;
    }

    memcpy(result->assignments, ctx->assignments, sizeof(uint32_t) * ctx->max_vertex);
    result->num_communities = ctx->num_communities;
    result->modularity = calculate_modularity(ctx, graph);
    result->iterations = phase;

    louvain_context_destroy(ctx);

    return result;
}

void nimcp_community_partition_destroy(NimcpCommunityPartition* partition)
{
    LOG_TRACE("Entering nimcp_community_partition_destroy");
    if (!partition) {
        return;
    }

    nimcp_free(partition->assignments);
    nimcp_free(partition);
}

uint32_t nimcp_get_community_id(const NimcpCommunityPartition* partition, uint32_t vertex_idx)
{
    LOG_TRACE("Entering nimcp_get_community_id");
    if (!partition || !partition->assignments) {
        return UINT32_MAX;
    }

    return partition->assignments[vertex_idx];
}

uint32_t nimcp_get_community_members(const NimcpCommunityPartition* partition,
                                      uint32_t community_id, uint32_t* members,
                                      uint32_t max_members)
{
    if (!partition || !members) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < partition->num_communities && count < max_members; i++) {
        if (partition->assignments[i] == community_id) {
            members[count++] = i;
        }
    }

    return count;
}

NimcpCommunityPartition* nimcp_louvain_refine(const NimcpGraph* graph,
                                              const NimcpCommunityPartition* partition,
                                              uint32_t additional_iterations)
{
    if (!graph || !partition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_louvain_refine: required parameter is NULL (graph, partition)");
        return NULL;
    }

    // Create new partition (copy of input)
    NimcpCommunityPartition* result =
        (NimcpCommunityPartition*)nimcp_malloc(sizeof(NimcpCommunityPartition));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    result->assignments = (uint32_t*)nimcp_malloc(sizeof(uint32_t) * graph->vertex_count);
    if (!result->assignments) {
        nimcp_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_louvain_refine: result->assignments is NULL");
        return NULL;
    }

    memcpy(result->assignments, partition->assignments,
           sizeof(uint32_t) * graph->vertex_count);
    result->num_communities = partition->num_communities;
    result->modularity = partition->modularity;
    result->iterations = partition->iterations;

    // Continue optimization for specified iterations
    LouvainContext* ctx = louvain_context_create(graph);
    if (!ctx) {
        nimcp_free(result->assignments);
        nimcp_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_louvain_refine: ctx is NULL");
        return NULL;
    }

    memcpy(ctx->assignments, partition->assignments, sizeof(uint32_t) * graph->vertex_count);

    for (uint32_t i = 0; i < additional_iterations; i++) {
        uint32_t improvements = 0;
        if (!louvain_optimization_phase(ctx, graph, 1.0, &improvements)) {
            break;
        }
        result->iterations++;
    }

    memcpy(result->assignments, ctx->assignments, sizeof(uint32_t) * graph->vertex_count);
    result->modularity = calculate_modularity(ctx, graph);

    louvain_context_destroy(ctx);

    return result;
}
