/**
 * @file nimcp_jepa_substrate_bridge.c
 * @brief JEPA (Joint Embedding Predictive Architecture)-Neural Substrate Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_substrate_bridge.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(jepa_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_jepa_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_jepa_substrate_bridge_mesh_registry = NULL;

nimcp_error_t jepa_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_jepa_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "jepa_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "jepa_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_jepa_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_jepa_substrate_bridge_mesh_registry = registry;
    return err;
}

void jepa_substrate_bridge_mesh_unregister(void) {
    if (g_jepa_substrate_bridge_mesh_registry && g_jepa_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_jepa_substrate_bridge_mesh_registry, g_jepa_substrate_bridge_mesh_id);
        g_jepa_substrate_bridge_mesh_id = 0;
        g_jepa_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from jepa_substrate_bridge module (instance + global) */
static inline void jepa_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_jepa_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_jepa_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_jepa_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "JEPA_SUBSTRATE_BRIDGE"


struct jepa_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* jepa;
    neural_substrate_t* substrate;
    jepa_substrate_config_t config;
    jepa_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

jepa_substrate_config_t jepa_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_jepa_substrate_defau", 0.0f);


    jepa_substrate_config_t cfg = { .enable_atp_modulation = true, .enable_fatigue_modulation = true,
        .enable_bio_async = false, .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT, .min_capacity = 0.2f };
    return cfg;
}

jepa_substrate_bridge_t* jepa_substrate_bridge_create(void* jepa, neural_substrate_t* substrate, const jepa_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_create", 0.0f);


    jepa_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(jepa_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->jepa = jepa;
    bridge->substrate = substrate;
    bridge->config = config ? *config : jepa_substrate_default_config();
    bridge->effects.prediction_horizon = 1.0f;
    bridge->effects.model_precision = 1.0f;
    bridge->effects.embedding_quality = 1.0f;
    bridge->effects.update_rate = 1.0f;
    bridge->effects.overall_capacity = 1.0f;
    if (bridge_base_init(&bridge->base, 0, "jepa_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "jepa_substrate");
    return bridge;
}

void jepa_substrate_bridge_destroy(jepa_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "jepa_substrate");
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_destroy", 0.0f);


    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int jepa_substrate_bridge_update(jepa_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_update", 0.0f);


    NIMCP_CHECK_THROW(bridge && bridge->substrate, -1, "bridge or substrate is NULL");
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) return -1;
    float atp = metabolic.atp_level, metabolic_cap = metabolic.metabolic_capacity, min_cap = bridge->config.min_capacity;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* ATP enables longer prediction horizons and better model precision */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.prediction_horizon = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        bridge->effects.model_precision = nimcp_clampf(atp * 1.05f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }
    /* Low fatigue enables quality embeddings and update rate */
    if (bridge->config.enable_fatigue_modulation) {
        bridge->effects.embedding_quality = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        bridge->effects.update_rate = nimcp_clampf(metabolic_cap * 0.9f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }
    bridge->effects.overall_capacity = (bridge->effects.prediction_horizon + bridge->effects.model_precision +
                                        bridge->effects.embedding_quality + bridge->effects.update_rate) / 4.0f;
    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_substrate_bridge_get_effects(const jepa_substrate_bridge_t* bridge, jepa_substrate_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_get_effects", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, -1, "bridge or effects is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_substrate_bridge_apply_effects(jepa_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_apply_effects", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    if (!bridge->bio_async_connected || !bridge->ctx) return 0;

    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION, BIO_MODULE_SUBSTRATE_JEPA, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;  /* Predictive learning uses dopamine */
    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_JEPA;
    msg.processing_capacity = bridge->effects.prediction_horizon;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.prediction_horizon;
    msg.effect_values[1] = bridge->effects.model_precision;
    msg.effect_values[2] = bridge->effects.embedding_quality;
    msg.effect_values[3] = bridge->effects.update_rate;
    msg.atp_level = atp_level;
    msg.fatigue_level = fatigue_level;
    msg.update_count = bridge->update_count;
    msg.critical_low = (atp_level < bridge->config.min_capacity);
    bio_router_broadcast(bridge->ctx, &msg, sizeof(msg));

    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    if (delta < -0.1f || delta > 0.1f) {
        bio_msg_substrate_capacity_update_t update_msg;
        memset(&update_msg, 0, sizeof(update_msg));
        bio_msg_init_header(&update_msg.header, BIO_MSG_SUBSTRATE_CAPACITY_UPDATE, BIO_MODULE_SUBSTRATE_JEPA, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_JEPA;
        update_msg.old_capacity = bridge->prev_overall_capacity;
        update_msg.new_capacity = bridge->effects.overall_capacity;
        update_msg.delta = delta;
        update_msg.significant_change = true;
        bio_router_broadcast(bridge->ctx, &update_msg, sizeof(update_msg));
    }
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int jepa_substrate_bridge_register_bio_async(jepa_substrate_bridge_t* bridge, bio_router_t* router) {
    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_register_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    if (bridge->bio_async_connected && bridge->ctx) {
        bio_router_unregister_module(bridge->ctx);
        bridge->ctx = NULL;
        bridge->bio_async_connected = false;
    }
    if (!router) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_SUBSTRATE_JEPA, .module_name = "jepa_substrate_bridge", .inbox_capacity = 16, .user_data = bridge };
    bridge->ctx = bio_router_register_module(&info);
    if (bridge->ctx) bridge->bio_async_connected = true;
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int jepa_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_substrate_bridge_heartbeat("jepa_substra_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_substrate_bridge_heartbeat("jepa_substra_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Substrate_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Substrate_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_substrate_bridge_set_instance_health_agent(jepa_substrate_bridge_t* bridge,
                                                      nimcp_health_agent_t* agent) {
    if (!bridge) return;
    bridge->health_agent = agent;
}

int jepa_substrate_bridge_training_begin(jepa_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    jepa_substrate_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_substra_training_begin", 0.0f);

    /* Snapshot initial metabolic effects */
    bridge->prev_overall_capacity = bridge->effects.overall_capacity;

    /* Reset update count for training epoch */
    bridge->update_count = 0;

    /* Reset effects to baseline for clean training start */
    bridge->effects.prediction_horizon = 1.0f;
    bridge->effects.model_precision = 1.0f;
    bridge->effects.embedding_quality = 1.0f;
    bridge->effects.update_rate = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    jepa_substrate_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_substra_training_begin", 1.0f);
    return 0;
}

int jepa_substrate_bridge_training_end(jepa_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    jepa_substrate_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_substra_training_end", 0.0f);

    /* Compute capacity delta over training */
    float delta = bridge->effects.overall_capacity - bridge->prev_overall_capacity;
    (void)delta;  /* Available for diagnostics */

    /* Log final update count */
    NIMCP_LOGGING_DEBUG("Substrate bridge training ended: %lu updates, capacity=%.3f",
                       (unsigned long)bridge->update_count,
                       bridge->effects.overall_capacity);

    jepa_substrate_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_substra_training_end", 1.0f);
    return 0;
}

int jepa_substrate_bridge_training_step(jepa_substrate_bridge_t* bridge, uint32_t step) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_substrate_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_substra_training_step", progress);

    /* Fetch and apply metabolic modulation for this step */
    if (bridge->substrate) {
        substrate_metabolic_state_t metabolic;
        if (substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
            float atp = metabolic.atp_level;
            float cap = metabolic.metabolic_capacity;
            float min = bridge->config.min_capacity;

            nimcp_platform_mutex_lock(bridge->base.mutex);
            /* Modulate training effects based on metabolic state */
            if (bridge->config.enable_atp_modulation) {
                bridge->effects.prediction_horizon =
                    nimcp_clampf(atp * bridge->config.atp_sensitivity, min, 1.0f);
                bridge->effects.model_precision =
                    nimcp_clampf(atp * 1.05f * bridge->config.atp_sensitivity, min, 1.0f);
            }
            if (bridge->config.enable_fatigue_modulation) {
                bridge->effects.embedding_quality =
                    nimcp_clampf(cap * bridge->config.fatigue_sensitivity, min, 1.0f);
                bridge->effects.update_rate =
                    nimcp_clampf(cap * 0.9f * bridge->config.fatigue_sensitivity, min, 1.0f);
            }

            bridge->effects.overall_capacity =
                (bridge->effects.prediction_horizon + bridge->effects.model_precision +
                 bridge->effects.embedding_quality + bridge->effects.update_rate) / 4.0f;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
        }
    }

    bridge->update_count++;

    jepa_substrate_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_substra_training_step", 1.0f);
    return 0;
}
