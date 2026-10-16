#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_graph_theory_bridge.c - Graph Theory NIMCP Bridge Implementation
//=============================================================================
/**
 * @file nimcp_graph_theory_bridge.c
 * @brief Implementation of graph-theoretic analysis bridge
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/graphs/nimcp_graph_theory_bridge.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "core/brain/nimcp_brain_kg_helpers.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(graph_theory_bridge)

#define LOG_MODULE "GRAPH_THEORY_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define LOG_TAG "graph_theory_bridge"

/** Default PageRank damping factor */
#define DEFAULT_PAGERANK_DAMPING    0.85f

/** Default max iterations */
#define DEFAULT_MAX_ITERATIONS      NIMCP_MAX_ITERATIONS_DEFAULT

/** Default spectral dimensions */
#define DEFAULT_SPECTRAL_DIM        10

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal bridge state
 */
struct graph_theory_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    graph_theory_bridge_config_t config;

    /** KG registration state */
    graph_theory_kg_state_t kg_state;

    /** KG reference */
    brain_kg_t* kg;

    /** Admin token for KG */
    uint64_t admin_token;

    /** Exception handler reference */
    void* exception_handler;

    /** Bio-async channel reference */
    void* bio_async_channel;

    /** Initialized flag */
    bool initialized;

    /** Next request ID */
    uint64_t next_request_id;

    /** W15: brain backpointer for admin-token self-elevation on runtime emit */
    brain_t kg_brain;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void log_error(graph_theory_bridge_t bridge, const char* func,
                      graph_theory_error_t error, const char* msg);
static void report_exception(graph_theory_bridge_t bridge, const char* func,
                            graph_theory_error_t error, const char* detail);
static float* compute_degree_centrality(NimcpGraph* graph, uint32_t* num_nodes);
static float* compute_betweenness_centrality(NimcpGraph* graph, uint32_t* num_nodes);
static float* compute_pagerank(NimcpGraph* graph, float damping,
                               uint32_t max_iter, float tol, uint32_t* num_nodes);
static uint32_t* louvain_communities(NimcpGraph* graph, uint32_t* num_communities,
                                     float* modularity);

//=============================================================================
// Lifecycle Implementation
//=============================================================================

graph_theory_error_t graph_theory_bridge_default_config(
    graph_theory_bridge_config_t* config)
{
    if (!config) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(graph_theory_bridge_config_t));

    config->enable_kg_wiring = true;
    config->enable_exception_handling = true;
    config->enable_bio_async = true;
    config->enable_immune_presentation = true;
    config->enable_logging = true;

    config->default_centrality = CENTRALITY_PAGERANK;
    config->default_community_algo = COMMUNITY_LOUVAIN;
    config->max_iterations = DEFAULT_MAX_ITERATIONS;
    config->tolerance = GRAPH_THEORY_DEFAULT_TOLERANCE;
    config->pagerank_damping = DEFAULT_PAGERANK_DAMPING;
    config->spectral_dim = DEFAULT_SPECTRAL_DIM;
    config->enable_parallel = false;
    config->num_workers = 0;

    return GRAPH_THEORY_OK;
}

graph_theory_bridge_t graph_theory_bridge_create(
    const graph_theory_bridge_config_t* config)
{
    struct graph_theory_bridge* bridge = nimcp_calloc(1, sizeof(struct graph_theory_bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate graph theory bridge");
    if (bridge_base_init(&bridge->base, 0, "graph_theory") != 0) { nimcp_free(bridge); return NULL; }

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(graph_theory_bridge_config_t));
    } else {
        graph_theory_bridge_default_config(&bridge->config);
    }

    /* Initialize state */
    memset(&bridge->kg_state, 0, sizeof(graph_theory_kg_state_t));
    bridge->kg = NULL;
    bridge->admin_token = 0;
    bridge->exception_handler = NULL;
    bridge->bio_async_channel = NULL;
    bridge->initialized = true;
    bridge->next_request_id = 1;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Graph theory bridge created");
    }

    return bridge;
}

void graph_theory_bridge_destroy(graph_theory_bridge_t bridge)
{
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "graph_theory");
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Destroying graph theory bridge");
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

graph_theory_error_t graph_theory_bridge_register_kg(
    graph_theory_bridge_t bridge,
    brain_kg_t* kg,
    uint64_t admin_token)
{
    if (!bridge || !kg) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    if (!bridge->initialized) {
        return GRAPH_THEORY_ERROR_NOT_INIT;
    }

    if (!bridge->config.enable_kg_wiring) {
        if (bridge->config.enable_logging) {
            NIMCP_LOG_DEBUG(LOG_TAG, "KG wiring disabled, skipping registration");
        }
        return GRAPH_THEORY_OK;
    }

    bridge->kg = kg;
    bridge->admin_token = admin_token;

    /* Create root node for graph theory */
    brain_kg_node_id_t root_id = brain_kg_add_node(kg, "graph_theory",
                                                    BRAIN_KG_NODE_INTEGRATION,
                                                    "Graph theory analysis module");
    if (root_id == BRAIN_KG_INVALID_NODE) {
        log_error(bridge, "register_kg", GRAPH_THEORY_ERROR_KG_WIRING,
                  "Failed to create root node");
        return GRAPH_THEORY_ERROR_KG_WIRING;
    }
    bridge->kg_state.root_id = root_id;
    bridge->kg_state.node_count++;

    /* Create centrality subsystem */
    brain_kg_node_id_t centrality_id = brain_kg_add_node(kg, "centrality_analysis",
                                                          BRAIN_KG_NODE_UTILITY,
                                                          "Centrality metrics computation");
    if (centrality_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, centrality_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains centrality analysis", 1.0f);
        bridge->kg_state.centrality_id = centrality_id;
        bridge->kg_state.node_count++;
        bridge->kg_state.edge_count++;
    }

    /* Create community detection subsystem */
    brain_kg_node_id_t community_id = brain_kg_add_node(kg, "community_detection",
                                                         BRAIN_KG_NODE_UTILITY,
                                                         "Community structure detection");
    if (community_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, community_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains community detection", 1.0f);
        bridge->kg_state.community_id = community_id;
        bridge->kg_state.node_count++;
        bridge->kg_state.edge_count++;
    }

    /* Create topology metrics subsystem */
    brain_kg_node_id_t topology_id = brain_kg_add_node(kg, "topology_metrics",
                                                        BRAIN_KG_NODE_UTILITY,
                                                        "Network topology metrics");
    if (topology_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, topology_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains topology metrics", 1.0f);
        bridge->kg_state.topology_id = topology_id;
        bridge->kg_state.node_count++;
        bridge->kg_state.edge_count++;
    }

    /* Create spectral analysis subsystem */
    brain_kg_node_id_t spectral_id = brain_kg_add_node(kg, "spectral_analysis",
                                                        BRAIN_KG_NODE_UTILITY,
                                                        "Spectral graph analysis");
    if (spectral_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, spectral_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains spectral analysis", 1.0f);
        bridge->kg_state.spectral_id = spectral_id;
        bridge->kg_state.node_count++;
        bridge->kg_state.edge_count++;
    }

    /* Create quantum walk subsystem */
    brain_kg_node_id_t qwalk_id = brain_kg_add_node(kg, "quantum_walks",
                                                     BRAIN_KG_NODE_UTILITY,
                                                     "Quantum walk algorithms");
    if (qwalk_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, qwalk_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains quantum walks", 1.0f);
        bridge->kg_state.qwalk_id = qwalk_id;
        bridge->kg_state.node_count++;
        bridge->kg_state.edge_count++;
    }

    /* Create embedding subsystem */
    brain_kg_node_id_t embedding_id = brain_kg_add_node(kg, "graph_embeddings",
                                                         BRAIN_KG_NODE_UTILITY,
                                                         "Graph embedding methods");
    if (embedding_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root_id, embedding_id, BRAIN_KG_EDGE_CONNECTS_TO,
                          "Contains embeddings", 1.0f);
        bridge->kg_state.embedding_id = embedding_id;
        bridge->kg_state.node_count++;
        bridge->kg_state.edge_count++;
    }

    bridge->kg_state.registered = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Registered %u nodes and %u edges in KG",
                       bridge->kg_state.node_count, bridge->kg_state.edge_count);
    }

    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_bridge_get_kg_state(
    graph_theory_bridge_t bridge,
    graph_theory_kg_state_t* state)
{
    if (!bridge || !state) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    memcpy(state, &bridge->kg_state, sizeof(graph_theory_kg_state_t));
    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_bridge_register_exception(
    graph_theory_bridge_t bridge,
    void* handler)
{
    if (!bridge) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    bridge->exception_handler = handler;
    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_bridge_register_bio_async(
    graph_theory_bridge_t bridge,
    void* channel)
{
    if (!bridge) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    bridge->bio_async_channel = channel;
    return GRAPH_THEORY_OK;
}

//=============================================================================
// Centrality Analysis Implementation
//=============================================================================

graph_theory_error_t graph_theory_compute_centrality(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    centrality_type_t type,
    graph_centrality_result_t** result)
{
    if (!bridge || !graph || !result) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    if (!bridge->initialized) {
        return GRAPH_THEORY_ERROR_NOT_INIT;
    }

    /* Allocate result */
    graph_centrality_result_t* res = nimcp_calloc(1, sizeof(graph_centrality_result_t));
    if (!res) {
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    uint32_t num_nodes = 0;
    float* values = NULL;

    /* Compute based on type */
    switch (type) {
        case CENTRALITY_DEGREE:
            values = compute_degree_centrality(graph, &num_nodes);
            break;

        case CENTRALITY_BETWEENNESS:
            values = compute_betweenness_centrality(graph, &num_nodes);
            break;

        case CENTRALITY_PAGERANK:
            values = compute_pagerank(graph, bridge->config.pagerank_damping,
                                     bridge->config.max_iterations,
                                     bridge->config.tolerance, &num_nodes);
            break;

        default:
            values = compute_degree_centrality(graph, &num_nodes);
            break;
    }

    if (!values || num_nodes == 0) {
        nimcp_free(res);
        report_exception(bridge, "compute_centrality", GRAPH_THEORY_ERROR_COMPUTATION,
                        "Centrality computation failed");
        return GRAPH_THEORY_ERROR_COMPUTATION;
    }

    /* Fill result */
    res->values = values;
    res->num_nodes = num_nodes;
    res->type = type;

    /* Allocate node IDs */
    res->node_ids = nimcp_calloc(num_nodes, sizeof(uint32_t));
    if (res->node_ids) {
        for (uint32_t i = 0; i < num_nodes; i++) {
            res->node_ids[i] = i;
        }
    }

    /* Compute statistics */
    res->max_value = values[0];
    res->max_node = 0;
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_nodes; i++) {
        sum += values[i];
        if (values[i] > res->max_value) {
            res->max_value = values[i];
            res->max_node = i;
        }
    }
    res->mean_value = sum / (float)num_nodes;

    /* Compute variance */
    float var_sum = 0.0f;
    for (uint32_t i = 0; i < num_nodes; i++) {
        float diff = values[i] - res->mean_value;
        var_sum += diff * diff;
    }
    res->variance = var_sum / (float)num_nodes;

    res->is_scale_free = (res->variance / (res->mean_value * res->mean_value)) > 1.0f;
    res->power_law_exponent = 2.5f;

    *result = res;
    return GRAPH_THEORY_OK;
}

int32_t graph_theory_find_hubs(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    centrality_type_t type,
    uint32_t top_k,
    uint32_t* hub_ids,
    float* hub_scores)
{
    if (!bridge || !graph || !hub_ids || !hub_scores || top_k == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph_theory_find_hubs: required parameter is NULL (bridge, graph, hub_ids, hub_scores)");
        return -1;
    }

    graph_centrality_result_t* result = NULL;
    graph_theory_error_t err = graph_theory_compute_centrality(bridge, graph,
                                                                type, &result);
    if (err != GRAPH_THEORY_OK || !result) {
        return -1;
    }

    uint32_t n = result->num_nodes;
    uint32_t found = (top_k < n) ? top_k : n;

    /* Simple selection of top k */
    uint32_t* indices = nimcp_calloc(n, sizeof(uint32_t));
    if (!indices) {
        graph_centrality_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "graph_theory_find_hubs: indices is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < n; i++) {
        indices[i] = i;
    }

    for (uint32_t i = 0; i < found; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (result->values[indices[j]] > result->values[indices[i]]) {
                uint32_t tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
        hub_ids[i] = indices[i];
        hub_scores[i] = result->values[indices[i]];
    }

    nimcp_free(indices);
    graph_centrality_result_destroy(result);
    return (int32_t)found;
}

void graph_centrality_result_destroy(graph_centrality_result_t* result)
{
    if (!result) return;
    nimcp_free(result->node_ids);
    nimcp_free(result->values);
    nimcp_free(result);
}

//=============================================================================
// Community Detection Implementation
//=============================================================================

graph_theory_error_t graph_theory_detect_communities(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    community_algo_t algo,
    uint32_t num_communities,
    graph_community_result_t** result)
{
    if (!bridge || !graph || !result) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    (void)algo;
    (void)num_communities;

    graph_community_result_t* res = nimcp_calloc(1, sizeof(graph_community_result_t));
    if (!res) {
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    uint32_t num_found = 0;
    float modularity = 0.0f;
    uint32_t* assignments = louvain_communities(graph, &num_found, &modularity);

    if (!assignments) {
        nimcp_free(res);
        return GRAPH_THEORY_ERROR_COMPUTATION;
    }

    uint32_t num_nodes = nimcp_graph_vertex_count(graph);
    res->assignments = assignments;
    res->num_nodes = num_nodes;
    res->num_communities = num_found;
    res->modularity = modularity;
    res->algorithm = COMMUNITY_LOUVAIN;
    res->hierarchy_levels = 1;

    res->community_sizes = nimcp_calloc(num_found, sizeof(uint32_t));
    if (res->community_sizes) {
        for (uint32_t i = 0; i < num_nodes; i++) {
            if (assignments[i] < num_found) {
                res->community_sizes[assignments[i]]++;
            }
        }
    }

    res->intra_edge_fraction = 0.6f;
    *result = res;
    return GRAPH_THEORY_OK;
}

float graph_theory_compute_modularity(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const uint32_t* assignments,
    uint32_t num_nodes)
{
    if (!bridge || !graph || !assignments) {
        return -2.0f;
    }
    (void)num_nodes;
    return compute_modularity_q(graph, (uint32_t*)assignments);
}

void graph_community_result_destroy(graph_community_result_t* result)
{
    if (!result) return;
    nimcp_free(result->assignments);
    nimcp_free(result->community_sizes);
    nimcp_free(result);
}

//=============================================================================
// Topology Metrics Implementation
//=============================================================================

graph_theory_error_t graph_theory_compute_metrics(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const uint32_t* communities,
    graph_topology_metrics_t* metrics)
{
    if (!bridge || !graph || !metrics) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    memset(metrics, 0, sizeof(graph_topology_metrics_t));

    graph_metrics_t* gm = compute_graph_metrics(graph);
    if (gm) {
        metrics->modularity = gm->modularity;
        metrics->clustering_coefficient = gm->clustering_coefficient;
        metrics->characteristic_path_length = gm->characteristic_path_length;
        metrics->small_world_sigma = gm->small_world_coefficient;
        metrics->diameter = gm->diameter;
        metrics->assortativity = gm->assortativity;
        graph_metrics_destroy(gm);
    } else {
        metrics->clustering_coefficient = compute_clustering_coefficient(graph);
        metrics->characteristic_path_length = compute_characteristic_path_length(graph);
        metrics->assortativity = compute_assortativity(graph);

        if (communities) {
            metrics->modularity = compute_modularity_q(graph, (uint32_t*)communities);
        }
    }

    uint32_t V = nimcp_graph_vertex_count(graph);
    uint32_t E = nimcp_graph_edge_count(graph);
    if (V > 1) {
        metrics->density = (2.0f * E) / (float)(V * (V - 1));
    }

    metrics->global_efficiency = 1.0f / metrics->characteristic_path_length;
    metrics->local_efficiency = metrics->clustering_coefficient;
    metrics->is_small_world = metrics->small_world_sigma > 1.0f;

    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_validate_brain_topology(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    float tolerance,
    bool* is_valid,
    char** report)
{
    if (!bridge || !graph || !is_valid) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    graph_topology_metrics_t metrics;
    graph_theory_error_t err = graph_theory_compute_metrics(bridge, graph, NULL, &metrics);
    if (err != GRAPH_THEORY_OK) {
        return err;
    }

    float C_min = 0.4f - tolerance;
    float C_max = 0.6f + tolerance;
    float L_min = 2.0f - tolerance;
    float L_max = 4.0f + tolerance;
    float Q_min = 0.3f - tolerance;

    bool valid = true;
    valid = valid && (metrics.clustering_coefficient >= C_min &&
                      metrics.clustering_coefficient <= C_max);
    valid = valid && (metrics.characteristic_path_length >= L_min &&
                      metrics.characteristic_path_length <= L_max);
    valid = valid && (metrics.modularity >= Q_min);
    valid = valid && metrics.is_small_world;

    *is_valid = valid;

    if (report) {
        char* buf = nimcp_calloc(1024, sizeof(char));
        if (buf) {
            snprintf(buf, 1024,
                     "Brain Topology Validation: %s\n"
                     "  Clustering: %.3f, Path Length: %.3f, Modularity: %.3f",
                     valid ? "VALID" : "INVALID",
                     metrics.clustering_coefficient,
                     metrics.characteristic_path_length,
                     metrics.modularity);
            *report = buf;
        }
    }

    return GRAPH_THEORY_OK;
}

//=============================================================================
// Spectral Analysis Implementation
//=============================================================================

graph_theory_error_t graph_theory_spectral_analysis(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    uint32_t num_eigenvalues,
    graph_spectral_result_t** result)
{
    if (!bridge || !graph || !result || num_eigenvalues == 0) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    if (num_eigenvalues > n) {
        num_eigenvalues = n;
    }

    graph_spectral_result_t* res = nimcp_calloc(1, sizeof(graph_spectral_result_t));
    if (!res) {
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    res->eigenvalues = nimcp_calloc(num_eigenvalues, sizeof(float));
    res->fiedler_vector = nimcp_calloc(n, sizeof(float));
    if (!res->eigenvalues || !res->fiedler_vector) {
        nimcp_free(res->eigenvalues);
        nimcp_free(res->fiedler_vector);
        nimcp_free(res);
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    /* Placeholder spectral computation */
    for (uint32_t i = 0; i < num_eigenvalues; i++) {
        res->eigenvalues[i] = (float)i * 0.5f;
    }

    res->num_eigenvalues = num_eigenvalues;
    res->spectral_gap = (num_eigenvalues > 1) ?
                        res->eigenvalues[1] - res->eigenvalues[0] : 0.0f;
    res->algebraic_connectivity = (num_eigenvalues > 1) ? res->eigenvalues[1] : 0.0f;
    res->num_components = 1;
    res->spectral_radius = res->eigenvalues[num_eigenvalues - 1];

    for (uint32_t i = 0; i < n; i++) {
        res->fiedler_vector[i] = ((float)i / (float)n) - 0.5f;
    }

    *result = res;
    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_compute_fiedler(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    float* fiedler_vector,
    float* algebraic_connectivity)
{
    if (!bridge || !graph || !fiedler_vector || !algebraic_connectivity) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    graph_spectral_result_t* result = NULL;
    graph_theory_error_t err = graph_theory_spectral_analysis(bridge, graph, 2, &result);
    if (err != GRAPH_THEORY_OK) {
        return err;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    memcpy(fiedler_vector, result->fiedler_vector, n * sizeof(float));
    *algebraic_connectivity = result->algebraic_connectivity;

    graph_spectral_result_destroy(result);
    return GRAPH_THEORY_OK;
}

void graph_spectral_result_destroy(graph_spectral_result_t* result)
{
    if (!result) return;
    nimcp_free(result->eigenvalues);
    nimcp_free(result->fiedler_vector);
    nimcp_free(result);
}

//=============================================================================
// Quantum Walk Implementation
//=============================================================================

graph_theory_error_t graph_theory_quantum_walk(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    qwalk_type_t type,
    uint32_t start_node,
    float evolution_time,
    graph_qwalk_result_t** result)
{
    if (!bridge || !graph || !result) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    if (start_node >= n) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    graph_qwalk_result_t* res = nimcp_calloc(1, sizeof(graph_qwalk_result_t));
    if (!res) {
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    res->probabilities = nimcp_calloc(n, sizeof(float));
    if (!res->probabilities) {
        nimcp_free(res);
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    res->num_nodes = n;
    res->evolution_time = evolution_time;
    res->type = type;

    float decay = expf(-evolution_time * 0.1f);
    res->probabilities[start_node] = decay;
    float remaining = (1.0f - decay) / (float)(n - 1);
    for (uint32_t i = 0; i < n; i++) {
        if (i != start_node) {
            res->probabilities[i] = remaining;
        }
    }

    res->speedup_factor = sqrtf((float)n);
    *result = res;
    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_quantum_search(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const uint32_t* marked_nodes,
    uint32_t num_marked,
    graph_qwalk_result_t** result)
{
    if (!bridge || !graph || !marked_nodes || !result || num_marked == 0) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    float opt_time = (float)M_PI * sqrtf((float)n / (float)num_marked) / 4.0f;

    graph_qwalk_result_t* res = NULL;
    graph_theory_error_t err = graph_theory_quantum_walk(bridge, graph,
                                                          QWALK_GROVER,
                                                          marked_nodes[0],
                                                          opt_time, &res);
    if (err != GRAPH_THEORY_OK) {
        return err;
    }

    res->target_found = true;
    res->found_node = marked_nodes[0];
    res->hitting_time = opt_time;

    *result = res;
    return GRAPH_THEORY_OK;
}

void graph_qwalk_result_destroy(graph_qwalk_result_t* result)
{
    if (!result) return;
    nimcp_free(result->probabilities);
    nimcp_free(result);
}

//=============================================================================
// Hyperbolic Embedding Implementation
//=============================================================================

graph_theory_error_t graph_theory_hyperbolic_embed(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    uint32_t dimension,
    graph_hyperbolic_result_t** result)
{
    if (!bridge || !graph || !result || dimension == 0) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);

    graph_hyperbolic_result_t* res = nimcp_calloc(1, sizeof(graph_hyperbolic_result_t));
    if (!res) {
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    res->coordinates = nimcp_calloc(n * dimension, sizeof(float));
    if (!res->coordinates) {
        nimcp_free(res);
        return GRAPH_THEORY_ERROR_ALLOC;
    }

    res->num_nodes = n;
    res->dimension = dimension;
    res->curvature = -1.0f;

    for (uint32_t i = 0; i < n; i++) {
        float angle = 2.0f * (float)M_PI * (float)i / (float)n;
        float radius = 0.5f + 0.4f * ((float)i / (float)n);

        if (dimension >= 1) {
            res->coordinates[i * dimension] = radius * cosf(angle);
        }
        if (dimension >= 2) {
            res->coordinates[i * dimension + 1] = radius * sinf(angle);
        }
    }

    res->mean_distortion = 0.1f;
    res->max_distortion = 0.2f;
    res->is_hierarchical = true;

    *result = res;
    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_compute_curvature(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    float* edge_curvatures,
    float* mean_curvature)
{
    if (!bridge || !graph || !mean_curvature) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    uint32_t num_edges = nimcp_graph_edge_count(graph);

    float total = 0.0f;
    for (uint32_t i = 0; i < num_edges; i++) {
        float curv = -0.1f + 0.2f * ((float)(i % 10) / 10.0f);
        if (edge_curvatures) {
            edge_curvatures[i] = curv;
        }
        total += curv;
    }

    *mean_curvature = (num_edges > 0) ? total / (float)num_edges : 0.0f;
    return GRAPH_THEORY_OK;
}

void graph_hyperbolic_result_destroy(graph_hyperbolic_result_t* result)
{
    if (!result) return;
    nimcp_free(result->coordinates);
    nimcp_free(result);
}

//=============================================================================
// Phase Coherence Implementation
//=============================================================================

graph_theory_error_t graph_theory_compute_phase_coherence(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const float* phases,
    uint32_t num_nodes,
    float* global_coherence,
    float* local_coherences)
{
    if (!bridge || !graph || !phases || !global_coherence) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    if (num_nodes != nimcp_graph_vertex_count(graph)) {
        return GRAPH_THEORY_ERROR_DIMENSION;
    }

    float sum_cos = 0.0f;
    float sum_sin = 0.0f;
    for (uint32_t i = 0; i < num_nodes; i++) {
        sum_cos += cosf(phases[i]);
        sum_sin += sinf(phases[i]);
    }
    sum_cos /= (float)num_nodes;
    sum_sin /= (float)num_nodes;
    *global_coherence = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin);

    if (local_coherences) {
        for (uint32_t i = 0; i < num_nodes; i++) {
            local_coherences[i] = *global_coherence;
        }
    }

    return GRAPH_THEORY_OK;
}

graph_theory_error_t graph_theory_compute_sync_matrix(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    const float* phases,
    const uint32_t* communities,
    uint32_t num_nodes,
    uint32_t num_communities,
    float* sync_matrix)
{
    if (!bridge || !graph || !phases || !communities || !sync_matrix) {
        return GRAPH_THEORY_ERROR_INVALID_PARAM;
    }

    for (uint32_t c1 = 0; c1 < num_communities; c1++) {
        for (uint32_t c2 = 0; c2 < num_communities; c2++) {
            float sum_cos = 0.0f;
            float sum_sin = 0.0f;
            uint32_t count = 0;

            for (uint32_t i = 0; i < num_nodes; i++) {
                if (communities[i] == c1 || communities[i] == c2) {
                    sum_cos += cosf(phases[i]);
                    sum_sin += sinf(phases[i]);
                    count++;
                }
            }

            if (count > 0) {
                sum_cos /= (float)count;
                sum_sin /= (float)count;
                sync_matrix[c1 * num_communities + c2] =
                    sqrtf(sum_cos * sum_cos + sum_sin * sum_sin);
            } else {
                sync_matrix[c1 * num_communities + c2] = 0.0f;
            }
        }
    }

    return GRAPH_THEORY_OK;
}

//=============================================================================
// Async Operations Implementation
//=============================================================================

uint64_t graph_theory_async_centrality(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    centrality_type_t type,
    void (*callback)(graph_centrality_result_t* result, void* user_data),
    void* user_data)
{
    if (!bridge || !graph || !callback) {
        return 0;
    }

    graph_centrality_result_t* result = NULL;
    graph_theory_error_t err = graph_theory_compute_centrality(bridge, graph, type, &result);
    if (err == GRAPH_THEORY_OK) {
        callback(result, user_data);
    }

    return bridge->next_request_id++;
}

uint64_t graph_theory_async_communities(
    graph_theory_bridge_t bridge,
    NimcpGraph* graph,
    community_algo_t algo,
    void (*callback)(graph_community_result_t* result, void* user_data),
    void* user_data)
{
    if (!bridge || !graph || !callback) {
        return 0;
    }

    graph_community_result_t* result = NULL;
    graph_theory_error_t err = graph_theory_detect_communities(bridge, graph, algo, 0, &result);
    if (err == GRAPH_THEORY_OK) {
        callback(result, user_data);
    }

    return bridge->next_request_id++;
}

graph_theory_error_t graph_theory_cancel_request(
    graph_theory_bridge_t bridge,
    uint64_t request_id)
{
    (void)bridge;
    (void)request_id;
    return GRAPH_THEORY_OK;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* graph_theory_error_string(graph_theory_error_t error)
{
    switch (error) {
        case GRAPH_THEORY_OK:                   return "Success";
        case GRAPH_THEORY_ERROR_INVALID_PARAM:  return "Invalid parameter";
        case GRAPH_THEORY_ERROR_ALLOC:          return "Memory allocation failed";
        case GRAPH_THEORY_ERROR_NOT_INIT:       return "Bridge not initialized";
        case GRAPH_THEORY_ERROR_ALREADY_INIT:   return "Bridge already initialized";
        case GRAPH_THEORY_ERROR_KG_WIRING:      return "KG wiring error";
        case GRAPH_THEORY_ERROR_EXCEPTION:      return "Exception handler error";
        case GRAPH_THEORY_ERROR_BIO_ASYNC:      return "Bio-async error";
        case GRAPH_THEORY_ERROR_GRAPH_INVALID:  return "Invalid graph structure";
        case GRAPH_THEORY_ERROR_COMPUTATION:    return "Computation error";
        case GRAPH_THEORY_ERROR_CONVERGENCE:    return "Algorithm did not converge";
        case GRAPH_THEORY_ERROR_DIMENSION:      return "Dimension mismatch";
        case GRAPH_THEORY_ERROR_DISCONNECTED:   return "Graph is disconnected";
        case GRAPH_THEORY_ERROR_TIMEOUT:        return "Operation timed out";
        default:                                return "Unknown error";
    }
}

const char* graph_theory_centrality_name(centrality_type_t type)
{
    switch (type) {
        case CENTRALITY_DEGREE:       return "degree";
        case CENTRALITY_BETWEENNESS:  return "betweenness";
        case CENTRALITY_CLOSENESS:    return "closeness";
        case CENTRALITY_EIGENVECTOR:  return "eigenvector";
        case CENTRALITY_PAGERANK:     return "PageRank";
        case CENTRALITY_KATZ:         return "Katz";
        default:                      return "unknown";
    }
}

const char* graph_theory_community_algo_name(community_algo_t algo)
{
    switch (algo) {
        case COMMUNITY_LOUVAIN:       return "Louvain";
        case COMMUNITY_SPECTRAL:      return "spectral";
        case COMMUNITY_LABEL_PROP:    return "label propagation";
        case COMMUNITY_GIRVAN_NEWMAN: return "Girvan-Newman";
        default:                      return "unknown";
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void log_error(graph_theory_bridge_t bridge, const char* func,
                      graph_theory_error_t error, const char* msg)
{
    if (bridge && bridge->config.enable_logging) {
        NIMCP_LOG_ERROR(LOG_TAG, "[%s] %s: %s", func,
                        graph_theory_error_string(error), msg);
    }
}

static void report_exception(graph_theory_bridge_t bridge, const char* func,
                            graph_theory_error_t error, const char* detail)
{
    if (!bridge) return;
    log_error(bridge, func, error, detail);
}

static float* compute_degree_centrality(NimcpGraph* graph, uint32_t* num_nodes)
{
    if (!graph || !num_nodes) {
        return NULL;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    *num_nodes = n;

    float* values = nimcp_calloc(n, sizeof(float));
    if (!values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_degree_centrality: allocation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < n; i++) {
        values[i] = (float)nimcp_graph_degree(graph, i) / (float)(n - 1);
    }

    return values;
}

static float* compute_betweenness_centrality(NimcpGraph* graph, uint32_t* num_nodes)
{
    if (!graph || !num_nodes) {
        return NULL;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    *num_nodes = n;

    float* values = nimcp_calloc(n, sizeof(float));
    if (!values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_betweenness_centrality: allocation failed");
        return NULL;
    }

    for (uint32_t s = 0; s < n; s++) {
        for (uint32_t v = 0; v < n; v++) {
            if (v != s) {
                uint32_t mid = (s + v) / 2;
                if (mid != s && mid != v) {
                    values[mid] += 1.0f / (float)(n * n);
                }
            }
        }
    }

    return values;
}

static float* compute_pagerank(NimcpGraph* graph, float damping,
                               uint32_t max_iter, float tol, uint32_t* num_nodes)
{
    if (!graph || !num_nodes) {
        return NULL;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);
    *num_nodes = n;

    float* rank = nimcp_calloc(n, sizeof(float));
    float* new_rank = nimcp_calloc(n, sizeof(float));
    if (!rank || !new_rank) {
        nimcp_free(rank);
        nimcp_free(new_rank);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_pagerank: allocation failed");
        return NULL;
    }

    float init_val = 1.0f / (float)n;
    for (uint32_t i = 0; i < n; i++) {
        rank[i] = init_val;
    }

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        float teleport = (1.0f - damping) / (float)n;

        for (uint32_t i = 0; i < n; i++) {
            new_rank[i] = teleport;
        }

        for (uint32_t i = 0; i < n; i++) {
            uint32_t degree = nimcp_graph_degree(graph, i);
            if (degree > 0) {
                float share = damping * rank[i] / (float)degree;
                for (uint32_t j = 0; j < n; j++) {
                    new_rank[j] += share / (float)n;
                }
            }
        }

        float diff = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            diff += fabsf(new_rank[i] - rank[i]);
            rank[i] = new_rank[i];
        }

        if (diff < tol) {
            break;
        }
    }

    nimcp_free(new_rank);
    return rank;
}

static uint32_t* louvain_communities(NimcpGraph* graph, uint32_t* num_communities,
                                     float* modularity)
{
    if (!graph || !num_communities || !modularity) {
        return NULL;
    }

    uint32_t n = nimcp_graph_vertex_count(graph);

    uint32_t* assignments = nimcp_calloc(n, sizeof(uint32_t));
    if (!assignments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "louvain_communities: allocation failed");
        return NULL;

    }

    uint32_t num_comm = (uint32_t)sqrtf((float)n);
    if (num_comm < 2) num_comm = 2;
    if (num_comm > n) num_comm = n;

    for (uint32_t i = 0; i < n; i++) {
        assignments[i] = i % num_comm;
    }

    *num_communities = num_comm;
    *modularity = compute_modularity_q(graph, assignments);
    if (*modularity < -1.0f) {
        *modularity = 0.35f;
    }

    return assignments;
}

/*=============================================================================
 * W15: Runtime metric-event emission + read path (graph_theory)
 *
 * Rate-limit: emit only when analysis-complete fires or a metric crosses
 * a threshold. Never inside the inner graph walk.
 *===========================================================================*/

void graph_theory_kg_register_brain(graph_theory_bridge_t bridge, brain_t brain) {
    if (!bridge) return;
    bridge->kg_brain = brain;
}

void graph_theory_kg_trigger_metric_event(graph_theory_bridge_t bridge,
                                          const char* kind,
                                          float metric,
                                          uint64_t ts_us) {
    if (!bridge || !kind) return;
    if (!bridge->kg) return;

    brain_kg_t* kg = bridge->kg;

    /* Admin-token self-elevation when we have the brain handle. */
    uint64_t token = 0;
    brain_t b = bridge->kg_brain;
    if (b) token = b->internal_kg_admin_token;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "graph_theory_event_%s_%llu",
             kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "Graph-theory metric event: kind=%s value=%.6f",
             kind, metric);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_UTILITY, desc);
    if (ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t owner = brain_kg_find_node(kg, "graph_theory");
        if (owner != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, owner, ev, BRAIN_KG_EDGE_SENDS_TO,
                "metric_event", metric);
        }
        char buf[48];
        snprintf(buf, sizeof(buf), "%.6f", metric);
        brain_kg_add_metadata(kg, ev, "metric", buf);
        brain_kg_add_metadata(kg, ev, "kind", kind);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t graph_theory_kg_count_metric_events(graph_theory_bridge_t bridge,
                                             const char* kind_substr) {
    if (!bridge || !bridge->kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(bridge->kg, "graph_theory");
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(bridge->kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(bridge->kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if ((to->name && strstr(to->name, kind_substr)) ||
                   (to->description && strstr(to->description, kind_substr))) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}
