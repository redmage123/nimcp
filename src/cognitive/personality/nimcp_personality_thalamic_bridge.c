/**
 * @file nimcp_personality_thalamic_bridge.c
 * @brief Personality-Thalamic Bridge Implementation
 *
 * WHAT: Routes personality/trait signals through the thalamic router
 * WHY: Personality modulates behavior via conscious and unconscious thalamic pathways
 * HOW: Packages trait signals into routed_signal_t and calls thalamic_router_route_signal
 */

#include "cognitive/personality/nimcp_personality_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(personality_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_personality_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_personality_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t personality_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_personality_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "personality_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "personality_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_personality_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_personality_thalamic_bridge_mesh_registry = registry;
    return err;
}

void personality_thalamic_bridge_mesh_unregister(void) {
    if (g_personality_thalamic_bridge_mesh_registry && g_personality_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_personality_thalamic_bridge_mesh_registry, g_personality_thalamic_bridge_mesh_id);
        g_personality_thalamic_bridge_mesh_id = 0;
        g_personality_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from personality_thalamic_bridge module (instance-level) */
static inline void personality_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_personality_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_personality_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_personality_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PERSONALITY_THALAMIC_BRIDGE"


/* Source ID for personality signals in thalamic routing */
#define PERSONALITY_THALAMIC_SOURCE_ID 0x0600

/* Default destination IDs for personality signals */
#define PERSONALITY_DEST_PREFRONTAL    0x5001
#define PERSONALITY_DEST_ORBITOFRONTAL 0x5002
#define PERSONALITY_DEST_ACC           0x5003  /* Anterior Cingulate Cortex */

struct personality_thalamic_bridge {
    bridge_base_t base;
    void* personality;
    thalamic_router_t* router;
    personality_thalamic_config_t config;
    personality_thalamic_stats_t stats;
    float attention_weight;

    /* Phase 8: Instance health agent (B24 upgrade) */
    nimcp_health_agent_t* health_agent;
};

personality_thalamic_config_t personality_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__personality_thalamic", 0.0f);


    personality_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_state_modulation = true,
        .min_trait_activation = 0.3f,
        .regulation_threshold = 0.5f
    };
    return cfg;
}

personality_thalamic_bridge_t* personality_thalamic_bridge_create(void* personality, thalamic_router_t* router, const personality_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__create", 0.0f);


    personality_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(personality_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "personality_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->personality = personality;
    bridge->router = router;
    bridge->config = config ? *config : personality_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "personality_thalamic");
    return bridge;
}

void personality_thalamic_bridge_destroy(personality_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "personality_thalamic");
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int personality_thalamic_bridge_reset(personality_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route personality trait signal through thalamic router
 *
 * WHAT: Package trait signal and route through thalamic attention mechanism
 * WHY: Trait expression needs attention-gated regulatory processing
 * HOW: Create routed_signal_t, apply state modulation if enabled, call router
 */
int personality_thalamic_route_trait(personality_thalamic_bridge_t* bridge, const personality_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_route_trait: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__personality_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Attention gating: filter signals below threshold */
    if (bridge->config.enable_attention_gating &&
        signal->trait_activation < bridge->config.min_trait_activation) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Signal gated, not an error */
    }

    /* Route signal through thalamic router if available */
    if (bridge->router) {
        /* Define destinations for personality signals */
        uint32_t dest_ids[] = {
            PERSONALITY_DEST_PREFRONTAL,
            PERSONALITY_DEST_ORBITOFRONTAL,
            PERSONALITY_DEST_ACC
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package signal data: trait_activation, state_intensity, regulation_effort */
        float signal_data[3] = {
            signal->trait_activation,
            signal->state_intensity,
            signal->regulation_effort
        };

        /* Apply state modulation to attention if enabled */
        float attention = bridge->attention_weight;
        if (bridge->config.enable_state_modulation &&
            signal->signal_type == PERSONALITY_SIGNAL_STATE) {
            /* State changes get enhanced attention */
            attention = attention * (1.0f + signal->state_intensity * 0.2f);
            if (attention > 1.0f) attention = 1.0f;
        }

        /* Determine priority based on trait activation */
        signal_priority_t priority = SIGNAL_PRIORITY_NORMAL;
        if (signal->trait_activation > 0.8f) {
            priority = SIGNAL_PRIORITY_HIGH;
        } else if (signal->trait_activation < 0.4f) {
            priority = SIGNAL_PRIORITY_LOW;
        }

        /* Create routed signal packet */
        routed_signal_t routed = {
            .source_id = PERSONALITY_THALAMIC_SOURCE_ID | signal->signal_type,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 3,
            .attention_weight = attention,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = false  /* Personality signals don't bypass queue */
        };

        /* Route through thalamic router */
        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "personality_thalamic_route_trait: thalamic routing failed");
            return -1;  /* Routing failed */
        }
    }

    /* Update statistics AFTER successful routing */
    bridge->stats.traits_expressed++;
    bridge->stats.avg_trait_activation = (bridge->stats.avg_trait_activation * (bridge->stats.traits_expressed - 1) +
                                          signal->trait_activation) / bridge->stats.traits_expressed;

    if (signal->signal_type == PERSONALITY_SIGNAL_STATE) {
        bridge->stats.state_changes++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route regulation signal through thalamic router
 *
 * WHAT: Route regulation effort through attention-gated pathway
 * WHY: Trait regulation requires executive attention for control
 * HOW: Package effort data, route with appropriate priority
 */
int personality_thalamic_route_regulation(personality_thalamic_bridge_t* bridge, const void* regulation, float effort) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_route_regulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__personality_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Route through thalamic router if available */
    if (bridge->router) {
        uint32_t dest_ids[] = {
            PERSONALITY_DEST_PREFRONTAL,
            PERSONALITY_DEST_ACC
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package effort as signal data */
        float signal_data[1] = { effort };

        /* High regulation effort gets higher priority */
        signal_priority_t priority = SIGNAL_PRIORITY_NORMAL;
        if (effort >= bridge->config.regulation_threshold) {
            priority = SIGNAL_PRIORITY_HIGH;
        }

        routed_signal_t routed = {
            .source_id = PERSONALITY_THALAMIC_SOURCE_ID | PERSONALITY_SIGNAL_REGULATION,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 1,
            .attention_weight = bridge->attention_weight,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = false
        };

        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "personality_thalamic_route_regulation: thalamic routing failed");
            return -1;
        }
    }

    /* Update statistics after successful routing */
    if (effort >= bridge->config.regulation_threshold) {
        bridge->stats.regulations_applied++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_thalamic_set_attention(personality_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__personality_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_thalamic_get_attention(personality_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__personality_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_thalamic_bridge_get_stats(personality_thalamic_bridge_t* bridge, personality_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int personality_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    personality_thalamic_bridge_heartbeat("personality__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Personality_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                personality_thalamic_bridge_heartbeat("personality__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Personality_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Personality_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

//=============================================================================
// Instance Health Agent Setter (B24 Upgrade)
//=============================================================================

void personality_thalamic_bridge_set_instance_health_agent(
    personality_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B24 Upgrade)
//=============================================================================

int personality_thalamic_bridge_training_begin(personality_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    personality_thalamic_bridge_heartbeat_instance(bridge->health_agent, "personality_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int personality_thalamic_bridge_training_end(personality_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    personality_thalamic_bridge_heartbeat_instance(bridge->health_agent, "personality_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int personality_thalamic_bridge_training_step(personality_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    personality_thalamic_bridge_heartbeat_instance(bridge->health_agent, "personality_thalamic_bridge_training_step", progress);
    return 0;
}
