/**
 * @file nimcp_swarm_lifecycle.c
 * @brief Swarm lifecycle management — peer registry CRUD, heartbeat tracking,
 *        state transitions, and dead peer cleanup.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <unistd.h>

/* ============================================================================
 * Peer State Enum
 * ============================================================================ */

typedef enum {
    NIMCP_PEER_UNKNOWN     = 0,
    NIMCP_PEER_DISCOVERING,
    NIMCP_PEER_JOINING,
    NIMCP_PEER_ACTIVE,
    NIMCP_PEER_SUSPECTED,
    NIMCP_PEER_BYZANTINE,
    NIMCP_PEER_LEAVING,
    NIMCP_PEER_DEAD
} nimcp_peer_state_t;

/* ============================================================================
 * Peer Entry & Registry Structs
 * ============================================================================ */

typedef struct {
    uint32_t device_id;
    nimcp_peer_state_t state;
    char address[256];
    uint16_t port;
    int socket_fd;
    uint64_t last_heartbeat_ts;
    uint32_t missed_heartbeats;
    uint64_t join_timestamp;
    float gradient_norm_ema;
    uint32_t anomaly_count;
    uint32_t total_syncs;
    bool quarantined;
} nimcp_peer_entry_t;

typedef struct {
    nimcp_peer_entry_t* peers;
    uint32_t count;
    uint32_t capacity;
    nimcp_mutex_t* lock;
} nimcp_peer_registry_t;

/* ============================================================================
 * State Name Lookup
 * ============================================================================ */

const char* nimcp_peer_state_name(nimcp_peer_state_t state)
{
    switch (state) {
        case NIMCP_PEER_UNKNOWN:     return "UNKNOWN";
        case NIMCP_PEER_DISCOVERING: return "DISCOVERING";
        case NIMCP_PEER_JOINING:     return "JOINING";
        case NIMCP_PEER_ACTIVE:      return "ACTIVE";
        case NIMCP_PEER_SUSPECTED:   return "SUSPECTED";
        case NIMCP_PEER_BYZANTINE:   return "BYZANTINE";
        case NIMCP_PEER_LEAVING:     return "LEAVING";
        case NIMCP_PEER_DEAD:        return "DEAD";
        default:                     return "INVALID";
    }
}

/* ============================================================================
 * Registry Create / Destroy
 * ============================================================================ */

nimcp_peer_registry_t* nimcp_peer_registry_create(uint32_t capacity)
{
    if (capacity == 0) {
        LOG_ERROR("[edge/lifecycle] Cannot create registry with zero capacity");
        return NULL;
    }

    nimcp_peer_registry_t* registry =
        (nimcp_peer_registry_t*)nimcp_calloc(1, sizeof(nimcp_peer_registry_t));
    if (!registry) {
        LOG_ERROR("[edge/lifecycle] Failed to allocate peer registry");
        return NULL;
    }

    registry->peers =
        (nimcp_peer_entry_t*)nimcp_calloc(capacity, sizeof(nimcp_peer_entry_t));
    if (!registry->peers) {
        LOG_ERROR("[edge/lifecycle] Failed to allocate peer entries (capacity=%u)",
                  capacity);
        nimcp_free(registry);
        return NULL;
    }

    registry->lock = nimcp_mutex_create(NULL);
    if (!registry->lock) {
        LOG_ERROR("[edge/lifecycle] Failed to create registry mutex");
        nimcp_free(registry->peers);
        nimcp_free(registry);
        return NULL;
    }

    registry->count = 0;
    registry->capacity = capacity;

    /* Initialize all socket_fd to -1 (invalid) */
    for (uint32_t i = 0; i < capacity; i++) {
        registry->peers[i].socket_fd = -1;
    }

    LOG_INFO("[edge/lifecycle] Peer registry created (capacity=%u)", capacity);
    return registry;
}

void nimcp_peer_registry_destroy(nimcp_peer_registry_t* registry)
{
    if (!registry) {
        return;
    }

    /* Close any open sockets */
    if (registry->peers) {
        for (uint32_t i = 0; i < registry->count; i++) {
            if (registry->peers[i].socket_fd >= 0) {
                close(registry->peers[i].socket_fd);
                registry->peers[i].socket_fd = -1;
            }
        }
        nimcp_free(registry->peers);
        registry->peers = NULL;
    }

    if (registry->lock) {
        nimcp_mutex_free(registry->lock);
        registry->lock = NULL;
    }

    LOG_INFO("[edge/lifecycle] Peer registry destroyed");
    nimcp_free(registry);
}

/* ============================================================================
 * Internal Helpers (caller must hold lock)
 * ============================================================================ */

/**
 * @brief Find peer index by device_id. Returns index or -1 if not found.
 * @note Caller must hold registry->lock.
 */
static int peer_find_index(const nimcp_peer_registry_t* registry, uint32_t device_id)
{
    for (uint32_t i = 0; i < registry->count; i++) {
        if (registry->peers[i].device_id == device_id) {
            return (int)i;
        }
    }
    return -1;
}

/* ============================================================================
 * Peer Add / Remove / Find
 * ============================================================================ */

int nimcp_peer_registry_add(nimcp_peer_registry_t* registry,
                            uint32_t device_id,
                            const char* address,
                            uint16_t port,
                            const nimcp_device_profile_t* profile)
{
    if (!registry) {
        LOG_ERROR("[edge/lifecycle] NULL registry in add");
        return -1;
    }
    if (!address) {
        LOG_ERROR("[edge/lifecycle] NULL address in add");
        return -1;
    }

    nimcp_mutex_lock(registry->lock);

    /* Check for duplicate */
    if (peer_find_index(registry, device_id) >= 0) {
        LOG_WARN("[edge/lifecycle] Peer %u already in registry", device_id);
        nimcp_mutex_unlock(registry->lock);
        return -1;
    }

    /* Check capacity */
    if (registry->count >= registry->capacity) {
        LOG_WARN("[edge/lifecycle] Registry full (%u/%u), cannot add peer %u",
                 registry->count, registry->capacity, device_id);
        nimcp_mutex_unlock(registry->lock);
        return -1;
    }

    /* Add at end */
    nimcp_peer_entry_t* entry = &registry->peers[registry->count];
    memset(entry, 0, sizeof(nimcp_peer_entry_t));

    entry->device_id = device_id;
    entry->state = NIMCP_PEER_JOINING;
    strncpy(entry->address, address, sizeof(entry->address) - 1);
    entry->address[sizeof(entry->address) - 1] = '\0';
    entry->port = port;
    entry->socket_fd = -1;

    uint64_t now = nimcp_time_now_us();
    entry->last_heartbeat_ts = now;
    entry->join_timestamp = now;
    entry->missed_heartbeats = 0;
    entry->gradient_norm_ema = 0.0f;
    entry->anomaly_count = 0;
    entry->total_syncs = 0;
    entry->quarantined = false;

    registry->count++;

    LOG_INFO("[edge/lifecycle] Peer %u added (%s:%u), state=JOINING, count=%u",
             device_id, address, port, registry->count);

    nimcp_mutex_unlock(registry->lock);
    return 0;
}

int nimcp_peer_registry_remove(nimcp_peer_registry_t* registry, uint32_t device_id)
{
    if (!registry) {
        LOG_ERROR("[edge/lifecycle] NULL registry in remove");
        return -1;
    }

    nimcp_mutex_lock(registry->lock);

    int idx = peer_find_index(registry, device_id);
    if (idx < 0) {
        LOG_WARN("[edge/lifecycle] Peer %u not found for removal", device_id);
        nimcp_mutex_unlock(registry->lock);
        return -1;
    }

    /* Close socket if open */
    if (registry->peers[idx].socket_fd >= 0) {
        close(registry->peers[idx].socket_fd);
        registry->peers[idx].socket_fd = -1;
    }

    /* Swap with last entry to maintain contiguous array */
    uint32_t last = registry->count - 1;
    if ((uint32_t)idx != last) {
        memcpy(&registry->peers[idx], &registry->peers[last],
               sizeof(nimcp_peer_entry_t));
    }
    memset(&registry->peers[last], 0, sizeof(nimcp_peer_entry_t));
    registry->peers[last].socket_fd = -1;
    registry->count--;

    LOG_INFO("[edge/lifecycle] Peer %u removed, count=%u", device_id, registry->count);

    nimcp_mutex_unlock(registry->lock);
    return 0;
}

/**
 * NOTE: Returns a direct pointer into the registry array. Caller MUST
 * either hold registry->lock OR use the pointer within the same locked
 * section. For thread-safe access, use nimcp_peer_registry_copy_entry().
 */
nimcp_peer_entry_t* nimcp_peer_registry_find(nimcp_peer_registry_t* registry,
                                              uint32_t device_id)
{
    if (!registry) {
        return NULL;
    }

    int idx = peer_find_index(registry, device_id);
    if (idx < 0) {
        return NULL;
    }

    return &registry->peers[idx];
}

/**
 * @brief Thread-safe copy of a peer entry by device_id.
 *
 * Locks the registry, copies the entry to the caller's output buffer,
 * and unlocks. The caller owns the copy and can use it freely.
 *
 * @param registry  Peer registry.
 * @param device_id Device ID to look up.
 * @param out       Output buffer (caller-allocated).
 * @return 0 on success, -1 if not found or invalid args.
 */
int nimcp_peer_registry_copy_entry(nimcp_peer_registry_t* registry,
    uint32_t device_id, nimcp_peer_entry_t* out)
{
    if (!registry || !out) return -1;
    nimcp_mutex_lock(registry->lock);
    int idx = peer_find_index(registry, device_id);
    if (idx < 0) {
        nimcp_mutex_unlock(registry->lock);
        return -1;
    }
    memcpy(out, &registry->peers[idx], sizeof(nimcp_peer_entry_t));
    nimcp_mutex_unlock(registry->lock);
    return 0;
}

/* ============================================================================
 * Heartbeat & State Management
 * ============================================================================ */

int nimcp_peer_registry_update_heartbeat(nimcp_peer_registry_t* registry,
                                          uint32_t device_id)
{
    if (!registry) {
        LOG_ERROR("[edge/lifecycle] NULL registry in update_heartbeat");
        return -1;
    }

    nimcp_mutex_lock(registry->lock);

    int idx = peer_find_index(registry, device_id);
    if (idx < 0) {
        LOG_WARN("[edge/lifecycle] Heartbeat for unknown peer %u", device_id);
        nimcp_mutex_unlock(registry->lock);
        return -1;
    }

    nimcp_peer_entry_t* entry = &registry->peers[idx];
    entry->last_heartbeat_ts = nimcp_time_now_us();
    entry->missed_heartbeats = 0;

    /* Recover from suspected state */
    if (entry->state == NIMCP_PEER_SUSPECTED) {
        LOG_INFO("[edge/lifecycle] Peer %u recovered: SUSPECTED -> ACTIVE", device_id);
        entry->state = NIMCP_PEER_ACTIVE;
    }

    nimcp_mutex_unlock(registry->lock);
    return 0;
}

int nimcp_peer_registry_set_state(nimcp_peer_registry_t* registry,
                                   uint32_t device_id,
                                   nimcp_peer_state_t new_state)
{
    if (!registry) {
        LOG_ERROR("[edge/lifecycle] NULL registry in set_state");
        return -1;
    }

    nimcp_mutex_lock(registry->lock);

    int idx = peer_find_index(registry, device_id);
    if (idx < 0) {
        LOG_WARN("[edge/lifecycle] Cannot set state for unknown peer %u", device_id);
        nimcp_mutex_unlock(registry->lock);
        return -1;
    }

    nimcp_peer_state_t old_state = registry->peers[idx].state;
    registry->peers[idx].state = new_state;

    LOG_INFO("[edge/lifecycle] Peer %u state: %s -> %s",
             device_id, nimcp_peer_state_name(old_state),
             nimcp_peer_state_name(new_state));

    nimcp_mutex_unlock(registry->lock);
    return 0;
}

/* ============================================================================
 * Sweep — detect dead peers
 * ============================================================================ */

uint32_t nimcp_peer_registry_sweep(nimcp_peer_registry_t* registry,
                                    uint32_t heartbeat_timeout_ms,
                                    uint32_t dead_timeout_ms)
{
    if (!registry) {
        return 0;
    }

    nimcp_mutex_lock(registry->lock);

    uint64_t now = nimcp_time_now_us();
    uint64_t heartbeat_timeout_us = (uint64_t)heartbeat_timeout_ms * 1000ULL;
    uint64_t dead_timeout_us = (uint64_t)dead_timeout_ms * 1000ULL;
    uint32_t newly_dead = 0;

    for (uint32_t i = 0; i < registry->count; i++) {
        nimcp_peer_entry_t* entry = &registry->peers[i];

        /* Only sweep peers in trackable states */
        if (entry->state == NIMCP_PEER_DEAD ||
            entry->state == NIMCP_PEER_LEAVING ||
            entry->state == NIMCP_PEER_UNKNOWN) {
            continue;
        }

        uint64_t elapsed = 0;
        if (now > entry->last_heartbeat_ts) {
            elapsed = now - entry->last_heartbeat_ts;
        }

        /* ACTIVE -> SUSPECTED after heartbeat timeout */
        if (entry->state == NIMCP_PEER_ACTIVE && elapsed > heartbeat_timeout_us) {
            entry->missed_heartbeats++;
            LOG_WARN("[edge/lifecycle] Peer %u: ACTIVE -> SUSPECTED "
                     "(missed_heartbeats=%u, elapsed=%llu us)",
                     entry->device_id, entry->missed_heartbeats,
                     (unsigned long long)elapsed);
            entry->state = NIMCP_PEER_SUSPECTED;
        }

        /* SUSPECTED -> DEAD after dead timeout */
        if (entry->state == NIMCP_PEER_SUSPECTED && elapsed > dead_timeout_us) {
            LOG_WARN("[edge/lifecycle] Peer %u: SUSPECTED -> DEAD "
                     "(elapsed=%llu us, dead_timeout=%llu us)",
                     entry->device_id,
                     (unsigned long long)elapsed,
                     (unsigned long long)dead_timeout_us);
            entry->state = NIMCP_PEER_DEAD;

            /* Close socket for dead peer */
            if (entry->socket_fd >= 0) {
                close(entry->socket_fd);
                entry->socket_fd = -1;
            }

            newly_dead++;
        }

        /* JOINING peers that never became active */
        if (entry->state == NIMCP_PEER_JOINING && elapsed > dead_timeout_us) {
            LOG_WARN("[edge/lifecycle] Peer %u: JOINING -> DEAD "
                     "(never became active, elapsed=%llu us)",
                     entry->device_id, (unsigned long long)elapsed);
            entry->state = NIMCP_PEER_DEAD;

            if (entry->socket_fd >= 0) {
                close(entry->socket_fd);
                entry->socket_fd = -1;
            }

            newly_dead++;
        }
    }

    nimcp_mutex_unlock(registry->lock);

    if (newly_dead > 0) {
        LOG_INFO("[edge/lifecycle] Sweep complete: %u newly dead peers", newly_dead);
    }

    return newly_dead;
}

/* ============================================================================
 * Active Peer Queries
 * ============================================================================ */

uint32_t nimcp_peer_registry_get_active_count(nimcp_peer_registry_t* registry)
{
    if (!registry) {
        return 0;
    }

    nimcp_mutex_lock(registry->lock);

    uint32_t active = 0;
    for (uint32_t i = 0; i < registry->count; i++) {
        if (registry->peers[i].state == NIMCP_PEER_ACTIVE) {
            active++;
        }
    }

    nimcp_mutex_unlock(registry->lock);
    return active;
}

uint32_t nimcp_peer_registry_get_active_peers(nimcp_peer_registry_t* registry,
                                               uint32_t* out_ids,
                                               uint32_t max_count)
{
    if (!registry || !out_ids || max_count == 0) {
        return 0;
    }

    nimcp_mutex_lock(registry->lock);

    uint32_t filled = 0;
    for (uint32_t i = 0; i < registry->count && filled < max_count; i++) {
        if (registry->peers[i].state == NIMCP_PEER_ACTIVE) {
            out_ids[filled++] = registry->peers[i].device_id;
        }
    }

    nimcp_mutex_unlock(registry->lock);
    return filled;
}
