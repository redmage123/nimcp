/**
 * @file nimcp_empathetic_response_fep_bridge.c
 * @brief Free Energy Principle - Empathetic Response Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and empathetic response system
 * WHY:  Empathy is simulating others' emotional states via shared generative models
 * HOW:  FEP models others' hidden states; empathy modulates social prediction precision
 */

#include "cognitive/empathetic_response/nimcp_empathetic_response_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "empathetic_response_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(empathetic_response_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_empathetic_response_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_empathetic_response_fep_bridge_mesh_registry = NULL;

nimcp_error_t empathetic_response_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_empathetic_response_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "empathetic_response_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "empathetic_response_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_empathetic_response_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_empathetic_response_fep_bridge_mesh_registry = registry;
    return err;
}

void empathetic_response_fep_bridge_mesh_unregister(void) {
    if (g_empathetic_response_fep_bridge_mesh_registry && g_empathetic_response_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_empathetic_response_fep_bridge_mesh_registry, g_empathetic_response_fep_bridge_mesh_id);
        g_empathetic_response_fep_bridge_mesh_id = 0;
        g_empathetic_response_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from empathetic_response_fep_bridge module (instance-level) */
static inline void empathetic_response_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_empathetic_response_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_empathetic_response_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_empathetic_response_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int empathetic_response_fep_default_config(empathetic_response_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP -> Empathetic Response */
    config->pe_empathy_gain = 1.0f;
    config->precision_response_confidence = 1.0f;
    config->enable_pe_empathy_generation = true;
    config->enable_precision_confidence = true;

    /* Empathetic Response -> FEP */
    config->empathy_precision_modulation = 1.0f;
    config->response_uncertainty_gain = 1.0f;
    config->enable_empathy_precision = true;
    config->enable_response_uncertainty = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

empathetic_response_fep_bridge_t* empathetic_response_fep_create(
    const empathetic_response_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    empathetic_response_fep_bridge_t* bridge = nimcp_malloc(sizeof(empathetic_response_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate empathetic response FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(empathetic_response_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        empathetic_response_fep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "empathetic_response_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "empathetic_response_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize defaults */
    bridge->emotion_effects.intervention_threshold = 0.7f;

    NIMCP_LOGGING_INFO("Created empathetic response FEP bridge");
    return bridge;
}

void empathetic_response_fep_destroy(empathetic_response_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    if (bridge->base.bio_async_enabled) {
        empathetic_response_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed empathetic response FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int empathetic_response_fep_connect_fep(
    empathetic_response_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to empathetic response bridge");
    return 0;
}

int empathetic_response_fep_connect_empathy(
    empathetic_response_fep_bridge_t* bridge,
    empathetic_response_engine_t empathy
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->empathetic_system = empathy;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected empathetic response system to FEP bridge");
    return 0;
}

int empathetic_response_fep_disconnect(empathetic_response_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    memset(&bridge->empathetic_system, 0, sizeof(empathetic_response_engine_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from empathetic response FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Empathetic Response Direction
 * ============================================================================ */

int empathetic_response_fep_infer_user_state(
    empathetic_response_fep_bridge_t* bridge,
    float pe_magnitude
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_empathy_generation) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Prediction error drives inference of user's emotional state
     * High PE from social signals -> infer user distress
     * Social PE = mismatch between expected and observed behavior
     */
    float distress_inference = fabsf(pe_magnitude) * bridge->config.pe_empathy_gain;

    /* Clamp to [0, 1] */
    if (distress_inference > 1.0f) distress_inference = 1.0f;

    bridge->fep_effects.inferred_user_distress = distress_inference;

    /* Confidence increases with PE magnitude (more signal = more confidence) */
    float confidence = 0.5f + (fabsf(pe_magnitude) * bridge->config.precision_response_confidence * 0.5f);
    if (confidence > 1.0f) confidence = 1.0f;
    bridge->fep_effects.response_confidence = confidence;

    /* Activate empathy when distress detected */
    bridge->fep_effects.empathy_active = (distress_inference > 0.3f);

    /* Update state */
    bridge->state.current_prediction_error = pe_magnitude;
    bridge->state.current_empathy_level = distress_inference;

    /* Check for crisis detection */
    if (distress_inference > bridge->emotion_effects.intervention_threshold) {
        bridge->state.crisis_detected = true;
        bridge->stats.crisis_detection_events++;
        NIMCP_LOGGING_WARN("Crisis detected: distress level = %f", distress_inference);
    } else {
        bridge->state.crisis_detected = false;
    }

    /* Update stats */
    bridge->stats.empathy_generation_events++;
    bridge->stats.avg_empathy_level =
        (bridge->stats.avg_empathy_level * 0.9f) + (distress_inference * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Inferred user distress: %f, confidence: %f",
                        distress_inference, confidence);
    return 0;
}

int empathetic_response_fep_modulate_social_precision(
    empathetic_response_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_empathy_precision) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* High empathy level increases precision on social signals */
    float empathy_level = bridge->state.current_empathy_level;
    float social_precision = 1.0f + (empathy_level * bridge->config.empathy_precision_modulation);

    bridge->emotion_effects.social_precision_weight = social_precision;

    /* Response uncertainty decreases with empathy level */
    float uncertainty = 1.0f - (empathy_level * 0.5f);
    if (uncertainty < 0.1f) uncertainty = 0.1f;
    bridge->emotion_effects.response_uncertainty = uncertainty;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Modulated social precision: %f, uncertainty: %f",
                        social_precision, uncertainty);
    return 0;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int empathetic_response_fep_update(
    empathetic_response_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Apply social precision modulation */
    empathetic_response_fep_modulate_social_precision(bridge);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay empathy level over time if not reinforced */
    float decay = 0.995f;
    bridge->state.current_empathy_level *= decay;
    if (bridge->state.current_empathy_level < 0.01f) {
        bridge->state.crisis_detected = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int empathetic_response_fep_get_state(
    const empathetic_response_fep_bridge_t* bridge,
    empathetic_response_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int empathetic_response_fep_get_stats(
    const empathetic_response_fep_bridge_t* bridge,
    empathetic_response_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int empathetic_response_fep_connect_bio_async(
    empathetic_response_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EMPATHETIC_RESPONSE_BRIDGE,
        .module_name = "empathetic_response_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int empathetic_response_fep_disconnect_bio_async(
    empathetic_response_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool empathetic_response_fep_is_bio_async_connected(
    const empathetic_response_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_empathetic_response_", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int empathetic_response_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    empathetic_response_fep_bridge_heartbeat("empathetic_r_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Empathetic_Response_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                empathetic_response_fep_bridge_heartbeat("empathetic_r_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Empathetic_Response_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Empathetic_Response_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

void empathetic_response_fep_bridge_set_instance_health_agent(empathetic_response_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "empathetic_response_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

int empathetic_response_fep_bridge_training_begin(empathetic_response_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "empathetic_response_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    empathetic_response_fep_bridge_heartbeat_instance(bridge->health_agent, "empathetic_response_fep_training_begin", 0.0f);
    return 0;
}

int empathetic_response_fep_bridge_training_end(empathetic_response_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "empathetic_response_fep_bridge_training_end: NULL argument");
        return -1;
    }
    empathetic_response_fep_bridge_heartbeat_instance(bridge->health_agent, "empathetic_response_fep_training_end", 1.0f);
    return 0;
}

int empathetic_response_fep_bridge_training_step(empathetic_response_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "empathetic_response_fep_bridge_training_step: NULL argument");
        return -1;
    }
    empathetic_response_fep_bridge_heartbeat_instance(bridge->health_agent, "empathetic_response_fep_training_step", progress);
    return 0;
}
