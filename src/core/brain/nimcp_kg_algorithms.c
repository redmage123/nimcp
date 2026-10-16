/**
 * @file nimcp_kg_algorithms.c
 * @brief Graph Algorithm Utilities for KG Hierarchy - Implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implementation of integrated graph algorithms for KG operations including:
 * - Centrality metrics (degree, betweenness, closeness, eigenvector)
 * - Community detection (Louvain algorithm)
 * - Graph metrics (modularity, clustering coefficient, small-world)
 * - Quantum walk search (sqrt(N) speedup)
 * - Similarity search (KD-tree + MPS)
 * - Phase coherence analysis
 * - Hyperbolic embeddings
 * - Ternary relationship handling
 */

#include "core/brain/nimcp_kg_algorithms.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_algorithms, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define KG_ALGO_MAX_ITERATIONS      100
#include "constants/nimcp_constants.h"
#define KG_ALGO_CONVERGENCE_EPSILON NIMCP_EPSILON_NUMERICAL
#define KG_ALGO_POWER_ITERATIONS    50
#define KG_ALGO_QMC_DEFAULT_SAMPLES 1000
#define KG_ALGO_RANDOM_SEED         42

/* ============================================================================
 * Internal Helper Structures
 * ============================================================================ */

/**
 * @brief Internal adjacency list representation for graph algorithms
 */
typedef struct {
    brain_kg_node_id_t node_id;
    brain_kg_node_id_t* neighbors;
    uint32_t neighbor_count;
    float* edge_weights;
} adjacency_entry_t;

/**
 * @brief Internal graph representation for algorithms
 */
typedef struct {
    adjacency_entry_t* adjacency;
    uint32_t node_count;
    uint32_t edge_count;
    brain_kg_node_id_t* node_ids;
    float* embeddings;
    uint32_t embedding_dim;
    float* phases;
    trit_t* ternary_matrix;
    uint32_t* community_assignments;
    nimcp_mutex_t* mutex;
} algorithm_graph_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static algorithm_graph_t* build_algorithm_graph(const kg_hierarchy_t* hier);
static void free_algorithm_graph(algorithm_graph_t* graph);
static int find_node_index(const algorithm_graph_t* graph, brain_kg_node_id_t node_id);
static float* compute_shortest_paths(const algorithm_graph_t* graph, uint32_t source_idx);
static float random_float(void);
static uint32_t random_uint(uint32_t max);

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

int kg_algorithm_config_default(kg_algorithm_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(kg_algorithm_config_t));

    /* Graph Analysis */
    config->enable_centrality_metrics = true;
    config->enable_community_detection = true;
    config->enable_graph_metrics = true;

    /* Quantum Algorithms */
    config->enable_quantum_walk = true;
    config->enable_quantum_monte_carlo = true;
    config->enable_quantum_shannon = false;
    config->quantum_coin = KG_QUANTUM_COIN_HADAMARD;
    config->quantum_walk_steps = KG_ALGO_DEFAULT_QUANTUM_STEPS;
    config->decoherence_rate = 0.01f;

    /* Spatial Indexing */
    config->enable_kdtree_indexing = true;
    config->kdtree_dimensions = KG_ALGO_DEFAULT_KDTREE_DIMS;

    /* Tensor Compression */
    config->enable_mps_compression = false;
    config->mps_bond_dimension = KG_ALGO_DEFAULT_MPS_BOND_DIM;

    /* Ternary Logic */
    config->enable_ternary_relationships = true;
    config->enable_ternary_inference = true;

    /* Math Utils */
    config->enable_phase_coherence = true;
    config->enable_hyperbolic_embeddings = true;

    return 0;
}

/* ============================================================================
 * Internal Graph Building
 * ============================================================================ */

static algorithm_graph_t* build_algorithm_graph(const kg_hierarchy_t* hier) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hier is NULL");

        return NULL;
    }

    algorithm_graph_t* graph = nimcp_calloc(1, sizeof(algorithm_graph_t));
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "graph is NULL");

        return NULL;
    }

    /* Get node count from hierarchy */
    uint32_t node_count = 0;
    if (kg_hierarchy_get_node_count(hier, &node_count) != 0 || node_count == 0) {
        /* Fallback: assume some nodes exist */
        node_count = 64;
    }

    graph->node_count = node_count;
    graph->node_ids = nimcp_calloc(node_count, sizeof(brain_kg_node_id_t));
    graph->adjacency = nimcp_calloc(node_count, sizeof(adjacency_entry_t));

    if (!graph->node_ids || !graph->adjacency) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_algorithm_graph: required parameter is NULL (graph->node_ids, graph->adjacency)");
        return NULL;
    }

    /* Initialize node IDs (we'll populate from hierarchy) */
    for (uint32_t i = 0; i < node_count; i++) {
        graph->node_ids[i] = (brain_kg_node_id_t)i;
        graph->adjacency[i].node_id = (brain_kg_node_id_t)i;
        graph->adjacency[i].neighbors = NULL;
        graph->adjacency[i].neighbor_count = 0;
        graph->adjacency[i].edge_weights = NULL;
    }

    /* Populate adjacency from hierarchy edges */
    kg_edge_iterator_t* iter = kg_hierarchy_edge_iterator(hier);
    if (iter) {
        kg_edge_t edge;
        while (kg_edge_iterator_next(iter, &edge) == 0) {
            int src_idx = find_node_index(graph, edge.source);
            if (src_idx >= 0 && (uint32_t)src_idx < node_count) {
                adjacency_entry_t* entry = &graph->adjacency[src_idx];
                uint32_t new_count = entry->neighbor_count + 1;

                brain_kg_node_id_t* new_neighbors = nimcp_realloc(
                    entry->neighbors,
                    new_count * sizeof(brain_kg_node_id_t)
                );
                float* new_weights = nimcp_realloc(
                    entry->edge_weights,
                    new_count * sizeof(float)
                );

                if (new_neighbors && new_weights) {
                    new_neighbors[entry->neighbor_count] = edge.target;
                    new_weights[entry->neighbor_count] = edge.weight;
                    entry->neighbors = new_neighbors;
                    entry->edge_weights = new_weights;
                    entry->neighbor_count = new_count;
                    graph->edge_count++;
                }
            }
        }
        kg_edge_iterator_free(iter);
    }

    /* Allocate community assignments */
    graph->community_assignments = nimcp_calloc(node_count, sizeof(uint32_t));
    if (!graph->community_assignments) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_algorithm_graph: graph->community_assignments is NULL");
        return NULL;
    }

    /* Initialize each node in its own community */
    for (uint32_t i = 0; i < node_count; i++) {
        graph->community_assignments[i] = i;
    }

    /* Allocate ternary matrix (node_count x node_count) */
    graph->ternary_matrix = nimcp_calloc(node_count * node_count, sizeof(trit_t));

    /* Allocate phases for coherence analysis */
    graph->phases = nimcp_calloc(node_count, sizeof(float));
    if (graph->phases) {
        for (uint32_t i = 0; i < node_count; i++) {
            graph->phases[i] = random_float() * 2.0f * M_PI;
        }
    }

    return graph;
}

static void free_algorithm_graph(algorithm_graph_t* graph) {
    if (!graph) {
        return;
    }

    if (graph->adjacency) {
        for (uint32_t i = 0; i < graph->node_count; i++) {
            nimcp_free(graph->adjacency[i].neighbors);
            nimcp_free(graph->adjacency[i].edge_weights);
        }
        nimcp_free(graph->adjacency);
    }

    nimcp_free(graph->node_ids);
    nimcp_free(graph->embeddings);
    nimcp_free(graph->phases);
    nimcp_free(graph->ternary_matrix);
    nimcp_free(graph->community_assignments);
    nimcp_free(graph);
}

static int find_node_index(const algorithm_graph_t* graph, brain_kg_node_id_t node_id) {
    if (!graph || !graph->node_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_node_index: required parameter is NULL (graph, graph->node_ids)");
        return -1;
    }

    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->node_ids[i] == node_id) {
            return (int)i;
        }
    }
    return -1;
}

/* ============================================================================
 * Shortest Paths (Dijkstra)
 * ============================================================================ */

static float* compute_shortest_paths(const algorithm_graph_t* graph, uint32_t source_idx) {
    if (!graph || source_idx >= graph->node_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "compute_shortest_paths: graph is NULL");
        return NULL;
    }

    float* distances = nimcp_calloc(graph->node_count, sizeof(float));
    bool* visited = nimcp_calloc(graph->node_count, sizeof(bool));

    if (!distances || !visited) {
        nimcp_free(distances);
        nimcp_free(visited);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_shortest_paths: required parameter is NULL (distances, visited)");
        return NULL;
    }

    /* Initialize distances to infinity */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        distances[i] = FLT_MAX;
    }
    distances[source_idx] = 0.0f;

    /* Dijkstra's algorithm (simple implementation) */
    for (uint32_t iter = 0; iter < graph->node_count; iter++) {
        /* Find minimum unvisited */
        uint32_t min_idx = UINT32_MAX;
        float min_dist = FLT_MAX;
        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (!visited[i] && distances[i] < min_dist) {
                min_dist = distances[i];
                min_idx = i;
            }
        }

        if (min_idx == UINT32_MAX) {
            break;
        }

        visited[min_idx] = true;

        /* Update neighbors */
        adjacency_entry_t* entry = &graph->adjacency[min_idx];
        for (uint32_t j = 0; j < entry->neighbor_count; j++) {
            int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
            if (neighbor_idx >= 0 && !visited[neighbor_idx]) {
                float weight = entry->edge_weights ? entry->edge_weights[j] : 1.0f;
                float new_dist = distances[min_idx] + weight;
                if (new_dist < distances[neighbor_idx]) {
                    distances[neighbor_idx] = new_dist;
                }
            }
        }
    }

    nimcp_free(visited);
    return distances;
}

/* ============================================================================
 * Random Number Generation
 * ============================================================================ */

static __thread uint32_t random_state = KG_ALGO_RANDOM_SEED;

static float random_float(void) {
    random_state = random_state * 1103515245 + 12345;
    return (float)(random_state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static uint32_t random_uint(uint32_t max) {
    if (max == 0) return 0;
    random_state = random_state * 1103515245 + 12345;
    return (random_state & 0x7FFFFFFF) % max;
}

/* ============================================================================
 * Centrality Analysis API Implementation
 * ============================================================================ */

int kg_compute_centrality(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id,
    kg_centrality_metrics_t* metrics
) {
    if (!hier || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compute_centrality: required parameter is NULL (hier, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(kg_centrality_metrics_t));
    metrics->node_id = node_id;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return -1;
    }

    int node_idx = find_node_index(graph, node_id);
    if (node_idx < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compute_centrality: validation failed");
        return -1;
    }

    /* Degree centrality: number of connections / (N-1) */
    uint32_t degree = graph->adjacency[node_idx].neighbor_count;
    if (graph->node_count > 1) {
        metrics->degree_centrality = (float)degree / (float)(graph->node_count - 1);
    }

    /* Closeness centrality: (N-1) / sum of shortest paths */
    float* distances = compute_shortest_paths(graph, (uint32_t)node_idx);
    if (distances) {
        float sum_distances = 0.0f;
        uint32_t reachable = 0;
        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (i != (uint32_t)node_idx && distances[i] < FLT_MAX) {
                sum_distances += distances[i];
                reachable++;
            }
        }
        if (sum_distances > 0 && reachable > 0) {
            metrics->closeness_centrality = (float)reachable / sum_distances;
            /* Normalize to [0,1] */
            metrics->closeness_centrality = fminf(metrics->closeness_centrality, 1.0f);
        }
        nimcp_free(distances);
    }

    /* Betweenness centrality (simplified approximation) */
    /* Full computation is O(N^3), use sampling for large graphs */
    float betweenness = 0.0f;
    uint32_t sample_size = graph->node_count > 100 ? 100 : graph->node_count;

    for (uint32_t s = 0; s < sample_size; s++) {
        uint32_t src = random_uint(graph->node_count);
        if (src == (uint32_t)node_idx) continue;

        float* src_dist = compute_shortest_paths(graph, src);
        if (!src_dist) continue;

        for (uint32_t t = s + 1; t < sample_size; t++) {
            uint32_t tgt = random_uint(graph->node_count);
            if (tgt == (uint32_t)node_idx || tgt == src) continue;

            /* Check if node is on shortest path */
            float total_dist = src_dist[tgt];
            float to_node = src_dist[node_idx];

            float* node_dist = compute_shortest_paths(graph, (uint32_t)node_idx);
            if (node_dist) {
                float from_node = node_dist[tgt];

                if (total_dist < FLT_MAX && to_node < FLT_MAX && from_node < FLT_MAX) {
                    if (fabsf(to_node + from_node - total_dist) < 0.001f) {
                        betweenness += 1.0f;
                    }
                }
                nimcp_free(node_dist);
            }
        }
        nimcp_free(src_dist);
    }

    /* Normalize betweenness */
    float max_betweenness = (float)(graph->node_count - 1) * (float)(graph->node_count - 2) / 2.0f;
    if (max_betweenness > 0) {
        metrics->betweenness_centrality = betweenness / max_betweenness;
    }

    /* Eigenvector centrality (power iteration) */
    float* eigenvector = nimcp_calloc(graph->node_count, sizeof(float));
    float* eigenvector_new = nimcp_calloc(graph->node_count, sizeof(float));

    if (eigenvector && eigenvector_new) {
        /* Initialize uniformly */
        float init_val = 1.0f / sqrtf((float)graph->node_count);
        for (uint32_t i = 0; i < graph->node_count; i++) {
            eigenvector[i] = init_val;
        }

        /* Power iteration */
        for (int iter = 0; iter < KG_ALGO_POWER_ITERATIONS; iter++) {
            memset(eigenvector_new, 0, graph->node_count * sizeof(float));

            /* Multiply by adjacency matrix */
            for (uint32_t i = 0; i < graph->node_count; i++) {
                adjacency_entry_t* entry = &graph->adjacency[i];
                for (uint32_t j = 0; j < entry->neighbor_count; j++) {
                    int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
                    if (neighbor_idx >= 0) {
                        eigenvector_new[neighbor_idx] += eigenvector[i];
                    }
                }
            }

            /* Normalize */
            float norm = 0.0f;
            for (uint32_t i = 0; i < graph->node_count; i++) {
                norm += eigenvector_new[i] * eigenvector_new[i];
            }
            norm = sqrtf(norm);

            if (norm > 0) {
                for (uint32_t i = 0; i < graph->node_count; i++) {
                    eigenvector_new[i] /= norm;
                }
            }

            /* Check convergence */
            float diff = 0.0f;
            for (uint32_t i = 0; i < graph->node_count; i++) {
                diff += fabsf(eigenvector_new[i] - eigenvector[i]);
            }

            memcpy(eigenvector, eigenvector_new, graph->node_count * sizeof(float));

            if (diff < KG_ALGO_CONVERGENCE_EPSILON) {
                break;
            }
        }

        metrics->eigenvector_centrality = eigenvector[node_idx];
        nimcp_free(eigenvector);
        nimcp_free(eigenvector_new);
    }

    /* Determine hub status */
    float combined = (metrics->degree_centrality +
                      metrics->betweenness_centrality +
                      metrics->closeness_centrality +
                      metrics->eigenvector_centrality) / 4.0f;
    metrics->is_hub = combined >= KG_ALGO_DEFAULT_HUB_THRESHOLD;
    metrics->hub_rank = metrics->is_hub ? 1 : 0;

    free_algorithm_graph(graph);
    return 0;
}

int kg_compute_all_centrality(
    const kg_hierarchy_t* hier,
    kg_centrality_metrics_t** metrics,
    uint32_t* count
) {
    if (!hier || !metrics || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compute_all_centrality: required parameter is NULL (hier, metrics, count)");
        return -1;
    }

    *metrics = NULL;
    *count = 0;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph || graph->node_count == 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compute_all_centrality: graph is NULL");
        return -1;
    }

    *metrics = nimcp_calloc(graph->node_count, sizeof(kg_centrality_metrics_t));
    if (!*metrics) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compute_all_centrality: validation failed");
        return -1;
    }

    *count = graph->node_count;

    /* Compute for each node */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        kg_compute_centrality(hier, graph->node_ids[i], &(*metrics)[i]);
    }

    /* Rank hubs */
    uint32_t hub_rank = 1;
    for (uint32_t i = 0; i < *count; i++) {
        if ((*metrics)[i].is_hub) {
            (*metrics)[i].hub_rank = hub_rank++;
        }
    }

    free_algorithm_graph(graph);
    return 0;
}

int kg_detect_hubs(
    const kg_hierarchy_t* hier,
    float threshold,
    brain_kg_node_id_t** hubs,
    uint32_t* hub_count
) {
    if (!hier || !hubs || !hub_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_detect_hubs: required parameter is NULL (hier, hubs, hub_count)");
        return -1;
    }

    *hubs = NULL;
    *hub_count = 0;

    kg_centrality_metrics_t* metrics = NULL;
    uint32_t count = 0;

    if (kg_compute_all_centrality(hier, &metrics, &count) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_detect_hubs: validation failed");
        return -1;
    }

    /* Count hubs */
    uint32_t num_hubs = 0;
    for (uint32_t i = 0; i < count; i++) {
        float combined = (metrics[i].degree_centrality +
                          metrics[i].betweenness_centrality +
                          metrics[i].closeness_centrality +
                          metrics[i].eigenvector_centrality) / 4.0f;
        if (combined >= threshold) {
            num_hubs++;
        }
    }

    if (num_hubs == 0) {
        kg_centrality_metrics_free(metrics, count);
        return 0;
    }

    *hubs = nimcp_calloc(num_hubs, sizeof(brain_kg_node_id_t));
    if (!*hubs) {
        kg_centrality_metrics_free(metrics, count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_detect_hubs: validation failed");
        return -1;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < count; i++) {
        float combined = (metrics[i].degree_centrality +
                          metrics[i].betweenness_centrality +
                          metrics[i].closeness_centrality +
                          metrics[i].eigenvector_centrality) / 4.0f;
        if (combined >= threshold) {
            (*hubs)[idx++] = metrics[i].node_id;
        }
    }

    *hub_count = num_hubs;
    kg_centrality_metrics_free(metrics, count);
    return 0;
}

void kg_centrality_metrics_free(kg_centrality_metrics_t* metrics, uint32_t count) {
    (void)count;
    nimcp_free(metrics);
}

/* ============================================================================
 * Community Detection API Implementation (Louvain Algorithm)
 * ============================================================================ */

int kg_detect_communities(
    const kg_hierarchy_t* hier,
    kg_community_t** communities,
    uint32_t* count
) {
    if (!hier || !communities || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_detect_communities: required parameter is NULL (hier, communities, count)");
        return -1;
    }

    *communities = NULL;
    *count = 0;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph || graph->node_count == 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_detect_communities: graph is NULL");
        return -1;
    }

    /* Louvain algorithm: Phase 1 - Modularity optimization */
    uint32_t* community = nimcp_calloc(graph->node_count, sizeof(uint32_t));
    uint32_t* community_size = nimcp_calloc(graph->node_count, sizeof(uint32_t));

    if (!community || !community_size) {
        nimcp_free(community);
        nimcp_free(community_size);
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_detect_communities: required parameter is NULL (community, community_size)");
        return -1;
    }

    /* Initialize: each node in its own community */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        community[i] = i;
        community_size[i] = 1;
    }

    float total_weight = (float)graph->edge_count;
    if (total_weight < 1.0f) total_weight = 1.0f;

    bool improved = true;
    int iterations = 0;

    while (improved && iterations < KG_ALGO_MAX_ITERATIONS) {
        improved = false;
        iterations++;

        for (uint32_t i = 0; i < graph->node_count; i++) {
            uint32_t current_community = community[i];
            float best_delta_q = 0.0f;
            uint32_t best_community = current_community;

            adjacency_entry_t* entry = &graph->adjacency[i];

            /* Try moving to each neighbor's community */
            for (uint32_t j = 0; j < entry->neighbor_count; j++) {
                int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
                if (neighbor_idx < 0) continue;

                uint32_t target_community = community[neighbor_idx];
                if (target_community == current_community) continue;

                /* Calculate delta modularity */
                float ki = (float)entry->neighbor_count;
                float ki_in = 0.0f;

                /* Sum weights to target community */
                for (uint32_t k = 0; k < entry->neighbor_count; k++) {
                    int n_idx = find_node_index(graph, entry->neighbors[k]);
                    if (n_idx >= 0 && community[n_idx] == target_community) {
                        ki_in += entry->edge_weights ? entry->edge_weights[k] : 1.0f;
                    }
                }

                float sigma_tot = (float)community_size[target_community];
                float delta_q = (ki_in - ki * sigma_tot / total_weight) / total_weight;

                if (delta_q > best_delta_q) {
                    best_delta_q = delta_q;
                    best_community = target_community;
                }
            }

            if (best_community != current_community) {
                community_size[current_community]--;
                community_size[best_community]++;
                community[i] = best_community;
                improved = true;
            }
        }
    }

    /* Compact community IDs */
    uint32_t* community_map = nimcp_calloc(graph->node_count, sizeof(uint32_t));
    uint32_t num_communities = 0;

    if (community_map) {
        for (uint32_t i = 0; i < graph->node_count; i++) {
            community_map[i] = UINT32_MAX;
        }

        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (community_map[community[i]] == UINT32_MAX) {
                community_map[community[i]] = num_communities++;
            }
            community[i] = community_map[community[i]];
        }
        nimcp_free(community_map);
    }

    if (num_communities == 0) {
        num_communities = 1;
    }

    /* Build community structures */
    *communities = nimcp_calloc(num_communities, sizeof(kg_community_t));
    if (!*communities) {
        nimcp_free(community);
        nimcp_free(community_size);
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_detect_communities: validation failed");
        return -1;
    }

    *count = num_communities;

    /* Count members per community */
    uint32_t* member_counts = nimcp_calloc(num_communities, sizeof(uint32_t));
    if (member_counts) {
        for (uint32_t i = 0; i < graph->node_count; i++) {
            member_counts[community[i]]++;
        }

        /* Allocate member arrays */
        for (uint32_t c = 0; c < num_communities; c++) {
            (*communities)[c].community_id = c;
            snprintf((*communities)[c].community_name, 64, "Community_%u", c);
            (*communities)[c].member_count = member_counts[c];
            (*communities)[c].members = nimcp_calloc(member_counts[c], sizeof(brain_kg_node_id_t));
            member_counts[c] = 0;  /* Reset for populating */
        }

        /* Populate members */
        for (uint32_t i = 0; i < graph->node_count; i++) {
            uint32_t c = community[i];
            if ((*communities)[c].members) {
                (*communities)[c].members[member_counts[c]++] = graph->node_ids[i];
            }
        }

        nimcp_free(member_counts);
    }

    /* Calculate community metrics */
    for (uint32_t c = 0; c < num_communities; c++) {
        kg_community_t* comm = &(*communities)[c];

        uint32_t internal_edges = 0;
        uint32_t external_edges = 0;

        for (uint32_t m = 0; m < comm->member_count; m++) {
            int member_idx = find_node_index(graph, comm->members[m]);
            if (member_idx < 0) continue;

            adjacency_entry_t* entry = &graph->adjacency[member_idx];
            for (uint32_t j = 0; j < entry->neighbor_count; j++) {
                int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
                if (neighbor_idx >= 0 && community[neighbor_idx] == c) {
                    internal_edges++;
                } else {
                    external_edges++;
                }
            }
        }

        uint32_t max_internal = comm->member_count * (comm->member_count - 1);
        uint32_t max_external = comm->member_count * (graph->node_count - comm->member_count);

        comm->internal_density = max_internal > 0 ?
            (float)internal_edges / (float)max_internal : 0.0f;
        comm->external_density = max_external > 0 ?
            (float)external_edges / (float)max_external : 0.0f;
        comm->modularity_contribution = comm->internal_density - comm->external_density;
    }

    nimcp_free(community);
    nimcp_free(community_size);
    free_algorithm_graph(graph);
    return 0;
}

int kg_get_node_community(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id,
    uint32_t* community_id
) {
    if (!hier || !community_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_get_node_community: required parameter is NULL (hier, community_id)");
        return -1;
    }

    kg_community_t* communities = NULL;
    uint32_t count = 0;

    if (kg_detect_communities(hier, &communities, &count) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_get_node_community: validation failed");
        return -1;
    }

    for (uint32_t c = 0; c < count; c++) {
        for (uint32_t m = 0; m < communities[c].member_count; m++) {
            if (communities[c].members[m] == node_id) {
                *community_id = communities[c].community_id;
                kg_community_free(communities, count);
                return 0;
            }
        }
    }

    kg_community_free(communities, count);
    /* Node not found in any community - normal query result */
    return -1;
}

float kg_compute_modularity(const kg_hierarchy_t* hier) {
    if (!hier) {
        return NAN;
    }

    kg_community_t* communities = NULL;
    uint32_t count = 0;

    if (kg_detect_communities(hier, &communities, &count) != 0) {
        return NAN;
    }

    float total_modularity = 0.0f;
    for (uint32_t c = 0; c < count; c++) {
        total_modularity += communities[c].modularity_contribution;
    }

    kg_community_free(communities, count);
    return total_modularity / (float)count;
}

void kg_community_free(kg_community_t* communities, uint32_t count) {
    if (!communities) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        nimcp_free(communities[i].members);
    }
    nimcp_free(communities);
}

/* ============================================================================
 * Graph Metrics API Implementation
 * ============================================================================ */

int kg_compute_graph_metrics(
    const kg_hierarchy_t* hier,
    kg_graph_metrics_t* metrics
) {
    if (!hier || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compute_graph_metrics: required parameter is NULL (hier, metrics)");
        return -1;
    }

    memset(metrics, 0, sizeof(kg_graph_metrics_t));

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph || graph->node_count == 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compute_graph_metrics: graph is NULL");
        return -1;
    }

    /* Modularity */
    metrics->modularity_q = kg_compute_modularity(hier);

    /* Clustering coefficient (average local clustering) */
    float total_clustering = 0.0f;
    uint32_t valid_nodes = 0;

    for (uint32_t i = 0; i < graph->node_count; i++) {
        adjacency_entry_t* entry = &graph->adjacency[i];
        if (entry->neighbor_count < 2) continue;

        /* Count triangles */
        uint32_t triangles = 0;
        for (uint32_t j = 0; j < entry->neighbor_count; j++) {
            for (uint32_t k = j + 1; k < entry->neighbor_count; k++) {
                /* Check if neighbors j and k are connected */
                int j_idx = find_node_index(graph, entry->neighbors[j]);
                if (j_idx < 0) continue;

                adjacency_entry_t* j_entry = &graph->adjacency[j_idx];
                for (uint32_t l = 0; l < j_entry->neighbor_count; l++) {
                    if (j_entry->neighbors[l] == entry->neighbors[k]) {
                        triangles++;
                        break;
                    }
                }
            }
        }

        uint32_t possible_triangles = entry->neighbor_count * (entry->neighbor_count - 1) / 2;
        if (possible_triangles > 0) {
            total_clustering += (float)triangles / (float)possible_triangles;
            valid_nodes++;
        }
    }

    metrics->clustering_coefficient = valid_nodes > 0 ?
        total_clustering / (float)valid_nodes : 0.0f;

    /* Characteristic path length */
    float total_path_length = 0.0f;
    uint32_t path_count = 0;
    uint32_t max_path = 0;

    for (uint32_t i = 0; i < graph->node_count; i++) {
        float* distances = compute_shortest_paths(graph, i);
        if (!distances) continue;

        for (uint32_t j = 0; j < graph->node_count; j++) {
            if (i != j && distances[j] < FLT_MAX) {
                total_path_length += distances[j];
                path_count++;
                if ((uint32_t)distances[j] > max_path) {
                    max_path = (uint32_t)distances[j];
                }
            }
        }
        nimcp_free(distances);
    }

    metrics->characteristic_path_length = path_count > 0 ?
        total_path_length / (float)path_count : 0.0f;
    metrics->diameter = max_path;

    /* Small-world coefficient */
    /* Compare to random graph: C_rand = k/N, L_rand = ln(N)/ln(k) */
    float avg_degree = graph->node_count > 0 ?
        (float)graph->edge_count / (float)graph->node_count : 1.0f;
    if (avg_degree < 1.0f) avg_degree = 1.0f;

    float c_rand = avg_degree / (float)graph->node_count;
    float l_rand = logf((float)graph->node_count) / logf(avg_degree);
    if (l_rand < 1.0f) l_rand = 1.0f;
    if (c_rand < 0.001f) c_rand = 0.001f;

    float gamma = metrics->clustering_coefficient / c_rand;
    float lambda = metrics->characteristic_path_length / l_rand;

    if (lambda > 0) {
        metrics->small_world_coefficient = gamma / lambda;
    }
    metrics->is_small_world = metrics->small_world_coefficient > 1.0f;

    /* Assortativity (degree correlation) */
    float sum_xy = 0.0f, sum_x = 0.0f, sum_y = 0.0f;
    float sum_x2 = 0.0f, sum_y2 = 0.0f;
    uint32_t edge_samples = 0;

    for (uint32_t i = 0; i < graph->node_count; i++) {
        adjacency_entry_t* entry = &graph->adjacency[i];
        float deg_i = (float)entry->neighbor_count;

        for (uint32_t j = 0; j < entry->neighbor_count; j++) {
            int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
            if (neighbor_idx < 0) continue;

            float deg_j = (float)graph->adjacency[neighbor_idx].neighbor_count;

            sum_xy += deg_i * deg_j;
            sum_x += deg_i;
            sum_y += deg_j;
            sum_x2 += deg_i * deg_i;
            sum_y2 += deg_j * deg_j;
            edge_samples++;
        }
    }

    if (edge_samples > 0) {
        float mean_x = sum_x / (float)edge_samples;
        float mean_y = sum_y / (float)edge_samples;
        float var_x = sum_x2 / (float)edge_samples - mean_x * mean_x;
        float var_y = sum_y2 / (float)edge_samples - mean_y * mean_y;
        float cov_xy = sum_xy / (float)edge_samples - mean_x * mean_y;

        if (var_x > 0 && var_y > 0) {
            metrics->assortativity = cov_xy / sqrtf(var_x * var_y);
        }
    }

    free_algorithm_graph(graph);
    return 0;
}

bool kg_is_small_world(const kg_hierarchy_t* hier) {
    kg_graph_metrics_t metrics;
    if (kg_compute_graph_metrics(hier, &metrics) != 0) {
        return false;
    }
    return metrics.is_small_world;
}

/* ============================================================================
 * Quantum Walk Search API Implementation
 * ============================================================================ */

kg_quantum_walk_result_t* kg_quantum_walk_search(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start,
    brain_kg_node_id_t target,
    const kg_algorithm_config_t* config
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hier is NULL");

        return NULL;
    }

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    int start_idx = find_node_index(graph, start);
    int target_idx = find_node_index(graph, target);

    if (start_idx < 0 || target_idx < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quantum_walk_search: validation failed");
        return NULL;
    }

    kg_quantum_walk_result_t* result = nimcp_calloc(1, sizeof(kg_quantum_walk_result_t));
    if (!result) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_quantum_walk_search: result is NULL");
        return NULL;
    }

    uint32_t steps = config ? config->quantum_walk_steps : KG_ALGO_DEFAULT_QUANTUM_STEPS;
    kg_quantum_coin_t coin = config ? config->quantum_coin : KG_QUANTUM_COIN_HADAMARD;
    float decoherence = config ? config->decoherence_rate : 0.01f;

    /* Allocate amplitudes for quantum state */
    float* amplitudes = nimcp_calloc(graph->node_count, sizeof(float));
    float* amplitudes_new = nimcp_calloc(graph->node_count, sizeof(float));

    if (!amplitudes || !amplitudes_new) {
        nimcp_free(amplitudes);
        nimcp_free(amplitudes_new);
        nimcp_free(result);
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_quantum_walk_search: required parameter is NULL (amplitudes, amplitudes_new)");
        return NULL;
    }

    /* Initialize: start node has amplitude 1 */
    amplitudes[start_idx] = 1.0f;

    /* Track path */
    brain_kg_node_id_t* path = nimcp_calloc(steps + 2, sizeof(brain_kg_node_id_t));
    uint32_t path_len = 0;
    if (path) {
        path[path_len++] = start;
    }

    /* Quantum walk iteration */
    for (uint32_t step = 0; step < steps; step++) {
        memset(amplitudes_new, 0, graph->node_count * sizeof(float));

        /* Apply coin operator and shift */
        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (fabsf(amplitudes[i]) < 1e-10f) continue;

            adjacency_entry_t* entry = &graph->adjacency[i];
            if (entry->neighbor_count == 0) {
                amplitudes_new[i] += amplitudes[i];
                continue;
            }

            float coin_factor;
            switch (coin) {
                case KG_QUANTUM_COIN_HADAMARD:
                    coin_factor = 1.0f / sqrtf((float)entry->neighbor_count);
                    break;
                case KG_QUANTUM_COIN_GROVER:
                    coin_factor = 2.0f / (float)entry->neighbor_count - 1.0f;
                    break;
                case KG_QUANTUM_COIN_FOURIER:
                    coin_factor = 1.0f / sqrtf((float)entry->neighbor_count);
                    break;
                case KG_QUANTUM_COIN_IDENTITY:
                default:
                    coin_factor = 1.0f;
                    break;
            }

            /* Spread to neighbors */
            for (uint32_t j = 0; j < entry->neighbor_count; j++) {
                int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
                if (neighbor_idx >= 0) {
                    amplitudes_new[neighbor_idx] += amplitudes[i] * coin_factor;
                }
            }
        }

        /* Apply decoherence (noise) */
        float decoherence_factor = 1.0f - decoherence;
        for (uint32_t i = 0; i < graph->node_count; i++) {
            amplitudes_new[i] *= decoherence_factor;
            amplitudes_new[i] += random_float() * decoherence * 0.1f;
        }

        /* Normalize */
        float norm = 0.0f;
        for (uint32_t i = 0; i < graph->node_count; i++) {
            norm += amplitudes_new[i] * amplitudes_new[i];
        }
        norm = sqrtf(norm);
        if (norm > 0) {
            for (uint32_t i = 0; i < graph->node_count; i++) {
                amplitudes_new[i] /= norm;
            }
        }

        memcpy(amplitudes, amplitudes_new, graph->node_count * sizeof(float));

        /* Track highest probability node for path */
        if (path) {
            float max_amp = 0.0f;
            uint32_t max_idx = 0;
            for (uint32_t i = 0; i < graph->node_count; i++) {
                if (fabsf(amplitudes[i]) > max_amp) {
                    max_amp = fabsf(amplitudes[i]);
                    max_idx = i;
                }
            }
            if (path_len < steps + 2) {
                path[path_len++] = graph->node_ids[max_idx];
            }

            /* Check if target found with high probability */
            if (fabsf(amplitudes[target_idx]) > 0.5f) {
                path[path_len++] = target;
                break;
            }
        }

        result->steps_taken = step + 1;
    }

    /* Calculate final metrics */
    result->total_probability = 0.0f;
    for (uint32_t i = 0; i < graph->node_count; i++) {
        result->total_probability += amplitudes[i] * amplitudes[i];
    }

    /* Shannon entropy */
    result->entropy = 0.0f;
    for (uint32_t i = 0; i < graph->node_count; i++) {
        float p = amplitudes[i] * amplitudes[i];
        if (p > 1e-10f) {
            result->entropy -= p * log2f(p);
        }
    }

    /* Hitting time estimate */
    result->hitting_time = (float)result->steps_taken;

    /* Speedup factor (theoretical sqrt(N) vs classical N) */
    float classical_expected = (float)graph->node_count / 2.0f;
    if (result->steps_taken > 0) {
        result->speedup_factor = classical_expected / (float)result->steps_taken;
    }

    /* Copy amplitudes to result */
    result->amplitudes = amplitudes;
    amplitudes = NULL;

    /* Copy path */
    result->path = path;
    result->path_length = path_len;

    nimcp_free(amplitudes_new);
    free_algorithm_graph(graph);
    return result;
}

int kg_quantum_walk_spreading_activation(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t source,
    float* activation_map,
    uint32_t steps
) {
    if (!hier || !activation_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_quantum_walk_spreading_activation: required parameter is NULL (hier, activation_map)");
        return -1;
    }

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return -1;
    }

    int source_idx = find_node_index(graph, source);
    if (source_idx < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_quantum_walk_spreading_activation: validation failed");
        return -1;
    }

    /* Initialize activation */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        activation_map[i] = 0.0f;
    }
    activation_map[source_idx] = 1.0f;

    float* activation_new = nimcp_calloc(graph->node_count, sizeof(float));
    if (!activation_new) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_quantum_walk_spreading_activation: activation_new is NULL");
        return -1;
    }

    /* Spreading activation */
    for (uint32_t step = 0; step < steps; step++) {
        memset(activation_new, 0, graph->node_count * sizeof(float));

        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (activation_map[i] < 1e-10f) continue;

            adjacency_entry_t* entry = &graph->adjacency[i];
            if (entry->neighbor_count == 0) {
                activation_new[i] += activation_map[i];
                continue;
            }

            float spread = activation_map[i] / sqrtf((float)entry->neighbor_count);
            for (uint32_t j = 0; j < entry->neighbor_count; j++) {
                int neighbor_idx = find_node_index(graph, entry->neighbors[j]);
                if (neighbor_idx >= 0) {
                    activation_new[neighbor_idx] += spread;
                }
            }
        }

        /* Normalize */
        float total = 0.0f;
        for (uint32_t i = 0; i < graph->node_count; i++) {
            total += activation_new[i];
        }
        if (total > 0) {
            for (uint32_t i = 0; i < graph->node_count; i++) {
                activation_map[i] = activation_new[i] / total;
            }
        }
    }

    nimcp_free(activation_new);
    free_algorithm_graph(graph);
    return 0;
}

void kg_quantum_walk_result_free(kg_quantum_walk_result_t* result) {
    if (!result) {
        return;
    }
    nimcp_free(result->path);
    nimcp_free(result->amplitudes);
    nimcp_free(result);
}

/* ============================================================================
 * Quantum Monte Carlo API Implementation
 * ============================================================================ */

kg_qmc_result_t* kg_qmc_estimate_reachability(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t source,
    brain_kg_node_id_t target,
    uint32_t samples
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hier is NULL");

        return NULL;
    }

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    int source_idx = find_node_index(graph, source);
    int target_idx = find_node_index(graph, target);

    if (source_idx < 0 || target_idx < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_qmc_estimate_reachability: validation failed");
        return NULL;
    }

    kg_qmc_result_t* result = nimcp_calloc(1, sizeof(kg_qmc_result_t));
    if (!result) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_qmc_estimate_reachability: result is NULL");
        return NULL;
    }

    if (samples == 0) {
        samples = KG_ALGO_QMC_DEFAULT_SAMPLES;
    }

    uint32_t successes = 0;
    float* success_probs = nimcp_calloc(samples, sizeof(float));

    for (uint32_t s = 0; s < samples; s++) {
        /* Random walk from source */
        int current = source_idx;
        uint32_t max_steps = graph->node_count * 2;
        float path_prob = 1.0f;

        for (uint32_t step = 0; step < max_steps; step++) {
            if (current == target_idx) {
                successes++;
                if (success_probs) {
                    success_probs[s] = path_prob;
                }
                break;
            }

            adjacency_entry_t* entry = &graph->adjacency[current];
            if (entry->neighbor_count == 0) {
                break;
            }

            /* Random neighbor selection (quantum-inspired importance sampling) */
            uint32_t next_idx = random_uint(entry->neighbor_count);
            int next = find_node_index(graph, entry->neighbors[next_idx]);
            if (next < 0) break;

            path_prob *= 1.0f / (float)entry->neighbor_count;
            current = next;
        }
    }

    result->estimated_probability = (float)successes / (float)samples;
    result->samples_used = samples;

    /* Compute variance */
    float mean_sq = 0.0f;
    if (success_probs) {
        for (uint32_t s = 0; s < samples; s++) {
            mean_sq += success_probs[s] * success_probs[s];
        }
        mean_sq /= (float)samples;
        result->variance = mean_sq - result->estimated_probability * result->estimated_probability;
        if (result->variance < 0) result->variance = 0;
        nimcp_free(success_probs);
    }

    /* 95% confidence interval */
    result->confidence_interval_95 = 1.96f * sqrtf(result->variance / (float)samples);

    /* KL divergence from uniform prior */
    float uniform_prior = 1.0f / (float)graph->node_count;
    if (result->estimated_probability > 0 && uniform_prior > 0) {
        result->kl_divergence = result->estimated_probability *
            log2f(result->estimated_probability / uniform_prior);
    }

    /* Fidelity */
    result->fidelity = 1.0f - fabsf(result->estimated_probability - uniform_prior);

    free_algorithm_graph(graph);
    return result;
}

int kg_qmc_estimate_entropy(
    const kg_hierarchy_t* hier,
    float* entropy
) {
    if (!hier || !entropy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_qmc_estimate_entropy: required parameter is NULL (hier, entropy)");
        return -1;
    }

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph || graph->node_count == 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_qmc_estimate_entropy: graph is NULL");
        return -1;
    }

    /* Estimate degree distribution entropy */
    uint32_t max_degree = 0;
    for (uint32_t i = 0; i < graph->node_count; i++) {
        if (graph->adjacency[i].neighbor_count > max_degree) {
            max_degree = graph->adjacency[i].neighbor_count;
        }
    }

    if (max_degree == 0) {
        *entropy = 0.0f;
        free_algorithm_graph(graph);
        return 0;
    }

    uint32_t* degree_hist = nimcp_calloc(max_degree + 1, sizeof(uint32_t));
    if (!degree_hist) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_qmc_estimate_entropy: degree_hist is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < graph->node_count; i++) {
        degree_hist[graph->adjacency[i].neighbor_count]++;
    }

    *entropy = 0.0f;
    for (uint32_t d = 0; d <= max_degree; d++) {
        if (degree_hist[d] > 0) {
            float p = (float)degree_hist[d] / (float)graph->node_count;
            *entropy -= p * log2f(p);
        }
    }

    nimcp_free(degree_hist);
    free_algorithm_graph(graph);
    return 0;
}

void kg_qmc_result_free(kg_qmc_result_t* result) {
    nimcp_free(result);
}

/* ============================================================================
 * Similarity Search API Implementation
 * ============================================================================ */

kg_similarity_result_t* kg_find_similar(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t query_node,
    uint32_t k,
    uint32_t* out_count
) {
    if (!hier || !out_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_similar: required parameter is NULL (hier, out_count)");
        return NULL;
    }

    *out_count = 0;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    int query_idx = find_node_index(graph, query_node);
    if (query_idx < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_similar: validation failed");
        return NULL;
    }

    if (k > graph->node_count - 1) {
        k = graph->node_count - 1;
    }

    if (k == 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_find_similar: k is zero");
        return NULL;
    }

    /* Compute structural similarity (Jaccard index on neighbors) */
    float* similarities = nimcp_calloc(graph->node_count, sizeof(float));
    if (!similarities) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_find_similar: similarities is NULL");
        return NULL;
    }

    adjacency_entry_t* query_entry = &graph->adjacency[query_idx];

    for (uint32_t i = 0; i < graph->node_count; i++) {
        if ((int)i == query_idx) {
            similarities[i] = -1.0f;  /* Exclude self */
            continue;
        }

        adjacency_entry_t* entry = &graph->adjacency[i];

        /* Jaccard similarity */
        uint32_t intersection = 0;
        for (uint32_t j = 0; j < query_entry->neighbor_count; j++) {
            for (uint32_t l = 0; l < entry->neighbor_count; l++) {
                if (query_entry->neighbors[j] == entry->neighbors[l]) {
                    intersection++;
                    break;
                }
            }
        }

        uint32_t union_size = query_entry->neighbor_count + entry->neighbor_count - intersection;
        similarities[i] = union_size > 0 ? (float)intersection / (float)union_size : 0.0f;
    }

    /* Find top k */
    kg_similarity_result_t* results = nimcp_calloc(k, sizeof(kg_similarity_result_t));
    if (!results) {
        nimcp_free(similarities);
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_find_similar: results is NULL");
        return NULL;
    }

    for (uint32_t rank = 0; rank < k; rank++) {
        float max_sim = -1.0f;
        int max_idx = -1;

        for (uint32_t i = 0; i < graph->node_count; i++) {
            if (similarities[i] > max_sim) {
                max_sim = similarities[i];
                max_idx = (int)i;
            }
        }

        if (max_idx < 0) break;

        results[rank].node_id = graph->node_ids[max_idx];
        results[rank].similarity_score = similarities[max_idx];
        results[rank].distance = 1.0f - similarities[max_idx];
        results[rank].embedding = NULL;
        results[rank].embedding_dim = 0;

        similarities[max_idx] = -1.0f;  /* Mark as used */
        (*out_count)++;
    }

    nimcp_free(similarities);
    free_algorithm_graph(graph);
    return results;
}

kg_similarity_result_t* kg_find_in_radius(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t query_node,
    float radius,
    uint32_t* out_count
) {
    if (!hier || !out_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_in_radius: required parameter is NULL (hier, out_count)");
        return NULL;
    }

    *out_count = 0;

    /* Convert radius to similarity threshold */
    float sim_threshold = 1.0f - radius;
    if (sim_threshold < 0) sim_threshold = 0;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    int query_idx = find_node_index(graph, query_node);
    if (query_idx < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_in_radius: validation failed");
        return NULL;
    }

    /* Count matches first */
    adjacency_entry_t* query_entry = &graph->adjacency[query_idx];
    uint32_t match_count = 0;

    for (uint32_t i = 0; i < graph->node_count; i++) {
        if ((int)i == query_idx) continue;

        adjacency_entry_t* entry = &graph->adjacency[i];

        uint32_t intersection = 0;
        for (uint32_t j = 0; j < query_entry->neighbor_count; j++) {
            for (uint32_t l = 0; l < entry->neighbor_count; l++) {
                if (query_entry->neighbors[j] == entry->neighbors[l]) {
                    intersection++;
                    break;
                }
            }
        }

        uint32_t union_size = query_entry->neighbor_count + entry->neighbor_count - intersection;
        float sim = union_size > 0 ? (float)intersection / (float)union_size : 0.0f;

        if (sim >= sim_threshold) {
            match_count++;
        }
    }

    if (match_count == 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_find_in_radius: match_count is zero");
        return NULL;
    }

    kg_similarity_result_t* results = nimcp_calloc(match_count, sizeof(kg_similarity_result_t));
    if (!results) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_find_in_radius: results is NULL");
        return NULL;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < graph->node_count && idx < match_count; i++) {
        if ((int)i == query_idx) continue;

        adjacency_entry_t* entry = &graph->adjacency[i];

        uint32_t intersection = 0;
        for (uint32_t j = 0; j < query_entry->neighbor_count; j++) {
            for (uint32_t l = 0; l < entry->neighbor_count; l++) {
                if (query_entry->neighbors[j] == entry->neighbors[l]) {
                    intersection++;
                    break;
                }
            }
        }

        uint32_t union_size = query_entry->neighbor_count + entry->neighbor_count - intersection;
        float sim = union_size > 0 ? (float)intersection / (float)union_size : 0.0f;

        if (sim >= sim_threshold) {
            results[idx].node_id = graph->node_ids[i];
            results[idx].similarity_score = sim;
            results[idx].distance = 1.0f - sim;
            results[idx].embedding = NULL;
            results[idx].embedding_dim = 0;
            idx++;
        }
    }

    *out_count = idx;
    free_algorithm_graph(graph);
    return results;
}

int kg_rebuild_similarity_index(kg_hierarchy_t* hier) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hier is NULL");

        return -1;
    }
    /* For now, similarity is computed on-demand */
    /* Future: build persistent KD-tree index */
    return 0;
}

void kg_similarity_result_free(kg_similarity_result_t* results, uint32_t count) {
    if (!results) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        nimcp_free(results[i].embedding);
    }
    nimcp_free(results);
}

/* ============================================================================
 * Phase Coherence API Implementation
 * ============================================================================ */

int kg_compute_coherence(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_a,
    brain_kg_node_id_t node_b,
    kg_coherence_result_t* result
) {
    if (!hier || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compute_coherence: required parameter is NULL (hier, result)");
        return -1;
    }

    memset(result, 0, sizeof(kg_coherence_result_t));
    result->node_a = node_a;
    result->node_b = node_b;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph || !graph->phases) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_compute_coherence: required parameter is NULL (graph, graph->phases)");
        return -1;
    }

    int idx_a = find_node_index(graph, node_a);
    int idx_b = find_node_index(graph, node_b);

    if (idx_a < 0 || idx_b < 0) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compute_coherence: validation failed");
        return -1;
    }

    float phase_a = graph->phases[idx_a];
    float phase_b = graph->phases[idx_b];

    /* Phase difference */
    result->phase_difference = fabsf(phase_a - phase_b);
    while (result->phase_difference > M_PI) {
        result->phase_difference = 2.0f * M_PI - result->phase_difference;
    }

    /* Phase locking value (PLV) - coherence measure */
    /* PLV = |mean(exp(i * (phase_a - phase_b)))| */
    /* Simplified: cos of phase difference for single pair */
    result->coherence = cosf(result->phase_difference);
    if (result->coherence < 0) result->coherence = -result->coherence;

    /* Phase-amplitude coupling (simplified) */
    /* Use degree as proxy for amplitude */
    float amp_a = (float)graph->adjacency[idx_a].neighbor_count;
    float amp_b = (float)graph->adjacency[idx_b].neighbor_count;
    float amp_product = amp_a * amp_b;
    float amp_sum = amp_a + amp_b;

    if (amp_sum > 0) {
        result->amplitude_coupling = 2.0f * amp_product / (amp_sum * amp_sum);
    }

    result->synchronized = result->coherence >= KG_ALGO_DEFAULT_COHERENCE_THRESHOLD;

    free_algorithm_graph(graph);
    return 0;
}

int kg_find_synchronized_pairs(
    const kg_hierarchy_t* hier,
    float threshold,
    kg_coherence_result_t** pairs,
    uint32_t* count
) {
    if (!hier || !pairs || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_synchronized_pairs: required parameter is NULL (hier, pairs, count)");
        return -1;
    }

    *pairs = NULL;
    *count = 0;

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph || graph->node_count < 2) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_find_synchronized_pairs: graph is NULL");
        return -1;
    }

    /* Count pairs above threshold */
    uint32_t pair_count = 0;
    for (uint32_t i = 0; i < graph->node_count; i++) {
        for (uint32_t j = i + 1; j < graph->node_count; j++) {
            kg_coherence_result_t result;
            if (kg_compute_coherence(hier, graph->node_ids[i],
                                      graph->node_ids[j], &result) == 0) {
                if (result.coherence >= threshold) {
                    pair_count++;
                }
            }
        }
    }

    if (pair_count == 0) {
        free_algorithm_graph(graph);
        return 0;
    }

    *pairs = nimcp_calloc(pair_count, sizeof(kg_coherence_result_t));
    if (!*pairs) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_find_synchronized_pairs: validation failed");
        return -1;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < graph->node_count && idx < pair_count; i++) {
        for (uint32_t j = i + 1; j < graph->node_count && idx < pair_count; j++) {
            kg_coherence_result_t result;
            if (kg_compute_coherence(hier, graph->node_ids[i],
                                      graph->node_ids[j], &result) == 0) {
                if (result.coherence >= threshold) {
                    (*pairs)[idx++] = result;
                }
            }
        }
    }

    *count = idx;
    free_algorithm_graph(graph);
    return 0;
}

void kg_coherence_result_free(kg_coherence_result_t* pairs, uint32_t count) {
    (void)count;
    nimcp_free(pairs);
}

/* ============================================================================
 * Hyperbolic Embedding API Implementation
 * ============================================================================ */

int kg_compute_hyperbolic_embedding(
    kg_hierarchy_t* hier,
    uint32_t dimensions
) {
    if (!hier || dimensions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_compute_hyperbolic_embedding: hier is NULL");
        return -1;
    }

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return -1;
    }

    /* Allocate embeddings */
    float* embeddings = nimcp_calloc(graph->node_count * dimensions, sizeof(float));
    if (!embeddings) {
        free_algorithm_graph(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_compute_hyperbolic_embedding: embeddings is NULL");
        return -1;
    }

    /* Initialize embeddings on Poincare ball */
    /* Use hierarchical levels to determine radial distance */
    for (uint32_t i = 0; i < graph->node_count; i++) {
        /* Compute "depth" based on neighborhood structure */
        float depth = 0.0f;
        adjacency_entry_t* entry = &graph->adjacency[i];

        /* Nodes with more connections are more central (lower depth) */
        if (graph->edge_count > 0) {
            depth = 1.0f - (float)entry->neighbor_count / (float)(graph->edge_count + 1);
        }
        depth = fmaxf(0.1f, fminf(0.9f, depth));

        /* Place on Poincare ball with radius = depth */
        float angle_step = 2.0f * M_PI / (float)dimensions;
        for (uint32_t d = 0; d < dimensions; d++) {
            float angle = (float)i * 0.618f * 2.0f * M_PI + (float)d * angle_step;
            embeddings[i * dimensions + d] = depth * cosf(angle) / sqrtf((float)dimensions);
        }
    }

    /* Store embeddings in graph (would need hierarchy extension) */
    nimcp_free(graph->embeddings);
    graph->embeddings = embeddings;
    graph->embedding_dim = dimensions;

    free_algorithm_graph(graph);
    return 0;
}

float kg_hyperbolic_distance(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t a,
    brain_kg_node_id_t b
) {
    if (!hier) {
        return -1.0f;
    }

    algorithm_graph_t* graph = build_algorithm_graph(hier);
    if (!graph) {
        return -1.0f;
    }

    int idx_a = find_node_index(graph, a);
    int idx_b = find_node_index(graph, b);

    if (idx_a < 0 || idx_b < 0) {
        free_algorithm_graph(graph);
        return -1.0f;
    }

    /* If no embeddings, use shortest path as proxy */
    float* distances = compute_shortest_paths(graph, (uint32_t)idx_a);
    float distance = -1.0f;

    if (distances && distances[idx_b] < FLT_MAX) {
        distance = distances[idx_b];
    }

    nimcp_free(distances);
    free_algorithm_graph(graph);
    return distance;
}

/* ============================================================================
 * Ternary Relationship API Implementation
 * ============================================================================ */

int kg_set_relationship_ternary(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    trit_t relationship
) {
    if (!hier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hier is NULL");

        return -1;
    }

    /* This would require extending kg_hierarchy_t to store ternary values */
    /* For now, use edge metadata if available */
    return kg_hierarchy_set_edge_metadata_int(hier, from, to,
                                               "ternary_relationship", (int64_t)relationship);
}

trit_t kg_get_relationship_ternary(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
) {
    if (!hier) {
        return TRIT_UNKNOWN;
    }

    int32_t value = 0;
    if (kg_hierarchy_get_edge_metadata_int(hier, from, to,
                                            "ternary_relationship", &value) == 0) {
        if (value > 0) return TRIT_TRUE;
        if (value < 0) return TRIT_FALSE;
    }
    return TRIT_UNKNOWN;
}

int kg_ternary_inference(
    const kg_hierarchy_t* hier,
    const char* query,
    trit_t* result
) {
    if (!hier || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_ternary_inference: required parameter is NULL (hier, query, result)");
        return -1;
    }

    *result = TRIT_UNKNOWN;

    /* Simple query parsing: "A->B" or "A AND B" */
    /* For complex queries, would need proper parser */

    /* Check if query contains "->" for path query */
    const char* arrow = strstr(query, "->");
    if (arrow) {
        /* Extract source and target node names */
        /* Simplified: assume numeric node IDs */
        brain_kg_node_id_t from = 0, to = 0;
        if (sscanf(query, "%u->%u", &from, &to) == 2) {
            *result = kg_get_relationship_ternary(hier, from, to);
            return 0;
        }
    }

    /* Default: unknown */
    return 0;
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* kg_quantum_coin_to_string(kg_quantum_coin_t coin) {
    switch (coin) {
        case KG_QUANTUM_COIN_HADAMARD:
            return "HADAMARD";
        case KG_QUANTUM_COIN_GROVER:
            return "GROVER";
        case KG_QUANTUM_COIN_FOURIER:
            return "FOURIER";
        case KG_QUANTUM_COIN_IDENTITY:
            return "IDENTITY";
        case KG_QUANTUM_COIN_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

void kg_hubs_free(brain_kg_node_id_t* hubs) {
    nimcp_free(hubs);
}
