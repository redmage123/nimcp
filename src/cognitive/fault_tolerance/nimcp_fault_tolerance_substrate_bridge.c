/**
 * @file nimcp_fault_tolerance_substrate_bridge.c
 * @brief Fault Tolerance-Neural Substrate Bridge Implementation
 *
 * WHAT: Links fault tolerance to metabolic state
 * WHY: Error handling requires sustained prefrontal resources
 * HOW: Monitors ATP/fatigue; modulates detection, recovery, redundancy
 */

#include "cognitive/fault_tolerance/nimcp_fault_tolerance_substrate_bridge.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fault_tolerance_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_fault_tolerance_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_fault_tolerance_substrate_bridge_mesh_registry = NULL;

nimcp_error_t fault_tolerance_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_fault_tolerance_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "fault_tolerance_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "fault_tolerance_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_fault_tolerance_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_fault_tolerance_substrate_bridge_mesh_registry = registry;
    return err;
}

void fault_tolerance_substrate_bridge_mesh_unregister(void) {
    if (g_fault_tolerance_substrate_bridge_mesh_registry && g_fault_tolerance_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_fault_tolerance_substrate_bridge_mesh_registry, g_fault_tolerance_substrate_bridge_mesh_id);
        g_fault_tolerance_substrate_bridge_mesh_id = 0;
        g_fault_tolerance_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from fault_tolerance_substrate_bridge module (instance-level) */
static inline void fault_tolerance_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fault_tolerance_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fault_tolerance_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fault_tolerance_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct fault_tolerance_substrate_bridge {
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    void* fault_tolerance;
    neural_substrate_t* substrate;
    fault_tolerance_substrate_config_t config;
    fault_tolerance_substrate_effects_t effects;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;

    /* Phase 8: Instance health agent */
    nimcp_health_agent_t* health_agent;         /**< Health agent (Phase 8) */
};

fault_tolerance_substrate_config_t fault_tolerance_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_fault_tolerance_subs", 0.0f);


    fault_tolerance_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.3f  /* Higher min for critical fault tolerance */
    };
    return cfg;
}

fault_tolerance_substrate_bridge_t* fault_tolerance_substrate_bridge_create(void* fault_tolerance, neural_substrate_t* substrate, const fault_tolerance_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_create", 0.0f);


    fault_tolerance_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(fault_tolerance_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->fault_tolerance = fault_tolerance;
    bridge->substrate = substrate;
    bridge->config = config ? *config : fault_tolerance_substrate_default_config();

    if (bridge_base_init(&bridge->base, 0, "fault_tolerance_substrate") != 0) { nimcp_free(bridge); return NULL; }

    bridge->effects.detection_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.recovery_speed = 1.0f;
    bridge->effects.redundancy_capacity = 1.0f;
    bridge->effects.monitoring_depth = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    return bridge;
}

void fault_tolerance_substrate_bridge_destroy(fault_tolerance_substrate_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_destroy", 0.0f);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int fault_tolerance_substrate_bridge_update(fault_tolerance_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_update: NULL argument "
                              "(bridge=%p)", (const void*)bridge);
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_update", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fault_tolerance_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    if (bridge->config.enable_atp_modulation) {
        /* Detection sensitivity requires stable ATP for continuous monitoring */
        bridge->effects.detection_sensitivity = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        /* Redundancy maintenance is ATP-intensive */
        bridge->effects.redundancy_capacity = nimcp_clampf(atp * 0.9f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    if (bridge->config.enable_fatigue_modulation) {
        /* Recovery speed decreases with fatigue */
        bridge->effects.recovery_speed = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        /* Monitoring depth narrows under metabolic stress */
        bridge->effects.monitoring_depth = nimcp_clampf(metabolic_cap * 0.85f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    bridge->effects.overall_capacity = (bridge->effects.detection_sensitivity +
                                        bridge->effects.recovery_speed +
                                        bridge->effects.redundancy_capacity +
                                        bridge->effects.monitoring_depth) / 4.0f;

    bridge->update_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fault_tolerance_substrate_bridge_get_effects(const fault_tolerance_substrate_bridge_t* bridge, fault_tolerance_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_get_effects: NULL argument");
        return -1;
    }
    *effects = bridge->effects;
    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_get_effects", 0.0f);


    return 0;
}

int fault_tolerance_substrate_bridge_apply_effects(fault_tolerance_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_apply_effects: NULL bridge");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_apply_effects", 0.0f);


    return 0;
}

int fault_tolerance_substrate_bridge_register_bio_async(fault_tolerance_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_register_bio_async: NULL bridge");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_register_bio_async", 0.0f);


    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int fault_tolerance_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    fault_tolerance_substrate_bridge_heartbeat("fault_tolera_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Fault_Tolerance_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fault_tolerance_substrate_bridge_heartbeat("fault_tolera_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Fault_Tolerance_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Fault_Tolerance_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void fault_tolerance_substrate_bridge_set_instance_health_agent(fault_tolerance_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        struct fault_tolerance_substrate_bridge* ctx = (struct fault_tolerance_substrate_bridge*)bridge;
        ctx->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Functions (FULL implementation)
 * ============================================================================ */
int fault_tolerance_substrate_bridge_training_begin(fault_tolerance_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    struct fault_tolerance_substrate_bridge* ctx = (struct fault_tolerance_substrate_bridge*)bridge;
    fault_tolerance_substrate_bridge_heartbeat_instance(ctx->health_agent, "fault_tolera_training_begin", 0.0f);
    ctx->update_count = 0;
    ctx->effects.detection_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    ctx->effects.recovery_speed = 1.0f;
    ctx->effects.redundancy_capacity = 1.0f;
    ctx->effects.monitoring_depth = 1.0f;
    ctx->effects.overall_capacity = 1.0f;
    NIMCP_LOGGING_INFO("%s training begin: counters reset", "fault_tolerance_substrate_bridge");
    return 0;
}

int fault_tolerance_substrate_bridge_training_step(fault_tolerance_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    struct fault_tolerance_substrate_bridge* ctx = (struct fault_tolerance_substrate_bridge*)bridge;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fault_tolerance_substrate_bridge_heartbeat_instance(ctx->health_agent, "fault_tolera_training_step", progress);
    ctx->update_count++;
    /* Modulate effects capacity with training progress */
    ctx->effects.overall_capacity = 0.5f + 0.5f * progress;
    ctx->effects.detection_sensitivity = 0.5f + 0.5f * progress;
    ctx->effects.recovery_speed = 0.5f + 0.5f * progress;
    return 0;
}

int fault_tolerance_substrate_bridge_training_end(fault_tolerance_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fault_tolerance_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    struct fault_tolerance_substrate_bridge* ctx = (struct fault_tolerance_substrate_bridge*)bridge;
    fault_tolerance_substrate_bridge_heartbeat_instance(ctx->health_agent, "fault_tolera_training_end", 1.0f);
    ctx->effects.overall_capacity = 1.0f;
    ctx->effects.detection_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    ctx->effects.recovery_speed = 1.0f;
    NIMCP_LOGGING_INFO("%s training end: updates=%lu",
                       "fault_tolerance_substrate_bridge", (unsigned long)ctx->update_count);
    return 0;
}
