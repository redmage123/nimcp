/**
 * @file nimcp_graph_metrics.c
 * @brief Implementation of network topology metrics for brain validation
 *
 * WHAT: Graph-theoretic measures (modularity, clustering, path length, assortativity)
 * WHY: Validate brain topology against biological constraints
 * HOW: Full implementations of Newman's Q, Watts-Strogatz metrics, degree correlation
 *
 * ALGORITHMS:
 * - Modularity: O(E) - single pass over edges
 * - Clustering: O(V*k^2) - count triangles per vertex
 * - Path Length: O(V^3) - Floyd-Warshall all-pairs shortest paths
 * - Assortativity: O(E) - Pearson correlation over edges
 *
 * OPTIMIZATIONS:
 * - Early termination for invalid graphs
 * - Cached degree calculations
 * - Symmetric adjacency handling
 */

#include "utils/algorithms/nimcp_graph_metrics.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graph_metrics)

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Count triangles involving a vertex
 *
 * WHAT: Counts closed triplets (A-B-C-A) containing vertex
 * WHY: Needed for clustering coefficient calculation
 * HOW: For each pair of neighbors, check if they're connected
 *
 * COMPLEXITY: O(k^2) where k = degree of vertex
 */
static uint32_t count_triangles(NimcpGraph* graph, uint32_t vertex)
{
    if (!graph || vertex >= graph->vertex_count) {
        return 0;
    }

    // Get all neighbors of this vertex
    uint32_t degree = graph->vertices[vertex].edge_count;
    if (degree < 2) {
        return 0;  // Need at least 2 neighbors to form triangle
    }

    uint32_t* neighbors = (uint32_t*)nimcp_malloc(degree * sizeof(uint32_t));
    if (!neighbors) {
        return 0;
    }

    // Collect neighbors
    uint32_t neighbor_count = 0;
    NimcpEdgeNode* edge = graph->vertices[vertex].edges;
    while (edge && neighbor_count < degree) {
        neighbors[neighbor_count++] = edge->dest;
        edge = edge->next;
    }

    // Count triangles: for each pair of neighbors, check if edge exists
    uint32_t triangles = 0;
    for (uint32_t i = 0; i < neighbor_count; i++) {
        for (uint32_t j = i + 1; j < neighbor_count; j++) {
            uint32_t n1 = neighbors[i];
            uint32_t n2 = neighbors[j];

            // Check if edge exists between neighbors
            NimcpEdgeNode* e = graph->vertices[n1].edges;
            while (e) {
                if (e->dest == n2) {
                    triangles++;
                    break;
                }
                e = e->next;
            }
        }
    }

    nimcp_free(neighbors);
    return triangles;
}

/**
 * @brief Get degree of vertex
 *
 * WHAT: Returns number of edges connected to vertex
 * WHY: Cached degree for efficient metric calculations
 */
static inline uint32_t get_degree(NimcpGraph* graph, uint32_t vertex)
{
    if (!graph || vertex >= graph->vertex_count) {
        return 0;
    }
    return graph->vertices[vertex].edge_count;
}

/**
 * @brief Check if edge exists between vertices
 *
 * WHAT: Tests for presence of directed edge
 * WHY: Triangle counting and adjacency queries
 */
static bool has_edge(NimcpGraph* graph, uint32_t from, uint32_t to)
{
    if (!graph || from >= graph->vertex_count || to >= graph->vertex_count) {
        return false;
    }

    NimcpEdgeNode* edge = graph->vertices[from].edges;
    while (edge) {
        if (edge->dest == to) {
            return true;
        }
        edge = edge->next;
    }
    return false;
}

//=============================================================================
// Public API Implementation
//=============================================================================

/**
 * WHAT: Computes Newman's modularity Q for community structure
 * WHY: Quantify strength of network division into modules
 * HOW: Q = (1/2m) * Σ[A_ij - (k_i*k_j)/(2m)] * δ(c_i, c_j)
 *
 * FORMULA BREAKDOWN:
 * - A_ij = 1 if edge exists, 0 otherwise
 * - k_i, k_j = degrees of vertices i, j
 * - 2m = total edge count (doubled for undirected)
 * - δ(c_i, c_j) = 1 if same community, 0 otherwise
 *
 * INTERPRETATION:
 * - Q > 0.3: Strong modularity (brain-like)
 * - Q ≈ 0.0: Random structure
 * - Q < 0.0: Less modular than random
 */
float compute_modularity_q(NimcpGraph* graph, uint32_t* communities)
{
    if (!graph || !communities) {
        return -1.0F;
    }

    if (graph->vertex_count == 0 || graph->edge_count == 0) {
        return 0.0F;  // Empty graph has zero modularity
    }

    // 2m = twice the number of edges (for undirected graphs)
    float two_m = 2.0F * (float)graph->edge_count;

    float q_sum = 0.0F;

    // Iterate over all vertex pairs
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        uint32_t k_i = get_degree(graph, i);

        for (uint32_t j = 0; j < graph->vertex_count; j++) {
            // Only consider vertices in same community
            if (communities[i] != communities[j]) {
                continue;
            }

            uint32_t k_j = get_degree(graph, j);

            // A_ij = 1 if edge exists, 0 otherwise
            float a_ij = has_edge(graph, i, j) ? 1.0F : 0.0F;

            // Expected edges under random null model
            float expected = (float)k_i * (float)k_j / two_m;

            // Accumulate Q
            q_sum += a_ij - expected;
        }
    }

    // Normalize by 2m
    float Q = q_sum / two_m;

    return Q;
}

/**
 * WHAT: Computes average clustering coefficient
 * WHY: Measure local network cohesion (neighbor connectivity)
 * HOW: C = (1/n) * Σ[2*T_i / (k_i*(k_i-1))]
 *
 * ALGORITHM:
 * 1. For each vertex i with degree k_i >= 2:
 * 2.   Count triangles T_i (closed triplets)
 * 3.   Local clustering C_i = 2*T_i / (k_i*(k_i-1))
 * 4. Average all local clustering values
 *
 * COMPLEXITY: O(V*k^2) where k = average degree
 */
float compute_clustering_coefficient(NimcpGraph* graph)
{
    if (!graph) {
        return -1.0F;
    }

    if (graph->vertex_count == 0) {
        return 0.0F;
    }

    float total_clustering = 0.0F;
    uint32_t counted_vertices = 0;

    // Compute local clustering for each vertex
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        uint32_t k = get_degree(graph, i);

        if (k < 2) {
            continue;  // Need at least 2 neighbors for clustering
        }

        uint32_t triangles = count_triangles(graph, i);

        // Local clustering: C_i = 2*T_i / (k_i*(k_i-1))
        float max_triangles = (float)k * (float)(k - 1);
        float local_clustering = (2.0F * (float)triangles) / max_triangles;

        total_clustering += local_clustering;
        counted_vertices++;
    }

    // Return average clustering
    if (counted_vertices == 0) {
        return 0.0F;
    }

    return total_clustering / (float)counted_vertices;
}

/**
 * WHAT: Computes average shortest path length (characteristic path length)
 * WHY: Measure global communication efficiency
 * HOW: Floyd-Warshall all-pairs shortest paths, then average
 *
 * ALGORITHM:
 * 1. Initialize distance matrix (0 for diagonal, 1 for edges, ∞ otherwise)
 * 2. Floyd-Warshall: for k, i, j: d[i][j] = min(d[i][j], d[i][k] + d[k][j])
 * 3. Average all finite distances
 *
 * COMPLEXITY: O(V^3) time, O(V^2) space
 *
 * NOTE: Uses unweighted distances (each edge = 1 hop)
 */
float compute_characteristic_path_length(NimcpGraph* graph)
{
    if (!graph) {
        return -1.0F;
    }

    uint32_t n = graph->vertex_count;
    if (n == 0) {
        return 0.0F;
    }

    // Integer overflow check for n * n * sizeof(float)
    if ((size_t)n * n > SIZE_MAX / sizeof(float)) {
        return -1.0F;
    }

    // Allocate distance matrix (flattened 2D array)
    float* dist = (float*)nimcp_malloc((size_t)n * n * sizeof(float));
    if (!dist) {
        return -1.0F;
    }

    // Initialize distance matrix
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i == j) {
                dist[i * n + j] = 0.0F;  // Distance to self = 0
            } else if (has_edge(graph, i, j)) {
                dist[i * n + j] = 1.0F;  // Direct edge = distance 1
            } else {
                dist[i * n + j] = FLT_MAX;  // No path initially
            }
        }
    }

    // Floyd-Warshall: all-pairs shortest paths
    for (uint32_t k = 0; k < n; k++) {
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                float dist_ik = dist[i * n + k];
                float dist_kj = dist[k * n + j];

                if (dist_ik != FLT_MAX && dist_kj != FLT_MAX) {
                    float new_dist = dist_ik + dist_kj;
                    if (new_dist < dist[i * n + j]) {
                        dist[i * n + j] = new_dist;
                    }
                }
            }
        }
    }

    // Compute average of all finite distances (excluding diagonal)
    float sum = 0.0F;
    uint32_t count = 0;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            if (i != j && dist[i * n + j] != FLT_MAX) {
                sum += dist[i * n + j];
                count++;
            }
        }
    }

    nimcp_free(dist);

    if (count == 0) {
        return 0.0F;  // Disconnected graph
    }

    return sum / (float)count;
}

/**
 * WHAT: Computes degree assortativity coefficient
 * WHY: Measure tendency of hubs to connect to other hubs
 * HOW: Pearson correlation of degrees at edge endpoints
 *
 * FORMULA:
 * r = [Σ_e (j_e * k_e) - (1/m) * Σ_e j_e * Σ_e k_e] /
 *     sqrt([Σ_e j_e^2 - (1/m)*(Σ_e j_e)^2] * [Σ_e k_e^2 - (1/m)*(Σ_e k_e)^2])
 *
 * where j_e, k_e are degrees at endpoints of edge e
 *
 * INTERPRETATION:
 * - r > 0: Assortative (hubs connect to hubs)
 * - r = 0: Neutral (brain-like)
 * - r < 0: Disassortative (hubs avoid hubs)
 */
float compute_assortativity(NimcpGraph* graph)
{
    if (!graph) {
        return -2.0F;
    }

    if (graph->edge_count == 0) {
        return 0.0F;
    }

    float m = (float)graph->edge_count;

    // Accumulate statistics over all edges
    float sum_jk = 0.0F;    // Σ(j * k)
    float sum_j = 0.0F;     // Σ(j)
    float sum_k = 0.0F;     // Σ(k)
    float sum_j2 = 0.0F;    // Σ(j^2)
    float sum_k2 = 0.0F;    // Σ(k^2)

    // Iterate over all edges
    for (uint32_t i = 0; i < graph->vertex_count; i++) {
        uint32_t deg_i = get_degree(graph, i);

        NimcpEdgeNode* edge = graph->vertices[i].edges;
        while (edge) {
            uint32_t j = edge->dest;
            uint32_t deg_j = get_degree(graph, j);

            float j_f = (float)deg_i;
            float k_f = (float)deg_j;

            sum_jk += j_f * k_f;
            sum_j += j_f;
            sum_k += k_f;
            sum_j2 += j_f * j_f;
            sum_k2 += k_f * k_f;

            edge = edge->next;
        }
    }

    // Compute Pearson correlation coefficient
    float numerator = sum_jk - (sum_j * sum_k) / m;
    float denom_j = sum_j2 - (sum_j * sum_j) / m;
    float denom_k = sum_k2 - (sum_k * sum_k) / m;

    if (denom_j <= 0.0F || denom_k <= 0.0F) {
        return 0.0F;  // All vertices have same degree
    }

    float denominator = sqrtf(denom_j * denom_k);

    if (denominator == 0.0F) {
        return 0.0F;
    }

    float r = numerator / denominator;

    return r;
}

//=============================================================================
// Community Detection (Label Propagation Algorithm)
//=============================================================================

/**
 * @brief Detect communities using Label Propagation Algorithm (LPA)
 *
 * WHAT: Assigns community labels to vertices based on neighbor consensus
 * WHY:  Near-linear time community detection for modularity calculation
 * HOW:  Each vertex adopts most frequent label among neighbors; iterate to convergence
 *
 * ALGORITHM (Raghavan et al., 2007):
 * 1. Initialize each vertex with unique label
 * 2. In each iteration:
 *    a. Process vertices in random order
 *    b. Each vertex adopts the most frequent label among neighbors
 *    c. Break ties randomly
 * 3. Stop when no labels change or max iterations reached
 *
 * COMPLEXITY: O(k*E) where k = iterations (~5-10 typically), E = edges
 *
 * @param graph Network to analyze (must be non-NULL)
 * @param communities Output array for community labels [vertex_count] (caller allocated)
 * @param max_iterations Maximum iterations (0 for default of 100)
 * @return Number of communities detected, or 0 on error
 */
static uint32_t detect_communities_label_propagation(
    NimcpGraph* graph,
    uint32_t* communities,
    uint32_t max_iterations)
{
    if (!graph || !communities || graph->vertex_count == 0) {
        return 0;
    }

    uint32_t n = graph->vertex_count;
    if (max_iterations == 0) {
        max_iterations = 100;
    }

    // Initialize each vertex with unique community label
    for (uint32_t i = 0; i < n; i++) {
        communities[i] = i;
    }

    // Allocate temporary arrays for label counting
    // We use a simple approach: for each vertex, count neighbor labels
    uint32_t* label_counts = (uint32_t*)nimcp_calloc(n, sizeof(uint32_t));
    uint32_t* vertex_order = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));

    if (!label_counts || !vertex_order) {
        nimcp_free(label_counts);
        nimcp_free(vertex_order);
        return 0;
    }

    // Initialize vertex processing order
    for (uint32_t i = 0; i < n; i++) {
        vertex_order[i] = i;
    }

    // Simple pseudo-random shuffle using linear congruential generator
    uint32_t seed = 12345;
    for (uint32_t i = n - 1; i > 0; i--) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        uint32_t j = seed % (i + 1);
        uint32_t tmp = vertex_order[i];
        vertex_order[i] = vertex_order[j];
        vertex_order[j] = tmp;
    }

    // Iterate until convergence or max iterations
    bool changed = true;
    uint32_t iteration = 0;

    while (changed && iteration < max_iterations) {
        changed = false;
        iteration++;

        // Shuffle order each iteration for better convergence
        seed = seed * 1103515245 + 12345;
        for (uint32_t i = n - 1; i > 0; i--) {
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            uint32_t j = seed % (i + 1);
            uint32_t tmp = vertex_order[i];
            vertex_order[i] = vertex_order[j];
            vertex_order[j] = tmp;
        }

        // Process each vertex
        for (uint32_t idx = 0; idx < n; idx++) {
            uint32_t v = vertex_order[idx];
            uint32_t degree = get_degree(graph, v);

            if (degree == 0) {
                continue;  // Isolated vertex keeps its label
            }

            // Reset label counts
            memset(label_counts, 0, n * sizeof(uint32_t));

            // Count labels among neighbors
            NimcpEdgeNode* edge = graph->vertices[v].edges;
            while (edge) {
                uint32_t neighbor_label = communities[edge->dest];
                label_counts[neighbor_label]++;
                edge = edge->next;
            }

            // Find most frequent label (with tie-breaking toward lower labels for stability)
            uint32_t best_label = communities[v];
            uint32_t best_count = 0;

            for (uint32_t label = 0; label < n; label++) {
                if (label_counts[label] > best_count) {
                    best_count = label_counts[label];
                    best_label = label;
                }
            }

            // Update if label changed
            if (best_label != communities[v]) {
                communities[v] = best_label;
                changed = true;
            }
        }

        // Heartbeat for long-running community detection
        graph_metrics_heartbeat("community_detection",
                                (float)iteration / (float)max_iterations);
    }

    nimcp_free(label_counts);
    nimcp_free(vertex_order);

    // Count unique communities
    uint32_t* seen = (uint32_t*)nimcp_calloc(n, sizeof(uint32_t));
    if (!seen) {
        return 1;  // Assume at least 1 community
    }

    uint32_t num_communities = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t label = communities[i];
        if (label < n && seen[label] == 0) {
            seen[label] = 1;
            num_communities++;
        }
    }

    nimcp_free(seen);
    return num_communities;
}

/**
 * WHAT: Computes all graph metrics in one call
 * WHY: Convenient single-function API for complete analysis
 * HOW: Calls individual metric functions and packages results
 *
 * Uses Label Propagation Algorithm for community detection to compute modularity
 */
graph_metrics_t* compute_graph_metrics(NimcpGraph* graph)
{
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    graph_metrics_t* metrics = (graph_metrics_t*)nimcp_malloc(sizeof(graph_metrics_t));
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metrics is NULL");

        return NULL;
    }

    // Initialize all metrics to invalid values
    metrics->modularity = 0.0F;
    metrics->clustering_coefficient = -1.0F;
    metrics->characteristic_path_length = -1.0F;
    metrics->small_world_coefficient = 0.0F;
    metrics->diameter = 0;
    metrics->assortativity = -2.0F;

    // Compute basic metrics
    metrics->clustering_coefficient = compute_clustering_coefficient(graph);
    metrics->characteristic_path_length = compute_characteristic_path_length(graph);
    metrics->assortativity = compute_assortativity(graph);

    // Compute modularity using Label Propagation community detection
    if (graph->vertex_count > 0) {
        uint32_t* communities = (uint32_t*)nimcp_calloc(graph->vertex_count, sizeof(uint32_t));
        if (communities) {
            // Detect communities using Label Propagation Algorithm
            uint32_t num_communities = detect_communities_label_propagation(
                graph, communities, 100);  // max 100 iterations

            // Compute modularity with detected communities
            metrics->modularity = compute_modularity_q(graph, communities);

            // Log community detection result for debugging
            (void)num_communities;  // Suppress unused warning in release builds

            nimcp_free(communities);
        }
    }

    // Compute diameter (max shortest path) from distance matrix
    // Reuse Floyd-Warshall calculation
    if (graph->vertex_count > 0) {
        uint32_t n = graph->vertex_count;
        if ((size_t)n * n > SIZE_MAX / sizeof(float)) {
            nimcp_free(metrics);
            return NULL;
        }
        float* dist = (float*)nimcp_malloc((size_t)n * n * sizeof(float));

        if (dist) {
            // Initialize distance matrix
            for (uint32_t i = 0; i < n; i++) {
                for (uint32_t j = 0; j < n; j++) {
                    if (i == j) {
                        dist[i * n + j] = 0.0F;
                    } else if (has_edge(graph, i, j)) {
                        dist[i * n + j] = 1.0F;
                    } else {
                        dist[i * n + j] = FLT_MAX;
                    }
                }
            }

            // Floyd-Warshall
            for (uint32_t k = 0; k < n; k++) {
                for (uint32_t i = 0; i < n; i++) {
                    for (uint32_t j = 0; j < n; j++) {
                        float dist_ik = dist[i * n + k];
                        float dist_kj = dist[k * n + j];
                        if (dist_ik != FLT_MAX && dist_kj != FLT_MAX) {
                            float new_dist = dist_ik + dist_kj;
                            if (new_dist < dist[i * n + j]) {
                                dist[i * n + j] = new_dist;
                            }
                        }
                    }
                }
            }

            // Find maximum finite distance
            float max_dist = 0.0F;
            for (uint32_t i = 0; i < n; i++) {
                for (uint32_t j = 0; j < n; j++) {
                    if (i != j && dist[i * n + j] != FLT_MAX) {
                        if (dist[i * n + j] > max_dist) {
                            max_dist = dist[i * n + j];
                        }
                    }
                }
            }

            metrics->diameter = (uint32_t)max_dist;
            nimcp_free(dist);
        }
    }

    // Compute small-world coefficient (requires random graph comparison)
    // σ = (C/C_random) / (L/L_random)
    // For now, use approximations:
    // C_random ≈ k/N (k = average degree, N = vertices)
    // L_random ≈ log(N)/log(k)
    if (graph->vertex_count > 1 && graph->edge_count > 0) {
        float N = (float)graph->vertex_count;
        float k_avg = (2.0F * (float)graph->edge_count) / N;

        if (k_avg > 1.0F) {
            float C_random = k_avg / N;
            float L_random = logf(N) / logf(k_avg);

            if (C_random > 0.0F && L_random > 0.0F &&
                metrics->clustering_coefficient > 0.0F &&
                metrics->characteristic_path_length > 0.0F) {

                float C_ratio = metrics->clustering_coefficient / C_random;
                float L_ratio = metrics->characteristic_path_length / L_random;

                if (L_ratio > 0.0F) {
                    metrics->small_world_coefficient = C_ratio / L_ratio;
                }
            }
        }
    }

    return metrics;
}

/**
 * WHAT: Frees graph metrics structure
 * WHY: Clean memory management
 * HOW: NULL-safe deallocation
 */
void graph_metrics_destroy(graph_metrics_t* metrics)
{
    if (metrics) {
        nimcp_free(metrics);
    }
}
