/**
 * @file nimcp_salience_thalamic_bridge.c
 * @brief Salience-Thalamic Bridge Implementation
 *
 * WHAT: Routes salience signals through the thalamic router for priority processing
 * WHY: Salient stimuli require attention-gated priority processing via pulvinar
 * HOW: Packages salience signals into routed_signal_t and calls thalamic_router_route_signal
 */

#include "cognitive/salience/nimcp_salience_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(salience_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SALIENCE_THALAMIC_BRIDGE"


/* Source ID for salience signals in thalamic routing */
#define SALIENCE_THALAMIC_SOURCE_ID 0x0A00

/* Default destination IDs for salience signals */
#define SALIENCE_DEST_ANTERIOR_INSULA  0x4001
#define SALIENCE_DEST_ACC              0x4002  /* Anterior Cingulate Cortex */
#define SALIENCE_DEST_PULVINAR         0x4003
#define SALIENCE_DEST_PREFRONTAL       0x4004

struct salience_thalamic_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* salience;
    thalamic_router_t* router;
    salience_thalamic_config_t config;
    salience_thalamic_stats_t stats;
    float attention_weight;
};

salience_thalamic_config_t salience_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_salience_thalamic_de", 0.0f);


    salience_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_priority_override = true,
        .min_salience_threshold = NIMCP_SALIENCE_THRESHOLD,
        .switch_threshold = 0.7f
    };
    return cfg;
}

salience_thalamic_bridge_t* salience_thalamic_bridge_create(void* salience, thalamic_router_t* router, const salience_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_create", 0.0f);


    salience_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(salience_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "salience_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->salience = salience;
    bridge->router = router;
    bridge->config = config ? *config : salience_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "salience_thalamic");
    return bridge;
}

void salience_thalamic_bridge_destroy(salience_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "salience_thalamic");
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int salience_thalamic_bridge_reset(salience_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route salience detection signal through thalamic router
 *
 * WHAT: Package salience signal and route through thalamic attention mechanism
 * WHY: Salient stimuli need priority processing via pulvinar coordination
 * HOW: Create routed_signal_t, apply urgency-based priority, call router
 */
int salience_thalamic_route_detection(salience_thalamic_bridge_t* bridge, const salience_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_route_detection: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_salience_thalamic_ro", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Attention gating: filter signals below threshold */
    if (bridge->config.enable_attention_gating &&
        signal->salience_value < bridge->config.min_salience_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Signal gated, not an error */
    }

    /* Route signal through thalamic router if available */
    if (bridge->router) {
        /* Define destinations for salience signals */
        uint32_t dest_ids[] = {
            SALIENCE_DEST_ANTERIOR_INSULA,
            SALIENCE_DEST_ACC,
            SALIENCE_DEST_PULVINAR,
            SALIENCE_DEST_PREFRONTAL
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package signal data: salience_value, priority_level, urgency */
        float signal_data[3] = {
            signal->salience_value,
            signal->priority_level,
            signal->urgency
        };

        /* Determine priority based on salience value and urgency */
        signal_priority_t priority = SIGNAL_PRIORITY_NORMAL;
        float effective_salience = signal->salience_value * (1.0f + signal->urgency);
        if (effective_salience > 0.8f || signal->signal_type == SALIENCE_SIGNAL_SWITCH) {
            priority = SIGNAL_PRIORITY_HIGH;
        } else if (effective_salience < 0.4f) {
            priority = SIGNAL_PRIORITY_LOW;
        }

        /* Create routed signal packet */
        routed_signal_t routed = {
            .source_id = SALIENCE_THALAMIC_SOURCE_ID | signal->signal_type,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 3,
            .attention_weight = bridge->attention_weight,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = (priority == SIGNAL_PRIORITY_HIGH)  /* High salience bypasses queue */
        };

        /* Route through thalamic router */
        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_thalamic_route_detection: routing failed");
            return -1;  /* Routing failed */
        }
    }

    /* Update statistics AFTER successful routing */
    bridge->stats.detections_routed++;
    if (bridge->stats.detections_routed > 0) {
        bridge->stats.avg_salience_value = (bridge->stats.avg_salience_value * (bridge->stats.detections_routed - 1) +
                                            signal->salience_value) / bridge->stats.detections_routed;
    }

    if (signal->signal_type == SALIENCE_SIGNAL_SWITCH &&
        signal->salience_value >= bridge->config.switch_threshold) {
        bridge->stats.attention_switches++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route priority override signal through thalamic router
 *
 * WHAT: Route priority signal through attention-gated pathway
 * WHY: Priority overrides can redirect attention allocation
 * HOW: Package priority data, route with bypass if threshold met
 */
int salience_thalamic_route_priority(salience_thalamic_bridge_t* bridge, const void* stimulus, float priority) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_route_priority: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_salience_thalamic_ro", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Route through thalamic router if available */
    if (bridge->router) {
        uint32_t dest_ids[] = {
            SALIENCE_DEST_ACC,
            SALIENCE_DEST_PREFRONTAL
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package priority as signal data */
        float signal_data[1] = { priority };

        /* Priority override uses high priority routing */
        signal_priority_t routing_priority = SIGNAL_PRIORITY_NORMAL;
        bool bypass = false;
        if (bridge->config.enable_priority_override &&
            priority >= bridge->config.switch_threshold) {
            routing_priority = SIGNAL_PRIORITY_HIGH;
            bypass = true;
        }

        routed_signal_t routed = {
            .source_id = SALIENCE_THALAMIC_SOURCE_ID | SALIENCE_SIGNAL_PRIORITY,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 1,
            .attention_weight = bridge->attention_weight,
            .priority = routing_priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = bypass
        };

        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_thalamic_route_priority: routing failed");
            return -1;
        }
    }

    /* Update statistics after successful routing */
    if (bridge->config.enable_priority_override &&
        priority >= bridge->config.switch_threshold) {
        bridge->stats.priority_overrides++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int salience_thalamic_set_attention(salience_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_salience_thalamic_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int salience_thalamic_get_attention(salience_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_salience_thalamic_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int salience_thalamic_bridge_get_stats(salience_thalamic_bridge_t* bridge, salience_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int salience_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    salience_thalamic_bridge_heartbeat("salience_tha_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Salience_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                salience_thalamic_bridge_heartbeat("salience_tha_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Salience_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Salience_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_thalamic_bridge_set_instance_health_agent(salience_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "salience_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_thalamic_bridge_training_begin(salience_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    salience_thalamic_bridge_heartbeat_instance(bridge->health_agent, "salience_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int salience_thalamic_bridge_training_end(salience_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    salience_thalamic_bridge_heartbeat_instance(bridge->health_agent, "salience_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int salience_thalamic_bridge_training_step(salience_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_thalamic_bridge_heartbeat_instance(bridge->health_agent, "salience_thalamic_bridge_training_step", progress);
    return 0;
}
