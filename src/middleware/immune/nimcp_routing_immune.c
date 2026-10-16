#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_routing_immune.c - Brain Immune Integration with Routing
//=============================================================================

#include "middleware/immune/nimcp_routing_immune.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(routing_immune)

/* No longer need compatibility macros - using platform API directly */

//=============================================================================
// Internal Structures
//=============================================================================

struct routing_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Component handles */
    brain_immune_system_t* immune_system;
    thalamic_router_t* thalamic_router;
    attention_gate_t* attention_gate;
    event_bus_t event_bus;

    /* Configuration */
    routing_immune_config_t config;

    /* Anomaly tracking */
    routing_anomaly_t* anomalies;
    uint32_t anomaly_count;
    uint32_t next_anomaly_id;

    /* Current state */
    float inflammation_boost;           /**< Current priority boost from inflammation */
    float cytokine_attention_mod;       /**< Current attention modifier from cytokines */
    routing_immune_strategy_t strategy; /**< Current routing strategy */

    /* Statistics */
    routing_immune_stats_t stats;

    /* Timing */
    uint64_t last_update_ms;

};

//=============================================================================
// Default Configuration
//=============================================================================

routing_immune_config_t routing_immune_default_config(void) {
    routing_immune_config_t config = {
        .drop_rate_threshold = ROUTING_IMMUNE_DROP_THRESHOLD,
        .latency_threshold_ms = ROUTING_IMMUNE_LATENCY_THRESHOLD,
        .queue_overflow_threshold = 900,  // 90% of typical queue size
        .attention_collapse_threshold = 0.1f,

        .local_inflammation_boost = 0.1f,      // 10% boost
        .regional_inflammation_boost = 0.3f,   // 30% boost
        .systemic_inflammation_boost = 0.6f,   // 60% boost
        .storm_inflammation_boost = 1.0f,      // 100% boost (emergency)

        .pro_cytokine_attention_boost = 0.25f, // 25% attention increase
        .anti_cytokine_attention_calm = 0.2f,  // 20% attention decrease

        .enable_immune_events = true,
        .enable_anomaly_detection = true,
        .update_interval_ms = 100  // Check every 100ms
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

routing_immune_bridge_t* routing_immune_create(
    brain_immune_system_t* immune_system,
    thalamic_router_t* thalamic_router,
    attention_gate_t* attention_gate,
    event_bus_t event_bus,
    const routing_immune_config_t* config
) {
    if (!immune_system || !thalamic_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_create: required parameter is NULL (immune_system, thalamic_router)");
        return NULL;
    }

    routing_immune_bridge_t* bridge = nimcp_malloc(sizeof(routing_immune_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(routing_immune_bridge_t));

    bridge->immune_system = immune_system;
    bridge->thalamic_router = thalamic_router;
    bridge->attention_gate = attention_gate;
    bridge->event_bus = event_bus;

    bridge->config = config ? *config : routing_immune_default_config();

    bridge->anomalies = nimcp_malloc(sizeof(routing_anomaly_t) * ROUTING_IMMUNE_MAX_ANOMALIES);
    if (!bridge->anomalies) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "routing_immune_create: bridge->anomalies is NULL");
        return NULL;
    }

    bridge->anomaly_count = 0;
    bridge->next_anomaly_id = 1;

    bridge->inflammation_boost = 1.0f;
    bridge->cytokine_attention_mod = 1.0f;
    bridge->strategy = ROUTING_STRATEGY_NORMAL;

    bridge->base.mutex = nimcp_platform_mutex_create();
    if (!bridge->base.mutex) {
        nimcp_free(bridge->anomalies);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "routing_immune_create: bridge->base is NULL");
        return NULL;
    }

    return bridge;
}

void routing_immune_destroy(routing_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge->anomalies);
    nimcp_free(bridge);
}

//=============================================================================
// Immune → Routing: Inflammation Effects
//=============================================================================

bool routing_immune_apply_inflammation_effect(
    routing_immune_bridge_t* bridge,
    brain_inflammation_level_t inflammation_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_apply_inflammation_effect: bridge is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float boost = 1.0f;
    switch (inflammation_level) {
        case INFLAMMATION_NONE:
            boost = 1.0f;
            break;
        case INFLAMMATION_LOCAL:
            boost = 1.0f + bridge->config.local_inflammation_boost;
            break;
        case INFLAMMATION_REGIONAL:
            boost = 1.0f + bridge->config.regional_inflammation_boost;
            break;
        case INFLAMMATION_SYSTEMIC:
            boost = 1.0f + bridge->config.systemic_inflammation_boost;
            break;
        case INFLAMMATION_STORM:
            boost = 1.0f + bridge->config.storm_inflammation_boost;
            break;
    }

    bridge->inflammation_boost = boost;
    bridge->stats.immune_modulations_applied++;
    bridge->stats.current_inflammation_boost = boost;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Routing immune: Applied inflammation boost %.2f for level %d",
                    boost, inflammation_level);
    return true;
}

bool routing_immune_apply_cytokine_effect(
    routing_immune_bridge_t* bridge,
    brain_cytokine_type_t cytokine_type,
    float concentration
) {
    if (!bridge || !bridge->attention_gate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_apply_cytokine_effect: required parameter is NULL (bridge, bridge->attention_gate)");
        return false;
    }

    if (concentration < 0.0f || concentration > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_immune_apply_cytokine_effect: validation failed");
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float modifier = 1.0f;
    bool is_pro_inflammatory = false;

    switch (cytokine_type) {
        case CYTOKINE_IL1B:
        case CYTOKINE_IL6:
        case CYTOKINE_TNFA:
        case BRAIN_CYTOKINE_IFN_GAMMA:
            // Pro-inflammatory: boost attention
            is_pro_inflammatory = true;
            modifier = 1.0f + (bridge->config.pro_cytokine_attention_boost * concentration);
            break;

        case CYTOKINE_IL10:
            // Anti-inflammatory: calm attention
            is_pro_inflammatory = false;
            modifier = 1.0f - (bridge->config.anti_cytokine_attention_calm * concentration);
            break;

        default:
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_immune_apply_cytokine_effect: operation failed");
            return false;
    }

    bridge->cytokine_attention_mod = modifier;
    bridge->stats.cytokine_effects_applied++;
    bridge->stats.current_cytokine_attention_mod = modifier;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Routing immune: Applied cytokine modifier %.2f (type %d, conc %.2f)",
                    modifier, cytokine_type, concentration);
    return true;
}

bool routing_immune_set_strategy_from_phase(
    routing_immune_bridge_t* bridge,
    brain_immune_phase_t immune_phase
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_set_strategy_from_phase: bridge is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    routing_immune_strategy_t strategy;
    switch (immune_phase) {
        case IMMUNE_PHASE_SURVEILLANCE:
        case IMMUNE_PHASE_MEMORY:
            strategy = ROUTING_STRATEGY_NORMAL;
            break;

        case IMMUNE_PHASE_RECOGNITION:
            strategy = ROUTING_STRATEGY_ALERT;
            break;

        case IMMUNE_PHASE_ACTIVATION:
            strategy = ROUTING_STRATEGY_DEFENSIVE;
            break;

        case IMMUNE_PHASE_EFFECTOR:
        case IMMUNE_PHASE_RESOLUTION:
            strategy = ROUTING_STRATEGY_EMERGENCY;
            break;

        default:
            strategy = ROUTING_STRATEGY_NORMAL;
            break;
    }

    bridge->strategy = strategy;
    bridge->stats.current_strategy = strategy;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Routing immune: Set strategy to %s for phase %d",
                    routing_immune_strategy_name(strategy), immune_phase);
    return true;
}

//=============================================================================
// Routing → Immune: Anomaly Detection
//=============================================================================

bool routing_immune_detect_anomaly(
    routing_immune_bridge_t* bridge,
    const routing_stats_t* stats,
    bool* anomaly_detected,
    routing_anomaly_type_t* anomaly_type
) {
    if (!bridge || !stats || !anomaly_detected || !anomaly_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_detect_anomaly: required parameter is NULL (bridge, stats, anomaly_detected, anomaly_type)");
        return false;
    }

    *anomaly_detected = false;
    *anomaly_type = ROUTING_ANOMALY_NONE;

    if (!bridge->config.enable_anomaly_detection) {
        return true;
    }

    // Check drop rate
    float drop_rate = (stats->signals_routed > 0) ?
        (float)stats->signals_dropped / (float)stats->signals_routed : 0.0f;

    if (drop_rate > bridge->config.drop_rate_threshold) {
        *anomaly_detected = true;
        *anomaly_type = ROUTING_ANOMALY_HIGH_DROP_RATE;
        return true;
    }

    // Check latency
    if (stats->avg_latency_ms > bridge->config.latency_threshold_ms) {
        *anomaly_detected = true;
        *anomaly_type = ROUTING_ANOMALY_HIGH_LATENCY;
        return true;
    }

    // Check queue overflow
    if (stats->queue_depth > bridge->config.queue_overflow_threshold) {
        *anomaly_detected = true;
        *anomaly_type = ROUTING_ANOMALY_QUEUE_OVERFLOW;
        return true;
    }

    return true;
}

bool routing_immune_present_anomaly(
    routing_immune_bridge_t* bridge,
    const routing_anomaly_t* anomaly,
    uint32_t* antigen_id
) {
    if (!bridge || !anomaly || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_present_anomaly: required parameter is NULL (bridge, anomaly, antigen_id)");
        return false;
    }

    // Create epitope from anomaly signature
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0, BRAIN_IMMUNE_EPITOPE_SIZE);

    // Encode anomaly type and metrics into epitope
    epitope[0] = (uint8_t)anomaly->type;
    memcpy(&epitope[1], &anomaly->affected_source_id, sizeof(uint32_t));
    memcpy(&epitope[5], &anomaly->affected_dest_id, sizeof(uint32_t));

    uint32_t severity_int = (uint32_t)(anomaly->severity * 10.0f);
    memcpy(&epitope[9], &severity_int, sizeof(uint32_t));

    size_t epitope_len = 13;

    // Present to immune system as anomaly source
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_ANOMALY,
        epitope,
        epitope_len,
        (uint32_t)(anomaly->severity * 10.0f),
        anomaly->affected_source_id,
        antigen_id
    );

    if (result == 0) {
        bridge->stats.anomalies_presented++;
        NIMCP_LOGGING_INFO("Routing immune: Presented anomaly type %d as antigen %u",
                       anomaly->type, *antigen_id);
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_immune_present_anomaly: result is zero");
    return false;
}

bool routing_immune_record_anomaly(
    routing_immune_bridge_t* bridge,
    const routing_anomaly_t* anomaly
) {
    if (!bridge || !anomaly) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_record_anomaly: required parameter is NULL (bridge, anomaly)");
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    // If full, replace oldest
    uint32_t index;
    if (bridge->anomaly_count < ROUTING_IMMUNE_MAX_ANOMALIES) {
        index = bridge->anomaly_count++;
    } else {
        // Find oldest anomaly
        index = 0;
        uint64_t oldest_time = bridge->anomalies[0].detection_time_ms;
        for (uint32_t i = 1; i < ROUTING_IMMUNE_MAX_ANOMALIES; i++) {
            if (bridge->anomalies[i].detection_time_ms < oldest_time) {
                oldest_time = bridge->anomalies[i].detection_time_ms;
                index = i;
            }
        }
    }

    bridge->anomalies[index] = *anomaly;
    bridge->anomalies[index].id = bridge->next_anomaly_id++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return true;
}

//=============================================================================
// Event Integration
//=============================================================================

bool routing_immune_publish_event(
    routing_immune_bridge_t* bridge,
    event_type_t event_type,
    const void* data
) {
    if (!bridge || !bridge->event_bus || !bridge->config.enable_immune_events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_publish_event: required parameter is NULL (bridge, bridge->event_bus, bridge->config)");
        return false;
    }

    // Create event based on type
    event_t event;
    memset(&event, 0, sizeof(event_t));

    event.type = event_type;
    event.source = EVENT_SOURCE_ROUTER;
    event.priority = MW_EVENT_PRIORITY_HIGH;
    event.timestamp_us = nimcp_time_get_us();

    // Publish to event bus
    bool result = event_bus_publish(bridge->event_bus, &event);

    if (result) {
        NIMCP_LOGGING_DEBUG("Routing immune: Published event type %d", event_type);
    }

    return result;
}

bool routing_immune_subscribe_routing_events(
    routing_immune_bridge_t* bridge
) {
    if (!bridge || !bridge->event_bus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_subscribe_routing_events: required parameter is NULL (bridge, bridge->event_bus)");
        return false;
    }

    // Subscribe to attention shift and error events
    // NOTE: Using default config - actual filtering done via predicate if needed
    subscription_config_t sub_config = {
        .event_types = NULL,     /* Subscribe to all types */
        .num_types = 0,
        .event_sources = NULL,   /* Subscribe to all sources */
        .num_sources = 0,
        .predicate = NULL,
        .predicate_context = NULL,
        .priority = SUBSCRIBER_PRIORITY_NORMAL
    };
    (void)sub_config; /* Suppress unused warning - placeholder for future use */

    // This would require a callback function - simplified for now
    // In a full implementation, we'd register callbacks here

    NIMCP_LOGGING_DEBUG("Routing immune: Subscribed to routing events");
    return true;
}

//=============================================================================
// Update and Query
//=============================================================================

bool routing_immune_update(
    routing_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_update: bridge is NULL");
        return false;
    }

    bridge->last_update_ms += delta_ms;

    if (bridge->last_update_ms < bridge->config.update_interval_ms) {
        return true;  // Not time to update yet
    }

    bridge->last_update_ms = 0;

    // Get current routing stats
    routing_stats_t routing_stats;
    if (!thalamic_router_get_stats(bridge->thalamic_router, &routing_stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "routing_immune_update: thalamic_router_get_stats is NULL");
        return false;
    }

    // Detect anomalies
    bool anomaly_detected;
    routing_anomaly_type_t anomaly_type;

    if (routing_immune_detect_anomaly(bridge, &routing_stats,
                                      &anomaly_detected, &anomaly_type)) {
        if (anomaly_detected) {
            bridge->stats.anomalies_detected++;

            // Create anomaly record
            routing_anomaly_t anomaly = {
                .type = anomaly_type,
                .detection_time_ms = nimcp_time_get_ms(),
                .severity = 0.5f,  // Medium severity by default
                .drop_rate = (routing_stats.signals_routed > 0) ?
                    (float)routing_stats.signals_dropped / (float)routing_stats.signals_routed : 0.0f,
                .avg_latency_ms = routing_stats.avg_latency_ms,
                .queue_depth = routing_stats.queue_depth,
                .presented_to_immune = false
            };

            // Record anomaly
            routing_immune_record_anomaly(bridge, &anomaly);

            // Present to immune system if severe enough
            if (anomaly.severity > 0.3f) {
                uint32_t antigen_id;
                if (routing_immune_present_anomaly(bridge, &anomaly, &antigen_id)) {
                    // Mark as presented
                    nimcp_platform_mutex_lock(bridge->base.mutex);
                    if (bridge->anomaly_count > 0) {
                        bridge->anomalies[bridge->anomaly_count - 1].presented_to_immune = true;
                        bridge->anomalies[bridge->anomaly_count - 1].antigen_id = antigen_id;
                    }
                    nimcp_platform_mutex_unlock(bridge->base.mutex);
                }
            }
        }
    }

    return true;
}

bool routing_immune_get_stats(
    const routing_immune_bridge_t* bridge,
    routing_immune_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "routing_immune_get_stats: required parameter is NULL (bridge, stats)");
        return false;
    }

    /* P1 fix: Check if mutex exists and lock succeeded before accessing stats */
    if (bridge->base.mutex) {
        if (nimcp_platform_mutex_lock(bridge->base.mutex) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "routing_immune_get_stats: validation failed");
            return false;  /* Failed to acquire lock */
        }
        *stats = bridge->stats;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
    } else {
        /* No mutex - read without lock (not thread-safe but avoid crash) */
        *stats = bridge->stats;
    }

    return true;
}

float routing_immune_get_inflammation_boost(
    const routing_immune_bridge_t* bridge
) {
    if (!bridge) {
        return 1.0f;
    }

    return bridge->inflammation_boost;
}

float routing_immune_get_cytokine_modifier(
    const routing_immune_bridge_t* bridge
) {
    if (!bridge) {
        return 1.0f;
    }

    return bridge->cytokine_attention_mod;
}

routing_immune_strategy_t routing_immune_get_strategy(
    const routing_immune_bridge_t* bridge
) {
    if (!bridge) {
        return ROUTING_STRATEGY_NORMAL;
    }

    return bridge->strategy;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* routing_anomaly_type_name(routing_anomaly_type_t type) {
    switch (type) {
        case ROUTING_ANOMALY_NONE: return "NONE";
        case ROUTING_ANOMALY_HIGH_DROP_RATE: return "HIGH_DROP_RATE";
        case ROUTING_ANOMALY_HIGH_LATENCY: return "HIGH_LATENCY";
        case ROUTING_ANOMALY_QUEUE_OVERFLOW: return "QUEUE_OVERFLOW";
        case ROUTING_ANOMALY_ATTENTION_COLLAPSE: return "ATTENTION_COLLAPSE";
        case ROUTING_ANOMALY_ROUTE_FAILURE: return "ROUTE_FAILURE";
        case ROUTING_ANOMALY_SIGNAL_CORRUPTION: return "SIGNAL_CORRUPTION";
        default: return "UNKNOWN";
    }
}

const char* routing_immune_strategy_name(routing_immune_strategy_t strategy) {
    switch (strategy) {
        case ROUTING_STRATEGY_NORMAL: return "NORMAL";
        case ROUTING_STRATEGY_ALERT: return "ALERT";
        case ROUTING_STRATEGY_DEFENSIVE: return "DEFENSIVE";
        case ROUTING_STRATEGY_EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}
