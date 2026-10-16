/**
 * @file nimcp_remorse_substrate_bridge.c
 * @brief Remorse-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/remorse/nimcp_remorse_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(remorse_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_remorse_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_remorse_substrate_bridge_mesh_registry = NULL;

nimcp_error_t remorse_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_remorse_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "remorse_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "remorse_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_remorse_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_remorse_substrate_bridge_mesh_registry = registry;
    return err;
}

void remorse_substrate_bridge_mesh_unregister(void) {
    if (g_remorse_substrate_bridge_mesh_registry && g_remorse_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_remorse_substrate_bridge_mesh_registry, g_remorse_substrate_bridge_mesh_id);
        g_remorse_substrate_bridge_mesh_id = 0;
        g_remorse_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from remorse_substrate_bridge module (instance-level) */
static inline void remorse_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_remorse_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_remorse_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_remorse_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "REMORSE_SUBSTRATE_BRIDGE"


struct remorse_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* remorse;
    neural_substrate_t* substrate;
    remorse_substrate_config_t config;
    remorse_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

remorse_substrate_config_t remorse_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_remorse_substrate_de", 0.0f);


    remorse_substrate_config_t cfg = { .enable_atp_modulation = true, .enable_fatigue_modulation = true,
        .enable_bio_async = false, .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .min_capacity = 0.2f };
    return cfg;
}

remorse_substrate_bridge_t* remorse_substrate_bridge_create(void* remorse, neural_substrate_t* substrate, const remorse_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_create", 0.0f);


    remorse_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(remorse_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->remorse = remorse;
    bridge->substrate = substrate;
    bridge->config = config ? *config : remorse_substrate_default_config();
    bridge->effects.guilt_processing = 1.0f;
    bridge->effects.repair_motivation = 1.0f;
    bridge->effects.moral_learning = 1.0f;
    bridge->effects.self_forgiveness = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    if (bridge_base_init(&bridge->base, 0, "remorse_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "remorse_substrate");
    return bridge;
}

void remorse_substrate_bridge_destroy(remorse_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "remorse_substrate");
    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_destroy", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int remorse_substrate_bridge_update(remorse_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "remorse_substrate_bridge_update: validation failed");
        return -1;
    }
    float atp = metabolic.atp_level, metabolic_cap = metabolic.metabolic_capacity, min_cap = bridge->config.min_capacity;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* ATP enables guilt processing and repair motivation */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.guilt_processing = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.repair_motivation = nimcp_clampf(atp * 1.1f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }
    /* Low fatigue enables moral learning and self-forgiveness */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.moral_learning = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.self_forgiveness = nimcp_clampf(metabolic_cap * 0.85f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }
    bridge->effects.overall_capacity = (bridge->effects.guilt_processing + bridge->effects.repair_motivation +
                                        bridge->effects.moral_learning + bridge->effects.self_forgiveness) / 4.0f;
    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int remorse_substrate_bridge_get_effects(const remorse_substrate_bridge_t* bridge, remorse_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_get_effects", 0.0f);


    return 0;
}

int remorse_substrate_bridge_apply_effects(remorse_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION, BIO_MODULE_SUBSTRATE_REMORSE, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_SEROTONIN;
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_REMORSE;
    msg.processing_capacity = bridge->effects.guilt_processing;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.guilt_processing;
    msg.effect_values[1] = bridge->effects.repair_motivation;
    msg.effect_values[2] = bridge->effects.moral_learning;
    msg.effect_values[3] = bridge->effects.self_forgiveness;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);
    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE, BIO_MODULE_SUBSTRATE_REMORSE, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_REMORSE;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int remorse_substrate_bridge_register_bio_async(remorse_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_register_bio_async", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }
    if (!router) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_SUBSTRATE_REMORSE, .module_name = "remorse_substrate_bridge", .inbox_capacity = 16, .user_data = bridge };
    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) bridge->bio_async_connected = true;
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int remorse_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    remorse_substrate_bridge_heartbeat("remorse_subs_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Remorse_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                remorse_substrate_bridge_heartbeat("remorse_subs_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Remorse_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Remorse_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void remorse_substrate_bridge_set_instance_health_agent(remorse_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "remorse_substrate_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int remorse_substrate_bridge_training_begin(remorse_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "remorse_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    remorse_substrate_bridge_heartbeat_instance(bridge->health_agent, "remorse_substrate_training_begin", 0.0f);
    return 0;
}

int remorse_substrate_bridge_training_end(remorse_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "remorse_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    remorse_substrate_bridge_heartbeat_instance(bridge->health_agent, "remorse_substrate_training_end", 1.0f);
    return 0;
}

int remorse_substrate_bridge_training_step(remorse_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "remorse_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    remorse_substrate_bridge_heartbeat_instance(bridge->health_agent, "remorse_substrate_training_step", progress);
    return 0;
}
