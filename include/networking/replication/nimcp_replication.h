/**
 * @file nimcp_replication.h
 * @brief Brain replication for distributed AI consciousness
 *
 * WHAT: Replicate brain state across multiple Artemis instances
 * WHY: High availability, load balancing, disaster recovery
 * HOW: Redis/PostgreSQL backend + conflict-free replicated data types (CRDTs)
 */

#ifndef NIMCP_REPLICATION_H
#define NIMCP_REPLICATION_H

#include <stdbool.h>
#include <stdint.h>
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Replication backend types
 */
typedef enum {
    REPLICATION_BACKEND_REDIS,      /**< Redis pub/sub + persistence */
    REPLICATION_BACKEND_POSTGRES,   /**< PostgreSQL with NOTIFY/LISTEN */
    REPLICATION_BACKEND_FILESYSTEM, /**< Shared filesystem (NFS, etc.) */
    REPLICATION_BACKEND_CUSTOM      /**< Custom backend */
} replication_backend_t;

/**
 * @brief Replication strategy
 */
typedef enum {
    REPLICATION_STRATEGY_LEADER_FOLLOWER, /**< One writer, many readers */
    REPLICATION_STRATEGY_MULTI_MASTER,    /**< All nodes can write (CRDT) */
    REPLICATION_STRATEGY_EVENTUAL         /**< Eventual consistency */
} replication_strategy_t;

/**
 * @brief Replication configuration
 */
typedef struct {
    replication_backend_t backend;
    replication_strategy_t strategy;

    // Connection settings
    char connection_string[256]; /**< Backend connection (e.g., "redis://localhost:6379") */
    char cluster_name[64];       /**< Cluster identifier */
    char node_id[64];            /**< This node's unique ID */

    // Timing
    uint32_t sync_interval_ms;      /**< How often to sync (0 = on every update) */
    uint32_t heartbeat_interval_ms; /**< Node heartbeat interval */
    uint32_t node_timeout_ms;       /**< Consider node dead after this */

    // Conflict resolution
    bool enable_vector_clock; /**< Use vector clocks for causality */
    bool enable_crdt;         /**< Use CRDTs for conflict-free merging */

    // Bio-async integration
    bool enable_bio_async;    /**< Enable bio-async communication */
} replication_config_t;

/**
 * @brief Opaque replication handle
 */
typedef struct replication_cluster_struct* replication_cluster_t;

/**
 * @brief Node status in cluster
 */
typedef enum {
    NODE_STATUS_LEADER,    /**< Leader node (can write) */
    NODE_STATUS_FOLLOWER,  /**< Follower node (read-only) */
    NODE_STATUS_CANDIDATE, /**< Candidate for leadership */
    NODE_STATUS_DEAD       /**< Node failed/unreachable */
} replication_node_status_t;

/**
 * @brief Cluster node information
 */
typedef struct {
    char node_id[64];
    replication_node_status_t status;
    uint64_t last_heartbeat;
    uint32_t lag_ms;  /**< Replication lag */
    uint64_t version; /**< State version */
} cluster_node_t;

//=============================================================================
// Cluster Management API
//=============================================================================

/**
 * @brief Create replication cluster
 *
 * @param config Replication configuration
 * @return Cluster handle or NULL on error
 */
replication_cluster_t replication_create_cluster(const replication_config_t* config);

/**
 * @brief Destroy replication cluster
 *
 * @param cluster Cluster handle
 */
void replication_destroy_cluster(replication_cluster_t cluster);

/**
 * @brief Register brain with cluster
 *
 * Makes brain state available to other nodes in cluster
 *
 * @param cluster Cluster handle
 * @param brain Brain to replicate
 * @param brain_name Unique name for this brain in cluster
 * @return true on success
 */
bool replication_register_brain(replication_cluster_t cluster, brain_t brain,
                                const char* brain_name);

/**
 * @brief Unregister brain from cluster
 *
 * @param cluster Cluster handle
 * @param brain_name Brain name
 * @return true on success
 */
bool replication_unregister_brain(replication_cluster_t cluster, const char* brain_name);

//=============================================================================
// Synchronization API
//=============================================================================

/**
 * @brief Sync brain state to cluster
 *
 * Pushes local brain state to all replicas
 *
 * @param cluster Cluster handle
 * @param brain_name Brain to sync
 * @return true on success
 */
bool replication_sync_push(replication_cluster_t cluster, const char* brain_name);

/**
 * @brief Sync brain state from cluster
 *
 * Pulls latest brain state from cluster
 *
 * @param cluster Cluster handle
 * @param brain_name Brain to sync
 * @return true on success
 */
bool replication_sync_pull(replication_cluster_t cluster, const char* brain_name);

/**
 * @brief Get brain from cluster
 *
 * Loads brain from any available replica
 *
 * @param cluster Cluster handle
 * @param brain_name Brain name
 * @return Brain handle or NULL if not found
 */
brain_t replication_get_brain(replication_cluster_t cluster, const char* brain_name);

/**
 * @brief Auto-sync brain on every update
 *
 * Automatically pushes changes to cluster after learning
 *
 * @param cluster Cluster handle
 * @param brain_name Brain to auto-sync
 * @param enabled Enable/disable auto-sync
 * @return true on success
 */
bool replication_set_autosync(replication_cluster_t cluster, const char* brain_name, bool enabled);

//=============================================================================
// Cluster Monitoring API
//=============================================================================

/**
 * @brief Get cluster status
 *
 * @param cluster Cluster handle
 * @param nodes Output array of node info
 * @param max_nodes Size of nodes array
 * @return Number of nodes in cluster
 */
uint32_t replication_get_cluster_status(replication_cluster_t cluster, cluster_node_t* nodes,
                                        uint32_t max_nodes);

/**
 * @brief Get this node's status
 *
 * @param cluster Cluster handle
 * @return This node's status
 */
replication_node_status_t replication_get_node_status(replication_cluster_t cluster);

/**
 * @brief Get replication lag
 *
 * @param cluster Cluster handle
 * @param brain_name Brain name
 * @return Lag in milliseconds
 */
uint32_t replication_get_lag(replication_cluster_t cluster, const char* brain_name);

/**
 * @brief Check if cluster is healthy
 *
 * @param cluster Cluster handle
 * @return true if majority of nodes are alive
 */
bool replication_is_healthy(replication_cluster_t cluster);

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Create simple Redis replication cluster
 *
 * @param redis_url Redis connection URL
 * @param cluster_name Cluster name
 * @param node_id This node's ID
 * @return Cluster handle or NULL on error
 */
replication_cluster_t replication_create_redis_cluster(const char* redis_url,
                                                       const char* cluster_name,
                                                       const char* node_id);

/**
 * @brief Create filesystem-based replication
 *
 * Simple replication using shared filesystem (e.g., NFS)
 *
 * @param shared_dir Shared directory path
 * @param node_id This node's ID
 * @return Cluster handle or NULL on error
 */
replication_cluster_t replication_create_filesystem_cluster(const char* shared_dir,
                                                            const char* node_id);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_REPLICATION_H
