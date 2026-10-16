/**
 * @file nimcp_emotion_fep_bridge.c
 * @brief Free Energy Principle - Emotion Integration Bridge Implementation
 */

#include "cognitive/emotion/nimcp_emotion_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>

#define LOG_MODULE "emotion_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_fep_bridge_mesh_registry = NULL;

nimcp_error_t emotion_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_fep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_fep_bridge_mesh_registry = registry;
    return err;
}

void emotion_fep_bridge_mesh_unregister(void) {
    if (g_emotion_fep_bridge_mesh_registry && g_emotion_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_fep_bridge_mesh_registry, g_emotion_fep_bridge_mesh_id);
        g_emotion_fep_bridge_mesh_id = 0;
        g_emotion_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_fep_bridge module (instance-level) */
static inline void emotion_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


int emotion_fep_bridge_default_config(emotion_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->pe_valence_scaling = 1.0f;
    config->pe_arousal_scaling = EMOTION_FEP_AROUSAL_PE_SCALING;
    config->precision_intensity_scaling = 1.0f;
    config->enable_pe_emotion_generation = true;
    config->enable_precision_intensity = true;
    config->enable_interoceptive_inference = true;
    config->emotion_precision_modulation = 1.0f;
    config->emotion_learning_modulation = 1.0f;
    config->enable_emotion_precision = true;
    config->enable_emotion_learning = true;
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;
    return 0;
}

emotion_fep_bridge_t* emotion_fep_bridge_create(const emotion_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__create", 0.0f);


    emotion_fep_bridge_t* bridge = nimcp_malloc(sizeof(emotion_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(emotion_fep_bridge_t));
    if (config) bridge->config = *config;
    else emotion_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "emotion_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void emotion_fep_bridge_destroy(emotion_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__destroy", 0.0f);


    if (bridge->base.bio_async_enabled) emotion_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int emotion_fep_bridge_connect_fep(emotion_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_bridge_connect_emotion(emotion_fep_bridge_t* bridge, emotion_recognition_system_t* emotion) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__connect_emotion", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && emotion, NIMCP_ERROR_NULL_POINTER, "bridge or emotion is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->emotion_system = emotion;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_bridge_disconnect(emotion_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->emotion_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_generate_valenced_pe(emotion_fep_bridge_t* bridge, float pe_magnitude) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__emotion_fep_generate", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_emotion_generation) return 0;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.prediction_error_valence = (pe_magnitude > 0) ? 1.0f : -1.0f;
    bridge->fep_effects.prediction_error_arousal = pe_magnitude * bridge->config.pe_arousal_scaling;
    bridge->fep_effects.emotion_generated = true;
    bridge->stats.emotion_generation_events++;
    bridge->state.current_prediction_error = pe_magnitude;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_modulate_precision_by_intensity(emotion_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__emotion_fep_modulate", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_precision_intensity) return 0;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.precision_intensity = bridge->state.current_precision * bridge->config.precision_intensity_scaling;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_apply_emotion_precision_modulation(emotion_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__emotion_fep_apply_em", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_emotion_precision) return 0;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->emotion_effects.emotion_precision_modifier = bridge->config.emotion_precision_modulation;
    bridge->stats.precision_modulation_events++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_apply_emotion_learning_modulation(emotion_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__emotion_fep_apply_em", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_emotion_learning) return 0;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->emotion_effects.emotion_learning_modifier = bridge->config.emotion_learning_modulation;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_bridge_update(emotion_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "emotion_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "emotion_fep_bridge_update");
    emotion_fep_modulate_precision_by_intensity(bridge);
    emotion_fep_apply_emotion_precision_modulation(bridge);
    emotion_fep_apply_emotion_learning_modulation(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int emotion_fep_bridge_get_state(emotion_fep_bridge_t* bridge, emotion_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_bridge_get_stats(emotion_fep_bridge_t* bridge, emotion_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_fep_bridge_connect_bio_async(emotion_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EMOTION_BRIDGE,
        .module_name = "emotion_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int emotion_fep_bridge_disconnect_bio_async(emotion_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool emotion_fep_bridge_is_bio_async_connected(const emotion_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Emotion FEP Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int emotion_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    emotion_fep_bridge_heartbeat("emotion_fep__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_fep_bridge_heartbeat("emotion_fep__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Emotion FEP Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotion_fep_bridge_set_instance_health_agent(emotion_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        /* Instance-level setter - FEP bridge struct is in header (opaque) */
        g_emotion_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotion_fep_bridge_training_begin(emotion_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_fep_bridge_heartbeat_instance(NULL, "emotion_fep_training_begin", 0.0f);
    return 0;
}

int emotion_fep_bridge_training_end(emotion_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_fep_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_fep_bridge_heartbeat_instance(NULL, "emotion_fep_training_end", 1.0f);
    return 0;
}

int emotion_fep_bridge_training_step(emotion_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_fep_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_fep_bridge_heartbeat_instance(NULL, "emotion_fep_training_step", progress);
    return 0;
}

/* ============================================================================
 * Security Integration (BBB)
 * ============================================================================ */
BRIDGE_DEFINE_SECURITY_SETTERS(emotion_fep_bridge)
