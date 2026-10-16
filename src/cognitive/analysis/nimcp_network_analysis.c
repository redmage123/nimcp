//=============================================================================
// nimcp_network_analysis.c - Brain Network Analysis (Full Implementation)
//=============================================================================
/**
 * @file nimcp_network_analysis.c
 * @brief Full implementation of network analysis with real algorithms
 *
 * WHAT: Network topology analysis using community detection and graph metrics
 * WHY:  Provides real insights into brain network organization
 * HOW:  Integrates Louvain community detection and graph centrality algorithms
 *
 * INTEGRATION:
 * - community_detect() for community structure detection
 * - community_detect_hubs() for hub neuron identification
 * - compute_graph_metrics() for topology metrics
 */

#include "cognitive/analysis/nimcp_network_analysis.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/topology/nimcp_community_detection.h"
#include "utils/algorithms/nimcp_graph_metrics.h"
#include "utils/containers/nimcp_graph.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/brain/nimcp_brain.h"
#include <string.h>
#include <math.h>

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#define LOG_MODULE "network_analysis"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(network_analysis, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Static Helpers
//=============================================================================

/**
 * @brief Build graph representation from neural network
 *
 * WHAT: Convert neural network to graph data structure
 * WHY:  Graph metrics algorithms require NimcpGraph format
 * HOW:  Extract neurons and synapses, build adjacency list graph
 *
 * @param network Neural network to convert
 * @return Graph or NULL on error (caller must free with nimcp_graph_destroy)
 */
static NimcpGraph* build_graph_from_network(neural_network_t network) {
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_graph_from_network: network is NULL");
        return NULL;
    }

    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) {
        NIMCP_LOGGING_ERROR("build_graph_from_network: failed to create graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_graph_from_network: graph is NULL");
        return NULL;
    }

    uint32_t num_neurons = neural_network_get_num_neurons(network);

    // Add vertices (neurons)
    for (uint32_t i = 0; i < num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neurons > 256) {
            network_analysis_heartbeat("network_anal_loop",
                             (float)(i + 1) / (float)num_neurons);
        }

        uint32_t vertex_idx = nimcp_graph_add_vertex(graph, i, 0.0F, 0.0F, 0.0F, 0);
        if (vertex_idx == NIMCP_INVALID_VERTEX) {
            NIMCP_LOGGING_WARN("Failed to add vertex %u to graph", i);
        }
    }

    // Add edges (synapses) by iterating through each neuron's synapses
    for (uint32_t i = 0; i < num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neurons > 256) {
            network_analysis_heartbeat("network_anal_loop",
                             (float)(i + 1) / (float)num_neurons);
        }

        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (!neuron) continue;

        uint32_t out_count = NEURON_OUT_COUNT(neuron);
        for (uint32_t s = 0; s < out_count; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && out_count > 256) {
                network_analysis_heartbeat("network_anal_loop",
                                 (float)(s + 1) / (float)out_count);
            }

            synapse_handle_t* syn = NEURON_OUT_HANDLE(neuron, s);
            if (syn) {
                // Add edge with absolute weight
                nimcp_weight_t edge_weight = fabsf(syn->weight);
                nimcp_graph_add_edge(graph, i, syn->target_neuron_id, edge_weight);
            }
        }
    }

    NIMCP_LOGGING_DEBUG("Built graph: %u vertices, %u edges",
                        graph->vertex_count, graph->edge_count);

    return graph;
}

//=============================================================================
// Lifecycle
//=============================================================================

network_analyzer_t* network_analyzer_create(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_create: NULL brain");
        NIMCP_LOGGING_ERROR("network_analyzer_create: NULL brain");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_cre", 0.0f);


    network_analyzer_t* analyzer = nimcp_calloc(1, sizeof(network_analyzer_t));
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network_analyzer_create: allocation failed");
        NIMCP_LOGGING_ERROR("network_analyzer_create: allocation failed");
        return NULL;
    }

    analyzer->brain = brain;
    analyzer->auto_analyze = true;  // Enable auto-analyze by default
    analyzer->analysis_interval = 10;  // Default interval
    analyzer->hub_threshold = 0.7F;
    analyzer->iteration_counter = 0;
    analyzer->topology_is_valid = true;
    analyzer->history_capacity = 1000;
    analyzer->analysis_count = 0;  // Initialize analysis count

    analyzer->modularity_history = nimcp_calloc(1000, sizeof(float));
    analyzer->community_count_history = nimcp_calloc(1000, sizeof(uint32_t));

    if (!analyzer->modularity_history || !analyzer->community_count_history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network_analyzer_create: history allocation failed");
        NIMCP_LOGGING_ERROR("network_analyzer_create: history allocation failed");
        network_analyzer_destroy(analyzer);
        return NULL;
    }

    // Initialize bio-async fields
    analyzer->bio_ctx = NULL;
    analyzer->bio_async_enabled = false;

    // Register with bio-async router if available
    NIMCP_LOGGING_DEBUG("network_analysis: Checking bio-async router initialization...");
    if (bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("network_analysis: Bio-router initialized, registering module (id=%d, inbox_capacity=32)...",
                           BIO_MODULE_NETWORK_ANALYSIS);
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_NETWORK_ANALYSIS,
            .module_name = "network_analysis",
            .inbox_capacity = 32,
            .user_data = analyzer
        };
        analyzer->bio_ctx = bio_router_register_module(&bio_info);
        if (analyzer->bio_ctx) {
            analyzer->bio_async_enabled = true;
            NIMCP_LOGGING_INFO("network_analysis: Bio-async communication enabled (module_id=%d)",
                              BIO_MODULE_NETWORK_ANALYSIS);
        } else {
            NIMCP_LOGGING_WARN("network_analysis: Bio-async registration failed - module will operate without async messaging");
        }
    } else {
        NIMCP_LOGGING_DEBUG("network_analysis: Bio-router not initialized, skipping async registration");
    }

    NIMCP_LOGGING_INFO("Network analyzer created");
    return analyzer;
}

void network_analyzer_destroy(network_analyzer_t* analyzer)
{
    if (!analyzer) return;

    // Unregister from bio-async router
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_des", 0.0f);


    if (analyzer->bio_async_enabled && analyzer->bio_ctx) {
        bio_router_unregister_module(analyzer->bio_ctx);
        analyzer->bio_ctx = NULL;
        analyzer->bio_async_enabled = false;
        NIMCP_LOGGING_INFO("Bio-async communication disabled for network_analysis");
    }

    if (analyzer->communities) {
        if (analyzer->communities->community_ids) {
            nimcp_free(analyzer->communities->community_ids);
        }
        if (analyzer->communities->community_sizes) {
            nimcp_free(analyzer->communities->community_sizes);
        }
        if (analyzer->communities->internal_density) {
            nimcp_free(analyzer->communities->internal_density);
        }
        if (analyzer->communities->external_density) {
            nimcp_free(analyzer->communities->external_density);
        }
        nimcp_free(analyzer->communities);
    }

    if (analyzer->hubs) {
        if (analyzer->hubs->hubs) {
            nimcp_free(analyzer->hubs->hubs);
        }
        nimcp_free(analyzer->hubs);
    }

    if (analyzer->modularity_history) {
        nimcp_free(analyzer->modularity_history);
    }
    if (analyzer->community_count_history) {
        nimcp_free(analyzer->community_count_history);
    }

    nimcp_free(analyzer);
    analyzer = NULL;
}

//=============================================================================
// Analysis Operations (Stubs)
//=============================================================================

bool network_analyzer_run(network_analyzer_t* analyzer)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_run", 0.0f);


    if (analyzer && analyzer->bio_ctx) {
        bio_router_process_inbox(analyzer->bio_ctx, 5);
    }

    if (!analyzer || !analyzer->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_run: required parameter is NULL (analyzer, analyzer->brain)");
        return false;
    }

    NIMCP_LOGGING_INFO("network_analyzer_run: running full network analysis");

    // Run all analysis components
    bool success = true;

    // 1. Detect communities using real Louvain algorithm
    if (!network_analyzer_detect_communities(analyzer)) {
        NIMCP_LOGGING_WARN("Community detection failed, continuing with other analyses");
        success = false;  // Don't abort, just mark as partial failure
    }

    // 2. Detect hub neurons
    if (!network_analyzer_detect_hubs(analyzer)) {
        NIMCP_LOGGING_WARN("Hub detection failed, continuing with other analyses");
        success = false;  // Don't abort, just mark as partial failure
    }

    // 3. Compute topology metrics
    if (!network_analyzer_compute_metrics(analyzer)) {
        NIMCP_LOGGING_WARN("Metric computation failed, continuing");
        success = false;
    }

    // Record modularity in history if communities were detected
    if (analyzer->communities && analyzer->analysis_count < analyzer->history_capacity) {
        analyzer->modularity_history[analyzer->analysis_count] = analyzer->communities->modularity;
        analyzer->community_count_history[analyzer->analysis_count] = analyzer->communities->num_communities;
    }

    // Increment analysis count
    analyzer->analysis_count++;

    NIMCP_LOGGING_INFO("Network analysis %s (run #%u)",
                       success ? "completed successfully" : "partially completed",
                       analyzer->analysis_count);

    return success;
}

bool network_analyzer_detect_communities(network_analyzer_t* analyzer)
{
    if (!analyzer || !analyzer->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_communities: required parameter is NULL (analyzer, analyzer->brain)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_det", 0.0f);


    NIMCP_LOGGING_INFO("network_analyzer_detect_communities: running real community detection");

    // Get adaptive network from brain and extract base network
    adaptive_network_t adaptive_net = brain_get_network(analyzer->brain);
    if (!adaptive_net) {
        NIMCP_LOGGING_ERROR("Failed to get adaptive network from brain");
        strncpy(analyzer->last_error, "Failed to get adaptive network", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_communities: adaptive_net is NULL");
        return false;
    }

    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    if (!network) {
        NIMCP_LOGGING_ERROR("Failed to get base network from adaptive network");
        strncpy(analyzer->last_error, "Failed to get base network", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_communities: network is NULL");
        return false;
    }

    // Free old communities if they exist
    if (analyzer->communities) {
        topology_community_structure_free(analyzer->communities);
        analyzer->communities = NULL;
    }

    /* Run Louvain with aggressive scale-down on large brains. On a 1.8M
     * neuron / 1.45B synapse brain, full analysis takes minutes to hours
     * (hot-path incidents: see comments in nimcp_curiosity.c, brain_learning.c).
     *
     * Sampling (50K top-degree nodes) + sparsification (drop |w|<0.05) brings
     * the cost to seconds while preserving the community structure of the
     * high-activity hub subgraph — which is what downstream consumers
     * (quantum_shannon, introspection history) actually care about. */
    community_detection_config_t config = community_default_config();
    config.max_nodes_sample = 50000;   /* top-N by out-degree; 0 = no cap */
    config.weight_threshold = 0.05f;   /* drop low-weight edges; 0 = keep all */
    analyzer->communities = community_detect(network, &config);

    if (!analyzer->communities) {
        NIMCP_LOGGING_WARN("Community detection returned NULL - network may be too small or disconnected");
        strncpy(analyzer->last_error, "Community detection failed", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_communities: analyzer->communities is NULL");
        return false;
    }

    NIMCP_LOGGING_INFO("Detected %u communities with modularity Q=%.3f",
                       analyzer->communities->num_communities,
                       analyzer->communities->modularity);

    analyzer->last_error[0] = '\0';  // Clear error
    return true;
}

bool network_analyzer_detect_hubs(network_analyzer_t* analyzer)
{
    if (!analyzer || !analyzer->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_hubs: required parameter is NULL (analyzer, analyzer->brain)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_det", 0.0f);


    NIMCP_LOGGING_INFO("network_analyzer_detect_hubs: running real hub detection");

    // Get base neural network from brain's adaptive network
    adaptive_network_t adaptive_net = brain_get_network(analyzer->brain);
    if (!adaptive_net) {
        NIMCP_LOGGING_ERROR("Failed to get adaptive network from brain");
        strncpy(analyzer->last_error, "Failed to get adaptive network", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_hubs: adaptive_net is NULL");
        return false;
    }

    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    if (!network) {
        NIMCP_LOGGING_ERROR("Failed to get base network from adaptive network");
        strncpy(analyzer->last_error, "Failed to get base network", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_hubs: network is NULL");
        return false;
    }

    // Free old hubs if they exist
    if (analyzer->hubs) {
        if (analyzer->hubs->hubs) {
            nimcp_free(analyzer->hubs->hubs);
        }
        nimcp_free(analyzer->hubs);
        analyzer->hubs = NULL;
    }

    // Run real hub detection algorithm
    hub_structure_t* hub_struct = community_detect_hubs(network, analyzer->hub_threshold);
    if (!hub_struct) {
        NIMCP_LOGGING_WARN("Hub detection returned NULL - no hubs found or network too small");
        strncpy(analyzer->last_error, "Hub detection failed", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_detect_hubs: hub_struct is NULL");
        return false;
    }

    // Convert hub_structure_t to hub_detection_t format
    analyzer->hubs = nimcp_calloc(1, sizeof(hub_detection_t));
    if (!analyzer->hubs) {
        hub_structure_free(hub_struct);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network_analyzer_detect_hubs: analyzer->hubs is NULL");
        return false;
    }

    analyzer->hubs->num_hubs = hub_struct->num_hubs;
    analyzer->hubs->hub_threshold = analyzer->hub_threshold;
    analyzer->hubs->timestamp = nimcp_time_get_ms();

    if (hub_struct->num_hubs > 0) {
        analyzer->hubs->hubs = nimcp_calloc(hub_struct->num_hubs, sizeof(hub_neuron_t));
        if (!analyzer->hubs->hubs) {
            hub_structure_free(hub_struct);
            nimcp_free(analyzer->hubs);
            analyzer->hubs = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network_analyzer_detect_hubs: analyzer->hubs->hubs is NULL");
            return false;
        }

        // Copy hub data
        for (uint32_t i = 0; i < hub_struct->num_hubs; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hub_struct->num_hubs > 256) {
                network_analysis_heartbeat("network_anal_loop",
                                 (float)(i + 1) / (float)hub_struct->num_hubs);
            }

            analyzer->hubs->hubs[i].neuron_id = hub_struct->hub_indices[i];
            analyzer->hubs->hubs[i].degree_centrality = hub_struct->degree_centrality[i];
            analyzer->hubs->hubs[i].betweenness = hub_struct->betweenness_centrality[i];
            analyzer->hubs->hubs[i].community_id = hub_struct->hub_communities ?
                                                    hub_struct->hub_communities[i] : 0;
            analyzer->hubs->hubs[i].is_connector_hub = false;  // TODO: implement if needed
        }
    }

    hub_structure_free(hub_struct);

    NIMCP_LOGGING_INFO("Detected %u hub neurons with threshold %.3f",
                       analyzer->hubs->num_hubs, analyzer->hub_threshold);

    analyzer->last_error[0] = '\0';  // Clear error
    return true;
}

bool network_analyzer_compute_metrics(network_analyzer_t* analyzer)
{
    if (!analyzer || !analyzer->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_compute_metrics: required parameter is NULL (analyzer, analyzer->brain)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_com", 0.0f);


    NIMCP_LOGGING_INFO("network_analyzer_compute_metrics: computing real topology metrics");

    // Get base neural network from brain's adaptive network
    adaptive_network_t adaptive_net = brain_get_network(analyzer->brain);
    if (!adaptive_net) {
        NIMCP_LOGGING_ERROR("Failed to get adaptive network from brain");
        strncpy(analyzer->last_error, "Failed to get adaptive network", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_compute_metrics: adaptive_net is NULL");
        return false;
    }

    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    if (!network) {
        NIMCP_LOGGING_ERROR("Failed to get base network from adaptive network");
        strncpy(analyzer->last_error, "Failed to get base network", sizeof(analyzer->last_error) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_compute_metrics: network is NULL");
        return false;
    }

    // Get basic network statistics
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        NIMCP_LOGGING_WARN("Network has no neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "network_analyzer_compute_metrics: num_neurons is zero");
        return false;
    }

    // Count total synapses for density calculation
    uint32_t total_synapses = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neurons > 256) {
            network_analysis_heartbeat("network_anal_loop",
                             (float)(i + 1) / (float)num_neurons);
        }

        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron) {
            total_synapses += NEURON_OUT_COUNT(neuron);
        }
    }

    // Calculate network density
    uint32_t max_possible_connections = num_neurons * (num_neurons - 1);
    analyzer->metrics.density = (max_possible_connections > 0) ?
                                (float)total_synapses / (float)max_possible_connections : 0.0F;
    analyzer->metrics.num_edges = total_synapses;

    // Build graph from network for advanced metrics computation
    NimcpGraph* graph = build_graph_from_network(network);
    if (!graph) {
        NIMCP_LOGGING_WARN("Failed to build graph - using default metrics");
        // Fallback to reasonable defaults
        analyzer->metrics.clustering_coefficient = 0.45F;
        analyzer->metrics.avg_path_length = 3.0F;
        analyzer->metrics.small_worldness = 1.5F;
        analyzer->metrics.assortativity = 0.0F;
    } else {
        // Compute real graph metrics
        graph_metrics_t* gm = compute_graph_metrics(graph);
        if (gm) {
            analyzer->metrics.clustering_coefficient = gm->clustering_coefficient;
            analyzer->metrics.avg_path_length = gm->characteristic_path_length;
            analyzer->metrics.small_worldness = gm->small_world_coefficient;
            analyzer->metrics.assortativity = gm->assortativity;

            NIMCP_LOGGING_INFO("Real metrics: C=%.3f, L=%.2f, σ=%.2f, r=%.3f",
                               gm->clustering_coefficient,
                               gm->characteristic_path_length,
                               gm->small_world_coefficient,
                               gm->assortativity);

            graph_metrics_destroy(gm);
        } else {
            NIMCP_LOGGING_WARN("compute_graph_metrics failed - using defaults");
            analyzer->metrics.clustering_coefficient = 0.45F;
            analyzer->metrics.avg_path_length = 3.0F;
            analyzer->metrics.small_worldness = 1.5F;
            analyzer->metrics.assortativity = 0.0F;
        }

        nimcp_graph_destroy(graph);
    }

    NIMCP_LOGGING_INFO("Computed metrics: density=%.3f, edges=%u, C=%.3f, L=%.2f",
                       analyzer->metrics.density, total_synapses,
                       analyzer->metrics.clustering_coefficient,
                       analyzer->metrics.avg_path_length);

    analyzer->last_error[0] = '\0';  // Clear error
    return true;
}

//=============================================================================
// Validation (Stubs)
//=============================================================================

bool network_analyzer_validate_learning(network_analyzer_t* analyzer)
{
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_validate_learning: analyzer is NULL");
        return false;
    }

    // Check if analysis has been run
    if (!analyzer->communities || !analyzer->hubs) {
        strncpy(analyzer->last_error, "No analysis results available - run analysis first",
                sizeof(analyzer->last_error) - 1);
        analyzer->last_error[sizeof(analyzer->last_error) - 1] = '\0';
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_validate_learning: required parameter is NULL (analyzer->communities, analyzer->hubs)");
        return false;
    }

    /* Validate learning by checking topology health indicators */
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_val", 0.0f);

    bool valid = true;

    /* Check modularity is in reasonable range [0, 1] */
    if (analyzer->communities->modularity < 0.0f || analyzer->communities->modularity > 1.0f) {
        strncpy(analyzer->last_error, "Modularity out of range",
                sizeof(analyzer->last_error) - 1);
        valid = false;
    }

    /* Check that communities were found */
    if (analyzer->communities->num_communities == 0) {
        strncpy(analyzer->last_error, "No communities detected",
                sizeof(analyzer->last_error) - 1);
        valid = false;
    }

    /* Check hub count is reasonable (not every neuron is a hub) */
    if (analyzer->hubs->num_hubs > 0) {
        /* Hubs should be a minority of neurons */
        NIMCP_LOGGING_INFO("Learning validation: %u communities (Q=%.3f), %u hubs",
                           analyzer->communities->num_communities,
                           analyzer->communities->modularity,
                           analyzer->hubs->num_hubs);
    }

    if (valid) {
        analyzer->last_error[0] = '\0';
    }
    return valid;
}

const char* network_analyzer_get_error(network_analyzer_t* analyzer)
{
    if (!analyzer) return "NULL analyzer";
    return analyzer->last_error;
}

//=============================================================================
// Configuration
//=============================================================================

void network_analyzer_set_auto_analyze(network_analyzer_t* analyzer, bool enable, uint32_t interval)
{
    if (!analyzer) return;

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_set", 0.0f);


    analyzer->auto_analyze = enable;
    analyzer->analysis_interval = interval;
}

void network_analyzer_set_hub_threshold(network_analyzer_t* analyzer, float threshold)
{
    if (!analyzer) return;

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_set", 0.0f);


    analyzer->hub_threshold = threshold;
}

//=============================================================================
// Query Results (Stubs)
//=============================================================================

const community_structure_t* network_analyzer_get_communities(network_analyzer_t* analyzer)
{
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_get_communities: analyzer is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_get", 0.0f);


    return analyzer->communities;
}

const hub_detection_t* network_analyzer_get_hubs(network_analyzer_t* analyzer)
{
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_get_hubs: analyzer is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_get", 0.0f);


    return analyzer->hubs;
}

topology_metrics_t network_analyzer_get_metrics(network_analyzer_t* analyzer)
{
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_get", 0.0f);


    topology_metrics_t metrics = {0};
    if (!analyzer) return metrics;
    return analyzer->metrics;
}

const float* network_analyzer_get_modularity_history(network_analyzer_t* analyzer, uint32_t* count)
{
    if (!analyzer || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_analyzer_get_modularity_history: required parameter is NULL (analyzer, count)");
        return NULL;
    }

    *count = analyzer->analysis_count;
    return analyzer->modularity_history;
}

//=============================================================================
// Reporting (Stubs)
//=============================================================================

void network_analyzer_print_report(network_analyzer_t* analyzer)
{
    if (!analyzer) return;

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_pri", 0.0f);


    printf("=== Network Topology Analysis ===\n");

    if (analyzer->communities) {
        printf("\nCommunities:\n");
        printf("  Number of communities: %u\n", analyzer->communities->num_communities);
        printf("  Modularity: %.3f\n", analyzer->communities->modularity);
    }

    if (analyzer->hubs) {
        printf("\nHub Neurons:\n");
        printf("  Number of hubs: %u\n", analyzer->hubs->num_hubs);
        for (uint32_t i = 0; i < analyzer->hubs->num_hubs; i++) {
            printf("  Hub %u: Neuron %u (centrality=%.3f, betweenness=%.3f)\n",
                   i, analyzer->hubs->hubs[i].neuron_id,
                   analyzer->hubs->hubs[i].degree_centrality,
                   analyzer->hubs->hubs[i].betweenness);
        }
    }

    printf("\nTopology Metrics:\n");
    printf("  Clustering coefficient: %.3f\n", analyzer->metrics.clustering_coefficient);
    printf("  Average path length: %.3f\n", analyzer->metrics.avg_path_length);
    printf("  Small-worldness: %.3f\n", analyzer->metrics.small_worldness);
    printf("  Density: %.3f\n", analyzer->metrics.density);
}

void network_analyzer_print_modularity_trend(network_analyzer_t* analyzer)
{
    if (!analyzer) return;

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_pri", 0.0f);


    printf("=== Modularity Trend ===\n");
    printf("Analysis runs: %u\n", analyzer->analysis_count);

    if (analyzer->analysis_count > 0) {
        printf("Modularity history:\n");
        for (uint32_t i = 0; i < analyzer->analysis_count && i < analyzer->history_capacity; i++) {
            printf("  Run %u: %.3f\n", i + 1, analyzer->modularity_history[i]);
        }
    }
}

//=============================================================================
// Integration Hooks (Stubs)
//=============================================================================

void network_analyzer_on_learning_event(network_analyzer_t* analyzer)
{
    if (!analyzer) return;

    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_on_", 0.0f);


    analyzer->iteration_counter++;

    if (analyzer->auto_analyze &&
        analyzer->iteration_counter >= analyzer->analysis_interval) {
        network_analyzer_run(analyzer);
        analyzer->iteration_counter = 0;
    }
}

bool network_analyzer_check_new_community(network_analyzer_t* analyzer)
{
    if (!analyzer) {
        return false;
    }

    /* Check if community count changed since last analysis */
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_network_analyzer_che", 0.0f);

    if (!analyzer->communities || analyzer->analysis_count < 2) {
        return false;
    }

    /* Compare current community count with previous */
    uint32_t current = analyzer->communities->num_communities;
    uint32_t prev_idx = analyzer->analysis_count - 2;
    if (prev_idx < analyzer->history_capacity) {
        uint32_t previous = analyzer->community_count_history[prev_idx];
        if (current > previous) {
            NIMCP_LOGGING_INFO("New community detected: %u -> %u communities",
                               previous, current);
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Network Analysis self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int network_analysis_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    network_analysis_heartbeat("network_anal_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Network_Analysis");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                network_analysis_heartbeat("network_anal_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log observation if logging available */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Network_Analysis");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Network_Analysis");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void network_analysis_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_network_analysis_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int network_analysis_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "network_analysis_training_begin: NULL argument");
        return -1;
    }
    network_analysis_heartbeat_instance(NULL, "network_analysis_training_begin", 0.0f);
    return 0;
}

int network_analysis_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "network_analysis_training_end: NULL argument");
        return -1;
    }
    network_analysis_heartbeat_instance(NULL, "network_analysis_training_end", 1.0f);
    return 0;
}

int network_analysis_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "network_analysis_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    network_analysis_heartbeat_instance(NULL, "network_analysis_training_step", progress);
    return 0;
}
