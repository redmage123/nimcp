/**
 * @file nimcp_joy_thalamic_bridge.c
 * @brief Joy-Thalamic Bridge Implementation
 *
 * WHAT: Routes joy/positive affect signals through the thalamic router
 * WHY: Joy experience requires conscious awareness via thalamic gating
 * HOW: Packages joy signals into routed_signal_t and calls thalamic_router_route_signal
 */

#include "cognitive/joy/nimcp_joy_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(joy_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_joy_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_joy_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t joy_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_joy_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "joy_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "joy_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_joy_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_joy_thalamic_bridge_mesh_registry = registry;
    return err;
}

void joy_thalamic_bridge_mesh_unregister(void) {
    if (g_joy_thalamic_bridge_mesh_registry && g_joy_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_joy_thalamic_bridge_mesh_registry, g_joy_thalamic_bridge_mesh_id);
        g_joy_thalamic_bridge_mesh_id = 0;
        g_joy_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from joy_thalamic_bridge module (instance-level) */
static inline void joy_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_joy_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_joy_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_joy_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "JOY_THALAMIC_BRIDGE"


/* Source ID for joy signals in thalamic routing */
#define JOY_THALAMIC_SOURCE_ID 0x1200

/* Default destination IDs for joy signals */
#define JOY_DEST_VENTRAL_STRIATUM  0x2101
#define JOY_DEST_ORBITOFRONTAL     0x2102
#define JOY_DEST_PREFRONTAL        0x2103

struct joy_thalamic_bridge {
    bridge_base_t base;
    void* joy;
    thalamic_router_t* router;
    joy_thalamic_config_t config;
    joy_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

joy_thalamic_config_t joy_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_joy_thalamic_default", 0.0f);


    joy_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_savoring_boost = true,
        .min_hedonic_threshold = 0.3f,
        .anticipation_threshold = 0.5f
    };
    return cfg;
}

joy_thalamic_bridge_t* joy_thalamic_bridge_create(void* joy, thalamic_router_t* router, const joy_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_create", 0.0f);


    joy_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(joy_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "joy_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->joy = joy;
    bridge->router = router;
    bridge->config = config ? *config : joy_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "joy_thalamic");
    return bridge;
}

void joy_thalamic_bridge_destroy(joy_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "joy_thalamic");
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int joy_thalamic_bridge_reset(joy_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route joy/pleasure signal through thalamic router
 *
 * WHAT: Package joy signal and route through thalamic attention mechanism
 * WHY: Joy signals need attention-gated conscious experience
 * HOW: Create routed_signal_t, apply attention weight, call router
 */
int joy_thalamic_route_pleasure(joy_thalamic_bridge_t* bridge, const joy_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_route_pleasure: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_joy_thalamic_route_p", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Attention gating: filter signals below threshold */
    if (bridge->config.enable_attention_gating &&
        signal->hedonic_value < bridge->config.min_hedonic_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Signal gated, not an error */
    }

    /* Route signal through thalamic router if available */
    if (bridge->router) {
        /* Define destinations for joy signals */
        uint32_t dest_ids[] = {
            JOY_DEST_VENTRAL_STRIATUM,
            JOY_DEST_ORBITOFRONTAL,
            JOY_DEST_PREFRONTAL
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package signal data: joy_intensity, hedonic_value, duration_expectation */
        float signal_data[3] = {
            signal->joy_intensity,
            signal->hedonic_value,
            signal->duration_expectation
        };

        /* Determine priority based on joy intensity */
        signal_priority_t priority = SIGNAL_PRIORITY_NORMAL;
        if (signal->joy_intensity > 0.8f) {
            priority = SIGNAL_PRIORITY_HIGH;
        } else if (signal->joy_intensity < 0.4f) {
            priority = SIGNAL_PRIORITY_LOW;
        }

        /* Create routed signal packet */
        routed_signal_t routed = {
            .source_id = JOY_THALAMIC_SOURCE_ID | signal->signal_type,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 3,
            .attention_weight = bridge->attention_weight,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = (priority == SIGNAL_PRIORITY_HIGH)
        };

        /* Route through thalamic router */
        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_route_pleasure: routed_ok is NULL");
            return -1;  /* Routing failed */
        }
    }

    /* Update statistics AFTER successful routing */
    bridge->stats.pleasures_routed++;
    bridge->stats.avg_joy_intensity = (bridge->stats.avg_joy_intensity * (bridge->stats.pleasures_routed - 1) +
                                       signal->joy_intensity) / bridge->stats.pleasures_routed;

    if (signal->signal_type == JOY_SIGNAL_ANTICIPATION) {
        bridge->stats.anticipations_triggered++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route savoring experience through thalamic router
 *
 * WHAT: Route savoring experience through attention-gated pathway
 * WHY: Savoring requires sustained conscious attention for enhancement
 * HOW: Package duration data, route with boost if enabled
 */
int joy_thalamic_route_savoring(joy_thalamic_bridge_t* bridge, const void* experience, float duration) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_route_savoring: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_joy_thalamic_route_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Route through thalamic router if available */
    if (bridge->router) {
        uint32_t dest_ids[] = {
            JOY_DEST_ORBITOFRONTAL,
            JOY_DEST_PREFRONTAL
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package duration as signal data */
        float signal_data[1] = { duration };

        /* Savoring gets boosted attention if enabled */
        float attention = bridge->attention_weight;
        if (bridge->config.enable_savoring_boost) {
            attention = attention * 1.2f;
            if (attention > 1.0f) attention = 1.0f;
        }

        routed_signal_t routed = {
            .source_id = JOY_THALAMIC_SOURCE_ID | JOY_SIGNAL_SAVORING,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 1,
            .attention_weight = attention,
            .priority = SIGNAL_PRIORITY_NORMAL,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = false
        };

        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_route_savoring: routed_ok is NULL");
            return -1;
        }
    }

    /* Update statistics after successful routing */
    if (bridge->config.enable_savoring_boost) {
        bridge->stats.savoring_episodes++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int joy_thalamic_set_attention(joy_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_joy_thalamic_set_att", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int joy_thalamic_get_attention(joy_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_joy_thalamic_get_att", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int joy_thalamic_bridge_get_stats(joy_thalamic_bridge_t* bridge, joy_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "joy_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int joy_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    joy_thalamic_bridge_heartbeat("joy_thalamic_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Joy_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                joy_thalamic_bridge_heartbeat("joy_thalamic_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Joy_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Joy_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */

void joy_thalamic_bridge_set_instance_health_agent(joy_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "joy_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int joy_thalamic_bridge_training_begin(joy_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "joy_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    joy_thalamic_bridge_heartbeat_instance(bridge->health_agent, "joy_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int joy_thalamic_bridge_training_end(joy_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "joy_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    joy_thalamic_bridge_heartbeat_instance(bridge->health_agent, "joy_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int joy_thalamic_bridge_training_step(joy_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "joy_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    joy_thalamic_bridge_heartbeat_instance(bridge->health_agent, "joy_thalamic_bridge_training_step", progress);
    return 0;
}
