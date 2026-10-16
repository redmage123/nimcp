/**
 * @file nimcp_social_thalamic_bridge.c
 * @brief Social Cognition-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/social/nimcp_social_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(social_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_social_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_social_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t social_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_social_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "social_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "social_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_social_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_social_thalamic_bridge_mesh_registry = registry;
    return err;
}

void social_thalamic_bridge_mesh_unregister(void) {
    if (g_social_thalamic_bridge_mesh_registry && g_social_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_social_thalamic_bridge_mesh_registry, g_social_thalamic_bridge_mesh_id);
        g_social_thalamic_bridge_mesh_id = 0;
        g_social_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from social_thalamic_bridge module (instance-level) */
static inline void social_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_social_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_social_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_social_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SOCIAL_THALAMIC_BRIDGE"


struct social_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* social;
    thalamic_router_t* router;
    social_thalamic_config_t config;
    social_thalamic_stats_t stats;
    float attention_weight;
};

BRIDGE_DEFINE_SECURITY_SETTERS(social_thalamic_bridge)

social_thalamic_config_t social_thalamic_default_config(void) {
    social_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_salience_boost = true,
        .min_salience_threshold = 0.25f,
        .betrayal_boost = 0.5f  /* Betrayal signals get strong priority */
    };
    return cfg;
}

social_thalamic_bridge_t* social_thalamic_bridge_create(void* social, thalamic_router_t* router, const social_thalamic_config_t* config) {
    social_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(social_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->social = social;
    bridge->router = router;
    bridge->config = config ? *config : social_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "social_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "social_thalamic");
    return bridge;
}

void social_thalamic_bridge_destroy(social_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int social_thalamic_bridge_reset(social_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_thalamic_route_bond(social_thalamic_bridge_t* bridge, const social_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_thalamic_route_bond: required parameter is NULL (bridge, signal)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(social_thalamic_signal_t));
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Betrayal signals bypass attention gating */
    if (bridge->config.enable_attention_gating &&
        signal->social_salience < bridge->config.min_salience_threshold &&
        signal->signal_type != SOCIAL_SIGNAL_BETRAYAL) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.bonds_routed++;
    bridge->stats.avg_social_salience = (bridge->stats.avg_social_salience * (bridge->stats.bonds_routed - 1) +
                                         signal->social_salience) / bridge->stats.bonds_routed;
    if (signal->signal_type == SOCIAL_SIGNAL_BETRAYAL) {
        bridge->stats.betrayals_detected++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_thalamic_route_trust(social_thalamic_bridge_t* bridge, const void* trust_event, float significance) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_thalamic_route_trust: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.trust_events++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_thalamic_set_attention(social_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_thalamic_get_attention(const social_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_thalamic_bridge_get_stats(const social_thalamic_bridge_t* bridge, social_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int social_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Social_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Social_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Social_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void social_thalamic_bridge_set_instance_health_agent(social_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "social_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int social_thalamic_bridge_training_begin(social_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    social_thalamic_bridge_heartbeat_instance(bridge->health_agent, "social_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int social_thalamic_bridge_training_end(social_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    social_thalamic_bridge_heartbeat_instance(bridge->health_agent, "social_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int social_thalamic_bridge_training_step(social_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    social_thalamic_bridge_heartbeat_instance(bridge->health_agent, "social_thalamic_bridge_training_step", progress);
    return 0;
}
