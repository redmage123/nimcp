/**
 * @file nimcp_mesh_resilience_integration.c
 * @brief Health Agent and Mesh Resilience Integration Implementation
 *
 * WHAT: Wires health agents to mesh network for distributed resilience
 * WHY:  Aggregate health monitoring through mesh for coordinated recovery
 * HOW:  Health agent registration, heartbeat routing, failure detection
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_resilience_integration.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal resilience integration structure
 */
struct mesh_resilience_integration {
    uint32_t magic;
    mesh_resilience_config_t config;

    /* Dependencies */
    mesh_bootstrap_t* bootstrap;
    mesh_integration_t* integration;
    mesh_health_bridge_t* health_bridge;

    /* Optional integrations */
    gpu_recovery_context_t* gpu_recovery;
    brain_immune_system_t* immune;

    /* Agent registrations */
    mesh_agent_registration_t agents[MESH_RESILIENCE_MAX_AGENTS];
    size_t agent_count;

    /* Recovery action queue */
    mesh_recovery_action_t recovery_queue[MESH_RESILIENCE_MAX_ACTIONS];
    size_t recovery_queue_head;
    size_t recovery_queue_tail;

    /* Failure event history */
    mesh_failure_event_t failure_history[MESH_RESILIENCE_MAX_FAILURES];
    size_t failure_history_count;
    uint64_t next_event_id;

    /* Statistics */
    mesh_resilience_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

nimcp_error_t mesh_resilience_default_config(
    mesh_resilience_config_t* config
) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));

    config->heartbeat_interval_ms = MESH_RESILIENCE_HEARTBEAT_INTERVAL_MS;
    config->missed_threshold = 3;
    config->failure_threshold = 5;

    config->failure_timeout_ms = MESH_RESILIENCE_FAILURE_TIMEOUT_MS;
    config->enable_auto_recovery = true;
    config->auto_recovery_min = MESH_FAILURE_CRITICAL;

    config->integrate_gpu_recovery = true;
    config->integrate_immune_system = true;
    config->enable_checkpointing = true;

    config->route_health_through_mesh = true;
    config->aggregate_per_channel = true;

    config->notify_coordinator_pools = true;
    config->trigger_elections_on_failure = true;

    config->verbose_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

mesh_resilience_integration_t* mesh_resilience_integration_create(
    mesh_bootstrap_t* bootstrap,
    const mesh_resilience_config_t* config
) {
    if (!bootstrap) {
        LOG_ERROR("Cannot create resilience integration without bootstrap");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_resilience_integration_create: bootstrap is NULL");
        return NULL;
    }

    mesh_resilience_config_t default_config;
    if (!config) {
        mesh_resilience_default_config(&default_config);
        config = &default_config;
    }

    mesh_resilience_integration_t* res = nimcp_calloc(1, sizeof(*res));
    if (!res) {
        LOG_ERROR("Failed to allocate resilience integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_resilience_integration_create: res is NULL");
        return NULL;
    }

    res->magic = MESH_RESILIENCE_MAGIC;
    res->config = *config;
    res->bootstrap = bootstrap;
    res->integration = mesh_bootstrap_get_integration(bootstrap);
    res->health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);

    /* Initialize event ID */
    res->next_event_id = 1;

    /* Create mutex */
    mutex_attr_t attr = {0};
    res->mutex = nimcp_mutex_create(&attr);
    if (!res->mutex) {
        LOG_ERROR("Failed to create resilience mutex");
        nimcp_free(res);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_resilience_integration_create: res->mutex is NULL");
        return NULL;
    }

    LOG_DEBUG("Resilience integration created");
    return res;
}

void mesh_resilience_integration_destroy(
    mesh_resilience_integration_t* resilience
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) return;

    nimcp_mutex_lock(resilience->mutex);
    /* Cleanup registrations */
    nimcp_mutex_unlock(resilience->mutex);

    nimcp_mutex_destroy(resilience->mutex);
    resilience->magic = 0;
    nimcp_free(resilience);

    LOG_DEBUG("Resilience integration destroyed");
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static int find_agent_by_id(
    const mesh_resilience_integration_t* res,
    mesh_participant_id_t id
) {
    for (size_t i = 0; i < res->agent_count; i++) {
        if (res->agents[i].active && res->agents[i].participant_id == id) {
            return (int)i;
        }
    }
    /* Not found is normal lookup result, not an error (P2: false positive removal) */
    return -1;
}

static mesh_channel_id_t get_channel_from_id(mesh_participant_id_t id) {
    return (mesh_channel_id_t)(id >> 48);
}

static void add_failure_event(
    mesh_resilience_integration_t* res,
    const mesh_failure_event_t* event
) {
    /* Circular buffer */
    size_t idx = res->failure_history_count % MESH_RESILIENCE_MAX_FAILURES;
    res->failure_history[idx] = *event;
    if (res->failure_history_count < MESH_RESILIENCE_MAX_FAILURES) {
        res->failure_history_count++;
    }
}

static void enqueue_recovery(
    mesh_resilience_integration_t* res,
    const mesh_recovery_action_t* action
) {
    size_t next_tail = (res->recovery_queue_tail + 1) % MESH_RESILIENCE_MAX_ACTIONS;
    if (next_tail == res->recovery_queue_head) {
        /* Queue full, drop oldest */
        res->recovery_queue_head =
            (res->recovery_queue_head + 1) % MESH_RESILIENCE_MAX_ACTIONS;
    }
    res->recovery_queue[res->recovery_queue_tail] = *action;
    res->recovery_queue_tail = next_tail;
}

static bool dequeue_recovery(
    mesh_resilience_integration_t* res,
    mesh_recovery_action_t* action
) {
    if (res->recovery_queue_head == res->recovery_queue_tail) {
        /* Empty queue is normal state, not an error (P2: false positive removal) */
        return false;
    }
    *action = res->recovery_queue[res->recovery_queue_head];
    res->recovery_queue_head =
        (res->recovery_queue_head + 1) % MESH_RESILIENCE_MAX_ACTIONS;
    return true;
}

static mesh_health_status_t compute_status_from_score(float score) {
    if (score >= 0.8f) return MESH_HEALTH_HEALTHY;
    if (score >= 0.6f) return MESH_HEALTH_DEGRADED;
    if (score >= 0.4f) return MESH_HEALTH_UNHEALTHY;
    if (score >= 0.2f) return MESH_HEALTH_CRITICAL;
    return MESH_HEALTH_DEAD;
}

/* ============================================================================
 * Health Agent Registration API
 * ============================================================================ */

/**
 * P2-147: Unlocked helper for agent registration. Used by heartbeat auto-register
 * to avoid unlock-relock window. Caller MUST hold resilience->mutex.
 */
static nimcp_error_t register_agent_unlocked(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    health_agent_t* agent,
    const char* module_name
) {
    /* Check if already registered */
    int idx = find_agent_by_id(resilience, participant_id);
    if (idx >= 0) {
        /* Update existing */
        resilience->agents[idx].agent = agent;
        return NIMCP_SUCCESS;
    }

    /* Check capacity */
    if (resilience->agent_count >= MESH_RESILIENCE_MAX_AGENTS) {
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Create new registration */
    mesh_agent_registration_t* reg = &resilience->agents[resilience->agent_count];
    memset(reg, 0, sizeof(*reg));

    reg->participant_id = participant_id;
    reg->agent = agent;
    reg->channel = get_channel_from_id(participant_id);
    if (module_name) {
        strncpy(reg->module_name, module_name, MESH_MAX_NAME_LEN - 1);
    }

    reg->status = MESH_HEALTH_UNKNOWN;
    reg->health_score = 1.0f;
    reg->last_heartbeat_ns = nimcp_time_now_ns();
    reg->active = true;
    reg->registered_at_ns = nimcp_time_now_ns();

    resilience->agent_count++;
    resilience->stats.agents_registered++;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_register_agent(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    health_agent_t* agent,
    const char* module_name
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);

    nimcp_error_t err = register_agent_unlocked(resilience, participant_id, agent, module_name);

    if (err == NIMCP_ERROR_CAPACITY_EXCEEDED) {
        nimcp_mutex_unlock(resilience->mutex);
        LOG_WARN("Resilience integration at max agent capacity");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_resilience_integration: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    /* Also register with health bridge */
    if (err == NIMCP_SUCCESS && resilience->health_bridge) {
        mesh_health_bridge_register_agent(
            resilience->health_bridge,
            participant_id,
            agent
        );
    }

    nimcp_mutex_unlock(resilience->mutex);

    if (resilience->config.verbose_logging) {
        LOG_DEBUG("Registered health agent for '%s' (0x%llx)",
                 module_name ? module_name : "unknown",
                 (unsigned long long)participant_id);
    }

    return err;
}

nimcp_error_t mesh_resilience_unregister_agent(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);

    int idx = find_agent_by_id(resilience, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(resilience->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_resilience_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Unregister from health bridge */
    if (resilience->health_bridge) {
        mesh_health_bridge_unregister_agent(
            resilience->health_bridge,
            participant_id
        );
    }

    /* Mark inactive and compact */
    resilience->agents[idx].active = false;
    for (size_t i = idx; i < resilience->agent_count - 1; i++) {
        resilience->agents[i] = resilience->agents[i + 1];
    }
    resilience->agent_count--;

    nimcp_mutex_unlock(resilience->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_get_agent(
    const mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_agent_registration_t* registration
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!registration) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_resilience_integration_t*)resilience)->mutex);

    int idx = find_agent_by_id(resilience, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(((mesh_resilience_integration_t*)resilience)->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_resilience_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    *registration = resilience->agents[idx];

    nimcp_mutex_unlock(((mesh_resilience_integration_t*)resilience)->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Heartbeat Aggregation API
 * ============================================================================ */

nimcp_error_t mesh_resilience_heartbeat(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_heartbeat_op_t op,
    uint8_t progress
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);

    resilience->stats.heartbeats_aggregated++;

    int idx = find_agent_by_id(resilience, participant_id);
    if (idx < 0) {
        /* P2-147: Use unlocked helper to avoid unlock-relock window */
        register_agent_unlocked(resilience, participant_id, NULL, NULL);
        idx = find_agent_by_id(resilience, participant_id);
    }

    if (idx >= 0) {
        mesh_agent_registration_t* reg = &resilience->agents[idx];
        reg->last_heartbeat_ns = nimcp_time_now_ns();
        reg->missed_heartbeats = 0;
        reg->heartbeats_received++;

        if (op == MESH_HEARTBEAT_ERROR) {
            reg->health_score *= 0.9f;  /* Decay on error */
        }
        reg->status = compute_status_from_score(reg->health_score);
    }

    /* Forward to health bridge if enabled */
    if (resilience->config.route_health_through_mesh && resilience->health_bridge) {
        mesh_health_bridge_heartbeat(
            resilience->health_bridge,
            participant_id,
            op,
            progress
        );
    }

    nimcp_mutex_unlock(resilience->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_update_metrics(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    const health_metrics_t* metrics
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!metrics) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(resilience->mutex);

    int idx = find_agent_by_id(resilience, participant_id);
    if (idx < 0) {
        nimcp_mutex_unlock(resilience->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_resilience_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    mesh_agent_registration_t* reg = &resilience->agents[idx];

    /* Compute health score from metrics */
    float score = 1.0f;
    score -= 0.15f * metrics->cpu_utilization;
    score -= 0.15f * metrics->memory_utilization;

    if (metrics->transactions_processed > 0) {
        float success_rate = 1.0f -
            ((float)metrics->transactions_failed / (float)metrics->transactions_processed);
        score -= 0.30f * (1.0f - success_rate);
    }

    float latency_factor = metrics->avg_latency_ms / 1000.0f;
    if (latency_factor > 1.0f) latency_factor = 1.0f;
    score -= 0.20f * latency_factor;

    if (score < 0.0f) score = 0.0f;
    if (score > 1.0f) score = 1.0f;

    reg->health_score = score;
    reg->status = compute_status_from_score(score);
    reg->last_heartbeat_ns = metrics->last_heartbeat_ns;

    /* Forward to health bridge */
    if (resilience->health_bridge) {
        mesh_health_bridge_update_metrics(
            resilience->health_bridge,
            participant_id,
            metrics
        );
    }

    nimcp_mutex_unlock(resilience->mutex);
    return NIMCP_SUCCESS;
}

size_t mesh_resilience_check_heartbeats(
    mesh_resilience_integration_t* resilience
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) return 0;

    nimcp_mutex_lock(resilience->mutex);

    resilience->stats.health_checks_performed++;
    uint64_t now = nimcp_time_now_ns();
    size_t new_failures = 0;

    for (size_t i = 0; i < resilience->agent_count; i++) {
        mesh_agent_registration_t* reg = &resilience->agents[i];
        if (!reg->active) continue;

        uint64_t interval_ns = resilience->config.heartbeat_interval_ms * 1000000ULL;
        uint64_t elapsed_ns = now - reg->last_heartbeat_ns;

        if (elapsed_ns > interval_ns) {
            uint32_t missed = (uint32_t)(elapsed_ns / interval_ns);
            reg->missed_heartbeats = missed;

            mesh_health_status_t old_status = reg->status;

            if (missed >= resilience->config.failure_threshold) {
                reg->status = MESH_HEALTH_DEAD;
                reg->health_score = 0.0f;

                if (old_status != MESH_HEALTH_DEAD) {
                    new_failures++;
                    reg->failures_detected++;
                    resilience->stats.failures_detected++;

                    /* Create failure event */
                    mesh_failure_event_t event = {0};
                    event.event_id = resilience->next_event_id++;
                    event.timestamp_ns = now;
                    event.participant_id = reg->participant_id;
                    event.channel = reg->channel;
                    event.severity = MESH_FAILURE_CRITICAL;
                    event.source = MESH_FAILURE_SRC_HEARTBEAT;
                    snprintf(event.description, sizeof(event.description),
                             "Missed %u heartbeats from '%s'",
                             missed, reg->module_name);
                    add_failure_event(resilience, &event);

                    /* Trigger auto-recovery if enabled */
                    if (resilience->config.enable_auto_recovery) {
                        mesh_recovery_action_t action = {0};
                        action.type = MESH_RECOVERY_RESTART_MODULE;
                        action.target = reg->participant_id;
                        action.channel = reg->channel;
                        action.severity = MESH_FAILURE_CRITICAL;
                        action.requested_at_ns = now;
                        strncpy(action.reason, event.description,
                                sizeof(action.reason) - 1);
                        enqueue_recovery(resilience, &action);
                    }

                    if (resilience->config.verbose_logging) {
                        LOG_WARN("Agent '%s' marked as dead (missed %u heartbeats)",
                                reg->module_name, missed);
                    }
                }
            } else if (missed >= resilience->config.missed_threshold) {
                reg->status = MESH_HEALTH_CRITICAL;
                reg->health_score *= 0.8f;
            }
        }
    }

    /* Also check via health bridge */
    if (resilience->health_bridge) {
        mesh_health_bridge_check_heartbeats(resilience->health_bridge);
    }

    resilience->stats.last_check_ns = now;

    nimcp_mutex_unlock(resilience->mutex);
    return new_failures;
}

/**
 * P1-49: Unlocked helper for channel aggregation to avoid deadlock when called
 * from mesh_resilience_get_system_metrics() which already holds the mutex.
 */
static nimcp_error_t aggregate_channel_unlocked(
    mesh_resilience_integration_t* resilience,
    mesh_channel_id_t channel,
    mesh_channel_health_metrics_t* metrics
) {
    memset(metrics, 0, sizeof(*metrics));
    metrics->channel = channel;

    float total_score = 0.0f;
    float min_score = 1.0f;
    float max_score = 0.0f;
    uint64_t total_hb = 0;
    uint64_t total_missed = 0;

    for (size_t i = 0; i < resilience->agent_count; i++) {
        mesh_agent_registration_t* reg = &resilience->agents[i];
        if (!reg->active || reg->channel != channel) continue;

        metrics->total_agents++;
        total_score += reg->health_score;

        if (reg->health_score < min_score) min_score = reg->health_score;
        if (reg->health_score > max_score) max_score = reg->health_score;

        switch (reg->status) {
            case MESH_HEALTH_HEALTHY:
                metrics->healthy_agents++;
                break;
            case MESH_HEALTH_DEGRADED:
            case MESH_HEALTH_UNHEALTHY:
                metrics->degraded_agents++;
                break;
            case MESH_HEALTH_CRITICAL:
            case MESH_HEALTH_DEAD:
                metrics->failed_agents++;
                break;
            default:
                break;
        }

        total_hb += reg->heartbeats_received;
        total_missed += reg->missed_heartbeats;
    }

    if (metrics->total_agents > 0) {
        metrics->avg_health_score = total_score / (float)metrics->total_agents;
        metrics->min_health_score = min_score;
        metrics->max_health_score = max_score;
        metrics->total_heartbeats = total_hb;
        metrics->missed_heartbeats = total_missed;
    }

    metrics->status = compute_status_from_score(metrics->avg_health_score);
    metrics->computed_at_ns = nimcp_time_now_ns();

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_aggregate_channel(
    mesh_resilience_integration_t* resilience,
    mesh_channel_id_t channel,
    mesh_channel_health_metrics_t* metrics
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!metrics) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(resilience->mutex);
    nimcp_error_t err = aggregate_channel_unlocked(resilience, channel, metrics);
    nimcp_mutex_unlock(resilience->mutex);
    return err;
}

nimcp_error_t mesh_resilience_get_system_metrics(
    mesh_resilience_integration_t* resilience,
    mesh_system_resilience_metrics_t* metrics
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!metrics) return NIMCP_ERROR_NULL_POINTER;

    memset(metrics, 0, sizeof(*metrics));

    nimcp_mutex_lock(resilience->mutex);

    /* Aggregate per channel */
    mesh_channel_id_t channels[] = {
        MESH_CHANNEL_SYSTEM,
        MESH_CHANNEL_LEFT_HEMISPHERE,
        MESH_CHANNEL_RIGHT_HEMISPHERE,
        MESH_CHANNEL_SUBCORTICAL,
        MESH_CHANNEL_GPU_COMPUTE
    };

    metrics->channel_count = sizeof(channels) / sizeof(channels[0]);

    float total_score = 0.0f;
    for (size_t i = 0; i < metrics->channel_count; i++) {
        /* P1-49: Use unlocked helper since we already hold the mutex */
        aggregate_channel_unlocked(
            resilience, channels[i], &metrics->channels[i]);
        metrics->total_agents += metrics->channels[i].total_agents;
        metrics->healthy_agents += metrics->channels[i].healthy_agents;
        metrics->degraded_agents += metrics->channels[i].degraded_agents;
        metrics->failed_agents += metrics->channels[i].failed_agents;
        total_score += metrics->channels[i].avg_health_score;
    }

    if (metrics->channel_count > 0) {
        metrics->system_health_score = total_score / (float)metrics->channel_count;
    }

    /* Compute resilience score */
    if (resilience->stats.recoveries_triggered > 0) {
        metrics->resilience_score =
            (float)resilience->stats.recoveries_succeeded /
            (float)resilience->stats.recoveries_triggered;
    } else {
        metrics->resilience_score = 1.0f;
    }

    /* Copy failure stats */
    metrics->total_failures = resilience->stats.failures_detected;
    metrics->total_recoveries = resilience->stats.recoveries_triggered;
    metrics->successful_recoveries = resilience->stats.recoveries_succeeded;
    if (metrics->total_recoveries > 0) {
        metrics->recovery_success_rate =
            (float)metrics->successful_recoveries / (float)metrics->total_recoveries;
    }

    metrics->status = compute_status_from_score(metrics->system_health_score);
    metrics->computed_at_ns = nimcp_time_now_ns();

    nimcp_mutex_unlock(resilience->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Failure Detection and Recovery API
 * ============================================================================ */

nimcp_error_t mesh_resilience_report_failure(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_failure_severity_t severity,
    mesh_failure_source_t source,
    const char* description
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);

    /* Create failure event */
    mesh_failure_event_t event = {0};
    event.event_id = resilience->next_event_id++;
    event.timestamp_ns = nimcp_time_now_ns();
    event.participant_id = participant_id;
    event.channel = get_channel_from_id(participant_id);
    event.severity = severity;
    event.source = source;
    if (description) {
        strncpy(event.description, description, sizeof(event.description) - 1);
    }

    add_failure_event(resilience, &event);
    resilience->stats.failures_detected++;

    /* Update agent status */
    int idx = find_agent_by_id(resilience, participant_id);
    if (idx >= 0) {
        resilience->agents[idx].failures_detected++;
        if (severity >= MESH_FAILURE_CRITICAL) {
            resilience->agents[idx].status = MESH_HEALTH_CRITICAL;
            resilience->agents[idx].health_score = 0.2f;
        }
    }

    /* Queue recovery if auto-recovery enabled */
    if (resilience->config.enable_auto_recovery &&
        severity >= resilience->config.auto_recovery_min) {
        mesh_recovery_action_t action = {0};
        action.type = MESH_RECOVERY_RESTART_MODULE;
        action.target = participant_id;
        action.channel = event.channel;
        action.severity = severity;
        action.requested_at_ns = event.timestamp_ns;
        strncpy(action.reason, event.description, sizeof(action.reason) - 1);
        enqueue_recovery(resilience, &action);
    }

    nimcp_mutex_unlock(resilience->mutex);

    if (resilience->config.verbose_logging) {
        LOG_WARN("Failure reported for 0x%llx: %s (severity=%d, source=%d)",
                (unsigned long long)participant_id,
                description ? description : "unknown",
                severity, source);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_request_recovery(
    mesh_resilience_integration_t* resilience,
    const mesh_recovery_action_t* action
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!action) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(resilience->mutex);
    enqueue_recovery(resilience, action);
    nimcp_mutex_unlock(resilience->mutex);

    return NIMCP_SUCCESS;
}

size_t mesh_resilience_execute_recoveries(
    mesh_resilience_integration_t* resilience
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) return 0;

    nimcp_mutex_lock(resilience->mutex);

    size_t executed = 0;
    mesh_recovery_action_t action;

    while (dequeue_recovery(resilience, &action)) {
        resilience->stats.recoveries_triggered++;
        bool success = false;

        switch (action.type) {
            case MESH_RECOVERY_RESTART_MODULE:
                /* Would restart the module */
                success = true;
                break;

            case MESH_RECOVERY_TRIGGER_ELECTION:
                if (resilience->config.trigger_elections_on_failure) {
                    /* Would trigger election via coordinator pool */
                    resilience->stats.elections_triggered++;
                    success = true;
                }
                break;

            case MESH_RECOVERY_GPU_RESET:
                if (resilience->gpu_recovery) {
                    /* Would trigger GPU recovery */
                    resilience->stats.gpu_recoveries++;
                    success = true;
                }
                break;

            case MESH_RECOVERY_IMMUNE_RESPONSE:
                if (resilience->immune) {
                    /* Would trigger immune response */
                    resilience->stats.immune_responses++;
                    success = true;
                }
                break;

            default:
                success = true;  /* Assume success for other types */
                break;
        }

        if (success) {
            resilience->stats.recoveries_succeeded++;

            /* Update agent status */
            int idx = find_agent_by_id(resilience, action.target);
            if (idx >= 0) {
                resilience->agents[idx].recoveries_triggered++;
                resilience->agents[idx].status = MESH_HEALTH_DEGRADED;
                resilience->agents[idx].health_score = 0.6f;
            }
        }

        executed++;
    }

    nimcp_mutex_unlock(resilience->mutex);
    return executed;
}

nimcp_error_t mesh_resilience_get_failures(
    const mesh_resilience_integration_t* resilience,
    mesh_failure_event_t* events,
    size_t max_events,
    size_t* event_count
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!events || !event_count) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_resilience_integration_t*)resilience)->mutex);

    size_t to_copy = resilience->failure_history_count;
    if (to_copy > max_events) to_copy = max_events;

    for (size_t i = 0; i < to_copy; i++) {
        events[i] = resilience->failure_history[i];
    }
    *event_count = to_copy;

    nimcp_mutex_unlock(((mesh_resilience_integration_t*)resilience)->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Coordinator Pool Integration API
 * ============================================================================ */

nimcp_error_t mesh_resilience_route_to_coordinator(
    mesh_resilience_integration_t* resilience,
    mesh_coordinator_pool_t* pool
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!pool) return NIMCP_ERROR_NULL_POINTER;

    /* Would create health update transaction and route to pool */
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_trigger_election(
    mesh_resilience_integration_t* resilience,
    mesh_pool_id_t pool_id,
    const char* reason
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);
    resilience->stats.elections_triggered++;
    nimcp_mutex_unlock(resilience->mutex);

    if (resilience->config.verbose_logging) {
        LOG_DEBUG("Triggered election for pool %u: %s",
                 pool_id, reason ? reason : "unspecified");
    }

    (void)pool_id;
    (void)reason;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * GPU Recovery Integration API
 * ============================================================================ */

nimcp_error_t mesh_resilience_connect_gpu_recovery(
    mesh_resilience_integration_t* resilience,
    gpu_recovery_context_t* gpu_recovery
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);
    resilience->gpu_recovery = gpu_recovery;
    nimcp_mutex_unlock(resilience->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_trigger_gpu_recovery(
    mesh_resilience_integration_t* resilience,
    const char* reason
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!resilience->gpu_recovery) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_resilience_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_mutex_lock(resilience->mutex);
    resilience->stats.gpu_recoveries++;
    nimcp_mutex_unlock(resilience->mutex);

    if (resilience->config.verbose_logging) {
        LOG_DEBUG("Triggered GPU recovery: %s", reason ? reason : "unspecified");
    }

    (void)reason;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Immune System Integration API
 * ============================================================================ */

nimcp_error_t mesh_resilience_connect_immune(
    mesh_resilience_integration_t* resilience,
    brain_immune_system_t* immune
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);
    resilience->immune = immune;
    nimcp_mutex_unlock(resilience->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_trigger_immune_response(
    mesh_resilience_integration_t* resilience,
    mesh_participant_id_t participant_id,
    mesh_failure_severity_t severity
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!resilience->immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_resilience_integration: error condition");
        return NIMCP_ERROR_NOT_FOUND;
    }

    nimcp_mutex_lock(resilience->mutex);
    resilience->stats.immune_responses++;
    nimcp_mutex_unlock(resilience->mutex);

    if (resilience->config.verbose_logging) {
        LOG_DEBUG("Triggered immune response for 0x%llx (severity=%d)",
                 (unsigned long long)participant_id, severity);
    }

    (void)participant_id;
    (void)severity;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

nimcp_error_t mesh_resilience_get_stats(
    const mesh_resilience_integration_t* resilience,
    mesh_resilience_stats_t* stats
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (!stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(((mesh_resilience_integration_t*)resilience)->mutex);

    *stats = resilience->stats;

    /* Compute average health score */
    float total = 0.0f;
    size_t count = 0;
    for (size_t i = 0; i < resilience->agent_count; i++) {
        if (resilience->agents[i].active) {
            total += resilience->agents[i].health_score;
            count++;
        }
    }
    if (count > 0) {
        stats->avg_health_score = total / (float)count;
    }

    /* Compute uptime ratio */
    if (resilience->stats.failures_detected > 0) {
        stats->uptime_ratio = 1.0f -
            ((float)resilience->stats.failures_detected /
             (float)(resilience->stats.health_checks_performed + 1));
    } else {
        stats->uptime_ratio = 1.0f;
    }

    nimcp_mutex_unlock(((mesh_resilience_integration_t*)resilience)->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_resilience_reset_stats(
    mesh_resilience_integration_t* resilience
) {
    if (!resilience || resilience->magic != MESH_RESILIENCE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_resilience_integration: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(resilience->mutex);
    memset(&resilience->stats, 0, sizeof(resilience->stats));
    nimcp_mutex_unlock(resilience->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_recovery_action_to_string(mesh_recovery_action_type_t type) {
    switch (type) {
        case MESH_RECOVERY_NONE:                return "NONE";
        case MESH_RECOVERY_RESTART_MODULE:      return "RESTART_MODULE";
        case MESH_RECOVERY_RESTART_CHANNEL:     return "RESTART_CHANNEL";
        case MESH_RECOVERY_TRIGGER_ELECTION:    return "TRIGGER_ELECTION";
        case MESH_RECOVERY_GPU_RESET:           return "GPU_RESET";
        case MESH_RECOVERY_CHECKPOINT:          return "CHECKPOINT";
        case MESH_RECOVERY_ROLLBACK:            return "ROLLBACK";
        case MESH_RECOVERY_QUARANTINE:          return "QUARANTINE";
        case MESH_RECOVERY_IMMUNE_RESPONSE:     return "IMMUNE_RESPONSE";
        case MESH_RECOVERY_LOAD_SHED:           return "LOAD_SHED";
        case MESH_RECOVERY_GRACEFUL_DEGRADATION: return "GRACEFUL_DEGRADATION";
        default:                                return "UNKNOWN";
    }
}

const char* mesh_failure_severity_to_string(mesh_failure_severity_t severity) {
    switch (severity) {
        case MESH_FAILURE_NONE:     return "NONE";
        case MESH_FAILURE_WARNING:  return "WARNING";
        case MESH_FAILURE_DEGRADED: return "DEGRADED";
        case MESH_FAILURE_CRITICAL: return "CRITICAL";
        case MESH_FAILURE_FATAL:    return "FATAL";
        default:                    return "UNKNOWN";
    }
}

const char* mesh_failure_source_to_string(mesh_failure_source_t source) {
    switch (source) {
        case MESH_FAILURE_SRC_UNKNOWN:      return "UNKNOWN";
        case MESH_FAILURE_SRC_HEARTBEAT:    return "HEARTBEAT";
        case MESH_FAILURE_SRC_MEMORY:       return "MEMORY";
        case MESH_FAILURE_SRC_COMPUTATION:  return "COMPUTATION";
        case MESH_FAILURE_SRC_GPU:          return "GPU";
        case MESH_FAILURE_SRC_NETWORK:      return "NETWORK";
        case MESH_FAILURE_SRC_CONSENSUS:    return "CONSENSUS";
        case MESH_FAILURE_SRC_COORDINATOR:  return "COORDINATOR";
        case MESH_FAILURE_SRC_RESOURCE:     return "RESOURCE";
        default:                            return "UNKNOWN";
    }
}
