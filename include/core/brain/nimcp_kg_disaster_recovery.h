/**
 * @file nimcp_kg_disaster_recovery.h
 * @brief Disaster Recovery & High Availability for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Disaster recovery, replication, and high availability for the brain KG
 * WHY:  Production deployments need data durability, failover, and recovery capabilities
 * HOW:  Multi-replica replication, WAL logging, point-in-time recovery, automatic failover
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                   KG DISASTER RECOVERY ARCHITECTURE                        |
 * +===========================================================================+
 * |                                                                            |
 * |   PRIMARY                          REPLICAS                                |
 * |   +----------------+              +----------------+                       |
 * |   |   brain_kg_t   |  ==Sync==>  |   Replica #1   |   (Hot Standby)       |
 * |   |                |              +----------------+                       |
 * |   |  [WAL Writer]  |  ==Async==> +----------------+                       |
 * |   |       |        |              |   Replica #2   |   (Warm Standby)      |
 * |   |       v        |              +----------------+                       |
 * |   | [WAL Segments] |              +----------------+                       |
 * |   +----------------+              |   Replica #N   |   (Cold Standby)      |
 * |          |                        +----------------+                       |
 * |          v                                                                 |
 * |   +----------------+              +----------------+                       |
 * |   |    BACKUPS     |  <==PITR==> |  Recovery Pt.  |                       |
 * |   | (Full + Incr)  |              |  (Timestamp)   |                       |
 * |   +----------------+              +----------------+                       |
 * |                                                                            |
 * |   RPO: Zero to Hours (configurable)                                        |
 * |   RTO: Immediate to Days (configurable)                                    |
 * +===========================================================================+
 * ```
 *
 * REPLICATION MODES:
 * - SYNC:      All replicas must acknowledge before commit (zero data loss)
 * - SEMI_SYNC: At least one replica must acknowledge (bounded data loss)
 * - ASYNC:     Best effort replication (eventual consistency)
 *
 * RECOVERY OPTIONS:
 * - Hot Standby:  Instant failover, replicas serve read queries
 * - Warm Standby: Fast failover (<5 min), replicas idle
 * - Cold Standby: Manual recovery from backup
 * - PITR:         Point-in-time recovery to any timestamp/transaction
 *
 * THREAD SAFETY: All operations are thread-safe via internal locking
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_DISASTER_RECOVERY_H
#define NIMCP_KG_DISASTER_RECOVERY_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum replicas supported */
#define KG_DR_MAX_REPLICAS              16

/** Maximum backup path length */
#define KG_DR_MAX_PATH_LEN              256

/** Maximum replica ID length */
#define KG_DR_MAX_REPLICA_ID_LEN        64

/** Maximum host name length */
#define KG_DR_MAX_HOST_LEN              256

/** Maximum checkpoint name length */
#define KG_DR_MAX_CHECKPOINT_NAME_LEN   64

/** Maximum backup label length */
#define KG_DR_MAX_BACKUP_LABEL_LEN      64

/** Default WAL segment size (MB) */
#define KG_DR_DEFAULT_WAL_SEGMENT_MB    16

/** Default WAL retention segments */
#define KG_DR_DEFAULT_WAL_RETENTION     32

/** Default health check interval (ms) */
#define KG_DR_DEFAULT_HEALTH_CHECK_MS   1000

/** Default failover timeout (ms) */
#define KG_DR_DEFAULT_FAILOVER_TIMEOUT_MS 5000

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Recovery Point Objective - maximum acceptable data loss
 *
 * Biological basis: Analogous to synaptic consolidation windows.
 * Short-term plasticity (seconds) vs long-term consolidation (hours).
 */
typedef enum {
    KG_RPO_ZERO = 0,                     /**< No data loss (synchronous replication) */
    KG_RPO_SECONDS,                      /**< Up to 60 seconds data loss */
    KG_RPO_MINUTES,                      /**< Up to 5 minutes data loss */
    KG_RPO_HOURS                         /**< Up to 1 hour data loss (async backup) */
} kg_rpo_level_t;

/**
 * @brief Recovery Time Objective - maximum acceptable downtime
 *
 * Biological basis: Similar to neural recovery times after injury.
 * From immediate synaptic switching to long-term neural regeneration.
 */
typedef enum {
    KG_RTO_IMMEDIATE = 0,                /**< Hot standby, instant failover */
    KG_RTO_MINUTES,                      /**< Warm standby, <5 min failover */
    KG_RTO_HOURS,                        /**< Cold standby, <1 hour recovery */
    KG_RTO_DAYS                          /**< Backup restore, may take hours/days */
} kg_rto_level_t;

/**
 * @brief Replication mode
 *
 * Controls the trade-off between data consistency and performance.
 */
typedef enum {
    KG_REPL_NONE = 0,                    /**< No replication (single instance) */
    KG_REPL_ASYNC,                       /**< Asynchronous replication (eventual consistency) */
    KG_REPL_SEMI_SYNC,                   /**< Semi-synchronous (ack from 1+ replica) */
    KG_REPL_SYNC                         /**< Synchronous (all replicas must ack) */
} kg_replication_mode_t;

/**
 * @brief Point-in-time recovery target type
 */
typedef enum {
    KG_PITR_TIMESTAMP = 0,               /**< Recover to specific timestamp */
    KG_PITR_TRANSACTION,                 /**< Recover to specific transaction ID */
    KG_PITR_CHECKPOINT,                  /**< Recover to named checkpoint */
    KG_PITR_LATEST                       /**< Recover to latest available */
} kg_pitr_target_type_t;

/**
 * @brief Backup status
 */
typedef enum {
    KG_BACKUP_STATUS_UNKNOWN = 0,        /**< Status not determined */
    KG_BACKUP_STATUS_IN_PROGRESS,        /**< Backup currently running */
    KG_BACKUP_STATUS_COMPLETED,          /**< Backup completed successfully */
    KG_BACKUP_STATUS_FAILED,             /**< Backup failed */
    KG_BACKUP_STATUS_VERIFIED,           /**< Backup verified after completion */
    KG_BACKUP_STATUS_CORRUPT             /**< Backup found to be corrupted */
} kg_backup_status_t;

/**
 * @brief Replica health status
 */
typedef enum {
    KG_REPLICA_HEALTH_UNKNOWN = 0,       /**< Health not determined */
    KG_REPLICA_HEALTH_HEALTHY,           /**< Replica is healthy and synced */
    KG_REPLICA_HEALTH_LAGGING,           /**< Replica is behind but catching up */
    KG_REPLICA_HEALTH_UNREACHABLE,       /**< Cannot connect to replica */
    KG_REPLICA_HEALTH_DIVERGED,          /**< Replica has diverged from primary */
    KG_REPLICA_HEALTH_FAILED             /**< Replica has failed */
} kg_replica_health_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Replica status information
 *
 * Provides real-time status of a replication replica including
 * health, lag, and synchronization state.
 */
typedef struct {
    char replica_id[KG_DR_MAX_REPLICA_ID_LEN]; /**< Unique replica identifier */
    char host[KG_DR_MAX_HOST_LEN];       /**< Replica host */
    uint16_t port;                       /**< Replica port */
    bool is_primary;                     /**< True if this is the primary */
    bool is_healthy;                     /**< Health check status */
    kg_replica_health_t health_status;   /**< Detailed health status */
    uint64_t lag_ms;                     /**< Replication lag in milliseconds */
    uint64_t last_heartbeat;             /**< Last heartbeat timestamp */
    uint64_t applied_version;            /**< Latest applied transaction version */
} kg_replica_status_t;

/**
 * @brief Point-in-time recovery target specification
 *
 * Specifies the target point for point-in-time recovery.
 * Can target a specific timestamp, transaction ID, or named checkpoint.
 */
typedef struct {
    kg_pitr_target_type_t target_type;   /**< Type of recovery target */
    union {
        uint64_t timestamp;              /**< Target timestamp (microseconds since epoch) */
        uint64_t transaction_id;         /**< Target transaction ID */
        char checkpoint_name[KG_DR_MAX_CHECKPOINT_NAME_LEN]; /**< Target checkpoint name */
    } target;
} kg_pitr_target_t;

/**
 * @brief Backup configuration
 *
 * Configures backup frequency, retention, and storage options.
 */
typedef struct {
    char backup_path[KG_DR_MAX_PATH_LEN]; /**< Local backup directory */
    char remote_path[KG_DR_MAX_PATH_LEN]; /**< Remote backup location (S3, GCS, etc.) */
    uint32_t full_backup_interval_hours; /**< Full backup frequency */
    uint32_t incremental_interval_min;   /**< Incremental backup frequency */
    uint32_t retention_days;             /**< How long to keep backups */
    bool encrypt_backups;                /**< Encrypt backup files */
    bool compress_backups;               /**< Compress backup files */
    bool verify_after_backup;            /**< Verify backup integrity after creation */
} kg_backup_config_t;

/**
 * @brief Backup information
 *
 * Metadata about a completed backup for listing and selection.
 */
typedef struct {
    char backup_id[KG_DR_MAX_BACKUP_LABEL_LEN]; /**< Unique backup identifier */
    char label[KG_DR_MAX_BACKUP_LABEL_LEN];     /**< User-provided label */
    uint64_t timestamp;                  /**< Backup creation timestamp */
    uint64_t size_bytes;                 /**< Backup size in bytes */
    bool is_full;                        /**< True if full backup, false if incremental */
    bool is_encrypted;                   /**< Whether backup is encrypted */
    bool is_compressed;                  /**< Whether backup is compressed */
    kg_backup_status_t status;           /**< Backup status */
    uint64_t transaction_id;             /**< Transaction ID at backup time */
} kg_backup_info_t;

/**
 * @brief Disaster recovery configuration
 *
 * Complete configuration for the DR system including replication,
 * backup, failover, and WAL settings.
 */
typedef struct {
    kg_rpo_level_t rpo;                  /**< Recovery point objective */
    kg_rto_level_t rto;                  /**< Recovery time objective */
    kg_replication_mode_t replication;   /**< Replication mode */
    kg_backup_config_t backup;           /**< Backup settings */

    /* Replica configuration */
    kg_replica_status_t* replicas;       /**< Array of replicas (can be NULL) */
    uint32_t replica_count;              /**< Number of replicas */
    uint32_t min_replicas_for_write;     /**< Minimum replicas for write quorum */

    /* Failover settings */
    bool enable_auto_failover;           /**< Automatic failover on primary failure */
    uint32_t failover_timeout_ms;        /**< Time before triggering failover */
    uint32_t health_check_interval_ms;   /**< Health check frequency */

    /* WAL (Write-Ahead Log) settings */
    bool enable_wal;                     /**< Enable WAL for durability */
    uint32_t wal_segment_size_mb;        /**< WAL segment size */
    uint32_t wal_retention_segments;     /**< WAL segments to retain */
} kg_dr_config_t;

/**
 * @brief Disaster recovery context handle (opaque)
 */
typedef struct kg_dr_context kg_dr_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default DR configuration
 *
 * WHAT: Initialize a DR configuration with sensible defaults
 * WHY:  Simplify setup with production-ready defaults
 * HOW:  Sets RPO_MINUTES, RTO_MINUTES, ASYNC replication, WAL enabled
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int kg_dr_default_config(kg_dr_config_t* config);

/**
 * @brief Create disaster recovery context
 *
 * WHAT: Initialize DR system for a brain KG
 * WHY:  Enable replication, backup, and recovery capabilities
 * HOW:  Allocates context, initializes WAL, connects replicas
 *
 * @param kg Brain knowledge graph to protect
 * @param config DR configuration (NULL for defaults)
 * @return New DR context or NULL on error
 */
kg_dr_context_t* kg_dr_create(brain_kg_t* kg, const kg_dr_config_t* config);

/**
 * @brief Destroy disaster recovery context
 *
 * WHAT: Clean up DR resources
 * WHY:  Release memory and close connections
 * HOW:  Flushes pending writes, disconnects replicas, frees memory
 *
 * @param dr DR context to destroy (NULL safe)
 */
void kg_dr_destroy(kg_dr_context_t* dr);

/* ============================================================================
 * Replication Management API
 * ============================================================================ */

/**
 * @brief Add a replica to the replication set
 *
 * WHAT: Register a new replica for replication
 * WHY:  Increase availability and read capacity
 * HOW:  Connect to replica, initiate synchronization
 *
 * @param dr DR context
 * @param host Replica hostname or IP
 * @param port Replica port
 * @return 0 on success, -1 on error
 */
int kg_dr_add_replica(kg_dr_context_t* dr, const char* host, uint16_t port);

/**
 * @brief Remove a replica from the replication set
 *
 * WHAT: Deregister and disconnect a replica
 * WHY:  Decommission or replace failed replica
 * HOW:  Disconnect cleanly, remove from replica list
 *
 * @param dr DR context
 * @param replica_id ID of replica to remove
 * @return 0 on success, -1 if not found
 */
int kg_dr_remove_replica(kg_dr_context_t* dr, const char* replica_id);

/**
 * @brief Get status of all replicas
 *
 * WHAT: Retrieve current status of all replicas
 * WHY:  Monitor replication health and lag
 * HOW:  Query each replica, aggregate status
 *
 * @param dr DR context
 * @param status Output array (caller allocated)
 * @param count Input: array capacity; Output: number of replicas
 * @return 0 on success, -1 on error
 */
int kg_dr_get_replica_status(const kg_dr_context_t* dr, kg_replica_status_t* status, uint32_t* count);

/**
 * @brief Promote a replica to primary
 *
 * WHAT: Manually promote a replica to become the new primary
 * WHY:  Planned failover or maintenance
 * HOW:  Stop writes to old primary, promote replica, redirect traffic
 *
 * @param dr DR context
 * @param replica_id ID of replica to promote
 * @return 0 on success, -1 on error
 */
int kg_dr_promote_replica(kg_dr_context_t* dr, const char* replica_id);

/* ============================================================================
 * Backup Operations API
 * ============================================================================ */

/**
 * @brief Perform a full backup
 *
 * WHAT: Create a complete backup of the KG
 * WHY:  Establish recovery baseline
 * HOW:  Snapshot all data, optionally compress and encrypt
 *
 * @param dr DR context
 * @param label User-provided label for the backup
 * @return 0 on success, -1 on error
 */
int kg_dr_backup_full(kg_dr_context_t* dr, const char* label);

/**
 * @brief Perform an incremental backup
 *
 * WHAT: Backup changes since last backup
 * WHY:  Efficient storage, faster backup
 * HOW:  Capture WAL segments since last backup
 *
 * @param dr DR context
 * @return 0 on success, -1 on error
 */
int kg_dr_backup_incremental(kg_dr_context_t* dr);

/**
 * @brief List available backups
 *
 * WHAT: Get list of all available backups
 * WHY:  Select backup for restore
 * HOW:  Scan backup directory, parse metadata
 *
 * @param dr DR context
 * @param backups Output array (caller allocated)
 * @param count Input: array capacity; Output: number of backups
 * @return 0 on success, -1 on error
 */
int kg_dr_list_backups(const kg_dr_context_t* dr, kg_backup_info_t* backups, uint32_t* count);

/**
 * @brief Verify backup integrity
 *
 * WHAT: Check backup for corruption
 * WHY:  Ensure backup is usable before disaster
 * HOW:  Verify checksums, test restore to temp location
 *
 * @param dr DR context
 * @param backup_id Backup to verify
 * @return 0 if valid, -1 if corrupted or not found
 */
int kg_dr_verify_backup(const kg_dr_context_t* dr, const char* backup_id);

/**
 * @brief Delete a backup
 *
 * WHAT: Remove a backup from storage
 * WHY:  Free storage space, enforce retention policy
 * HOW:  Delete backup files and metadata
 *
 * @param dr DR context
 * @param backup_id Backup to delete
 * @return 0 on success, -1 if not found
 */
int kg_dr_delete_backup(kg_dr_context_t* dr, const char* backup_id);

/* ============================================================================
 * Point-in-Time Recovery API
 * ============================================================================ */

/**
 * @brief Perform point-in-time recovery
 *
 * WHAT: Restore KG to a specific point in time
 * WHY:  Recover from data corruption or accidental deletion
 * HOW:  Restore from backup, replay WAL to target point
 *
 * @param dr DR context
 * @param target Recovery target specification
 * @return 0 on success, -1 on error
 */
int kg_dr_pitr_recover(kg_dr_context_t* dr, const kg_pitr_target_t* target);

/**
 * @brief List available recovery points
 *
 * WHAT: Get list of timestamps to which recovery is possible
 * WHY:  Help user choose recovery target
 * HOW:  Analyze WAL segments and backup timestamps
 *
 * @param dr DR context
 * @param timestamps Output array (caller allocated)
 * @param count Input: array capacity; Output: number of timestamps
 * @return 0 on success, -1 on error
 */
int kg_dr_pitr_list_recovery_points(const kg_dr_context_t* dr, uint64_t* timestamps, uint32_t* count);

/**
 * @brief Estimate recovery time for a target
 *
 * WHAT: Calculate expected recovery duration
 * WHY:  Plan recovery operations
 * HOW:  Estimate based on data size and WAL segments to replay
 *
 * @param dr DR context
 * @param target Recovery target
 * @param est_seconds Output: estimated seconds for recovery
 * @return 0 on success, -1 on error
 */
int kg_dr_pitr_estimate_recovery_time(const kg_dr_context_t* dr, const kg_pitr_target_t* target, uint32_t* est_seconds);

/**
 * @brief Create a named checkpoint
 *
 * WHAT: Create a named recovery point
 * WHY:  Easy reference for recovery (e.g., "before_migration")
 * HOW:  Record current transaction ID with checkpoint name
 *
 * @param dr DR context
 * @param checkpoint_name Name for the checkpoint
 * @return 0 on success, -1 on error
 */
int kg_dr_pitr_create_checkpoint(kg_dr_context_t* dr, const char* checkpoint_name);

/* ============================================================================
 * Failover API
 * ============================================================================ */

/**
 * @brief Trigger failover to a specific replica
 *
 * WHAT: Initiate failover to a designated replica
 * WHY:  Handle primary failure or planned switchover
 * HOW:  Verify replica is suitable, promote, redirect traffic
 *
 * @param dr DR context
 * @param new_primary_id ID of replica to become primary (NULL for automatic)
 * @return 0 on success, -1 on error
 */
int kg_dr_trigger_failover(kg_dr_context_t* dr, const char* new_primary_id);

/**
 * @brief Failback to original primary
 *
 * WHAT: Return to original primary after recovery
 * WHY:  Restore preferred topology after incident
 * HOW:  Sync original primary, promote, demote temporary primary
 *
 * @param dr DR context
 * @param original_primary_id ID of original primary
 * @return 0 on success, -1 on error
 */
int kg_dr_failback(kg_dr_context_t* dr, const char* original_primary_id);

/**
 * @brief Check if this instance is the primary
 *
 * WHAT: Determine primary/replica status
 * WHY:  Route writes appropriately
 * HOW:  Check internal state flag
 *
 * @param dr DR context
 * @return true if primary, false if replica
 */
bool kg_dr_is_primary(const kg_dr_context_t* dr);

/**
 * @brief Get current primary replica ID
 *
 * WHAT: Identify the current primary
 * WHY:  Direct writes to correct instance
 * HOW:  Return primary's replica ID
 *
 * @param dr DR context
 * @param primary_id Output buffer for primary ID
 * @param buffer_size Size of output buffer
 * @return 0 on success, -1 if no primary
 */
int kg_dr_get_primary_id(const kg_dr_context_t* dr, char* primary_id, size_t buffer_size);

/* ============================================================================
 * Health Monitoring API
 * ============================================================================ */

/**
 * @brief Perform health check on DR system
 *
 * WHAT: Check health of all DR components
 * WHY:  Early detection of issues
 * HOW:  Ping replicas, verify WAL, check backup status
 *
 * @param dr DR context
 * @return 0 if healthy, -1 if issues detected
 */
int kg_dr_health_check(kg_dr_context_t* dr);

/**
 * @brief Get maximum replication lag
 *
 * WHAT: Get the worst replication lag across all replicas
 * WHY:  Monitor data consistency guarantees
 * HOW:  Query all replicas, return maximum lag
 *
 * @param dr DR context
 * @return Maximum lag in milliseconds, or -1.0f on error
 */
float kg_dr_get_replication_lag(const kg_dr_context_t* dr);

/**
 * @brief Get DR system statistics
 *
 * WHAT: Retrieve DR operational statistics
 * WHY:  Monitor DR system performance
 * HOW:  Aggregate stats from all components
 *
 * @param dr DR context
 * @param total_replicas Output: number of replicas
 * @param healthy_replicas Output: number of healthy replicas
 * @param pending_wal_bytes Output: WAL bytes awaiting replication
 * @return 0 on success, -1 on error
 */
int kg_dr_get_stats(const kg_dr_context_t* dr, uint32_t* total_replicas,
                    uint32_t* healthy_replicas, uint64_t* pending_wal_bytes);

/**
 * @brief Register health change callback
 *
 * WHAT: Register callback for health status changes
 * WHY:  Enable proactive alerting
 * HOW:  Callback invoked when replica health changes
 *
 * @param dr DR context
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
typedef void (*kg_dr_health_callback_fn)(const char* replica_id,
                                          kg_replica_health_t old_health,
                                          kg_replica_health_t new_health,
                                          void* user_data);

int kg_dr_register_health_callback(kg_dr_context_t* dr,
                                   kg_dr_health_callback_fn callback,
                                   void* user_data);

/* ============================================================================
 * WAL Management API
 * ============================================================================ */

/**
 * @brief Force WAL flush
 *
 * WHAT: Flush all pending WAL entries to disk
 * WHY:  Ensure durability before critical operations
 * HOW:  Write buffered WAL to segment, fsync
 *
 * @param dr DR context
 * @return 0 on success, -1 on error
 */
int kg_dr_wal_flush(kg_dr_context_t* dr);

/**
 * @brief Get current WAL position
 *
 * WHAT: Get the current WAL write position
 * WHY:  Monitor WAL growth, coordinate replication
 * HOW:  Return current segment and offset
 *
 * @param dr DR context
 * @param segment Output: current segment number
 * @param offset Output: offset within segment
 * @return 0 on success, -1 on error
 */
int kg_dr_wal_position(const kg_dr_context_t* dr, uint64_t* segment, uint64_t* offset);

/**
 * @brief Archive old WAL segments
 *
 * WHAT: Move old WAL segments to archive storage
 * WHY:  Free local storage while preserving for PITR
 * HOW:  Copy segments to archive, remove local copies
 *
 * @param dr DR context
 * @return Number of segments archived, -1 on error
 */
int kg_dr_wal_archive(kg_dr_context_t* dr);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert RPO level to string
 */
const char* kg_rpo_level_to_string(kg_rpo_level_t level);

/**
 * @brief Convert RTO level to string
 */
const char* kg_rto_level_to_string(kg_rto_level_t level);

/**
 * @brief Convert replication mode to string
 */
const char* kg_replication_mode_to_string(kg_replication_mode_t mode);

/**
 * @brief Convert backup status to string
 */
const char* kg_backup_status_to_string(kg_backup_status_t status);

/**
 * @brief Convert replica health to string
 */
const char* kg_replica_health_to_string(kg_replica_health_t health);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_DISASTER_RECOVERY_H */
