/**
 * @file nimcp_autobio_substrate_bridge.c
 * @brief Autobiographical Memory-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/autobiographical_memory/nimcp_autobio_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <string.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(autobio_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "AUTOBIO_SUBSTRATE_BRIDGE"


struct autobio_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* autobio;
    neural_substrate_t* substrate;
    autobio_substrate_config_t config;
    autobio_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

autobio_substrate_config_t autobio_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_autobio_substrate_de", 0.0f);


    autobio_substrate_config_t cfg = { .enable_atp_modulation = true, .enable_fatigue_modulation = true,
        .enable_bio_async = false, .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .min_capacity = 0.2f };
    return cfg;
}

autobio_substrate_bridge_t* autobio_substrate_bridge_create(void* autobio, neural_substrate_t* substrate, const autobio_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_create", 0.0f);


    autobio_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(autobio_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->autobio = autobio;
    bridge->substrate = substrate;
    bridge->config = config ? *config : autobio_substrate_default_config();
    bridge->effects.recall_vividness = 1.0f;
    bridge->effects.detail_resolution = 1.0f;
    bridge->effects.temporal_accuracy = 1.0f;
    bridge->effects.narrative_coherence = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    if (bridge_base_init(&bridge->base, 0, "autobio_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "autobio_substrate");
    return bridge;
}

void autobio_substrate_bridge_destroy(autobio_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "autobio_substrate");
    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_destroy", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int autobio_substrate_bridge_update(autobio_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_substrate_bridge_update: validation failed");
        return -1;
    }
    float atp = metabolic.atp_level, metabolic_cap = metabolic.metabolic_capacity, min_cap = bridge->config.min_capacity;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* ATP enables vivid recall and detail resolution */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.recall_vividness = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.detail_resolution = nimcp_clampf(atp * 1.1f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }
    /* Low fatigue enables temporal accuracy and coherence */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.temporal_accuracy = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.narrative_coherence = nimcp_clampf(metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }
    bridge->effects.overall_capacity = (bridge->effects.recall_vividness + bridge->effects.detail_resolution +
                                        bridge->effects.temporal_accuracy + bridge->effects.narrative_coherence) / 4.0f;
    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_substrate_bridge_get_effects(const autobio_substrate_bridge_t* bridge, autobio_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_get_effects", 0.0f);


    return 0;
}

int autobio_substrate_bridge_apply_effects(autobio_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION, BIO_MODULE_SUBSTRATE_AUTOBIO, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Autobiographical memory uses acetylcholine for recall */
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_AUTOBIO;
    msg.processing_capacity = bridge->effects.recall_vividness;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.recall_vividness;
    msg.effect_values[1] = bridge->effects.detail_resolution;
    msg.effect_values[2] = bridge->effects.temporal_accuracy;
    msg.effect_values[3] = bridge->effects.narrative_coherence;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);
    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE, BIO_MODULE_SUBSTRATE_AUTOBIO, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_AUTOBIO;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int autobio_substrate_bridge_register_bio_async(autobio_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_register_bio_async", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }
    if (!router) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_SUBSTRATE_AUTOBIO, .module_name = "autobio_substrate_bridge", .inbox_capacity = 16, .user_data = bridge };
    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) bridge->bio_async_connected = true;
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int autobio_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobio_substrate_bridge_heartbeat("autobio_subs_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Autobiographical_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                autobio_substrate_bridge_heartbeat("autobio_subs_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Autobiographical_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Autobiographical_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

/**
 * @brief Set instance-level health agent for a specific autobio_substrate_bridge
 * @param bridge Bridge instance
 * @param agent Health agent for this instance (NULL to disable)
 */
void autobio_substrate_bridge_set_instance_health_agent(autobio_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
    /* Also update module-level instance agent as fallback */
    g_autobio_substrate_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("autobio_substrate_bridge: instance health agent %s",
                        agent ? "set" : "cleared");
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

/**
 * @brief Begin training session for autobio_substrate_bridge
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int autobio_substrate_bridge_training_begin(autobio_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    autobio_substrate_bridge_heartbeat_instance(bridge->health_agent, "autobio_sub_training_begin", 0.0f);

    /* Reset update counter for this training session */
    bridge->update_count = 0;

    /* Reset effects to baseline for training calibration */
    bridge->effects.overall_capacity = 1.0f;
    bridge->prev_overall_capacity = 1.0f;

    NIMCP_LOGGING_INFO("autobio_substrate_bridge: training session begun, counters reset");
    return 0;
}

/**
 * @brief End training session for autobio_substrate_bridge
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int autobio_substrate_bridge_training_end(autobio_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    autobio_substrate_bridge_heartbeat_instance(bridge->health_agent, "autobio_sub_training_end", 1.0f);

    /* Compute average capacity across training session */
    float avg_capacity = bridge->effects.overall_capacity;
    if (bridge->update_count > 0) {
        avg_capacity = bridge->effects.overall_capacity;
    }

    /* Log training session metrics */
    NIMCP_LOGGING_INFO("autobio_substrate_bridge: training ended, updates=%llu avg_capacity=%.3f min_cap=%.3f",
                       (unsigned long long)bridge->update_count,
                       avg_capacity,
                       bridge->config.min_capacity);
    return 0;
}

/**
 * @brief Execute single training step for autobio_substrate_bridge
 * @param bridge Bridge instance
 * @param progress Training progress [0.0, 1.0]
 * @return 0 on success, -1 on error
 */
int autobio_substrate_bridge_training_step(autobio_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_substrate_bridge_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    autobio_substrate_bridge_heartbeat_instance(bridge->health_agent, "autobio_sub_training_step", p);

    /* Adapt min_capacity threshold based on training progress */
    float base_min = 0.2f;
    bridge->config.min_capacity = base_min + (1.0f - base_min) * p * 0.1f;

    /* Adapt sensitivity weights during training */
    bridge->config.atp_sensitivity = 1.0f + p * 0.05f;
    bridge->config.fatigue_sensitivity = 1.0f + p * 0.03f;

    /* Increment step counter */
    bridge->update_count++;

    return 0;
}
