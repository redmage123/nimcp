/**
 * @file nimcp_kg_federation.h
 * @brief Cross-Brain Federation for Knowledge Graph Synchronization
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Federation layer enabling multiple brain instances to share KG data
 * WHY:  Enable distributed AI systems with shared knowledge and coordination
 * HOW:  Peer-to-peer synchronization with configurable conflict resolution
 *
 * FEDERATION ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    KG FEDERATION SYSTEM                                    |
 * +===========================================================================+
 * |                                                                            |
 * |   Brain Instance A          Federation Layer         Brain Instance B      |
 * |   ---------------           ----------------         ----------------      |
 * |   +-----------+             +-----------+            +-----------+         |
 * |   | brain_kg  |<----------->| Sync      |<---------->| brain_kg  |         |
 * |   | (local)   |   push/pull | Protocol  |  push/pull | (remote)  |         |
 * |   +-----------+             +-----------+            +-----------+         |
 * |        |                         |                        |                |
 * |        v                         v                        v                |
 * |   +-----------+             +-----------+            +-----------+         |
 * |   | Changes   |             | Conflict  |            | Changes   |         |
 * |   | Tracking  |             | Resolver  |            | Tracking  |         |
 * |   +-----------+             +-----------+            +-----------+         |
 * |                                                                            |
 * |   SYNC POLICIES:                                                           |
 * |   - PULL_ONLY:      Receive updates from peers                            |
 * |   - PUSH_ONLY:      Send updates to peers                                 |
 * |   - BIDIRECTIONAL:  Full two-way synchronization                          |
 * |   - SELECTIVE:      Sync only specified subgraphs                         |
 * |                                                                            |
 * |   CONFLICT STRATEGIES:                                                     |
 * |   - LAST_WRITE_WINS:  Most recent timestamp prevails                      |
 * |   - FIRST_WRITE_WINS: Original value preserved                            |
 * |   - MERGE:            Attempt automated merge                             |
 * |   - MANUAL:           Queue for manual resolution                         |
 * |   - PRIORITY:         Higher trust peer wins                              |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * THREAD SAFETY: All federation operations are thread-safe
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_FEDERATION_H
#define NIMCP_KG_FEDERATION_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of peer ID */
#define KG_FEDERATION_PEER_ID_LEN       64

/** Maximum length of hostname */
#define KG_FEDERATION_HOST_LEN          256

/** Maximum number of peers in federation */
#define KG_FEDERATION_MAX_PEERS         64

/** Maximum node types in sync filter (comma-separated) */
#define KG_FEDERATION_MAX_FILTER_LEN    512

/** Default discovery port for auto-discovery */
#define KG_FEDERATION_DEFAULT_DISCOVERY_PORT   5353

/** Default sync interval in milliseconds */
#define KG_FEDERATION_DEFAULT_SYNC_INTERVAL_MS 30000

/** Default maximum items per sync batch */
#define KG_FEDERATION_DEFAULT_BATCH_SIZE       1000

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** Forward declaration for diff result (from future kg_diff module) */
typedef struct kg_diff_result kg_diff_result_t;

/** Forward declaration for conflict entry */
typedef struct kg_conflict kg_conflict_t;

/** Forward declaration for conflict resolution action */
typedef struct kg_conflict_resolution kg_conflict_resolution_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Synchronization policy
 *
 * WHAT: Defines how this brain instance participates in federation sync
 * WHY:  Different deployment scenarios require different sync behaviors
 * HOW:  Controls push/pull directions during synchronization
 */
typedef enum {
    KG_SYNC_NONE = 0,               /**< No sync (isolated mode) */
    KG_SYNC_PULL_ONLY,              /**< Only receive updates from peers */
    KG_SYNC_PUSH_ONLY,              /**< Only send updates to peers */
    KG_SYNC_BIDIRECTIONAL,          /**< Full two-way synchronization */
    KG_SYNC_SELECTIVE               /**< Sync only specified subgraphs */
} kg_sync_policy_t;

/**
 * @brief Conflict resolution strategy
 *
 * WHAT: Strategy for handling conflicting updates from multiple peers
 * WHY:  Distributed systems inevitably have concurrent modifications
 * HOW:  Applied automatically during sync merge operations
 */
typedef enum {
    KG_CONFLICT_LAST_WRITE_WINS = 0, /**< Most recent write wins (by timestamp) */
    KG_CONFLICT_FIRST_WRITE_WINS,    /**< First write wins (original preserved) */
    KG_CONFLICT_MERGE,               /**< Attempt to merge changes automatically */
    KG_CONFLICT_MANUAL,              /**< Require manual resolution */
    KG_CONFLICT_PRIORITY             /**< Higher priority/trust peer wins */
} kg_conflict_strategy_t;

/**
 * @brief Peer connection state
 *
 * WHAT: Current state of connection to a federation peer
 * WHY:  Track peer health for routing and failover decisions
 * HOW:  Updated by heartbeat and sync operations
 */
typedef enum {
    KG_PEER_STATE_UNKNOWN = 0,       /**< Connection state not determined */
    KG_PEER_STATE_DISCONNECTED,      /**< Not connected to peer */
    KG_PEER_STATE_CONNECTING,        /**< Connection in progress */
    KG_PEER_STATE_CONNECTED,         /**< Connected and ready */
    KG_PEER_STATE_SYNCING,           /**< Active synchronization in progress */
    KG_PEER_STATE_ERROR              /**< Connection error */
} kg_peer_state_t;

/* ============================================================================
 * Data Structures - Federation Peer
 * ============================================================================ */

/**
 * @brief Federation peer information
 *
 * WHAT: Represents a peer brain instance in the federation
 * WHY:  Track peer identity, connectivity, and trust level
 * HOW:  Populated during peer discovery or manual registration
 */
typedef struct {
    char peer_id[KG_FEDERATION_PEER_ID_LEN]; /**< Unique peer identifier */
    char host[KG_FEDERATION_HOST_LEN];       /**< Peer hostname or IP */
    uint16_t port;                           /**< Peer port number */

    /* Connection state */
    bool is_connected;                       /**< Currently connected */
    kg_peer_state_t state;                   /**< Detailed connection state */
    uint64_t last_sync;                      /**< Last synchronization timestamp (ms) */
    uint64_t lag_ms;                         /**< Sync lag (time since last sync) */

    /* Trust and priority */
    float trust_score;                       /**< Trust level [0.0-1.0] */
    uint8_t priority;                        /**< Priority for conflict resolution */

    /* Statistics */
    uint64_t messages_sent;                  /**< Total messages sent to peer */
    uint64_t messages_received;              /**< Total messages received from peer */
    uint64_t sync_count;                     /**< Number of successful syncs */
    uint64_t conflict_count;                 /**< Number of conflicts with this peer */
} kg_federation_peer_t;

/* ============================================================================
 * Data Structures - Sync Filter
 * ============================================================================ */

/**
 * @brief Subgraph sync filter
 *
 * WHAT: Defines which parts of the KG to synchronize
 * WHY:  Enable selective sync for bandwidth/privacy optimization
 * HOW:  Filter by node types, modules, layers, or hemispheres
 */
typedef struct {
    char* node_types;                        /**< Node types to sync (comma-separated) */
    char* modules;                           /**< Module names to sync (comma-separated) */
    uint8_t layers;                          /**< Layer bitmask (bit 0 = Layer I, etc.) */
    uint8_t hemispheres;                     /**< Hemisphere bitmask (0x1=L, 0x2=R, 0x4=B) */
    bool include_weights;                    /**< Sync weight snapshots */
    bool include_neuromod;                   /**< Sync neuromodulator state */
    bool include_metadata;                   /**< Sync node metadata */
    bool include_edges;                      /**< Sync edges (connections) */
} kg_sync_filter_t;

/* ============================================================================
 * Data Structures - Federation Configuration
 * ============================================================================ */

/**
 * @brief Federation configuration
 *
 * WHAT: Configuration for federation behavior
 * WHY:  Customize sync policy, conflict handling, and discovery
 * HOW:  Passed to kg_federation_create() to initialize federation
 */
typedef struct {
    char local_id[KG_FEDERATION_PEER_ID_LEN]; /**< This brain's unique ID */

    /* Sync policy */
    kg_sync_policy_t policy;                 /**< Synchronization policy */
    kg_conflict_strategy_t conflict;         /**< Conflict resolution strategy */
    kg_sync_filter_t filter;                 /**< What to sync (selective mode) */

    /* Timing */
    uint32_t sync_interval_ms;               /**< Auto-sync interval (0 = manual only) */
    uint32_t max_sync_batch_size;            /**< Max items per sync batch */
    uint32_t connection_timeout_ms;          /**< Peer connection timeout */
    uint32_t sync_timeout_ms;                /**< Sync operation timeout */

    /* Discovery */
    bool enable_auto_discovery;              /**< Discover peers automatically */
    uint16_t discovery_port;                 /**< mDNS/broadcast port for discovery */
    uint16_t listen_port;                    /**< Port for incoming connections */

    /* Security */
    bool require_tls;                        /**< Require TLS for peer connections */
    bool verify_peer_certs;                  /**< Verify peer TLS certificates */
} kg_federation_config_t;

/* ============================================================================
 * Data Structures - Federated Query Result
 * ============================================================================ */

/**
 * @brief Result from a federated query
 *
 * WHAT: Query result from a single peer
 * WHY:  Track which peer returned data and performance metrics
 * HOW:  Returned as array from kg_federation_query()
 */
typedef struct {
    char source_peer[KG_FEDERATION_PEER_ID_LEN]; /**< Peer that returned this result */
    void* result_data;                       /**< Query result data (caller interprets) */
    size_t result_size;                      /**< Size of result data in bytes */
    uint64_t query_time_ms;                  /**< Query execution time */
    bool is_local;                           /**< Result from local KG */
    bool is_partial;                         /**< Result may be incomplete */
    int error_code;                          /**< 0 = success, else error code */
    char error_msg[128];                     /**< Error message if error_code != 0 */
} kg_federated_result_t;

/* ============================================================================
 * Data Structures - Conflict Information
 * ============================================================================ */

/**
 * @brief Conflict entry for manual resolution
 *
 * WHAT: Details about a detected conflict during sync
 * WHY:  Provide context for manual or automated resolution
 * HOW:  Queued when MANUAL strategy is selected
 */
struct kg_conflict {
    uint64_t conflict_id;                    /**< Unique conflict identifier */
    brain_kg_node_id_t node_id;              /**< Affected node ID */
    char peer_a[KG_FEDERATION_PEER_ID_LEN];  /**< First conflicting peer */
    char peer_b[KG_FEDERATION_PEER_ID_LEN];  /**< Second conflicting peer */
    uint64_t timestamp_a;                    /**< Peer A's change timestamp */
    uint64_t timestamp_b;                    /**< Peer B's change timestamp */
    char field_name[64];                     /**< Field with conflict */
    char value_a[256];                       /**< Value from peer A */
    char value_b[256];                       /**< Value from peer B */
    uint64_t detected_at;                    /**< When conflict was detected */
};

/**
 * @brief Conflict resolution action
 *
 * WHAT: Specifies how to resolve a specific conflict
 * WHY:  Used with kg_federation_resolve_conflict()
 * HOW:  Choose which value to keep or provide merged value
 */
typedef enum {
    KG_RESOLVE_KEEP_A = 0,                   /**< Keep value from peer A */
    KG_RESOLVE_KEEP_B,                       /**< Keep value from peer B */
    KG_RESOLVE_KEEP_LOCAL,                   /**< Keep local value */
    KG_RESOLVE_KEEP_REMOTE,                  /**< Keep remote value */
    KG_RESOLVE_MERGE,                        /**< Use provided merged value */
    KG_RESOLVE_DISCARD                       /**< Discard both, revert to last known good */
} kg_resolve_action_t;

struct kg_conflict_resolution {
    uint64_t conflict_id;                    /**< Conflict being resolved */
    kg_resolve_action_t action;              /**< Resolution action */
    char merged_value[256];                  /**< Merged value (for MERGE action) */
};

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Federation handle (opaque)
 */
typedef struct kg_federation kg_federation_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default federation configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Provide starting point for customization
 * HOW:  Sets bidirectional sync, last-write-wins, 30s interval
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int kg_federation_default_config(kg_federation_config_t* config);

/**
 * @brief Create federation layer for a brain KG
 *
 * WHAT: Initialize federation with given configuration
 * WHY:  Enable distributed synchronization for the brain
 * HOW:  Allocate federation state, start background threads if auto-sync
 *
 * @param kg Brain knowledge graph to federate
 * @param config Federation configuration (NULL for defaults)
 * @return Federation handle or NULL on error
 */
kg_federation_t* kg_federation_create(
    brain_kg_t* kg,
    const kg_federation_config_t* config
);

/**
 * @brief Destroy federation layer
 *
 * WHAT: Clean up federation resources
 * WHY:  Proper resource management
 * HOW:  Disconnect peers, stop threads, free memory
 *
 * @param fed Federation to destroy (NULL safe)
 * @note Does NOT destroy underlying KG
 */
void kg_federation_destroy(kg_federation_t* fed);

/**
 * @brief Get federation configuration
 *
 * @param fed Federation handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int kg_federation_get_config(
    const kg_federation_t* fed,
    kg_federation_config_t* config
);

/**
 * @brief Update federation configuration
 *
 * WHAT: Modify federation settings at runtime
 * WHY:  Adjust sync behavior without recreation
 * HOW:  Apply new config, may restart background tasks
 *
 * @param fed Federation handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int kg_federation_set_config(
    kg_federation_t* fed,
    const kg_federation_config_t* config
);

/* ============================================================================
 * Peer Management API
 * ============================================================================ */

/**
 * @brief Add a peer to the federation
 *
 * WHAT: Register a new peer brain instance
 * WHY:  Enable synchronization with remote brain
 * HOW:  Connect to peer, exchange identities, add to peer list
 *
 * @param fed Federation handle
 * @param host Peer hostname or IP address
 * @param port Peer port number
 * @return 0 on success, -1 on error
 */
int kg_federation_add_peer(
    kg_federation_t* fed,
    const char* host,
    uint16_t port
);

/**
 * @brief Remove a peer from the federation
 *
 * WHAT: Unregister a peer brain instance
 * WHY:  Stop synchronizing with a specific peer
 * HOW:  Disconnect, remove from peer list
 *
 * @param fed Federation handle
 * @param peer_id Peer identifier to remove
 * @return 0 on success, -1 if peer not found
 */
int kg_federation_remove_peer(
    kg_federation_t* fed,
    const char* peer_id
);

/**
 * @brief Get list of federation peers
 *
 * WHAT: Retrieve information about all registered peers
 * WHY:  Monitor federation membership and health
 * HOW:  Copy peer info to caller's array
 *
 * @param fed Federation handle
 * @param peers Output array (caller allocated)
 * @param count In: array capacity, Out: number of peers
 * @return 0 on success, -1 on error
 */
int kg_federation_get_peers(
    const kg_federation_t* fed,
    kg_federation_peer_t* peers,
    uint32_t* count
);

/**
 * @brief Get peer count
 *
 * @param fed Federation handle
 * @return Number of registered peers
 */
uint32_t kg_federation_get_peer_count(const kg_federation_t* fed);

/**
 * @brief Get specific peer information
 *
 * @param fed Federation handle
 * @param peer_id Peer identifier
 * @param peer Output peer information
 * @return 0 on success, -1 if not found
 */
int kg_federation_get_peer(
    const kg_federation_t* fed,
    const char* peer_id,
    kg_federation_peer_t* peer
);

/**
 * @brief Set trust score for a peer
 *
 * WHAT: Adjust peer's trust level
 * WHY:  Trust affects conflict resolution priority
 * HOW:  Update peer's trust_score field
 *
 * @param fed Federation handle
 * @param peer_id Peer identifier
 * @param trust_score Trust level [0.0-1.0]
 * @return 0 on success, -1 if peer not found
 */
int kg_federation_set_trust(
    kg_federation_t* fed,
    const char* peer_id,
    float trust_score
);

/**
 * @brief Check if connected to a specific peer
 *
 * @param fed Federation handle
 * @param peer_id Peer identifier
 * @return true if connected, false otherwise
 */
bool kg_federation_is_peer_connected(
    const kg_federation_t* fed,
    const char* peer_id
);

/* ============================================================================
 * Synchronization API
 * ============================================================================ */

/**
 * @brief Trigger immediate synchronization with all peers
 *
 * WHAT: Start sync operation with all connected peers
 * WHY:  Force synchronization outside auto-sync interval
 * HOW:  Push/pull changes based on policy
 *
 * @param fed Federation handle
 * @return 0 on success, -1 on error
 */
int kg_federation_sync_now(kg_federation_t* fed);

/**
 * @brief Synchronize with a specific peer
 *
 * WHAT: Start sync operation with one peer
 * WHY:  Targeted synchronization
 * HOW:  Exchange changes with specified peer only
 *
 * @param fed Federation handle
 * @param peer_id Peer to synchronize with
 * @return 0 on success, -1 on error
 */
int kg_federation_sync_with_peer(
    kg_federation_t* fed,
    const char* peer_id
);

/**
 * @brief Push local changes to peers
 *
 * WHAT: Send specified changes to all connected peers
 * WHY:  Explicit change propagation
 * HOW:  Serialize and transmit diff to peers
 *
 * @param fed Federation handle
 * @param changes Changes to push (from kg_diff)
 * @return 0 on success, -1 on error
 */
int kg_federation_push_changes(
    kg_federation_t* fed,
    const kg_diff_result_t* changes
);

/**
 * @brief Pull changes from a specific peer
 *
 * WHAT: Request updates from a peer
 * WHY:  Explicitly fetch remote changes
 * HOW:  Request diff from peer, apply locally
 *
 * @param fed Federation handle
 * @param peer_id Peer to pull from
 * @return 0 on success, -1 on error
 */
int kg_federation_pull_changes(
    kg_federation_t* fed,
    const char* peer_id
);

/**
 * @brief Get sync statistics
 *
 * @param fed Federation handle
 * @param total_syncs Output: total sync operations
 * @param successful_syncs Output: successful syncs
 * @param failed_syncs Output: failed syncs
 * @param last_sync_time Output: timestamp of last sync
 * @return 0 on success
 */
int kg_federation_get_sync_stats(
    const kg_federation_t* fed,
    uint64_t* total_syncs,
    uint64_t* successful_syncs,
    uint64_t* failed_syncs,
    uint64_t* last_sync_time
);

/**
 * @brief Start automatic synchronization
 *
 * @param fed Federation handle
 * @return 0 on success, -1 if already running
 */
int kg_federation_start_auto_sync(kg_federation_t* fed);

/**
 * @brief Stop automatic synchronization
 *
 * @param fed Federation handle
 * @return 0 on success
 */
int kg_federation_stop_auto_sync(kg_federation_t* fed);

/**
 * @brief Check if auto-sync is running
 *
 * @param fed Federation handle
 * @return true if auto-sync is active
 */
bool kg_federation_is_auto_sync_running(const kg_federation_t* fed);

/* ============================================================================
 * Federated Query API
 * ============================================================================ */

/**
 * @brief Execute query across all federation peers
 *
 * WHAT: Run query on local KG and all connected peers
 * WHY:  Aggregate knowledge from distributed brains
 * HOW:  Parallel query execution, collect results
 *
 * @param fed Federation handle
 * @param query Query string (format depends on KG query language)
 * @param results Output array of results (caller must free)
 * @param result_count Output: number of results
 * @return 0 on success, -1 on error
 */
int kg_federation_query(
    const kg_federation_t* fed,
    const char* query,
    kg_federated_result_t** results,
    uint32_t* result_count
);

/**
 * @brief Execute query on a specific peer
 *
 * WHAT: Run query on one peer's KG
 * WHY:  Targeted remote query
 * HOW:  Send query to peer, wait for result
 *
 * @param fed Federation handle
 * @param peer_id Target peer
 * @param query Query string
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int kg_federation_query_peer(
    const kg_federation_t* fed,
    const char* peer_id,
    const char* query,
    kg_federated_result_t* result
);

/**
 * @brief Free federated query results
 *
 * @param results Results array to free
 * @param count Number of results
 */
void kg_federation_free_results(
    kg_federated_result_t* results,
    uint32_t count
);

/* ============================================================================
 * Conflict Resolution API
 * ============================================================================ */

/**
 * @brief Get pending conflicts
 *
 * WHAT: Retrieve conflicts awaiting manual resolution
 * WHY:  Review and resolve sync conflicts
 * HOW:  Copy pending conflicts to caller's array
 *
 * @param fed Federation handle
 * @param conflicts Output array (caller allocated)
 * @param count In: array capacity, Out: number of conflicts
 * @return 0 on success, -1 on error
 */
int kg_federation_get_conflicts(
    const kg_federation_t* fed,
    kg_conflict_t* conflicts,
    uint32_t* count
);

/**
 * @brief Get pending conflict count
 *
 * @param fed Federation handle
 * @return Number of unresolved conflicts
 */
uint32_t kg_federation_get_conflict_count(const kg_federation_t* fed);

/**
 * @brief Resolve a specific conflict
 *
 * WHAT: Apply resolution to a pending conflict
 * WHY:  Complete manual conflict resolution
 * HOW:  Apply chosen value, remove from pending list
 *
 * @param fed Federation handle
 * @param conflict_id ID of conflict to resolve
 * @param resolution Resolution action
 * @return 0 on success, -1 if conflict not found
 */
int kg_federation_resolve_conflict(
    kg_federation_t* fed,
    uint64_t conflict_id,
    kg_conflict_resolution_t resolution
);

/**
 * @brief Resolve all conflicts with default strategy
 *
 * WHAT: Auto-resolve all pending conflicts
 * WHY:  Bulk resolution using configured strategy
 * HOW:  Apply conflict_strategy to each pending conflict
 *
 * @param fed Federation handle
 * @return Number of conflicts resolved
 */
uint32_t kg_federation_resolve_all_conflicts(kg_federation_t* fed);

/**
 * @brief Discard a conflict without resolution
 *
 * @param fed Federation handle
 * @param conflict_id Conflict to discard
 * @return 0 on success, -1 if not found
 */
int kg_federation_discard_conflict(
    kg_federation_t* fed,
    uint64_t conflict_id
);

/* ============================================================================
 * Discovery API
 * ============================================================================ */

/**
 * @brief Start peer auto-discovery
 *
 * WHAT: Begin listening for peer announcements
 * WHY:  Automatic federation formation
 * HOW:  mDNS or broadcast discovery protocol
 *
 * @param fed Federation handle
 * @return 0 on success, -1 on error
 */
int kg_federation_start_discovery(kg_federation_t* fed);

/**
 * @brief Stop peer auto-discovery
 *
 * @param fed Federation handle
 * @return 0 on success
 */
int kg_federation_stop_discovery(kg_federation_t* fed);

/**
 * @brief Check if discovery is active
 *
 * @param fed Federation handle
 * @return true if discovery is running
 */
bool kg_federation_is_discovery_active(const kg_federation_t* fed);

/**
 * @brief Announce this brain to the network
 *
 * WHAT: Broadcast presence for peer discovery
 * WHY:  Allow other brains to discover this instance
 * HOW:  Send mDNS or broadcast announcement
 *
 * @param fed Federation handle
 * @return 0 on success, -1 on error
 */
int kg_federation_announce(kg_federation_t* fed);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert sync policy to string
 *
 * @param policy Sync policy enum value
 * @return String representation
 */
const char* kg_sync_policy_to_string(kg_sync_policy_t policy);

/**
 * @brief Convert conflict strategy to string
 *
 * @param strategy Conflict strategy enum value
 * @return String representation
 */
const char* kg_conflict_strategy_to_string(kg_conflict_strategy_t strategy);

/**
 * @brief Convert peer state to string
 *
 * @param state Peer state enum value
 * @return String representation
 */
const char* kg_peer_state_to_string(kg_peer_state_t state);

/**
 * @brief Generate a unique peer ID
 *
 * WHAT: Create a unique identifier for this brain instance
 * WHY:  Unique identification in federation
 * HOW:  UUID v4 generation
 *
 * @param peer_id_out Output buffer (min KG_FEDERATION_PEER_ID_LEN bytes)
 * @return 0 on success, -1 on error
 */
int kg_federation_generate_peer_id(char* peer_id_out);

/**
 * @brief Initialize sync filter with defaults
 *
 * @param filter Filter to initialize
 * @return 0 on success
 */
int kg_sync_filter_init(kg_sync_filter_t* filter);

/**
 * @brief Clean up sync filter resources
 *
 * @param filter Filter to clean up
 */
void kg_sync_filter_cleanup(kg_sync_filter_t* filter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_FEDERATION_H */
