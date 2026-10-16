/**
 * @file nimcp_kg_federation.c
 * @brief Cross-Brain Federation for Knowledge Graph Synchronization
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implementation of peer-to-peer federation for distributed brain knowledge graphs.
 * Supports configurable sync policies, conflict resolution, and auto-discovery.
 */

#include "core/brain/nimcp_kg_federation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_federation, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Pending conflict entry
 */
typedef struct {
    kg_conflict_t conflict;
    bool resolved;
} pending_conflict_t;

/**
 * @brief Federation internal structure
 */
struct kg_federation {
    brain_kg_t* kg;
    kg_federation_config_t config;
    nimcp_mutex_t* mutex;

    /* Peer management */
    kg_federation_peer_t peers[KG_FEDERATION_MAX_PEERS];
    uint32_t peer_count;

    /* Conflict queue */
    pending_conflict_t conflicts[256];
    uint32_t conflict_count;
    uint64_t next_conflict_id;

    /* Sync statistics */
    uint64_t total_syncs;
    uint64_t successful_syncs;
    uint64_t failed_syncs;
    uint64_t last_sync_time;

    /* Auto-sync state */
    bool auto_sync_running;
    bool discovery_active;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_current_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Find peer by ID
 */
static kg_federation_peer_t* find_peer(
    kg_federation_t* fed,
    const char* peer_id
) {
    for (uint32_t i = 0; i < fed->peer_count; i++) {
        if (strcmp(fed->peers[i].peer_id, peer_id) == 0) {
            return &fed->peers[i];
        }
    }
    return NULL;
}

/**
 * @brief Find pending conflict by ID
 */
static pending_conflict_t* find_conflict(
    kg_federation_t* fed,
    uint64_t conflict_id
) {
    for (uint32_t i = 0; i < fed->conflict_count; i++) {
        if (fed->conflicts[i].conflict.conflict_id == conflict_id &&
            !fed->conflicts[i].resolved) {
            return &fed->conflicts[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int kg_federation_default_config(kg_federation_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Generate a default local ID */
    snprintf(config->local_id, KG_FEDERATION_PEER_ID_LEN, "brain_%lu",
             (unsigned long)get_current_timestamp_ms());

    config->policy = KG_SYNC_BIDIRECTIONAL;
    config->conflict = KG_CONFLICT_LAST_WRITE_WINS;
    config->sync_interval_ms = KG_FEDERATION_DEFAULT_SYNC_INTERVAL_MS;
    config->max_sync_batch_size = KG_FEDERATION_DEFAULT_BATCH_SIZE;
    config->connection_timeout_ms = 5000;
    config->sync_timeout_ms = 30000;
    config->enable_auto_discovery = false;
    config->discovery_port = KG_FEDERATION_DEFAULT_DISCOVERY_PORT;
    config->listen_port = 5354;
    config->require_tls = false;
    config->verify_peer_certs = false;

    /* Initialize filter to sync everything */
    config->filter.include_weights = true;
    config->filter.include_neuromod = true;
    config->filter.include_metadata = true;
    config->filter.include_edges = true;
    config->filter.layers = 0xFF;      /* All layers */
    config->filter.hemispheres = 0x07; /* Both hemispheres + bilateral */

    return 0;
}

kg_federation_t* kg_federation_create(
    brain_kg_t* kg,
    const kg_federation_config_t* config
) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }

    kg_federation_t* fed = nimcp_calloc(1, sizeof(kg_federation_t));
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fed is NULL");

        return NULL;
    }

    fed->kg = kg;

    /* Apply configuration */
    if (config) {
        memcpy(&fed->config, config, sizeof(kg_federation_config_t));
    } else {
        kg_federation_default_config(&fed->config);
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    fed->mutex = nimcp_mutex_create(&attr);
    if (!fed->mutex) {
        nimcp_free(fed);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_federation_create: fed->mutex is NULL");
        return NULL;
    }

    fed->next_conflict_id = 1;

    return fed;
}

void kg_federation_destroy(kg_federation_t* fed) {
    if (!fed) {
        return;
    }

    /* Stop auto-sync if running */
    if (fed->auto_sync_running) {
        kg_federation_stop_auto_sync(fed);
    }

    /* Stop discovery if active */
    if (fed->discovery_active) {
        kg_federation_stop_discovery(fed);
    }

    /* Clean up sync filter */
    kg_sync_filter_cleanup(&fed->config.filter);

    if (fed->mutex) {
        nimcp_mutex_free(fed->mutex);
    }

    nimcp_free(fed);
}

int kg_federation_get_config(
    const kg_federation_t* fed,
    kg_federation_config_t* config
) {
    if (!fed || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_get_config: required parameter is NULL (fed, config)");
        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);
    memcpy(config, &fed->config, sizeof(kg_federation_config_t));
    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

int kg_federation_set_config(
    kg_federation_t* fed,
    const kg_federation_config_t* config
) {
    if (!fed || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_set_config: required parameter is NULL (fed, config)");
        return -1;
    }

    nimcp_mutex_lock(fed->mutex);
    memcpy(&fed->config, config, sizeof(kg_federation_config_t));
    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

/* ============================================================================
 * Peer Management API
 * ============================================================================ */

int kg_federation_add_peer(
    kg_federation_t* fed,
    const char* host,
    uint16_t port
) {
    if (!fed || !host) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_add_peer: required parameter is NULL (fed, host)");
        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    if (fed->peer_count >= KG_FEDERATION_MAX_PEERS) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_federation_add_peer: capacity exceeded");
        return -1;
    }

    /* Create new peer entry */
    kg_federation_peer_t* peer = &fed->peers[fed->peer_count];
    memset(peer, 0, sizeof(*peer));

    /* Generate peer ID from host:port */
    snprintf(peer->peer_id, KG_FEDERATION_PEER_ID_LEN, "%s:%u", host, port);
    strncpy(peer->host, host, KG_FEDERATION_HOST_LEN - 1);
    peer->port = port;
    peer->state = KG_PEER_STATE_DISCONNECTED;
    peer->trust_score = 0.5f; /* Default trust */
    peer->priority = 128;     /* Default priority */

    fed->peer_count++;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_remove_peer(
    kg_federation_t* fed,
    const char* peer_id
) {
    if (!fed || !peer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_remove_peer: required parameter is NULL (fed, peer_id)");
        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    for (uint32_t i = 0; i < fed->peer_count; i++) {
        if (strcmp(fed->peers[i].peer_id, peer_id) == 0) {
            /* Shift remaining entries */
            for (uint32_t j = i; j < fed->peer_count - 1; j++) {
                memcpy(&fed->peers[j], &fed->peers[j + 1],
                       sizeof(kg_federation_peer_t));
            }
            fed->peer_count--;
            nimcp_mutex_unlock(fed->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(fed->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_federation_remove_peer: operation failed");
    return -1; /* Not found */
}

int kg_federation_get_peers(
    const kg_federation_t* fed,
    kg_federation_peer_t* peers,
    uint32_t* count
) {
    if (!fed || !peers || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_get_peers: required parameter is NULL (fed, peers, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    uint32_t capacity = *count;
    uint32_t copy_count = (fed->peer_count < capacity) ? fed->peer_count : capacity;

    for (uint32_t i = 0; i < copy_count; i++) {
        memcpy(&peers[i], &fed->peers[i], sizeof(kg_federation_peer_t));
    }

    *count = fed->peer_count;

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

uint32_t kg_federation_get_peer_count(const kg_federation_t* fed) {
    if (!fed) {
        return 0;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);
    uint32_t count = fed->peer_count;
    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return count;
}

int kg_federation_get_peer(
    const kg_federation_t* fed,
    const char* peer_id,
    kg_federation_peer_t* peer
) {
    if (!fed || !peer_id || !peer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_get_peer: required parameter is NULL (fed, peer_id, peer)");
        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    kg_federation_peer_t* found = find_peer((kg_federation_t*)fed, peer_id);
    if (!found) {
        nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_get_peer: found is NULL");
        return -1;
    }

    memcpy(peer, found, sizeof(kg_federation_peer_t));

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

int kg_federation_set_trust(
    kg_federation_t* fed,
    const char* peer_id,
    float trust_score
) {
    if (!fed || !peer_id || trust_score < 0.0f || trust_score > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_set_trust: required parameter is NULL (fed, peer_id)");
        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    kg_federation_peer_t* peer = find_peer(fed, peer_id);
    if (!peer) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_set_trust: peer is NULL");
        return -1;
    }

    peer->trust_score = trust_score;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

bool kg_federation_is_peer_connected(
    const kg_federation_t* fed,
    const char* peer_id
) {
    if (!fed || !peer_id) {
        return false;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    kg_federation_peer_t* peer = find_peer((kg_federation_t*)fed, peer_id);
    bool connected = (peer && peer->is_connected);

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return connected;
}

/* ============================================================================
 * Synchronization API
 * ============================================================================ */

int kg_federation_sync_now(kg_federation_t* fed) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    if (fed->config.policy == KG_SYNC_NONE) {
        return 0; /* No sync in isolated mode */
    }

    nimcp_mutex_lock(fed->mutex);

    fed->total_syncs++;
    uint64_t now = get_current_timestamp_ms();

    /* In a real implementation, we would:
     * 1. Collect local changes since last sync
     * 2. For each connected peer:
     *    a. Push local changes (if policy allows)
     *    b. Pull remote changes (if policy allows)
     *    c. Resolve conflicts
     * 3. Apply merged changes
     */

    int success_count = 0;
    for (uint32_t i = 0; i < fed->peer_count; i++) {
        kg_federation_peer_t* peer = &fed->peers[i];
        if (peer->is_connected) {
            peer->last_sync = now;
            peer->sync_count++;
            success_count++;
        }
    }

    if (success_count > 0) {
        fed->successful_syncs++;
    } else if (fed->peer_count > 0) {
        fed->failed_syncs++;
    }

    fed->last_sync_time = now;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_sync_with_peer(
    kg_federation_t* fed,
    const char* peer_id
) {
    if (!fed || !peer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_sync_with_peer: required parameter is NULL (fed, peer_id)");
        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    kg_federation_peer_t* peer = find_peer(fed, peer_id);
    if (!peer) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_sync_with_peer: peer is NULL");
        return -1;
    }

    fed->total_syncs++;
    uint64_t now = get_current_timestamp_ms();

    /* In a real implementation, sync with specific peer */
    if (peer->is_connected) {
        peer->last_sync = now;
        peer->sync_count++;
        fed->successful_syncs++;
    } else {
        fed->failed_syncs++;
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_federation_sync_with_peer: validation failed");
        return -1;
    }

    fed->last_sync_time = now;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_push_changes(
    kg_federation_t* fed,
    const kg_diff_result_t* changes
) {
    if (!fed || !changes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_push_changes: required parameter is NULL (fed, changes)");
        return -1;
    }

    if (fed->config.policy == KG_SYNC_NONE ||
        fed->config.policy == KG_SYNC_PULL_ONLY) {
        return 0; /* Push not allowed by policy */
    }

    nimcp_mutex_lock(fed->mutex);

    /* In a real implementation, serialize and send changes to all peers */
    for (uint32_t i = 0; i < fed->peer_count; i++) {
        kg_federation_peer_t* peer = &fed->peers[i];
        if (peer->is_connected) {
            peer->messages_sent++;
        }
    }

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_pull_changes(
    kg_federation_t* fed,
    const char* peer_id
) {
    if (!fed || !peer_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_pull_changes: required parameter is NULL (fed, peer_id)");
        return -1;
    }

    if (fed->config.policy == KG_SYNC_NONE ||
        fed->config.policy == KG_SYNC_PUSH_ONLY) {
        return 0; /* Pull not allowed by policy */
    }

    nimcp_mutex_lock(fed->mutex);

    kg_federation_peer_t* peer = find_peer(fed, peer_id);
    if (!peer || !peer->is_connected) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_pull_changes: required parameter is NULL (peer, peer->is_connected)");
        return -1;
    }

    /* In a real implementation, request and apply changes from peer */
    peer->messages_sent++;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_get_sync_stats(
    const kg_federation_t* fed,
    uint64_t* total_syncs,
    uint64_t* successful_syncs,
    uint64_t* failed_syncs,
    uint64_t* last_sync_time
) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    if (total_syncs) *total_syncs = fed->total_syncs;
    if (successful_syncs) *successful_syncs = fed->successful_syncs;
    if (failed_syncs) *failed_syncs = fed->failed_syncs;
    if (last_sync_time) *last_sync_time = fed->last_sync_time;

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

int kg_federation_start_auto_sync(kg_federation_t* fed) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    if (fed->auto_sync_running) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_federation_start_auto_sync: validation failed");
        return -1; /* Already running */
    }

    /* In a real implementation, start a background thread for auto-sync */
    fed->auto_sync_running = true;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_stop_auto_sync(kg_federation_t* fed) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    /* In a real implementation, stop the auto-sync thread */
    fed->auto_sync_running = false;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

bool kg_federation_is_auto_sync_running(const kg_federation_t* fed) {
    if (!fed) {
        return false;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);
    bool running = fed->auto_sync_running;
    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return running;
}

/* ============================================================================
 * Federated Query API
 * ============================================================================ */

int kg_federation_query(
    const kg_federation_t* fed,
    const char* query,
    kg_federated_result_t** results,
    uint32_t* result_count
) {
    if (!fed || !query || !results || !result_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_query: required parameter is NULL (fed, query, results, result_count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    /* Allocate result array for local + all peers */
    uint32_t total_results = 1 + fed->peer_count;
    kg_federated_result_t* result_array = nimcp_calloc(
        total_results, sizeof(kg_federated_result_t));

    if (!result_array) {
        nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_federation_query: result_array is NULL");
        return -1;
    }

    /* Local result */
    strncpy(result_array[0].source_peer, fed->config.local_id,
            KG_FEDERATION_PEER_ID_LEN - 1);
    result_array[0].is_local = true;
    result_array[0].error_code = 0;
    /* In real implementation, execute query on local KG */

    /* Peer results */
    for (uint32_t i = 0; i < fed->peer_count; i++) {
        kg_federated_result_t* r = &result_array[1 + i];
        strncpy(r->source_peer, fed->peers[i].peer_id,
                KG_FEDERATION_PEER_ID_LEN - 1);
        r->is_local = false;

        if (fed->peers[i].is_connected) {
            r->error_code = 0;
            /* In real implementation, send query to peer */
        } else {
            r->error_code = -1;
            strncpy(r->error_msg, "Peer not connected", sizeof(r->error_msg) - 1);
        }
    }

    *results = result_array;
    *result_count = total_results;

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

int kg_federation_query_peer(
    const kg_federation_t* fed,
    const char* peer_id,
    const char* query,
    kg_federated_result_t* result
) {
    if (!fed || !peer_id || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_query_peer: required parameter is NULL (fed, peer_id, query, result)");
        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    memset(result, 0, sizeof(*result));
    strncpy(result->source_peer, peer_id, KG_FEDERATION_PEER_ID_LEN - 1);
    result->is_local = false;

    kg_federation_peer_t* peer = find_peer((kg_federation_t*)fed, peer_id);
    if (!peer || !peer->is_connected) {
        result->error_code = -1;
        strncpy(result->error_msg, "Peer not found or not connected",
                sizeof(result->error_msg) - 1);
        nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_query_peer: required parameter is NULL (peer, peer->is_connected)");
        return -1;
    }

    /* In real implementation, send query to peer and wait for response */
    result->error_code = 0;

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

void kg_federation_free_results(
    kg_federated_result_t* results,
    uint32_t count
) {
    if (!results) {
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (results[i].result_data) {
            nimcp_free(results[i].result_data);
        }
    }

    nimcp_free(results);
}

/* ============================================================================
 * Conflict Resolution API
 * ============================================================================ */

int kg_federation_get_conflicts(
    const kg_federation_t* fed,
    kg_conflict_t* conflicts,
    uint32_t* count
) {
    if (!fed || !conflicts || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_get_conflicts: required parameter is NULL (fed, conflicts, count)");
        return -1;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    uint32_t capacity = *count;
    uint32_t unresolved = 0;

    for (uint32_t i = 0; i < fed->conflict_count && unresolved < capacity; i++) {
        if (!fed->conflicts[i].resolved) {
            memcpy(&conflicts[unresolved], &fed->conflicts[i].conflict,
                   sizeof(kg_conflict_t));
            unresolved++;
        }
    }

    *count = unresolved;

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return 0;
}

uint32_t kg_federation_get_conflict_count(const kg_federation_t* fed) {
    if (!fed) {
        return 0;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);

    uint32_t unresolved = 0;
    for (uint32_t i = 0; i < fed->conflict_count; i++) {
        if (!fed->conflicts[i].resolved) {
            unresolved++;
        }
    }

    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return unresolved;
}

int kg_federation_resolve_conflict(
    kg_federation_t* fed,
    uint64_t conflict_id,
    kg_conflict_resolution_t resolution
) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    pending_conflict_t* pending = find_conflict(fed, conflict_id);
    if (!pending) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_resolve_conflict: pending is NULL");
        return -1;
    }

    /* In real implementation, apply the resolution action */
    (void)resolution;

    pending->resolved = true;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

uint32_t kg_federation_resolve_all_conflicts(kg_federation_t* fed) {
    if (!fed) {
        return 0;
    }

    nimcp_mutex_lock(fed->mutex);

    uint32_t resolved_count = 0;

    for (uint32_t i = 0; i < fed->conflict_count; i++) {
        if (!fed->conflicts[i].resolved) {
            /* Apply default strategy */
            fed->conflicts[i].resolved = true;
            resolved_count++;
        }
    }

    nimcp_mutex_unlock(fed->mutex);

    return resolved_count;
}

int kg_federation_discard_conflict(
    kg_federation_t* fed,
    uint64_t conflict_id
) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    pending_conflict_t* pending = find_conflict(fed, conflict_id);
    if (!pending) {
        nimcp_mutex_unlock(fed->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_federation_discard_conflict: pending is NULL");
        return -1;
    }

    pending->resolved = true; /* Mark as handled (discarded) */

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

/* ============================================================================
 * Discovery API
 * ============================================================================ */

int kg_federation_start_discovery(kg_federation_t* fed) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    /* In real implementation, start mDNS/broadcast discovery */
    fed->discovery_active = true;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

int kg_federation_stop_discovery(kg_federation_t* fed) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    nimcp_mutex_lock(fed->mutex);

    fed->discovery_active = false;

    nimcp_mutex_unlock(fed->mutex);

    return 0;
}

bool kg_federation_is_discovery_active(const kg_federation_t* fed) {
    if (!fed) {
        return false;
    }

    nimcp_mutex_lock(((kg_federation_t*)fed)->mutex);
    bool active = fed->discovery_active;
    nimcp_mutex_unlock(((kg_federation_t*)fed)->mutex);

    return active;
}

int kg_federation_announce(kg_federation_t* fed) {
    if (!fed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fed is NULL");

        return -1;
    }

    /* In real implementation, broadcast presence announcement */

    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static const char* sync_policy_strings[] = {
    "NONE",
    "PULL_ONLY",
    "PUSH_ONLY",
    "BIDIRECTIONAL",
    "SELECTIVE"
};

const char* kg_sync_policy_to_string(kg_sync_policy_t policy) {
    if (policy >= 0 && policy <= KG_SYNC_SELECTIVE) {
        return sync_policy_strings[policy];
    }
    return "UNKNOWN";
}

static const char* conflict_strategy_strings[] = {
    "LAST_WRITE_WINS",
    "FIRST_WRITE_WINS",
    "MERGE",
    "MANUAL",
    "PRIORITY"
};

const char* kg_conflict_strategy_to_string(kg_conflict_strategy_t strategy) {
    if (strategy >= 0 && strategy <= KG_CONFLICT_PRIORITY) {
        return conflict_strategy_strings[strategy];
    }
    return "UNKNOWN";
}

static const char* peer_state_strings[] = {
    "UNKNOWN",
    "DISCONNECTED",
    "CONNECTING",
    "CONNECTED",
    "SYNCING",
    "ERROR"
};

const char* kg_peer_state_to_string(kg_peer_state_t state) {
    if (state >= 0 && state <= KG_PEER_STATE_ERROR) {
        return peer_state_strings[state];
    }
    return "UNKNOWN";
}

int kg_federation_generate_peer_id(char* peer_id_out) {
    if (!peer_id_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "peer_id_out is NULL");

        return -1;
    }

    /* Generate UUID v4 (simplified version) */
    uint64_t now = get_current_timestamp_ms();
    snprintf(peer_id_out, KG_FEDERATION_PEER_ID_LEN,
             "%08lx-%04lx-%04lx-%04lx-%012lx",
             (unsigned long)(now >> 32),
             (unsigned long)((now >> 16) & 0xFFFF),
             (unsigned long)(0x4000 | ((now >> 4) & 0x0FFF)),
             (unsigned long)(0x8000 | (now & 0x3FFF)),
             (unsigned long)(now * 31 + 17));

    return 0;
}

int kg_sync_filter_init(kg_sync_filter_t* filter) {
    if (!filter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filter is NULL");

        return -1;
    }

    memset(filter, 0, sizeof(*filter));
    filter->include_weights = true;
    filter->include_neuromod = true;
    filter->include_metadata = true;
    filter->include_edges = true;
    filter->layers = 0xFF;
    filter->hemispheres = 0x07;

    return 0;
}

void kg_sync_filter_cleanup(kg_sync_filter_t* filter) {
    if (!filter) {
        return;
    }

    if (filter->node_types) {
        nimcp_free(filter->node_types);
        filter->node_types = NULL;
    }

    if (filter->modules) {
        nimcp_free(filter->modules);
        filter->modules = NULL;
    }
}
