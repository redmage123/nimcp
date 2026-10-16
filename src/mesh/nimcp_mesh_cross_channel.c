/**
 * @file nimcp_mesh_cross_channel.c
 * @brief Cross-Channel Transaction Routing and System Coordinator Implementation
 *
 * WHAT: Cross-channel transaction handling and system-level coordination
 * WHY:  Enable communication between channels and resolve cross-channel conflicts
 * HOW:  Route through ordering service, arbitrate conflicts via FEP
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_cross_channel.h"
#include "utils/error/nimcp_error_codes.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Error code compatibility aliases */

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Registered channel info
 */
typedef struct registered_channel {
    mesh_channel_id_t id;
    char name[MESH_MAX_NAME_LEN];
    bool connected;
    bool healthy;
    mesh_channel_system_stats_t stats;
} registered_channel_t;

/**
 * @brief Pending transaction entry
 */
typedef struct cross_tx_entry {
    mesh_cross_transaction_t* tx;
    struct cross_tx_entry* next;
} cross_tx_entry_t;

/**
 * @brief Internal system coordinator
 */
/* TODO: mesh_system_coordinator and mesh_cross_router need mutex for thread safety -
 * all channel/pending state is unprotected against concurrent access */
struct mesh_system_coordinator_internal {
    mesh_system_coord_config_t config;

    /* Registered channels */
    registered_channel_t channels[MESH_CROSS_MAX_CHANNELS];
    size_t channel_count;

    /* Dependencies (pointers to opaque types) */
    mesh_ordering_service_t* ordering;
    mesh_msp_t* msp;

    /* Statistics */
    mesh_system_coord_stats_t stats;

    /* Pending conflict resolutions */
    mesh_cross_transaction_t* pending_conflicts[256];
    size_t pending_conflict_count;

    /* P1: Thread safety */
    nimcp_mutex_t* mutex;
};

/**
 * @brief Internal cross-channel router
 */
struct mesh_cross_router_internal {
    mesh_cross_router_config_t config;
    bool running;

    /* System coordinator */
    mesh_system_coordinator_t system_coord;

    /* Pending transactions */
    cross_tx_entry_t* pending_head;
    cross_tx_entry_t* pending_tail;
    size_t pending_count;

    /* Statistics */
    uint64_t total_submitted;
    uint64_t total_completed;
    uint64_t total_failed;

    /* P1: Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Time Utilities
 * ============================================================================ */

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

mesh_system_coord_config_t mesh_system_coord_default_config(void) {
    mesh_system_coord_config_t config = {
        .arbitration_timeout_ms = 100.0f,
        .health_check_interval_ms = 1000.0f,
        .enable_fep_arbitration = true,
        .fe_threshold = MESH_CROSS_FE_THRESHOLD,
        .enable_auto_rebalance = true,
        .max_pending_conflicts = 64
    };
    return config;
}

mesh_cross_router_config_t mesh_cross_router_default_config(void) {
    mesh_cross_router_config_t config = {
        .endorsement_timeout_ms = MESH_CROSS_ENDORSEMENT_TIMEOUT_MS,
        .transaction_timeout_ms = MESH_CROSS_DEFAULT_TIMEOUT_MS,
        .max_pending = MESH_CROSS_MAX_PENDING,
        .require_both_endorsements = true,
        .enable_parallel_endorsement = true
    };
    return config;
}

/* ============================================================================
 * System Coordinator Lifecycle
 * ============================================================================ */

mesh_system_coordinator_t mesh_system_coord_create(
    const mesh_system_coord_config_t* config,
    mesh_ordering_service_t* ordering,
    mesh_msp_t* msp
) {
    mesh_system_coordinator_t coord = (mesh_system_coordinator_t)nimcp_calloc(
        1, sizeof(struct mesh_system_coordinator_internal));
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_system_coord_create: coord is NULL");
        return NULL;
    }

    coord->config = config ? *config : mesh_system_coord_default_config();
    coord->ordering = ordering;
    coord->msp = msp;

    /* P1: Create mutex for thread safety */
    coord->mutex = nimcp_mutex_create(NULL);
    if (!coord->mutex) {
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_system_coord_create: mutex creation failed");
        return NULL;
    }

    return coord;
}

void mesh_system_coord_destroy(mesh_system_coordinator_t coord) {
    if (!coord) return;

    /* Free pending conflicts (just pointers, not owned) */
    coord->pending_conflict_count = 0;

    /* Free stats channel array */
    nimcp_free(coord->stats.channel_stats);

    /* P1: Destroy mutex */
    if (coord->mutex) {
        nimcp_mutex_destroy(coord->mutex);
    }

    nimcp_free(coord);
}

nimcp_error_t mesh_system_coord_register_channel(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id,
    const char* channel_name
) {
    if (!coord) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    if (coord->channel_count >= MESH_CROSS_MAX_CHANNELS) {
        nimcp_mutex_unlock(coord->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_cross_channel: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Check if already registered */
    for (size_t i = 0; i < coord->channel_count; i++) {
        if (coord->channels[i].id == channel_id) {
            nimcp_mutex_unlock(coord->mutex);
            return NIMCP_SUCCESS;  /* Already registered */
        }
    }

    /* Add new channel */
    registered_channel_t* ch = &coord->channels[coord->channel_count];
    ch->id = channel_id;
    ch->connected = true;
    ch->healthy = true;
    memset(&ch->stats, 0, sizeof(ch->stats));
    ch->stats.channel_id = channel_id;
    ch->stats.connected = true;
    ch->stats.healthy = true;

    if (channel_name) {
        strncpy(ch->name, channel_name, MESH_MAX_NAME_LEN - 1);
    } else {
        snprintf(ch->name, MESH_MAX_NAME_LEN, "channel_%u", channel_id);
    }

    coord->channel_count++;
    nimcp_mutex_unlock(coord->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_system_coord_unregister_channel(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id
) {
    if (!coord) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    for (size_t i = 0; i < coord->channel_count; i++) {
        if (coord->channels[i].id == channel_id) {
            /* Remove by shifting remaining channels */
            for (size_t j = i; j < coord->channel_count - 1; j++) {
                coord->channels[j] = coord->channels[j + 1];
            }
            coord->channel_count--;
            nimcp_mutex_unlock(coord->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_mutex_unlock(coord->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_cross_channel: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Cross-Channel Router Lifecycle
 * ============================================================================ */

mesh_cross_router_t mesh_cross_router_create(
    const mesh_cross_router_config_t* config,
    mesh_system_coordinator_t system_coord
) {
    mesh_cross_router_t router = (mesh_cross_router_t)nimcp_calloc(
        1, sizeof(struct mesh_cross_router_internal));
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cross_router_create: router is NULL");
        return NULL;
    }

    router->config = config ? *config : mesh_cross_router_default_config();
    router->system_coord = system_coord;

    /* P1: Create mutex for thread safety */
    router->mutex = nimcp_mutex_create(NULL);
    if (!router->mutex) {
        nimcp_free(router);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cross_router_create: mutex creation failed");
        return NULL;
    }

    return router;
}

void mesh_cross_router_destroy(mesh_cross_router_t router) {
    if (!router) return;

    /* Free pending transactions */
    cross_tx_entry_t* entry = router->pending_head;
    while (entry) {
        cross_tx_entry_t* next = entry->next;
        mesh_cross_transaction_destroy(entry->tx);
        nimcp_free(entry);
        entry = next;
    }

    /* P1: Destroy mutex */
    if (router->mutex) {
        nimcp_mutex_destroy(router->mutex);
    }

    nimcp_free(router);
}

nimcp_error_t mesh_cross_router_start(mesh_cross_router_t router) {
    if (!router) return NIMCP_ERROR_INVALID_PARAM;
    router->running = true;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cross_router_stop(mesh_cross_router_t router, bool drain) {
    if (!router) return NIMCP_ERROR_INVALID_PARAM;

    if (drain) {
        /* Process remaining pending transactions */
        /* In production, this would wait for completion */
    }

    router->running = false;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Cross-Channel Transaction API
 * ============================================================================ */

mesh_cross_transaction_t* mesh_cross_transaction_create(
    mesh_channel_id_t source_channel,
    mesh_channel_id_t target_channel,
    mesh_participant_id_t proposer,
    mesh_tx_type_t tx_type,
    const void* payload,
    size_t payload_size
) {
    mesh_cross_transaction_t* tx = (mesh_cross_transaction_t*)nimcp_calloc(
        1, sizeof(mesh_cross_transaction_t));
    if (!tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cross_transaction_create: tx is NULL");
        return NULL;
    }

    tx->source_channel = source_channel;
    tx->target_channel = target_channel;
    tx->proposer = proposer;
    tx->tx_type = tx_type;
    tx->status = MESH_CROSS_TX_PENDING;
    tx->timeout_ms = MESH_CROSS_DEFAULT_TIMEOUT_MS;

    /* Copy payload */
    if (payload && payload_size > 0) {
        tx->payload = nimcp_malloc(payload_size);
        if (!tx->payload) {
            nimcp_free(tx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_cross_transaction_create: tx->payload is NULL");
            return NULL;
        }
        memcpy(tx->payload, payload, payload_size);
        tx->payload_size = payload_size;
    }

    /* Generate transaction ID */
    tx->base_id.channel = source_channel;
    tx->base_id.proposer = proposer;
    tx->base_id.timestamp_ns = get_time_ns();

    return tx;
}

void mesh_cross_transaction_destroy(mesh_cross_transaction_t* tx) {
    if (!tx) return;

    nimcp_free(tx->payload);

    /* Free endorsement sets */
    nimcp_free(tx->source_endorsements.endorsements);
    nimcp_free(tx->target_endorsements.endorsements);

    nimcp_free(tx);
}

static nimcp_error_t process_cross_transaction(
    mesh_cross_router_t router,
    mesh_cross_transaction_t* tx
) {
    if (!router || !tx) return NIMCP_ERROR_INVALID_PARAM;

    tx->status = MESH_CROSS_TX_VALIDATING;
    tx->started_ns = get_time_ns();

    /* Validate access via MSP (if available) */
    if (router->system_coord && router->system_coord->msp) {
        /* In production, would call MSP validation */
        /* For now, assume access granted */
    }

    /* Simulate endorsement collection */
    tx->status = MESH_CROSS_TX_ENDORSING_SOURCE;
    /* Would collect source channel endorsements here */

    tx->status = MESH_CROSS_TX_ENDORSING_TARGET;
    /* Would collect target channel endorsements here */

    /* Submit to ordering service */
    tx->status = MESH_CROSS_TX_ORDERING;
    if (router->system_coord && router->system_coord->ordering) {
        /* Would submit to ordering service here */
    }

    /* Commit */
    tx->status = MESH_CROSS_TX_COMMITTING;

    /* Success */
    tx->status = MESH_CROSS_TX_COMMITTED;
    tx->completed_ns = get_time_ns();

    /* Update stats */
    router->total_completed++;

    if (router->system_coord) {
        /* P1: Lock system coordinator mutex for stats update */
        nimcp_mutex_lock(router->system_coord->mutex);

        router->system_coord->stats.total_cross_transactions++;
        router->system_coord->stats.successful_transactions++;

        /* Update per-channel stats */
        for (size_t i = 0; i < router->system_coord->channel_count; i++) {
            if (router->system_coord->channels[i].id == tx->source_channel) {
                router->system_coord->channels[i].stats.cross_tx_sent++;
            }
            if (router->system_coord->channels[i].id == tx->target_channel) {
                router->system_coord->channels[i].stats.cross_tx_received++;
            }
        }

        nimcp_mutex_unlock(router->system_coord->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_cross_router_submit(
    mesh_cross_router_t router,
    mesh_cross_transaction_t* tx
) {
    if (!router || !tx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(router->mutex);

    if (!router->running) {
        nimcp_mutex_unlock(router->mutex);
        return NIMCP_ERROR_NOT_READY;
    }
    if (router->pending_count >= router->config.max_pending) {
        nimcp_mutex_unlock(router->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_cross_channel: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    tx->submitted_ns = get_time_ns();
    router->total_submitted++;

    /* Process immediately for simplicity */
    nimcp_error_t err = process_cross_transaction(router, tx);

    nimcp_mutex_unlock(router->mutex);

    /* Invoke callback if set (outside lock to avoid holding lock during user callback) */
    if (tx->callback) {
        mesh_result_t result = {
            .tx_id = tx->base_id,
            .status = (tx->status == MESH_CROSS_TX_COMMITTED) ?
                     MESH_TX_STATUS_COMMITTED : MESH_TX_STATUS_FAILED,
            .error = tx->error,
            .commit_timestamp_ns = tx->completed_ns
        };
        strncpy(result.error_msg, tx->error_msg, sizeof(result.error_msg) - 1);
        tx->callback(&result, tx->callback_ctx);
    }

    return err;
}

nimcp_error_t mesh_cross_router_submit_async(
    mesh_cross_router_t router,
    mesh_cross_transaction_t* tx,
    mesh_tx_callback_t callback,
    void* ctx
) {
    if (!router || !tx) return NIMCP_ERROR_INVALID_PARAM;

    tx->callback = callback;
    tx->callback_ctx = ctx;

    return mesh_cross_router_submit(router, tx);
}

nimcp_error_t mesh_cross_router_wait(
    mesh_cross_router_t router,
    const mesh_tx_id_t* tx_id,
    uint32_t timeout_ms
) {
    if (!router || !tx_id) return NIMCP_ERROR_INVALID_PARAM;

    /* For synchronous processing, already completed */
    (void)timeout_ms;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Conflict Resolution
 * ============================================================================ */

nimcp_error_t mesh_system_coord_arbitrate(
    mesh_system_coordinator_t coord,
    mesh_cross_transaction_t* tx1,
    mesh_cross_transaction_t* tx2,
    mesh_cross_transaction_t** winner
) {
    if (!coord || !tx1 || !tx2 || !winner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cross_channel: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    coord->stats.conflicts_detected++;

    /* Compute free energy for both if not already done */
    if (!tx1->fe_computed) {
        mesh_system_coord_compute_free_energy(coord, tx1, &tx1->free_energy);
    }
    if (!tx2->fe_computed) {
        mesh_system_coord_compute_free_energy(coord, tx2, &tx2->free_energy);
    }

    /* Lower free energy wins */
    mesh_cross_transaction_t* loser;
    if (tx1->free_energy <= tx2->free_energy) {
        *winner = tx1;
        loser = tx2;
        tx1->conflict_result = MESH_CONFLICT_WINNER;
        tx2->conflict_result = MESH_CONFLICT_LOSER;
    } else {
        *winner = tx2;
        loser = tx1;
        tx2->conflict_result = MESH_CONFLICT_WINNER;
        tx1->conflict_result = MESH_CONFLICT_LOSER;
    }

    /* Notify loser via callback if registered */
    if (loser->callback) {
        loser->error = NIMCP_ERROR_CONFLICT_LOST;
        snprintf(loser->error_msg, sizeof(loser->error_msg),
                 "Lost conflict resolution to tx with FE=%.3f (ours=%.3f)",
                 (*winner)->free_energy, loser->free_energy);
        mesh_result_t result = {
            .tx_id = loser->base_id,
            .status = MESH_TX_STATUS_FAILED,
            .error = loser->error,
            .commit_timestamp_ns = 0
        };
        strncpy(result.error_msg, loser->error_msg, sizeof(result.error_msg) - 1);
        loser->callback(&result, loser->callback_ctx);
    }

    coord->stats.conflicts_resolved++;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_system_coord_compute_free_energy(
    mesh_system_coordinator_t coord,
    mesh_cross_transaction_t* tx,
    float* free_energy
) {
    if (!coord || !tx || !free_energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_cross_channel: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    float fe = 0.0f;

    /* Use structured FEP payload if available */
    if (tx->has_fep_data) {
        const mesh_fep_payload_t* fep = &tx->fep_data;

        /* FEP formula: F = D_KL + Complexity
         * D_KL approximated as prediction error weighted by confidence
         * F = |outcome - prediction| * confidence + complexity
         */
        float prediction_error = 0.0f;
        if (fep->flags & MESH_FEP_FLAG_HAS_PREDICTION &&
            fep->flags & MESH_FEP_FLAG_HAS_OUTCOME) {
            prediction_error = fabsf(fep->outcome - fep->prediction);
        }

        fe = prediction_error * fep->confidence;
        fe += fep->complexity * 0.1f;

        /* Apply salience and temporal discount */
        fe *= (2.0f - fep->salience);  /* Higher salience = lower FE */
        fe *= fep->temporal_discount;

        /* Urgent transactions get lower FE */
        if (fep->flags & MESH_FEP_FLAG_URGENT) {
            fe *= 0.5f;
        }
    } else {
        /* Fallback: base free energy from transaction size */
        fe = (float)tx->payload_size / 1000.0f;
    }

    /* Add penalty for cross-channel distance */
    int channel_distance = abs((int)tx->source_channel - (int)tx->target_channel);
    fe += channel_distance * 0.1f;

    /* Add penalty for endorsement failures */
    if (!tx->source_endorsements.policy_satisfied) {
        fe += 0.5f;
    }
    if (!tx->target_endorsements.policy_satisfied) {
        fe += 0.5f;
    }

    /* Clamp to reasonable range */
    if (fe < 0.0f) fe = 0.0f;
    if (fe > 10.0f) fe = 10.0f;

    *free_energy = fe;
    tx->free_energy = fe;
    tx->fe_computed = true;

    return NIMCP_SUCCESS;
}

bool mesh_cross_transactions_conflict(
    const mesh_cross_transaction_t* tx1,
    const mesh_cross_transaction_t* tx2
) {
    if (!tx1 || !tx2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_cross_transactions_conflict: required parameter is NULL (tx1, tx2)");
        return false;
    }

    /* Transactions conflict if they target the same channel with state changes */
    if (tx1->target_channel == tx2->target_channel &&
        tx1->tx_type == MESH_TX_STATE_CHANGE &&
        tx2->tx_type == MESH_TX_STATE_CHANGE) {
        return true;
    }

    /* Also conflict if they're trying to modify the same world state key */
    /* Would check payload for actual conflict detection */

    return false;  /* No conflict detected - normal path */
}

/* ============================================================================
 * System Health
 * ============================================================================ */

bool mesh_system_coord_is_healthy(mesh_system_coordinator_t coord) {
    if (!coord) {
        return false;
    }

    nimcp_mutex_lock(coord->mutex);
    bool healthy = true;
    for (size_t i = 0; i < coord->channel_count; i++) {
        if (!coord->channels[i].healthy) {
            healthy = false;
            break;
        }
    }
    nimcp_mutex_unlock(coord->mutex);

    return healthy;
}

bool mesh_system_coord_channel_healthy(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id
) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_system_coord_channel_healthy: coord is NULL");
        return false;
    }

    nimcp_mutex_lock(coord->mutex);
    for (size_t i = 0; i < coord->channel_count; i++) {
        if (coord->channels[i].id == channel_id) {
            bool val = coord->channels[i].healthy;
            nimcp_mutex_unlock(coord->mutex);
            return val;
        }
    }
    nimcp_mutex_unlock(coord->mutex);

    return false;  /* Channel not found - not healthy */
}

nimcp_error_t mesh_system_coord_mark_unhealthy(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id,
    const char* reason
) {
    if (!coord) return NIMCP_ERROR_INVALID_PARAM;
    (void)reason;  /* Would log reason in production */

    nimcp_mutex_lock(coord->mutex);
    for (size_t i = 0; i < coord->channel_count; i++) {
        if (coord->channels[i].id == channel_id) {
            coord->channels[i].healthy = false;
            coord->channels[i].stats.healthy = false;
            nimcp_mutex_unlock(coord->mutex);
            return NIMCP_SUCCESS;
        }
    }
    nimcp_mutex_unlock(coord->mutex);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_cross_channel: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

nimcp_error_t mesh_system_coord_mark_healthy(
    mesh_system_coordinator_t coord,
    mesh_channel_id_t channel_id
) {
    if (!coord) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);
    for (size_t i = 0; i < coord->channel_count; i++) {
        if (coord->channels[i].id == channel_id) {
            coord->channels[i].healthy = true;
            coord->channels[i].stats.healthy = true;
            nimcp_mutex_unlock(coord->mutex);
            return NIMCP_SUCCESS;
        }
    }
    nimcp_mutex_unlock(coord->mutex);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_cross_channel: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_system_coord_get_stats(
    mesh_system_coordinator_t coord,
    mesh_system_coord_stats_t* stats
) {
    if (!coord || !stats) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    *stats = coord->stats;

    /* Allocate and copy channel stats */
    if (coord->channel_count > 0) {
        stats->channel_stats = (mesh_channel_system_stats_t*)nimcp_calloc(
            coord->channel_count, sizeof(mesh_channel_system_stats_t));
        if (stats->channel_stats) {
            for (size_t i = 0; i < coord->channel_count; i++) {
                stats->channel_stats[i] = coord->channels[i].stats;
            }
            stats->channel_count = coord->channel_count;
        }
    }

    nimcp_mutex_unlock(coord->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_system_coord_reset_stats(mesh_system_coordinator_t coord) {
    if (!coord) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(coord->mutex);

    memset(&coord->stats, 0, sizeof(mesh_system_coord_stats_t));

    for (size_t i = 0; i < coord->channel_count; i++) {
        memset(&coord->channels[i].stats, 0, sizeof(mesh_channel_system_stats_t));
        coord->channels[i].stats.channel_id = coord->channels[i].id;
        coord->channels[i].stats.connected = coord->channels[i].connected;
        coord->channels[i].stats.healthy = coord->channels[i].healthy;
    }

    nimcp_mutex_unlock(coord->mutex);

    return NIMCP_SUCCESS;
}

void mesh_system_coord_stats_free(mesh_system_coord_stats_t* stats) {
    if (!stats) return;
    nimcp_free(stats->channel_stats);
    stats->channel_stats = NULL;
    stats->channel_count = 0;
}

size_t mesh_cross_router_pending_count(mesh_cross_router_t router) {
    if (!router) return 0;
    nimcp_mutex_lock(router->mutex);
    size_t count = router->pending_count;
    nimcp_mutex_unlock(router->mutex);
    return count;
}

/* ============================================================================
 * Debug
 * ============================================================================ */

void mesh_system_coord_print_debug(mesh_system_coordinator_t coord) {
    if (!coord) {
        printf("System Coordinator: NULL\n");
        return;
    }

    printf("=== System Coordinator Debug ===\n");
    printf("Channels registered: %zu\n", coord->channel_count);
    printf("FEP arbitration: %s\n", coord->config.enable_fep_arbitration ? "enabled" : "disabled");
    printf("System healthy: %s\n", mesh_system_coord_is_healthy(coord) ? "yes" : "no");

    printf("\nRegistered Channels:\n");
    for (size_t i = 0; i < coord->channel_count; i++) {
        printf("  [%u] %s: %s %s\n",
               coord->channels[i].id,
               coord->channels[i].name,
               coord->channels[i].connected ? "connected" : "disconnected",
               coord->channels[i].healthy ? "healthy" : "unhealthy");
    }

    printf("\nStatistics:\n");
    printf("  Total cross-channel txs: %llu\n",
           (unsigned long long)coord->stats.total_cross_transactions);
    printf("  Successful: %llu\n",
           (unsigned long long)coord->stats.successful_transactions);
    printf("  Failed: %llu\n",
           (unsigned long long)coord->stats.failed_transactions);
    printf("  Conflicts detected: %llu\n",
           (unsigned long long)coord->stats.conflicts_detected);
    printf("  Conflicts resolved: %llu\n",
           (unsigned long long)coord->stats.conflicts_resolved);
    printf("================================\n");
}

void mesh_cross_router_print_debug(mesh_cross_router_t router) {
    if (!router) {
        printf("Cross-Channel Router: NULL\n");
        return;
    }

    printf("=== Cross-Channel Router Debug ===\n");
    printf("Running: %s\n", router->running ? "yes" : "no");
    printf("Pending: %zu / %zu\n", router->pending_count, router->config.max_pending);
    printf("Endorsement timeout: %.1f ms\n", router->config.endorsement_timeout_ms);
    printf("Transaction timeout: %.1f ms\n", router->config.transaction_timeout_ms);

    printf("\nStatistics:\n");
    printf("  Total submitted: %llu\n", (unsigned long long)router->total_submitted);
    printf("  Total completed: %llu\n", (unsigned long long)router->total_completed);
    printf("  Total failed: %llu\n", (unsigned long long)router->total_failed);
    printf("==================================\n");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_cross_tx_status_to_string(mesh_cross_tx_status_t status) {
    switch (status) {
        case MESH_CROSS_TX_PENDING:          return "PENDING";
        case MESH_CROSS_TX_VALIDATING:       return "VALIDATING";
        case MESH_CROSS_TX_ENDORSING_SOURCE: return "ENDORSING_SOURCE";
        case MESH_CROSS_TX_ENDORSING_TARGET: return "ENDORSING_TARGET";
        case MESH_CROSS_TX_ORDERING:         return "ORDERING";
        case MESH_CROSS_TX_COMMITTING:       return "COMMITTING";
        case MESH_CROSS_TX_COMMITTED:        return "COMMITTED";
        case MESH_CROSS_TX_FAILED:           return "FAILED";
        case MESH_CROSS_TX_CONFLICT:         return "CONFLICT";
        case MESH_CROSS_TX_ACCESS_DENIED:    return "ACCESS_DENIED";
        default:                             return "UNKNOWN";
    }
}

const char* mesh_conflict_result_to_string(mesh_conflict_result_t result) {
    switch (result) {
        case MESH_CONFLICT_NONE:     return "NONE";
        case MESH_CONFLICT_WINNER:   return "WINNER";
        case MESH_CONFLICT_LOSER:    return "LOSER";
        case MESH_CONFLICT_MERGED:   return "MERGED";
        case MESH_CONFLICT_DEFERRED: return "DEFERRED";
        default:                     return "UNKNOWN";
    }
}
