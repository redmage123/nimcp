/**
 * @file nimcp_mesh_health_bridge.c
 * @brief Health Agent Mesh Integration Bridge Implementation
 *
 * WHAT: Integrates health agent heartbeats with mesh participants
 * WHY:  Distributed health monitoring through mesh consensus
 * HOW:  Heartbeat routing, health aggregation, mesh-wide status
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

#define HEALTH_BRIDGE_MAGIC 0x48454C54  /* "HELT" */
#define MAX_HEALTH_RECORDS 512

/**
 * @brief Internal health record entry
 */
typedef struct health_record_entry {
    mesh_health_record_t record;
    health_agent_t* agent;
    bool active;
} health_record_entry_t;

/**
 * @brief Internal health bridge structure
 */
struct mesh_health_bridge {
    bridge_base_t base;
    uint32_t magic;
    mesh_health_bridge_config_t config;

    /* Dependencies */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* integration;

    /* Health records */
    health_record_entry_t records[MAX_HEALTH_RECORDS];
    size_t record_count;

    /* Statistics */
    mesh_health_bridge_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_health_bridge_default_config(
    mesh_health_bridge_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    config->default_heartbeat_interval_ms = 1000;
    config->missed_heartbeat_threshold = 3;
    config->dead_threshold = 5;

    config->weight_cpu = 0.15f;
    config->weight_memory = 0.15f;
    config->weight_tx_success = 0.30f;
    config->weight_latency = 0.20f;
    config->weight_errors = 0.20f;

    config->degraded_threshold = 0.7f;
    config->unhealthy_threshold = 0.5f;
    config->critical_threshold = 0.3f;

    config->route_heartbeats_through_mesh = true;
    config->enable_consensus_health = true;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_health_bridge_t* mesh_health_bridge_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_health_bridge_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create health bridge without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_health_bridge_create: bootstrap is NULL");
        return NULL;
    }

    mesh_health_bridge_config_t default_config;
    if (!config) {
        mesh_health_bridge_default_config(&default_config);
        config = &default_config;
    }

    mesh_health_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR("Failed to allocate health bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_health_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = HEALTH_BRIDGE_MAGIC;
    bridge->config = *config;
    bridge->bootstrap = bootstrap;
    bridge->integration = mesh_bootstrap_get_integration(bootstrap);

    /* Create mutex */
    mutex_attr_t attr = {0};
    bridge->mutex = nimcp_mutex_create(&attr);
    if (!bridge->mutex) {
        LOG_ERROR("Failed to create bridge mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_health_bridge_create: bridge->mutex is NULL");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "mesh_health_bridge");

    LOG_DEBUG("Health bridge created");
    return bridge;
}

void mesh_health_bridge_destroy(mesh_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) return;

    nimcp_mutex_lock(bridge->mutex);
    /* Cleanup */
    nimcp_mutex_unlock(bridge->mutex);

    nimcp_mutex_destroy(bridge->mutex);
    bridge_base_cleanup(&bridge->base);
    bridge->magic = 0;
    nimcp_free(bridge);

    LOG_DEBUG("Health bridge destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static int find_record_by_id(
    const mesh_health_bridge_t* bridge,
    mesh_participant_id_t id
) {
    for (size_t i = 0; i < bridge->record_count; i++) {
        if (bridge->records[i].active &&
            bridge->records[i].record.participant_id == id) {
            return (int)i;
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return -1;
}

static float compute_health_score(
    const mesh_health_bridge_t* bridge,
    const mesh_health_record_t* record
) {
    const mesh_health_bridge_config_t* cfg = &bridge->config;
    float score = 1.0f;

    /* CPU: higher usage = lower score */
    score -= cfg->weight_cpu * record->cpu_usage;

    /* Memory: higher usage = lower score */
    score -= cfg->weight_memory * record->memory_usage;

    /* Transaction success: lower success = lower score */
    score -= cfg->weight_tx_success * (1.0f - record->transaction_success_rate);

    /* Latency: higher latency = lower score (normalized) */
    float latency_factor = record->avg_latency_ms / 1000.0f;
    if (latency_factor > 1.0f) latency_factor = 1.0f;
    score -= cfg->weight_latency * latency_factor;

    /* Errors: more errors = lower score */
    float error_factor = (float)record->error_count / 100.0f;
    if (error_factor > 1.0f) error_factor = 1.0f;
    score -= cfg->weight_errors * error_factor;

    /* Missed heartbeats penalty */
    score -= 0.1f * record->missed_heartbeats;

    /* Clamp */
    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    return score;
}

static mesh_health_status_t score_to_status(
    const mesh_health_bridge_t* bridge,
    float score,
    uint32_t missed_heartbeats
) {
    const mesh_health_bridge_config_t* cfg = &bridge->config;

    if (missed_heartbeats >= cfg->dead_threshold) {
        return MESH_HEALTH_DEAD;
    }
    if (score < cfg->critical_threshold) {
        return MESH_HEALTH_CRITICAL;
    }
    if (score < cfg->unhealthy_threshold) {
        return MESH_HEALTH_UNHEALTHY;
    }
    if (score < cfg->degraded_threshold) {
        return MESH_HEALTH_DEGRADED;
    }
    return MESH_HEALTH_HEALTHY;
}

static void update_status_stats(
    mesh_health_bridge_t* bridge,
    mesh_health_status_t old_status,
    mesh_health_status_t new_status
) {
    if (old_status != new_status) {
        bridge->stats.status_changes++;

        if (new_status == MESH_HEALTH_DEAD) {
            bridge->stats.dead_detections++;
        }
        if (old_status == MESH_HEALTH_DEAD && new_status != MESH_HEALTH_DEAD) {
            bridge->stats.recovery_detections++;
        }
    }

    /* Recount current status */
    bridge->stats.current_healthy = 0;
    bridge->stats.current_degraded = 0;
    bridge->stats.current_unhealthy = 0;
    bridge->stats.current_critical = 0;
    bridge->stats.current_dead = 0;

    for (size_t i = 0; i < bridge->record_count; i++) {
        if (!bridge->records[i].active) continue;

        switch (bridge->records[i].record.status) {
            case MESH_HEALTH_HEALTHY:  bridge->stats.current_healthy++; break;
            case MESH_HEALTH_DEGRADED: bridge->stats.current_degraded++; break;
            case MESH_HEALTH_UNHEALTHY: bridge->stats.current_unhealthy++; break;
            case MESH_HEALTH_CRITICAL: bridge->stats.current_critical++; break;
            case MESH_HEALTH_DEAD:     bridge->stats.current_dead++; break;
            default: break;
        }
    }
}

/* ============================================================================
 * Registration API
 * ============================================================================ */

nimcp_error_t mesh_health_bridge_register_agent(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    health_agent_t* agent
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Check if already registered */
    int idx = find_record_by_id(bridge, participant_id);
    if (idx >= 0) {
        /* Update agent */
        bridge->records[idx].agent = agent;
        nimcp_mutex_unlock(bridge->mutex);
        return NIMCP_SUCCESS;
    }

    /* Find slot */
    if (bridge->record_count >= MAX_HEALTH_RECORDS) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_health_bridge: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Add new record */
    health_record_entry_t* entry = &bridge->records[bridge->record_count++];
    memset(entry, 0, sizeof(*entry));
    entry->record.participant_id = participant_id;
    entry->record.status = MESH_HEALTH_UNKNOWN;
    entry->record.health_score = 1.0f;
    entry->record.heartbeat_interval_ms = bridge->config.default_heartbeat_interval_ms;
    entry->record.transaction_success_rate = 1.0f;
    entry->agent = agent;
    entry->active = true;

    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.verbose_logging) {
        LOG_DEBUG("Registered health agent for participant 0x%llx",
                 (unsigned long long)participant_id);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_health_bridge_unregister_agent(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    int idx = find_record_by_id(bridge, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_health_bridge: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->records[idx].active = false;
    bridge->records[idx].agent = NULL;

    /* Compact array */
    for (size_t i = idx; i < bridge->record_count - 1; i++) {
        bridge->records[i] = bridge->records[i + 1];
    }
    bridge->record_count--;

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Heartbeat API
 * ============================================================================ */

nimcp_error_t mesh_health_bridge_heartbeat(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    mesh_heartbeat_op_t op,
    uint8_t progress
) {
    (void)progress;  /* Used for PROGRESS ops */

    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.heartbeats_received++;

    int idx = find_record_by_id(bridge, participant_id);
    if (idx < 0) {
        /* Auto-register if not found */
        nimcp_mutex_unlock(bridge->mutex);
        mesh_health_bridge_register_agent(bridge, participant_id, NULL);
        nimcp_mutex_lock(bridge->mutex);
        idx = find_record_by_id(bridge, participant_id);
        if (idx < 0) {
            nimcp_mutex_unlock(bridge->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SYSTEM, "mesh_health_bridge: error condition");
            return NIMCP_ERROR_SYSTEM;
        }
    }

    mesh_health_record_t* record = &bridge->records[idx].record;
    mesh_health_status_t old_status = record->status;

    record->last_heartbeat_ns = nimcp_time_now_ns();
    record->missed_heartbeats = 0;

    /* Handle operation type */
    if (op == MESH_HEARTBEAT_ERROR) {
        record->error_count++;
        record->last_error_ns = record->last_heartbeat_ns;
    }

    /* Recompute health score */
    record->health_score = compute_health_score(bridge, record);
    record->status = score_to_status(bridge, record->health_score,
                                     record->missed_heartbeats);

    update_status_stats(bridge, old_status, record->status);

    /* Route through mesh if enabled */
    if (bridge->config.route_heartbeats_through_mesh && bridge->integration) {
        bridge->stats.heartbeats_routed++;
        /* Would create and submit heartbeat transaction here */
    }

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_health_bridge_update_metrics(
    mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    const health_metrics_t* metrics
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!metrics) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);

    int idx = find_record_by_id(bridge, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(bridge->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_health_bridge: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_health_record_t* record = &bridge->records[idx].record;

    /* Update from health_metrics_t */
    record->cpu_usage = metrics->cpu_utilization;
    record->memory_usage = metrics->memory_utilization;
    /* Calculate success rate from transaction counts */
    if (metrics->transactions_processed > 0) {
        record->transaction_success_rate = 1.0f -
            ((float)metrics->transactions_failed / (float)metrics->transactions_processed);
    } else {
        record->transaction_success_rate = 1.0f;
    }
    record->avg_latency_ms = metrics->avg_latency_ms;

    /* Recompute */
    record->health_score = compute_health_score(bridge, record);
    mesh_health_status_t old_status = record->status;
    record->status = score_to_status(bridge, record->health_score,
                                     record->missed_heartbeats);
    update_status_stats(bridge, old_status, record->status);

    nimcp_mutex_unlock(bridge->mutex);
    return NIMCP_SUCCESS;
}

size_t mesh_health_bridge_check_heartbeats(mesh_health_bridge_t* bridge) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) return 0;

    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.health_checks_performed++;

    uint64_t now = nimcp_time_now_ns();
    size_t dead_count = 0;

    for (size_t i = 0; i < bridge->record_count; i++) {
        if (!bridge->records[i].active) continue;

        mesh_health_record_t* record = &bridge->records[i].record;
        uint64_t interval_ns = record->heartbeat_interval_ms * 1000000ULL;

        if (now - record->last_heartbeat_ns > interval_ns) {
            mesh_health_status_t old_status = record->status;
            record->missed_heartbeats++;

            record->health_score = compute_health_score(bridge, record);
            record->status = score_to_status(bridge, record->health_score,
                                            record->missed_heartbeats);

            if (record->status == MESH_HEALTH_DEAD &&
                old_status != MESH_HEALTH_DEAD) {
                dead_count++;
                if (bridge->config.verbose_logging) {
                    LOG_WARN("Participant 0x%llx marked as dead",
                            (unsigned long long)record->participant_id);
                }
            }

            update_status_stats(bridge, old_status, record->status);
        }
    }

    nimcp_mutex_unlock(bridge->mutex);
    return dead_count;
}

/* ============================================================================
 * Health Query API
 * ============================================================================ */

nimcp_error_t mesh_health_bridge_get_health(
    const mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id,
    mesh_health_record_t* record_out
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!record_out) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_health_bridge_t*)bridge)->mutex);

    int idx = find_record_by_id(bridge, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(((mesh_health_bridge_t*)bridge)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_health_bridge: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    *record_out = bridge->records[idx].record;

    nimcp_mutex_unlock(((mesh_health_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_health_bridge_get_channel_health(
    const mesh_health_bridge_t* bridge,
    mesh_channel_id_t channel_id,
    mesh_channel_health_t* health_out
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!health_out) return NIMCP_ERROR_NULL_POINTER;

    memset(health_out, 0, sizeof(*health_out));
    health_out->channel_id = channel_id;
    health_out->min_health_score = 1.0f;

    nimcp_mutex_lock(((mesh_health_bridge_t*)bridge)->mutex);

    float total_score = 0.0f;

    for (size_t i = 0; i < bridge->record_count; i++) {
        if (!bridge->records[i].active) continue;

        /* Check if participant belongs to channel */
        mesh_channel_id_t ch = mesh_get_channel(
            bridge->records[i].record.participant_id);
        if (ch != channel_id) continue;

        const mesh_health_record_t* rec = &bridge->records[i].record;
        health_out->total_participants++;

        switch (rec->status) {
            case MESH_HEALTH_HEALTHY:  health_out->healthy_participants++; break;
            case MESH_HEALTH_DEGRADED: health_out->degraded_participants++; break;
            case MESH_HEALTH_UNHEALTHY:
            case MESH_HEALTH_CRITICAL: health_out->unhealthy_participants++; break;
            case MESH_HEALTH_DEAD:     health_out->dead_participants++; break;
            default: break;
        }

        total_score += rec->health_score;
        if (rec->health_score < health_out->min_health_score) {
            health_out->min_health_score = rec->health_score;
        }
    }

    if (health_out->total_participants > 0) {
        health_out->avg_health_score = total_score /
            (float)health_out->total_participants;
    }

    /* Determine overall status */
    float healthy_ratio = (float)health_out->healthy_participants /
        (float)(health_out->total_participants > 0 ?
                health_out->total_participants : 1);

    if (healthy_ratio >= 0.9f) {
        health_out->status = MESH_HEALTH_HEALTHY;
    } else if (healthy_ratio >= 0.7f) {
        health_out->status = MESH_HEALTH_DEGRADED;
    } else if (healthy_ratio >= 0.5f) {
        health_out->status = MESH_HEALTH_UNHEALTHY;
    } else {
        health_out->status = MESH_HEALTH_CRITICAL;
    }

    nimcp_mutex_unlock(((mesh_health_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_health_bridge_get_system_health(
    const mesh_health_bridge_t* bridge,
    mesh_system_health_t* health_out
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!health_out) return NIMCP_ERROR_NULL_POINTER;

    memset(health_out, 0, sizeof(*health_out));
    health_out->computed_at_ns = nimcp_time_now_ns();

    /* Get health for each standard channel */
    mesh_channel_id_t channels[] = {
        MESH_CHANNEL_SYSTEM,
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_CHANNEL_RIGHT_HEMISPHERE,
        MESH_CHANNEL_SUBCORTICAL,
        MESH_CHANNEL_GPU_COMPUTE
    };

    for (size_t i = 0; i < sizeof(channels)/sizeof(channels[0]); i++) {
        mesh_health_bridge_get_channel_health(
            bridge, channels[i], &health_out->channels[i]);
        health_out->total_participants +=
            health_out->channels[i].total_participants;
    }
    health_out->channel_count = sizeof(channels)/sizeof(channels[0]);

    /* Compute overall system health */
    nimcp_mutex_lock(((mesh_health_bridge_t*)bridge)->mutex);

    float total_score = 0.0f;
    size_t healthy_count = bridge->stats.current_healthy +
                           bridge->stats.current_degraded;

    for (size_t i = 0; i < bridge->record_count; i++) {
        if (!bridge->records[i].active) continue;
        total_score += bridge->records[i].record.health_score;
    }

    if (bridge->record_count > 0) {
        health_out->system_health_score = total_score /
            (float)bridge->record_count;
        health_out->healthy_percentage = (healthy_count * 100) /
            bridge->record_count;
    }

    /* Determine overall status */
    if (health_out->system_health_score >= 0.8f) {
        health_out->status = MESH_HEALTH_HEALTHY;
    } else if (health_out->system_health_score >= 0.6f) {
        health_out->status = MESH_HEALTH_DEGRADED;
    } else if (health_out->system_health_score >= 0.4f) {
        health_out->status = MESH_HEALTH_UNHEALTHY;
    } else {
        health_out->status = MESH_HEALTH_CRITICAL;
    }

    /* Would get coordinator and ordering service status here */
    health_out->leader_healthy = true;
    health_out->ordering_service_healthy = true;

    nimcp_mutex_unlock(((mesh_health_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

bool mesh_health_bridge_is_healthy(
    const mesh_health_bridge_t* bridge,
    mesh_participant_id_t participant_id
) {
    mesh_health_record_t record;
    if (mesh_health_bridge_get_health(bridge, participant_id, &record)
        != NIMCP_SUCCESS) {
        return false;
    }
    return record.status == MESH_HEALTH_HEALTHY ||
           record.status == MESH_HEALTH_DEGRADED;
}

bool mesh_health_bridge_is_channel_healthy(
    const mesh_health_bridge_t* bridge,
    mesh_channel_id_t channel_id
) {
    mesh_channel_health_t health;
    if (mesh_health_bridge_get_channel_health(bridge, channel_id, &health)
        != NIMCP_SUCCESS) {
        return false;
    }
    return health.status == MESH_HEALTH_HEALTHY ||
           health.status == MESH_HEALTH_DEGRADED;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_health_bridge_get_stats(
    const mesh_health_bridge_t* bridge,
    mesh_health_bridge_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_health_bridge_get_stats: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_health_bridge_t*)bridge)->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((mesh_health_bridge_t*)bridge)->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_health_bridge_reset_stats(
    mesh_health_bridge_t* bridge
) {
    if (!bridge || bridge->magic != HEALTH_BRIDGE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_health_bridge: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_health_status_to_string(mesh_health_status_t status) {
    switch (status) {
        case MESH_HEALTH_UNKNOWN:   return "UNKNOWN";
        case MESH_HEALTH_HEALTHY:   return "HEALTHY";
        case MESH_HEALTH_DEGRADED:  return "DEGRADED";
        case MESH_HEALTH_UNHEALTHY: return "UNHEALTHY";
        case MESH_HEALTH_CRITICAL:  return "CRITICAL";
        case MESH_HEALTH_DEAD:      return "DEAD";
        default:                    return "INVALID";
    }
}

const char* mesh_heartbeat_op_to_string(mesh_heartbeat_op_t op) {
    switch (op) {
        case MESH_HEARTBEAT_START:    return "START";
        case MESH_HEARTBEAT_PROGRESS: return "PROGRESS";
        case MESH_HEARTBEAT_COMPLETE: return "COMPLETE";
        case MESH_HEARTBEAT_ERROR:    return "ERROR";
        case MESH_HEARTBEAT_PING:     return "PING";
        default:                      return "UNKNOWN";
    }
}
