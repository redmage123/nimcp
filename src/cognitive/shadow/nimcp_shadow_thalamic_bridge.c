/**
 * @file nimcp_shadow_thalamic_bridge.c
 * @brief Shadow-Thalamic Bridge Implementation
 *
 * WHAT: Routes shadow/unconscious content signals through the thalamic router
 * WHY: Shadow integration requires controlled conscious access via thalamic gating
 * HOW: Packages shadow signals into routed_signal_t and calls thalamic_router_route_signal
 */

#include "cognitive/shadow/nimcp_shadow_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(shadow_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_shadow_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_shadow_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t shadow_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_shadow_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "shadow_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "shadow_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_shadow_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_shadow_thalamic_bridge_mesh_registry = registry;
    return err;
}

void shadow_thalamic_bridge_mesh_unregister(void) {
    if (g_shadow_thalamic_bridge_mesh_registry && g_shadow_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_shadow_thalamic_bridge_mesh_registry, g_shadow_thalamic_bridge_mesh_id);
        g_shadow_thalamic_bridge_mesh_id = 0;
        g_shadow_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from shadow_thalamic_bridge module (instance-level) */
static inline void shadow_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_shadow_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_shadow_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_shadow_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "SHADOW_THALAMIC_BRIDGE"


/* Source ID for shadow signals in thalamic routing */
#define SHADOW_THALAMIC_SOURCE_ID 0x1500

/* Default destination IDs for shadow signals */
#define SHADOW_DEST_LIMBIC       0x6001
#define SHADOW_DEST_PREFRONTAL   0x6002
#define SHADOW_DEST_ACC          0x6003  /* Anterior Cingulate Cortex */
#define SHADOW_DEST_INSULA       0x6004

struct shadow_thalamic_bridge {
    bridge_base_t base;
    void* shadow;
    thalamic_router_t* router;
    shadow_thalamic_config_t config;
    shadow_thalamic_stats_t stats;
    float attention_weight;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS(shadow_thalamic_bridge)

shadow_thalamic_config_t shadow_thalamic_default_config(void) {
    shadow_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_gradual_emergence = true,
        .min_emergence_threshold = 0.3f,
        .integration_threshold = 0.5f
    };
    return cfg;
}

shadow_thalamic_bridge_t* shadow_thalamic_bridge_create(void* shadow, thalamic_router_t* router, const shadow_thalamic_config_t* config) {
    shadow_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(shadow_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "shadow_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->shadow = shadow;
    bridge->router = router;
    bridge->config = config ? *config : shadow_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "shadow_thalamic");
    return bridge;
}

void shadow_thalamic_bridge_destroy(shadow_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "shadow_thalamic");
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int shadow_thalamic_bridge_reset(shadow_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route shadow emergence signal through thalamic router
 *
 * WHAT: Package shadow signal and route through thalamic attention mechanism
 * WHY: Shadow content needs controlled, gradual conscious access
 * HOW: Create routed_signal_t, apply gradual emergence if enabled, call router
 */
int shadow_thalamic_route_emergence(shadow_thalamic_bridge_t* bridge, const shadow_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_route_emergence: required parameter is NULL (bridge, signal)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(shadow_thalamic_signal_t));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Attention gating: filter signals below threshold */
    if (bridge->config.enable_attention_gating &&
        signal->emergence_strength < bridge->config.min_emergence_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Signal gated, not an error */
    }

    /* Route signal through thalamic router if available */
    if (bridge->router) {
        /* Define destinations for shadow signals */
        uint32_t dest_ids[] = {
            SHADOW_DEST_LIMBIC,
            SHADOW_DEST_PREFRONTAL,
            SHADOW_DEST_ACC,
            SHADOW_DEST_INSULA
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package signal data: emergence_strength, integration_potential, awareness_level */
        float signal_data[3] = {
            signal->emergence_strength,
            signal->integration_potential,
            signal->awareness_level
        };

        /* Apply gradual emergence modulation to attention if enabled */
        float attention = bridge->attention_weight;
        if (bridge->config.enable_gradual_emergence) {
            /* Gradual emergence: attenuate sudden strong signals to prevent overwhelming */
            float emergence_factor = signal->emergence_strength;
            if (emergence_factor > 0.7f) {
                /* Strong emergence gets dampened to allow gradual integration */
                attention = attention * (0.7f + 0.3f * (1.0f - emergence_factor));
            }
        }

        /* Determine priority - shadow signals are typically low/normal priority
         * to allow conscious processing to prepare */
        signal_priority_t priority = SIGNAL_PRIORITY_LOW;
        if (signal->signal_type == SHADOW_SIGNAL_PROJECTION) {
            /* Projections may need higher priority for conscious recognition */
            priority = SIGNAL_PRIORITY_NORMAL;
        } else if (signal->emergence_strength > 0.8f) {
            priority = SIGNAL_PRIORITY_NORMAL;
        }

        /* Create routed signal packet */
        routed_signal_t routed = {
            .source_id = SHADOW_THALAMIC_SOURCE_ID | signal->signal_type,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 3,
            .attention_weight = attention,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = false  /* Shadow signals should not bypass queue */
        };

        /* Route through thalamic router */
        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_route_emergence: routed_ok is NULL");
            return -1;  /* Routing failed */
        }
    }

    /* Update statistics AFTER successful routing */
    bridge->stats.emergences_routed++;
    bridge->stats.avg_emergence_strength = (bridge->stats.avg_emergence_strength * (bridge->stats.emergences_routed - 1) +
                                            signal->emergence_strength) / bridge->stats.emergences_routed;

    if (signal->signal_type == SHADOW_SIGNAL_PROJECTION) {
        bridge->stats.projections_detected++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route shadow integration signal through thalamic router
 *
 * WHAT: Route integration readiness through attention-gated pathway
 * WHY: Integration requires conscious attention and readiness
 * HOW: Package readiness data, route with appropriate priority
 */
int shadow_thalamic_route_integration(shadow_thalamic_bridge_t* bridge, const void* content, float readiness) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_route_integration: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Route through thalamic router if available */
    if (bridge->router) {
        uint32_t dest_ids[] = {
            SHADOW_DEST_PREFRONTAL,
            SHADOW_DEST_ACC
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package readiness as signal data */
        float signal_data[1] = { readiness };

        /* Integration signals get normal priority when threshold met */
        signal_priority_t priority = SIGNAL_PRIORITY_LOW;
        if (readiness >= bridge->config.integration_threshold) {
            priority = SIGNAL_PRIORITY_NORMAL;
        }

        routed_signal_t routed = {
            .source_id = SHADOW_THALAMIC_SOURCE_ID | SHADOW_SIGNAL_INTEGRATION,
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
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_route_integration: routed_ok is NULL");
            return -1;
        }
    }

    /* Update statistics after successful routing */
    if (readiness >= bridge->config.integration_threshold) {
        bridge->stats.integrations_achieved++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_thalamic_set_attention(shadow_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_thalamic_get_attention(shadow_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_thalamic_bridge_get_stats(shadow_thalamic_bridge_t* bridge, shadow_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int shadow_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Shadow_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Shadow_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Shadow_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void shadow_thalamic_bridge_set_instance_health_agent(shadow_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shadow_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int shadow_thalamic_bridge_training_begin(shadow_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    shadow_thalamic_bridge_heartbeat_instance(bridge->health_agent, "shadow_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int shadow_thalamic_bridge_training_end(shadow_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    shadow_thalamic_bridge_heartbeat_instance(bridge->health_agent, "shadow_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int shadow_thalamic_bridge_training_step(shadow_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    shadow_thalamic_bridge_heartbeat_instance(bridge->health_agent, "shadow_thalamic_bridge_training_step", progress);
    return 0;
}
