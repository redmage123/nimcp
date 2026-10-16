/**
 * @file nimcp_ethics_substrate_bridge.c
 * @brief Ethics-Neural Substrate Bridge Implementation
 *
 * WHAT: Links ethical reasoning to metabolic state
 * WHY: Moral cognition requires sustained prefrontal resources
 * HOW: Monitors ATP/fatigue; modulates moral reasoning, judgment speed, consistency
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/ethics/nimcp_ethics_substrate_bridge.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ethics_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_ethics_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_ethics_substrate_bridge_mesh_registry = NULL;

nimcp_error_t ethics_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_ethics_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "ethics_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "ethics_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_ethics_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_ethics_substrate_bridge_mesh_registry = registry;
    return err;
}

void ethics_substrate_bridge_mesh_unregister(void) {
    if (g_ethics_substrate_bridge_mesh_registry && g_ethics_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_ethics_substrate_bridge_mesh_registry, g_ethics_substrate_bridge_mesh_id);
        g_ethics_substrate_bridge_mesh_id = 0;
        g_ethics_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void ethics_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_ethics_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_ethics_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_ethics_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct ethics_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* ethics;
    neural_substrate_t* substrate;
    ethics_substrate_config_t config;
    ethics_substrate_effects_t effects;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
};

ethics_substrate_config_t ethics_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_ethics_substrate_def", 0.0f);


    ethics_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

ethics_substrate_bridge_t* ethics_substrate_bridge_create(void* ethics, neural_substrate_t* substrate, const ethics_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_create", 0.0f);


    ethics_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(ethics_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->ethics = ethics;
    bridge->substrate = substrate;
    bridge->config = config ? *config : ethics_substrate_default_config();

    bridge->effects.moral_clarity = 1.0f;
    bridge->effects.deliberation_depth = 1.0f;
    bridge->effects.bias_resistance = 1.0f;
    bridge->effects.empathy_capacity = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    if (bridge_base_init(&bridge->base, 0, "ethics_substrate") != 0) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void ethics_substrate_bridge_destroy(ethics_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_destroy", 0.0f);


    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int ethics_substrate_bridge_update(ethics_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.moral_clarity = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.bias_resistance = nimcp_clampf(atp * 1.1f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.deliberation_depth = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.empathy_capacity = nimcp_clampf(metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    bridge->effects.overall_capacity = (bridge->effects.moral_clarity +
                                        bridge->effects.deliberation_depth +
                                        bridge->effects.bias_resistance +
                                        bridge->effects.empathy_capacity) / 4.0f;

    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ethics_substrate_bridge_get_effects(const ethics_substrate_bridge_t* bridge, ethics_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_get_effects", 0.0f);


    return 0;
}

int ethics_substrate_bridge_apply_effects(ethics_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_apply_effects", 0.0f);


    return 0;
}

int ethics_substrate_bridge_register_bio_async(ethics_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_register_bio_async", 0.0f);


    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Substrate Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_substrate_bridge_heartbeat("ethics_subst_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Substrate_Bridge_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_substrate_bridge_heartbeat("ethics_subst_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Ethics substrate bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Substrate_Bridge_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Substrate_Bridge_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_substrate_bridge_set_instance_health_agent(ethics_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "ethics_substrate_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_substrate_bridge_training_begin(ethics_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    ethics_substrate_bridge_heartbeat_instance(bridge->health_agent, "ethics_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int ethics_substrate_bridge_training_end(ethics_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    ethics_substrate_bridge_heartbeat_instance(bridge->health_agent, "ethics_substrate_bridge_training_end", 1.0f);
    return 0;
}

int ethics_substrate_bridge_training_step(ethics_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_substrate_bridge_heartbeat_instance(bridge->health_agent, "ethics_substrate_bridge_training_step", progress);
    return 0;
}
