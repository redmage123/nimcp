/**
 * @file nimcp_free_energy_substrate_bridge.c
 * @brief Free Energy Principle-Neural Substrate Bridge Implementation
 *
 * WHAT: Links FEP to metabolic state
 * WHY: Variational inference requires sustained computational resources
 * HOW: Monitors ATP/fatigue; modulates precision, depth, active inference
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/free_energy/nimcp_free_energy_substrate_bridge.h"
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
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(free_energy_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_free_energy_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_free_energy_substrate_bridge_mesh_registry = NULL;

nimcp_error_t free_energy_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_free_energy_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "free_energy_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "free_energy_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_free_energy_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_free_energy_substrate_bridge_mesh_registry = registry;
    return err;
}

void free_energy_substrate_bridge_mesh_unregister(void) {
    if (g_free_energy_substrate_bridge_mesh_registry && g_free_energy_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_free_energy_substrate_bridge_mesh_registry, g_free_energy_substrate_bridge_mesh_id);
        g_free_energy_substrate_bridge_mesh_id = 0;
        g_free_energy_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from free_energy_substrate_bridge module (instance-level) */
static inline void free_energy_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_free_energy_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_free_energy_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_free_energy_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct free_energy_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
    void* free_energy;
    neural_substrate_t* substrate;
    free_energy_substrate_config_t config;
    free_energy_substrate_effects_t effects;
    bio_module_context_t ctx;
    bio_router_t* router;
    bool bio_async_connected;
    uint64_t update_count;
    float prev_overall_capacity;
};

free_energy_substrate_config_t free_energy_substrate_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__free_energy_substrat", 0.0f);


    free_energy_substrate_config_t cfg = {
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f
    };
    return cfg;
}

free_energy_substrate_bridge_t* free_energy_substrate_bridge_create(void* free_energy, neural_substrate_t* substrate, const free_energy_substrate_config_t* config) {
    if (!substrate) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__create", 0.0f);


    free_energy_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(free_energy_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->free_energy = free_energy;
    bridge->substrate = substrate;
    bridge->config = config ? *config : free_energy_substrate_default_config();

    bridge->effects.precision_weighting = 1.0f;
    bridge->effects.prediction_depth = 1.0f;
    bridge->effects.active_inference = 1.0f;
    bridge->effects.model_complexity = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    if (bridge_base_init(&bridge->base, 0, "free_energy_substrate") != 0) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "free_energy_substrate");
    return bridge;
}

void free_energy_substrate_bridge_destroy(free_energy_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__destroy", 0.0f);


    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int free_energy_substrate_bridge_update(free_energy_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_substrate_bridge_update: required parameter is NULL (bridge, bridge->substrate)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__update", 0.0f);


    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "free_energy_substrate_bridge_update: validation failed");
        return -1;
    }

    float atp = metabolic.atp_level;
    float metabolic_cap = metabolic.metabolic_capacity;
    float min_cap = bridge->config.min_capacity;

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_atp_modulation) {
        /* Precision weighting requires stable neural resources */
        bridge->effects.precision_weighting = nimcp_clampf(atp * bridge->config.atp_sensitivity, min_cap, 1.0f);
        /* Active inference is computationally demanding */
        bridge->effects.active_inference = nimcp_clampf(atp * 0.95f * bridge->config.atp_sensitivity, min_cap, 1.0f);
    }

    if (bridge->config.enable_fatigue_modulation) {
        /* Prediction depth decreases with fatigue */
        bridge->effects.prediction_depth = nimcp_clampf(metabolic_cap * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
        /* Model complexity simplifies under metabolic stress */
        bridge->effects.model_complexity = nimcp_clampf(metabolic_cap * 0.85f * bridge->config.fatigue_sensitivity, min_cap, 1.0f);
    }

    bridge->effects.overall_capacity = (bridge->effects.precision_weighting +
                                        bridge->effects.prediction_depth +
                                        bridge->effects.active_inference +
                                        bridge->effects.model_complexity) / 4.0f;

    bridge->update_count++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int free_energy_substrate_bridge_get_effects(const free_energy_substrate_bridge_t* bridge, free_energy_substrate_effects_t* effects) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__get_effects", 0.0f);


    return 0;
}

int free_energy_substrate_bridge_apply_effects(free_energy_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_substrate_bridge_apply_effects: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected || !bridge->ctx) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__apply_effects", 0.0f);


    substrate_metabolic_state_t metabolic;
    float atp_level = 1.0f, fatigue_level = 0.0f;
    if (bridge->substrate && substrate_get_metabolic_state(bridge->substrate, &metabolic) == 0) {
        atp_level = metabolic.atp_level;
        fatigue_level = 1.0f - metabolic.metabolic_capacity;
    }

    bio_msg_substrate_modulation_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_SUBSTRATE_MODULATION,
                        BIO_MODULE_SUBSTRATE_FREE_ENERGY, 0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;  /* FEP uses dopamine for precision weighting */

    msg.bridge_module_id = BIO_MODULE_SUBSTRATE_FREE_ENERGY;
    msg.processing_capacity = bridge->effects.precision_weighting;
    msg.overall_capacity = bridge->effects.overall_capacity;
    msg.effect_values[0] = bridge->effects.precision_weighting;
    msg.effect_values[1] = bridge->effects.prediction_depth;
    msg.effect_values[2] = bridge->effects.active_inference;
    msg.effect_values[3] = bridge->effects.model_complexity;
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
                            BIO_MODULE_SUBSTRATE_FREE_ENERGY, 0, sizeof(update_msg));
        update_msg.bridge_module_id = BIO_MODULE_SUBSTRATE_FREE_ENERGY;
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
                            BIO_MODULE_SUBSTRATE_FREE_ENERGY, 0, sizeof(alert));
        alert.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
        alert.bridge_module_id = BIO_MODULE_SUBSTRATE_FREE_ENERGY;
        alert.atp_level = atp_level;
        alert.threshold = bridge->config.min_capacity;
        alert.min_capacity = bridge->config.min_capacity;
        bio_router_broadcast(bridge->ctx, &alert, sizeof(alert));
    }

    bridge->prev_overall_capacity = bridge->effects.overall_capacity;
    return 0;
}

int free_energy_substrate_bridge_register_bio_async(free_energy_substrate_bridge_t* bridge, bio_router_t* router) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_substrate_bridge_register_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__register_bio_async", 0.0f);


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
        .module_id = BIO_MODULE_SUBSTRATE_FREE_ENERGY,
        .module_name = "free_energy_substrate_bridge",
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

/**
 * WHAT: Query knowledge graph for Free Energy Substrate Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int free_energy_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    free_energy_substrate_bridge_heartbeat("free_energy__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Free_Energy_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                free_energy_substrate_bridge_heartbeat("free_energy__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FE Substrate Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Free_Energy_Substrate_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Free_Energy_Substrate_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void free_energy_substrate_bridge_set_instance_health_agent(free_energy_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int free_energy_substrate_bridge_training_begin(free_energy_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "free_energy_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    free_energy_substrate_bridge_heartbeat_instance(bridge->health_agent, "fe_sub_training_begin", 0.0f);
    struct free_energy_substrate_bridge* b = (struct free_energy_substrate_bridge*)bridge;
    b->update_count = 0;
    b->effects.precision_weighting = (b->effects.precision_weighting > 0.0f) ? b->effects.precision_weighting : 1.0f;
    b->effects.prediction_depth = (b->effects.prediction_depth > 0.0f) ? b->effects.prediction_depth : 1.0f;
    NIMCP_LOGGING_INFO("free_energy_substrate_bridge: training begun, counters reset");
    return 0;
}

int free_energy_substrate_bridge_training_step(free_energy_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "free_energy_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    free_energy_substrate_bridge_heartbeat_instance(bridge->health_agent, "fe_sub_training_step", clamped);
    struct free_energy_substrate_bridge* b = (struct free_energy_substrate_bridge*)bridge;
    float p = clamped;
    nimcp_platform_mutex_lock(b->base.mutex);
    b->effects.precision_weighting += (1.0f - p) * 0.001f;
    if (b->effects.precision_weighting > 2.0f) b->effects.precision_weighting = 2.0f;
    if (b->effects.precision_weighting < 0.0f) b->effects.precision_weighting = 0.0f;
    b->effects.prediction_depth += (1.0f - p) * 0.001f;
    if (b->effects.prediction_depth > 2.0f) b->effects.prediction_depth = 2.0f;
    if (b->effects.prediction_depth < 0.0f) b->effects.prediction_depth = 0.0f;
    b->effects.active_inference += (1.0f - p) * 0.001f;
    if (b->effects.active_inference > 2.0f) b->effects.active_inference = 2.0f;
    if (b->effects.active_inference < 0.0f) b->effects.active_inference = 0.0f;
    b->effects.model_complexity += (1.0f - p) * 0.001f;
    if (b->effects.model_complexity > 2.0f) b->effects.model_complexity = 2.0f;
    if (b->effects.model_complexity < 0.0f) b->effects.model_complexity = 0.0f;
    b->effects.overall_capacity += (1.0f - p) * 0.001f;
    if (b->effects.overall_capacity > 2.0f) b->effects.overall_capacity = 2.0f;
    if (b->effects.overall_capacity < 0.0f) b->effects.overall_capacity = 0.0f;
    b->update_count++;
    nimcp_platform_mutex_unlock(b->base.mutex);
    return 0;
}

int free_energy_substrate_bridge_training_end(free_energy_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "free_energy_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    free_energy_substrate_bridge_heartbeat_instance(bridge->health_agent, "fe_sub_training_end", 1.0f);
    struct free_energy_substrate_bridge* b = (struct free_energy_substrate_bridge*)bridge;
    float metric_sum = 0.0f;
    metric_sum += b->effects.precision_weighting;
    metric_sum += b->effects.prediction_depth;
    metric_sum += b->effects.active_inference;
    metric_sum += b->effects.model_complexity;
    metric_sum += b->effects.overall_capacity;
    float avg_metric = metric_sum / 5.0f;
    NIMCP_LOGGING_INFO("free_energy_substrate_bridge: training complete, avg_metric=%.4f", avg_metric);
    return 0;
}
