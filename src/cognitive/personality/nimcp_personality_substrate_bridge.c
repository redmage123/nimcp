/**
 * @file nimcp_personality_substrate_bridge.c
 * @brief Personality-Neural Substrate Bridge Implementation
 *
 * WHAT: Links personality traits to metabolic state
 * WHY: Personality expression varies with energy and fatigue levels
 * HOW: Monitors ATP/fatigue; modulates trait expression, self-regulation, consistency
 *
 * Uses shared metabolic modulation utilities from nimcp_metabolic_modulation.h
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/personality/nimcp_personality_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(personality_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_personality_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_personality_substrate_bridge_mesh_registry = NULL;

nimcp_error_t personality_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_personality_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "personality_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "personality_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_personality_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_personality_substrate_bridge_mesh_registry = registry;
    return err;
}

void personality_substrate_bridge_mesh_unregister(void) {
    if (g_personality_substrate_bridge_mesh_registry && g_personality_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_personality_substrate_bridge_mesh_registry, g_personality_substrate_bridge_mesh_id);
        g_personality_substrate_bridge_mesh_id = 0;
        g_personality_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from personality_substrate_bridge module (instance-level) */
static inline void personality_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_personality_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_personality_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_personality_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PERSONALITY_SUBSTRATE_BRIDGE"


struct personality_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* personality;
    neural_substrate_t* substrate;
    personality_substrate_config_t config;
    personality_substrate_effects_t effects;
    metabolic_modulation_config_t metabolic_config;  /* Shared metabolic config */
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;

    /* Phase 8: Instance health agent (B24 upgrade) */
    nimcp_health_agent_t* health_agent;
};

personality_substrate_config_t personality_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__personality_substrat", 0.0f);


    personality_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

personality_substrate_bridge_t* personality_substrate_bridge_create(void* personality, neural_substrate_t* substrate, const personality_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__create", 0.0f);


    personality_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(personality_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->personality = personality;
    bridge->substrate = substrate;
    bridge->config = config ? *config : personality_substrate_default_config();

    /* Initialize shared metabolic config with personality-specific multipliers */
    metabolic_effect_multipliers_t personality_mult = {
        .atp_primary_mult = 1.0f,
        .atp_secondary_mult = 1.05f,  /* Personality uses 1.05 for trait consistency */
        .fatigue_primary_mult = 1.0f,
        .fatigue_secondary_mult = 0.9f
    };
    bridge->metabolic_config = metabolic_config_from_fields(
        bridge->config.enable_atp_modulation,
        bridge->config.enable_fatigue_modulation,
        bridge->config.enable_bio_async,
        bridge->config.atp_sensitivity,
        bridge->config.fatigue_sensitivity,
        bridge->config.min_capacity,
        &personality_mult
    );

    /* Initialize effects to full capacity */
    bridge->effects.self_regulation = 1.0f;
    bridge->effects.trait_consistency = 1.0f;
    bridge->effects.stress_resilience = 1.0f;
    bridge->effects.social_energy = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    if (bridge_base_init(&bridge->base, 0, "personality_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "personality_substrate");
    return bridge;
}

void personality_substrate_bridge_destroy(personality_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "personality_substrate");
    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__destroy", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int personality_substrate_bridge_update(personality_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "personality_substrate_bridge_update: validation failed");
        return -1;
    }

    /* Use shared metabolic computation */
    metabolic_input_t input = {
        .atp_level = metabolic.atp_level,
        .metabolic_capacity = metabolic.metabolic_capacity
    };
    metabolic_effects_t generic_effects;
    metabolic_effects_init_full(&generic_effects);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (metabolic_compute_effects(&input, &bridge->metabolic_config, &generic_effects) == 0) {
        /* Map generic effects to personality-specific effect names */
        /* ATP depletion reduces self-control (ego depletion) */
        bridge->effects.self_regulation = generic_effects.primary_atp;
        bridge->effects.trait_consistency = generic_effects.secondary_atp;
        /* Fatigue increases impulsivity, reduces agreeableness */
        bridge->effects.stress_resilience = generic_effects.primary_fatigue;
        bridge->effects.social_energy = generic_effects.secondary_fatigue;
        bridge->effects.overall_capacity = generic_effects.overall_capacity;
    }

    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_substrate_bridge_get_effects(const personality_substrate_bridge_t* bridge, personality_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__get_effects", 0.0f);


    return 0;
}

int personality_substrate_bridge_apply_effects(personality_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_PERSONALITY, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_SEROTONIN;  /* Personality uses serotonin */

    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_PERSONALITY;
    msg.processing_capacity = bridge->effects.self_regulation;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.self_regulation;
    msg.effect_values[1] = bridge->effects.trait_consistency;
    msg.effect_values[2] = bridge->effects.stress_resilience;
    msg.effect_values[3] = bridge->effects.social_energy;
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
                            BIO_MODULE_SUBSTRATE_PERSONALITY, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_PERSONALITY;
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
                            BIO_MODULE_SUBSTRATE_PERSONALITY, 0, sizeof(alert));
        alert.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
        alert.bridge_module_id = BIO_MODULE_SUBSTRATE_PERSONALITY;
        alert.atp_level = atp_level;
        alert.threshold = bridge->config.min_capacity;
        alert.min_capacity = bridge->config.min_capacity;
        bio_router_broadcast(bridge->ctx, &alert, sizeof(alert));
    }

    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int personality_substrate_bridge_register_bio_async(personality_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "personality_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__register_bio_async", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }

    if (!router) {
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_PERSONALITY,
        .module_name = "personality_substrate_bridge",
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

int personality_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    personality_substrate_bridge_heartbeat("personality__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Personality_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                personality_substrate_bridge_heartbeat("personality__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Personality_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Personality_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

//=============================================================================
// Instance Health Agent Setter (B24 Upgrade)
//=============================================================================

void personality_substrate_bridge_set_instance_health_agent(
    personality_substrate_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B24 Upgrade)
//=============================================================================

int personality_substrate_bridge_training_begin(personality_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    personality_substrate_bridge_heartbeat_instance(bridge->health_agent, "personality_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int personality_substrate_bridge_training_end(personality_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    personality_substrate_bridge_heartbeat_instance(bridge->health_agent, "personality_substrate_bridge_training_end", 1.0f);
    return 0;
}

int personality_substrate_bridge_training_step(personality_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    personality_substrate_bridge_heartbeat_instance(bridge->health_agent, "personality_substrate_bridge_training_step", progress);
    return 0;
}
