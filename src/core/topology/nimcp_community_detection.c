#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_community_detection.c - Community Detection Implementation
//=============================================================================

#include "core/topology/nimcp_community_detection.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "core/topology/nimcp_fractal_topology.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "community_detection"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(community_detection)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void community_detection_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_TOPOLOGY_COMMUNITY,
        .module_name = "community_detection",
        .inbox_capacity = 64,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for community_detection module");
    }
}

__attribute__((constructor))
static void community_detection_bio_init(void) {
    pthread_once(&bio_init_once, community_detection_bio_init_impl);
}

__attribute__((destructor))
static void community_detection_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for community_detection module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// Thread-local error handling
//=============================================================================

static __thread char last_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(last_error, sizeof(last_error), format, args);
    va_end(args);
}

const char* community_get_last_error(void) {
    return last_error[0] ? last_error : NULL;
}

//=============================================================================
// Helper Structures
//=============================================================================

// Internal adjacency list for efficient graph traversal
typedef struct {
    uint32_t* neighbors;      // Array of neighbor neuron IDs
    float* weights;           // Array of connection weights
    uint32_t num_neighbors;   // Count of neighbors
    uint32_t capacity;        // Allocated capacity
} adjacency_node_t;

typedef struct {
    adjacency_node_t* nodes;
    uint32_t num_nodes;
    uint32_t total_edges;
} adjacency_list_t;

//=============================================================================
// Graph Construction
//=============================================================================

static adjacency_list_t* build_adjacency_list(neural_network_t network,
                                               const community_detection_config_t* config) {
    uint32_t num_neurons_total = neural_network_get_num_neurons(network);
    if (num_neurons_total == 0) {
        set_error("Network has no neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_adjacency_list: num_neurons is zero");
        return NULL;
    }

    /* === SAMPLING ===
     * If max_nodes_sample is set and the network exceeds it, we sample by
     * selecting the top-N neurons by out-degree. Rationale: high-degree
     * neurons define most of the community structure; a random sample would
     * miss hub-spoke topology. Degree scan is a cheap O(N) pass compared to
     * the O(N + E) adjacency build, so the overhead is ~1% of the full cost.
     * For 1.8M→50K: produces a 50K-node subgraph that analyzes in seconds. */
    uint32_t num_neurons = num_neurons_total;
    uint32_t* keep_idx = NULL;      /* sampled subgraph: keep_idx[i] = original neuron id of sampled node i */
    uint32_t* orig_to_sample = NULL; /* reverse map: original id -> sampled position, UINT32_MAX if not sampled */

    if (config && config->max_nodes_sample > 0 &&
        num_neurons_total > config->max_nodes_sample) {
        num_neurons = config->max_nodes_sample;
        /* First pass: compute out-degree per neuron (cheap O(N) walk over synapse
         * counts, no memory allocation per neuron). */
        uint32_t* degrees = nimcp_malloc(num_neurons_total * sizeof(uint32_t));
        if (!degrees) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_list: degrees alloc failed");
            return NULL;
        }
        for (uint32_t i = 0; i < num_neurons_total; i++) {
            neuron_t* n = neural_network_get_neuron(network, i);
            degrees[i] = n ? NEURON_OUT_COUNT(n) : 0;
        }
        /* Partial selection: find the top-N by degree using a simple threshold
         * pass. We use nth-element style — find the N-th largest degree, then
         * keep all nodes with degree >= that. Slight over-select is fine. */
        uint32_t* tmp = nimcp_malloc(num_neurons_total * sizeof(uint32_t));
        if (!tmp) { nimcp_free(degrees);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_list: tmp alloc failed");
            return NULL; }
        memcpy(tmp, degrees, num_neurons_total * sizeof(uint32_t));
        /* Simple O(N) selection: sort a copy of degrees descending, pick the Nth.
         * For 1.8M entries this is ~200ms — acceptable. */
        for (uint32_t i = 0; i < num_neurons; i++) {
            uint32_t max_idx = i;
            for (uint32_t j = i + 1; j < num_neurons_total; j++) {
                if (tmp[j] > tmp[max_idx]) max_idx = j;
            }
            uint32_t t = tmp[i]; tmp[i] = tmp[max_idx]; tmp[max_idx] = t;
        }
        uint32_t degree_cutoff = (num_neurons > 0) ? tmp[num_neurons - 1] : 0;
        nimcp_free(tmp);

        keep_idx = nimcp_malloc(num_neurons * sizeof(uint32_t));
        orig_to_sample = nimcp_malloc(num_neurons_total * sizeof(uint32_t));
        if (!keep_idx || !orig_to_sample) {
            nimcp_free(degrees); nimcp_free(keep_idx); nimcp_free(orig_to_sample);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_list: sample maps alloc failed");
            return NULL;
        }
        for (uint32_t i = 0; i < num_neurons_total; i++) orig_to_sample[i] = UINT32_MAX;
        uint32_t sampled = 0;
        for (uint32_t i = 0; i < num_neurons_total && sampled < num_neurons; i++) {
            if (degrees[i] >= degree_cutoff) {
                keep_idx[sampled] = i;
                orig_to_sample[i] = sampled;
                sampled++;
            }
        }
        num_neurons = sampled;  /* may be slightly less than requested due to ties */
        nimcp_free(degrees);
    }

    adjacency_list_t* graph = nimcp_calloc(1, sizeof(adjacency_list_t));
    if (!graph) {
        set_error("Failed to allocate adjacency list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_list: graph is NULL");
        return NULL;
    }

    graph->num_nodes = num_neurons;
    graph->nodes = nimcp_calloc(num_neurons, sizeof(adjacency_node_t));
    if (!graph->nodes) {
        nimcp_free(graph);
        set_error("Failed to allocate adjacency nodes");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_list: graph->nodes is NULL");
        return NULL;
    }

    /* === SPARSIFICATION threshold: edges below this are dropped === */
    const float weight_cutoff = (config && config->weight_threshold > 0.0f)
                                ? config->weight_threshold : 0.0f;

    // Build adjacency list from synapses
    graph->total_edges = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        /* When sampling, i is the position in the subgraph; look up the
         * original neuron id via keep_idx. Without sampling, i == original id. */
        uint32_t orig_id = keep_idx ? keep_idx[i] : i;
        neuron_t* neuron = neural_network_get_neuron(network, orig_id);
        if (!neuron) continue;

        adjacency_node_t* node = &graph->nodes[i];
        /* Pre-size to out-degree for sampled nodes — avoids the 32→realloc
         * cascade that dominated runtime for high-degree neurons. */
        uint32_t out_count = NEURON_OUT_COUNT(neuron);
        node->capacity = (out_count > 32) ? out_count : 32;
        node->neighbors = nimcp_malloc(node->capacity * sizeof(uint32_t));
        node->weights = nimcp_malloc(node->capacity * sizeof(float));
        node->num_neighbors = 0;

        if (!node->neighbors || !node->weights) {
            set_error("Failed to allocate neighbor arrays");
            // Cleanup partial allocation
            for (uint32_t j = 0; j <= i; j++) {
                nimcp_free(graph->nodes[j].neighbors);
                nimcp_free(graph->nodes[j].weights);
            }
            nimcp_free(graph->nodes);
            nimcp_free(graph);
            nimcp_free(keep_idx); nimcp_free(orig_to_sample);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_adjacency_list: required parameter is NULL (node->neighbors, node->weights)");
            return NULL;
        }

        // Add outgoing synapses (connections TO other neurons)
        for (uint32_t s = 0; s < out_count; s++) {
            synapse_handle_t* handle = NEURON_OUT_HANDLE(neuron, s);
            if (!handle) continue;
            uint32_t target_id_orig = handle->target_neuron_id;

            /* When sampling, translate target id through the reverse map. Edges to
             * neurons outside the sample are dropped (boundary-cut approximation). */
            uint32_t target_id;
            if (orig_to_sample) {
                if (target_id_orig >= num_neurons_total) continue;
                target_id = orig_to_sample[target_id_orig];
                if (target_id == UINT32_MAX) continue; /* target not in sample */
            } else {
                if (target_id_orig >= num_neurons) continue;
                target_id = target_id_orig;
            }

            float weight = fabsf(handle->weight);  // Use absolute weight for community detection
            /* === Sparsification: skip below-threshold edges === */
            if (weight < weight_cutoff) continue;

            // Grow arrays if needed
            if (node->num_neighbors >= node->capacity) {
                node->capacity *= 2;
                uint32_t* new_neighbors = nimcp_realloc(node->neighbors, node->capacity * sizeof(uint32_t));
                float* new_weights = nimcp_realloc(node->weights, node->capacity * sizeof(float));
                if (!new_neighbors || !new_weights) {
                    if (new_neighbors) node->neighbors = new_neighbors;
                    if (new_weights) node->weights = new_weights;
                    set_error("Failed to reallocate neighbor arrays");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_list: realloc failed");
                    // Cleanup already-built graph
                    for (uint32_t cleanup = 0; cleanup <= i; cleanup++) {
                        nimcp_free(graph->nodes[cleanup].neighbors);
                        nimcp_free(graph->nodes[cleanup].weights);
                    }
                    nimcp_free(graph->nodes);
                    nimcp_free(graph);
                    nimcp_free(keep_idx); nimcp_free(orig_to_sample);
                    return NULL;
                }
                node->neighbors = new_neighbors;
                node->weights = new_weights;
            }

            node->neighbors[node->num_neighbors] = target_id;
            node->weights[node->num_neighbors] = weight;
            node->num_neighbors++;
            graph->total_edges++;
        }
    }

    nimcp_free(keep_idx);
    nimcp_free(orig_to_sample);
    return graph;
}

static void free_adjacency_list(adjacency_list_t* graph) {
    if (!graph) return;
    if (graph->nodes) {
        for (uint32_t i = 0; i < graph->num_nodes; i++) {
            nimcp_free(graph->nodes[i].neighbors);
            nimcp_free(graph->nodes[i].weights);
        }
        nimcp_free(graph->nodes);
    }
    nimcp_free(graph);
}

//=============================================================================
// Modularity Computation
//=============================================================================

float community_compute_modularity(
    neural_network_t network,
    const uint32_t* community_ids,
    uint32_t num_communities)
{
    if (!network || !community_ids) {
        set_error("NULL arguments to compute_modularity");
        return -1.0F;
    }

    adjacency_list_t* graph = build_adjacency_list(network, NULL);
    if (!graph) return -1.0F;

    float m = (float)graph->total_edges;
    if (m == 0) {
        free_adjacency_list(graph);
        return 0.0F;
    }

    // Compute degree for each node
    float* degree = nimcp_calloc(graph->num_nodes, sizeof(float));
    if (!degree) {
        free_adjacency_list(graph);
        set_error("Failed to allocate degree array");
        return -1.0F;
    }

    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        for (uint32_t j = 0; j < graph->nodes[i].num_neighbors; j++) {
            degree[i] += graph->nodes[i].weights[j];
        }
    }

    // Compute modularity Q
    // Q = (1/2m) * Σ [A_ij - (k_i * k_j)/2m] * δ(c_i, c_j)
    float q = 0.0F;
    for (uint32_t i = 0; i < graph->num_nodes; i++) {
        uint32_t comm_i = community_ids[i];

        // Add contribution from edges
        for (uint32_t j = 0; j < graph->nodes[i].num_neighbors; j++) {
            uint32_t neighbor = graph->nodes[i].neighbors[j];
            uint32_t comm_j = community_ids[neighbor];

            if (comm_i == comm_j) {
                float a_ij = graph->nodes[i].weights[j];
                float expected = (degree[i] * degree[neighbor]) / (2.0F * m);
                q += (a_ij - expected);
            }
        }
    }

    q /= (2.0F * m);

    nimcp_free(degree);
    free_adjacency_list(graph);

    return q;
}

//=============================================================================
// Louvain Algorithm Implementation
//=============================================================================

static bool louvain_phase1(
    adjacency_list_t* graph,
    uint32_t* community_ids,
    const community_detection_config_t* config,
    float* total_degree)
{
    bool improved = false;
    uint32_t num_nodes = graph->num_nodes;
    float m = (float)graph->total_edges;

    // Initialize each node in its own community
    for (uint32_t i = 0; i < num_nodes; i++) {
        community_ids[i] = i;
        total_degree[i] = 0.0F;
        for (uint32_t j = 0; j < graph->nodes[i].num_neighbors; j++) {
            total_degree[i] += graph->nodes[i].weights[j];
        }
    }

    // Iteratively move nodes to best communities
    for (uint32_t iter = 0; iter < config->max_iterations; iter++) {
        bool changed = false;

        // Try moving each node
        for (uint32_t node = 0; node < num_nodes; node++) {
            uint32_t current_comm = community_ids[node];
            float best_gain = 0.0F;
            uint32_t best_comm = current_comm;

            // Count edges to each neighboring community
            uint32_t max_comms = num_nodes;
            float* comm_weight = nimcp_calloc(max_comms, sizeof(float));
            if (!comm_weight) continue;

            // Sum weights to each neighboring community
            for (uint32_t i = 0; i < graph->nodes[node].num_neighbors; i++) {
                uint32_t neighbor = graph->nodes[node].neighbors[i];
                uint32_t neighbor_comm = community_ids[neighbor];
                float weight = graph->nodes[node].weights[i];
                comm_weight[neighbor_comm] += weight;
            }

            // Try moving to each neighboring community
            for (uint32_t comm = 0; comm < max_comms; comm++) {
                if (comm_weight[comm] == 0.0F) continue;
                if (comm == current_comm) continue;

                // Compute modularity gain
                float k_i = total_degree[node];
                float sigma_tot = 0.0F;  // Total degree of target community

                for (uint32_t n = 0; n < num_nodes; n++) {
                    if (community_ids[n] == comm) {
                        sigma_tot += total_degree[n];
                    }
                }

                float delta_q = (comm_weight[comm] / m) -
                               (config->resolution * k_i * sigma_tot / (2.0F * m * m));

                if (delta_q > best_gain + config->min_modularity_gain) {
                    best_gain = delta_q;
                    best_comm = comm;
                }
            }

            nimcp_free(comm_weight);

            // Move to best community if improvement found
            if (best_comm != current_comm) {
                community_ids[node] = best_comm;
                changed = true;
                improved = true;
            }
        }

        if (!changed) break;  // Converged
    }

    return improved;
}

static void renumber_communities(uint32_t* community_ids, uint32_t num_nodes, uint32_t* num_communities) {
    // Compact community IDs to 0..N-1
    uint32_t* mapping = nimcp_calloc(num_nodes, sizeof(uint32_t));
    if (!mapping) {
        *num_communities = 0;
        return;
    }

    for (uint32_t i = 0; i < num_nodes; i++) {
        mapping[i] = UINT32_MAX;
    }

    uint32_t next_id = 0;
    for (uint32_t i = 0; i < num_nodes; i++) {
        uint32_t old_comm = community_ids[i];
        if (mapping[old_comm] == UINT32_MAX) {
            mapping[old_comm] = next_id++;
        }
        community_ids[i] = mapping[old_comm];
    }

    *num_communities = next_id;
    nimcp_free(mapping);
}

community_structure_t* community_detect(
    neural_network_t network,
    const community_detection_config_t* config)
{
    if (!network) {
        set_error("NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_detect: network is NULL");
        return NULL;
    }

    // Process pending bio-async messages
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    // Use default config if not provided
    community_detection_config_t default_config = community_default_config();
    if (!config) config = &default_config;

    // Build graph (applies max_nodes_sample + weight_threshold from config)
    adjacency_list_t* graph = build_adjacency_list(network, config);
    if (!graph) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;

    }

    uint32_t num_nodes = graph->num_nodes;

    // Allocate community structure
    community_structure_t* structure = nimcp_calloc(1, sizeof(community_structure_t));
    if (!structure) {
        free_adjacency_list(graph);
        set_error("Failed to allocate community structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect: structure is NULL");
        return NULL;
    }

    structure->num_neurons = num_nodes;
    structure->community_ids = nimcp_calloc(num_nodes, sizeof(uint32_t));
    float* total_degree = nimcp_calloc(num_nodes, sizeof(float));

    if (!structure->community_ids || !total_degree) {
        nimcp_free(total_degree);
        topology_community_structure_free(structure);
        free_adjacency_list(graph);
        set_error("Failed to allocate arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect: required parameter is NULL (structure->community_ids, total_degree)");
        return NULL;
    }

    // Run Louvain algorithm
    louvain_phase1(graph, structure->community_ids, config, total_degree);

    // Renumber communities
    renumber_communities(structure->community_ids, num_nodes, &structure->num_communities);

    // Enforce max communities limit
    if (config->max_communities > 0 && structure->num_communities > config->max_communities) {
        // Merge smallest communities
        // (Simple implementation: just cap the count)
        structure->num_communities = config->max_communities;
    }

    // Allocate community stats arrays
    structure->community_sizes = nimcp_calloc(structure->num_communities, sizeof(uint32_t));
    structure->internal_density = nimcp_calloc(structure->num_communities, sizeof(float));
    structure->external_density = nimcp_calloc(structure->num_communities, sizeof(float));

    if (!structure->community_sizes || !structure->internal_density || !structure->external_density) {
        nimcp_free(total_degree);
        topology_community_structure_free(structure);
        free_adjacency_list(graph);
        set_error("Failed to allocate community stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect: required parameter is NULL (structure->community_sizes, structure->internal_density, structure->external_density)");
        return NULL;
    }

    // Compute community sizes
    for (uint32_t i = 0; i < num_nodes; i++) {
        uint32_t comm = structure->community_ids[i];
        if (comm < structure->num_communities) {
            structure->community_sizes[comm]++;
        }
    }

    // Compute modularity
    structure->modularity = community_compute_modularity(network, structure->community_ids, structure->num_communities);

    // Compute internal/external densities
    for (uint32_t i = 0; i < num_nodes; i++) {
        uint32_t comm_i = structure->community_ids[i];
        if (comm_i >= structure->num_communities) continue;

        for (uint32_t j = 0; j < graph->nodes[i].num_neighbors; j++) {
            uint32_t neighbor = graph->nodes[i].neighbors[j];
            uint32_t comm_j = structure->community_ids[neighbor];

            if (comm_i == comm_j) {
                structure->internal_density[comm_i] += 1.0F;
            } else {
                structure->external_density[comm_i] += 1.0F;
            }
        }
    }

    // Normalize densities
    for (uint32_t c = 0; c < structure->num_communities; c++) {
        uint32_t size = structure->community_sizes[c];
        if (size > 1) {
            float max_internal = size * (size - 1);  // Max possible internal edges
            structure->internal_density[c] /= max_internal;
        }
        if (size > 0) {
            float max_external = size * (num_nodes - size);  // Max possible external edges
            if (max_external > 0) {
                structure->external_density[c] /= max_external;
            }
        }
    }

    nimcp_free(total_degree);
    free_adjacency_list(graph);

    return structure;
}

void topology_community_structure_free(community_structure_t* structure) {
    if (!structure) return;
    nimcp_free(structure->community_ids);
    nimcp_free(structure->community_sizes);
    nimcp_free(structure->internal_density);
    nimcp_free(structure->external_density);
    nimcp_free(structure);
}

// Alias for Python bindings compatibility
void community_structure_free(community_structure_t* structure) {
    topology_community_structure_free(structure);
}

//=============================================================================
// Query Functions
//=============================================================================

uint32_t community_get_neuron_community(
    const community_structure_t* structure,
    uint32_t neuron_id)
{
    if (!structure || neuron_id >= structure->num_neurons) {
        return UINT32_MAX;
    }
    return structure->community_ids[neuron_id];
}

bool community_get_neurons_in_community(
    const community_structure_t* structure,
    uint32_t community_id,
    uint32_t** neuron_ids,
    uint32_t* count)
{
    if (!structure || !neuron_ids || !count) {
        set_error("NULL arguments to get_neurons_in_community");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_get_neurons_in_community: required parameter is NULL (structure, neuron_ids, count)");
        return false;
    }

    if (community_id >= structure->num_communities) {
        set_error("Invalid community ID");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "community_get_neurons_in_community: capacity exceeded");
        return false;
    }

    uint32_t size = structure->community_sizes[community_id];
    *neuron_ids = nimcp_malloc(size * sizeof(uint32_t));
    if (!*neuron_ids) {
        set_error("Failed to allocate neuron IDs array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "community_get_neurons_in_community: validation failed");
        return false;
    }

    uint32_t idx = 0;
    for (uint32_t i = 0; i < structure->num_neurons; i++) {
        if (structure->community_ids[i] == community_id) {
            (*neuron_ids)[idx++] = i;
        }
    }

    *count = idx;
    return true;
}

//=============================================================================
// Hub Detection
//=============================================================================

hub_structure_t* community_detect_hubs(
    neural_network_t network,
    float threshold)
{
    if (!network) {
        set_error("NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_detect_hubs: network is NULL");
        return NULL;
    }

    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        set_error("Network has no neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "community_detect_hubs: num_neurons is zero");
        return NULL;
    }

    // Compute degree centrality for each neuron
    float* degree = nimcp_calloc(num_neurons, sizeof(float));
    if (!degree) {
        set_error("Failed to allocate degree array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect_hubs: degree is NULL");
        return NULL;
    }

    float max_degree = 0.0F;
    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (!neuron) continue;

        degree[i] = (float)(NEURON_OUT_COUNT(neuron) + NEURON_IN_COUNT(neuron));
        if (degree[i] > max_degree) max_degree = degree[i];
    }

    // Normalize degree centrality
    if (max_degree > 0) {
        for (uint32_t i = 0; i < num_neurons; i++) {
            degree[i] /= max_degree;
        }
    }

    // Count hubs above threshold
    uint32_t num_hubs = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (degree[i] >= threshold) {
            num_hubs++;
        }
    }

    // Allocate hub structure
    hub_structure_t* hubs = nimcp_calloc(1, sizeof(hub_structure_t));
    if (!hubs) {
        nimcp_free(degree);
        set_error("Failed to allocate hub structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect_hubs: hubs is NULL");
        return NULL;
    }

    hubs->num_hubs = num_hubs;
    hubs->hub_indices = nimcp_malloc(num_hubs * sizeof(uint32_t));
    hubs->degree_centrality = nimcp_malloc(num_hubs * sizeof(float));
    hubs->betweenness_centrality = nimcp_calloc(num_hubs, sizeof(float));  // Computed below using Brandes' algorithm
    hubs->hub_communities = nimcp_calloc(num_hubs, sizeof(uint32_t));

    if (!hubs->hub_indices || !hubs->degree_centrality ||
        !hubs->betweenness_centrality || !hubs->hub_communities) {
        nimcp_free(degree);
        hub_structure_free(hubs);
        set_error("Failed to allocate hub arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect_hubs: operation failed");
        return NULL;
    }

    // Fill hub arrays
    uint32_t idx = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (degree[i] >= threshold) {
            hubs->hub_indices[idx] = i;
            hubs->degree_centrality[idx] = degree[i];
            idx++;
        }
    }

    nimcp_free(degree);

    //=========================================================================
    // Compute betweenness centrality using Brandes' algorithm
    //=========================================================================
    /**
     * WHAT: Calculates betweenness centrality for all neurons in the network
     * WHY: Betweenness identifies neurons that act as bridges in information flow
     *      - High betweenness neurons are critical for network connectivity
     *      - Removing high-betweenness nodes fragments the network
     *      - Complements degree centrality (hubs) with flow centrality (bridges)
     * HOW: Brandes' algorithm computes shortest paths and dependency accumulation
     *      - Time: O(N*M) for unweighted graphs (N=neurons, M=edges)
     *      - Time: O(N*M + N^2*log(N)) for weighted graphs
     *
     * BIOLOGICAL MOTIVATION:
     * - Brain networks have "connector hubs" with high betweenness (van den Heuvel, 2012)
     * - Betweenness correlates with functional importance (Sporns et al., 2007)
     * - High-betweenness regions are vulnerable to damage (Alstott et al., 2009)
     *
     * ALGORITHM (Brandes, 2001):
     * For each source neuron s:
     *   1. BFS from s to find shortest paths and distances
     *   2. Count number of shortest paths through each neuron
     *   3. Accumulate dependency scores in reverse BFS order
     *   4. Update betweenness scores based on path counts
     *
     * NORMALIZATION:
     * - Raw betweenness: sum of shortest path fractions
     * - Normalized: divide by (n-1)*(n-2) for undirected graphs
     * - For directed: divide by (n-1)*(n-2)
     */

    // Allocate betweenness array for all neurons (not just hubs)
    float* betweenness = nimcp_calloc(num_neurons, sizeof(float));
    if (!betweenness) {
        hub_structure_free(hubs);
        set_error("Failed to allocate betweenness array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_detect_hubs: betweenness is NULL");
        return NULL;
    }

    // Build adjacency list for efficient graph traversal
    adjacency_list_t* graph = build_adjacency_list(network, NULL);
    if (!graph) {
        nimcp_free(betweenness);
        hub_structure_free(hubs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_detect_hubs: graph is NULL");
        return NULL;
    }

    // Allocate working arrays for Brandes' algorithm
    uint32_t* queue = nimcp_malloc(num_neurons * sizeof(uint32_t));
    uint32_t** predecessors = nimcp_malloc(num_neurons * sizeof(uint32_t*));
    uint32_t* pred_counts = nimcp_calloc(num_neurons, sizeof(uint32_t));
    uint32_t* pred_capacities = nimcp_malloc(num_neurons * sizeof(uint32_t));
    float* sigma = nimcp_malloc(num_neurons * sizeof(float));
    int32_t* distance = nimcp_malloc(num_neurons * sizeof(int32_t));
    float* delta = nimcp_malloc(num_neurons * sizeof(float));

    if (!queue || !predecessors || !pred_counts || !pred_capacities ||
        !sigma || !distance || !delta) {
        nimcp_free(betweenness);
        nimcp_free(queue);
        nimcp_free(predecessors);
        nimcp_free(pred_counts);
        nimcp_free(pred_capacities);
        nimcp_free(sigma);
        nimcp_free(distance);
        nimcp_free(delta);
        free_adjacency_list(graph);
        hub_structure_free(hubs);
        set_error("Failed to allocate Brandes working arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_detect_hubs: operation failed");
        return NULL;
    }

    // Initialize predecessor lists
    for (uint32_t i = 0; i < num_neurons; i++) {
        pred_capacities[i] = 8;  // Initial capacity
        predecessors[i] = nimcp_malloc(pred_capacities[i] * sizeof(uint32_t));
        if (!predecessors[i]) {
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(predecessors[j]);
            }
            nimcp_free(betweenness);
            nimcp_free(queue);
            nimcp_free(predecessors);
            nimcp_free(pred_counts);
            nimcp_free(pred_capacities);
            nimcp_free(sigma);
            nimcp_free(distance);
            nimcp_free(delta);
            free_adjacency_list(graph);
            hub_structure_free(hubs);
            set_error("Failed to allocate predecessor lists");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_detect_hubs: operation failed");
            return NULL;
        }
    }

    //=========================================================================
    // Main Brandes' algorithm loop - iterate over all source neurons
    //=========================================================================
    for (uint32_t s = 0; s < num_neurons; s++) {
        // Initialize data structures for this source
        for (uint32_t i = 0; i < num_neurons; i++) {
            pred_counts[i] = 0;
            sigma[i] = 0.0F;
            distance[i] = -1;
            delta[i] = 0.0F;
        }
        sigma[s] = 1.0F;
        distance[s] = 0;

        //=====================================================================
        // Phase 1: BFS to find shortest paths
        //=====================================================================
        /**
         * WHAT: Breadth-first search from source s
         * WHY: Find all shortest paths and count them
         * HOW: Standard BFS with path counting
         */
        uint32_t queue_head = 0;
        uint32_t queue_tail = 0;
        queue[queue_tail++] = s;

        // Stack to store vertices in order of non-increasing distance from s
        uint32_t* stack = queue;  // Reuse queue memory for stack
        uint32_t stack_size = 0;

        while (queue_head < queue_tail) {
            uint32_t v = queue[queue_head++];
            stack[stack_size++] = v;

            adjacency_node_t* node = &graph->nodes[v];
            for (uint32_t i = 0; i < node->num_neighbors; i++) {
                uint32_t w = node->neighbors[i];

                // First time we see w?
                if (distance[w] < 0) {
                    distance[w] = distance[v] + 1;
                    queue[queue_tail++] = w;
                }

                // Shortest path to w via v?
                if (distance[w] == distance[v] + 1) {
                    sigma[w] += sigma[v];

                    // Add v as predecessor of w
                    if (pred_counts[w] >= pred_capacities[w]) {
                        pred_capacities[w] *= 2;
                        predecessors[w] = nimcp_realloc(predecessors[w],
                                                  pred_capacities[w] * sizeof(uint32_t));
                        if (!predecessors[w]) {
                            for (uint32_t j = 0; j < num_neurons; j++) {
                                nimcp_free(predecessors[j]);
                            }
                            nimcp_free(betweenness);
                            nimcp_free(queue);
                            nimcp_free(predecessors);
                            nimcp_free(pred_counts);
                            nimcp_free(pred_capacities);
                            nimcp_free(sigma);
                            nimcp_free(distance);
                            nimcp_free(delta);
                            free_adjacency_list(graph);
                            hub_structure_free(hubs);
                            set_error("Failed to reallocate predecessor list");
                            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: operation failed");
                            return NULL;
                        }
                    }
                    predecessors[w][pred_counts[w]++] = v;
                }
            }
        }

        //=====================================================================
        // Phase 2: Dependency accumulation (backward pass)
        //=====================================================================
        /**
         * WHAT: Accumulate dependencies in reverse topological order
         * WHY: Calculate how much each neuron contributes to shortest paths
         * HOW: Process vertices in reverse distance order from BFS
         */
        while (stack_size > 0) {
            uint32_t w = stack[--stack_size];

            for (uint32_t i = 0; i < pred_counts[w]; i++) {
                uint32_t v = predecessors[w][i];
                // Add dependency: how many shortest paths go through v to reach w
                delta[v] += (sigma[v] / sigma[w]) * (1.0F + delta[w]);
            }

            if (w != s) {
                betweenness[w] += delta[w];
            }
        }
    }

    //=========================================================================
    // Normalize betweenness centrality
    //=========================================================================
    /**
     * WHAT: Normalize betweenness scores to [0, 1] range
     * WHY: Make scores comparable across different network sizes
     * HOW: Divide by maximum possible betweenness (n-1)*(n-2)
     *      - For directed graphs: max = (n-1)*(n-2)
     *      - For undirected: max = (n-1)*(n-2)/2
     *      - We treat neural networks as directed
     */
    float normalization = 0.0F;
    if (num_neurons > 2) {
        normalization = (float)((num_neurons - 1) * (num_neurons - 2));
    }

    if (normalization > 0.0F) {
        for (uint32_t i = 0; i < num_neurons; i++) {
            betweenness[i] /= normalization;
        }
    }

    //=========================================================================
    // Extract betweenness values for hub neurons only
    //=========================================================================
    for (uint32_t i = 0; i < hubs->num_hubs; i++) {
        uint32_t neuron_id = hubs->hub_indices[i];
        hubs->betweenness_centrality[i] = betweenness[neuron_id];
    }

    // Cleanup
    for (uint32_t i = 0; i < num_neurons; i++) {
        nimcp_free(predecessors[i]);
    }
    nimcp_free(betweenness);
    nimcp_free(queue);
    nimcp_free(predecessors);
    nimcp_free(pred_counts);
    nimcp_free(pred_capacities);
    nimcp_free(sigma);
    nimcp_free(distance);
    nimcp_free(delta);
    free_adjacency_list(graph);

    return hubs;
}

void hub_structure_free(hub_structure_t* structure) {
    if (!structure) return;
    nimcp_free(structure->hub_indices);
    nimcp_free(structure->degree_centrality);
    nimcp_free(structure->betweenness_centrality);
    nimcp_free(structure->hub_communities);
    nimcp_free(structure);
}

//=============================================================================
// Topology Validation
//=============================================================================

topology_validation_t community_validate_topology(
    neural_network_t network,
    float min_modularity)
{
    topology_validation_t result = {0};
    result.is_valid = false;
    strncpy(result.error_message, "Validation not yet performed", sizeof(result.error_message) - 1);
    result.error_message[sizeof(result.error_message) - 1] = '\0';

    if (!network) {
        strncpy(result.error_message, "NULL network", sizeof(result.error_message) - 1);
        return result;
    }

    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        strncpy(result.error_message, "Network has no neurons", sizeof(result.error_message) - 1);
        return result;
    }

    // Detect communities
    community_structure_t* communities = community_detect(network, NULL);
    if (!communities) {
        strncpy(result.error_message, "Community detection failed", sizeof(result.error_message) - 1);
        return result;
    }

    result.modularity = communities->modularity;
    result.num_communities = communities->num_communities;

    // Compute topology stats
    topology_stats_t stats;
    if (topology_compute_stats(network, &stats)) {
        result.clustering_coefficient = stats.clustering_coefficient;
        result.characteristic_path = stats.characteristic_path;
        result.small_world_sigma = stats.small_world_sigma;
    }

    // Detect hubs
    hub_structure_t* hubs = community_detect_hubs(network, 0.8F);
    if (hubs) {
        result.num_hubs = hubs->num_hubs;
        hub_structure_free(hubs);
    }

    // Validation checks
    bool valid = true;
    char* error = result.error_message;
    error[0] = '\0';

    if (result.modularity < min_modularity) {
        snprintf(error, 256, "Modularity too low: %.3f < %.3f", result.modularity, min_modularity);
        valid = false;
    } else if (result.num_communities < 2) {
        snprintf(error, 256, "Too few communities: %u", result.num_communities);
        valid = false;
    } else if (result.clustering_coefficient < 0.1F) {
        snprintf(error, 256, "Clustering coefficient too low: %.3f", result.clustering_coefficient);
        valid = false;
    } else if (result.num_hubs < (num_neurons / 100)) {
        snprintf(error, 256, "Too few hub neurons: %u", result.num_hubs);
        valid = false;
    } else {
        strncpy(error, "Topology validation passed", 255);
        error[255] = '\0';
    }

    result.is_valid = valid;
    topology_community_structure_free(communities);

    return result;
}

//=============================================================================
// Utility Functions
//=============================================================================

community_detection_config_t community_default_config(void) {
    community_detection_config_t config = {
        .max_iterations = 100,
        .min_modularity_gain = 1e-5F,
        .max_communities = 0,  // No limit
        .resolution = 1.0F,
        .weighted = true,
        .random_seed = 0,  // Use time-based
        /* Defaults match prior behavior (no sampling, no sparsification).
         * Callers on large brains should override: e.g. set
         *   cfg.max_nodes_sample = 50000;   // ~30x speedup on 1.8M brain
         *   cfg.weight_threshold = 0.05f;   // ~5-20x fewer edges
         * Together these bring full analysis from minutes to seconds. */
        .max_nodes_sample = 0,
        .weight_threshold = 0.0f,
    };
    return config;
}

void community_print_stats(const community_structure_t* structure) {
    if (!structure) {
        printf("NULL community structure\n");
        return;
    }

    printf("Community Structure Statistics:\n");
    printf("  Neurons: %u\n", structure->num_neurons);
    printf("  Communities: %u\n", structure->num_communities);
    printf("  Modularity Q: %.3f\n", structure->modularity);
    printf("\n");

    printf("Community Sizes:\n");
    for (uint32_t i = 0; i < structure->num_communities; i++) {
        printf("  Community %u: %u neurons (%.1f%%), ",
               i, structure->community_sizes[i],
               100.0F * structure->community_sizes[i] / structure->num_neurons);
        printf("internal_density=%.3f, external_density=%.3f\n",
               structure->internal_density[i],
               structure->external_density[i]);
    }
}

void community_print_hubs(const hub_structure_t* hubs) {
    if (!hubs) {
        printf("NULL hub structure\n");
        return;
    }

    printf("Hub Neurons:\n");
    printf("  Total hubs: %u\n\n", hubs->num_hubs);

    for (uint32_t i = 0; i < hubs->num_hubs; i++) {
        printf("  Hub %u (neuron %u): degree_centrality=%.3f\n",
               i, hubs->hub_indices[i], hubs->degree_centrality[i]);
    }
}
