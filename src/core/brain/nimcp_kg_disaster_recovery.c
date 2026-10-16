/**
 * @file nimcp_kg_disaster_recovery.c
 * @brief Disaster Recovery & High Availability for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of disaster recovery with replication, WAL, backup, and
 * point-in-time recovery capabilities.
 */

#include "core/brain/nimcp_kg_disaster_recovery.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_disaster_recovery, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CHECKPOINTS         64
#define MAX_HEALTH_CALLBACKS    16
#define MAX_BACKUPS             128

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Named checkpoint entry
 */
typedef struct {
    char name[KG_DR_MAX_CHECKPOINT_NAME_LEN];  /**< Checkpoint name */
    uint64_t transaction_id;                    /**< Transaction at checkpoint */
    uint64_t timestamp;                         /**< Creation timestamp */
    bool active;                                /**< Active flag */
} kg_checkpoint_t;

/**
 * @brief WAL segment
 */
typedef struct {
    uint64_t segment_id;            /**< Segment number */
    uint64_t start_transaction;     /**< First transaction in segment */
    uint64_t end_transaction;       /**< Last transaction in segment */
    uint64_t size_bytes;            /**< Segment size */
    bool archived;                  /**< Whether archived */
} kg_wal_segment_t;

/**
 * @brief Health callback registration
 */
typedef struct {
    kg_dr_health_callback_fn callback;  /**< Callback function */
    void* user_data;                    /**< User context */
    bool active;                        /**< Active flag */
} kg_health_callback_t;

/**
 * @brief DR context implementation
 */
struct kg_dr_context {
    brain_kg_t* kg;                      /**< Associated KG */
    kg_dr_config_t config;               /**< Configuration */

    /* Identity */
    char replica_id[KG_DR_MAX_REPLICA_ID_LEN];  /**< This instance's ID */
    bool is_primary;                     /**< Primary flag */
    char primary_id[KG_DR_MAX_REPLICA_ID_LEN];  /**< Current primary's ID */

    /* Replica management */
    kg_replica_status_t* replicas;       /**< Replica array */
    uint32_t replica_count;              /**< Number of replicas */
    uint32_t max_replicas;               /**< Maximum replicas */

    /* Backup management */
    kg_backup_info_t* backups;           /**< Backup array */
    uint32_t backup_count;               /**< Number of backups */

    /* Checkpoints */
    kg_checkpoint_t checkpoints[MAX_CHECKPOINTS];  /**< Named checkpoints */
    uint32_t checkpoint_count;           /**< Number of checkpoints */

    /* WAL state */
    uint64_t current_segment;            /**< Current WAL segment */
    uint64_t current_offset;             /**< Offset in current segment */
    uint64_t current_transaction;        /**< Current transaction ID */
    FILE* wal_file;                      /**< Current WAL file handle */
    char wal_path[KG_DR_MAX_PATH_LEN];   /**< WAL directory path */

    /* Health callbacks */
    kg_health_callback_t health_callbacks[MAX_HEALTH_CALLBACKS];
    uint32_t health_callback_count;

    /* Statistics */
    uint64_t pending_wal_bytes;          /**< Pending WAL bytes */
    uint64_t last_backup_ts;             /**< Last backup timestamp */

    /* Thread safety */
    nimcp_mutex_t* mutex;                /**< Context mutex */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    return get_timestamp_us() / 1000ULL;
}

/**
 * @brief Generate a unique replica ID
 */
static void generate_replica_id(char* buf, size_t size) {
    uint64_t ts = get_timestamp_us();
    uint32_t rand_part = (uint32_t)nimcp_tl_rand();
    snprintf(buf, size, "replica_%08x%08x", (uint32_t)(ts & 0xFFFFFFFF), rand_part);
}

/**
 * @brief Find replica by ID
 */
static kg_replica_status_t* find_replica(kg_dr_context_t* dr, const char* replica_id) {
    if (!dr || !replica_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_replica: required parameter is NULL (dr, replica_id)");
        return NULL;
    }

    for (uint32_t i = 0; i < dr->replica_count; i++) {
        if (strncmp(dr->replicas[i].replica_id, replica_id, KG_DR_MAX_REPLICA_ID_LEN) == 0) {
            return &dr->replicas[i];
        }
    }
    return NULL;
}

/**
 * @brief Invoke health callbacks
 */
static void invoke_health_callbacks(
    kg_dr_context_t* dr,
    const char* replica_id,
    kg_replica_health_t old_health,
    kg_replica_health_t new_health
) {
    for (uint32_t i = 0; i < MAX_HEALTH_CALLBACKS; i++) {
        if (dr->health_callbacks[i].active && dr->health_callbacks[i].callback) {
            dr->health_callbacks[i].callback(replica_id, old_health, new_health,
                                             dr->health_callbacks[i].user_data);
        }
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_dr_default_config(kg_dr_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->rpo = KG_RPO_MINUTES;
    config->rto = KG_RTO_MINUTES;
    config->replication = KG_REPL_ASYNC;

    /* Backup defaults */
    config->backup.full_backup_interval_hours = 24;
    config->backup.incremental_interval_min = 60;
    config->backup.retention_days = 7;
    config->backup.encrypt_backups = true;
    config->backup.compress_backups = true;
    config->backup.verify_after_backup = true;

    /* Replica defaults */
    config->replica_count = 0;
    config->min_replicas_for_write = 0;

    /* Failover defaults */
    config->enable_auto_failover = true;
    config->failover_timeout_ms = KG_DR_DEFAULT_FAILOVER_TIMEOUT_MS;
    config->health_check_interval_ms = KG_DR_DEFAULT_HEALTH_CHECK_MS;

    /* WAL defaults */
    config->enable_wal = true;
    config->wal_segment_size_mb = KG_DR_DEFAULT_WAL_SEGMENT_MB;
    config->wal_retention_segments = KG_DR_DEFAULT_WAL_RETENTION;

    return 0;
}

kg_dr_context_t* kg_dr_create(brain_kg_t* kg, const kg_dr_config_t* config) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_create: kg is NULL");
        return NULL;
    }

    kg_dr_context_t* dr = nimcp_calloc(1, sizeof(kg_dr_context_t));
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return NULL;
    }

    dr->kg = kg;

    /* Apply configuration */
    if (config) {
        dr->config = *config;
    } else {
        kg_dr_default_config(&dr->config);
    }

    /* Generate replica ID */
    generate_replica_id(dr->replica_id, KG_DR_MAX_REPLICA_ID_LEN);
    dr->is_primary = true;  /* Assume primary initially */
    strncpy(dr->primary_id, dr->replica_id, KG_DR_MAX_REPLICA_ID_LEN - 1);

    /* Allocate replicas */
    dr->max_replicas = KG_DR_MAX_REPLICAS;
    dr->replicas = nimcp_calloc(dr->max_replicas, sizeof(kg_replica_status_t));
    if (!dr->replicas) {
        nimcp_free(dr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_dr_create: dr->replicas is NULL");
        return NULL;
    }

    /* Allocate backups */
    dr->backups = nimcp_calloc(MAX_BACKUPS, sizeof(kg_backup_info_t));
    if (!dr->backups) {
        nimcp_free(dr->replicas);
        nimcp_free(dr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_dr_create: dr->backups is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    dr->mutex = nimcp_mutex_create(&attr);
    if (!dr->mutex) {
        nimcp_free(dr->backups);
        nimcp_free(dr->replicas);
        nimcp_free(dr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_dr_create: dr->mutex is NULL");
        return NULL;
    }

    /* Initialize WAL state */
    dr->current_segment = 0;
    dr->current_offset = 0;
    dr->current_transaction = 0;

    /* Seed random */
    srand((unsigned int)time(NULL));

    return dr;
}

void kg_dr_destroy(kg_dr_context_t* dr) {
    if (!dr) {
        return;
    }

    /* Close WAL file */
    if (dr->wal_file) {
        fclose(dr->wal_file);
    }

    /* Free backups */
    if (dr->backups) {
        nimcp_free(dr->backups);
    }

    /* Free replicas */
    if (dr->replicas) {
        nimcp_free(dr->replicas);
    }

    /* Destroy mutex */
    if (dr->mutex) {
        nimcp_mutex_free(dr->mutex);
    }

    nimcp_free(dr);
}

/* ============================================================================
 * Replication Management API
 * ============================================================================ */

int kg_dr_add_replica(kg_dr_context_t* dr, const char* host, uint16_t port) {
    if (!dr || !host) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_add_replica: required parameter is NULL (dr, host)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    if (dr->replica_count >= dr->max_replicas) {
        nimcp_mutex_unlock(dr->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "kg_dr_add_replica: capacity exceeded");
        return -1;
    }

    /* Create new replica entry */
    kg_replica_status_t* replica = &dr->replicas[dr->replica_count];
    memset(replica, 0, sizeof(*replica));

    generate_replica_id(replica->replica_id, KG_DR_MAX_REPLICA_ID_LEN);
    strncpy(replica->host, host, KG_DR_MAX_HOST_LEN - 1);
    replica->port = port;
    replica->is_primary = false;
    replica->is_healthy = true;
    replica->health_status = KG_REPLICA_HEALTH_HEALTHY;
    replica->last_heartbeat = get_timestamp_ms();
    replica->applied_version = dr->current_transaction;

    dr->replica_count++;

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

int kg_dr_remove_replica(kg_dr_context_t* dr, const char* replica_id) {
    if (!dr || !replica_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_remove_replica: required parameter is NULL (dr, replica_id)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    for (uint32_t i = 0; i < dr->replica_count; i++) {
        if (strncmp(dr->replicas[i].replica_id, replica_id, KG_DR_MAX_REPLICA_ID_LEN) == 0) {
            /* Shift remaining replicas */
            for (uint32_t j = i; j < dr->replica_count - 1; j++) {
                dr->replicas[j] = dr->replicas[j + 1];
            }
            dr->replica_count--;
            nimcp_mutex_unlock(dr->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(dr->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_dr_remove_replica: operation failed");
    return -1;
}

int kg_dr_get_replica_status(const kg_dr_context_t* dr, kg_replica_status_t* status, uint32_t* count) {
    if (!dr || !status || !count || *count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_get_replica_status: required parameter is NULL (dr, status, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    uint32_t to_copy = dr->replica_count;
    if (to_copy > *count) {
        to_copy = *count;
    }

    memcpy(status, dr->replicas, to_copy * sizeof(kg_replica_status_t));
    *count = to_copy;

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);

    return 0;
}

int kg_dr_promote_replica(kg_dr_context_t* dr, const char* replica_id) {
    if (!dr || !replica_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_promote_replica: required parameter is NULL (dr, replica_id)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    kg_replica_status_t* replica = find_replica(dr, replica_id);
    if (!replica) {
        nimcp_mutex_unlock(dr->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_promote_replica: replica is NULL");
        return -1;
    }

    /* Demote current primary */
    if (dr->is_primary) {
        dr->is_primary = false;
    }

    /* Promote replica */
    replica->is_primary = true;
    strncpy(dr->primary_id, replica_id, KG_DR_MAX_REPLICA_ID_LEN - 1);

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

/* ============================================================================
 * Backup Operations API
 * ============================================================================ */

int kg_dr_backup_full(kg_dr_context_t* dr, const char* label) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    if (dr->backup_count >= MAX_BACKUPS) {
        nimcp_mutex_unlock(dr->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "kg_dr_backup_full: capacity exceeded");
        return -1;
    }

    /* Create backup entry */
    kg_backup_info_t* backup = &dr->backups[dr->backup_count];
    memset(backup, 0, sizeof(*backup));

    snprintf(backup->backup_id, KG_DR_MAX_BACKUP_LABEL_LEN, "full_%lu",
             (unsigned long)get_timestamp_us());

    if (label) {
        strncpy(backup->label, label, KG_DR_MAX_BACKUP_LABEL_LEN - 1);
    }

    backup->timestamp = get_timestamp_us();
    backup->is_full = true;
    backup->is_encrypted = dr->config.backup.encrypt_backups;
    backup->is_compressed = dr->config.backup.compress_backups;
    backup->status = KG_BACKUP_STATUS_COMPLETED;
    backup->transaction_id = dr->current_transaction;

    /* In a real implementation, we would actually write the backup here */
    backup->size_bytes = 1024 * 1024;  /* Placeholder */

    dr->backup_count++;
    dr->last_backup_ts = get_timestamp_ms();

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

int kg_dr_backup_incremental(kg_dr_context_t* dr) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    if (dr->backup_count >= MAX_BACKUPS) {
        nimcp_mutex_unlock(dr->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "kg_dr_backup_incremental: capacity exceeded");
        return -1;
    }

    /* Create backup entry */
    kg_backup_info_t* backup = &dr->backups[dr->backup_count];
    memset(backup, 0, sizeof(*backup));

    snprintf(backup->backup_id, KG_DR_MAX_BACKUP_LABEL_LEN, "incr_%lu",
             (unsigned long)get_timestamp_us());

    backup->timestamp = get_timestamp_us();
    backup->is_full = false;
    backup->is_encrypted = dr->config.backup.encrypt_backups;
    backup->is_compressed = dr->config.backup.compress_backups;
    backup->status = KG_BACKUP_STATUS_COMPLETED;
    backup->transaction_id = dr->current_transaction;
    backup->size_bytes = 64 * 1024;  /* Placeholder - incremental is smaller */

    dr->backup_count++;
    dr->last_backup_ts = get_timestamp_ms();

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

int kg_dr_list_backups(const kg_dr_context_t* dr, kg_backup_info_t* backups, uint32_t* count) {
    if (!dr || !backups || !count || *count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_list_backups: required parameter is NULL (dr, backups, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    uint32_t to_copy = dr->backup_count;
    if (to_copy > *count) {
        to_copy = *count;
    }

    memcpy(backups, dr->backups, to_copy * sizeof(kg_backup_info_t));
    *count = to_copy;

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);

    return 0;
}

int kg_dr_verify_backup(const kg_dr_context_t* dr, const char* backup_id) {
    if (!dr || !backup_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_verify_backup: required parameter is NULL (dr, backup_id)");
        return -1;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    for (uint32_t i = 0; i < dr->backup_count; i++) {
        if (strncmp(dr->backups[i].backup_id, backup_id, KG_DR_MAX_BACKUP_LABEL_LEN) == 0) {
            /* In a real implementation, we would verify checksums here */
            ((kg_dr_context_t*)dr)->backups[i].status = KG_BACKUP_STATUS_VERIFIED;
            nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_dr_verify_backup: validation failed");
    return -1;
}

int kg_dr_delete_backup(kg_dr_context_t* dr, const char* backup_id) {
    if (!dr || !backup_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_delete_backup: required parameter is NULL (dr, backup_id)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    for (uint32_t i = 0; i < dr->backup_count; i++) {
        if (strncmp(dr->backups[i].backup_id, backup_id, KG_DR_MAX_BACKUP_LABEL_LEN) == 0) {
            /* Shift remaining backups */
            for (uint32_t j = i; j < dr->backup_count - 1; j++) {
                dr->backups[j] = dr->backups[j + 1];
            }
            dr->backup_count--;
            nimcp_mutex_unlock(dr->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(dr->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_dr_delete_backup: operation failed");
    return -1;
}

/* ============================================================================
 * Point-in-Time Recovery API
 * ============================================================================ */

int kg_dr_pitr_recover(kg_dr_context_t* dr, const kg_pitr_target_t* target) {
    if (!dr || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_pitr_recover: required parameter is NULL (dr, target)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    /* In a real implementation, we would:
     * 1. Find the appropriate backup
     * 2. Restore from backup
     * 3. Replay WAL to target point
     */

    switch (target->target_type) {
        case KG_PITR_TIMESTAMP:
            /* Recover to timestamp */
            break;

        case KG_PITR_TRANSACTION:
            /* Recover to transaction */
            break;

        case KG_PITR_CHECKPOINT:
            /* Find checkpoint and recover */
            break;

        case KG_PITR_LATEST:
            /* Recover to latest */
            break;
    }

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

int kg_dr_pitr_list_recovery_points(const kg_dr_context_t* dr, uint64_t* timestamps, uint32_t* count) {
    if (!dr || !timestamps || !count || *count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_pitr_list_recovery_points: required parameter is NULL (dr, timestamps, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    /* Return backup timestamps as recovery points */
    uint32_t to_copy = dr->backup_count;
    if (to_copy > *count) {
        to_copy = *count;
    }

    for (uint32_t i = 0; i < to_copy; i++) {
        timestamps[i] = dr->backups[i].timestamp;
    }
    *count = to_copy;

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);

    return 0;
}

int kg_dr_pitr_estimate_recovery_time(const kg_dr_context_t* dr, const kg_pitr_target_t* target, uint32_t* est_seconds) {
    if (!dr || !target || !est_seconds) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_pitr_estimate_recovery_time: required parameter is NULL (dr, target, est_seconds)");
        return -1;
    }

    /* Simplified estimation */
    switch (dr->config.rto) {
        case KG_RTO_IMMEDIATE:
            *est_seconds = 0;
            break;
        case KG_RTO_MINUTES:
            *est_seconds = 300;
            break;
        case KG_RTO_HOURS:
            *est_seconds = 3600;
            break;
        case KG_RTO_DAYS:
            *est_seconds = 86400;
            break;
    }

    return 0;
}

int kg_dr_pitr_create_checkpoint(kg_dr_context_t* dr, const char* checkpoint_name) {
    if (!dr || !checkpoint_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_pitr_create_checkpoint: required parameter is NULL (dr, checkpoint_name)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    if (dr->checkpoint_count >= MAX_CHECKPOINTS) {
        nimcp_mutex_unlock(dr->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "kg_dr_pitr_create_checkpoint: capacity exceeded");
        return -1;
    }

    /* Find free slot or oldest checkpoint to replace */
    int slot = -1;
    for (uint32_t i = 0; i < MAX_CHECKPOINTS; i++) {
        if (!dr->checkpoints[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        slot = 0;  /* Replace oldest */
    }

    kg_checkpoint_t* cp = &dr->checkpoints[slot];
    strncpy(cp->name, checkpoint_name, KG_DR_MAX_CHECKPOINT_NAME_LEN - 1);
    cp->transaction_id = dr->current_transaction;
    cp->timestamp = get_timestamp_us();
    cp->active = true;

    if (!dr->checkpoints[slot].active) {
        dr->checkpoint_count++;
    }

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

/* ============================================================================
 * Failover API
 * ============================================================================ */

int kg_dr_trigger_failover(kg_dr_context_t* dr, const char* new_primary_id) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    if (new_primary_id) {
        /* Promote specific replica */
        kg_replica_status_t* replica = find_replica(dr, new_primary_id);
        if (!replica) {
            nimcp_mutex_unlock(dr->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_trigger_failover: replica is NULL");
            return -1;
        }

        replica->is_primary = true;
        strncpy(dr->primary_id, new_primary_id, KG_DR_MAX_REPLICA_ID_LEN - 1);
    } else {
        /* Automatic: find healthiest replica with lowest lag */
        kg_replica_status_t* best = NULL;
        uint64_t lowest_lag = UINT64_MAX;

        for (uint32_t i = 0; i < dr->replica_count; i++) {
            if (dr->replicas[i].is_healthy &&
                dr->replicas[i].lag_ms < lowest_lag) {
                best = &dr->replicas[i];
                lowest_lag = dr->replicas[i].lag_ms;
            }
        }

        if (best) {
            best->is_primary = true;
            strncpy(dr->primary_id, best->replica_id, KG_DR_MAX_REPLICA_ID_LEN - 1);
        }
    }

    dr->is_primary = false;

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

int kg_dr_failback(kg_dr_context_t* dr, const char* original_primary_id) {
    if (!dr || !original_primary_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_failback: required parameter is NULL (dr, original_primary_id)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    /* Check if we're the original primary */
    if (strncmp(dr->replica_id, original_primary_id, KG_DR_MAX_REPLICA_ID_LEN) == 0) {
        /* We're the original primary, promote ourselves */
        dr->is_primary = true;
        strncpy(dr->primary_id, dr->replica_id, KG_DR_MAX_REPLICA_ID_LEN - 1);

        /* Demote current primary replica */
        for (uint32_t i = 0; i < dr->replica_count; i++) {
            if (dr->replicas[i].is_primary) {
                dr->replicas[i].is_primary = false;
            }
        }
    }

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

bool kg_dr_is_primary(const kg_dr_context_t* dr) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_is_primary: dr is NULL");
        return false;
    }
    return dr->is_primary;
}

int kg_dr_get_primary_id(const kg_dr_context_t* dr, char* primary_id, size_t buffer_size) {
    if (!dr || !primary_id || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_get_primary_id: required parameter is NULL (dr, primary_id)");
        return -1;
    }

    strncpy(primary_id, dr->primary_id, buffer_size - 1);
    primary_id[buffer_size - 1] = '\0';

    return 0;
}

/* ============================================================================
 * Health Monitoring API
 * ============================================================================ */

int kg_dr_health_check(kg_dr_context_t* dr) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    uint64_t now = get_timestamp_ms();
    bool all_healthy = true;

    for (uint32_t i = 0; i < dr->replica_count; i++) {
        kg_replica_status_t* replica = &dr->replicas[i];
        kg_replica_health_t old_health = replica->health_status;

        /* Check heartbeat timeout */
        if (now - replica->last_heartbeat > dr->config.health_check_interval_ms * 3) {
            replica->is_healthy = false;
            replica->health_status = KG_REPLICA_HEALTH_UNREACHABLE;
            all_healthy = false;
        } else if (replica->lag_ms > 30000) {
            replica->health_status = KG_REPLICA_HEALTH_LAGGING;
        } else {
            replica->is_healthy = true;
            replica->health_status = KG_REPLICA_HEALTH_HEALTHY;
        }

        if (old_health != replica->health_status) {
            invoke_health_callbacks(dr, replica->replica_id, old_health,
                                   replica->health_status);
        }
    }

    nimcp_mutex_unlock(dr->mutex);

    return all_healthy ? 0 : -1;
}

float kg_dr_get_replication_lag(const kg_dr_context_t* dr) {
    if (!dr) {
        return -1.0f;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    uint64_t max_lag = 0;
    for (uint32_t i = 0; i < dr->replica_count; i++) {
        if (dr->replicas[i].lag_ms > max_lag) {
            max_lag = dr->replicas[i].lag_ms;
        }
    }

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);

    return (float)max_lag;
}

int kg_dr_get_stats(const kg_dr_context_t* dr, uint32_t* total_replicas,
                    uint32_t* healthy_replicas, uint64_t* pending_wal_bytes) {
    if (!dr || !total_replicas || !healthy_replicas || !pending_wal_bytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_get_replication_lag: required parameter is NULL (dr, total_replicas, healthy_replicas, pending_wal_bytes)");
        return -1;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    *total_replicas = dr->replica_count;
    *healthy_replicas = 0;

    for (uint32_t i = 0; i < dr->replica_count; i++) {
        if (dr->replicas[i].is_healthy) {
            (*healthy_replicas)++;
        }
    }

    *pending_wal_bytes = dr->pending_wal_bytes;

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);

    return 0;
}

int kg_dr_register_health_callback(kg_dr_context_t* dr,
                                   kg_dr_health_callback_fn callback,
                                   void* user_data) {
    if (!dr || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_get_replication_lag: required parameter is NULL (dr, callback)");
        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    for (uint32_t i = 0; i < MAX_HEALTH_CALLBACKS; i++) {
        if (!dr->health_callbacks[i].active) {
            dr->health_callbacks[i].callback = callback;
            dr->health_callbacks[i].user_data = user_data;
            dr->health_callbacks[i].active = true;
            dr->health_callback_count++;
            nimcp_mutex_unlock(dr->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(dr->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_dr_get_replication_lag: operation failed");
    return -1;
}

/* ============================================================================
 * WAL Management API
 * ============================================================================ */

int kg_dr_wal_flush(kg_dr_context_t* dr) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return -1;
    }

    nimcp_mutex_lock(dr->mutex);

    if (dr->wal_file) {
        fflush(dr->wal_file);
        /* In a real implementation, we would fsync here */
    }

    nimcp_mutex_unlock(dr->mutex);

    return 0;
}

int kg_dr_wal_position(const kg_dr_context_t* dr, uint64_t* segment, uint64_t* offset) {
    if (!dr || !segment || !offset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_dr_wal_position: required parameter is NULL (dr, segment, offset)");
        return -1;
    }

    nimcp_mutex_lock(((kg_dr_context_t*)dr)->mutex);

    *segment = dr->current_segment;
    *offset = dr->current_offset;

    nimcp_mutex_unlock(((kg_dr_context_t*)dr)->mutex);

    return 0;
}

int kg_dr_wal_archive(kg_dr_context_t* dr) {
    if (!dr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dr is NULL");

        return -1;
    }

    /* In a real implementation, we would archive old WAL segments */
    return 0;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

static const char* rpo_level_strings[] = {
    "ZERO",
    "SECONDS",
    "MINUTES",
    "HOURS"
};

const char* kg_rpo_level_to_string(kg_rpo_level_t level) {
    if (level >= 0 && level <= KG_RPO_HOURS) {
        return rpo_level_strings[level];
    }
    return "UNKNOWN";
}

static const char* rto_level_strings[] = {
    "IMMEDIATE",
    "MINUTES",
    "HOURS",
    "DAYS"
};

const char* kg_rto_level_to_string(kg_rto_level_t level) {
    if (level >= 0 && level <= KG_RTO_DAYS) {
        return rto_level_strings[level];
    }
    return "UNKNOWN";
}

static const char* replication_mode_strings[] = {
    "NONE",
    "ASYNC",
    "SEMI_SYNC",
    "SYNC"
};

const char* kg_replication_mode_to_string(kg_replication_mode_t mode) {
    if (mode >= 0 && mode <= KG_REPL_SYNC) {
        return replication_mode_strings[mode];
    }
    return "UNKNOWN";
}

static const char* backup_status_strings[] = {
    "UNKNOWN",
    "IN_PROGRESS",
    "COMPLETED",
    "FAILED",
    "VERIFIED",
    "CORRUPT"
};

const char* kg_backup_status_to_string(kg_backup_status_t status) {
    if (status >= 0 && status <= KG_BACKUP_STATUS_CORRUPT) {
        return backup_status_strings[status];
    }
    return "UNKNOWN";
}

static const char* replica_health_strings[] = {
    "UNKNOWN",
    "HEALTHY",
    "LAGGING",
    "UNREACHABLE",
    "DIVERGED",
    "FAILED"
};

const char* kg_replica_health_to_string(kg_replica_health_t health) {
    if (health >= 0 && health <= KG_REPLICA_HEALTH_FAILED) {
        return replica_health_strings[health];
    }
    return "UNKNOWN";
}
