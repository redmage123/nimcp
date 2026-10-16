/**
 * @file nimcp_mirror_thalamic_bridge.c
 * @brief Mirror Neurons-Thalamic Bridge Implementation
 */

#include "cognitive/mirror_neurons/nimcp_mirror_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "async/nimcp_bio_messages.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t mirror_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_thalamic_bridge_mesh_registry = registry;
    return err;
}

void mirror_thalamic_bridge_mesh_unregister(void) {
    if (g_mirror_thalamic_bridge_mesh_registry && g_mirror_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_thalamic_bridge_mesh_registry, g_mirror_thalamic_bridge_mesh_id);
        g_mirror_thalamic_bridge_mesh_id = 0;
        g_mirror_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_thalamic_bridge module (global + instance) */
static inline void mirror_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct mirror_thalamic_bridge {
    bridge_base_t base;
    void* mirror;
    thalamic_router_t* router;
    mirror_thalamic_config_t config;
    mirror_thalamic_stats_t stats;
    float attention_weight;

    /* Bio-async registration */
    bool bio_async_registered;
    uint32_t handler_id;

    /* Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_thalamic_bridge)

mirror_thalamic_config_t mirror_thalamic_default_config(void) {
    mirror_thalamic_bridge_heartbeat("default_config", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: creating default config");

    mirror_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_empathy_boost = true,
        .min_mirroring_strength = 0.3f,
        .empathy_threshold = 0.5f,
        .bio_async_enabled = true,
    };
    return cfg;
}

/* Forward declarations for bio-async */
bool mirror_thalamic_register_bio_async(mirror_thalamic_bridge_t* bridge);
void mirror_thalamic_unregister_bio_async(mirror_thalamic_bridge_t* bridge);

mirror_thalamic_bridge_t* mirror_thalamic_bridge_create(void* mirror, thalamic_router_t* router, const mirror_thalamic_config_t* config) {
    mirror_thalamic_bridge_heartbeat("create", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: create (mirror=%p, router=%p, config=%p)",
                        mirror, (void*)router, (const void*)config);

    mirror_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(mirror_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_WARN("Mirror thalamic bridge: allocation failed (%zu bytes)",
                           sizeof(mirror_thalamic_bridge_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");
        return NULL;
    }
    if (bridge_base_init(&bridge->base, 0, "mirror_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->mirror = mirror;
    bridge->router = router;
    bridge->config = config ? *config : mirror_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Register with bio-async if enabled */
    if (bridge->config.bio_async_enabled) {
        mirror_thalamic_register_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Mirror thalamic bridge: created (attn_gating=%d, empathy_boost=%d)",
                       bridge->config.enable_attention_gating, bridge->config.enable_empathy_boost);
    return bridge;
}

void mirror_thalamic_bridge_destroy(mirror_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "destroy", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: destroy (actions=%lu, empathic=%lu)",
                        (unsigned long)bridge->stats.actions_mirrored,
                        (unsigned long)bridge->stats.empathic_responses);

    if (bridge->bio_async_registered) {
        mirror_thalamic_unregister_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    NIMCP_LOGGING_INFO("Mirror thalamic bridge: destroyed");
    nimcp_free(bridge);
    bridge = NULL;
}

int mirror_thalamic_bridge_reset(mirror_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "reset", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: reset");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_thalamic_route_action(mirror_thalamic_bridge_t* bridge, const mirror_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_route_action: required parameter is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(*signal));
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "route_action", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: route_action (type=0x%x, strength=%.3f)",
                        signal->signal_type, signal->mirroring_strength);

    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->mirroring_strength < bridge->config.min_mirroring_strength) {
        NIMCP_LOGGING_TRACE("Mirror thalamic bridge: gated out (strength=%.3f < min=%.3f)",
                            signal->mirroring_strength, bridge->config.min_mirroring_strength);
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.actions_mirrored++;
    bridge->stats.avg_mirroring_strength = (bridge->stats.avg_mirroring_strength * (bridge->stats.actions_mirrored - 1) +
                                            signal->mirroring_strength) / bridge->stats.actions_mirrored;
    if (signal->signal_type == MIRROR_SIGNAL_IMITATION) {
        bridge->stats.imitations_triggered++;
        NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: imitation triggered (total=%lu)",
                            (unsigned long)bridge->stats.imitations_triggered);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_thalamic_route_empathy(mirror_thalamic_bridge_t* bridge, const void* emotion, float resonance) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_route_empathy: bridge is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, emotion, sizeof(float));
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "route_empathy", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: route_empathy (resonance=%.3f, threshold=%.3f)",
                        resonance, bridge->config.empathy_threshold);

    nimcp_mutex_lock(bridge->base.mutex);
    if (resonance >= bridge->config.empathy_threshold) {
        bridge->stats.empathic_responses++;
        NIMCP_LOGGING_TRACE("Mirror thalamic bridge: empathic response triggered (total=%lu)",
                            (unsigned long)bridge->stats.empathic_responses);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_thalamic_set_attention(mirror_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "set_attention", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: set_attention (raw=%.3f)", attention);

    nimcp_mutex_lock(bridge->base.mutex);
    float old_weight = bridge->attention_weight;
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    if (bridge->attention_weight != old_weight) {
        NIMCP_LOGGING_TRACE("Mirror thalamic bridge: attention changed %.3f -> %.3f",
                            old_weight, bridge->attention_weight);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mirror_thalamic_get_attention(const mirror_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_get_attention: required parameter is NULL");
        return -1;
    }
    *attention = bridge->attention_weight;
    mirror_thalamic_bridge_heartbeat("get_attention", 0.0f);
    return 0;
}

int mirror_thalamic_bridge_get_stats(const mirror_thalamic_bridge_t* bridge, mirror_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    *stats = bridge->stats;
    mirror_thalamic_bridge_heartbeat("get_stats", 0.0f);
    return 0;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

int mirror_thalamic_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    mirror_thalamic_bridge_heartbeat("query_self_knowledge", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: querying self-knowledge from KG");

    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_thalamic_bridge_heartbeat("mirror_thala_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Mirror thalamic bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

bool mirror_thalamic_register_bio_async(mirror_thalamic_bridge_t* bridge) {
    if (!bridge || bridge->bio_async_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_thalamic_register_bio_async: bridge is NULL");
        return false;
    }
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "register_bio_async", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: registering bio-async");
    bridge->bio_async_registered = true;
    return true;
}

void mirror_thalamic_unregister_bio_async(mirror_thalamic_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_registered) return;
    mirror_thalamic_bridge_heartbeat_instance(bridge->health_agent, "unregister_bio_async", 0.0f);
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: unregistering bio-async");
    bridge->bio_async_registered = false;
}

/* ============================================================================
 * Instance-Level Health Agent API
 * ============================================================================ */

int mirror_thalamic_bridge_set_instance_health_agent(
    mirror_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "mirror_thalamic_bridge_set_instance_health_agent: bridge is NULL");
        return -1;
    }
    bridge->health_agent = agent;
    NIMCP_LOGGING_DEBUG("Mirror thalamic bridge: instance health agent %s",
                        agent ? "set" : "cleared");
    return 0;
}

/* ============================================================================
 * Training/Cognitive Integration Hooks (stubs for future wiring)
 * ============================================================================ */

void mirror_thalamic_bridge_on_training_epoch(mirror_thalamic_bridge_t* bridge, uint32_t epoch) {
    (void)bridge; (void)epoch;
    /* TODO: Wire to training layer notifications */
}

void mirror_thalamic_bridge_on_cognitive_event(mirror_thalamic_bridge_t* bridge, uint32_t event_type) {
    (void)bridge; (void)event_type;
    /* TODO: Wire to cognitive event bus */
}

int mirror_thalamic_bridge_training_begin(void* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_thalamic_bridge_heartbeat_instance(NULL, "mirror_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_thalamic_bridge_training_end(void* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_thalamic_bridge_heartbeat_instance(NULL, "mirror_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int mirror_thalamic_bridge_training_step(void* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_thalamic_bridge_heartbeat_instance(NULL, "mirror_thalamic_bridge_training_step", progress);
    return 0;
}
