/**
 * @file nimcp_kg_temporal.h
 * @brief Temporal Queries (Time-Travel) for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Bi-temporal query support for knowledge graph time-travel capabilities
 * WHY:  Enable historical analysis, version tracking, and temporal diffs
 * HOW:  Track valid time vs transaction time, store node versions, support temporal queries
 *
 * BI-TEMPORAL MODEL:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        BI-TEMPORAL KNOWLEDGE GRAPH                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                           ║
 * ║   VALID TIME: When the fact was true in reality                           ║
 * ║   ────────────────────────────────────────────────────────                ║
 * ║   Example: "Module X was active from T1 to T2"                            ║
 * ║                                                                           ║
 * ║   TRANSACTION TIME: When the fact was recorded in the database            ║
 * ║   ────────────────────────────────────────────────────────                ║
 * ║   Example: "We learned about Module X's activity at T3"                   ║
 * ║                                                                           ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                    TEMPORAL QUERY MODES                              │ ║
 * ║   ├─────────────────────────────────────────────────────────────────────┤ ║
 * ║   │  CURRENT:   Current state (default)                                 │ ║
 * ║   │  AS_OF:     State at a specific timestamp                           │ ║
 * ║   │  BETWEEN:   Changes between two timestamps                          │ ║
 * ║   │  SINCE:     All changes since a timestamp                           │ ║
 * ║   │  VERSIONS:  All versions of an entity                               │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                           ║
 * ║   USE CASES:                                                              ║
 * ║   ─────────────                                                           ║
 * ║   - Audit trail: "What did the brain look like at T1?"                   ║
 * ║   - Debugging: "What changed between the error and now?"                 ║
 * ║   - Rollback: "Restore to a known-good state"                            ║
 * ║   - Analysis: "How has topology evolved over time?"                      ║
 * ║                                                                           ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe via internal synchronization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_TEMPORAL_H
#define NIMCP_KG_TEMPORAL_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum versions to track per node */
#define KG_TEMPORAL_MAX_VERSIONS_PER_NODE   256

/** Maximum nodes in a temporal diff result */
#define KG_TEMPORAL_MAX_DIFF_NODES          1024

/** Maximum topology snapshots for evolution analysis */
#define KG_TEMPORAL_MAX_SNAPSHOTS           128

/** Sentinel value for "current" (no end time) */
#define KG_TEMPORAL_TIME_CURRENT            0

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Temporal query mode
 *
 * WHAT: Specifies how to interpret timestamps in temporal queries
 * WHY:  Different use cases require different temporal perspectives
 * HOW:  Query engine interprets timestamps according to selected mode
 */
typedef enum {
    KG_TEMPORAL_CURRENT = 0,            /**< Current state (default) */
    KG_TEMPORAL_AS_OF,                  /**< State at specific timestamp */
    KG_TEMPORAL_BETWEEN,                /**< Changes between timestamps */
    KG_TEMPORAL_SINCE,                  /**< All changes since timestamp */
    KG_TEMPORAL_VERSIONS                /**< All versions of entity */
} kg_temporal_mode_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Bi-temporal coordinates
 *
 * WHAT: Two-dimensional time coordinates for temporal data
 * WHY:  Distinguish when facts were true vs when they were recorded
 * HOW:  Track both valid_time and transaction_time for each fact
 *
 * BIOLOGICAL BASIS: The brain maintains both episodic memory (when events
 * happened) and autobiographical memory (when we learned about them).
 */
typedef struct {
    uint64_t valid_time;                /**< When fact was true in reality (ms) */
    uint64_t transaction_time;          /**< When fact was recorded in DB (ms) */
} kg_bitemporal_t;

/**
 * @brief Temporal query specification
 *
 * WHAT: Parameters for temporal queries
 * WHY:  Provide flexible time-travel query capabilities
 * HOW:  Combine mode with timestamps and time dimension selector
 */
typedef struct {
    kg_temporal_mode_t mode;            /**< Query mode */
    uint64_t as_of_timestamp;           /**< For AS_OF queries (ms) */
    uint64_t start_timestamp;           /**< For BETWEEN queries (ms) */
    uint64_t end_timestamp;             /**< For BETWEEN queries (ms) */
    bool use_transaction_time;          /**< Query by transaction vs valid time */
} kg_temporal_query_t;

/**
 * @brief Node version record
 *
 * WHAT: Historical snapshot of a node at a point in time
 * WHY:  Enable time-travel queries and version history
 * HOW:  Store complete node state with temporal coordinates
 */
typedef struct {
    brain_kg_node_id_t node_id;         /**< Node identifier */
    uint64_t version;                   /**< Version number (1-indexed) */
    uint64_t valid_from;                /**< Start of validity period (ms) */
    uint64_t valid_to;                  /**< End of validity (0 = current) */
    uint64_t transaction_time;          /**< When this version was recorded (ms) */
    void* snapshot_data;                /**< Node state at this version */
    size_t snapshot_size;               /**< Size of snapshot data (bytes) */
} kg_node_version_t;

/**
 * @brief Temporal diff result
 *
 * WHAT: Summary of changes between two points in time
 * WHY:  Enable diff analysis for debugging and auditing
 * HOW:  Track added, removed, and modified nodes between timestamps
 */
typedef struct {
    brain_kg_node_id_t* added_nodes;    /**< Nodes added in this period */
    uint32_t added_count;               /**< Number of added nodes */
    brain_kg_node_id_t* removed_nodes;  /**< Nodes removed in this period */
    uint32_t removed_count;             /**< Number of removed nodes */
    brain_kg_node_id_t* modified_nodes; /**< Nodes modified in this period */
    uint32_t modified_count;            /**< Number of modified nodes */
    uint64_t from_timestamp;            /**< Start of diff period (ms) */
    uint64_t to_timestamp;              /**< End of diff period (ms) */
} kg_temporal_diff_t;

/**
 * @brief Topology snapshot for evolution analysis
 *
 * WHAT: Point-in-time snapshot of graph topology metrics
 * WHY:  Track how brain topology evolves over time
 * HOW:  Capture key metrics at sampled timestamps
 */
typedef struct {
    uint64_t timestamp;                 /**< Snapshot timestamp (ms) */
    uint32_t node_count;                /**< Total nodes at this time */
    uint32_t edge_count;                /**< Total edges at this time */
    uint32_t active_nodes;              /**< Active nodes at this time */
    uint32_t component_count;           /**< Number of connected components */
    float density;                      /**< Graph density [0-1] */
    float avg_clustering;               /**< Average clustering coefficient */
    uint32_t diameter;                  /**< Graph diameter (longest shortest path) */
} kg_topology_snapshot_t;

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * @brief Get default temporal query (current state)
 *
 * @param query Output query structure
 * @return 0 on success, -1 on error
 */
int kg_temporal_query_default(kg_temporal_query_t* query);

/* ============================================================================
 * Temporal Query API
 * ============================================================================ */

/**
 * @brief Query a node at a specific point in time
 *
 * WHAT: Retrieve node state as it was at the specified time
 * WHY:  Enable time-travel debugging and historical analysis
 * HOW:  Look up version valid at query timestamp
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to query
 * @param query Temporal query specification
 * @param result Output: pointer to result data (caller must free)
 * @param result_size Output: size of result data
 * @return 0 on success, -1 if not found or error
 */
int kg_temporal_query_node(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    const kg_temporal_query_t* query,
    void** result,
    size_t* result_size
);

/**
 * @brief Query a subgraph at a specific point in time
 *
 * WHAT: Retrieve subgraph rooted at node as it was at the specified time
 * WHY:  Enable historical analysis of connected module groups
 * HOW:  Traverse graph from root, reconstructing historical state
 *
 * @param kg Brain knowledge graph
 * @param root Root node for subgraph extraction
 * @param depth Maximum traversal depth from root
 * @param query Temporal query specification
 * @param result Output: reconstructed historical subgraph (caller must destroy)
 * @return 0 on success, -1 on error
 */
int kg_temporal_query_subgraph(
    const brain_kg_t* kg,
    brain_kg_node_id_t root,
    uint32_t depth,
    const kg_temporal_query_t* query,
    brain_kg_t** result
);

/**
 * @brief Query multiple nodes with temporal specification
 *
 * WHAT: Batch temporal query for multiple nodes
 * WHY:  Efficient retrieval of historical state for node sets
 * HOW:  Retrieve historical state for each node in the set
 *
 * @param kg Brain knowledge graph
 * @param node_ids Array of node IDs to query
 * @param node_count Number of nodes to query
 * @param query Temporal query specification
 * @param results Output array of result pointers (caller allocated, caller frees contents)
 * @param result_sizes Output array of result sizes
 * @return Number of nodes successfully queried, -1 on error
 */
int kg_temporal_query_nodes(
    const brain_kg_t* kg,
    const brain_kg_node_id_t* node_ids,
    uint32_t node_count,
    const kg_temporal_query_t* query,
    void** results,
    size_t* result_sizes
);

/* ============================================================================
 * Version History API
 * ============================================================================ */

/**
 * @brief Get all versions of a node
 *
 * WHAT: Retrieve complete version history for a node
 * WHY:  Enable audit trail and rollback capabilities
 * HOW:  Return all stored versions in chronological order
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to get history for
 * @param versions Output array (caller allocated)
 * @param max Maximum versions to retrieve
 * @param count Output: actual number of versions retrieved
 * @return 0 on success, -1 on error
 */
int kg_temporal_get_versions(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    kg_node_version_t* versions,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Get node version at a specific timestamp
 *
 * WHAT: Retrieve the version that was valid at the given time
 * WHY:  Point-in-time lookup for debugging and analysis
 * HOW:  Binary search through versions by valid_from/valid_to
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to query
 * @param timestamp Time to query (ms since epoch)
 * @param version Output: version record at that time
 * @return 0 on success, -1 if no version exists at that time
 */
int kg_temporal_get_version_at(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    uint64_t timestamp,
    kg_node_version_t* version
);

/**
 * @brief Get the current version number of a node
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to query
 * @return Current version number, 0 if node has no history
 */
uint64_t kg_temporal_get_current_version(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id
);

/**
 * @brief Check if a node existed at a specific timestamp
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to check
 * @param timestamp Time to check (ms since epoch)
 * @return true if node existed at that time, false otherwise
 */
bool kg_temporal_node_existed_at(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    uint64_t timestamp
);

/**
 * @brief Get the creation timestamp of a node
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to query
 * @return Creation timestamp (ms since epoch), 0 if not found
 */
uint64_t kg_temporal_get_creation_time(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id
);

/**
 * @brief Free version snapshot data
 *
 * @param version Version record to free snapshot from (NULL safe)
 */
void kg_temporal_version_free(kg_node_version_t* version);

/* ============================================================================
 * Temporal Diff API
 * ============================================================================ */

/**
 * @brief Compute diff between two timestamps
 *
 * WHAT: Calculate what changed between two points in time
 * WHY:  Debugging, auditing, change analysis
 * HOW:  Compare node sets and states at both timestamps
 *
 * @param kg Brain knowledge graph
 * @param from_timestamp Start of diff period (ms)
 * @param to_timestamp End of diff period (ms)
 * @param diff Output: diff result (caller must free with kg_temporal_diff_free)
 * @return 0 on success, -1 on error
 */
int kg_temporal_diff(
    const brain_kg_t* kg,
    uint64_t from_timestamp,
    uint64_t to_timestamp,
    kg_temporal_diff_t* diff
);

/**
 * @brief Compute diff for a specific subgraph
 *
 * WHAT: Calculate changes within a subgraph between timestamps
 * WHY:  Focused analysis of specific module groups
 * HOW:  Diff only nodes reachable from root
 *
 * @param kg Brain knowledge graph
 * @param root Root node of subgraph
 * @param depth Maximum depth from root
 * @param from_timestamp Start of diff period (ms)
 * @param to_timestamp End of diff period (ms)
 * @param diff Output: diff result
 * @return 0 on success, -1 on error
 */
int kg_temporal_diff_subgraph(
    const brain_kg_t* kg,
    brain_kg_node_id_t root,
    uint32_t depth,
    uint64_t from_timestamp,
    uint64_t to_timestamp,
    kg_temporal_diff_t* diff
);

/**
 * @brief Get detailed changes for a modified node
 *
 * WHAT: Retrieve what specifically changed in a modified node
 * WHY:  Detailed audit of individual node changes
 * HOW:  Compare versions at start and end of period
 *
 * @param kg Brain knowledge graph
 * @param node_id Modified node to examine
 * @param from_timestamp Start of period
 * @param to_timestamp End of period
 * @param before_version Output: version before change
 * @param after_version Output: version after change
 * @return 0 on success, -1 if node wasn't modified in period
 */
int kg_temporal_get_node_changes(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    uint64_t from_timestamp,
    uint64_t to_timestamp,
    kg_node_version_t* before_version,
    kg_node_version_t* after_version
);

/**
 * @brief Free temporal diff result
 *
 * @param diff Diff result to free (NULL safe)
 */
void kg_temporal_diff_free(kg_temporal_diff_t* diff);

/* ============================================================================
 * Trend Analysis API
 * ============================================================================ */

/**
 * @brief Get topology evolution over time
 *
 * WHAT: Sample topology metrics at regular intervals
 * WHY:  Analyze how brain structure has evolved
 * HOW:  Take topology snapshots at sampled timestamps
 *
 * @param kg Brain knowledge graph
 * @param start Start of analysis period (ms)
 * @param end End of analysis period (ms)
 * @param sample_count Number of samples to take
 * @param snapshots Output array (caller allocated, size sample_count)
 * @return Number of snapshots populated, -1 on error
 */
int kg_temporal_get_topology_evolution(
    const brain_kg_t* kg,
    uint64_t start,
    uint64_t end,
    uint32_t sample_count,
    kg_topology_snapshot_t* snapshots
);

/**
 * @brief Get node count trend over time
 *
 * WHAT: Track node count at sampled timestamps
 * WHY:  Simple growth analysis
 * HOW:  Count nodes at each sample point
 *
 * @param kg Brain knowledge graph
 * @param start Start of analysis period (ms)
 * @param end End of analysis period (ms)
 * @param timestamps Output: sampled timestamps (caller allocated)
 * @param node_counts Output: node counts at each timestamp (caller allocated)
 * @param sample_count Number of samples to take
 * @return Number of samples populated, -1 on error
 */
int kg_temporal_get_node_count_trend(
    const brain_kg_t* kg,
    uint64_t start,
    uint64_t end,
    uint64_t* timestamps,
    uint32_t* node_counts,
    uint32_t sample_count
);

/**
 * @brief Get activity trend for a specific node
 *
 * WHAT: Track state changes for a node over time
 * WHY:  Analyze module stability and patterns
 * HOW:  Sample node state at regular intervals
 *
 * @param kg Brain knowledge graph
 * @param node_id Node to track
 * @param start Start of analysis period (ms)
 * @param end End of analysis period (ms)
 * @param timestamps Output: sampled timestamps (caller allocated)
 * @param states Output: node states at each timestamp (caller allocated)
 * @param sample_count Number of samples to take
 * @return Number of samples populated, -1 on error
 */
int kg_temporal_get_node_activity_trend(
    const brain_kg_t* kg,
    brain_kg_node_id_t node_id,
    uint64_t start,
    uint64_t end,
    uint64_t* timestamps,
    brain_kg_node_state_t* states,
    uint32_t sample_count
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert temporal mode to string
 *
 * @param mode Temporal query mode
 * @return Human-readable string representation
 */
const char* kg_temporal_mode_to_string(kg_temporal_mode_t mode);

/**
 * @brief Get current timestamp in milliseconds
 *
 * @return Current timestamp (ms since epoch)
 */
uint64_t kg_temporal_now(void);

/**
 * @brief Create a bi-temporal coordinate with current transaction time
 *
 * @param valid_time When the fact is valid
 * @return Bi-temporal coordinate with current transaction time
 */
kg_bitemporal_t kg_temporal_create_bitemporal(uint64_t valid_time);

/**
 * @brief Check if a timestamp falls within a bi-temporal validity range
 *
 * @param bitemporal Bi-temporal coordinates
 * @param timestamp Timestamp to check
 * @param use_transaction_time Check transaction time instead of valid time
 * @return true if timestamp is within range
 */
bool kg_temporal_in_range(
    const kg_bitemporal_t* bitemporal,
    uint64_t timestamp,
    bool use_transaction_time
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_TEMPORAL_H */
