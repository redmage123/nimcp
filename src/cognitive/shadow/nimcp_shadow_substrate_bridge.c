/**
 * @file nimcp_shadow_substrate_bridge.c
 * @brief Shadow (Unconscious)-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/shadow/nimcp_shadow_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(shadow_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_shadow_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_shadow_substrate_bridge_mesh_registry = NULL;

nimcp_error_t shadow_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_shadow_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "shadow_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "shadow_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_shadow_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_shadow_substrate_bridge_mesh_registry = registry;
    return err;
}

void shadow_substrate_bridge_mesh_unregister(void) {
    if (g_shadow_substrate_bridge_mesh_registry && g_shadow_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_shadow_substrate_bridge_mesh_registry, g_shadow_substrate_bridge_mesh_id);
        g_shadow_substrate_bridge_mesh_id = 0;
        g_shadow_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from shadow_substrate_bridge module (instance-level) */
static inline void shadow_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_shadow_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_shadow_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_shadow_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "SHADOW_SUBSTRATE_BRIDGE"


struct shadow_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* shadow;
    neural_substrate_t* substrate;
    shadow_substrate_config_t config;
    shadow_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS(shadow_substrate_bridge)

shadow_substrate_config_t shadow_substrate_default_config(void) {
    shadow_substrate_config_t cfg = { .enable_atp_modulation = true, .enable_fatigue_modulation = true,
        .enable_bio_async = false, .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .min_capacity = 0.2f };
    return cfg;
}

shadow_substrate_bridge_t* shadow_substrate_bridge_create(void* shadow, neural_substrate_t* substrate, const shadow_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    shadow_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(shadow_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->shadow = shadow;
    bridge->substrate = substrate;
    bridge->config = config ? *config : shadow_substrate_default_config();
    bridge->effects.repression_strength = 1.0f;
    bridge->effects.integration_capacity = 1.0f;
    bridge->effects.awareness_threshold = 0.5f;
    bridge->effects.projection_tendency = 0.0f;
    bridge->effects.overall_capacity = 1.0f;
    if (bridge_base_init(&bridge->base, 0, "shadow_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "shadow_substrate");
    return bridge;
}

void shadow_substrate_bridge_destroy(shadow_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "shadow_substrate");
    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int shadow_substrate_bridge_update(shadow_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "shadow_substrate_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "shadow_substrate_bridge_update");
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_substrate_bridge_update: validation failed");
        return -1;
    }
    float atp = metabolic.atp_level, fatigue = 1.0f - metabolic.metabolic_capacity, min_cap = bridge->config.min_capacity;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* ATP enables repression strength - low ATP weakens repression */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.repression_strength = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.integration_capacity = nimcp_clampf(atp * 1.1f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }
    /* Fatigue raises awareness threshold and increases projection */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.awareness_threshold = nimcp_clampf(0.5f - fatigue * 0.3f * bridge->config.fatigue_sensitivity, 0.2f, 0.8f);
        bridge->effects.projection_tendency = nimcp_clampf(fatigue * 0.6f * bridge->config.fatigue_sensitivity, 0.0f, 0.8f);
    }
    bridge->effects.overall_capacity = (bridge->effects.repression_strength + bridge->effects.integration_capacity) / 2.0f;
    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int shadow_substrate_bridge_get_effects(const shadow_substrate_bridge_t* bridge, shadow_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_substrate_bridge_apply_effects(shadow_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) {
        return 0;
    }

    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_SHADOW, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_SEROTONIN;  /* Shadow uses serotonin */

    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_SHADOW;
    msg.processing_capacity = bridge->effects.repression_strength;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.repression_strength;
    msg.effect_values[1] = bridge->effects.integration_capacity;
    msg.effect_values[2] = bridge->effects.awareness_threshold;
    msg.effect_values[3] = bridge->effects.projection_tendency;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);

    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE,
                            BIO_MODULE_SUBSTRATE_SHADOW, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_SHADOW;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }

    if (msg.critical_low) {
        bio_msg_substrate_atp_critical_t alert;
        memset(&alert, 0, sizeof(alert));
        bio_msg_init_header(&alert.header, BIO_MSG_SUBSTRATE_ATP_CRITICAL,
                            BIO_MODULE_SUBSTRATE_SHADOW, 0, sizeof(alert));
        alert.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
        alert.bridge_module_id = BIO_MODULE_SUBSTRATE_SHADOW;
        alert.atp_level = atp_level;
        alert.threshold = bridge->config.min_capacity;
        alert.min_capacity = bridge->config.min_capacity;
        bio_router_broadcast(bridge->ctx, &alert, sizeof(alert));
    }

    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int shadow_substrate_bridge_register_bio_async(shadow_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }

    if (!router) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_SHADOW,
        .module_name = "shadow_substrate_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) {
        bridge->bio_async_connected = true;
    }
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int shadow_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Shadow_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Shadow_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Shadow_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void shadow_substrate_bridge_set_instance_health_agent(shadow_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shadow_substrate_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int shadow_substrate_bridge_training_begin(shadow_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    shadow_substrate_bridge_heartbeat_instance(bridge->health_agent, "shadow_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int shadow_substrate_bridge_training_end(shadow_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    shadow_substrate_bridge_heartbeat_instance(bridge->health_agent, "shadow_substrate_bridge_training_end", 1.0f);
    return 0;
}

int shadow_substrate_bridge_training_step(shadow_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    shadow_substrate_bridge_heartbeat_instance(bridge->health_agent, "shadow_substrate_bridge_training_step", progress);
    return 0;
}
