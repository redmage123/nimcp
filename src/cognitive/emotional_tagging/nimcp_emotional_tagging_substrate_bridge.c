/**
 * @file nimcp_emotional_tagging_substrate_bridge.c
 * @brief Emotional Tagging-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/emotional_tagging/nimcp_emotional_tagging_substrate_bridge.h"
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

BRIDGE_BOILERPLATE(emotional_tagging_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "EMOTIONAL_TAGGING_SUBSTRATE_BRIDGE"


struct emotional_tagging_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* emotional_tagging;
    neural_substrate_t* substrate;
    emotional_tagging_substrate_config_t config;
    emotional_tagging_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent (Phase 8) */
};

emotional_tagging_substrate_config_t emotional_tagging_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_emotional_tagging_su", 0.0f);


    emotional_tagging_substrate_config_t cfg = { .enable_atp_modulation = true, .enable_fatigue_modulation = true,
        .enable_bio_async = false, .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .min_capacity = 0.2f };
    return cfg;
}

emotional_tagging_substrate_bridge_t* emotional_tagging_substrate_bridge_create(void* emotional_tagging, neural_substrate_t* substrate, const emotional_tagging_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_create", 0.0f);


    emotional_tagging_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(emotional_tagging_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->emotional_tagging = emotional_tagging;
    bridge->substrate = substrate;
    bridge->config = config ? *config : emotional_tagging_substrate_default_config();
    bridge->effects.tagging_strength = 1.0f;
    bridge->effects.tag_specificity = 1.0f;
    bridge->effects.consolidation_quality = 1.0f;
    bridge->effects.retrieval_accuracy = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    if (bridge_base_init(&bridge->base, 0, "emotional_tagging_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "emotional_tagging_substrate");
    return bridge;
}

void emotional_tagging_substrate_bridge_destroy(emotional_tagging_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotional_tagging_substrate");
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_destroy", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int emotional_tagging_substrate_bridge_update(emotional_tagging_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotional_tagging_substrate_bridge_update: validation failed");
        return -1;
    }
    float atp = metabolic.atp_level, metabolic_cap = metabolic.metabolic_capacity, min_cap = bridge->config.min_capacity;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* ATP enables tagging strength and consolidation */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.tagging_strength = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.consolidation_quality = nimcp_clampf(atp * 1.1f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }
    /* Low fatigue enables specificity and retrieval */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.tag_specificity = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.retrieval_accuracy = nimcp_clampf(metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }
    bridge->effects.overall_capacity = (bridge->effects.tagging_strength + bridge->effects.tag_specificity +
                                        bridge->effects.consolidation_quality + bridge->effects.retrieval_accuracy) / 4.0f;
    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_tagging_substrate_bridge_get_effects(const emotional_tagging_substrate_bridge_t* bridge, emotional_tagging_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_get_effects", 0.0f);


    return 0;
}

int emotional_tagging_substrate_bridge_apply_effects(emotional_tagging_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION, BIO_MODULE_SUBSTRATE_EMOTIONAL_TAGGING, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Emotional tagging uses norepinephrine for salience */
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_EMOTIONAL_TAGGING;
    msg.processing_capacity = bridge->effects.tagging_strength;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.tagging_strength;
    msg.effect_values[1] = bridge->effects.tag_specificity;
    msg.effect_values[2] = bridge->effects.consolidation_quality;
    msg.effect_values[3] = bridge->effects.retrieval_accuracy;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);
    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE, BIO_MODULE_SUBSTRATE_EMOTIONAL_TAGGING, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_EMOTIONAL_TAGGING;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int emotional_tagging_substrate_bridge_register_bio_async(emotional_tagging_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_register_bio_async", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }
    if (!router) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_SUBSTRATE_EMOTIONAL_TAGGING, .module_name = "emotional_tagging_substrate_bridge", .inbox_capacity = 16, .user_data = bridge };
    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) bridge->bio_async_connected = true;
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_tagging_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_substrate_bridge_heartbeat("emotional_ta_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_Tagging_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_tagging_substrate_bridge_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_Tagging_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_Tagging_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void emotional_tagging_substrate_bridge_set_instance_health_agent(emotional_tagging_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int emotional_tagging_substrate_bridge_training_begin(emotional_tagging_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    emotional_tagging_substrate_bridge_heartbeat_instance(bridge->health_agent, "etag_sub_training_begin", 0.0f);
    return 0;
}

int emotional_tagging_substrate_bridge_training_end(emotional_tagging_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    emotional_tagging_substrate_bridge_heartbeat_instance(bridge->health_agent, "etag_sub_training_end", 1.0f);
    return 0;
}

int emotional_tagging_substrate_bridge_training_step(emotional_tagging_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    emotional_tagging_substrate_bridge_heartbeat_instance(bridge->health_agent, "etag_sub_training_step", progress);
    return 0;
}
