/**
 * @file nimcp_brain_kg.h
 * @brief Internal Runtime Knowledge Graph for Brain Self-Awareness
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: In-memory knowledge graph with full CRUD for real-time module mapping
 * WHY:  Brain needs dynamic self-awareness of its own module structure and connections
 * HOW:  Graph data structure with nodes (modules) and edges (connections), accessible via API
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BRAIN INTERNAL KNOWLEDGE GRAPH                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                          NODES (Modules)                             │  ║
 * ║   │                                                                      │  ║
 * ║   │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐   │  ║
 * ║   │  │Prefrontal│  │Hippocam.│  │ Basal   │  │Cerebellum│  │ Immune  │   │  ║
 * ║   │  │ Cortex  │  │         │  │ Ganglia │  │         │  │ System  │   │  ║
 * ║   │  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘   │  ║
 * ║   │       │            │            │            │            │         │  ║
 * ║   └───────┼────────────┼────────────┼────────────┼────────────┼─────────┘  ║
 * ║           │            │            │            │            │            ║
 * ║   ┌───────┴────────────┴────────────┴────────────┴────────────┴─────────┐  ║
 * ║   │                         EDGES (Connections)                          │  ║
 * ║   │                                                                      │  ║
 * ║   │  connects_to, integrates_with, receives_from, sends_to,             │  ║
 * ║   │  modulates, inhibits, excites, coordinates_with                     │  ║
 * ║   └──────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌──────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                        CRUD OPERATIONS                                │  ║
 * ║   │                                                                       │  ║
 * ║   │  CREATE: brain_kg_add_node(), brain_kg_add_edge()                    │  ║
 * ║   │  READ:   brain_kg_get_node(), brain_kg_get_connections()             │  ║
 * ║   │  UPDATE: brain_kg_update_node(), brain_kg_update_edge()              │  ║
 * ║   │  DELETE: brain_kg_remove_node(), brain_kg_remove_edge()              │  ║
 * ║   └──────────────────────────────────────────────────────────────────────┘  ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * USAGE:
 * ```c
 * // Create KG
 * brain_kg_t* kg = brain_kg_create(NULL);
 *
 * // Add nodes (modules)
 * brain_kg_node_id_t prefrontal = brain_kg_add_node(kg, "prefrontal_cortex",
 *     BRAIN_KG_NODE_CORTICAL, "Executive control, working memory, planning");
 * brain_kg_node_id_t hippocampus = brain_kg_add_node(kg, "hippocampus",
 *     BRAIN_KG_NODE_SUBCORTICAL, "Episodic memory, spatial navigation");
 *
 * // Add edges (connections)
 * brain_kg_add_edge(kg, prefrontal, hippocampus, BRAIN_KG_EDGE_CONNECTS_TO,
 *     "Memory retrieval pathway", 0.8f);
 *
 * // Query connections
 * brain_kg_edge_list_t* edges = brain_kg_get_outgoing(kg, prefrontal);
 *
 * // Traverse paths
 * brain_kg_path_t* path = brain_kg_find_path(kg, prefrontal, hippocampus);
 *
 * brain_kg_destroy(kg);
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe via internal mutex
 */

#ifndef NIMCP_BRAIN_KG_H
#define NIMCP_BRAIN_KG_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum nodes in the internal KG
 * Sized for the post-retrofit load: ~200 structural roots (regions,
 * networks, cognitive families, safety) plus timestamped runtime event
 * nodes. Original cap of 1024 was set pre-retrofit and would fill in
 * minutes of training, after which every emit silently dropped. */
#define BRAIN_KG_MAX_NODES          16384

/** Maximum edges in the internal KG */
#define BRAIN_KG_MAX_EDGES          65536

/** Maximum length of node name */
#define BRAIN_KG_MAX_NAME_LEN       128

/** Maximum length of node/edge description */
#define BRAIN_KG_MAX_DESC_LEN       512

/** Maximum metadata key-value pairs per node */
#define BRAIN_KG_MAX_METADATA       32

/** Invalid node ID sentinel */
#define BRAIN_KG_INVALID_NODE       UINT32_MAX

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Node type categories (module classification)
 */
typedef enum {
    BRAIN_KG_NODE_CORE = 0,          /**< Core brain infrastructure */
    BRAIN_KG_NODE_CORTICAL,          /**< Cortical regions (PFC, temporal, etc.) */
    BRAIN_KG_NODE_SUBCORTICAL,       /**< Subcortical structures (BG, thalamus, etc.) */
    BRAIN_KG_NODE_BRAINSTEM,         /**< Brainstem (medulla, pons, midbrain) */
    BRAIN_KG_NODE_COGNITIVE,         /**< Cognitive modules (ethics, curiosity, etc.) */
    BRAIN_KG_NODE_PERCEPTION,        /**< Sensory processing (visual, audio, speech) */
    BRAIN_KG_NODE_PLASTICITY,        /**< Plasticity systems (STDP, homeostatic, etc.) */
    BRAIN_KG_NODE_TRAINING,          /**< Training subsystems (NAS, HPO, backprop) */
    BRAIN_KG_NODE_SWARM,             /**< Swarm intelligence modules */
    BRAIN_KG_NODE_SECURITY,          /**< Security modules (BBB, immune, etc.) */
    BRAIN_KG_NODE_INTEGRATION,       /**< Integration/bridge modules */
    BRAIN_KG_NODE_COORDINATOR,       /**< Orchestrators and coordinators */
    BRAIN_KG_NODE_UTILITY,           /**< Utility modules (IO, memory, etc.) */
    BRAIN_KG_NODE_CUSTOM,            /**< User-defined node type */
    BRAIN_KG_NODE_TYPE_COUNT
} brain_kg_node_type_t;

/**
 * @brief Edge type categories (connection semantics)
 */
typedef enum {
    BRAIN_KG_EDGE_CONNECTS_TO = 0,   /**< Generic bidirectional connection */
    BRAIN_KG_EDGE_SENDS_TO,          /**< Unidirectional signal path (A → B) */
    BRAIN_KG_EDGE_RECEIVES_FROM,     /**< Unidirectional receive path (A ← B) */
    BRAIN_KG_EDGE_INTEGRATES_WITH,   /**< Integration/bridge relationship */
    BRAIN_KG_EDGE_MODULATES,         /**< Neuromodulatory influence */
    BRAIN_KG_EDGE_EXCITES,           /**< Excitatory connection */
    BRAIN_KG_EDGE_INHIBITS,          /**< Inhibitory connection */
    BRAIN_KG_EDGE_COORDINATES_WITH,  /**< Coordination/synchronization */
    BRAIN_KG_EDGE_DEPENDS_ON,        /**< Initialization dependency */
    BRAIN_KG_EDGE_PROVIDES_TO,       /**< Service/capability provision */
    BRAIN_KG_EDGE_HANDLES_MESSAGE,   /**< Module to message handler set link */
    BRAIN_KG_EDGE_CUSTOM,            /**< User-defined edge type */
    BRAIN_KG_EDGE_TYPE_COUNT
} brain_kg_edge_type_t;

/**
 * @brief Node state (lifecycle tracking)
 */
typedef enum {
    BRAIN_KG_STATE_UNKNOWN = 0,      /**< State not determined */
    BRAIN_KG_STATE_UNINITIALIZED,    /**< Not yet initialized */
    BRAIN_KG_STATE_INITIALIZING,     /**< Currently initializing */
    BRAIN_KG_STATE_ACTIVE,           /**< Running and operational */
    BRAIN_KG_STATE_DISABLED,         /**< Temporarily disabled */
    BRAIN_KG_STATE_ERROR,            /**< Error state */
    BRAIN_KG_STATE_SHUTDOWN          /**< Shut down */
} brain_kg_node_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/** Node identifier type */
typedef uint32_t brain_kg_node_id_t;

/** Edge identifier type */
typedef uint32_t brain_kg_edge_id_t;

/**
 * @brief Key-value metadata pair
 */
typedef struct {
    char key[64];                    /**< Metadata key */
    char value[256];                 /**< Metadata value */
} brain_kg_metadata_t;

/**
 * @brief Node (module) in the knowledge graph
 */
typedef struct {
    brain_kg_node_id_t id;           /**< Unique node ID */
    char name[BRAIN_KG_MAX_NAME_LEN]; /**< Module name (e.g., "prefrontal_cortex") */
    brain_kg_node_type_t type;       /**< Node type category */
    brain_kg_node_state_t state;     /**< Current lifecycle state */
    char description[BRAIN_KG_MAX_DESC_LEN]; /**< Human-readable description */

    /* Metadata */
    brain_kg_metadata_t metadata[BRAIN_KG_MAX_METADATA];
    uint32_t metadata_count;         /**< Number of metadata entries */

    /* Module pointer (optional, for direct access) */
    void* module_ptr;                /**< Pointer to actual module instance */

    /* Statistics */
    uint64_t created_time;           /**< When node was added (ms) */
    uint64_t last_updated;           /**< Last update timestamp (ms) */
    uint32_t incoming_count;         /**< Number of incoming edges */
    uint32_t outgoing_count;         /**< Number of outgoing edges */

    /* Flags */
    bool enabled;                    /**< Whether node is active */
    bool in_use;                     /**< Whether slot is occupied */
} brain_kg_node_t;

/**
 * @brief Edge (connection) in the knowledge graph
 */
typedef struct {
    brain_kg_edge_id_t id;           /**< Unique edge ID */
    brain_kg_node_id_t from;         /**< Source node ID */
    brain_kg_node_id_t to;           /**< Target node ID */
    brain_kg_edge_type_t type;       /**< Edge type category */
    char description[BRAIN_KG_MAX_DESC_LEN]; /**< Human-readable description */
    float weight;                    /**< Connection strength [0.0-1.0] */
    bool bidirectional;              /**< If true, connection works both ways */
    bool in_use;                     /**< Whether slot is occupied */
    uint64_t created_time;           /**< When edge was added (ms) */
} brain_kg_edge_t;

/**
 * @brief List of nodes (query result)
 */
typedef struct {
    brain_kg_node_t** nodes;         /**< Array of node pointers */
    uint32_t count;                  /**< Number of nodes */
    uint32_t capacity;               /**< Allocated capacity */
} brain_kg_node_list_t;

/**
 * @brief List of edges (query result)
 */
typedef struct {
    brain_kg_edge_t** edges;         /**< Array of edge pointers */
    uint32_t count;                  /**< Number of edges */
    uint32_t capacity;               /**< Allocated capacity */
} brain_kg_edge_list_t;

/**
 * @brief Path through the graph
 */
typedef struct {
    brain_kg_node_id_t* nodes;       /**< Array of node IDs in path */
    uint32_t length;                 /**< Number of nodes in path */
    float total_weight;              /**< Sum of edge weights */
} brain_kg_path_t;

/**
 * @brief Access permission levels for KG operations
 */
typedef enum {
    BRAIN_KG_ACCESS_NONE = 0,        /**< No access */
    BRAIN_KG_ACCESS_READ,            /**< Read-only access */
    BRAIN_KG_ACCESS_WRITE,           /**< Read and write access */
    BRAIN_KG_ACCESS_ADMIN            /**< Full access including security config */
} brain_kg_access_level_t;

/**
 * @brief Security event types for immune integration
 */
typedef enum {
    BRAIN_KG_SEC_UNAUTHORIZED_ACCESS = 0,  /**< Access without proper credentials */
    BRAIN_KG_SEC_INTEGRITY_VIOLATION,      /**< Checksum mismatch detected */
    BRAIN_KG_SEC_EXCESSIVE_MUTATIONS,      /**< Too many modifications in short time */
    BRAIN_KG_SEC_CRITICAL_NODE_MODIFIED,   /**< Core/security node modified */
    BRAIN_KG_SEC_SUSPICIOUS_PATTERN,       /**< Unusual access pattern detected */
    BRAIN_KG_SEC_EVENT_COUNT
} brain_kg_security_event_t;

/**
 * @brief KG configuration
 */
typedef struct {
    uint32_t max_nodes;              /**< Maximum nodes (0 = default) */
    uint32_t max_edges;              /**< Maximum edges (0 = default) */
    bool enable_statistics;          /**< Track usage statistics */
    bool auto_populate;              /**< Auto-populate from brain modules */

    /* Security configuration */
    bool enable_security;            /**< Enable security features */
    bool enable_integrity_checks;    /**< Enable checksum verification */
    bool enable_access_control;      /**< Enable access control */
    bool enable_immune_integration;  /**< Report violations to immune system */
    bool enable_audit_log;           /**< Log all modifications */
    uint32_t max_mutations_per_sec;  /**< Rate limit (0 = unlimited) */
    uint32_t integrity_check_interval_ms; /**< Checksum verification interval */
} brain_kg_config_t;

/**
 * @brief KG statistics
 */
typedef struct {
    uint32_t total_nodes;            /**< Number of nodes */
    uint32_t total_edges;            /**< Number of edges */
    uint32_t nodes_by_type[BRAIN_KG_NODE_TYPE_COUNT];
    uint32_t edges_by_type[BRAIN_KG_EDGE_TYPE_COUNT];
    uint64_t queries_count;          /**< Total queries performed */
    uint64_t modifications_count;    /**< Total modifications */
    uint64_t last_modified;          /**< Last modification timestamp */
} brain_kg_stats_t;

/**
 * @brief Internal knowledge graph handle
 */
typedef struct brain_kg brain_kg_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create internal knowledge graph
 *
 * @param config Configuration (NULL for defaults)
 * @return New KG handle or NULL on error
 */
brain_kg_t* brain_kg_create(const brain_kg_config_t* config);

/**
 * @brief Destroy internal knowledge graph
 *
 * @param kg KG handle (NULL safe)
 */
void brain_kg_destroy(brain_kg_t* kg);

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success
 */
int brain_kg_default_config(brain_kg_config_t* config);

/* ============================================================================
 * NODE CRUD API
 * ============================================================================ */

/**
 * @brief Add a new node (module) to the KG
 *
 * @param kg KG handle
 * @param name Unique node name
 * @param type Node type category
 * @param description Human-readable description
 * @return Node ID or BRAIN_KG_INVALID_NODE on error
 */
brain_kg_node_id_t brain_kg_add_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description
);

/**
 * @brief Get node by ID
 *
 * @param kg KG handle
 * @param id Node ID
 * @return Node pointer or NULL if not found
 */
const brain_kg_node_t* brain_kg_get_node(const brain_kg_t* kg, brain_kg_node_id_t id);

/**
 * @brief Find node by name
 *
 * @param kg KG handle
 * @param name Node name
 * @return Node ID or BRAIN_KG_INVALID_NODE if not found
 */
brain_kg_node_id_t brain_kg_find_node(const brain_kg_t* kg, const char* name);

/**
 * @brief Update node properties
 *
 * @param kg KG handle
 * @param id Node ID
 * @param description New description (NULL to keep current)
 * @param state New state
 * @return 0 on success, -1 on error
 */
int brain_kg_update_node(
    brain_kg_t* kg,
    brain_kg_node_id_t id,
    const char* description,
    brain_kg_node_state_t state
);

/**
 * @brief Set node module pointer
 *
 * @param kg KG handle
 * @param id Node ID
 * @param module_ptr Pointer to module instance
 * @return 0 on success
 */
int brain_kg_set_module_ptr(brain_kg_t* kg, brain_kg_node_id_t id, void* module_ptr);

/**
 * @brief Add metadata to node
 *
 * @param kg KG handle
 * @param id Node ID
 * @param key Metadata key
 * @param value Metadata value
 * @return 0 on success
 */
int brain_kg_add_metadata(
    brain_kg_t* kg,
    brain_kg_node_id_t id,
    const char* key,
    const char* value
);

/**
 * @brief Remove node from KG
 *
 * Also removes all edges connected to the node.
 *
 * @param kg KG handle
 * @param id Node ID
 * @return 0 on success, -1 on error
 */
int brain_kg_remove_node(brain_kg_t* kg, brain_kg_node_id_t id);

/**
 * @brief Get all nodes of a specific type
 *
 * @param kg KG handle
 * @param type Node type to filter
 * @return Node list (caller must free with brain_kg_node_list_destroy)
 */
brain_kg_node_list_t* brain_kg_get_nodes_by_type(const brain_kg_t* kg, brain_kg_node_type_t type);

/**
 * @brief Get all nodes
 *
 * @param kg KG handle
 * @return Node list (caller must free)
 */
brain_kg_node_list_t* brain_kg_get_all_nodes(const brain_kg_t* kg);

/**
 * @brief Free node list
 */
void brain_kg_node_list_destroy(brain_kg_node_list_t* list);

/* ============================================================================
 * EDGE CRUD API
 * ============================================================================ */

/**
 * @brief Add an edge (connection) between nodes
 *
 * @param kg KG handle
 * @param from Source node ID
 * @param to Target node ID
 * @param type Edge type
 * @param description Human-readable description
 * @param weight Connection strength [0.0-1.0]
 * @return Edge ID or BRAIN_KG_INVALID_NODE on error
 */
brain_kg_edge_id_t brain_kg_add_edge(
    brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    brain_kg_edge_type_t type,
    const char* description,
    float weight
);

/**
 * @brief Get edge by ID
 *
 * @param kg KG handle
 * @param id Edge ID
 * @return Edge pointer or NULL if not found
 */
const brain_kg_edge_t* brain_kg_get_edge(const brain_kg_t* kg, brain_kg_edge_id_t id);

/**
 * @brief Find edge between two nodes
 *
 * @param kg KG handle
 * @param from Source node ID
 * @param to Target node ID
 * @return Edge ID or BRAIN_KG_INVALID_NODE if not found
 */
brain_kg_edge_id_t brain_kg_find_edge(
    const brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
);

/**
 * @brief Update edge properties
 *
 * @param kg KG handle
 * @param id Edge ID
 * @param weight New weight (negative to keep current)
 * @param description New description (NULL to keep current)
 * @return 0 on success
 */
int brain_kg_update_edge(
    brain_kg_t* kg,
    brain_kg_edge_id_t id,
    float weight,
    const char* description
);

/**
 * @brief Remove edge from KG
 *
 * @param kg KG handle
 * @param id Edge ID
 * @return 0 on success
 */
int brain_kg_remove_edge(brain_kg_t* kg, brain_kg_edge_id_t id);

/**
 * @brief Get all outgoing edges from a node
 *
 * @param kg KG handle
 * @param node_id Source node ID
 * @return Edge list (caller must free)
 */
brain_kg_edge_list_t* brain_kg_get_outgoing(const brain_kg_t* kg, brain_kg_node_id_t node_id);

/**
 * @brief Get all incoming edges to a node
 *
 * @param kg KG handle
 * @param node_id Target node ID
 * @return Edge list (caller must free)
 */
brain_kg_edge_list_t* brain_kg_get_incoming(const brain_kg_t* kg, brain_kg_node_id_t node_id);

/**
 * @brief Get all edges of a specific type
 *
 * @param kg KG handle
 * @param type Edge type to filter
 * @return Edge list (caller must free)
 */
brain_kg_edge_list_t* brain_kg_get_edges_by_type(const brain_kg_t* kg, brain_kg_edge_type_t type);

/**
 * @brief Free edge list
 */
void brain_kg_edge_list_destroy(brain_kg_edge_list_t* list);

/* ============================================================================
 * GRAPH TRAVERSAL API
 * ============================================================================ */

/**
 * @brief Find path between two nodes
 *
 * Uses BFS/Dijkstra to find shortest path.
 *
 * @param kg KG handle
 * @param from Start node ID
 * @param to End node ID
 * @return Path or NULL if no path exists (caller must free)
 */
brain_kg_path_t* brain_kg_find_path(
    const brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
);

/**
 * @brief Check if two nodes are connected (directly or indirectly)
 *
 * @param kg KG handle
 * @param from Start node ID
 * @param to End node ID
 * @return true if connected
 */
bool brain_kg_are_connected(
    const brain_kg_t* kg,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
);

/**
 * @brief Get all nodes reachable from a given node
 *
 * @param kg KG handle
 * @param start_node Starting node ID
 * @param max_depth Maximum traversal depth (0 = unlimited)
 * @return Node list (caller must free)
 */
brain_kg_node_list_t* brain_kg_get_reachable(
    const brain_kg_t* kg,
    brain_kg_node_id_t start_node,
    uint32_t max_depth
);

/**
 * @brief Free path
 */
void brain_kg_path_destroy(brain_kg_path_t* path);

/* ============================================================================
 * POPULATION API (Auto-populate from brain modules)
 * ============================================================================ */

/**
 * @brief Populate KG from brain module structure
 *
 * Scans the brain_struct and adds nodes for all enabled modules.
 *
 * @param kg KG handle
 * @param brain Brain instance (nimcp_brain_t*)
 * @return Number of nodes added
 */
int brain_kg_populate_from_brain(brain_kg_t* kg, void* brain);

/**
 * @brief Refresh KG state from current brain state
 *
 * Updates node states based on current module states.
 *
 * @param kg KG handle
 * @param brain Brain instance
 * @return 0 on success
 */
int brain_kg_refresh_state(brain_kg_t* kg, void* brain);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Search nodes by name pattern
 *
 * @param kg KG handle
 * @param pattern Search pattern (substring match)
 * @return Node list (caller must free)
 */
brain_kg_node_list_t* brain_kg_search_nodes(const brain_kg_t* kg, const char* pattern);

/**
 * @brief Get nodes connected to a given node
 *
 * Returns all nodes with edges to/from the specified node.
 *
 * @param kg KG handle
 * @param node_id Node ID
 * @return Node list (caller must free)
 */
brain_kg_node_list_t* brain_kg_get_neighbors(const brain_kg_t* kg, brain_kg_node_id_t node_id);

/**
 * @brief Get hub nodes (most connected)
 *
 * @param kg KG handle
 * @param max_count Maximum nodes to return
 * @return Node list sorted by connection count (caller must free)
 */
brain_kg_node_list_t* brain_kg_get_hubs(const brain_kg_t* kg, uint32_t max_count);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get KG statistics
 *
 * @param kg KG handle
 * @param stats Output statistics
 * @return 0 on success
 */
int brain_kg_get_stats(const brain_kg_t* kg, brain_kg_stats_t* stats);

/**
 * @brief Generate summary string
 *
 * @param kg KG handle
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Characters written
 */
int brain_kg_generate_summary(const brain_kg_t* kg, char* buffer, size_t buffer_size);

/* ============================================================================
 * STRING CONVERSION
 * ============================================================================ */

const char* brain_kg_node_type_to_string(brain_kg_node_type_t type);
const char* brain_kg_edge_type_to_string(brain_kg_edge_type_t type);
const char* brain_kg_node_state_to_string(brain_kg_node_state_t state);

/* ============================================================================
 * SECURITY & IMMUNE INTEGRATION API
 * ============================================================================ */

/**
 * @brief Security event callback type for immune integration
 *
 * Called when security violations are detected. The immune system
 * can register this callback to receive threat notifications.
 *
 * @param event_type Type of security event
 * @param node_id Affected node (or BRAIN_KG_INVALID_NODE if N/A)
 * @param details Human-readable description
 * @param user_data User context pointer
 */
typedef void (*brain_kg_security_callback_t)(
    brain_kg_security_event_t event_type,
    brain_kg_node_id_t node_id,
    const char* details,
    void* user_data
);

/**
 * @brief Connect KG to brain immune system
 *
 * Enables automatic threat reporting to the immune system when
 * security violations are detected.
 *
 * @param kg KG handle
 * @param immune_system Brain immune system pointer (brain_immune_system_t*)
 * @return 0 on success
 */
int brain_kg_connect_immune(brain_kg_t* kg, void* immune_system);

/**
 * @brief Disconnect from brain immune system
 *
 * @param kg KG handle
 * @return 0 on success
 */
int brain_kg_disconnect_immune(brain_kg_t* kg);

/**
 * @brief Register security event callback
 *
 * @param kg KG handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int brain_kg_register_security_callback(
    brain_kg_t* kg,
    brain_kg_security_callback_t callback,
    void* user_data
);

/**
 * @brief Set access level for operations
 *
 * Higher levels inherit lower permissions.
 *
 * @param kg KG handle
 * @param level Access level
 * @param token Authentication token (for verification)
 * @return 0 on success, -1 if access denied
 */
int brain_kg_set_access_level(
    brain_kg_t* kg,
    brain_kg_access_level_t level,
    uint64_t token
);

/**
 * @brief Generate authentication token
 *
 * Creates a cryptographic token for access control.
 *
 * @param kg KG handle
 * @param level Requested access level
 * @param token_out Output token
 * @return 0 on success
 */
int brain_kg_generate_token(
    brain_kg_t* kg,
    brain_kg_access_level_t level,
    uint64_t* token_out
);

/**
 * @brief Verify integrity of entire KG
 *
 * Computes and verifies checksums for all nodes and edges.
 *
 * @param kg KG handle
 * @return 0 if integrity verified, -1 if violation detected
 */
int brain_kg_verify_integrity(brain_kg_t* kg);

/**
 * @brief Reset the integrity baseline so the next verify call seals a fresh checksum.
 *
 * Use this after a batch of authorized init-time mutations (e.g., the various
 * *_init_roots() calls during brain init) so a subsequent verify_integrity()
 * doesn't fire a false positive against a baseline captured mid-init.
 *
 * @param kg KG handle (no-op if NULL)
 */
void brain_kg_reset_integrity_baseline(brain_kg_t* kg);

/**
 * @brief Mark node as critical (protected)
 *
 * Critical nodes trigger immune alerts when modified.
 *
 * @param kg KG handle
 * @param node_id Node to protect
 * @return 0 on success
 */
int brain_kg_mark_critical(brain_kg_t* kg, brain_kg_node_id_t node_id);

/**
 * @brief Get security statistics
 *
 * @param kg KG handle
 * @param violations_out Output: total violations detected
 * @param last_violation_time Output: timestamp of last violation
 * @return 0 on success
 */
int brain_kg_get_security_stats(
    const brain_kg_t* kg,
    uint32_t* violations_out,
    uint64_t* last_violation_time
);

/**
 * @brief Lock KG for emergency protection
 *
 * Disables all write operations. Requires ADMIN token to unlock.
 *
 * @param kg KG handle
 * @return 0 on success
 */
int brain_kg_emergency_lock(brain_kg_t* kg);

/**
 * @brief Unlock KG after emergency lock
 *
 * @param kg KG handle
 * @param admin_token Admin authentication token
 * @return 0 on success, -1 if token invalid
 */
int brain_kg_emergency_unlock(brain_kg_t* kg, uint64_t admin_token);

/* ============================================================================
 * MESSAGE-TYPE INDEX API (Phase 6: KG Query Optimization)
 * ============================================================================ */

/**
 * @brief Maximum message types indexed
 */
#define BRAIN_KG_MAX_MESSAGE_TYPES  512

/**
 * @brief Maximum handlers per message type
 */
#define BRAIN_KG_MAX_HANDLERS_PER_MSG  64

/**
 * @brief Handler lookup result
 */
typedef struct {
    brain_kg_node_id_t* handlers;    /**< Array of module node IDs */
    uint32_t count;                  /**< Number of handlers */
    uint32_t capacity;               /**< Allocated capacity */
} brain_kg_handler_list_t;

/**
 * @brief Get all modules that handle a specific message type
 *
 * WHAT: Efficient O(1) lookup of handlers for a message type
 * WHY:  Enables fast KG-driven handler discovery at runtime
 * HOW:  Uses internal message-type index, not full edge scan
 *
 * @param kg KG handle
 * @param message_type Message type identifier (bio_message_type_t value)
 * @return Handler list (caller must free with brain_kg_handler_list_destroy)
 */
brain_kg_handler_list_t* brain_kg_get_handlers_for_message_type(
    const brain_kg_t* kg,
    uint32_t message_type
);

/**
 * @brief Free handler list
 */
void brain_kg_handler_list_destroy(brain_kg_handler_list_t* list);

/**
 * @brief Add message handler mapping to index
 *
 * WHAT: Register that a module handles a specific message type
 * WHY:  Populates message-type index for fast handler lookup
 * HOW:  Adds to internal hash index, also creates HANDLES_MESSAGE edge
 *
 * @param kg KG handle
 * @param module_node_id Module's node ID in the KG
 * @param message_type Message type identifier
 * @return 0 on success, -1 on error
 */
int brain_kg_add_message_handler(
    brain_kg_t* kg,
    brain_kg_node_id_t module_node_id,
    uint32_t message_type
);

/**
 * @brief Remove message handler mapping from index
 *
 * WHAT: Unregister a module's handler for a message type
 * WHY:  Maintains index consistency when handlers are removed
 * HOW:  Removes from internal hash index
 *
 * @param kg KG handle
 * @param module_node_id Module's node ID in the KG
 * @param message_type Message type identifier
 * @return 0 on success, -1 if not found
 */
int brain_kg_remove_message_handler(
    brain_kg_t* kg,
    brain_kg_node_id_t module_node_id,
    uint32_t message_type
);

/**
 * @brief Rebuild message-type index from edges
 *
 * WHAT: Reconstruct the message-type index from HANDLES_MESSAGE edges
 * WHY:  Recovery after index corruption or for lazy initialization
 * HOW:  Scans all edges, rebuilds index
 *
 * @param kg KG handle
 * @return Number of handlers indexed, -1 on error
 */
int brain_kg_rebuild_message_index(brain_kg_t* kg);

/**
 * @brief Get all message types handled by a module
 *
 * WHAT: Reverse lookup - find all messages a module handles
 * WHY:  Enable module introspection of its own handlers
 * HOW:  Scans index for entries containing the module
 *
 * @param kg KG handle
 * @param module_node_id Module's node ID
 * @param message_types Output array (caller allocated)
 * @param max_types Size of output array
 * @return Number of message types found
 */
uint32_t brain_kg_get_module_handled_messages(
    const brain_kg_t* kg,
    brain_kg_node_id_t module_node_id,
    uint32_t* message_types,
    uint32_t max_types
);

/**
 * @brief Invalidate message-type index cache
 *
 * WHAT: Mark index as needing rebuild
 * WHY:  Called when edges are modified outside of add/remove_message_handler
 * HOW:  Sets dirty flag, next query triggers rebuild
 *
 * @param kg KG handle
 */
void brain_kg_invalidate_message_index(brain_kg_t* kg);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_KG_H */
