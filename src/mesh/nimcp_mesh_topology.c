/**
 * @file nimcp_mesh_topology.c
 * @brief Mesh Network Topology Management Implementation
 *
 * WHAT: Topology analysis and coordinator placement implementation
 * WHY:  Optimize coordinator placement using fractal network properties
 * HOW:  Graph algorithms for centrality, clustering, and placement
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_topology.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/thread/nimcp_thread.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdatomic.h>
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * BBB Integration for Mesh Topology
 * ============================================================================ */

/** Global BBB system for mesh topology module - thread-safe access */
static _Atomic(bbb_system_t) g_mesh_topology_bbb = NULL;

/** Global health agent for mesh topology module */
static _Atomic(nimcp_health_agent_t*) g_mesh_topology_health_agent = NULL;

/**
 * @brief Set BBB system for mesh topology validation
 * @param bbb BBB system (can be NULL to disable)
 */
void mesh_topology_set_bbb(bbb_system_t bbb) {
    atomic_store(&g_mesh_topology_bbb, bbb);
}

/**
 * @brief Get current BBB system for mesh topology
 * @return BBB system or NULL
 */
bbb_system_t mesh_topology_get_bbb(void) {
    return atomic_load(&g_mesh_topology_bbb);
}

/**
 * @brief Set health agent for mesh topology heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void mesh_topology_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_mesh_topology_health_agent, agent);
}

/**
 * @brief Send heartbeat from mesh topology module
 */
static inline void mesh_topology_heartbeat(const char* operation, float progress) {
    nimcp_health_agent_t* agent = atomic_load(&g_mesh_topology_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}

/**
 * @brief Validate topology change (add participant) using BBB
 *
 * WHAT: Validate participant ID before adding to topology
 * WHY:  Prevent unauthorized topology modifications
 * HOW:  Use BBB integer validation for participant ID
 *
 * @param ctx Topology context
 * @param participant_id Participant to validate
 * @return true if valid, false if threat detected
 */
static bool validate_topology_change_bbb(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id
) {
    (void)ctx;  /* May be used for context-specific validation later */

    bbb_system_t bbb = atomic_load(&g_mesh_topology_bbb);
    if (!bbb) return true;  /* BBB not configured, allow */

    bbb_validation_result_t result;

    /* Validate participant ID as integer */
    bool valid = bbb_validate_integer(bbb, (int64_t)participant_id, &result);
    if (!valid) {
        LOG_WARN("BBB rejected topology change for participant 0x%lx: %s (threat=%s)",
                 (unsigned long)participant_id, result.reason,
                 bbb_threat_type_name(result.threat));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected topology participant ID");
        return false;
    }

    return true;
}

/**
 * @brief Validate connection parameters using BBB
 *
 * WHAT: Validate connection weight and participant IDs
 * WHY:  Prevent malicious connection injection
 * HOW:  Use BBB validation for numeric values
 *
 * @param from Source participant ID
 * @param to Destination participant ID
 * @param weight Connection weight
 * @return true if valid, false if threat detected
 */
static bool validate_connection_bbb(
    mesh_participant_id_t from,
    mesh_participant_id_t to,
    float weight
) {
    bbb_system_t bbb = atomic_load(&g_mesh_topology_bbb);
    if (!bbb) return true;  /* BBB not configured, allow */

    bbb_validation_result_t result;

    /* Validate from participant ID */
    if (!bbb_validate_integer(bbb, (int64_t)from, &result)) {
        LOG_WARN("BBB rejected connection from 0x%lx: %s",
                 (unsigned long)from, result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected connection source");
        return false;
    }

    /* Validate to participant ID */
    if (!bbb_validate_integer(bbb, (int64_t)to, &result)) {
        LOG_WARN("BBB rejected connection to 0x%lx: %s",
                 (unsigned long)to, result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected connection destination");
        return false;
    }

    /* Validate weight range (check for suspicious values) */
    if (weight < -1e10f || weight > 1e10f || weight != weight /* NaN check */) {
        LOG_WARN("BBB rejected connection weight %.2f - out of range or NaN", weight);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected connection weight");
        return false;
    }

    return true;
}

/* Error code compatibility aliases */

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Adjacency list entry
 */
typedef struct adj_entry {
    mesh_participant_id_t neighbor;
    float weight;
    struct adj_entry* next;
} adj_entry_t;

/**
 * @brief Node in topology graph
 */
typedef struct topo_node {
    mesh_participant_id_t id;
    adj_entry_t* neighbors;
    uint32_t degree;
    float betweenness;
    float closeness;
    float eigenvector;
    bool is_hub;
    uint32_t cluster_id;
    uint32_t coordinator_affinity;
    bool valid;
} topo_node_t;

/**
 * @brief Internal topology context
 */
struct mesh_topology_ctx_internal {
    mesh_topology_config_t config;

    /* Node storage (hash-like array) */
    topo_node_t* nodes;
    size_t node_capacity;
    size_t node_count;

    /* Connection count */
    size_t connection_count;

    /* Computed metrics */
    mesh_topology_metrics_t cached_metrics;
    bool metrics_valid;

    /* Hub tracking */
    mesh_participant_id_t* hub_ids;
    size_t hub_count;
    size_t hub_capacity;

    /* Cluster tracking */
    uint32_t num_clusters;
    bool clusters_computed;

    /* P2-154: Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Hash participant ID to node index
 */
static size_t hash_participant_id(mesh_participant_id_t id, size_t capacity) {
    return (size_t)(id % capacity);
}

/**
 * @brief Find node by participant ID
 */
static topo_node_t* find_node(mesh_topology_ctx_t ctx, mesh_participant_id_t id) {
    if (!ctx || ctx->node_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_node: ctx is NULL");
        return NULL;
    }

    size_t start = hash_participant_id(id, ctx->node_capacity);
    size_t idx = start;

    do {
        if (ctx->nodes[idx].valid && ctx->nodes[idx].id == id) {
            return &ctx->nodes[idx];
        }
        idx = (idx + 1) % ctx->node_capacity;
    } while (idx != start);

    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Find empty slot for new node
 */
static topo_node_t* find_empty_slot(mesh_topology_ctx_t ctx, mesh_participant_id_t id) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_empty_slot: ctx is NULL");
        return NULL;
    }

    size_t start = hash_participant_id(id, ctx->node_capacity);
    size_t idx = start;

    do {
        if (!ctx->nodes[idx].valid) {
            return &ctx->nodes[idx];
        }
        idx = (idx + 1) % ctx->node_capacity;
    } while (idx != start);

    /* Hash table full - capacity exhausted, not a NULL pointer (P2: false positive removal) */
    return NULL;
}

/**
 * @brief Free adjacency list
 */
static void free_adj_list(adj_entry_t* head) {
    while (head) {
        adj_entry_t* next = head->next;
        nimcp_free(head);
        head = next;
    }
}

/**
 * @brief Add neighbor to adjacency list
 */
static nimcp_error_t add_neighbor(topo_node_t* node, mesh_participant_id_t neighbor, float weight) {
    if (!node) return NIMCP_ERROR_INVALID_PARAM;

    /* Check if already exists */
    adj_entry_t* entry = node->neighbors;
    while (entry) {
        if (entry->neighbor == neighbor) {
            entry->weight = weight; /* Update weight */
            return NIMCP_SUCCESS;
        }
        entry = entry->next;
    }

    /* Add new entry */
    adj_entry_t* new_entry = (adj_entry_t*)nimcp_malloc(sizeof(adj_entry_t));
    if (!new_entry) return NIMCP_ERROR_NO_MEMORY;

    new_entry->neighbor = neighbor;
    new_entry->weight = weight;
    new_entry->next = node->neighbors;
    node->neighbors = new_entry;
    node->degree++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Remove neighbor from adjacency list
 */
static void remove_neighbor(topo_node_t* node, mesh_participant_id_t neighbor) {
    if (!node) return;

    adj_entry_t** pp = &node->neighbors;
    while (*pp) {
        if ((*pp)->neighbor == neighbor) {
            adj_entry_t* to_free = *pp;
            *pp = (*pp)->next;
            nimcp_free(to_free);
            node->degree--;
            return;
        }
        pp = &(*pp)->next;
    }
}

/**
 * @brief Compare function for sorting by degree (descending)
 */
static int compare_degree_desc(const void* a, const void* b) {
    const topo_node_t* na = *(const topo_node_t**)a;
    const topo_node_t* nb = *(const topo_node_t**)b;
    return (int)nb->degree - (int)na->degree;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

mesh_topology_config_t mesh_topology_default_config(void) {
    mesh_topology_config_t config = {
        .hub_percentile = MESH_TOPOLOGY_HUB_PERCENTILE,
        .high_centrality_threshold = MESH_TOPOLOGY_HIGH_CENTRALITY,
        .compute_clustering = true,
        .compute_path_length = true,
        .compute_small_world = true,
        .fit_power_law = true,
        .max_path_samples = 100
    };
    return config;
}

mesh_topology_ctx_t mesh_topology_create(const mesh_topology_config_t* config) {
    mesh_topology_ctx_t ctx = (mesh_topology_ctx_t)nimcp_calloc(1, sizeof(struct mesh_topology_ctx_internal));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_topology_create: ctx is NULL");
        return NULL;
    }

    ctx->config = config ? *config : mesh_topology_default_config();

    /* Initial node capacity */
    ctx->node_capacity = 128;
    ctx->nodes = (topo_node_t*)nimcp_calloc(ctx->node_capacity, sizeof(topo_node_t));
    if (!ctx->nodes) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_topology_create: ctx->nodes is NULL");
        return NULL;
    }

    /* Hub storage */
    ctx->hub_capacity = 32;
    ctx->hub_ids = (mesh_participant_id_t*)nimcp_malloc(ctx->hub_capacity * sizeof(mesh_participant_id_t));
    if (!ctx->hub_ids) {
        nimcp_free(ctx->nodes);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_topology_create: ctx->hub_ids is NULL");
        return NULL;
    }

    /* P2-154: Create mutex for thread safety */
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        nimcp_free(ctx->hub_ids);
        nimcp_free(ctx->nodes);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_topology_create: mutex creation failed");
        return NULL;
    }

    return ctx;
}

void mesh_topology_destroy(mesh_topology_ctx_t ctx) {
    if (!ctx) return;

    /* Free adjacency lists */
    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            free_adj_list(ctx->nodes[i].neighbors);
        }
    }

    /* P2-154: Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_destroy(ctx->mutex);
    }

    nimcp_free(ctx->nodes);
    nimcp_free(ctx->hub_ids);
    nimcp_free(ctx);
}

/* ============================================================================
 * Node Registration
 * ============================================================================ */

nimcp_error_t mesh_topology_add_participant(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id
) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* P2-154: Mutex protection for topology modification */
    nimcp_mutex_lock(ctx->mutex);

    /* BBB validation before topology modification */
    if (!validate_topology_change_bbb(ctx, participant_id)) {
        LOG_ERROR("Rejecting topology add for participant 0x%lx - BBB validation failed",
                  (unsigned long)participant_id);
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_BBB_VALIDATION;
    }

    /* Send heartbeat for topology modification */
    mesh_topology_heartbeat("add_participant", 0.0f);

    /* Check if already exists */
    if (find_node(ctx, participant_id)) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_SUCCESS; /* Already present */
    }

    /* Resize if needed */
    if (ctx->node_count >= ctx->node_capacity * 0.7) {
        size_t new_capacity = ctx->node_capacity * 2;
        topo_node_t* new_nodes = (topo_node_t*)nimcp_calloc(new_capacity, sizeof(topo_node_t));
        if (!new_nodes) {
            nimcp_mutex_unlock(ctx->mutex);
            return NIMCP_ERROR_NO_MEMORY;
        }

        /* Rehash all nodes */
        for (size_t i = 0; i < ctx->node_capacity; i++) {
            if (ctx->nodes[i].valid) {
                size_t new_idx = hash_participant_id(ctx->nodes[i].id, new_capacity);
                while (new_nodes[new_idx].valid) {
                    new_idx = (new_idx + 1) % new_capacity;
                }
                new_nodes[new_idx] = ctx->nodes[i];
            }
        }

        nimcp_free(ctx->nodes);
        ctx->nodes = new_nodes;
        ctx->node_capacity = new_capacity;
    }

    /* Find slot and insert */
    topo_node_t* slot = find_empty_slot(ctx, participant_id);
    if (!slot) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    memset(slot, 0, sizeof(topo_node_t));
    slot->id = participant_id;
    slot->valid = true;
    ctx->node_count++;
    ctx->metrics_valid = false;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_topology_add_connection(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t from,
    mesh_participant_id_t to,
    float weight
) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* P2-154: Mutex protection for topology modification */
    nimcp_mutex_lock(ctx->mutex);

    /* BBB validation of connection parameters */
    if (!validate_connection_bbb(from, to, weight)) {
        LOG_ERROR("Rejecting connection 0x%lx -> 0x%lx - BBB validation failed",
                  (unsigned long)from, (unsigned long)to);
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_BBB_VALIDATION;
    }

    /* Send heartbeat for connection modification */
    mesh_topology_heartbeat("add_connection", 0.0f);

    topo_node_t* from_node = find_node(ctx, from);
    topo_node_t* to_node = find_node(ctx, to);

    if (!from_node || !to_node) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_error_t err = add_neighbor(from_node, to, weight);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(ctx->mutex);
        return err;
    }

    ctx->connection_count++;
    ctx->metrics_valid = false;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_topology_remove_participant(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id
) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* P2-154: Mutex protection for topology modification */
    nimcp_mutex_lock(ctx->mutex);

    topo_node_t* node = find_node(ctx, participant_id);
    if (!node) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Remove all edges to this node from other nodes */
    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid && ctx->nodes[i].id != participant_id) {
            remove_neighbor(&ctx->nodes[i], participant_id);
        }
    }

    /* Update connection count (saturating subtraction to prevent unsigned underflow) */
    ctx->connection_count = (ctx->connection_count > node->degree) ?
        (ctx->connection_count - node->degree) : 0;

    /* Free and invalidate */
    free_adj_list(node->neighbors);
    node->valid = false;
    if (ctx->node_count > 0) {
        ctx->node_count--;
    }
    ctx->metrics_valid = false;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_topology_clear(mesh_topology_ctx_t ctx) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* P1: Lock mutex to prevent concurrent reads during clear */
    nimcp_mutex_lock(ctx->mutex);

    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            free_adj_list(ctx->nodes[i].neighbors);
            ctx->nodes[i].valid = false;
        }
    }

    ctx->node_count = 0;
    ctx->connection_count = 0;
    ctx->hub_count = 0;
    ctx->num_clusters = 0;
    ctx->metrics_valid = false;
    ctx->clusters_computed = false;

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Topology Analysis
 * ============================================================================ */

/* Forward declaration for unlocked hub identification (defined after public functions) */
static nimcp_error_t identify_hubs_unlocked(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t* hub_ids,
    size_t hub_capacity,
    size_t* num_hubs
);

/**
 * @brief Internal unlocked version of compute_metrics
 *
 * WHAT: Compute topology metrics without acquiring the mutex
 * WHY:  Called from mesh_topology_is_small_world() which may be called
 *       from contexts that already hold the mutex, preventing self-deadlock
 *       on non-recursive mutex
 * PRECONDITION: Caller must hold ctx->mutex
 */
static nimcp_error_t compute_metrics_unlocked(
    mesh_topology_ctx_t ctx,
    mesh_topology_metrics_t* metrics
) {
    if (ctx->node_count == 0) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    memset(metrics, 0, sizeof(mesh_topology_metrics_t));

    metrics->num_participants = (uint32_t)ctx->node_count;
    metrics->num_connections = (uint32_t)ctx->connection_count;

    /* Compute degree statistics */
    double sum_degree = 0;
    double sum_degree_sq = 0;

    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            sum_degree += ctx->nodes[i].degree;
            sum_degree_sq += ctx->nodes[i].degree * ctx->nodes[i].degree;
        }
    }

    metrics->avg_degree = (float)(sum_degree / ctx->node_count);
    double variance = (sum_degree_sq / ctx->node_count) - (metrics->avg_degree * metrics->avg_degree);
    metrics->degree_std = (float)sqrt(variance > 0 ? variance : 0);

    /* Identify hubs (top percentile by degree) - use unlocked version since we hold mutex */
    nimcp_error_t err = identify_hubs_unlocked(ctx, ctx->hub_ids, ctx->hub_capacity, &ctx->hub_count);
    if (err == NIMCP_SUCCESS) {
        metrics->num_hubs = (uint32_t)ctx->hub_count;

        /* Compute hub connectivity fraction */
        uint32_t hub_edges = 0;
        for (size_t i = 0; i < ctx->hub_count; i++) {
            topo_node_t* hub = find_node(ctx, ctx->hub_ids[i]);
            if (hub) hub_edges += hub->degree;
        }
        metrics->hub_connectivity_fraction = ctx->connection_count > 0 ?
            (float)hub_edges / ctx->connection_count : 0.0f;
    }

    /* Optional: clustering coefficient */
    if (ctx->config.compute_clustering && ctx->node_count > 2) {
        double total_clustering = 0.0;
        uint32_t nodes_with_neighbors = 0;

        for (size_t i = 0; i < ctx->node_capacity; i++) {
            if (!ctx->nodes[i].valid || ctx->nodes[i].degree < 2) continue;

            nodes_with_neighbors++;

            /* Count triangles: neighbors connected to each other */
            uint32_t triangles = 0;
            adj_entry_t* e1 = ctx->nodes[i].neighbors;
            while (e1) {
                topo_node_t* n1 = find_node(ctx, e1->neighbor);
                if (n1) {
                    adj_entry_t* e2 = e1->next;
                    while (e2) {
                        /* Check if n1 connects to e2->neighbor */
                        adj_entry_t* n1e = n1->neighbors;
                        while (n1e) {
                            if (n1e->neighbor == e2->neighbor) {
                                triangles++;
                                break;
                            }
                            n1e = n1e->next;
                        }
                        e2 = e2->next;
                    }
                }
                e1 = e1->next;
            }

            uint32_t possible = ctx->nodes[i].degree * (ctx->nodes[i].degree - 1) / 2;
            if (possible > 0) {
                total_clustering += (double)triangles / possible;
            }
        }

        metrics->clustering_coefficient = nodes_with_neighbors > 0 ?
            (float)(total_clustering / nodes_with_neighbors) : 0.0f;
    }

    /* Cache and return */
    ctx->cached_metrics = *metrics;
    ctx->metrics_valid = true;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_topology_compute_metrics(
    mesh_topology_ctx_t ctx,
    mesh_topology_metrics_t* metrics
) {
    if (!ctx || !metrics) return NIMCP_ERROR_INVALID_PARAM;

    /* P1: Lock mutex to get consistent snapshot of topology state */
    nimcp_mutex_lock(ctx->mutex);

    nimcp_error_t result = compute_metrics_unlocked(ctx, metrics);

    nimcp_mutex_unlock(ctx->mutex);

    return result;
}

nimcp_error_t mesh_topology_get_node_info(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id,
    mesh_node_info_t* info
) {
    if (!ctx || !info) return NIMCP_ERROR_INVALID_PARAM;

    /* P1: Lock mutex to get consistent snapshot of node state */
    nimcp_mutex_lock(ctx->mutex);

    topo_node_t* node = find_node(ctx, participant_id);
    if (!node) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    info->participant_id = node->id;
    info->degree = node->degree;
    info->betweenness_centrality = node->betweenness;
    info->closeness_centrality = node->closeness;
    info->eigenvector_centrality = node->eigenvector;
    info->is_hub = node->is_hub;
    info->cluster_id = node->cluster_id;
    info->coordinator_affinity = node->coordinator_affinity;

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Internal unlocked version of identify_hubs
 *
 * WHAT: Identify hub nodes without acquiring the mutex
 * WHY:  Called from compute_metrics_unlocked() which already holds the mutex,
 *       preventing self-deadlock on non-recursive mutex
 * PRECONDITION: Caller must hold ctx->mutex
 */
static nimcp_error_t identify_hubs_unlocked(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t* hub_ids,
    size_t hub_capacity,
    size_t* num_hubs
) {
    if (!ctx || !hub_ids || !num_hubs) return NIMCP_ERROR_INVALID_PARAM;
    if (ctx->node_count == 0) {
        *num_hubs = 0;
        return NIMCP_SUCCESS;
    }

    /* Collect all valid nodes */
    topo_node_t** sorted = (topo_node_t**)nimcp_malloc(ctx->node_count * sizeof(topo_node_t*));
    if (!sorted) return NIMCP_ERROR_NO_MEMORY;

    size_t idx = 0;
    for (size_t i = 0; i < ctx->node_capacity && idx < ctx->node_count; i++) {
        if (ctx->nodes[i].valid) {
            sorted[idx++] = &ctx->nodes[i];
        }
    }

    /* Sort by degree descending */
    qsort(sorted, idx, sizeof(topo_node_t*), compare_degree_desc);

    /* Take top percentile */
    size_t hub_count = (size_t)(ctx->node_count * (1.0f - ctx->config.hub_percentile));
    if (hub_count == 0 && ctx->node_count > 0) hub_count = 1;
    if (hub_count > hub_capacity) hub_count = hub_capacity;

    for (size_t i = 0; i < hub_count; i++) {
        hub_ids[i] = sorted[i]->id;
        sorted[i]->is_hub = true;
    }

    /* Mark non-hubs */
    for (size_t i = hub_count; i < idx; i++) {
        sorted[i]->is_hub = false;
    }

    *num_hubs = hub_count;
    nimcp_free(sorted);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_topology_identify_hubs(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t* hub_ids,
    size_t hub_capacity,
    size_t* num_hubs
) {
    if (!ctx || !hub_ids || !num_hubs) return NIMCP_ERROR_INVALID_PARAM;

    /* P1: Mutex protection for topology read/write */
    nimcp_mutex_lock(ctx->mutex);

    nimcp_error_t result = identify_hubs_unlocked(ctx, hub_ids, hub_capacity, num_hubs);

    nimcp_mutex_unlock(ctx->mutex);
    return result;
}

nimcp_error_t mesh_topology_compute_betweenness(mesh_topology_ctx_t ctx) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* P1: Mutex protection for topology read/write */
    nimcp_mutex_lock(ctx->mutex);

    if (ctx->node_count < 3) {
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_SUCCESS;
    }

    /* Send heartbeat at start of long computation */
    mesh_topology_heartbeat("compute_betweenness", 0.0f);

    /* Simplified betweenness: count shortest paths through each node */
    /* Using BFS-based approximation for efficiency */

    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            ctx->nodes[i].betweenness = 0.0f;
        }
    }

    /* Sample source nodes for efficiency */
    size_t sample_count = ctx->node_count < 50 ? ctx->node_count : 50;
    size_t sampled = 0;

    /* P3-E: Allocate BFS buffers once before loop instead of per-iteration */
    uint32_t* dist = (uint32_t*)nimcp_malloc(ctx->node_capacity * sizeof(uint32_t));
    uint32_t* paths = (uint32_t*)nimcp_malloc(ctx->node_capacity * sizeof(uint32_t));
    size_t* queue = (size_t*)nimcp_malloc(ctx->node_capacity * sizeof(size_t));
    if (!dist || !paths || !queue) {
        nimcp_free(dist);
        nimcp_free(paths);
        nimcp_free(queue);
        nimcp_mutex_unlock(ctx->mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    for (size_t src_idx = 0; src_idx < ctx->node_capacity && sampled < sample_count; src_idx++) {
        if (!ctx->nodes[src_idx].valid) continue;
        sampled++;

        /* Periodic heartbeat during long computation */
        if ((sampled % 10) == 0) {
            float progress = (float)sampled / (float)sample_count;
            mesh_topology_heartbeat("betweenness_bfs", progress);
        }

        /* Reset BFS buffers for this iteration */
        for (size_t j = 0; j < ctx->node_capacity; j++) {
            dist[j] = UINT32_MAX;
            paths[j] = 0;
        }
        dist[src_idx] = 0;
        paths[src_idx] = 1;

        size_t front = 0, back = 0;
        queue[back++] = src_idx;

        while (front < back) {
            size_t curr = queue[front++];

            adj_entry_t* e = ctx->nodes[curr].neighbors;
            while (e) {
                size_t neighbor_idx = SIZE_MAX;
                for (size_t k = 0; k < ctx->node_capacity; k++) {
                    if (ctx->nodes[k].valid && ctx->nodes[k].id == e->neighbor) {
                        neighbor_idx = k;
                        break;
                    }
                }

                if (neighbor_idx != SIZE_MAX) {
                    if (dist[neighbor_idx] == UINT32_MAX) {
                        dist[neighbor_idx] = dist[curr] + 1;
                        paths[neighbor_idx] = paths[curr];
                        queue[back++] = neighbor_idx;
                    } else if (dist[neighbor_idx] == dist[curr] + 1) {
                        paths[neighbor_idx] += paths[curr];
                    }
                }
                e = e->next;
            }
        }

        /* Accumulate betweenness for intermediate nodes */
        for (size_t j = 0; j < ctx->node_capacity; j++) {
            if (ctx->nodes[j].valid && j != src_idx && paths[j] > 0) {
                ctx->nodes[j].betweenness += (float)paths[j];
            }
        }
    }

    nimcp_free(queue);
    nimcp_free(dist);
    nimcp_free(paths);

    /* Normalize betweenness */
    float max_betweenness = 0.0f;
    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid && ctx->nodes[i].betweenness > max_betweenness) {
            max_betweenness = ctx->nodes[i].betweenness;
        }
    }

    if (max_betweenness > 0) {
        for (size_t i = 0; i < ctx->node_capacity; i++) {
            if (ctx->nodes[i].valid) {
                ctx->nodes[i].betweenness /= max_betweenness;
            }
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

bool mesh_topology_is_scale_free(
    mesh_topology_ctx_t ctx,
    float* gamma,
    float* r_squared
) {
    if (!ctx || ctx->node_count < 10) {
        return false;
    }

    /* Compute degree distribution and fit power law P(k) ~ k^gamma */
    /* Using simple linear regression on log-log scale */

    uint32_t max_degree = 0;
    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid && ctx->nodes[i].degree > max_degree) {
            max_degree = ctx->nodes[i].degree;
        }
    }

    if (max_degree < 3) {
        return false;
    }

    /* Count degree frequencies */
    uint32_t* freq = (uint32_t*)nimcp_calloc(max_degree + 1, sizeof(uint32_t));
    if (!freq) {
        return false;
    }

    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            freq[ctx->nodes[i].degree]++;
        }
    }

    /* Linear regression on log(k) vs log(P(k)) */
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0, sum_yy = 0;
    int n = 0;

    for (uint32_t k = 1; k <= max_degree; k++) {
        if (freq[k] > 0) {
            double log_k = log((double)k);
            double log_p = log((double)freq[k] / ctx->node_count);

            sum_x += log_k;
            sum_y += log_p;
            sum_xy += log_k * log_p;
            sum_xx += log_k * log_k;
            sum_yy += log_p * log_p;
            n++;
        }
    }

    nimcp_free(freq);

    if (n < 3) {
        return false;
    }

    /* Slope = gamma */
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);

    /* R-squared */
    double mean_y = sum_y / n;
    double ss_tot = sum_yy - n * mean_y * mean_y;
    double intercept = (sum_y - slope * sum_x) / n;
    double ss_res = 0;

    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid && ctx->nodes[i].degree > 0) {
            double log_k = log((double)ctx->nodes[i].degree);
            double predicted = slope * log_k + intercept;
            double actual = log((double)1.0 / ctx->node_count); /* Simplified */
            ss_res += (actual - predicted) * (actual - predicted);
        }
    }

    double r2 = ss_tot > 0 ? 1.0 - ss_res / ss_tot : 0.0;
    r2 = r2 > 0 ? r2 : 0.0;

    if (gamma) *gamma = (float)slope;
    if (r_squared) *r_squared = (float)r2;

    return r2 > 0.8 && slope < -1.5 && slope > -3.5;
}

bool mesh_topology_is_small_world(
    mesh_topology_ctx_t ctx,
    float* sigma
) {
    if (!ctx || ctx->node_count < 10) {
        return false;
    }

    /* Compute small-world coefficient: sigma = (C/C_rand) / (L/L_rand) */
    /* C = clustering coefficient, L = average path length */
    /* For random graphs: C_rand ≈ k/N, L_rand ≈ ln(N)/ln(k) */

    /* Lock mutex for consistent read of metrics and node_count.
     * Use compute_metrics_unlocked() to avoid self-deadlock since
     * we already hold the mutex. */
    nimcp_mutex_lock(ctx->mutex);

    if (!ctx->metrics_valid) {
        compute_metrics_unlocked(ctx, &ctx->cached_metrics);
    }

    float C = ctx->cached_metrics.clustering_coefficient;
    float k = ctx->cached_metrics.avg_degree;
    float N = (float)ctx->node_count;

    nimcp_mutex_unlock(ctx->mutex);

    if (k < 2 || N < 10) {
        return false;
    }

    /* Random graph values */
    float C_rand = k / N;
    float L_rand = logf(N) / logf(k);

    /* Estimate L (simplified: assume sparse graph) */
    float L = logf(N) / logf(k) * 1.2f; /* Approximate */

    if (C_rand < 0.001f || L_rand < 0.1f) {
        return false;
    }

    float s = (C / C_rand) / (L / L_rand);

    if (sigma) *sigma = s;

    return s > 1.0f;
}

/* ============================================================================
 * Coordinator Placement
 * ============================================================================ */

nimcp_error_t mesh_topology_recommend_placement(
    mesh_topology_ctx_t ctx,
    mesh_coord_placement_t* placement
) {
    if (!ctx || !placement) return NIMCP_ERROR_INVALID_PARAM;

    memset(placement, 0, sizeof(mesh_coord_placement_t));

    /* Compute optimal pool size */
    placement->recommended_pool_size = mesh_topology_optimal_pool_size((uint32_t)ctx->node_count);

    /* Get hubs for leader assignment */
    if (ctx->hub_count > 0) {
        placement->hub_assignments = (mesh_participant_id_t*)nimcp_malloc(
            ctx->hub_count * sizeof(mesh_participant_id_t));
        if (!placement->hub_assignments) return NIMCP_ERROR_NO_MEMORY;

        memcpy(placement->hub_assignments, ctx->hub_ids,
            ctx->hub_count * sizeof(mesh_participant_id_t));
        placement->hub_count = ctx->hub_count;
    }

    /* Compute expected load distribution */
    uint32_t pool_size = placement->recommended_pool_size;
    placement->load_distribution = (float*)nimcp_calloc(pool_size, sizeof(float));
    if (!placement->load_distribution) {
        nimcp_free(placement->hub_assignments);
        return NIMCP_ERROR_NO_MEMORY;
    }
    placement->distribution_size = pool_size;

    /* Assign participants and count load */
    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            uint32_t coord_idx = mesh_topology_assign_coordinator(ctx, ctx->nodes[i].id, pool_size);
            placement->load_distribution[coord_idx] += 1.0f;
        }
    }

    /* Normalize to fractions */
    if (ctx->node_count > 0) {
        for (uint32_t i = 0; i < pool_size; i++) {
            placement->load_distribution[i] /= (float)ctx->node_count;
        }
    }

    return NIMCP_SUCCESS;
}

uint32_t mesh_topology_optimal_pool_size(uint32_t num_participants) {
    if (num_participants <= 4) return MESH_TOPOLOGY_MIN_COORDINATORS;

    /* pool_size = max(3, min(8, 2*log2(N))) */
    double log2_n = log2((double)num_participants);
    uint32_t size = (uint32_t)(MESH_TOPOLOGY_COORD_LOG_RATIO * log2_n);

    if (size < MESH_TOPOLOGY_MIN_COORDINATORS) {
        size = MESH_TOPOLOGY_MIN_COORDINATORS;
    }
    if (size > MESH_TOPOLOGY_MAX_COORDINATORS) {
        size = MESH_TOPOLOGY_MAX_COORDINATORS;
    }

    return size;
}

uint32_t mesh_topology_assign_coordinator(
    mesh_topology_ctx_t ctx,
    mesh_participant_id_t participant_id,
    uint32_t pool_size
) {
    if (!ctx || pool_size == 0) return 0;

    topo_node_t* node = find_node(ctx, participant_id);

    /* Hubs go to leader (index 0) */
    if (node && node->is_hub) {
        return 0;
    }

    /* Others distributed by hash */
    return (uint32_t)(participant_id % pool_size);
}

nimcp_error_t mesh_topology_rebalance_assignments(
    mesh_topology_ctx_t ctx,
    uint32_t pool_size,
    uint32_t* assignments,
    size_t* num_assignments
) {
    if (!ctx || !assignments || !num_assignments || pool_size == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    size_t idx = 0;
    for (size_t i = 0; i < ctx->node_capacity && idx < ctx->node_count; i++) {
        if (ctx->nodes[i].valid) {
            assignments[idx] = mesh_topology_assign_coordinator(ctx, ctx->nodes[i].id, pool_size);
            ctx->nodes[i].coordinator_affinity = assignments[idx];
            idx++;
        }
    }

    *num_assignments = idx;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Cluster Detection
 * ============================================================================ */

nimcp_error_t mesh_topology_detect_clusters(
    mesh_topology_ctx_t ctx,
    uint32_t max_clusters,
    uint32_t* num_clusters
) {
    if (!ctx || !num_clusters) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(ctx->mutex);

    /* Simple connected components algorithm */
    /* Assigns cluster_id to each node */

    /* Reset cluster IDs */
    for (size_t i = 0; i < ctx->node_capacity; i++) {
        if (ctx->nodes[i].valid) {
            ctx->nodes[i].cluster_id = UINT32_MAX;
        }
    }

    uint32_t cluster_count = 0;

    for (size_t i = 0; i < ctx->node_capacity && cluster_count < max_clusters; i++) {
        if (!ctx->nodes[i].valid || ctx->nodes[i].cluster_id != UINT32_MAX) {
            continue;
        }

        /* BFS to find connected component */
        size_t* queue = (size_t*)nimcp_malloc(ctx->node_count * sizeof(size_t));
        if (!queue) {
            nimcp_mutex_unlock(ctx->mutex);
            return NIMCP_ERROR_NO_MEMORY;
        }

        size_t front = 0, back = 0;
        queue[back++] = i;
        ctx->nodes[i].cluster_id = cluster_count;

        while (front < back) {
            size_t curr = queue[front++];

            adj_entry_t* e = ctx->nodes[curr].neighbors;
            while (e) {
                for (size_t k = 0; k < ctx->node_capacity; k++) {
                    if (ctx->nodes[k].valid &&
                        ctx->nodes[k].id == e->neighbor &&
                        ctx->nodes[k].cluster_id == UINT32_MAX) {
                        ctx->nodes[k].cluster_id = cluster_count;
                        queue[back++] = k;
                        break;
                    }
                }
                e = e->next;
            }
        }

        nimcp_free(queue);
        cluster_count++;
    }

    ctx->num_clusters = cluster_count;
    ctx->clusters_computed = true;
    *num_clusters = cluster_count;

    nimcp_mutex_unlock(ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_topology_get_cluster_members(
    mesh_topology_ctx_t ctx,
    uint32_t cluster_id,
    mesh_participant_id_t* participants,
    size_t capacity,
    size_t* count
) {
    if (!ctx || !participants || !count) return NIMCP_ERROR_INVALID_PARAM;
    if (!ctx->clusters_computed) return NIMCP_ERROR_NOT_READY;

    size_t found = 0;
    for (size_t i = 0; i < ctx->node_capacity && found < capacity; i++) {
        if (ctx->nodes[i].valid && ctx->nodes[i].cluster_id == cluster_id) {
            participants[found++] = ctx->nodes[i].id;
        }
    }

    *count = found;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Debugging
 * ============================================================================ */

nimcp_error_t mesh_topology_get_stats(
    mesh_topology_ctx_t ctx,
    uint32_t* num_participants,
    uint32_t* num_connections,
    uint32_t* num_hubs,
    uint32_t* num_clusters
) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    /* P1: Lock mutex to get consistent snapshot of topology stats */
    nimcp_mutex_lock(ctx->mutex);

    if (num_participants) *num_participants = (uint32_t)ctx->node_count;
    if (num_connections) *num_connections = (uint32_t)ctx->connection_count;
    if (num_hubs) *num_hubs = (uint32_t)ctx->hub_count;
    if (num_clusters) *num_clusters = ctx->num_clusters;

    nimcp_mutex_unlock(ctx->mutex);

    return NIMCP_SUCCESS;
}

void mesh_topology_print_debug(mesh_topology_ctx_t ctx) {
    if (!ctx) {
        printf("Mesh Topology: NULL context\n");
        return;
    }

    printf("=== Mesh Topology Debug ===\n");
    printf("Participants: %zu\n", ctx->node_count);
    printf("Connections: %zu\n", ctx->connection_count);
    printf("Hubs: %zu\n", ctx->hub_count);
    printf("Clusters: %u\n", ctx->num_clusters);
    printf("Metrics valid: %s\n", ctx->metrics_valid ? "yes" : "no");

    if (ctx->metrics_valid) {
        printf("Avg degree: %.2f\n", ctx->cached_metrics.avg_degree);
        printf("Clustering: %.3f\n", ctx->cached_metrics.clustering_coefficient);
    }
    printf("===========================\n");
}

void mesh_coord_placement_free(mesh_coord_placement_t* placement) {
    if (!placement) return;

    nimcp_free(placement->hub_assignments);
    nimcp_free(placement->load_distribution);
    placement->hub_assignments = NULL;
    placement->load_distribution = NULL;
    placement->hub_count = 0;
    placement->distribution_size = 0;
}
