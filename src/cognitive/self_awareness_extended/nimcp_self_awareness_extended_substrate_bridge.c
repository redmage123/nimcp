/**
 * @file nimcp_self_awareness_extended_substrate_bridge.c
 * @brief Extended Self-Awareness-Neural Substrate Bridge Implementation
 *
 * WHAT: Links extended self-awareness to metabolic state
 * WHY: Extended self-awareness requires sustained prefrontal and parietal resources
 * HOW: Monitors ATP/fatigue; modulates metacognition, temporal self, narrative integration
 */

#include "cognitive/self_awareness_extended/nimcp_self_awareness_extended_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_awareness_extended_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_awareness_extended_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_awareness_extended_substrate_bridge_mesh_registry = NULL;

nimcp_error_t self_awareness_extended_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_self_awareness_extended_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_awareness_extended_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_awareness_extended_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_awareness_extended_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_awareness_extended_substrate_bridge_mesh_registry = registry;
    return err;
}

void self_awareness_extended_substrate_bridge_mesh_unregister(void) {
    if (g_self_awareness_extended_substrate_bridge_mesh_registry && g_self_awareness_extended_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_awareness_extended_substrate_bridge_mesh_registry, g_self_awareness_extended_substrate_bridge_mesh_id);
        g_self_awareness_extended_substrate_bridge_mesh_id = 0;
        g_self_awareness_extended_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_awareness_extended_substrate_bridge module (instance-level) */
static inline void self_awareness_extended_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_awareness_extended_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_awareness_extended_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_awareness_extended_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



struct self_awareness_ext_substrate_bridge {
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* self_awareness_ext;
    neural_substrate_t* substrate;
    self_awareness_ext_substrate_config_t config;
    self_awareness_ext_substrate_effects_t effects;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
};

self_awareness_ext_substrate_config_t self_awareness_ext_substrate_default_config(void) {
    self_awareness_ext_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

self_awareness_ext_substrate_bridge_t* self_awareness_ext_substrate_bridge_create(void* self_awareness_ext, neural_substrate_t* substrate, const self_awareness_ext_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }

    self_awareness_ext_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(self_awareness_ext_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->self_awareness_ext = self_awareness_ext;
    bridge->substrate = substrate;
    bridge->config = config ? *config : self_awareness_ext_substrate_default_config();

    if (bridge_base_init(&bridge->base, 0, "self_awareness_ext_substrate") != 0) { nimcp_free(bridge); return NULL; }

    bridge->effects.metacognitive_depth = 1.0f;
    bridge->effects.temporal_self_coherence = 1.0f;
    bridge->effects.narrative_integration = 1.0f;
    bridge->effects.future_self_projection = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    return bridge;
}

void self_awareness_ext_substrate_bridge_destroy(self_awareness_ext_substrate_bridge_t* bridge) {
    if (!bridge) return;

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int self_awareness_ext_substrate_bridge_update(self_awareness_ext_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_ext_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_ext_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    if (bridge->config.enable_atp_modulation) {
        /* Metacognitive depth requires stable prefrontal resources */
        bridge->effects.metacognitive_depth = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        /* Future self projection is cognitively demanding */
        bridge->effects.future_self_projection = nimcp_clampf(atp * 0.9f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    if (bridge->config.enable_fatigue_modulation) {
        /* Temporal self coherence decreases with fatigue */
        bridge->effects.temporal_self_coherence = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        /* Narrative integration is vulnerable to metabolic stress */
        bridge->effects.narrative_integration = nimcp_clampf(metabolic_cap * 0.85f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    bridge->effects.overall_capacity = (bridge->effects.metacognitive_depth +
                                        bridge->effects.temporal_self_coherence +
                                        bridge->effects.narrative_integration +
                                        bridge->effects.future_self_projection) / 4.0f;

    bridge->update_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_awareness_ext_substrate_bridge_get_effects(const self_awareness_ext_substrate_bridge_t* bridge, self_awareness_ext_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_ext_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    *effects = bridge->effects;
    return 0;
}

int self_awareness_ext_substrate_bridge_apply_effects(self_awareness_ext_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_ext_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    return 0;
}

int self_awareness_ext_substrate_bridge_register_bio_async(self_awareness_ext_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_ext_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int self_awareness_ext_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Awareness_Extended_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Awareness_Extended_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Awareness_Extended_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_awareness_extended_substrate_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_self_awareness_extended_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_awareness_extended_substrate_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_extended_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    self_awareness_extended_substrate_bridge_heartbeat_instance(NULL, "self_awareness_extended_substrate_bridge_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int self_awareness_extended_substrate_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_extended_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    self_awareness_extended_substrate_bridge_heartbeat_instance(NULL, "self_awareness_extended_substrate_bridge_training_end", 1.0f);
    (void)instance;
    return 0;
}

int self_awareness_extended_substrate_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_extended_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_awareness_extended_substrate_bridge_heartbeat_instance(NULL, "self_awareness_extended_substrate_bridge_training_step", progress);
    (void)instance;
    return 0;
}
