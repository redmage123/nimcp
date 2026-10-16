/**
 * @file nimcp_emotion_recognition_fep_bridge.c
 * @brief Free Energy Principle - Emotion Recognition Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and multimodal emotion recognition
 * WHY:  Emotion recognition is inferring hidden emotional causes from observations
 * HOW:  FEP provides precision-weighted emotional inference; emotions modulate sensory precision
 */

#include "cognitive/emotion_recognition/nimcp_emotion_recognition_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "emotion_recognition_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_recognition_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_recognition_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_recognition_fep_bridge_mesh_registry = NULL;

nimcp_error_t emotion_recognition_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_emotion_recognition_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_recognition_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_recognition_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_recognition_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_recognition_fep_bridge_mesh_registry = registry;
    return err;
}

void emotion_recognition_fep_bridge_mesh_unregister(void) {
    if (g_emotion_recognition_fep_bridge_mesh_registry && g_emotion_recognition_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_recognition_fep_bridge_mesh_registry, g_emotion_recognition_fep_bridge_mesh_id);
        g_emotion_recognition_fep_bridge_mesh_id = 0;
        g_emotion_recognition_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_recognition_fep_bridge module (instance-level) */
static inline void emotion_recognition_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_recognition_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_recognition_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_recognition_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int emotion_recognition_fep_default_config(emotion_recognition_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP -> Emotion Recognition */
    config->pe_inference_gain = 1.0f;
    config->precision_confidence_gain = 1.0f;
    config->enable_pe_emotion_inference = true;
    config->enable_precision_confidence = true;

    /* Emotion Recognition -> FEP */
    config->emotion_precision_modulation = 1.0f;
    config->distress_uncertainty_gain = 1.0f;
    config->enable_emotion_precision = true;
    config->enable_distress_uncertainty = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

emotion_recognition_fep_bridge_t* emotion_recognition_fep_create(
    const emotion_recognition_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    emotion_recognition_fep_bridge_t* bridge = nimcp_malloc(sizeof(emotion_recognition_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate emotion recognition FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(emotion_recognition_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        emotion_recognition_fep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "emotion_recognition_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotion_recognition_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize modality precision weights */
    for (int i = 0; i < 4; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 4 > 256) {
            emotion_recognition_fep_bridge_heartbeat("emotion_reco_loop",
                             (float)(i + 1) / (float)4);
        }

        bridge->emotion_effects.modality_precision_weights[i] = 1.0f;
    }

    NIMCP_LOGGING_INFO("Created emotion recognition FEP bridge");
    return bridge;
}

void emotion_recognition_fep_destroy(emotion_recognition_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    if (bridge->base.bio_async_enabled) {
        emotion_recognition_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed emotion recognition FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int emotion_recognition_fep_connect_fep(
    emotion_recognition_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to emotion recognition bridge");
    return 0;
}

int emotion_recognition_fep_connect_emotion(
    emotion_recognition_fep_bridge_t* bridge,
    emotion_recognition_system_t* emotion
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && emotion, NIMCP_ERROR_NULL_POINTER, "bridge or emotion is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->emotion_system = emotion;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected emotion recognition system to FEP bridge");
    return 0;
}

int emotion_recognition_fep_disconnect(emotion_recognition_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->emotion_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from emotion recognition FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Emotion Recognition Direction
 * ============================================================================ */

int emotion_recognition_fep_infer_emotion(
    emotion_recognition_fep_bridge_t* bridge,
    float pe_magnitude
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_emotion_inference) return 0;

    /* Guard: NaN/Inf pe_magnitude would poison EMA averages in stats */
    if (!isfinite(pe_magnitude)) {
        NIMCP_LOGGING_WARN("NaN/Inf pe_magnitude rejected in emotion_recognition_fep_infer_emotion");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Prediction error drives emotional state inference
     * High PE -> more intense emotional inference
     * Direction of PE determines valence inference
     */
    float inference_strength = fabsf(pe_magnitude) * bridge->config.pe_inference_gain;
    float inferred_valence = pe_magnitude * bridge->config.pe_inference_gain;

    /* Clamp values */
    if (inference_strength > 1.0f) inference_strength = 1.0f;
    if (inferred_valence > 1.0f) inferred_valence = 1.0f;
    if (inferred_valence < -1.0f) inferred_valence = -1.0f;

    /* Compute arousal from inference strength */
    float inferred_arousal = inference_strength;

    /* Update effects */
    bridge->fep_effects.inferred_valence = inferred_valence;
    bridge->fep_effects.inferred_arousal = inferred_arousal;
    bridge->fep_effects.inference_confidence = inference_strength;
    bridge->fep_effects.inference_active = true;

    /* Categorize emotion based on valence/arousal */
    if (inferred_valence > 0.3f && inferred_arousal > 0.5f) {
        bridge->fep_effects.inferred_emotion = EMOTION_HAPPINESS;
    } else if (inferred_valence < -0.3f && inferred_arousal > 0.5f) {
        bridge->fep_effects.inferred_emotion = EMOTION_ANGER;
    } else if (inferred_valence < -0.3f && inferred_arousal < 0.5f) {
        bridge->fep_effects.inferred_emotion = EMOTION_SADNESS;
    } else if (inferred_valence > 0.3f && inferred_arousal < 0.5f) {
        bridge->fep_effects.inferred_emotion = EMOTION_CALM;
    } else {
        bridge->fep_effects.inferred_emotion = EMOTION_NEUTRAL;
    }

    /* Update state */
    bridge->state.current_prediction_error = pe_magnitude;
    bridge->state.current_inferred_emotion = bridge->fep_effects.inferred_emotion;
    bridge->state.inference_confidence = inference_strength;
    bridge->state.recognition_active = true;

    /* Update stats */
    bridge->stats.emotion_inference_events++;
    bridge->stats.avg_inference_confidence =
        (bridge->stats.avg_inference_confidence * 0.9f) + (inference_strength * 0.1f);
    bridge->stats.avg_prediction_error =
        (bridge->stats.avg_prediction_error * 0.9f) + (fabsf(pe_magnitude) * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Inferred emotion from PE: valence=%f, arousal=%f, confidence=%f",
                        inferred_valence, inferred_arousal, inference_strength);
    return 0;
}

int emotion_recognition_fep_modulate_modality_precision(
    emotion_recognition_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_precision_confidence) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Modulate precision weights for each modality based on confidence */
    float confidence = bridge->state.inference_confidence;
    float precision_gain = bridge->config.precision_confidence_gain;

    /* Facial modality */
    bridge->emotion_effects.modality_precision_weights[0] =
        1.0f + (confidence * precision_gain * 0.3f);

    /* Vocal modality */
    bridge->emotion_effects.modality_precision_weights[1] =
        1.0f + (confidence * precision_gain * 0.25f);

    /* Text modality */
    bridge->emotion_effects.modality_precision_weights[2] =
        1.0f + (confidence * precision_gain * 0.2f);

    /* Physiological modality */
    bridge->emotion_effects.modality_precision_weights[3] =
        1.0f + (confidence * precision_gain * 0.25f);

    /* Compute uncertainty estimate */
    bridge->emotion_effects.uncertainty_estimate = 1.0f - confidence;

    /* Active sensing drive increases with uncertainty */
    bridge->emotion_effects.active_sensing_drive =
        bridge->emotion_effects.uncertainty_estimate * bridge->config.distress_uncertainty_gain;

    /* Update stats */
    bridge->stats.precision_modulation_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Modulated modality precisions: facial=%f, vocal=%f, text=%f, physio=%f",
                        bridge->emotion_effects.modality_precision_weights[0],
                        bridge->emotion_effects.modality_precision_weights[1],
                        bridge->emotion_effects.modality_precision_weights[2],
                        bridge->emotion_effects.modality_precision_weights[3]);
    return 0;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int emotion_recognition_fep_update(
    emotion_recognition_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update modality precision modulation */
    emotion_recognition_fep_modulate_modality_precision(bridge);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update timestamp */
    bridge->state.last_recognition_time += delta_ms;

    /* Decay inference confidence over time */
    float decay = NIMCP_EMA_DECAY_DEFAULT;
    bridge->state.inference_confidence *= decay;
    if (bridge->state.inference_confidence < 0.01f) {
        bridge->state.recognition_active = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int emotion_recognition_fep_get_state(
    const emotion_recognition_fep_bridge_t* bridge,
    emotion_recognition_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotion_recognition_fep_get_stats(
    const emotion_recognition_fep_bridge_t* bridge,
    emotion_recognition_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int emotion_recognition_fep_connect_bio_async(
    emotion_recognition_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EMOTION_RECOGNITION_BRIDGE,
        .module_name = "emotion_recognition_fep_bridge",
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

int emotion_recognition_fep_disconnect_bio_async(
    emotion_recognition_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


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

bool emotion_recognition_fep_is_bio_async_connected(
    const emotion_recognition_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_recognition_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_fep_bridge_heartbeat("emotion_reco_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Recognition_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_recognition_fep_bridge_heartbeat("emotion_reco_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Recognition_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Recognition_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Stubs
 * ============================================================================ */

void emotion_recognition_fep_bridge_set_instance_health_agent(emotion_recognition_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int emotion_recognition_fep_bridge_training_begin(emotion_recognition_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_recognition_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_recognition_fep_bridge_heartbeat_instance(bridge->health_agent, "erec_fep_training_begin", 0.0f);
    return 0;
}

int emotion_recognition_fep_bridge_training_end(emotion_recognition_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_recognition_fep_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_recognition_fep_bridge_heartbeat_instance(bridge->health_agent, "erec_fep_training_end", 1.0f);
    return 0;
}

int emotion_recognition_fep_bridge_training_step(emotion_recognition_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_recognition_fep_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_recognition_fep_bridge_heartbeat_instance(bridge->health_agent, "erec_fep_training_step", progress);
    return 0;
}
