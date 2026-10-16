/**
 * @file nimcp_bio_router_immune_bridge.c
 * @brief Bio-Async Router - Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between bio-async router and brain immune system
 * WHY:  Biological realism - immune signals use neural pathways, inflammation affects routing
 * HOW:  Monitor cytokine levels to modulate routing, monitor routing to trigger immune responses
 */

#include "async/immune/nimcp_bio_router_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_messages.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bio_router_immune_bridge)

#define LOG_MODULE "BIO_ROUTER_IMMUNE_BRIDGE"

/* BUG-28 fix: Named constant for byzantine behavior severity (was magic number 9) */
#define ROUTER_BYZANTINE_SEVERITY  9  /**< Byzantine behavior antigen severity */


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Clamp value to range
 *
 * WHAT: Constrain value to [min, max]
 * WHY:  Prevent overflow/underflow
 * HOW:  Return min if below, max if above, value otherwise
 */
static inline float clamp_f(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/* Forward declaration for unlocked stats helper */
static int router_immune_update_stats_unlocked(router_immune_bridge_t* bridge);

/**
 * @brief Map inflammation level to priority
 *
 * WHAT: Convert inflammation severity to routing priority
 * WHY:  Higher inflammation = higher message priority
 * HOW:  Map inflammation enum to priority level 0-10
 */
static uint32_t inflammation_to_priority(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return ROUTER_IMMUNE_PRIORITY_NORMAL;
        case INFLAMMATION_LOCAL:    return ROUTER_IMMUNE_PRIORITY_ELEVATED;
        case INFLAMMATION_REGIONAL: return ROUTER_IMMUNE_PRIORITY_HIGH;
        case INFLAMMATION_SYSTEMIC: return ROUTER_IMMUNE_PRIORITY_CRITICAL;
        case INFLAMMATION_STORM:    return ROUTER_IMMUNE_PRIORITY_CRITICAL;
        default:                    return ROUTER_IMMUNE_PRIORITY_NORMAL;
    }
}

/**
 * @brief Map inflammation level to latency multiplier
 *
 * WHAT: Convert inflammation severity to latency impact
 * WHY:  Inflammation causes congestion → slower routing
 * HOW:  Map inflammation enum to multiplier
 */
static float inflammation_to_latency_multiplier(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_LATENCY_NONE;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LATENCY_LOCAL;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_LATENCY_REGIONAL;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_LATENCY_SYSTEMIC;
        case INFLAMMATION_STORM:    return INFLAMMATION_LATENCY_STORM;
        default:                    return INFLAMMATION_LATENCY_NONE;
    }
}

/**
 * @brief Map cytokine type to routing priority
 *
 * WHAT: Determine routing priority based on cytokine type
 * WHY:  Pro-inflammatory cytokines need higher priority than anti-inflammatory
 * HOW:  Map cytokine type to priority level
 */
static uint32_t cytokine_to_priority(brain_cytokine_type_t type) {
    switch (type) {
        case BRAIN_CYTOKINE_TNF:       return ROUTER_IMMUNE_PRIORITY_CRITICAL;  /* Most urgent */
        case BRAIN_CYTOKINE_IL1:       return ROUTER_IMMUNE_PRIORITY_HIGH;
        case BRAIN_CYTOKINE_IL6:       return ROUTER_IMMUNE_PRIORITY_HIGH;
        case BRAIN_CYTOKINE_IFN_GAMMA: return ROUTER_IMMUNE_PRIORITY_ELEVATED;
        case BRAIN_CYTOKINE_IL10:      return ROUTER_IMMUNE_PRIORITY_ELEVATED;  /* Anti-inflammatory */
        default:                       return ROUTER_IMMUNE_PRIORITY_NORMAL;
    }
}

/**
 * @brief Compute routing anomaly severity
 *
 * WHAT: Calculate overall anomaly severity from metrics
 * WHY:  Need single severity score for immune antigen presentation
 * HOW:  Weighted combination of latency, drop rate, error rate anomalies
 */
static uint32_t compute_anomaly_severity(const router_anomaly_event_t* anomaly) {
    if (!anomaly) return 0;

    float severity = 0.0f;

    /* Latency spike contribution */
    if (anomaly->latency_spike) {
        float latency_factor = anomaly->observed_latency_ms / ROUTER_ANOMALY_LATENCY_THRESHOLD;
        severity += clamp_f(latency_factor * 3.0f, 0.0f, 3.0f);
    }

    /* Drop rate contribution */
    if (anomaly->high_drop_rate) {
        float drop_factor = anomaly->observed_drop_rate / ROUTER_ANOMALY_DROP_RATE_THRESHOLD;
        severity += clamp_f(drop_factor * 4.0f, 0.0f, 4.0f);
    }

    /* Error rate contribution */
    if (anomaly->high_error_rate) {
        float error_factor = anomaly->observed_error_rate / ROUTER_ANOMALY_ERROR_RATE_THRESHOLD;
        severity += clamp_f(error_factor * 3.0f, 0.0f, 3.0f);
    }

    /* Byzantine behavior is always high severity */
    if (anomaly->byzantine_behavior) {
        severity += 8.0f;
    }

    return (uint32_t)clamp_f(severity, 1.0f, 10.0f);
}

/**
 * @brief Find quarantined node by ID
 *
 * WHAT: Search quarantine list for specific node
 * WHY:  Need to check quarantine status and update existing entries
 * HOW:  Linear search through quarantine array
 */
static quarantined_node_state_t* find_quarantined_node(
    router_immune_bridge_t* bridge,
    uint32_t node_id
) {
    if (!bridge) {
        return NULL;
    }

    for (size_t i = 0; i < bridge->quarantine_count; i++) {
        if (bridge->quarantined_nodes[i].node_id == node_id) {
            return &bridge->quarantined_nodes[i];
        }
    }
    return NULL;  /* Not found - normal search behavior */
}

/**
 * @brief Find inflammation impact by region
 */
static inflammation_routing_impact_t* find_inflammation_impact(
    router_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge) {
        return NULL;
    }

    for (size_t i = 0; i < bridge->inflammation_count; i++) {
        if (bridge->inflammation_impacts[i].affected_region == region_id) {
            return &bridge->inflammation_impacts[i];
        }
    }
    return NULL;  /* Not found - normal search behavior */
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int router_immune_default_config(router_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_default_config: config is NULL");
        return -1;
    }

    /* All features enabled by default */
    config->enable_cytokine_priority_routing = true;
    config->enable_inflammation_latency_impact = true;
    config->enable_quarantine_routing_exclusion = true;
    config->enable_anomaly_immune_trigger = true;
    config->enable_byzantine_detection = true;

    /* Capacity defaults */
    config->max_cytokine_states = 128;
    config->max_inflammation_sites = 64;
    config->max_quarantined_nodes = 256;
    config->max_anomaly_history = 512;

    /* Biologically-based thresholds */
    config->latency_spike_threshold_ms = ROUTER_ANOMALY_LATENCY_THRESHOLD;
    config->drop_rate_threshold = ROUTER_ANOMALY_DROP_RATE_THRESHOLD;
    config->error_rate_threshold = ROUTER_ANOMALY_ERROR_RATE_THRESHOLD;

    /* Routing behavior */
    config->fully_isolate_quarantined = ROUTER_QUARANTINE_FULL_ISOLATION;
    config->cytokine_ttl_ms = CYTOKINE_MESSAGE_TTL_MS;

    return 0;
}

router_immune_bridge_t* router_immune_bridge_create(
    const router_immune_config_t* config,
    bio_router_t router,
    brain_immune_system_t* immune_system
) {
    /* Guard: require router and immune system */
    if (!router || !immune_system) {
        LOG_MODULE_ERROR("router_immune_bridge",
                  "Cannot create bridge without router and immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_bridge_create: required parameter is NULL (router, immune_system)");
        return NULL;
    }

    /* Allocate bridge - all pointers initialized to NULL via memset */
    router_immune_bridge_t* bridge = (router_immune_bridge_t*)
        nimcp_malloc(sizeof(router_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("router_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "router_immune_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(router_immune_bridge_t));

    /* Link systems */
    bridge->router = router;
    bridge->immune_system = immune_system;

    /* Apply configuration */
    router_immune_config_t default_cfg;
    if (!config) {
        router_immune_default_config(&default_cfg);
        config = &default_cfg;
    }

    bridge->enable_cytokine_priority_routing = config->enable_cytokine_priority_routing;
    bridge->enable_inflammation_latency_impact = config->enable_inflammation_latency_impact;
    bridge->enable_quarantine_routing_exclusion = config->enable_quarantine_routing_exclusion;
    bridge->enable_anomaly_immune_trigger = config->enable_anomaly_immune_trigger;
    bridge->enable_byzantine_detection = config->enable_byzantine_detection;

    /* Allocate cytokine states */
    bridge->cytokine_capacity = config->max_cytokine_states;
    bridge->cytokine_states = (cytokine_routing_state_t*)
        nimcp_malloc(sizeof(cytokine_routing_state_t) * bridge->cytokine_capacity);
    if (!bridge->cytokine_states) goto cleanup;

    /* Allocate inflammation impacts */
    bridge->inflammation_capacity = config->max_inflammation_sites;
    bridge->inflammation_impacts = (inflammation_routing_impact_t*)
        nimcp_malloc(sizeof(inflammation_routing_impact_t) * bridge->inflammation_capacity);
    if (!bridge->inflammation_impacts) goto cleanup;

    /* Allocate quarantined nodes */
    bridge->quarantine_capacity = config->max_quarantined_nodes;
    bridge->quarantined_nodes = (quarantined_node_state_t*)
        nimcp_malloc(sizeof(quarantined_node_state_t) * bridge->quarantine_capacity);
    if (!bridge->quarantined_nodes) goto cleanup;

    /* Allocate anomaly history */
    bridge->anomaly_capacity = config->max_anomaly_history;
    bridge->recent_anomalies = (router_anomaly_event_t*)
        nimcp_malloc(sizeof(router_anomaly_event_t) * bridge->anomaly_capacity);
    if (!bridge->recent_anomalies) goto cleanup;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "bio_router_immune") != 0) goto cleanup;
    if (!bridge->base.mutex) goto cleanup;

    LOG_MODULE_INFO("router_immune_bridge", "Bridge created successfully");
    return bridge;

cleanup:
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge->recent_anomalies);
    nimcp_free(bridge->quarantined_nodes);
    nimcp_free(bridge->inflammation_impacts);
    nimcp_free(bridge->cytokine_states);
    nimcp_free(bridge);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "router_immune_bridge_create: allocation failed");
    return NULL;
}

void router_immune_bridge_destroy(router_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "bio_router_immune");

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    /* Free arrays */
    nimcp_free(bridge->recent_anomalies);
    nimcp_free(bridge->quarantined_nodes);
    nimcp_free(bridge->inflammation_impacts);
    nimcp_free(bridge->cytokine_states);

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("router_immune_bridge", "Bridge destroyed");
}

int router_immune_bridge_start(router_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_bridge_start: bridge is NULL");
        return -1;
    }
    if (!bridge->router || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_bridge_start: required parameter is NULL (bridge->router, bridge->immune_system)");
        return -1;
    }

    /* Register module with router if not already registered */
    if (!bridge->module_ctx) {
        bio_module_info_t module_info;
        module_info.module_id = 0;  /* Will be assigned by router */
        module_info.module_name = "router_immune_bridge";
        module_info.inbox_capacity = 0;  /* Use default */
        module_info.user_data = bridge;

        bridge->module_ctx = bio_router_register_module(&module_info);
        if (!bridge->module_ctx) {
            LOG_MODULE_ERROR("router_immune_bridge",
                  "Failed to register with bio-async router");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_bridge_start: bridge->module_ctx is NULL");
            return -1;
        }
    }

    LOG_MODULE_INFO("router_immune_bridge", "Bridge started");
    return 0;
}

int router_immune_bridge_stop(router_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_bridge_stop: bridge is NULL");
        return -1;
    }

    /* Unregister from router */
    if (bridge->module_ctx) {
        bio_router_unregister_module(bridge->module_ctx);
        bridge->module_ctx = NULL;
    }

    LOG_MODULE_INFO("router_immune_bridge", "Bridge stopped");
    return 0;
}

/* ============================================================================
 * Immune → Router Implementation
 * ============================================================================ */

int router_immune_prioritize_cytokine(
    router_immune_bridge_t* bridge,
    brain_cytokine_type_t cytokine_type,
    float concentration,
    uint32_t source_cell
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_prioritize_cytokine: bridge is NULL");
        return -1;
    }
    if (!bridge->enable_cytokine_priority_routing) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* BUG-3 fix: Capacity check moved INSIDE the mutex lock to prevent TOCTOU race.
     * Previously the check was done before acquiring the lock, so another thread
     * could fill the last slot between the check and the lock acquisition. */
    if (bridge->cytokine_count >= bridge->cytokine_capacity) {
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "router_immune_prioritize_cytokine: capacity exceeded");
        return -1;
    }

    /* Create cytokine routing state */
    cytokine_routing_state_t* state = &bridge->cytokine_states[bridge->cytokine_count];
    state->type = cytokine_type;
    state->priority_level = cytokine_to_priority(cytokine_type);
    state->concentration = clamp_f(concentration, 0.0f, 1.0f);
    state->release_time = nimcp_platform_time_monotonic_us() / 1000;  /* HIGH-1 fix: store in ms to match expiration logic */
    state->expired = false;

    bridge->cytokine_count++;
    bridge->cytokine_messages_routed++;

    LOG_MODULE_DEBUG("router_immune_bridge",
                  "Prioritized cytokine type %d with priority %u",
              cytokine_type, state->priority_level);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int router_immune_apply_inflammation_latency(
    router_immune_bridge_t* bridge,
    uint32_t region_id,
    brain_inflammation_level_t inflammation_level
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_apply_inflammation_latency: bridge is NULL");
        return -1;
    }
    if (!bridge->enable_inflammation_latency_impact) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Find or create inflammation impact */
    inflammation_routing_impact_t* impact = find_inflammation_impact(bridge, region_id);
    if (!impact) {
        /* Add new inflammation impact */
        if (bridge->inflammation_count >= bridge->inflammation_capacity) {
            nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "router_immune_apply_inflammation_latency: capacity exceeded");
            return -1;
        }
        impact = &bridge->inflammation_impacts[bridge->inflammation_count++];
        impact->affected_region = region_id;
        impact->start_time = nimcp_platform_time_monotonic_us() / 1000;  /* HIGH-1 fix: store in ms */
    }

    /* Update inflammation state */
    impact->level = inflammation_level;
    impact->latency_multiplier = inflammation_to_latency_multiplier(inflammation_level);
    impact->routing_cost_penalty = (float)(inflammation_level) * 100.0f;
    impact->active = (inflammation_level != INFLAMMATION_NONE);

    LOG_MODULE_DEBUG("router_immune_bridge",
                  "Applied inflammation latency for region %u: multiplier %.2f",
              region_id, impact->latency_multiplier);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int router_immune_quarantine_node(
    router_immune_bridge_t* bridge,
    uint32_t node_id,
    uint64_t duration_ms,
    float trust_score,
    uint32_t antigen_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_quarantine_node: bridge is NULL");
        return -1;
    }
    if (!bridge->enable_quarantine_routing_exclusion) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Check if already quarantined */
    quarantined_node_state_t* existing = find_quarantined_node(bridge, node_id);
    if (existing) {
        /* Update existing quarantine */
        existing->quarantine_duration_ms = duration_ms;
        existing->trust_score = trust_score;
        existing->triggering_antigen_id = antigen_id;
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        return 0;
    }

    /* Add new quarantine */
    if (bridge->quarantine_count >= bridge->quarantine_capacity) {
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        LOG_MODULE_WARN("router_immune_bridge", "Quarantine capacity exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "router_immune_quarantine_node: capacity exceeded");
        return -1;
    }

    quarantined_node_state_t* qnode = &bridge->quarantined_nodes[bridge->quarantine_count++];
    qnode->node_id = node_id;
    qnode->quarantine_start = nimcp_platform_time_monotonic_us() / 1000;  /* HIGH-1 fix: store in ms to match duration_ms */
    qnode->quarantine_duration_ms = duration_ms;
    qnode->fully_isolated = ROUTER_QUARANTINE_FULL_ISOLATION;
    qnode->trust_score = trust_score;
    qnode->triggering_antigen_id = antigen_id;

    bridge->nodes_quarantined++;

    /* BUG-27 fix: duration_ms is uint64_t; %lu is 32-bit on some platforms.
     * Cast to unsigned long long to match %llu for portable formatting. */
    LOG_MODULE_INFO("router_immune_bridge",
                  "Quarantined node %u for %llu ms (trust: %.2f)",
              node_id, (unsigned long long)duration_ms, trust_score);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int router_immune_restore_node(
    router_immune_bridge_t* bridge,
    uint32_t node_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_restore_node: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Find and remove quarantined node */
    for (size_t i = 0; i < bridge->quarantine_count; i++) {
        if (bridge->quarantined_nodes[i].node_id == node_id) {
            /* Remove by swapping with last element */
            if (i < bridge->quarantine_count - 1) {
                bridge->quarantined_nodes[i] =
                    bridge->quarantined_nodes[bridge->quarantine_count - 1];
            }
            bridge->quarantine_count--;

            LOG_MODULE_INFO("router_immune_bridge",
                  "Restored node %u to routing", node_id);

            nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "router_immune_restore_node: operation failed");
    return -1;  /* Node not found in quarantine */
}

int router_immune_broadcast_alert(
    router_immune_bridge_t* bridge,
    uint32_t antigen_id,
    brain_inflammation_level_t severity
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_broadcast_alert: bridge is NULL");
        return -1;
    }
    if (!bridge->module_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_broadcast_alert: bridge->module_ctx is NULL");
        return -1;
    }

    /* Create immune alert message */
    /* Note: Actual message structure would be defined in bio_messages.h */
    struct {
        bio_message_header_t header;
        uint32_t antigen_id;
        uint32_t severity;
    } alert_msg;

    /* BUG-11 fix: Zero the entire struct first to avoid sending uninitialized
     * header fields (source_module, sequence, flags, etc.) over the bus. */
    memset(&alert_msg, 0, sizeof(alert_msg));

    /* Initialize message header fields */
    alert_msg.header.type = BIO_MSG_SECURITY_ALERT;
    alert_msg.antigen_id = antigen_id;
    alert_msg.severity = (uint32_t)severity;

    /* Broadcast via router on NOREPINEPHRINE channel (high priority) */
    nimcp_error_t err = bio_router_broadcast(
        bridge->module_ctx,
        &alert_msg,
        sizeof(alert_msg)
    );

    if (err != NIMCP_SUCCESS) {
        LOG_MODULE_ERROR("router_immune_bridge",
                  "Failed to broadcast immune alert");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "router_immune_broadcast_alert: validation failed");
        return -1;
    }

    LOG_MODULE_INFO("router_immune_bridge",
                  "Broadcast immune alert for antigen %u (severity %u)",
              antigen_id, severity);

    return 0;
}

/* ============================================================================
 * Router → Immune Implementation
 * ============================================================================ */

int router_immune_detect_anomalies(
    router_immune_bridge_t* bridge,
    uint32_t node_id
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_detect_anomalies: bridge is NULL");
        return -1;
    }
    if (!bridge->enable_anomaly_immune_trigger) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Update statistics under lock to prevent TOCTOU race (use _unlocked variant) */
    router_immune_update_stats_unlocked(bridge);

    /* Check for anomalies */
    bool anomaly_detected = false;
    router_anomaly_event_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));

    anomaly.node_id = node_id;
    anomaly.timestamp = nimcp_platform_time_monotonic_us() / 1000;  /* HIGH-1 fix: store in ms to match time_window_ms */
    anomaly.observed_latency_ms = bridge->stats.avg_latency_ms;
    anomaly.observed_drop_rate = bridge->stats.current_drop_rate;
    anomaly.observed_error_rate = bridge->stats.current_error_rate;

    /* Latency spike detection */
    if (bridge->stats.avg_latency_ms > ROUTER_ANOMALY_LATENCY_THRESHOLD) {
        anomaly.latency_spike = true;
        anomaly_detected = true;
    }

    /* Drop rate detection */
    if (bridge->stats.current_drop_rate > ROUTER_ANOMALY_DROP_RATE_THRESHOLD) {
        anomaly.high_drop_rate = true;
        anomaly_detected = true;
    }

    /* Error rate detection */
    if (bridge->stats.current_error_rate > ROUTER_ANOMALY_ERROR_RATE_THRESHOLD) {
        anomaly.high_error_rate = true;
        anomaly_detected = true;
    }

    if (anomaly_detected) {
        /* Compute severity and confidence */
        anomaly.severity = compute_anomaly_severity(&anomaly);
        anomaly.confidence = 0.8f;  /* Default confidence */

        /* Store in anomaly history */
        if (bridge->anomaly_count < bridge->anomaly_capacity) {
            bridge->recent_anomalies[bridge->anomaly_count++] = anomaly;
        }

        bridge->anomalies_detected++;

        LOG_MODULE_WARN("router_immune_bridge",
                  "Detected routing anomaly on node %u (severity %u)",
                  node_id, anomaly.severity);

        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

        /* Trigger immune response */
        return router_immune_trigger_from_anomaly(bridge, &anomaly);
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int router_immune_trigger_from_anomaly(
    router_immune_bridge_t* bridge,
    const router_anomaly_event_t* anomaly
) {
    /* Guard clauses */
    if (!bridge || !anomaly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_trigger_from_anomaly: required parameter is NULL (bridge, anomaly)");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_trigger_from_anomaly: bridge->immune_system is NULL");
        return -1;
    }

    /* Create antigen signature from anomaly */
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, sizeof(epitope));

    /* Encode anomaly characteristics into epitope */
    epitope[0] = (uint8_t)anomaly->node_id;
    epitope[1] = (uint8_t)(anomaly->observed_latency_ms / 10.0f);
    epitope[2] = (uint8_t)(anomaly->observed_drop_rate * 255.0f);
    epitope[3] = (uint8_t)(anomaly->observed_error_rate * 255.0f);
    epitope[4] = anomaly->latency_spike ? 1 : 0;
    epitope[5] = anomaly->high_drop_rate ? 1 : 0;
    epitope[6] = anomaly->high_error_rate ? 1 : 0;
    epitope[7] = anomaly->byzantine_behavior ? 1 : 0;

    /* Present as antigen to immune system */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        sizeof(epitope),
        anomaly->severity,
        anomaly->node_id,
        &antigen_id
    );

    if (result == 0) {
        /* BUG-17 fix: Protect immune_triggers increment with mutex */
        nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
        bridge->immune_triggers++;
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        LOG_MODULE_INFO("router_immune_bridge",
                  "Triggered immune response from anomaly (antigen %u)", antigen_id);
    }

    return result;
}

int router_immune_detect_byzantine(
    router_immune_bridge_t* bridge,
    uint32_t node_id,
    const void* msg,
    size_t msg_size
) {
    /* Guard clauses */
    if (!bridge || !msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_detect_byzantine: required parameter is NULL (bridge, msg)");
        return -1;
    }
    if (!bridge->enable_byzantine_detection) return 0;

    /* Byzantine detection logic would go here */
    /* For now, this is a placeholder */

    LOG_MODULE_DEBUG("router_immune_bridge",
                  "Byzantine detection check for node %u", node_id);

    return 0;
}

int router_immune_present_byzantine(
    router_immune_bridge_t* bridge,
    uint32_t node_id,
    const uint8_t* behavior_signature,
    size_t sig_len
) {
    /* Guard clauses */
    if (!bridge || !behavior_signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_present_byzantine: required parameter is NULL (bridge, behavior_signature)");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_present_byzantine: bridge->immune_system is NULL");
        return -1;
    }

    /* Present Byzantine behavior as antigen */
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_BFT,  /* Byzantine is BFT source */
        behavior_signature,
        sig_len,
        ROUTER_BYZANTINE_SEVERITY,
        node_id,
        &antigen_id
    );

    if (result == 0) {
        /* BUG-17 fix: Protect immune_triggers increment with mutex */
        nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
        bridge->immune_triggers++;
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        LOG_MODULE_WARN("router_immune_bridge",
                  "Presented Byzantine behavior as antigen %u", antigen_id);
    }

    return result;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int router_immune_bridge_update(
    router_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_bridge_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* BUG-22 fix: Use real monotonic time instead of total_updates * delta_ms.
     * Cytokine release_time and quarantine_start are stored as real monotonic ms
     * (nimcp_platform_time_monotonic_us() / 1000), so the expiration check must
     * use the same time source. The old approximation caused cytokines and
     * quarantines to never expire correctly when update calls were irregular. */
    uint64_t current_time = nimcp_platform_time_monotonic_us() / 1000;

    /* Update routing statistics under the lock (use _unlocked variant) */
    router_immune_update_stats_unlocked(bridge);

    /* Expire old cytokine routing states */
    router_immune_expire_cytokines(bridge, current_time);

    /* Release expired quarantines */
    router_immune_release_expired_quarantines(bridge, current_time);

    bridge->total_updates++;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    /* BUG-2 fix: Call detect_anomalies AFTER releasing the mutex.
     * detect_anomalies() acquires bridge->base.mutex internally, so calling it
     * while already holding the mutex causes a deadlock (non-recursive mutex). */
    if (bridge->enable_anomaly_immune_trigger) {
        router_immune_detect_anomalies(bridge, 0);  /* Node 0 = overall */
    }

    return 0;
}

/**
 * @brief Internal stats update — caller MUST hold bridge->base.mutex.
 *
 * BUG-9 fix: Split into _unlocked helper so that callers already holding
 * the mutex (detect_anomalies, bridge_update) can call without deadlock,
 * while the public API acquires the lock.
 */
static int router_immune_update_stats_unlocked(router_immune_bridge_t* bridge) {
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_update_stats: bridge->router is NULL");
        return -1;
    }

    /* Query router statistics */
    bio_router_stats_t router_stats;
    nimcp_error_t err = bio_router_get_stats(&router_stats);
    if (err != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "router_immune_update_stats: validation failed");
        return -1;
    }

    /* Update internal statistics */
    bridge->stats.messages_sent = router_stats.messages_routed;
    bridge->stats.messages_delivered = router_stats.messages_routed - router_stats.messages_dropped;
    bridge->stats.messages_dropped = router_stats.messages_dropped;
    bridge->stats.routing_errors = router_stats.handler_errors;
    bridge->stats.avg_latency_ms = router_stats.avg_routing_latency_us / 1000.0f;
    bridge->stats.max_latency_ms = router_stats.max_routing_latency_us / 1000.0f;

    /* Compute rates */
    if (bridge->stats.messages_sent > 0) {
        bridge->stats.current_drop_rate =
            (float)bridge->stats.messages_dropped / (float)bridge->stats.messages_sent;
        bridge->stats.current_error_rate =
            (float)bridge->stats.routing_errors / (float)bridge->stats.messages_sent;
    }

    /* BUG-12 fix: Use actual monotonic time instead of hardcoded 0 */
    /* HIGH-1 fix: Convert to ms for consistent time units throughout bridge */
    bridge->stats.last_update_time = nimcp_platform_time_monotonic_us() / 1000;

    return 0;
}

int router_immune_update_stats(router_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_update_stats: bridge is NULL");
        return -1;
    }

    /* BUG-9 fix: Acquire mutex for thread safety when called as public API */
    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    int result = router_immune_update_stats_unlocked(bridge);
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return result;
}

int router_immune_expire_cytokines(
    router_immune_bridge_t* bridge,
    uint64_t current_time
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_expire_cytokines: bridge is NULL");
        return -1;
    }

    /* Mark expired cytokines and compact array */
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < bridge->cytokine_count; read_idx++) {
        cytokine_routing_state_t* state = &bridge->cytokine_states[read_idx];

        /* Check if expired */
        uint64_t age_ms = current_time - state->release_time;
        if (age_ms < CYTOKINE_MESSAGE_TTL_MS) {
            /* Keep this entry */
            if (write_idx != read_idx) {
                bridge->cytokine_states[write_idx] = *state;
            }
            write_idx++;
        }
    }

    size_t expired_count = bridge->cytokine_count - write_idx;
    bridge->cytokine_count = write_idx;

    if (expired_count > 0) {
        LOG_MODULE_DEBUG("router_immune_bridge",
                  "Expired %zu cytokine routing states", expired_count);
    }

    return 0;
}

int router_immune_release_expired_quarantines(
    router_immune_bridge_t* bridge,
    uint64_t current_time
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_release_expired_quarantines: bridge is NULL");
        return -1;
    }

    /* Release expired quarantines and compact array */
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < bridge->quarantine_count; read_idx++) {
        quarantined_node_state_t* qnode = &bridge->quarantined_nodes[read_idx];

        /* Check if expired */
        uint64_t age_ms = current_time - qnode->quarantine_start;
        if (age_ms < qnode->quarantine_duration_ms) {
            /* Keep this quarantine */
            if (write_idx != read_idx) {
                bridge->quarantined_nodes[write_idx] = *qnode;
            }
            write_idx++;
        } else {
            LOG_MODULE_INFO("router_immune_bridge",
                  "Released quarantine for node %u", qnode->node_id);
        }
    }

    bridge->quarantine_count = write_idx;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int router_immune_get_stats(
    const router_immune_bridge_t* bridge,
    router_immune_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router_immune_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool router_immune_is_node_quarantined(
    const router_immune_bridge_t* bridge,
    uint32_t node_id
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    bool quarantined = (find_quarantined_node((router_immune_bridge_t*)bridge, node_id) != NULL);
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return quarantined;
}

float router_immune_get_latency_multiplier(
    const router_immune_bridge_t* bridge,
    uint32_t region_id
) {
    if (!bridge) return 1.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_routing_impact_t* impact =
        find_inflammation_impact((router_immune_bridge_t*)bridge, region_id);

    float multiplier = impact ? impact->latency_multiplier : 1.0f;

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return multiplier;
}

uint32_t router_immune_get_cytokine_priority(
    const router_immune_bridge_t* bridge,
    brain_cytokine_type_t cytokine_type
) {
    return cytokine_to_priority(cytokine_type);
}

uint32_t router_immune_get_anomaly_count(
    const router_immune_bridge_t* bridge,
    uint64_t time_window_ms
) {
    if (!bridge) return 0;

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    if (time_window_ms == 0) {
        /* Return all anomalies */
        uint32_t count = bridge->anomaly_count;
        nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
        return count;
    }

    /* Count anomalies within time window */
    /* HIGH-1 fix: Convert to ms — timestamps are stored in ms, time_window_ms is in ms */
    uint64_t current_time = nimcp_platform_time_monotonic_us() / 1000;
    uint32_t count = 0;
    for (size_t i = 0; i < bridge->anomaly_count; i++) {
        uint64_t age = current_time - bridge->recent_anomalies[i].timestamp;
        if (age <= time_window_ms) {
            count++;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return count;
}
