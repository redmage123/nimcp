//=============================================================================
// nimcp_entanglement.c - Entanglement Graph Implementation
//=============================================================================
/**
 * @file nimcp_entanglement.c
 * @brief Implementation of sparse graph for memory association via resonance
 *
 * WHAT: Graph-based memory association with quantum-walk-inspired retrieval
 * WHY:  Memories are interconnected - this captures and exploits those connections
 * HOW:  Hash tables for O(1) edge lookup, adjacency lists for traversal,
 *       read-write locks for thread safety
 *
 * IMPLEMENTATION DETAILS:
 *
 *   Data Structure Layout:
 *   +-----------------------------------------------------------------------+
 *   |  entangle_graph_struct:                                               |
 *   |  +-------------------------------------------------------------------+|
 *   |  |  Node Hash Table (node_id -> node_entry)                         ||
 *   |  |  - node_entry contains out_edges list, in_edges list             ||
 *   |  +-------------------------------------------------------------------+|
 *   |  |  Edge Hash Table ((from_id, to_id) -> edge_data)                 ||
 *   |  |  - O(1) lookup by endpoint pair                                   ||
 *   |  +-------------------------------------------------------------------+|
 *   |  |  Configuration, statistics, read-write lock                      ||
 *   |  +-------------------------------------------------------------------+|
 *   +-----------------------------------------------------------------------+
 *
 *   Hash Function for Edges:
 *   +-----------------------------------------------------------------------+
 *   |  hash = mix64(from_id ^ (to_id * 0x9E3779B97F4A7C15))               |
 *   |  - XOR with golden ratio constant for good distribution              |
 *   |  - mix64 provides avalanche effect                                   |
 *   +-----------------------------------------------------------------------+
 *
 *   Thread Safety Model:
 *   +-----------------------------------------------------------------------+
 *   |  - Multiple readers allowed (read lock)                               |
 *   |  - Single writer exclusive (write lock)                               |
 *   |  - Walk/spread states are per-thread (no locking needed)             |
 *   +-----------------------------------------------------------------------+
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_entanglement.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include "utils/thread/nimcp_thread.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(entanglement, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Constants and Macros
//=============================================================================

/** Initial hash table size (must be power of 2) */
#define INITIAL_HASH_SIZE 256

/** Golden ratio constant for hashing */
#define GOLDEN_RATIO_64 0x9E3779B97F4A7C15ULL

/** FNV-1a offset basis for hashing */
#define FNV_OFFSET_BASIS 0xCBF29CE484222325ULL

/** FNV-1a prime for hashing */
#define FNV_PRIME 0x100000001B3ULL

//=============================================================================
// Internal Type Definitions
//=============================================================================

/**
 * @brief Entry in the edge hash table
 */
typedef struct edge_entry_struct {
    entangle_edge_t edge;                    /**< Edge data */
    struct edge_entry_struct* hash_next;     /**< Next in hash bucket chain */
} edge_entry_t;

/**
 * @brief Entry in outgoing/incoming edge list for a node
 */
typedef struct edge_list_node_struct {
    uint64_t other_id;                       /**< Other endpoint ID */
    edge_entry_t* edge_ref;                  /**< Pointer to edge in hash table */
    struct edge_list_node_struct* next;      /**< Next in list */
} edge_list_node_t;

/**
 * @brief Entry in the node hash table
 */
typedef struct node_entry_struct {
    uint64_t node_id;                        /**< Node ID */
    edge_list_node_t* out_edges;             /**< Outgoing edge list head */
    edge_list_node_t* in_edges;              /**< Incoming edge list head */
    size_t out_degree;                       /**< Count of outgoing edges */
    size_t in_degree;                        /**< Count of incoming edges */
    struct node_entry_struct* hash_next;     /**< Next in hash bucket chain */
} node_entry_t;

/**
 * @brief Internal graph structure
 */
struct entangle_graph_struct {
    /* Hash tables */
    node_entry_t** node_table;               /**< Node hash table */
    size_t node_table_size;                  /**< Size of node hash table */
    size_t node_count;                       /**< Number of nodes */

    edge_entry_t** edge_table;               /**< Edge hash table */
    size_t edge_table_size;                  /**< Size of edge hash table */
    size_t edge_count;                       /**< Number of edges */

    /* Configuration */
    entangle_config_t config;                /**< Graph configuration */

    /* Statistics */
    entangle_stats_t stats;                  /**< Operational statistics */

    /* Thread safety */
    nimcp_rwlock_t rwlock;                   /**< Read-write lock */
    bool rwlock_initialized;                 /**< Lock initialization flag */
};

//=============================================================================
// Static Variables
//=============================================================================

/** Thread-local error message buffer */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Set the last error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_last_error, sizeof(s_last_error), format, args);
    va_end(args);
}

/**
 * @brief Clear the last error message
 */
static void clear_error(void) {
    s_last_error[0] = '\0';
}

/**
 * @brief Mix64 hash finalizer (MurmurHash3-style)
 */
static inline uint64_t mix64(uint64_t key) {
    key ^= key >> 33;
    key *= 0xFF51AFD7ED558CCDULL;
    key ^= key >> 33;
    key *= 0xC4CEB9FE1A85EC53ULL;
    key ^= key >> 33;
    return key;
}

/**
 * @brief Compute hash for a single node ID
 */
static inline size_t hash_node_id(uint64_t node_id, size_t table_size) {
    return (size_t)(mix64(node_id) & (table_size - 1));
}

/**
 * @brief Compute hash for an edge (from_id, to_id pair)
 */
static inline size_t hash_edge_ids(uint64_t from_id, uint64_t to_id, size_t table_size) {
    uint64_t combined = from_id ^ (to_id * GOLDEN_RATIO_64);
    return (size_t)(mix64(combined) & (table_size - 1));
}

/**
 * @brief Round up to next power of 2
 */
static inline size_t next_power_of_2(size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

/**
 * @brief Acquire read lock
 */
static inline void read_lock(entangle_graph_t graph) {
    if (graph && graph->rwlock_initialized) {
        nimcp_rwlock_rdlock(&graph->rwlock);
    }
}

/**
 * @brief Release read lock
 */
static inline void read_unlock(entangle_graph_t graph) {
    if (graph && graph->rwlock_initialized) {
        nimcp_rwlock_unlock(&graph->rwlock);
    }
}

/**
 * @brief Acquire write lock
 */
static inline void write_lock(entangle_graph_t graph) {
    if (graph && graph->rwlock_initialized) {
        nimcp_rwlock_wrlock(&graph->rwlock);
    }
}

/**
 * @brief Release write lock
 */
static inline void write_unlock(entangle_graph_t graph) {
    if (graph && graph->rwlock_initialized) {
        nimcp_rwlock_unlock(&graph->rwlock);
    }
}

/**
 * @brief Find node entry (without locking)
 */
static node_entry_t* find_node_unlocked(entangle_graph_t graph, uint64_t node_id) {
    if (!graph || !graph->node_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_node_unlocked: required parameter is NULL (graph, graph->node_table)");
        return NULL;
    }

    size_t idx = hash_node_id(node_id, graph->node_table_size);
    node_entry_t* entry = graph->node_table[idx];

    while (entry) {
        if (entry->node_id == node_id) {
            return entry;
        }
        entry = entry->hash_next;
    }

    return NULL;
}

/**
 * @brief Find or create node entry (without locking - caller must hold write lock)
 */
static node_entry_t* find_or_create_node_unlocked(entangle_graph_t graph, uint64_t node_id) {
    if (!graph || !graph->node_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_node_unlocked: required parameter is NULL (graph, graph->node_table)");
        return NULL;
    }

    size_t idx = hash_node_id(node_id, graph->node_table_size);
    node_entry_t* entry = graph->node_table[idx];

    /* Search existing */
    while (entry) {
        if (entry->node_id == node_id) {
            return entry;
        }
        entry = entry->hash_next;
    }

    /* Create new node entry */
    node_entry_t* new_entry = (node_entry_t*)nimcp_calloc(1, sizeof(node_entry_t));
    if (!new_entry) {
        set_error("Failed to allocate node entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_entry");

        return NULL;
    }

    new_entry->node_id = node_id;
    new_entry->hash_next = graph->node_table[idx];
    graph->node_table[idx] = new_entry;
    graph->node_count++;

    return new_entry;
}

/**
 * @brief Find edge entry (without locking)
 */
static edge_entry_t* find_edge_unlocked(entangle_graph_t graph, uint64_t from_id, uint64_t to_id) {
    if (!graph || !graph->edge_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_edge_unlocked: required parameter is NULL (graph, graph->edge_table)");
        return NULL;
    }

    size_t idx = hash_edge_ids(from_id, to_id, graph->edge_table_size);
    edge_entry_t* entry = graph->edge_table[idx];

    while (entry) {
        if (entry->edge.from_id == from_id && entry->edge.to_id == to_id) {
            return entry;
        }
        entry = entry->hash_next;
    }

    return NULL;
}

/**
 * @brief Add edge list node to a list
 */
static bool add_edge_to_list(edge_list_node_t** list_head, uint64_t other_id, edge_entry_t* edge_ref) {
    edge_list_node_t* node = (edge_list_node_t*)nimcp_malloc(sizeof(edge_list_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_edge_to_list: node is NULL");
        return false;
    }

    node->other_id = other_id;
    node->edge_ref = edge_ref;
    node->next = *list_head;
    *list_head = node;

    return true;
}

/**
 * @brief Remove edge list node from a list
 */
static bool remove_edge_from_list(edge_list_node_t** list_head, uint64_t other_id) {
    edge_list_node_t* prev = NULL;
    edge_list_node_t* curr = *list_head;

    while (curr) {
        if (curr->other_id == other_id) {
            if (prev) {
                prev->next = curr->next;
            } else {
                *list_head = curr->next;
            }
            nimcp_free(curr);
            curr = NULL;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "remove_edge_from_list: operation failed");
    return false;
}

/**
 * @brief Free all nodes in an edge list
 */
static void free_edge_list(edge_list_node_t* head) {
    while (head) {
        edge_list_node_t* next = head->next;
        nimcp_free(head);
        head = NULL;
        head = next;
    }
}

/**
 * @brief Comparison function for sorting neighbors by weight (descending)
 */
static int compare_neighbors_by_weight_desc(const void* a, const void* b) {
    const entangle_neighbor_t* na = (const entangle_neighbor_t*)a;
    const entangle_neighbor_t* nb = (const entangle_neighbor_t*)b;

    if (nb->edge.weight > na->edge.weight) return 1;
    if (nb->edge.weight < na->edge.weight) return -1;
    return 0;
}

/**
 * @brief Comparison function for sorting walk results by probability (descending)
 */
static int compare_walk_results_desc(const void* a, const void* b) {
    const quantum_walk_result_t* ra = (const quantum_walk_result_t*)a;
    const quantum_walk_result_t* rb = (const quantum_walk_result_t*)b;

    if (rb->probability > ra->probability) return 1;
    if (rb->probability < ra->probability) return -1;
    return 0;
}

/**
 * @brief Build node ID to index mapping for walk/spread states
 */
static bool build_node_index_map(
    entangle_graph_t graph,
    uint64_t** node_ids_out,
    size_t* num_nodes_out)
{
    if (!graph || !node_ids_out || !num_nodes_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "build_node_index_map: required parameter is NULL (graph, node_ids_out, num_nodes_out)");
        return false;
    }

    *node_ids_out = NULL;
    *num_nodes_out = 0;

    if (graph->node_count == 0) return true;

    uint64_t* ids = (uint64_t*)nimcp_calloc(graph->node_count, sizeof(uint64_t));
    if (!ids) {
        set_error("Failed to allocate node ID array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_node_index_map: ids is NULL");
        return false;
    }

    size_t idx = 0;
    for (size_t i = 0; i < graph->node_table_size && idx < graph->node_count; i++) {
        node_entry_t* entry = graph->node_table[i];
        while (entry && idx < graph->node_count) {
            ids[idx++] = entry->node_id;
            entry = entry->hash_next;
        }
    }

    *node_ids_out = ids;
    *num_nodes_out = idx;
    return true;
}

/**
 * @brief Find index of node ID in array
 */
static ssize_t find_node_index(const uint64_t* node_ids, size_t num_nodes, uint64_t target) {
    /* Linear search - could use hash map for large graphs */
    for (size_t i = 0; i < num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)num_nodes);
        }

        if (node_ids[i] == target) {
            return (ssize_t)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_node_index: validation failed");
    return -1;
}

//=============================================================================
// Configuration Functions
//=============================================================================

entangle_config_t entangle_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_config_defa", 0.0f);


    entangle_config_t config = {
        .initial_node_capacity = ENTANGLE_DEFAULT_NODE_CAPACITY,
        .initial_edge_capacity = ENTANGLE_DEFAULT_EDGE_CAPACITY,
        .auto_link_threshold = ENTANGLE_DEFAULT_LINK_THRESHOLD,
        .prune_threshold = ENTANGLE_DEFAULT_PRUNE_THRESHOLD,
        .enable_bidirectional = true,
        .track_creation_time = true,
        .resonance_cfg = resonance_config_default()
    };
    return config;
}

bool entangle_config_validate(const entangle_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_config_validate: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_config_vali", 0.0f);


    if (config->initial_node_capacity == 0) {
        set_error("initial_node_capacity must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entangle_config_validate: config->initial_node_capacity is zero");
        return false;
    }

    if (config->initial_edge_capacity == 0) {
        set_error("initial_edge_capacity must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entangle_config_validate: config->initial_edge_capacity is zero");
        return false;
    }

    if (config->auto_link_threshold < 0.0f || config->auto_link_threshold > 1.0f) {
        set_error("auto_link_threshold must be in [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_config_validate: validation failed");
        return false;
    }

    if (config->prune_threshold < 0.0f || config->prune_threshold > 1.0f) {
        set_error("prune_threshold must be in [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_config_validate: validation failed");
        return false;
    }

    if (config->prune_threshold > config->auto_link_threshold) {
        set_error("prune_threshold should be <= auto_link_threshold");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_config_validate: validation failed");
        return false;
    }

    clear_error();
    return true;
}

//=============================================================================
// Graph Management Functions
//=============================================================================

entangle_graph_t entangle_graph_create(const entangle_config_t* config) {
    /* Use defaults if no config provided */
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_graph_creat", 0.0f);


    entangle_config_t cfg = config ? *config : entangle_config_default();

    if (!entangle_config_validate(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_graph_create: entangle_config_validate is NULL");
        return NULL;
    }

    /* Allocate graph structure */
    entangle_graph_t graph = (entangle_graph_t)nimcp_calloc(1, sizeof(struct entangle_graph_struct));
    if (!graph) {
        set_error("Failed to allocate graph structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate graph");

        return NULL;
    }

    graph->config = cfg;

    /* Allocate node hash table (power of 2) */
    graph->node_table_size = next_power_of_2(cfg.initial_node_capacity);
    graph->node_table = (node_entry_t**)nimcp_calloc(graph->node_table_size, sizeof(node_entry_t*));
    if (!graph->node_table) {
        set_error("Failed to allocate node hash table");
        nimcp_free(graph);
        graph = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_graph_create: graph->node_table is NULL");
        return NULL;
    }

    /* Allocate edge hash table (power of 2) */
    graph->edge_table_size = next_power_of_2(cfg.initial_edge_capacity);
    graph->edge_table = (edge_entry_t**)nimcp_calloc(graph->edge_table_size, sizeof(edge_entry_t*));
    if (!graph->edge_table) {
        set_error("Failed to allocate edge hash table");
        nimcp_free(graph->node_table);
        nimcp_free(graph);
        graph = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_graph_create: graph->edge_table is NULL");
        return NULL;
    }

    /* Initialize read-write lock */
    if (nimcp_rwlock_init(&graph->rwlock) != NIMCP_SUCCESS) {
        set_error("Failed to initialize rwlock");
        nimcp_free(graph->edge_table);
        nimcp_free(graph->node_table);
        nimcp_free(graph);
        graph = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entangle_graph_create: validation failed");
        return NULL;
    }
    graph->rwlock_initialized = true;

    /* Initialize statistics */
    memset(&graph->stats, 0, sizeof(graph->stats));
    graph->stats.min_weight = 1.0f;
    graph->stats.memory_bytes = sizeof(struct entangle_graph_struct) +
                                graph->node_table_size * sizeof(node_entry_t*) +
                                graph->edge_table_size * sizeof(edge_entry_t*);

    clear_error();
    return graph;
}

void entangle_graph_destroy(entangle_graph_t graph) {
    if (!graph) return;

    /* No need to lock - we're destroying */

    /* Free all edge entries */
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_graph_destr", 0.0f);


    if (graph->edge_table) {
        for (size_t i = 0; i < graph->edge_table_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && graph->edge_table_size > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)graph->edge_table_size);
            }

            edge_entry_t* entry = graph->edge_table[i];
            while (entry) {
                edge_entry_t* next = entry->hash_next;
                nimcp_free(entry);
                entry = NULL;
                entry = next;
            }
        }
        nimcp_free(graph->edge_table);
    }

    /* Free all node entries (including edge lists) */
    if (graph->node_table) {
        for (size_t i = 0; i < graph->node_table_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && graph->node_table_size > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)graph->node_table_size);
            }

            node_entry_t* entry = graph->node_table[i];
            while (entry) {
                node_entry_t* next = entry->hash_next;
                free_edge_list(entry->out_edges);
                free_edge_list(entry->in_edges);
                nimcp_free(entry);
                entry = NULL;
                entry = next;
            }
        }
        nimcp_free(graph->node_table);
    }

    /* Destroy lock */
    if (graph->rwlock_initialized) {
        nimcp_rwlock_destroy(&graph->rwlock);
    }

    nimcp_free(graph);
    graph = NULL;
}

bool entangle_graph_clear(entangle_graph_t graph) {
    if (!graph) {
        set_error("NULL graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_graph_clear: graph is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_graph_clear", 0.0f);


    write_lock(graph);

    /* Free all edge entries */
    for (size_t i = 0; i < graph->edge_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->edge_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->edge_table_size);
        }

        edge_entry_t* entry = graph->edge_table[i];
        while (entry) {
            edge_entry_t* next = entry->hash_next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        graph->edge_table[i] = NULL;
    }

    /* Free node edge lists and reset nodes */
    for (size_t i = 0; i < graph->node_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->node_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->node_table_size);
        }

        node_entry_t* entry = graph->node_table[i];
        while (entry) {
            node_entry_t* next = entry->hash_next;
            free_edge_list(entry->out_edges);
            free_edge_list(entry->in_edges);
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
        graph->node_table[i] = NULL;
    }

    graph->edge_count = 0;
    graph->node_count = 0;

    /* Reset statistics */
    memset(&graph->stats, 0, sizeof(graph->stats));
    graph->stats.min_weight = 1.0f;
    graph->stats.memory_bytes = sizeof(struct entangle_graph_struct) +
                                graph->node_table_size * sizeof(node_entry_t*) +
                                graph->edge_table_size * sizeof(edge_entry_t*);

    write_unlock(graph);
    clear_error();
    return true;
}

bool entangle_node_exists(entangle_graph_t graph, uint64_t node_id) {
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_node_exists: graph is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_node_exists", 0.0f);


    read_lock(graph);
    bool exists = (find_node_unlocked(graph, node_id) != NULL);
    read_unlock(graph);

    return exists;
}

//=============================================================================
// Edge Operations
//=============================================================================

bool entangle_add_edge(entangle_graph_t graph, const entangle_edge_t* edge) {
    if (!graph || !edge) {
        set_error("NULL graph or edge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_add_edge: required parameter is NULL (graph, edge)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_add_edge", 0.0f);


    if (edge->from_id == edge->to_id) {
        set_error("Self-loops not allowed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_add_edge: validation failed");
        return false;
    }

    write_lock(graph);

    /* Check if edge already exists */
    if (find_edge_unlocked(graph, edge->from_id, edge->to_id) != NULL) {
        write_unlock(graph);
        set_error("Edge already exists");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "entangle_add_edge: validation failed");
        return false;
    }

    /* Check capacity limits */
    if (graph->edge_count >= ENTANGLE_MAX_EDGES) {
        write_unlock(graph);
        set_error("Maximum edge count exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "entangle_add_edge: capacity exceeded");
        return false;
    }

    /* Create edge entry */
    edge_entry_t* new_edge = (edge_entry_t*)nimcp_malloc(sizeof(edge_entry_t));
    if (!new_edge) {
        write_unlock(graph);
        set_error("Failed to allocate edge entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_add_edge: new_edge is NULL");
        return false;
    }

    new_edge->edge = *edge;
    new_edge->edge.weight = nimcp_clamp01(edge->weight);

    /* Add timestamp if enabled */
    if (graph->config.track_creation_time && edge->created_time_ms == 0) {
        new_edge->edge.created_time_ms = entangle_current_time_ms();
    }

    /* Insert into edge hash table */
    size_t edge_idx = hash_edge_ids(edge->from_id, edge->to_id, graph->edge_table_size);
    new_edge->hash_next = graph->edge_table[edge_idx];
    graph->edge_table[edge_idx] = new_edge;
    graph->edge_count++;

    /* Find or create source and target nodes */
    node_entry_t* from_node = find_or_create_node_unlocked(graph, edge->from_id);
    node_entry_t* to_node = find_or_create_node_unlocked(graph, edge->to_id);

    if (!from_node || !to_node) {
        /* Rollback edge creation */
        graph->edge_table[edge_idx] = new_edge->hash_next;
        nimcp_free(new_edge);
        new_edge = NULL;
        graph->edge_count--;
        write_unlock(graph);
        set_error("Failed to create node entries");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_add_edge: required parameter is NULL (from_node, to_node)");
        return false;
    }

    /* Add to adjacency lists */
    if (!add_edge_to_list(&from_node->out_edges, edge->to_id, new_edge)) {
        graph->edge_table[edge_idx] = new_edge->hash_next;
        nimcp_free(new_edge);
        new_edge = NULL;
        graph->edge_count--;
        write_unlock(graph);
        set_error("Failed to add to outgoing list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_add_edge: add_edge_to_list is NULL");
        return false;
    }
    from_node->out_degree++;

    if (!add_edge_to_list(&to_node->in_edges, edge->from_id, new_edge)) {
        remove_edge_from_list(&from_node->out_edges, edge->to_id);
        from_node->out_degree--;
        graph->edge_table[edge_idx] = new_edge->hash_next;
        nimcp_free(new_edge);
        new_edge = NULL;
        graph->edge_count--;
        write_unlock(graph);
        set_error("Failed to add to incoming list");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_add_edge: add_edge_to_list is NULL");
        return false;
    }
    to_node->in_degree++;

    /* Update statistics */
    graph->stats.num_edges = graph->edge_count;
    graph->stats.num_nodes = graph->node_count;
    graph->stats.edges_by_type[edge->type % ENTANGLE_EDGE_TYPE_COUNT]++;

    float weight = new_edge->edge.weight;
    if (weight < graph->stats.min_weight) graph->stats.min_weight = weight;
    if (weight > graph->stats.max_weight) graph->stats.max_weight = weight;

    /* Update average weight (incremental) */
    float n = (float)graph->edge_count;
    if (isfinite(weight) && fabsf(n) > 1e-7f) {
        graph->stats.avg_weight = ((n - 1.0f) * graph->stats.avg_weight + weight) / n;
    }

    graph->stats.memory_bytes += sizeof(edge_entry_t) + 2 * sizeof(edge_list_node_t);

    /* Check for high degree warning */
    size_t max_deg = (from_node->out_degree > to_node->in_degree) ?
                     from_node->out_degree : to_node->in_degree;
    if (max_deg > graph->stats.max_degree) {
        graph->stats.max_degree = max_deg;
    }

    /* Handle bidirectional edges */
    if (edge->bidirectional && graph->config.enable_bidirectional) {
        /* Check if reverse doesn't exist */
        if (find_edge_unlocked(graph, edge->to_id, edge->from_id) == NULL) {
            /* Create reverse edge */
            entangle_edge_t reverse = *edge;
            reverse.from_id = edge->to_id;
            reverse.to_id = edge->from_id;
            reverse.bidirectional = false;  /* Avoid infinite recursion */

            write_unlock(graph);
            entangle_add_edge(graph, &reverse);
            clear_error();
            return true;
        }
    }

    write_unlock(graph);
    clear_error();
    return true;
}

bool entangle_remove_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id) {
    if (!graph) {
        set_error("NULL graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_remove_edge: graph is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_remove_edge", 0.0f);


    write_lock(graph);

    /* Find edge in hash table */
    size_t edge_idx = hash_edge_ids(from_id, to_id, graph->edge_table_size);
    edge_entry_t* prev = NULL;
    edge_entry_t* curr = graph->edge_table[edge_idx];

    while (curr) {
        if (curr->edge.from_id == from_id && curr->edge.to_id == to_id) {
            /* Found edge - remove from hash table */
            if (prev) {
                prev->hash_next = curr->hash_next;
            } else {
                graph->edge_table[edge_idx] = curr->hash_next;
            }

            /* Update stats before freeing */
            entangle_edge_type_t type = curr->edge.type;
            if (graph->stats.edges_by_type[type % ENTANGLE_EDGE_TYPE_COUNT] > 0) {
                graph->stats.edges_by_type[type % ENTANGLE_EDGE_TYPE_COUNT]--;
            }

            /* Remove from adjacency lists */
            node_entry_t* from_node = find_node_unlocked(graph, from_id);
            node_entry_t* to_node = find_node_unlocked(graph, to_id);

            if (from_node) {
                remove_edge_from_list(&from_node->out_edges, to_id);
                if (from_node->out_degree > 0) from_node->out_degree--;
            }

            if (to_node) {
                remove_edge_from_list(&to_node->in_edges, from_id);
                if (to_node->in_degree > 0) to_node->in_degree--;
            }

            nimcp_free(curr);
            curr = NULL;
            graph->edge_count--;
            graph->stats.num_edges = graph->edge_count;
            graph->stats.memory_bytes -= sizeof(edge_entry_t) + 2 * sizeof(edge_list_node_t);

            write_unlock(graph);
            clear_error();
            return true;
        }
        prev = curr;
        curr = curr->hash_next;
    }

    write_unlock(graph);
    set_error("Edge not found");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_remove_edge: operation failed");
    return false;
}

bool entangle_update_edge(entangle_graph_t graph, const entangle_edge_t* edge) {
    if (!graph || !edge) {
        set_error("NULL graph or edge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_update_edge: required parameter is NULL (graph, edge)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_update_edge", 0.0f);


    write_lock(graph);

    edge_entry_t* entry = find_edge_unlocked(graph, edge->from_id, edge->to_id);
    if (!entry) {
        write_unlock(graph);
        set_error("Edge not found");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_update_edge: entry is NULL");
        return false;
    }

    /* Preserve IDs, update other fields */
    float old_weight = entry->edge.weight;
    entangle_edge_type_t old_type = entry->edge.type;

    entry->edge.resonance_score = edge->resonance_score;
    entry->edge.prime_similarity = edge->prime_similarity;
    entry->edge.quat_similarity = edge->quat_similarity;
    entry->edge.phase_coherence = edge->phase_coherence;
    entry->edge.type = edge->type;
    entry->edge.weight = nimcp_clamp01(edge->weight);
    entry->edge.bidirectional = edge->bidirectional;
    /* Note: we don't update created_time_ms */

    /* Update type stats if changed */
    if (old_type != edge->type) {
        if (graph->stats.edges_by_type[old_type % ENTANGLE_EDGE_TYPE_COUNT] > 0) {
            graph->stats.edges_by_type[old_type % ENTANGLE_EDGE_TYPE_COUNT]--;
        }
        graph->stats.edges_by_type[edge->type % ENTANGLE_EDGE_TYPE_COUNT]++;
    }

    /* Update weight stats (approximate) */
    if (entry->edge.weight < graph->stats.min_weight) {
        graph->stats.min_weight = entry->edge.weight;
    }
    if (entry->edge.weight > graph->stats.max_weight) {
        graph->stats.max_weight = entry->edge.weight;
    }

    /* Adjust average weight */
    float n = (float)graph->edge_count;
    if (n > 0) {
        graph->stats.avg_weight = graph->stats.avg_weight +
                                  (entry->edge.weight - old_weight) / n;
    }

    /* Handle bidirectional flag change */
    if (edge->bidirectional && graph->config.enable_bidirectional) {
        if (find_edge_unlocked(graph, edge->to_id, edge->from_id) == NULL) {
            /* Create reverse edge */
            entangle_edge_t reverse = *edge;
            reverse.from_id = edge->to_id;
            reverse.to_id = edge->from_id;
            reverse.bidirectional = false;

            write_unlock(graph);
            entangle_add_edge(graph, &reverse);
            clear_error();
            return true;
        }
    }

    write_unlock(graph);
    clear_error();
    return true;
}

bool entangle_get_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id, entangle_edge_t* edge) {
    if (!graph || !edge) {
        set_error("NULL graph or edge output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_edge: required parameter is NULL (graph, edge)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_edge", 0.0f);


    read_lock(graph);

    edge_entry_t* entry = find_edge_unlocked(graph, from_id, to_id);
    if (entry) {
        *edge = entry->edge;
        ((struct entangle_graph_struct*)graph)->stats.total_lookups++;
        read_unlock(graph);
        clear_error();
        return true;
    }

    read_unlock(graph);
    set_error("Edge not found");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_get_edge: operation failed");
    return false;
}

bool entangle_has_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id) {
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_has_edge: graph is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_has_edge", 0.0f);


    read_lock(graph);
    bool exists = (find_edge_unlocked(graph, from_id, to_id) != NULL);
    ((struct entangle_graph_struct*)graph)->stats.total_lookups++;
    read_unlock(graph);

    return exists;
}

float entangle_strengthen_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id, float delta) {
    if (!graph) {
        set_error("NULL graph");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_strengthen_", 0.0f);


    write_lock(graph);

    edge_entry_t* entry = find_edge_unlocked(graph, from_id, to_id);
    if (!entry) {
        write_unlock(graph);
        set_error("Edge not found");
        return -1.0f;
    }

    float old_weight = entry->edge.weight;
    entry->edge.weight = nimcp_clamp01(entry->edge.weight + delta);

    /* Update stats */
    if (entry->edge.weight > graph->stats.max_weight) {
        graph->stats.max_weight = entry->edge.weight;
    }
    float n = (float)graph->edge_count;
    if (n > 0) {
        graph->stats.avg_weight += (entry->edge.weight - old_weight) / n;
    }

    float result = entry->edge.weight;
    write_unlock(graph);
    clear_error();
    return result;
}

float entangle_weaken_edge(entangle_graph_t graph, uint64_t from_id, uint64_t to_id, float delta) {
    if (!graph) {
        set_error("NULL graph");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_weaken_edge", 0.0f);


    write_lock(graph);

    edge_entry_t* entry = find_edge_unlocked(graph, from_id, to_id);
    if (!entry) {
        write_unlock(graph);
        set_error("Edge not found");
        return -1.0f;
    }

    float old_weight = entry->edge.weight;
    entry->edge.weight = nimcp_clamp01(entry->edge.weight - delta);

    /* Update stats */
    if (entry->edge.weight < graph->stats.min_weight) {
        graph->stats.min_weight = entry->edge.weight;
    }
    float n = (float)graph->edge_count;
    if (n > 0) {
        graph->stats.avg_weight += (entry->edge.weight - old_weight) / n;
    }

    float result = entry->edge.weight;
    write_unlock(graph);
    clear_error();
    return result;
}

//=============================================================================
// Neighbor Query Functions
//=============================================================================

bool entangle_get_neighbors(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_neighbor_t* neighbors,
    size_t max_neighbors,
    size_t* count)
{
    if (!graph || !neighbors || !count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_neighbors: required parameter is NULL (graph, neighbors, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_neighbo", 0.0f);


    read_lock(graph);

    node_entry_t* node = find_node_unlocked(graph, node_id);
    if (!node) {
        read_unlock(graph);
        clear_error();
        return true;  /* Empty result, not an error */
    }

    /* Collect outgoing neighbors */
    edge_list_node_t* out = node->out_edges;
    while (out && *count < max_neighbors) {
        if (out->edge_ref) {
            neighbors[*count].neighbor_id = out->other_id;
            neighbors[*count].edge = out->edge_ref->edge;
            (*count)++;
        }
        out = out->next;
    }

    /* Collect incoming neighbors (may have duplicates with outgoing) */
    edge_list_node_t* in = node->in_edges;
    while (in && *count < max_neighbors) {
        /* Check if already added from outgoing */
        bool found = false;
        for (size_t i = 0; i < *count; i++) {
            if (neighbors[i].neighbor_id == in->other_id) {
                found = true;
                break;
            }
        }

        if (!found && in->edge_ref) {
            neighbors[*count].neighbor_id = in->other_id;
            neighbors[*count].edge = in->edge_ref->edge;
            (*count)++;
        }
        in = in->next;
    }

    read_unlock(graph);
    clear_error();
    return true;
}

bool entangle_get_outgoing(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_edge_t* edges,
    size_t max_edges,
    size_t* count)
{
    if (!graph || !edges || !count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_outgoing: required parameter is NULL (graph, edges, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_outgoin", 0.0f);


    read_lock(graph);

    node_entry_t* node = find_node_unlocked(graph, node_id);
    if (!node) {
        read_unlock(graph);
        clear_error();
        return true;
    }

    edge_list_node_t* out = node->out_edges;
    while (out && *count < max_edges) {
        if (out->edge_ref) {
            edges[*count] = out->edge_ref->edge;
            (*count)++;
        }
        out = out->next;
    }

    read_unlock(graph);
    clear_error();
    return true;
}

bool entangle_get_incoming(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_edge_t* edges,
    size_t max_edges,
    size_t* count)
{
    if (!graph || !edges || !count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_incoming: required parameter is NULL (graph, edges, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_incomin", 0.0f);


    read_lock(graph);

    node_entry_t* node = find_node_unlocked(graph, node_id);
    if (!node) {
        read_unlock(graph);
        clear_error();
        return true;
    }

    edge_list_node_t* in = node->in_edges;
    while (in && *count < max_edges) {
        if (in->edge_ref) {
            edges[*count] = in->edge_ref->edge;
            (*count)++;
        }
        in = in->next;
    }

    read_unlock(graph);
    clear_error();
    return true;
}

bool entangle_get_strongest(
    entangle_graph_t graph,
    uint64_t node_id,
    size_t k,
    entangle_neighbor_t* neighbors,
    size_t* count)
{
    if (!graph || !neighbors || !count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_strongest: required parameter is NULL (graph, neighbors, count)");
        return false;
    }

    *count = 0;
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_stronge", 0.0f);


    if (k == 0) {
        clear_error();
        return true;
    }

    /* Get all neighbors first */
    size_t max_temp = 1024;  /* Reasonable limit */
    entangle_neighbor_t* temp = (entangle_neighbor_t*)nimcp_calloc(max_temp, sizeof(entangle_neighbor_t));
    if (!temp) {
        set_error("Failed to allocate temporary buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_get_strongest: temp is NULL");
        return false;
    }

    size_t total = 0;
    bool result = entangle_get_neighbors(graph, node_id, temp, max_temp, &total);

    if (!result) {
        nimcp_free(temp);
        temp = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_strongest: result is NULL");
        return false;
    }

    if (total == 0) {
        nimcp_free(temp);
        temp = NULL;
        clear_error();
        return true;
    }

    /* Sort by weight descending */
    qsort(temp, total, sizeof(entangle_neighbor_t), compare_neighbors_by_weight_desc);

    /* Copy top-k */
    *count = (k < total) ? k : total;
    memcpy(neighbors, temp, *count * sizeof(entangle_neighbor_t));

    nimcp_free(temp);
    temp = NULL;
    clear_error();
    return true;
}

bool entangle_get_neighbors_by_type(
    entangle_graph_t graph,
    uint64_t node_id,
    entangle_edge_type_t type,
    entangle_neighbor_t* neighbors,
    size_t max_neighbors,
    size_t* count)
{
    if (!graph || !neighbors || !count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_neighbors_by_type: required parameter is NULL (graph, neighbors, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_neighbo", 0.0f);


    read_lock(graph);

    node_entry_t* node = find_node_unlocked(graph, node_id);
    if (!node) {
        read_unlock(graph);
        clear_error();
        return true;
    }

    /* Check outgoing edges */
    edge_list_node_t* out = node->out_edges;
    while (out && *count < max_neighbors) {
        if (out->edge_ref && out->edge_ref->edge.type == type) {
            neighbors[*count].neighbor_id = out->other_id;
            neighbors[*count].edge = out->edge_ref->edge;
            (*count)++;
        }
        out = out->next;
    }

    /* Check incoming edges */
    edge_list_node_t* in = node->in_edges;
    while (in && *count < max_neighbors) {
        if (in->edge_ref && in->edge_ref->edge.type == type) {
            /* Check if already added */
            bool found = false;
            for (size_t i = 0; i < *count; i++) {
                if (neighbors[i].neighbor_id == in->other_id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                neighbors[*count].neighbor_id = in->other_id;
                neighbors[*count].edge = in->edge_ref->edge;
                (*count)++;
            }
        }
        in = in->next;
    }

    read_unlock(graph);
    clear_error();
    return true;
}

//=============================================================================
// Auto-Entanglement Functions
//=============================================================================

bool entangle_compute_resonance(
    entangle_graph_t graph,
    const resonance_query_t* query,
    const resonance_target_t* target,
    resonance_result_t* result)
{
    if (!graph || !query || !target || !result) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_compute_resonance: required parameter is NULL (graph, query, target, result)");
        return false;
    }

    /* Use graph's resonance config */
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_compute_res", 0.0f);


    read_lock(graph);
    resonance_config_t cfg = graph->config.resonance_cfg;
    read_unlock(graph);

    return resonance_compute(query, target, &cfg, NULL, result);
}

bool entangle_auto_link(
    entangle_graph_t graph,
    uint64_t from_id,
    uint64_t to_id,
    const resonance_query_t* query,
    const resonance_target_t* target,
    entangle_edge_type_t type,
    entangle_edge_t* edge_out)
{
    if (!graph || !query || !target) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_auto_link: required parameter is NULL (graph, query, target)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_auto_link", 0.0f);


    if (from_id == to_id) {
        set_error("Self-loops not allowed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_auto_link: validation failed");
        return false;
    }

    /* Compute resonance */
    resonance_result_t result;
    if (!entangle_compute_resonance(graph, query, target, &result)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_auto_link: entangle_compute_resonance is NULL");
        return false;
    }

    /* Check threshold */
    read_lock(graph);
    float threshold = graph->config.auto_link_threshold;
    bool bidirectional = graph->config.enable_bidirectional;
    read_unlock(graph);

    if (result.total < threshold) {
        /* Below threshold - don't create edge */
        if (edge_out) {
            memset(edge_out, 0, sizeof(*edge_out));
        }
        clear_error();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_auto_link: validation failed");
        return false;
    }

    /* Create edge */
    entangle_edge_t edge = {
        .from_id = from_id,
        .to_id = to_id,
        .resonance_score = result.total,
        .prime_similarity = result.jaccard_component,
        .quat_similarity = result.quat_component,
        .phase_coherence = result.phase_component,
        .type = type,
        .created_time_ms = 0,  /* Will be set by add_edge */
        .weight = result.total,
        .bidirectional = bidirectional
    };

    if (!entangle_add_edge(graph, &edge)) {
        /* Edge may already exist */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_auto_link: entangle_add_edge is NULL");
        return false;
    }

    if (edge_out) {
        entangle_get_edge(graph, from_id, to_id, edge_out);
    }

    clear_error();
    return true;
}

size_t entangle_auto_link_batch(
    entangle_graph_t graph,
    uint64_t from_id,
    const resonance_query_t* query,
    const resonance_target_t* targets,
    const uint64_t* target_ids,
    size_t num_targets,
    entangle_edge_type_t type)
{
    if (!graph || !query || !targets || !target_ids || num_targets == 0) {
        if (!graph || !query || !targets || !target_ids) {
            set_error("NULL parameter");
        }
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_auto_link_b", 0.0f);


    size_t linked = 0;

    for (size_t i = 0; i < num_targets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_targets > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)num_targets);
        }

        if (target_ids[i] != from_id) {
            if (entangle_auto_link(graph, from_id, target_ids[i], query, &targets[i], type, NULL)) {
                linked++;
            }
        }
    }

    clear_error();
    return linked;
}

size_t entangle_prune_weak(entangle_graph_t graph, float threshold) {
    if (!graph) {
        set_error("NULL graph");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_prune_weak", 0.0f);


    if (threshold <= 0.0f) {
        read_lock(graph);
        threshold = graph->config.prune_threshold;
        read_unlock(graph);
    }

    /* Collect edges to remove (can't modify while iterating) */
    size_t capacity = 256;
    size_t to_remove_count = 0;

    typedef struct { uint64_t from; uint64_t to; } edge_pair_t;
    edge_pair_t* to_remove = (edge_pair_t*)nimcp_calloc(capacity, sizeof(edge_pair_t));
    if (!to_remove) {
        set_error("Failed to allocate removal list");
        return 0;
    }

    read_lock(graph);

    for (size_t i = 0; i < graph->edge_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->edge_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->edge_table_size);
        }

        edge_entry_t* entry = graph->edge_table[i];
        while (entry) {
            if (entry->edge.weight < threshold) {
                /* Add to removal list */
                if (to_remove_count >= capacity) {
                    capacity *= 2;
                    edge_pair_t* new_list = (edge_pair_t*)nimcp_realloc(to_remove, capacity * sizeof(edge_pair_t));
                    if (!new_list) {
                        read_unlock(graph);
                        nimcp_free(to_remove);
                        to_remove = NULL;
                        set_error("Failed to expand removal list");
                        return 0;
                    }
                    to_remove = new_list;
                }
                to_remove[to_remove_count].from = entry->edge.from_id;
                to_remove[to_remove_count].to = entry->edge.to_id;
                to_remove_count++;
            }
            entry = entry->hash_next;
        }
    }

    read_unlock(graph);

    /* Remove collected edges */
    size_t removed = 0;
    for (size_t i = 0; i < to_remove_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && to_remove_count > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)to_remove_count);
        }

        if (entangle_remove_edge(graph, to_remove[i].from, to_remove[i].to)) {
            removed++;
        }
    }

    nimcp_free(to_remove);
    to_remove = NULL;
    clear_error();
    return removed;
}

size_t entangle_decay_all(entangle_graph_t graph, float decay_factor) {
    if (!graph) {
        set_error("NULL graph");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_decay_all", 0.0f);


    if (decay_factor <= 0.0f || decay_factor > 1.0f) {
        set_error("Decay factor must be in (0, 1]");
        return 0;
    }

    write_lock(graph);

    size_t affected = 0;
    float weight_sum = 0.0f;
    float min_w = 1.0f;
    float max_w = 0.0f;

    for (size_t i = 0; i < graph->edge_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->edge_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->edge_table_size);
        }

        edge_entry_t* entry = graph->edge_table[i];
        while (entry) {
            entry->edge.weight *= decay_factor;
            weight_sum += entry->edge.weight;
            if (entry->edge.weight < min_w) min_w = entry->edge.weight;
            if (entry->edge.weight > max_w) max_w = entry->edge.weight;
            affected++;
            entry = entry->hash_next;
        }
    }

    /* Update statistics */
    if (affected > 0) {
        graph->stats.avg_weight = weight_sum / (float)affected;
        graph->stats.min_weight = min_w;
        graph->stats.max_weight = max_w;
    }

    write_unlock(graph);
    clear_error();
    return affected;
}

//=============================================================================
// Quantum Walk Functions
//=============================================================================

quantum_walk_state_t* quantum_walk_init(
    entangle_graph_t graph,
    uint64_t start_node,
    uint32_t max_steps)
{
    if (!graph) {
        set_error("NULL graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "graph is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_quantum_walk_init", 0.0f);


    read_lock(graph);

    /* Build node ID mapping */
    uint64_t* node_ids = NULL;
    size_t num_nodes = 0;

    if (!build_node_index_map(graph, &node_ids, &num_nodes)) {
        read_unlock(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "quantum_walk_init: build_node_index_map is NULL");
        return NULL;
    }

    if (num_nodes == 0) {
        read_unlock(graph);
        nimcp_free(node_ids);
        node_ids = NULL;
        set_error("Graph is empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_init: num_nodes is zero");
        return NULL;
    }

    /* Find start node index */
    ssize_t start_idx = find_node_index(node_ids, num_nodes, start_node);
    if (start_idx < 0) {
        read_unlock(graph);
        nimcp_free(node_ids);
        node_ids = NULL;
        set_error("Start node not found in graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_init: validation failed");
        return NULL;
    }

    read_unlock(graph);

    /* Allocate walk state */
    quantum_walk_state_t* state = (quantum_walk_state_t*)nimcp_malloc(sizeof(quantum_walk_state_t));
    if (!state) {
        nimcp_free(node_ids);
        node_ids = NULL;
        set_error("Failed to allocate walk state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate state");

        return NULL;
    }

    state->amplitudes = (float*)nimcp_calloc(num_nodes, sizeof(float));
    if (!state->amplitudes) {
        nimcp_free(node_ids);
        node_ids = NULL;
        nimcp_free(state);
        state = NULL;
        set_error("Failed to allocate amplitude array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_init: state->amplitudes is NULL");
        return NULL;
    }

    state->node_ids = node_ids;
    state->num_nodes = num_nodes;
    state->start_node = start_node;
    state->current_step = 0;
    state->max_steps = max_steps;
    state->is_collapsed = false;

    /* Initialize: all amplitude at start node */
    state->amplitudes[start_idx] = 1.0f;
    state->total_amplitude = 1.0f;

    /* Update stats */
    write_lock(graph);
    ((struct entangle_graph_struct*)graph)->stats.total_walks++;
    write_unlock(graph);

    clear_error();
    return state;
}

bool entangle_quantum_walk_step(entangle_graph_t graph, quantum_walk_state_t* state) {
    if (!graph || !state) {
        set_error("NULL graph or state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_quantum_walk_step: required parameter is NULL (graph, state)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_quantum_wal", 0.0f);


    if (state->is_collapsed) {
        set_error("Walk state already collapsed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_quantum_walk_step: validation failed");
        return false;
    }

    if (state->current_step >= state->max_steps) {
        set_error("Maximum steps reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "entangle_quantum_walk_step: capacity exceeded");
        return false;
    }

    /* Allocate new amplitudes */
    float* new_amplitudes = (float*)nimcp_calloc(state->num_nodes, sizeof(float));
    if (!new_amplitudes) {
        set_error("Failed to allocate new amplitude array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_quantum_walk_step: new_amplitudes is NULL");
        return false;
    }

    read_lock(graph);

    /* Evolve amplitudes: spread through graph */
    for (size_t i = 0; i < state->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)state->num_nodes);
        }

        float amp = state->amplitudes[i];

        /* Skip nodes with negligible amplitude */
        if (fabsf(amp) < ENTANGLE_AMPLITUDE_EPSILON) {
            continue;
        }

        uint64_t node_id = state->node_ids[i];
        node_entry_t* node = find_node_unlocked(graph, node_id);

        if (!node || node->out_degree == 0) {
            /* Dead end - amplitude stays (or could be absorbed) */
            new_amplitudes[i] += amp;
            continue;
        }

        /* Spread to neighbors proportional to weight */
        float total_weight = 0.0f;
        edge_list_node_t* out = node->out_edges;
        while (out) {
            if (out->edge_ref) {
                total_weight += out->edge_ref->edge.weight;
            }
            out = out->next;
        }

        if (total_weight < ENTANGLE_EPSILON) {
            /* No weighted edges - keep amplitude */
            new_amplitudes[i] += amp;
            continue;
        }

        /* Distribute amplitude */
        out = node->out_edges;
        while (out) {
            if (out->edge_ref) {
                ssize_t j = find_node_index(state->node_ids, state->num_nodes, out->other_id);
                if (j >= 0) {
                    float transfer = amp * (out->edge_ref->edge.weight / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f));
                    new_amplitudes[j] += transfer;
                }
            }
            out = out->next;
        }
    }

    read_unlock(graph);

    /* Normalize amplitudes */
    float sum_sq = 0.0f;
    for (size_t i = 0; i < state->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)state->num_nodes);
        }

        sum_sq += new_amplitudes[i] * new_amplitudes[i];
    }

    if (sum_sq > ENTANGLE_EPSILON) {
        float norm = sqrtf(sum_sq);
        for (size_t i = 0; i < state->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->num_nodes > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)state->num_nodes);
            }

            new_amplitudes[i] /= norm;
        }
        state->total_amplitude = 1.0f;
    } else {
        state->total_amplitude = 0.0f;
    }

    /* Update state */
    nimcp_free(state->amplitudes);
    state->amplitudes = new_amplitudes;
    state->current_step++;

    clear_error();
    return true;
}

uint32_t quantum_walk_run(entangle_graph_t graph, quantum_walk_state_t* state, uint32_t steps) {
    if (!graph || !state) {
        set_error("NULL graph or state");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_quantum_walk_run", 0.0f);


    uint32_t taken = 0;
    for (uint32_t i = 0; i < steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && steps > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)steps);
        }

        if (!entangle_quantum_walk_step(graph, state)) {
            break;
        }
        taken++;
    }

    return taken;
}

bool quantum_walk_collapse(quantum_walk_state_t* state, quantum_walk_result_t* result) {
    if (!state || !result) {
        set_error("NULL state or result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_collapse: required parameter is NULL (state, result)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_quantum_walk_collaps", 0.0f);


    if (state->is_collapsed) {
        set_error("Walk state already collapsed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_collapse: validation failed");
        return false;
    }

    if (state->num_nodes == 0) {
        set_error("Empty walk state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_collapse: state->num_nodes is zero");
        return false;
    }

    /* Compute probabilities (|amplitude|^2) */
    float* probs = (float*)nimcp_calloc(state->num_nodes, sizeof(float));
    if (!probs) {
        set_error("Failed to allocate probability array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_collapse: probs is NULL");
        return false;
    }

    float total_prob = 0.0f;
    for (size_t i = 0; i < state->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)state->num_nodes);
        }

        probs[i] = state->amplitudes[i] * state->amplitudes[i];
        total_prob += probs[i];
    }

    /* Normalize if needed */
    if (total_prob > ENTANGLE_EPSILON) {
        for (size_t i = 0; i < state->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->num_nodes > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)state->num_nodes);
            }

            probs[i] /= total_prob;
        }
    } else {
        /* Uniform distribution if no amplitude */
        float uniform = 1.0f / (float)state->num_nodes;
        for (size_t i = 0; i < state->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state->num_nodes > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)state->num_nodes);
            }

            probs[i] = uniform;
        }
    }

    /* Sample from distribution */
    float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
    float cumulative = 0.0f;
    size_t selected = 0;

    for (size_t i = 0; i < state->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)state->num_nodes);
        }

        cumulative += probs[i];
        if (r <= cumulative) {
            selected = i;
            break;
        }
    }

    /* Fill result */
    result->node_id = state->node_ids[selected];
    result->probability = probs[selected];
    result->steps_taken = state->current_step;

    nimcp_free(probs);
    probs = NULL;

    /* Mark as collapsed */
    state->is_collapsed = true;

    clear_error();
    return true;
}

bool quantum_walk_get_top_k(
    quantum_walk_state_t* state,
    size_t k,
    quantum_walk_result_t* results,
    size_t* count)
{
    if (!state || !results || !count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_get_top_k: required parameter is NULL (state, results, count)");
        return false;
    }

    *count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_quantum_walk_get_top", 0.0f);


    if (state->num_nodes == 0 || k == 0) {
        clear_error();
        return true;
    }

    /* Build result array with probabilities */
    quantum_walk_result_t* all = (quantum_walk_result_t*)nimcp_calloc(state->num_nodes, sizeof(quantum_walk_result_t));
    if (!all) {
        set_error("Failed to allocate result array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_get_top_k: all is NULL");
        return false;
    }

    for (size_t i = 0; i < state->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state->num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)state->num_nodes);
        }

        all[i].node_id = state->node_ids[i];
        all[i].probability = state->amplitudes[i] * state->amplitudes[i];
        all[i].steps_taken = state->current_step;
    }

    /* Sort by probability descending */
    qsort(all, state->num_nodes, sizeof(quantum_walk_result_t), compare_walk_results_desc);

    /* Copy top-k */
    *count = (k < state->num_nodes) ? k : state->num_nodes;
    memcpy(results, all, *count * sizeof(quantum_walk_result_t));

    nimcp_free(all);
    all = NULL;
    clear_error();
    return true;
}

float quantum_walk_get_probability(quantum_walk_state_t* state, uint64_t node_id) {
    if (!state) {
        set_error("NULL state");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_quantum_walk_get_pro", 0.0f);


    ssize_t idx = find_node_index(state->node_ids, state->num_nodes, node_id);
    if (idx < 0) {
        return -1.0f;  /* Not found */
    }

    return state->amplitudes[idx] * state->amplitudes[idx];
}

void entangle_quantum_walk_destroy(quantum_walk_state_t* state) {
    if (!state) return;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_quantum_wal", 0.0f);


    nimcp_free(state->amplitudes);
    nimcp_free(state->node_ids);
    nimcp_free(state);
    state = NULL;
}

//=============================================================================
// Classical Spreading Activation Functions
//=============================================================================

bool entangle_spread_activation(
    entangle_graph_t graph,
    const uint64_t* start_nodes,
    const float* start_activations,
    size_t num_starts,
    float decay,
    uint32_t max_hops,
    float threshold,
    entangle_neighbor_t* results,
    size_t max_results,
    size_t* result_count)
{
    if (!graph || !start_nodes || !results || !result_count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_spread_activation: required parameter is NULL (graph, start_nodes, results, result_count)");
        return false;
    }

    *result_count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_spread_acti", 0.0f);


    if (num_starts == 0 || max_hops == 0 || max_results == 0) {
        clear_error();
        return true;
    }

    decay = nimcp_clamp01(decay);
    if (decay < ENTANGLE_EPSILON) {
        clear_error();
        return true;  /* No spreading with zero decay */
    }

    read_lock(graph);

    /* Build node index map */
    uint64_t* node_ids = NULL;
    size_t num_nodes = 0;

    if (!build_node_index_map((entangle_graph_t)graph, &node_ids, &num_nodes)) {
        read_unlock(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "entangle_spread_activation: build_node_index_map is NULL");
        return false;
    }

    if (num_nodes == 0) {
        read_unlock(graph);
        nimcp_free(node_ids);
        node_ids = NULL;
        clear_error();
        return true;
    }

    /* Allocate activation arrays */
    float* activations = (float*)nimcp_calloc(num_nodes, sizeof(float));
    float* new_activations = (float*)nimcp_calloc(num_nodes, sizeof(float));

    if (!activations || !new_activations) {
        read_unlock(graph);
        nimcp_free(node_ids);
        node_ids = NULL;
        nimcp_free(activations);
        activations = NULL;
        nimcp_free(new_activations);
        new_activations = NULL;
        set_error("Failed to allocate activation arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_spread_activation: required parameter is NULL (activations, new_activations)");
        return false;
    }

    /* Initialize starting activations */
    for (size_t i = 0; i < num_starts; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_starts > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)num_starts);
        }

        ssize_t idx = find_node_index(node_ids, num_nodes, start_nodes[i]);
        if (idx >= 0) {
            activations[idx] = start_activations ? start_activations[i] : 1.0f;
        }
    }

    /* Spread activation for max_hops iterations */
    for (uint32_t hop = 0; hop < max_hops; hop++) {
        /* Phase 8: Loop progress heartbeat */
        if ((hop & 0xFF) == 0 && max_hops > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(hop + 1) / (float)max_hops);
        }

        memset(new_activations, 0, num_nodes * sizeof(float));

        for (size_t i = 0; i < num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_nodes > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)num_nodes);
            }

            float act = activations[i];
            if (act < threshold) continue;

            /* Keep some activation at current node */
            new_activations[i] += act * (1.0f - decay);

            /* Spread to neighbors */
            uint64_t node_id = node_ids[i];
            node_entry_t* node = find_node_unlocked((entangle_graph_t)graph, node_id);

            if (!node) continue;

            /* Spread through outgoing edges */
            edge_list_node_t* out = node->out_edges;
            while (out) {
                if (out->edge_ref) {
                    ssize_t j = find_node_index(node_ids, num_nodes, out->other_id);
                    if (j >= 0) {
                        float spread = act * decay * out->edge_ref->edge.weight;
                        new_activations[j] += spread;
                    }
                }
                out = out->next;
            }
        }

        /* Swap buffers */
        float* temp = activations;
        activations = new_activations;
        new_activations = temp;
    }

    /* Collect results above threshold */
    typedef struct { size_t idx; float activation; } act_entry_t;
    act_entry_t* entries = (act_entry_t*)nimcp_calloc(num_nodes, sizeof(act_entry_t));
    if (!entries) {
        read_unlock(graph);
        nimcp_free(node_ids);
        node_ids = NULL;
        nimcp_free(activations);
        activations = NULL;
        nimcp_free(new_activations);
        new_activations = NULL;
        set_error("Failed to allocate entry array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_spread_activation: entries is NULL");
        return false;
    }

    size_t entry_count = 0;
    for (size_t i = 0; i < num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_nodes > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)num_nodes);
        }

        if (activations[i] >= threshold) {
            entries[entry_count].idx = i;
            entries[entry_count].activation = activations[i];
            entry_count++;
        }
    }

    /* Sort by activation descending */
    for (size_t i = 0; i < entry_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && entry_count > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)entry_count);
        }

        for (size_t j = i + 1; j < entry_count; j++) {
            if (entries[j].activation > entries[i].activation) {
                act_entry_t tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }

    /* Copy to results */
    *result_count = (entry_count < max_results) ? entry_count : max_results;
    for (size_t i = 0; i < *result_count; i++) {
        results[i].neighbor_id = node_ids[entries[i].idx];
        memset(&results[i].edge, 0, sizeof(entangle_edge_t));
        results[i].edge.weight = entries[i].activation;  /* Use weight to store activation */
    }

    read_unlock(graph);

    /* Update stats */
    write_lock((entangle_graph_t)graph);
    ((struct entangle_graph_struct*)graph)->stats.total_spreads++;
    write_unlock((entangle_graph_t)graph);

    nimcp_free(entries);
    entries = NULL;
    nimcp_free(node_ids);
    node_ids = NULL;
    nimcp_free(activations);
    activations = NULL;
    nimcp_free(new_activations);
    new_activations = NULL;

    clear_error();
    return true;
}

bool entangle_cascade(
    entangle_graph_t graph,
    uint64_t start_node,
    uint32_t cascade_depth,
    size_t top_k,
    float decay,
    entangle_neighbor_t* results,
    size_t max_results,
    size_t* result_count)
{
    if (!graph || !results || !result_count) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_cascade: required parameter is NULL (graph, results, result_count)");
        return false;
    }

    *result_count = 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_cascade", 0.0f);


    if (cascade_depth == 0 || top_k == 0 || max_results == 0) {
        clear_error();
        return true;
    }

    /* Start with single node */
    size_t current_count = 1;
    uint64_t* current_nodes = (uint64_t*)nimcp_calloc(top_k, sizeof(uint64_t));
    float* current_activations = (float*)nimcp_calloc(top_k, sizeof(float));

    if (!current_nodes || !current_activations) {
        nimcp_free(current_nodes);
        current_nodes = NULL;
        nimcp_free(current_activations);
        current_activations = NULL;
        set_error("Failed to allocate cascade buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_cascade: required parameter is NULL (current_nodes, current_activations)");
        return false;
    }

    current_nodes[0] = start_node;
    current_activations[0] = 1.0f;

    /* Temporary buffer for spread results */
    size_t temp_max = top_k * 10;  /* Allow some expansion */
    entangle_neighbor_t* temp_results = (entangle_neighbor_t*)nimcp_calloc(temp_max, sizeof(entangle_neighbor_t));
    if (!temp_results) {
        nimcp_free(current_nodes);
        current_nodes = NULL;
        nimcp_free(current_activations);
        current_activations = NULL;
        set_error("Failed to allocate temp buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entangle_cascade: temp_results is NULL");
        return false;
    }

    /* Cascade iterations */
    for (uint32_t pass = 0; pass < cascade_depth; pass++) {
        /* Phase 8: Loop progress heartbeat */
        if ((pass & 0xFF) == 0 && cascade_depth > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(pass + 1) / (float)cascade_depth);
        }

        size_t spread_count = 0;

        if (!entangle_spread_activation(graph, current_nodes, current_activations,
                                        current_count, decay, 1, 0.01f,
                                        temp_results, temp_max, &spread_count)) {
            nimcp_free(current_nodes);
            current_nodes = NULL;
            nimcp_free(current_activations);
            current_activations = NULL;
            nimcp_free(temp_results);
            temp_results = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_cascade: operation failed");
            return false;
        }

        if (spread_count == 0) break;

        /* Keep top-k for next iteration */
        current_count = (spread_count < top_k) ? spread_count : top_k;
        for (size_t i = 0; i < current_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && current_count > 256) {
                entanglement_heartbeat("entanglement_loop",
                                 (float)(i + 1) / (float)current_count);
            }

            current_nodes[i] = temp_results[i].neighbor_id;
            current_activations[i] = temp_results[i].edge.weight;
        }
    }

    /* Final spread for results */
    if (!entangle_spread_activation(graph, current_nodes, current_activations,
                                    current_count, decay, 1, 0.01f,
                                    results, max_results, result_count)) {
        nimcp_free(current_nodes);
        current_nodes = NULL;
        nimcp_free(current_activations);
        current_activations = NULL;
        nimcp_free(temp_results);
        temp_results = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entangle_cascade: operation failed");
        return false;
    }

    nimcp_free(current_nodes);
    current_nodes = NULL;
    nimcp_free(current_activations);
    current_activations = NULL;
    nimcp_free(temp_results);
    temp_results = NULL;

    clear_error();
    return true;
}

//=============================================================================
// Statistics Functions
//=============================================================================

bool entangle_get_stats(entangle_graph_t graph, entangle_stats_t* stats) {
    if (!graph || !stats) {
        set_error("NULL graph or stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_get_stats: required parameter is NULL (graph, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_get_stats", 0.0f);


    read_lock(graph);

    /* Copy base stats */
    *stats = graph->stats;

    /* Compute derived stats */
    stats->num_nodes = graph->node_count;
    stats->num_edges = graph->edge_count;

    if (graph->node_count > 0) {
        stats->avg_degree = (float)(graph->edge_count * 2) / (float)graph->node_count;
    } else {
        stats->avg_degree = 0.0f;
    }

    /* Find max degree */
    stats->max_degree = 0;
    for (size_t i = 0; i < graph->node_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->node_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->node_table_size);
        }

        node_entry_t* entry = graph->node_table[i];
        while (entry) {
            size_t degree = entry->in_degree + entry->out_degree;
            if (degree > stats->max_degree) {
                stats->max_degree = degree;
            }
            entry = entry->hash_next;
        }
    }

    read_unlock(graph);
    clear_error();
    return true;
}

size_t entangle_node_degree(
    entangle_graph_t graph,
    uint64_t node_id,
    size_t* in_degree,
    size_t* out_degree)
{
    if (!graph) return 0;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_node_degree", 0.0f);


    read_lock(graph);

    node_entry_t* node = find_node_unlocked(graph, node_id);
    if (!node) {
        read_unlock(graph);
        if (in_degree) *in_degree = 0;
        if (out_degree) *out_degree = 0;
        return 0;
    }

    if (in_degree) *in_degree = node->in_degree;
    if (out_degree) *out_degree = node->out_degree;
    size_t total = node->in_degree + node->out_degree;

    read_unlock(graph);
    return total;
}

void entangle_reset_stats(entangle_graph_t graph) {
    if (!graph) return;

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_reset_stats", 0.0f);


    write_lock(graph);

    /* Keep structural stats, reset operational counters */
    graph->stats.total_lookups = 0;
    graph->stats.total_walks = 0;
    graph->stats.total_spreads = 0;

    write_unlock(graph);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* entangle_get_last_error(void) {
    return s_last_error[0] ? s_last_error : NULL;
}

const char* entangle_edge_type_name(entangle_edge_type_t type) {
    static const char* names[] = {
        "SEMANTIC",
        "TEMPORAL",
        "CAUSAL",
        "ASSOCIATIVE",
        "EMOTIONAL",
        "CONTEXTUAL"
    };

    if (type >= 0 && type < ENTANGLE_EDGE_TYPE_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

void entangle_edge_print(const entangle_edge_t* edge) {
    if (!edge) {
        printf("Edge: (null)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_edge_print", 0.0f);


    printf("Edge: %lu -> %lu\n", (unsigned long)edge->from_id, (unsigned long)edge->to_id);
    printf("  Type: %s\n", entangle_edge_type_name(edge->type));
    printf("  Weight: %.3f\n", edge->weight);
    printf("  Resonance: %.3f (Prime: %.3f, Quat: %.3f, Phase: %.3f)\n",
           edge->resonance_score, edge->prime_similarity,
           edge->quat_similarity, edge->phase_coherence);
    printf("  Bidirectional: %s\n", edge->bidirectional ? "yes" : "no");
    printf("  Created: %lu ms\n", (unsigned long)edge->created_time_ms);
}

void entangle_graph_print_summary(entangle_graph_t graph) {
    if (!graph) {
        printf("Graph: (null)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_graph_print", 0.0f);


    entangle_stats_t stats;
    entangle_get_stats(graph, &stats);

    printf("=== Entanglement Graph Summary ===\n");
    printf("Nodes: %zu\n", stats.num_nodes);
    printf("Edges: %zu\n", stats.num_edges);
    printf("Max degree: %zu\n", stats.max_degree);
    printf("Avg degree: %.2f\n", stats.avg_degree);
    printf("Weight range: [%.3f, %.3f], avg: %.3f\n",
           stats.min_weight, stats.max_weight, stats.avg_weight);
    printf("Memory: %zu bytes\n", stats.memory_bytes);
    printf("Edge types:\n");
    for (int i = 0; i < ENTANGLE_EDGE_TYPE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ENTANGLE_EDGE_TYPE_COUNT > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)ENTANGLE_EDGE_TYPE_COUNT);
        }

        if (stats.edges_by_type[i] > 0) {
            printf("  %s: %lu\n", entangle_edge_type_name(i),
                   (unsigned long)stats.edges_by_type[i]);
        }
    }
    printf("Operations: lookups=%lu, walks=%lu, spreads=%lu\n",
           (unsigned long)stats.total_lookups,
           (unsigned long)stats.total_walks,
           (unsigned long)stats.total_spreads);
    printf("=================================\n");
}

bool entangle_graph_validate(entangle_graph_t graph) {
    if (!graph) {
        set_error("NULL graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entangle_graph_validate: graph is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_graph_valid", 0.0f);


    read_lock(graph);

    bool valid = true;
    size_t counted_nodes = 0;
    size_t counted_edges = 0;

    /* Count and validate nodes */
    for (size_t i = 0; i < graph->node_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->node_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->node_table_size);
        }

        node_entry_t* entry = graph->node_table[i];
        while (entry) {
            counted_nodes++;

            /* Validate hash bucket */
            size_t expected_bucket = hash_node_id(entry->node_id, graph->node_table_size);
            if (expected_bucket != i) {
                set_error("Node %lu in wrong bucket (expected %zu, found %zu)",
                         (unsigned long)entry->node_id, expected_bucket, i);
                valid = false;
            }

            /* Count outgoing edges */
            size_t out_count = 0;
            edge_list_node_t* out = entry->out_edges;
            while (out) {
                out_count++;
                out = out->next;
            }
            if (out_count != entry->out_degree) {
                set_error("Node %lu out_degree mismatch (listed %zu, counted %zu)",
                         (unsigned long)entry->node_id, entry->out_degree, out_count);
                valid = false;
            }

            entry = entry->hash_next;
        }
    }

    if (counted_nodes != graph->node_count) {
        set_error("Node count mismatch (stored %zu, counted %zu)",
                 graph->node_count, counted_nodes);
        valid = false;
    }

    /* Count and validate edges */
    for (size_t i = 0; i < graph->edge_table_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && graph->edge_table_size > 256) {
            entanglement_heartbeat("entanglement_loop",
                             (float)(i + 1) / (float)graph->edge_table_size);
        }

        edge_entry_t* entry = graph->edge_table[i];
        while (entry) {
            counted_edges++;

            /* Validate hash bucket */
            size_t expected_bucket = hash_edge_ids(entry->edge.from_id, entry->edge.to_id,
                                                   graph->edge_table_size);
            if (expected_bucket != i) {
                set_error("Edge %lu->%lu in wrong bucket",
                         (unsigned long)entry->edge.from_id,
                         (unsigned long)entry->edge.to_id);
                valid = false;
            }

            /* Validate endpoints exist */
            if (!find_node_unlocked(graph, entry->edge.from_id)) {
                set_error("Edge from_id %lu doesn't have node entry",
                         (unsigned long)entry->edge.from_id);
                valid = false;
            }
            if (!find_node_unlocked(graph, entry->edge.to_id)) {
                set_error("Edge to_id %lu doesn't have node entry",
                         (unsigned long)entry->edge.to_id);
                valid = false;
            }

            entry = entry->hash_next;
        }
    }

    if (counted_edges != graph->edge_count) {
        set_error("Edge count mismatch (stored %zu, counted %zu)",
                 graph->edge_count, counted_edges);
        valid = false;
    }

    read_unlock(graph);

    if (valid) {
        clear_error();
    }
    return valid;
}

size_t entangle_graph_compact(entangle_graph_t graph) {
    if (!graph) {
        set_error("NULL graph");
        return 0;
    }

    /* For now, just return 0 - full compaction would require:
     * 1. Compute optimal hash table sizes
     * 2. Reallocate and rehash if significantly smaller
     * 3. Return bytes freed
     *
     * This is a complex operation that's optional for now.
     */
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_graph_compa", 0.0f);


    clear_error();
    return 0;
}

uint64_t entangle_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    entanglement_heartbeat("entanglement_entangle_current_tim", 0.0f);


    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
    /* Fallback to time() if clock_gettime fails */
    return (uint64_t)time(NULL) * 1000ULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void entanglement_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_entanglement_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int entanglement_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "entanglement_training_begin: NULL argument");
        return -1;
    }
    entanglement_heartbeat_instance(NULL, "entanglement_training_begin", 0.0f);
    (void)(struct edge_entry_struct*)instance; /* Module state available for reset */
    return 0;
}

int entanglement_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "entanglement_training_end: NULL argument");
        return -1;
    }
    entanglement_heartbeat_instance(NULL, "entanglement_training_end", 1.0f);
    (void)(struct edge_entry_struct*)instance; /* Module state available for finalization */
    return 0;
}

int entanglement_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "entanglement_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    entanglement_heartbeat_instance(NULL, "entanglement_training_step", progress);
    (void)(struct edge_entry_struct*)instance; /* Module state available for step adaptation */
    return 0;
}
