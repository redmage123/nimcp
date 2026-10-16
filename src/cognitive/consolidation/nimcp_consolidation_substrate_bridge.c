/**
 * @file nimcp_consolidation_substrate_bridge.c
 * @brief Memory Consolidation-Neural Substrate Bridge Implementation
 *
 * WHAT: Links memory consolidation to metabolic state
 * WHY: Consolidation requires sustained energy for synaptic protein synthesis
 * HOW: Monitors ATP/fatigue; modulates rate, fidelity, and memory priority
 */

#include "cognitive/consolidation/nimcp_consolidation_substrate_bridge.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(consolidation_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


struct consolidation_substrate_bridge {
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* consolidation;
    neural_substrate_t* substrate;
    consolidation_substrate_config_t config;
    consolidation_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

consolidation_substrate_config_t consolidation_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_consolidation_substr", 0.0f);


    consolidation_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

consolidation_substrate_bridge_t* consolidation_substrate_bridge_create(void* consolidation, neural_substrate_t* substrate, const consolidation_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_create", 0.0f);


    consolidation_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(consolidation_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->consolidation = consolidation;
    bridge->substrate = substrate;
    bridge->config = config ? *config : consolidation_substrate_default_config();

    if (bridge_base_init(&bridge->base, 0, "consolidation_substrate") != 0) { nimcp_free(bridge); return NULL; }

    bridge->effects.consolidation_rate = 1.0f;
    bridge->effects.encoding_fidelity = 1.0f;
    bridge->effects.priority_threshold = 1.0f;
    bridge->effects.protein_synthesis = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    return bridge;
}

void consolidation_substrate_bridge_destroy(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_destroy", 0.0f);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int consolidation_substrate_bridge_update(consolidation_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_update", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);

    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    if (bridge->config.enable_atp_modulation) {
        /* Consolidation rate heavily dependent on ATP for protein synthesis */
        bridge->effects.consolidation_rate = nimcp_clampf(atp * 1.2f * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.protein_synthesis = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    if (bridge->config.enable_fatigue_modulation) {
        /* Encoding fidelity decreases with fatigue */
        bridge->effects.encoding_fidelity = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        /* Priority threshold increases (more selective) under metabolic stress */
        bridge->effects.priority_threshold = nimcp_clampf(1.0f - (metabolic_cap * 0.3f), min_cap, 1.0f);
    }

    bridge->effects.overall_capacity = (bridge->effects.consolidation_rate +
                                        bridge->effects.encoding_fidelity +
                                        bridge->effects.priority_threshold +
                                        bridge->effects.protein_synthesis) / 4.0f;

    bridge->update_count++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int consolidation_substrate_bridge_get_effects(const consolidation_substrate_bridge_t* bridge, consolidation_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    /* Lock to prevent torn reads while update() writes under lock */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_get_effects", 0.0f);


    return 0;
}

int consolidation_substrate_bridge_apply_effects(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_CONSOLIDATION, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Consolidation uses acetylcholine */

    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_CONSOLIDATION;
    msg.processing_capacity = bridge->effects.consolidation_rate;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.consolidation_rate;
    msg.effect_values[1] = bridge->effects.encoding_fidelity;
    msg.effect_values[2] = bridge->effects.priority_threshold;
    msg.effect_values[3] = bridge->effects.protein_synthesis;
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
                            BIO_MODULE_SUBSTRATE_CONSOLIDATION, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_CONSOLIDATION;
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
                            BIO_MODULE_SUBSTRATE_CONSOLIDATION, 0, sizeof(alert));
        alert.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
        alert.bridge_module_id = BIO_MODULE_SUBSTRATE_CONSOLIDATION;
        alert.atp_level = atp_level;
        alert.threshold = bridge->config.min_capacity;
        alert.min_capacity = bridge->config.min_capacity;
        bio_router_broadcast(bridge->ctx, &alert, sizeof(alert));
    }

    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int consolidation_substrate_bridge_register_bio_async(consolidation_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_register_bio_async", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }

    if (!router) {
        bridge->router = NULL;
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_CONSOLIDATION,
        .module_name = "consolidation_substrate_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) {
        bridge->bio_async_connected = true;
    }
    bridge->router = router;
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int consolidation_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    consolidation_substrate_bridge_heartbeat("consolidatio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Consolidation_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                consolidation_substrate_bridge_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Consolidation_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Consolidation_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

void consolidation_substrate_bridge_set_instance_health_agent(consolidation_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "consolidation_substrate_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
    g_consolidation_substrate_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("consolidation_substrate_bridge: instance health agent %s", agent ? "set" : "cleared");
}

int consolidation_substrate_bridge_training_begin(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    consolidation_substrate_bridge_heartbeat_instance(bridge->health_agent, "consol_sub_training_begin", 0.0f);
    bridge->update_count = 0;
    bridge->effects.overall_capacity = 1.0f;
    bridge->prev_overall_capacity = 1.0f;
    NIMCP_LOGGING_INFO("consolidation_substrate_bridge: training begun, counters reset");
    return 0;
}

int consolidation_substrate_bridge_training_end(consolidation_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    consolidation_substrate_bridge_heartbeat_instance(bridge->health_agent, "consol_sub_training_end", 1.0f);
    NIMCP_LOGGING_INFO("consolidation_substrate_bridge: training ended, updates=%llu capacity=%.3f",
                       (unsigned long long)bridge->update_count, bridge->effects.overall_capacity);
    return 0;
}

int consolidation_substrate_bridge_training_step(consolidation_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    consolidation_substrate_bridge_heartbeat_instance(bridge->health_agent, "consol_sub_training_step", p);
    bridge->config.min_capacity = 0.2f + 0.8f * p * 0.1f;
    bridge->config.atp_sensitivity = 1.0f + p * 0.05f;
    bridge->config.fatigue_sensitivity = 1.0f + p * 0.03f;
    bridge->update_count++;
    return 0;
}
