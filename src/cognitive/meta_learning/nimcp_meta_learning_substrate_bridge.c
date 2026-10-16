/**
 * @file nimcp_meta_learning_substrate_bridge.c
 * @brief Meta-Learning-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/meta_learning/nimcp_meta_learning_substrate_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(meta_learning_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "META_LEARNING_SUBSTRATE_BRIDGE"


struct meta_learning_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* meta_learning;
    neural_substrate_t* substrate;
    meta_learning_substrate_config_t config;
    meta_learning_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

meta_learning_substrate_config_t meta_learning_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_meta_learning_substr", 0.0f);


    meta_learning_substrate_config_t cfg = { .enable_atp_modulation = true, .enable_fatigue_modulation = true,
        .enable_bio_async = false, .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .min_capacity = 0.2f };
    return cfg;
}

meta_learning_substrate_bridge_t* meta_learning_substrate_bridge_create(void* meta_learning, neural_substrate_t* substrate, const meta_learning_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_create", 0.0f);


    meta_learning_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(meta_learning_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->meta_learning = meta_learning;
    bridge->substrate = substrate;
    bridge->config = config ? *config : meta_learning_substrate_default_config();
    bridge->effects.learning_rate_adapt = 1.0f;
    bridge->effects.strategy_flexibility = 1.0f;
    bridge->effects.transfer_capacity = 1.0f;
    bridge->effects.plasticity_level = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    if (bridge_base_init(&bridge->base, 0, "meta_learning_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "meta_learning_substrate");
    return bridge;
}

void meta_learning_substrate_bridge_destroy(meta_learning_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "meta_learning_substrate");
    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_destroy", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int meta_learning_substrate_bridge_update(meta_learning_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_substrate_bridge_update: validation failed");
        return -1;
    }
    float atp = metabolic.atp_level, metabolic_cap = metabolic.metabolic_capacity, min_cap = bridge->config.min_capacity;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.learning_rate_adapt = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.plasticity_level = nimcp_clampf(atp * 1.1f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.strategy_flexibility = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.transfer_capacity = nimcp_clampf(metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }
    bridge->effects.overall_capacity = (bridge->effects.learning_rate_adapt + bridge->effects.strategy_flexibility +
                                        bridge->effects.transfer_capacity + bridge->effects.plasticity_level) / 4.0f;
    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int meta_learning_substrate_bridge_get_effects(const meta_learning_substrate_bridge_t* bridge, meta_learning_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_get_effects", 0.0f);


    return 0;
}

int meta_learning_substrate_bridge_apply_effects(meta_learning_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION, BIO_MODULE_SUBSTRATE_META_LEARNING, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_META_LEARNING;
    msg.processing_capacity = bridge->effects.learning_rate_adapt;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.learning_rate_adapt;
    msg.effect_values[1] = bridge->effects.strategy_flexibility;
    msg.effect_values[2] = bridge->effects.transfer_capacity;
    msg.effect_values[3] = bridge->effects.plasticity_level;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);
    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE, BIO_MODULE_SUBSTRATE_META_LEARNING, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_META_LEARNING;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int meta_learning_substrate_bridge_register_bio_async(meta_learning_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_register_bio_async", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }
    if (!router) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_SUBSTRATE_META_LEARNING, .module_name = "meta_learning_substrate_bridge", .inbox_capacity = 16, .user_data = bridge };
    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) bridge->bio_async_connected = true;
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int meta_learning_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    meta_learning_substrate_bridge_heartbeat("meta_learnin_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Meta_Learning_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                meta_learning_substrate_bridge_heartbeat("meta_learnin_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Meta_Learning_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Meta_Learning_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void meta_learning_substrate_bridge_set_instance_health_agent(meta_learning_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "meta_learning_substrate_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int meta_learning_substrate_bridge_training_begin(meta_learning_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_learning_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    meta_learning_substrate_bridge_heartbeat_instance(bridge->health_agent, "meta_learning_substrate_bridge_training_begin", 0.0f);
    return 0;
}

int meta_learning_substrate_bridge_training_end(meta_learning_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_learning_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    meta_learning_substrate_bridge_heartbeat_instance(bridge->health_agent, "meta_learning_substrate_bridge_training_end", 1.0f);
    return 0;
}

int meta_learning_substrate_bridge_training_step(meta_learning_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_learning_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    meta_learning_substrate_bridge_heartbeat_instance(bridge->health_agent, "meta_learning_substrate_bridge_training_step", progress);
    return 0;
}
