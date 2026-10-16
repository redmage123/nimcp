/**
 * @file nimcp_shadow_emotions_fep_bridge.c
 * @brief Shadow Emotions FEP Bridge Implementation
 */

#include "cognitive/shadow_emotions/nimcp_shadow_emotions_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#define LOG_MODULE "shadow_emotions_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(shadow_emotions_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from shadow_emotions_fep_bridge module (instance-level) */
static inline void shadow_emotions_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_shadow_emotions_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_shadow_emotions_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_shadow_emotions_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/** Instance-level health agent for opaque struct (static fallback) */


int shadow_emotions_fep_bridge_default_config(shadow_emotions_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->dysregulation_threshold = SHADOW_FEP_DYSREGULATION_THRESHOLD;
    config->precision_calibration_strength = SHADOW_FEP_PRECISION_CALIBRATION;
    config->intervention_effectiveness = SHADOW_FEP_INTERVENTION_STRENGTH;
    config->enable_pe_calibration = true;
    config->enable_precision_interventions = true;
    config->enable_self_correction = true;
    config->jealousy_pe_sensitivity = 1.5f;
    config->hubris_pe_insensitivity = 0.5f;
    config->enable_diagnostic_mode = true;
    config->enable_restoration_mode = true;
    config->fe_sensitivity = 1.0f;
    config->shadow_sensitivity = 1.0f;
    return 0;
}

shadow_emotions_fep_bridge_t* shadow_emotions_fep_bridge_create(const shadow_emotions_fep_config_t* config) {
    shadow_emotions_fep_bridge_t* bridge = nimcp_malloc(sizeof(shadow_emotions_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(shadow_emotions_fep_bridge_t));
    if (config) bridge->config = *config;
    else shadow_emotions_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "shadow_emotions_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void shadow_emotions_fep_bridge_destroy(shadow_emotions_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) shadow_emotions_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int shadow_emotions_fep_bridge_connect_fep(shadow_emotions_fep_bridge_t* bridge, fep_system_t* fep) {
    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_bridge_connect_shadow(shadow_emotions_fep_bridge_t* bridge, shadow_emotion_system_t* shadow) {
    NIMCP_FEP_CHECK_THROW(bridge && shadow, NIMCP_ERROR_NULL_POINTER, "bridge or shadow is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->shadow_system = shadow;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_bridge_disconnect(shadow_emotions_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->shadow_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_detect_dysregulation(shadow_emotions_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    float dysregulation_score = bridge->state.current_dysregulation;
    bool detected = (dysregulation_score > 0.5f);
    bridge->shadow_effects.shadow_pattern_detected = detected;
    if (detected) {
        bridge->stats.pattern_detections++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_trigger_intervention(shadow_emotions_fep_bridge_t* bridge, shadow_emotion_type_t emotion) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_precision_interventions) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.intervention_triggered = true;
    bridge->state.intervention_active = true;
    bridge->state.target_emotion = emotion;
    bridge->stats.intervention_events++;
    NIMCP_LOGGING_INFO("Shadow emotion intervention triggered");
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_recalibrate_precision(shadow_emotions_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_calibration) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.recalibration_active = true;
    bridge->stats.calibration_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_apply_shadow_diagnostic(shadow_emotions_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_diagnostic_mode) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->shadow_effects.fep_diagnostic_active = true;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_update_beliefs_from_correction(shadow_emotions_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_self_correction) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.successful_corrections++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_bridge_update(shadow_emotions_fep_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    (void)delta_ms;
    shadow_emotions_fep_detect_dysregulation(bridge);
    shadow_emotions_fep_recalibrate_precision(bridge);
    if (bridge->fep_system) {
        shadow_emotions_fep_apply_shadow_diagnostic(bridge);
    }
    return 0;
}

int shadow_emotions_fep_bridge_get_state(shadow_emotions_fep_bridge_t* bridge, shadow_emotions_fep_state_t* state) {
    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_bridge_get_stats(shadow_emotions_fep_bridge_t* bridge, shadow_emotions_fep_stats_t* stats) {
    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_fep_bridge_connect_bio_async(shadow_emotions_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SHADOW_BRIDGE,
        .module_name = "shadow_emotions_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }
    return 0;
}

int shadow_emotions_fep_bridge_disconnect_bio_async(shadow_emotions_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool shadow_emotions_fep_bridge_is_bio_async_connected(const shadow_emotions_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int shadow_emotions_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Shadow_Emotions_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Shadow_Emotions_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Shadow_Emotions_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

static nimcp_health_agent_t* g_shadow_emotions_fep_bridge_instance_health_agent = NULL;

void shadow_emotions_fep_bridge_set_instance_health_agent(nimcp_health_agent_t* agent) {
    g_shadow_emotions_fep_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int shadow_emotions_fep_bridge_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_fep_bridge_training_begin: ctx is NULL");
        return -1;
    }
    shadow_emotions_fep_bridge_heartbeat_instance(g_shadow_emotions_fep_bridge_instance_health_agent, "shadow_fep_training_begin", 0.0f);
    return 0;
}

int shadow_emotions_fep_bridge_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_fep_bridge_training_end: ctx is NULL");
        return -1;
    }
    shadow_emotions_fep_bridge_heartbeat_instance(g_shadow_emotions_fep_bridge_instance_health_agent, "shadow_fep_training_end", 1.0f);
    return 0;
}

int shadow_emotions_fep_bridge_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_fep_bridge_training_step: ctx is NULL");
        return -1;
    }
    shadow_emotions_fep_bridge_heartbeat_instance(g_shadow_emotions_fep_bridge_instance_health_agent, "shadow_fep_training_step", progress);
    return 0;
}
