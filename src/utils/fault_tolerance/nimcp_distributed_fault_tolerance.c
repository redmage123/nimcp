/**
 * @file nimcp_distributed_fault_tolerance.c
 * @brief Implementation of Distributed Fault Tolerance for Swarm Systems
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "utils/fault_tolerance/nimcp_distributed_fault_tolerance.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(distributed_fault_tolerance)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal DFT context
 */
#define DFT_CHECKPOINT_DATA_MAX 4096        /**< Max data per checkpoint */

struct dft_context {
    dft_config_t config;                    /**< Configuration */
    dft_peer_info_t peers[DFT_MAX_PEERS];   /**< Peer information */
    uint32_t peer_count;                    /**< Number of peers */
    dft_checkpoint_meta_t checkpoints[DFT_MAX_CHECKPOINTS]; /**< Checkpoints */
    uint8_t checkpoint_data[DFT_MAX_CHECKPOINTS][DFT_CHECKPOINT_DATA_MAX]; /**< Checkpoint data */
    uint32_t checkpoint_count;              /**< Number of checkpoints */
    uint64_t next_checkpoint_id;            /**< Next checkpoint ID */
    uint64_t current_view;                  /**< Current view number */
    uint32_t leader_id;                     /**< Current leader */
    dft_stats_t stats;                      /**< Statistics */
    dft_event_callback_t callbacks[8];      /**< Event callbacks */
    void* callback_data[8];                 /**< Callback user data */
    uint32_t callback_count;                /**< Number of callbacks */
    nimcp_mutex_t lock;                     /**< Context lock */
    nimcp_thread_t heartbeat_thread;        /**< Heartbeat thread */
    volatile bool running;                  /**< Is running (volatile for thread visibility) */
    /* P2-7 FIX: Mark initialized as volatile since it's read across threads
     * (e.g., dft_start checks initialized while heartbeat thread may be running).
     * Without volatile, compiler may optimize away the read and cache a stale value. */
    volatile bool initialized;              /**< Is initialized */
};

//=============================================================================
// Private Functions
//=============================================================================

/**
 * @brief Emit event to callbacks
 */
static void dft_emit_event(dft_context_t* ctx, dft_event_type_t event, const void* data) {
    if (!ctx) return;

    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i]) {
            ctx->callbacks[i](event, data, ctx->callback_data[i]);
        }
    }
}

/**
 * @brief Find peer by ID
 */
static dft_peer_info_t* dft_find_peer(dft_context_t* ctx, uint32_t node_id) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < ctx->peer_count; i++) {
        if (ctx->peers[i].node_id == node_id) {
            return &ctx->peers[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_find_peer: validation failed");
    return NULL;
}

/**
 * @brief Calculate CRC32 checksum
 */
static uint32_t dft_crc32(const void* data, size_t size) {
    if (!data || size == 0) return 0;

    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }

    return ~crc;
}

/**
 * @brief Heartbeat thread function
 */
static void* dft_heartbeat_thread(void* arg) {
    dft_context_t* ctx = (dft_context_t*)arg;
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }

    while (ctx->running) {
        // Send heartbeats
        dft_send_heartbeat(ctx);

        // Re-check running flag after heartbeat send
        if (!ctx->running) break;

        // Check for failures
        dft_failure_detection_t failures[DFT_MAX_PEERS];
        uint32_t failure_count = dft_detect_failures(ctx, failures, DFT_MAX_PEERS);

        // Re-check running flag after failure detection
        if (!ctx->running) break;

        // Process detected failures under lock to protect peer state
        uint32_t events_to_emit = 0;
        uint32_t event_indices[DFT_MAX_PEERS];

        nimcp_mutex_lock(&ctx->lock);
        for (uint32_t i = 0; i < failure_count; i++) {
            dft_peer_info_t* peer = dft_find_peer(ctx, failures[i].node_id);
            if (peer && peer->state == DFT_NODE_HEALTHY) {
                peer->state = DFT_NODE_SUSPECTED;
                event_indices[events_to_emit++] = i;
            }
        }
        nimcp_mutex_unlock(&ctx->lock);

        // Emit events outside lock to avoid deadlock with callbacks
        for (uint32_t i = 0; i < events_to_emit; i++) {
            if (!ctx->running) break;
            dft_emit_event(ctx, DFT_EVENT_NODE_SUSPECTED, &failures[event_indices[i]]);
        }

        // Sleep in small increments to allow quick shutdown
        uint32_t remaining_ms = ctx->config.heartbeat_interval_ms;
        while (remaining_ms > 0 && ctx->running) {
            uint32_t sleep_ms = (remaining_ms > 10) ? 10 : remaining_ms;
            nimcp_time_sleep_ms(sleep_ms);
            remaining_ms -= sleep_ms;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_heartbeat_thread: operation failed");
    return NULL;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dft_config_t dft_default_config(void) {
    dft_config_t config = {
        .node_id = 0,
        .heartbeat_interval_ms = DFT_HEARTBEAT_INTERVAL_MS,
        .failure_timeout_ms = DFT_FAILURE_TIMEOUT_MS,
        .quorum_threshold = DFT_QUORUM_THRESHOLD,
        .detection_method = DFT_DETECT_HEARTBEAT,
        .recovery_mode = DFT_RECOVERY_CONSENSUS,
        .replication = DFT_REPLICATE_ASYNC,
        .min_replicas = 2,
        .max_recovery_attempts = DFT_MAX_RECOVERY_ATTEMPTS,
        .enable_byzantine_detection = true,
        .enable_auto_recovery = true,
        .enable_bio_async = true,
        .enable_encryption = false
    };
    return config;
}

dft_context_t* dft_create(const dft_config_t* config) {
    // Guard: Validate config
    if (!config) {
        LOG_ERROR("DFT", "NULL config provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dft_create: config is NULL");
        return NULL;
    }

    // Allocate context
    dft_context_t* ctx = nimcp_malloc(sizeof(dft_context_t));
    if (!ctx) {
        LOG_ERROR("DFT", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dft_create: ctx is NULL");
        return NULL;
    }

    // Initialize
    memset(ctx, 0, sizeof(dft_context_t));
    ctx->config = *config;
    ctx->next_checkpoint_id = 1;
    ctx->current_view = 0;
    ctx->leader_id = config->node_id; // Self is leader initially

    // Initialize mutex
    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        LOG_ERROR("DFT", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "dft_create: validation failed");
        return NULL;
    }

    ctx->initialized = true;

    // Register with BBB security
    bbb_register_module("distributed_fault_tolerance", BBB_MODULE_TYPE_CORE);
    bbb_audit_log(BBB_AUDIT_INFO, "DFT", "CREATE", "Created DFT context for node %u", config->node_id);

    LOG_INFO("DFT", "Created DFT context for node %u", config->node_id);
    return ctx;
}

void dft_destroy(dft_context_t* ctx) {
    // Guard: NULL check
    if (!ctx) return;

    // Stop if running
    if (ctx->running) {
        dft_stop(ctx);
    }

    // Destroy mutex
    nimcp_mutex_destroy(&ctx->lock);

    bbb_audit_log(BBB_AUDIT_INFO, "DFT", "DESTROY", "Destroyed DFT context");

    // Free context
    nimcp_free(ctx);
}

bool dft_start(dft_context_t* ctx) {
    // Guard: Validate context
    if (!ctx || !ctx->initialized) {
        LOG_ERROR("DFT", "Invalid context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_start: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }

    // Guard: Already running
    if (ctx->running) {
        LOG_WARN("DFT", "Already running");
        return true;
    }

    ctx->running = true;

    // Start heartbeat thread
    if (nimcp_thread_create(&ctx->heartbeat_thread, dft_heartbeat_thread, ctx, NULL) != 0) {
        LOG_ERROR("DFT", "Failed to start heartbeat thread");
        ctx->running = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_start: validation failed");
        return false;
    }

    dft_emit_event(ctx, DFT_EVENT_NODE_JOINED, &ctx->config.node_id);
    LOG_INFO("DFT", "Started DFT for node %u", ctx->config.node_id);
    return true;
}

bool dft_stop(dft_context_t* ctx) {
    // Guard: Validate context
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_stop: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }

    // Guard: Not running
    if (!ctx->running) return true;

    // Set running flag under lock for memory barrier visibility
    nimcp_mutex_lock(&ctx->lock);
    ctx->running = false;
    nimcp_mutex_unlock(&ctx->lock);

    // Wait for heartbeat thread (pass by value, not pointer)
    nimcp_thread_join(ctx->heartbeat_thread, NULL);

    dft_emit_event(ctx, DFT_EVENT_NODE_LEFT, &ctx->config.node_id);
    LOG_INFO("DFT", "Stopped DFT for node %u", ctx->config.node_id);
    return true;
}

//=============================================================================
// Peer Management
//=============================================================================

bool dft_add_peer(dft_context_t* ctx, uint32_t node_id, void* user_data) {
    // Guard: Validate context
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_add_peer: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Guard: Capacity check
    if (ctx->peer_count >= DFT_MAX_PEERS) {
        nimcp_mutex_unlock(&ctx->lock);
        LOG_ERROR("DFT", "Maximum peers reached");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dft_add_peer: capacity exceeded");
        return false;
    }

    // Guard: Duplicate check (must be under lock for thread safety)
    if (dft_find_peer(ctx, node_id)) {
        nimcp_mutex_unlock(&ctx->lock);
        LOG_WARN("DFT", "Peer %u already exists", node_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_add_peer: validation failed");
        return false;
    }

    // Add peer
    dft_peer_info_t* peer = &ctx->peers[ctx->peer_count];
    peer->node_id = node_id;
    peer->state = DFT_NODE_HEALTHY;
    peer->last_heartbeat_ms = nimcp_time_get_ms();
    peer->health_score = 100.0f;
    peer->failure_count = 0;
    peer->user_data = user_data;
    ctx->peer_count++;

    nimcp_mutex_unlock(&ctx->lock);

    dft_emit_event(ctx, DFT_EVENT_NODE_JOINED, &node_id);
    LOG_INFO("DFT", "Added peer %u, total peers: %u", node_id, ctx->peer_count);
    return true;
}

bool dft_remove_peer(dft_context_t* ctx, uint32_t node_id) {
    // Guard: Validate context
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_remove_peer: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Find and remove peer
    for (uint32_t i = 0; i < ctx->peer_count; i++) {
        if (ctx->peers[i].node_id == node_id) {
            // Shift remaining peers
            for (uint32_t j = i; j < ctx->peer_count - 1; j++) {
                ctx->peers[j] = ctx->peers[j + 1];
            }
            ctx->peer_count--;

            nimcp_mutex_unlock(&ctx->lock);

            dft_emit_event(ctx, DFT_EVENT_NODE_LEFT, &node_id);
            LOG_INFO("DFT", "Removed peer %u", node_id);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_remove_peer: operation failed");
    return false;
}

bool dft_get_peer_info(dft_context_t* ctx, uint32_t node_id, dft_peer_info_t* info) {
    // Guard: Validate inputs
    if (!ctx || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_get_peer_info: required parameter is NULL (ctx, info)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    dft_peer_info_t* peer = dft_find_peer(ctx, node_id);
    if (peer) {
        *info = *peer;
        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_get_peer_info: validation failed");
    return false;
}

uint32_t dft_get_all_peers(dft_context_t* ctx, dft_peer_info_t* peers, uint32_t max_peers) {
    // Guard: Validate inputs
    if (!ctx || !peers || max_peers == 0) return 0;

    nimcp_mutex_lock(&ctx->lock);

    uint32_t count = (ctx->peer_count < max_peers) ? ctx->peer_count : max_peers;
    memcpy(peers, ctx->peers, count * sizeof(dft_peer_info_t));

    nimcp_mutex_unlock(&ctx->lock);
    return count;
}

uint32_t dft_get_peer_count(dft_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->peer_count;
}

//=============================================================================
// Heartbeat and Failure Detection
//=============================================================================

uint32_t dft_send_heartbeat(dft_context_t* ctx) {
    // Guard: Validate context
    if (!ctx || !ctx->running) return 0;

    uint32_t reached = 0;
    uint64_t now = nimcp_time_get_ms();

    nimcp_mutex_lock(&ctx->lock);
    ctx->stats.total_heartbeats_sent++;
    nimcp_mutex_unlock(&ctx->lock);

    // In a real implementation, this would send network messages
    // For now, we just update stats
    reached = ctx->peer_count;

    return reached;
}

bool dft_receive_heartbeat(dft_context_t* ctx, uint32_t node_id, float health_score) {
    // Guard: Validate context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_receive_heartbeat: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    dft_peer_info_t* peer = dft_find_peer(ctx, node_id);
    if (peer) {
        peer->last_heartbeat_ms = nimcp_time_get_ms();
        peer->health_score = health_score;

        // Recover from suspected state
        if (peer->state == DFT_NODE_SUSPECTED) {
            peer->state = DFT_NODE_HEALTHY;
            dft_emit_event(ctx, DFT_EVENT_NODE_RECOVERED, &node_id);
        }

        ctx->stats.total_heartbeats_received++;
        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_receive_heartbeat: operation failed");
    return false;
}

uint32_t dft_detect_failures(dft_context_t* ctx, dft_failure_detection_t* failures, uint32_t max_failures) {
    // Guard: Validate inputs
    if (!ctx || !failures || max_failures == 0) return 0;

    uint32_t count = 0;
    uint64_t now = nimcp_time_get_ms();

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->peer_count && count < max_failures; i++) {
        dft_peer_info_t* peer = &ctx->peers[i];

        // Skip already failed or quarantined nodes
        if (peer->state == DFT_NODE_FAILED || peer->state == DFT_NODE_QUARANTINED) {
            continue;
        }

        // Check heartbeat timeout
        uint64_t elapsed = now - peer->last_heartbeat_ms;
        if (elapsed > ctx->config.failure_timeout_ms) {
            failures[count].node_id = peer->node_id;
            failures[count].method = ctx->config.detection_method;
            failures[count].confidence = (float)elapsed / (float)(ctx->config.failure_timeout_ms * 2);
            if (failures[count].confidence > 1.0f) failures[count].confidence = 1.0f;
            failures[count].detected_at_ms = now;
            snprintf(failures[count].reason, sizeof(failures[count].reason),
                    "Heartbeat timeout: %lu ms", (unsigned long)elapsed);
            count++;

            ctx->stats.total_failures_detected++;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    return count;
}

bool dft_report_suspected_failure(dft_context_t* ctx, uint32_t node_id, const char* reason) {
    // Guard: Validate inputs
    if (!ctx || !reason) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_report_suspected_failure: required parameter is NULL (ctx, reason)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    dft_peer_info_t* peer = dft_find_peer(ctx, node_id);
    if (peer) {
        peer->suspected_by_count++;

        // Check if enough nodes suspect this peer
        float suspicion_ratio = (float)peer->suspected_by_count / (float)(ctx->peer_count + 1);
        if (suspicion_ratio >= ctx->config.quorum_threshold && peer->state != DFT_NODE_FAILED) {
            peer->state = DFT_NODE_FAILED;
            peer->failure_count++;
            dft_emit_event(ctx, DFT_EVENT_NODE_FAILED, &node_id);

            // Initiate recovery if auto-recovery enabled
            if (ctx->config.enable_auto_recovery) {
                nimcp_mutex_unlock(&ctx->lock);
                dft_initiate_recovery(ctx, node_id);
                return true;
            }
        }

        nimcp_mutex_unlock(&ctx->lock);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_report_suspected_failure: operation failed");
    return false;
}

//=============================================================================
// Checkpoint Management
//=============================================================================

bool dft_create_checkpoint(dft_context_t* ctx, const void* data, size_t data_size, dft_checkpoint_meta_t* meta) {
    // Guard: Validate inputs
    if (!ctx || !data || data_size == 0 || !meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_create_checkpoint: required parameter is NULL (ctx, data, meta)");
        return false;
    }

    // Guard: BBB security check
    if (!bbb_check_pointer((void*)data, "checkpoint_data")) {
        LOG_ERROR("DFT", "BBB rejected checkpoint data pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_create_checkpoint: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Guard: Data size check
    if (data_size > DFT_CHECKPOINT_DATA_MAX) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED,
            "dft_create_checkpoint: data_size %zu exceeds max %d",
            data_size, DFT_CHECKPOINT_DATA_MAX);
        return false;
    }

    // Guard: Capacity check
    if (ctx->checkpoint_count >= DFT_MAX_CHECKPOINTS) {
        // Remove oldest checkpoint (shift metadata and data)
        for (uint32_t i = 0; i < ctx->checkpoint_count - 1; i++) {
            ctx->checkpoints[i] = ctx->checkpoints[i + 1];
            memcpy(ctx->checkpoint_data[i], ctx->checkpoint_data[i + 1],
                   ctx->checkpoints[i].data_size);
        }
        ctx->checkpoint_count--;
    }

    // Create checkpoint metadata
    uint32_t idx = ctx->checkpoint_count;
    dft_checkpoint_meta_t* cp = &ctx->checkpoints[idx];
    cp->magic = DFT_CHECKPOINT_MAGIC;
    cp->checkpoint_id = ctx->next_checkpoint_id++;
    cp->source_node_id = ctx->config.node_id;
    cp->replica_count = 1;
    cp->replica_nodes[0] = ctx->config.node_id;
    cp->created_at_ms = nimcp_time_get_ms();
    cp->expires_at_ms = cp->created_at_ms + 3600000; // 1 hour expiry
    cp->data_size = data_size;
    cp->crc32 = dft_crc32(data, data_size);
    cp->is_valid = true;

    // Store checkpoint data
    memcpy(ctx->checkpoint_data[idx], data, data_size);

    ctx->checkpoint_count++;
    ctx->stats.total_checkpoints_created++;

    // Copy to output
    *meta = *cp;

    nimcp_mutex_unlock(&ctx->lock);

    dft_emit_event(ctx, DFT_EVENT_CHECKPOINT_CREATED, meta);
    LOG_INFO("DFT", "Created checkpoint %lu, size: %zu", (unsigned long)cp->checkpoint_id, data_size);

    return true;
}

bool dft_retrieve_checkpoint(dft_context_t* ctx, uint64_t checkpoint_id, void* data_buffer, size_t buffer_size, size_t* actual_size) {
    // Guard: Validate inputs
    if (!ctx || !data_buffer || buffer_size == 0 || !actual_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_retrieve_checkpoint: required parameter is NULL (ctx, data_buffer, actual_size)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Find checkpoint
    for (uint32_t i = 0; i < ctx->checkpoint_count; i++) {
        if (ctx->checkpoints[i].checkpoint_id == checkpoint_id) {
            dft_checkpoint_meta_t* cp = &ctx->checkpoints[i];

            if (cp->data_size > buffer_size) {
                *actual_size = cp->data_size;
                nimcp_mutex_unlock(&ctx->lock);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_retrieve_checkpoint: validation failed");
                return false; // Buffer too small
            }

            // Copy checkpoint data to output buffer
            memcpy(data_buffer, ctx->checkpoint_data[i], cp->data_size);
            *actual_size = cp->data_size;
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_retrieve_checkpoint: operation failed");
    return false;
}

uint64_t dft_get_latest_checkpoint_id(dft_context_t* ctx) {
    if (!ctx || ctx->checkpoint_count == 0) return 0;
    return ctx->checkpoints[ctx->checkpoint_count - 1].checkpoint_id;
}

uint32_t dft_list_checkpoints(dft_context_t* ctx, dft_checkpoint_meta_t* metas, uint32_t max_count) {
    // Guard: Validate inputs
    if (!ctx || !metas || max_count == 0) return 0;

    nimcp_mutex_lock(&ctx->lock);

    uint32_t count = (ctx->checkpoint_count < max_count) ? ctx->checkpoint_count : max_count;
    memcpy(metas, ctx->checkpoints, count * sizeof(dft_checkpoint_meta_t));

    nimcp_mutex_unlock(&ctx->lock);
    return count;
}

//=============================================================================
// Recovery Coordination
//=============================================================================

bool dft_initiate_recovery(dft_context_t* ctx, uint32_t node_id) {
    // Guard: Validate context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_initiate_recovery: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    dft_peer_info_t* peer = dft_find_peer(ctx, node_id);
    if (!peer) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_initiate_recovery: peer is NULL");
        return false;
    }

    // Mark as recovering
    peer->state = DFT_NODE_RECOVERING;

    nimcp_mutex_unlock(&ctx->lock);

    dft_emit_event(ctx, DFT_EVENT_RECOVERY_STARTED, &node_id);
    LOG_INFO("DFT", "Initiated recovery for node %u", node_id);

    bbb_audit_log(BBB_AUDIT_WARNING, "DFT", "RECOVERY", "Initiated recovery for node %u", node_id);

    return true;
}

bool dft_vote_recovery(dft_context_t* ctx, const dft_recovery_vote_t* vote) {
    // Guard: Validate inputs
    if (!ctx || !vote) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_vote_recovery: required parameter is NULL (ctx, vote)");
        return false;
    }

    // In real implementation, would broadcast vote to peers
    LOG_DEBUG("DFT", "Vote for recovery: node %u -> %s",
                   vote->target_node_id,
                   vote->vote_for_recovery ? "recover" : "wait");

    return true;
}

bool dft_check_recovery_consensus(dft_context_t* ctx, uint32_t node_id, bool* quorum_reached, uint64_t* checkpoint_id) {
    // Guard: Validate inputs
    if (!ctx || !quorum_reached || !checkpoint_id) {
        return false;
    }

    // For now, simple majority check
    *quorum_reached = (ctx->peer_count >= 2);
    *checkpoint_id = dft_get_latest_checkpoint_id(ctx);

    return true;
}

bool dft_complete_recovery(dft_context_t* ctx, uint32_t node_id, bool success) {
    // Guard: Validate context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_complete_recovery: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    dft_peer_info_t* peer = dft_find_peer(ctx, node_id);
    if (peer) {
        if (success) {
            peer->state = DFT_NODE_HEALTHY;
            peer->last_heartbeat_ms = nimcp_time_get_ms();
            ctx->stats.total_recoveries_completed++;
        } else {
            peer->state = DFT_NODE_FAILED;
        }

        nimcp_mutex_unlock(&ctx->lock);
        dft_emit_event(ctx, DFT_EVENT_RECOVERY_COMPLETE, &node_id);
        return true;
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_complete_recovery: operation failed");
    return false;
}

//=============================================================================
// Quorum and Leader Election
//=============================================================================

bool dft_has_quorum(dft_context_t* ctx) {
    if (!ctx) {
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    uint32_t healthy_count = 1; // Include self
    for (uint32_t i = 0; i < ctx->peer_count; i++) {
        if (ctx->peers[i].state == DFT_NODE_HEALTHY) {
            healthy_count++;
        }
    }

    float ratio = (float)healthy_count / (float)(ctx->peer_count + 1);
    bool has_quorum = (ratio >= ctx->config.quorum_threshold);

    nimcp_mutex_unlock(&ctx->lock);
    return has_quorum;
}

uint32_t dft_get_leader(dft_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->leader_id;
}

bool dft_trigger_election(dft_context_t* ctx) {
    // Guard: Validate context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_trigger_election: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Simple leader election: lowest healthy node ID wins
    uint32_t new_leader = ctx->config.node_id;

    for (uint32_t i = 0; i < ctx->peer_count; i++) {
        if (ctx->peers[i].state == DFT_NODE_HEALTHY && ctx->peers[i].node_id < new_leader) {
            new_leader = ctx->peers[i].node_id;
        }
    }

    if (new_leader != ctx->leader_id) {
        ctx->leader_id = new_leader;
        ctx->current_view++;
        dft_emit_event(ctx, DFT_EVENT_LEADER_ELECTED, &new_leader);
    }

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

//=============================================================================
// Events and Callbacks
//=============================================================================

bool dft_register_callback(dft_context_t* ctx, dft_event_callback_t callback, void* user_data) {
    // Guard: Validate inputs
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_register_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    // Guard: Capacity check
    if (ctx->callback_count >= 8) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dft_register_callback: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->callbacks[ctx->callback_count] = callback;
    ctx->callback_data[ctx->callback_count] = user_data;
    ctx->callback_count++;

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

bool dft_unregister_callback(dft_context_t* ctx, dft_event_callback_t callback) {
    // Guard: Validate inputs
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_unregister_callback: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i] == callback) {
            // Shift remaining callbacks
            for (uint32_t j = i; j < ctx->callback_count - 1; j++) {
                ctx->callbacks[j] = ctx->callbacks[j + 1];
                ctx->callback_data[j] = ctx->callback_data[j + 1];
            }
            ctx->callback_count--;
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dft_unregister_callback: operation failed");
    return false;
}

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

bool dft_get_stats(dft_context_t* ctx, dft_stats_t* stats) {
    // Guard: Validate inputs
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dft_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    *stats = ctx->stats;

    // Calculate availability
    uint32_t healthy = 1; // Include self
    for (uint32_t i = 0; i < ctx->peer_count; i++) {
        if (ctx->peers[i].state == DFT_NODE_HEALTHY) healthy++;
    }
    stats->current_availability = (float)healthy / (float)(ctx->peer_count + 1);

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

void dft_reset_stats(dft_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->lock);
    memset(&ctx->stats, 0, sizeof(dft_stats_t));
    nimcp_mutex_unlock(&ctx->lock);
}

float dft_get_cluster_health(dft_context_t* ctx) {
    if (!ctx) return 0.0f;

    float total_health = 100.0f; // Self

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->peer_count; i++) {
        if (ctx->peers[i].state == DFT_NODE_HEALTHY) {
            total_health += ctx->peers[i].health_score;
        }
    }

    float avg_health = total_health / (float)(ctx->peer_count + 1);

    nimcp_mutex_unlock(&ctx->lock);
    return avg_health;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* dft_node_state_to_string(dft_node_state_t state) {
    switch (state) {
        case DFT_NODE_HEALTHY: return "HEALTHY";
        case DFT_NODE_SUSPECTED: return "SUSPECTED";
        case DFT_NODE_FAILED: return "FAILED";
        case DFT_NODE_RECOVERING: return "RECOVERING";
        case DFT_NODE_QUARANTINED: return "QUARANTINED";
        case DFT_NODE_OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

const char* dft_detection_method_to_string(dft_detection_method_t method) {
    switch (method) {
        case DFT_DETECT_HEARTBEAT: return "HEARTBEAT";
        case DFT_DETECT_PING: return "PING";
        case DFT_DETECT_ACCRUAL: return "ACCRUAL";
        case DFT_DETECT_CONSENSUS: return "CONSENSUS";
        default: return "UNKNOWN";
    }
}

const char* dft_recovery_mode_to_string(dft_recovery_mode_t mode) {
    switch (mode) {
        case DFT_RECOVERY_LOCAL: return "LOCAL";
        case DFT_RECOVERY_PEER: return "PEER";
        case DFT_RECOVERY_LEADER: return "LEADER";
        case DFT_RECOVERY_CONSENSUS: return "CONSENSUS";
        default: return "UNKNOWN";
    }
}

const char* dft_event_type_to_string(dft_event_type_t event) {
    switch (event) {
        case DFT_EVENT_NODE_JOINED: return "NODE_JOINED";
        case DFT_EVENT_NODE_LEFT: return "NODE_LEFT";
        case DFT_EVENT_NODE_SUSPECTED: return "NODE_SUSPECTED";
        case DFT_EVENT_NODE_FAILED: return "NODE_FAILED";
        case DFT_EVENT_NODE_RECOVERED: return "NODE_RECOVERED";
        case DFT_EVENT_CHECKPOINT_CREATED: return "CHECKPOINT_CREATED";
        case DFT_EVENT_CHECKPOINT_REPLICATED: return "CHECKPOINT_REPLICATED";
        case DFT_EVENT_RECOVERY_STARTED: return "RECOVERY_STARTED";
        case DFT_EVENT_RECOVERY_COMPLETE: return "RECOVERY_COMPLETE";
        case DFT_EVENT_QUORUM_LOST: return "QUORUM_LOST";
        case DFT_EVENT_QUORUM_RESTORED: return "QUORUM_RESTORED";
        case DFT_EVENT_LEADER_ELECTED: return "LEADER_ELECTED";
        case DFT_EVENT_BYZANTINE_DETECTED: return "BYZANTINE_DETECTED";
        default: return "UNKNOWN";
    }
}
