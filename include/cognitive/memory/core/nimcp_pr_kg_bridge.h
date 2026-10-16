//=============================================================================
// nimcp_pr_kg_bridge.h - KG Bridge for Prime Resonant Memory System
//=============================================================================
/**
 * @file nimcp_pr_kg_bridge.h
 * @brief Integration bridge between PR Memory System and Brain Knowledge Graph
 *
 * WHAT: Bidirectional bridge connecting PR Memory nodes to Brain Knowledge Graph
 * WHY:  Enable self-aware memory system by integrating episodic/semantic memories
 *       with the brain's internal knowledge representation
 * HOW:  Maps PR memory nodes to KG nodes, syncs entanglement edges to KG edges,
 *       and provides query interfaces for KG-aware memory retrieval
 *
 * ARCHITECTURE:
 *
 *   PR Memory <-> Brain KG Bridge:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |  +-----------------+         +------------------+                     |
 *   |  | PR Memory Node  |         | Brain KG Node    |                     |
 *   |  |-----------------|  SYNC   |------------------|                     |
 *   |  | id: 12345       |<------->| id: 42           |                     |
 *   |  | content sig     |         | type: COGNITIVE  |                     |
 *   |  | quaternion      |         | state: ACTIVE    |                     |
 *   |  | phase           |         | metadata         |                     |
 *   |  +-----------------+         +------------------+                     |
 *   |         |                            |                                |
 *   |         | entangles                  | edge                           |
 *   |         v                            v                                |
 *   |  +-----------------+         +------------------+                     |
 *   |  | PR Memory Node  |         | Brain KG Node    |                     |
 *   |  | id: 67890       |<------->| id: 87           |                     |
 *   |  +-----------------+         +------------------+                     |
 *   |                                                                       |
 *   +-----------------------------------------------------------------------+
 *
 *   Bridge Mapping Table:
 *   +-----------------------------------------------------------------------+
 *   |  PR Node ID   |   KG Node ID   |   Sync Time   |   State              |
 *   |---------------|----------------|---------------|----------------------|
 *   |  12345        |   42           |   1704067200  |   SYNCED             |
 *   |  67890        |   87           |   1704067100  |   SYNCED             |
 *   |  99999        |   INVALID      |   0           |   PENDING            |
 *   +-----------------------------------------------------------------------+
 *
 *   Edge Type Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Entanglement Edge Type   |   Brain KG Edge Type                      |
 *   |---------------------------|-------------------------------------------|
 *   |  ENTANGLE_EDGE_SEMANTIC   |   BRAIN_KG_EDGE_CONNECTS_TO               |
 *   |  ENTANGLE_EDGE_CAUSAL     |   BRAIN_KG_EDGE_SENDS_TO                  |
 *   |  ENTANGLE_EDGE_ASSOCIATIVE|   BRAIN_KG_EDGE_MODULATES                 |
 *   |  ENTANGLE_EDGE_EMOTIONAL  |   BRAIN_KG_EDGE_MODULATES                 |
 *   |  ENTANGLE_EDGE_TEMPORAL   |   BRAIN_KG_EDGE_COORDINATES_WITH          |
 *   +-----------------------------------------------------------------------+
 *
 * INTEGRATION FLOW:
 *
 *   Memory Creation -> KG Registration:
 *   +-----------------------------------------------------------------------+
 *   |  1. PR Memory Node created (nimcp_pr_memory_node_create)              |
 *   |  2. If auto_register: pr_kg_register_memory() called automatically    |
 *   |  3. Creates BRAIN_KG_NODE_COGNITIVE node with memory metadata         |
 *   |  4. Stores bidirectional mapping (PR ID <-> KG ID)                    |
 *   |  5. Memory is now visible to brain self-awareness queries             |
 *   +-----------------------------------------------------------------------+
 *
 *   Entanglement -> KG Edge Sync:
 *   +-----------------------------------------------------------------------+
 *   |  1. Entanglement edge created between PR nodes                        |
 *   |  2. If sync_on_update: pr_kg_sync_entanglement() called               |
 *   |  3. Maps entanglement type to KG edge type                            |
 *   |  4. Creates KG edge with weight from resonance strength               |
 *   |  5. Brain can now trace memory relationships via KG API               |
 *   +-----------------------------------------------------------------------+
 *
 *   KG -> Memory Query:
 *   +-----------------------------------------------------------------------+
 *   |  1. Brain queries KG for nodes related to a module                    |
 *   |  2. pr_kg_query_by_module() finds all memories linked to module       |
 *   |  3. Returns PR memory node IDs for further processing                 |
 *   |  4. Enables contextual memory retrieval based on brain state          |
 *   +-----------------------------------------------------------------------+
 *
 * THREAD SAFETY:
 * - Bridge maintains internal mutex for mapping table
 * - Brain KG has its own internal mutex (thread-safe)
 * - Dual-lock order: Always lock bridge first, then KG (if needed)
 *
 * PERFORMANCE:
 * - Registration: ~500ns (KG add_node + mapping update)
 * - Sync: ~200ns (KG update_node)
 * - Query by module: O(N) where N = mappings for that module
 * - Batch operations: ~400ns per memory (amortized)
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_KG_BRIDGE_H
#define NIMCP_PR_KG_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "core/brain/nimcp_brain_kg.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of PR<->KG mappings */
#define PR_KG_MAX_MAPPINGS          65536

/** Maximum description length for KG nodes created from memories */
#define PR_KG_MAX_DESCRIPTION       256

/** Maximum memories returned in a single query */
#define PR_KG_MAX_QUERY_RESULTS     1024

/** Default edge weight scale factor */
#define PR_KG_DEFAULT_WEIGHT_SCALE  1.0f

/** Stale mapping threshold in milliseconds (5 minutes default) */
#define PR_KG_STALE_THRESHOLD_MS    (5 * 60 * 1000)

/** Invalid PR node ID sentinel */
#define PR_KG_INVALID_PR_ID         UINT64_MAX

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * @brief Forward declaration for PR Memory Node (from nimcp_pr_memory_node.h)
 *
 * The PR Memory Node represents an individual memory in the Prime Resonant
 * memory system. It contains content signature, quaternion state, phase,
 * and other memory-specific data.
 */
typedef struct pr_memory_node_struct pr_memory_node_t;

/**
 * @brief Forward declaration for Entanglement Graph (from nimcp_entanglement.h)
 *
 * The Entanglement Graph manages relationships between PR memory nodes,
 * with edges representing semantic, causal, associative, emotional,
 * and temporal connections.
 */
typedef struct entanglement_graph_struct entanglement_graph_t;

/**
 * @brief Forward declaration for Entanglement Edge
 */
typedef struct entanglement_edge_struct entanglement_edge_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Entanglement edge types (from nimcp_entanglement.h)
 *
 * These are the edge types used in the PR memory entanglement graph.
 * We define them here for KG bridge mapping purposes.
 */
typedef enum {
    ENTANGLE_EDGE_SEMANTIC = 0,     /**< Semantic similarity connection */
    ENTANGLE_EDGE_CAUSAL,           /**< Cause-effect relationship */
    ENTANGLE_EDGE_ASSOCIATIVE,      /**< Learned association */
    ENTANGLE_EDGE_EMOTIONAL,        /**< Emotional valence link */
    ENTANGLE_EDGE_TEMPORAL,         /**< Temporal sequence link */
    ENTANGLE_EDGE_CONTEXTUAL,       /**< Context-based link */
    ENTANGLE_EDGE_TYPE_COUNT
} pr_kg_entangle_edge_type_t;

/**
 * @brief Mapping state enumeration
 *
 * Tracks the synchronization state of each PR<->KG mapping.
 */
typedef enum {
    PR_KG_MAPPING_STATE_INVALID = 0, /**< Mapping slot not in use */
    PR_KG_MAPPING_STATE_PENDING,     /**< Registration pending */
    PR_KG_MAPPING_STATE_SYNCED,      /**< Fully synchronized */
    PR_KG_MAPPING_STATE_DIRTY,       /**< PR node changed, needs sync */
    PR_KG_MAPPING_STATE_ORPHANED,    /**< PR node deleted, KG node remains */
    PR_KG_MAPPING_STATE_ERROR        /**< Sync error occurred */
} pr_kg_mapping_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Bridge configuration
 *
 * WHAT: Configuration options for KG bridge behavior
 * WHY:  Different use cases need different sync strategies
 * HOW:  Set flags and parameters before bridge creation
 */
typedef struct {
    brain_kg_t* brain_kg;           /**< Existing brain KG (required) */
    bool auto_register_memories;    /**< Auto-add new memories to KG */
    bool sync_on_update;            /**< Sync on every memory update */
    bool sync_entanglements;        /**< Sync entanglement edges to KG */
    float edge_weight_scale;        /**< Scale factor for edge weights */
    uint64_t stale_threshold_ms;    /**< Time before mapping is stale */
    bool enable_statistics;         /**< Track bridge statistics */
} pr_kg_bridge_config_t;

/**
 * @brief Mapping entry between PR node and KG node
 *
 * WHAT: Bidirectional mapping record
 * WHY:  Enable translation between PR memory IDs and KG node IDs
 * HOW:  Hash table indexed by both PR ID and KG ID
 */
typedef struct {
    uint64_t pr_node_id;            /**< PR memory node ID */
    brain_kg_node_id_t kg_node_id;  /**< Brain KG node ID */
    uint64_t created_time_ms;       /**< When mapping was created */
    uint64_t sync_time_ms;          /**< Last sync timestamp */
    pr_kg_mapping_state_t state;    /**< Current mapping state */
    uint32_t sync_count;            /**< Number of syncs performed */
    uint32_t module_id;             /**< Associated brain module ID */
} pr_kg_mapping_t;

/**
 * @brief Query result for memory lookups
 *
 * WHAT: Container for memory query results
 * WHY:  Return multiple memories with their KG context
 * HOW:  Array of mappings with count
 */
typedef struct {
    pr_kg_mapping_t* mappings;      /**< Array of mapping results */
    uint32_t count;                 /**< Number of results */
    uint32_t capacity;              /**< Allocated capacity */
} pr_kg_query_result_t;

/**
 * @brief Context information for a KG node
 *
 * WHAT: Contextual data about a KG node and its memories
 * WHY:  Provide rich context for memory retrieval decisions
 * HOW:  Aggregate info from KG node and related memories
 */
typedef struct {
    brain_kg_node_id_t kg_node_id;      /**< KG node being queried */
    const char* node_name;              /**< KG node name */
    brain_kg_node_type_t node_type;     /**< KG node type */
    uint32_t memory_count;              /**< Number of linked memories */
    uint64_t* memory_ids;               /**< Array of PR memory IDs */
    uint32_t neighbor_count;            /**< Number of neighbor nodes */
    float avg_edge_weight;              /**< Average edge weight to neighbors */
} pr_kg_context_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the bridge
 * WHY:  Monitor bridge health and performance
 * HOW:  Updated atomically during operations
 */
typedef struct {
    uint64_t total_registrations;       /**< Total memories registered */
    uint64_t total_unregistrations;     /**< Total memories unregistered */
    uint64_t total_syncs;               /**< Total sync operations */
    uint64_t total_edge_syncs;          /**< Total entanglement syncs */
    uint64_t total_queries;             /**< Total query operations */
    uint64_t active_mappings;           /**< Current active mapping count */
    uint64_t stale_mappings;            /**< Current stale mapping count */
    uint64_t error_count;               /**< Total errors encountered */
    uint64_t last_sync_time_ms;         /**< Last sync timestamp */
    uint64_t total_sync_time_ns;        /**< Cumulative sync time */
} pr_kg_bridge_stats_t;

/**
 * @brief KG Bridge handle (opaque)
 *
 * The bridge maintains:
 * - Mapping table for PR ID <-> KG ID translation
 * - Configuration parameters
 * - Statistics counters
 * - Internal mutex for thread safety
 */
typedef struct pr_kg_bridge_struct* pr_kg_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets balanced defaults with auto-sync enabled
 *
 * @return Default configuration with:
 *         - brain_kg: NULL (must be set)
 *         - auto_register_memories: true
 *         - sync_on_update: true
 *         - sync_entanglements: true
 *         - edge_weight_scale: 1.0
 *         - stale_threshold_ms: 5 minutes
 *         - enable_statistics: true
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT pr_kg_bridge_config_t pr_kg_bridge_config_default(void);

/**
 * @brief Create KG bridge with configuration
 *
 * WHAT: Creates a new KG bridge instance
 * WHY:  Central entry point for PR<->KG integration
 * HOW:  Allocates bridge, initializes mapping table, connects to KG
 *
 * @param config Bridge configuration (brain_kg must be non-NULL)
 * @return New bridge handle, or NULL on error
 *
 * Performance: ~10us (allocation + initialization)
 *
 * Example:
 *   pr_kg_bridge_config_t config = pr_kg_bridge_config_default();
 *   config.brain_kg = my_brain_kg;
 *   pr_kg_bridge_t bridge = pr_kg_bridge_create(&config);
 */
NIMCP_EXPORT pr_kg_bridge_t pr_kg_bridge_create(const pr_kg_bridge_config_t* config);

/**
 * @brief Destroy KG bridge
 *
 * WHAT: Frees all bridge resources
 * WHY:  Clean shutdown and resource release
 * HOW:  Frees mapping table, releases mutex, NULL-safe
 *
 * NOTE: Does NOT destroy the connected brain_kg (caller owns it)
 *
 * @param bridge Bridge handle (NULL safe)
 *
 * Performance: ~5us
 */
NIMCP_EXPORT void pr_kg_bridge_destroy(pr_kg_bridge_t bridge);

/**
 * @brief Connect bridge to a different brain KG
 *
 * WHAT: Changes the connected brain KG
 * WHY:  Support hot-swapping KG or late initialization
 * HOW:  Updates internal KG pointer, invalidates existing mappings
 *
 * WARNING: This invalidates all existing mappings!
 *
 * @param bridge Bridge handle
 * @param brain_kg New brain KG to connect (must be non-NULL)
 * @return 0 on success, -1 on error
 *
 * Performance: ~100us (invalidates and clears mappings)
 */
NIMCP_EXPORT int pr_kg_bridge_connect(pr_kg_bridge_t bridge, brain_kg_t* brain_kg);

/**
 * @brief Check if bridge is connected and valid
 *
 * @param bridge Bridge handle
 * @return true if connected and operational
 */
NIMCP_EXPORT bool pr_kg_bridge_is_connected(const pr_kg_bridge_t bridge);

//=============================================================================
// Memory -> KG Sync Functions
//=============================================================================

/**
 * @brief Register a PR memory node in the brain KG
 *
 * WHAT: Creates a KG node for a PR memory and establishes mapping
 * WHY:  Make memory visible to brain's self-awareness
 * HOW:  Creates BRAIN_KG_NODE_COGNITIVE node with memory metadata
 *
 * Creates a new KG node with:
 * - Type: BRAIN_KG_NODE_COGNITIVE
 * - Name: "pr_memory_<id>"
 * - Description: Memory content summary
 * - Metadata: signature hash, quaternion values, phase
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR memory node ID
 * @param content_desc Brief content description for KG node
 * @param module_id Associated brain module ID (for later queries)
 * @return KG node ID, or BRAIN_KG_INVALID_NODE on error
 *
 * Performance: ~500ns
 *
 * Example:
 *   brain_kg_node_id_t kg_id = pr_kg_register_memory(bridge, mem_id,
 *       "Memory of beach sunset", PREFRONTAL_MODULE_ID);
 */
NIMCP_EXPORT brain_kg_node_id_t pr_kg_register_memory(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    const char* content_desc,
    uint32_t module_id
);

/**
 * @brief Register memory with additional metadata
 *
 * WHAT: Register with full metadata for KG node
 * WHY:  Store richer information in KG for later queries
 * HOW:  Same as pr_kg_register_memory but with extra metadata
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR memory node ID
 * @param content_desc Brief content description
 * @param module_id Associated brain module ID
 * @param signature Prime signature for metadata storage
 * @param importance Memory importance value [0, 1]
 * @return KG node ID, or BRAIN_KG_INVALID_NODE on error
 */
NIMCP_EXPORT brain_kg_node_id_t pr_kg_register_memory_full(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    const char* content_desc,
    uint32_t module_id,
    const prime_signature_t* signature,
    float importance
);

/**
 * @brief Unregister a PR memory from the brain KG
 *
 * WHAT: Removes KG node and mapping for a memory
 * WHY:  Clean up when memory is deleted
 * HOW:  Removes KG node (and its edges), removes mapping
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR memory node ID
 * @return 0 on success, -1 if not found or error
 *
 * Performance: ~300ns
 */
NIMCP_EXPORT int pr_kg_unregister_memory(pr_kg_bridge_t bridge, uint64_t pr_node_id);

/**
 * @brief Sync PR memory state to KG node
 *
 * WHAT: Updates KG node to reflect current PR memory state
 * WHY:  Keep KG in sync when memory state changes
 * HOW:  Updates KG node description, metadata, state
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR memory node ID
 * @param new_desc New description (NULL to keep current)
 * @param new_state New KG node state
 * @return 0 on success, -1 on error
 *
 * Performance: ~200ns
 */
NIMCP_EXPORT int pr_kg_sync_memory(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    const char* new_desc,
    brain_kg_node_state_t new_state
);

/**
 * @brief Sync an entanglement edge to a KG edge
 *
 * WHAT: Creates/updates KG edge from entanglement relationship
 * WHY:  Mirror memory relationships in KG for graph queries
 * HOW:  Maps entanglement type to KG edge type, sets weight
 *
 * Edge Type Mapping:
 * - ENTANGLE_EDGE_SEMANTIC -> BRAIN_KG_EDGE_CONNECTS_TO
 * - ENTANGLE_EDGE_CAUSAL -> BRAIN_KG_EDGE_SENDS_TO
 * - ENTANGLE_EDGE_ASSOCIATIVE -> BRAIN_KG_EDGE_MODULATES
 * - ENTANGLE_EDGE_EMOTIONAL -> BRAIN_KG_EDGE_MODULATES
 * - ENTANGLE_EDGE_TEMPORAL -> BRAIN_KG_EDGE_COORDINATES_WITH
 *
 * @param bridge Bridge handle
 * @param from_pr_id Source PR memory node ID
 * @param to_pr_id Target PR memory node ID
 * @param edge_type Entanglement edge type
 * @param resonance_strength Edge weight (will be scaled by config)
 * @return KG edge ID, or BRAIN_KG_INVALID_NODE on error
 *
 * Performance: ~400ns
 */
NIMCP_EXPORT brain_kg_edge_id_t pr_kg_sync_entanglement(
    pr_kg_bridge_t bridge,
    uint64_t from_pr_id,
    uint64_t to_pr_id,
    pr_kg_entangle_edge_type_t edge_type,
    float resonance_strength
);

/**
 * @brief Remove a synced entanglement edge from KG
 *
 * @param bridge Bridge handle
 * @param from_pr_id Source PR memory node ID
 * @param to_pr_id Target PR memory node ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_kg_remove_entanglement(
    pr_kg_bridge_t bridge,
    uint64_t from_pr_id,
    uint64_t to_pr_id
);

//=============================================================================
// KG -> Memory Query Functions
//=============================================================================

/**
 * @brief Query memories associated with a brain module
 *
 * WHAT: Find all memories linked to a specific brain module
 * WHY:  Enable module-specific memory retrieval
 * HOW:  Scans mapping table for entries with matching module_id
 *
 * @param bridge Bridge handle
 * @param module_id Brain module ID to query
 * @param result Output query result (must be pre-allocated)
 * @param max_results Maximum results to return
 * @return Number of results, or -1 on error
 *
 * Performance: O(N) where N = total mappings
 *
 * Example:
 *   pr_kg_query_result_t result;
 *   result.mappings = nimcp_malloc(100 * sizeof(pr_kg_mapping_t));
 *   result.capacity = 100;
 *   int count = pr_kg_query_by_module(bridge, HIPPOCAMPUS_ID, &result, 100);
 */
NIMCP_EXPORT int pr_kg_query_by_module(
    pr_kg_bridge_t bridge,
    uint32_t module_id,
    pr_kg_query_result_t* result,
    uint32_t max_results
);

/**
 * @brief Query memories along a KG path
 *
 * WHAT: Find memories corresponding to nodes along a KG path
 * WHY:  Retrieve memories in sequence based on KG structure
 * HOW:  Uses KG path, maps each node to PR memory if exists
 *
 * @param bridge Bridge handle
 * @param path KG path from brain_kg_find_path()
 * @param pr_node_ids Output array for PR node IDs (pre-allocated)
 * @param max_ids Size of output array
 * @return Number of PR nodes found along path, or -1 on error
 *
 * Performance: O(path_length)
 */
NIMCP_EXPORT int pr_kg_query_by_path(
    pr_kg_bridge_t bridge,
    const brain_kg_path_t* path,
    uint64_t* pr_node_ids,
    uint32_t max_ids
);

/**
 * @brief Get contextual information for a KG node
 *
 * WHAT: Retrieve rich context about a KG node and its memories
 * WHY:  Support context-aware memory retrieval
 * HOW:  Aggregates KG node info + linked memory stats
 *
 * @param bridge Bridge handle
 * @param kg_node_id KG node to get context for
 * @param context Output context structure (pre-allocated)
 * @return 0 on success, -1 on error
 *
 * Performance: ~500ns
 */
NIMCP_EXPORT int pr_kg_get_context(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id,
    pr_kg_context_t* context
);

/**
 * @brief Free context structure resources
 *
 * @param context Context to free (NULL safe)
 */
NIMCP_EXPORT void pr_kg_context_destroy(pr_kg_context_t* context);

/**
 * @brief Query memories connected to a specific KG node
 *
 * WHAT: Find memories that have edges to a given KG node
 * WHY:  Retrieve memories relevant to a brain region/module
 * HOW:  Uses KG edges to find connected memory nodes
 *
 * @param bridge Bridge handle
 * @param kg_node_id KG node to query connections from
 * @param pr_node_ids Output array for PR node IDs
 * @param max_ids Size of output array
 * @return Number of memories found, or -1 on error
 */
NIMCP_EXPORT int pr_kg_query_by_kg_node(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id,
    uint64_t* pr_node_ids,
    uint32_t max_ids
);

//=============================================================================
// Mapping Management Functions
//=============================================================================

/**
 * @brief Get KG node ID for a PR memory
 *
 * WHAT: Translate PR node ID to KG node ID
 * WHY:  Enable direct KG operations on memory nodes
 * HOW:  Hash lookup in mapping table
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR memory node ID
 * @return KG node ID, or BRAIN_KG_INVALID_NODE if not found
 *
 * Performance: O(1) average (hash lookup)
 */
NIMCP_EXPORT brain_kg_node_id_t pr_kg_get_kg_node(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id
);

/**
 * @brief Get PR memory ID for a KG node
 *
 * WHAT: Translate KG node ID to PR node ID
 * WHY:  Enable memory retrieval from KG queries
 * HOW:  Reverse hash lookup in mapping table
 *
 * @param bridge Bridge handle
 * @param kg_node_id KG node ID
 * @return PR node ID, or PR_KG_INVALID_PR_ID if not found
 *
 * Performance: O(1) average (hash lookup)
 */
NIMCP_EXPORT uint64_t pr_kg_get_pr_node(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id
);

/**
 * @brief Get mapping details for a PR memory
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR memory node ID
 * @param mapping Output mapping structure
 * @return 0 on success, -1 if not found
 */
NIMCP_EXPORT int pr_kg_get_mapping(
    pr_kg_bridge_t bridge,
    uint64_t pr_node_id,
    pr_kg_mapping_t* mapping
);

/**
 * @brief List all active mappings
 *
 * WHAT: Retrieve all PR<->KG mappings
 * WHY:  Debugging, export, bulk operations
 * HOW:  Iterates mapping table, returns active entries
 *
 * @param bridge Bridge handle
 * @param result Output query result
 * @param max_results Maximum results to return
 * @return Number of mappings returned, or -1 on error
 *
 * Performance: O(N) where N = mapping table size
 */
NIMCP_EXPORT int pr_kg_list_mappings(
    pr_kg_bridge_t bridge,
    pr_kg_query_result_t* result,
    uint32_t max_results
);

/**
 * @brief Get mapping state string
 *
 * @param state Mapping state
 * @return Human-readable state name
 */
NIMCP_EXPORT const char* pr_kg_mapping_state_to_string(pr_kg_mapping_state_t state);

//=============================================================================
// Batch Operations
//=============================================================================

/**
 * @brief Register multiple memories in batch
 *
 * WHAT: Efficiently register many memories at once
 * WHY:  Reduce overhead for bulk imports
 * HOW:  Single lock, batch KG operations
 *
 * @param bridge Bridge handle
 * @param pr_node_ids Array of PR memory node IDs
 * @param descriptions Array of content descriptions (can be NULL for generic)
 * @param module_id Module ID for all memories
 * @param count Number of memories to register
 * @return Number successfully registered, or -1 on error
 *
 * Performance: ~400ns per memory (amortized)
 */
NIMCP_EXPORT int pr_kg_batch_register(
    pr_kg_bridge_t bridge,
    const uint64_t* pr_node_ids,
    const char** descriptions,
    uint32_t module_id,
    size_t count
);

/**
 * @brief Sync all mapped nodes
 *
 * WHAT: Update all KG nodes from current PR memory states
 * WHY:  Periodic full synchronization
 * HOW:  Iterates all mappings, syncs each one
 *
 * @param bridge Bridge handle
 * @return Number synced, or -1 on error
 *
 * Performance: ~200ns per mapping
 */
NIMCP_EXPORT int pr_kg_batch_sync(pr_kg_bridge_t bridge);

/**
 * @brief Cleanup stale mappings
 *
 * WHAT: Remove mappings that haven't synced within threshold
 * WHY:  Prevent accumulation of dead mappings
 * HOW:  Checks sync_time against threshold, removes stale entries
 *
 * @param bridge Bridge handle
 * @param threshold_ms Time threshold in ms (0 = use config default)
 * @return Number of mappings removed, or -1 on error
 *
 * Performance: O(N) where N = mapping count
 */
NIMCP_EXPORT int pr_kg_cleanup_stale(pr_kg_bridge_t bridge, uint64_t threshold_ms);

/**
 * @brief Mark a mapping as dirty (needs sync)
 *
 * @param bridge Bridge handle
 * @param pr_node_id PR node to mark dirty
 * @return 0 on success, -1 if not found
 */
NIMCP_EXPORT int pr_kg_mark_dirty(pr_kg_bridge_t bridge, uint64_t pr_node_id);

//=============================================================================
// Prime Signature Integration
//=============================================================================

/**
 * @brief Generate prime signature from KG node metadata
 *
 * WHAT: Create prime signature from KG node's stored metadata
 * WHY:  Enable content-based similarity queries via KG
 * HOW:  Reconstructs signature from stored hash/factors
 *
 * NOTE: This creates a partial signature for similarity matching,
 *       not a full reconstruction of original content.
 *
 * @param bridge Bridge handle
 * @param kg_node_id KG node to generate signature from
 * @return New prime signature (caller must free), or NULL on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT prime_signature_t* pr_kg_signature_from_node(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id
);

/**
 * @brief Find memories similar to a KG node's content
 *
 * WHAT: Query memories by content similarity to a KG node
 * WHY:  Enable content-based retrieval via KG structure
 * HOW:  Uses signature from KG node metadata, compares to all memories
 *
 * @param bridge Bridge handle
 * @param kg_node_id Reference KG node
 * @param similarity_threshold Minimum Jaccard similarity [0, 1]
 * @param pr_node_ids Output array for PR node IDs
 * @param max_ids Size of output array
 * @return Number of similar memories found, or -1 on error
 *
 * Performance: O(N * sig_compare) where N = mapped memories
 */
NIMCP_EXPORT int pr_kg_find_similar_memories(
    pr_kg_bridge_t bridge,
    brain_kg_node_id_t kg_node_id,
    float similarity_threshold,
    uint64_t* pr_node_ids,
    uint32_t max_ids
);

/**
 * @brief Find memories similar to a given signature
 *
 * WHAT: Query memories by content similarity to a signature
 * WHY:  Enable content-based retrieval without KG node reference
 * HOW:  Compares given signature to all mapped memory signatures
 *
 * @param bridge Bridge handle
 * @param signature Reference signature
 * @param similarity_threshold Minimum Jaccard similarity [0, 1]
 * @param pr_node_ids Output array for PR node IDs
 * @param max_ids Size of output array
 * @return Number of similar memories found, or -1 on error
 */
NIMCP_EXPORT int pr_kg_find_similar_by_signature(
    pr_kg_bridge_t bridge,
    const prime_signature_t* signature,
    float similarity_threshold,
    uint64_t* pr_node_ids,
    uint32_t max_ids
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_kg_get_stats(pr_kg_bridge_t bridge, pr_kg_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
NIMCP_EXPORT void pr_kg_reset_stats(pr_kg_bridge_t bridge);

/**
 * @brief Generate diagnostic summary
 *
 * @param bridge Bridge handle
 * @param buf Output buffer
 * @param size Buffer size
 * @return Characters written
 */
NIMCP_EXPORT size_t pr_kg_generate_summary(
    pr_kg_bridge_t bridge,
    char* buf,
    size_t size
);

/**
 * @brief Print bridge state to stdout (debug)
 *
 * @param bridge Bridge handle
 */
NIMCP_EXPORT void pr_kg_print_state(pr_kg_bridge_t bridge);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_kg_get_last_error(void);

//=============================================================================
// Query Result Management
//=============================================================================

/**
 * @brief Allocate query result structure
 *
 * @param capacity Maximum mappings to store
 * @return New query result, or NULL on error
 */
NIMCP_EXPORT pr_kg_query_result_t* pr_kg_query_result_create(uint32_t capacity);

/**
 * @brief Free query result structure
 *
 * @param result Query result to free (NULL safe)
 */
NIMCP_EXPORT void pr_kg_query_result_destroy(pr_kg_query_result_t* result);

/**
 * @brief Clear query result for reuse
 *
 * @param result Query result to clear
 */
NIMCP_EXPORT void pr_kg_query_result_clear(pr_kg_query_result_t* result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_KG_BRIDGE_H
