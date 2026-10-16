/**
 * @file nimcp_emotional_system_fep_bridge.c
 * @brief Free Energy Principle - Emotional System Integration Bridge Implementation
 *
 * WHAT: Implements bidirectional integration between FEP and unified emotional system
 * WHY:  Emotions are interoceptive inferences about bodily states - the emotional system
 *       integrates all emotional subsystems under active inference framework
 * HOW:  FEP drives emotional state updates; emotions modulate precision and learning
 *
 * BIOLOGICAL BASIS:
 * - Barrett's theory of constructed emotion: Emotions are predictions about body states
 * - Interoceptive inference: Brain predicts and explains bodily sensations
 * - Emotional system integrates tagging, recognition, regulation under FEP
 */

#include "cognitive/nimcp_emotional_system_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "emotional_system_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(emotional_system_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from emotional_system_fep_bridge module (instance-level) */
static inline void emotional_system_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotional_system_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotional_system_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotional_system_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int emotional_system_fep_default_config(emotional_system_fep_config_t* config) {
    if (!config) return -1;

    /* FEP -> Emotional System */
    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    config->pe_emotional_state_gain = 1.0f;
    config->precision_regulation_threshold = 0.7f;
    config->enable_pe_emotional_update = true;
    config->enable_precision_regulation = true;

    /* Emotional System -> FEP */
    config->emotional_state_precision_gain = 1.0f;
    config->valence_value_modulation = 1.0f;
    config->enable_emotional_precision = true;
    config->enable_valence_value = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

emotional_system_fep_bridge_t* emotional_system_fep_create(
    const emotional_system_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    emotional_system_fep_bridge_t* bridge = nimcp_malloc(sizeof(emotional_system_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate emotional system FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(emotional_system_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        emotional_system_fep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "emotional_system_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_system_fep_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize defaults */
    bridge->emotion_effects.precision_weight = 1.0f;
    bridge->emotion_effects.value_estimate = 0.0f;
    bridge->emotion_effects.learning_rate_modifier = 1.0f;

    NIMCP_LOGGING_INFO("Created emotional system FEP bridge");
    return bridge;
}

void emotional_system_fep_destroy(emotional_system_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    if (bridge->base.bio_async_enabled) {
        emotional_system_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed emotional system FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int emotional_system_fep_connect_fep(
    emotional_system_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    if (!bridge || !fep) return -1;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to emotional system bridge");
    return 0;
}

int emotional_system_fep_connect_emotional_system(
    emotional_system_fep_bridge_t* bridge,
    emotional_system_t* system
) {
    if (!bridge || !system) return -1;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->emotional_system = system;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected emotional system to FEP bridge");
    return 0;
}

int emotional_system_fep_disconnect(emotional_system_fep_bridge_t* bridge) {
    if (!bridge) return -1;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->emotional_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from emotional system FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP -> Emotional System Direction
 * ============================================================================ */

int emotional_system_fep_update_from_pe(
    emotional_system_fep_bridge_t* bridge,
    float pe_magnitude
) {
    if (!bridge) return -1;
    if (!bridge->config.enable_pe_emotional_update) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Prediction error drives emotional state updates
     * Under Barrett's theory, emotions are constructed from interoceptive predictions
     * PE magnitude drives arousal; PE sign drives valence
     */
    float valence_from_pe = pe_magnitude * bridge->config.pe_emotional_state_gain;
    float arousal_from_pe = fabsf(pe_magnitude) * bridge->config.pe_emotional_state_gain;

    /* Clamp values */
    if (valence_from_pe > 1.0f) valence_from_pe = 1.0f;
    if (valence_from_pe < -1.0f) valence_from_pe = -1.0f;
    if (arousal_from_pe > 1.0f) arousal_from_pe = 1.0f;

    bridge->fep_effects.valence_from_pe = valence_from_pe;
    bridge->fep_effects.arousal_from_precision = arousal_from_pe;

    /* Check for regulation trigger */
    if (fabsf(pe_magnitude) > bridge->config.precision_regulation_threshold) {
        bridge->fep_effects.regulation_triggered = true;
        bridge->stats.regulation_events++;
        NIMCP_LOGGING_INFO("Emotion regulation triggered: PE magnitude = %f", pe_magnitude);
    } else {
        bridge->fep_effects.regulation_triggered = false;
    }

    /* Update state */
    bridge->state.current_valence = valence_from_pe;
    bridge->state.current_arousal = arousal_from_pe;
    bridge->state.emotion_active = (arousal_from_pe > 0.2f);

    /* Update stats */
    bridge->stats.emotional_update_events++;
    bridge->stats.avg_valence =
        (bridge->stats.avg_valence * 0.9f) + (valence_from_pe * 0.1f);
    bridge->stats.avg_arousal =
        (bridge->stats.avg_arousal * 0.9f) + (arousal_from_pe * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Updated emotional state from PE: valence=%f, arousal=%f",
                        valence_from_pe, arousal_from_pe);
    return 0;
}

int emotional_system_fep_modulate_precision(
    emotional_system_fep_bridge_t* bridge
) {
    if (!bridge) return -1;
    if (!bridge->config.enable_emotional_precision) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Emotional state modulates precision weighting
     * High arousal -> high precision (attention increase)
     * Valence modulates value estimates
     */
    float arousal = bridge->state.current_arousal;
    float valence = bridge->state.current_valence;

    /* Precision weight increases with arousal */
    float precision_weight = 1.0f + (arousal * bridge->config.emotional_state_precision_gain);
    bridge->emotion_effects.precision_weight = precision_weight;

    /* Value estimate from valence */
    float value_estimate = valence * bridge->config.valence_value_modulation;
    bridge->emotion_effects.value_estimate = value_estimate;

    /* Learning rate modulated by emotional intensity */
    float emotional_intensity = sqrtf(arousal * arousal + valence * valence);
    if (emotional_intensity > 1.0f) emotional_intensity = 1.0f;
    float learning_modifier = 1.0f + (emotional_intensity * 0.5f);
    bridge->emotion_effects.learning_rate_modifier = learning_modifier;

    /* Update precision in state */
    bridge->state.current_precision = precision_weight;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Modulated precision: weight=%f, value=%f, learning=%f",
                        precision_weight, value_estimate, learning_modifier);
    return 0;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int emotional_system_fep_update(
    emotional_system_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) return -1;

    /* Modulate precision based on current emotional state */
    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    emotional_system_fep_modulate_precision(bridge);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Emotional states decay towards neutral over time */
    float decay = 0.995f;
    bridge->state.current_valence *= decay;
    bridge->state.current_arousal *= decay;

    if (bridge->state.current_arousal < 0.1f) {
        bridge->state.emotion_active = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int emotional_system_fep_get_state(
    const emotional_system_fep_bridge_t* bridge,
    emotional_system_fep_state_t* state
) {
    if (!bridge || !state) return -1;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotional_system_fep_get_stats(
    const emotional_system_fep_bridge_t* bridge,
    emotional_system_fep_stats_t* stats
) {
    if (!bridge || !stats) return -1;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int emotional_system_fep_connect_bio_async(
    emotional_system_fep_bridge_t* bridge
) {
    if (!bridge) return -1;
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EMOTIONS_BRIDGE,
        .module_name = "emotional_system_fep_bridge",
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

int emotional_system_fep_disconnect_bio_async(
    emotional_system_fep_bridge_t* bridge
) {
    if (!bridge) return -1;
    if (!bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool emotional_system_fep_is_bio_async_connected(
    const emotional_system_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_system_fep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_system_fep_bridge_heartbeat("emotional_sy_emotional_system_fep", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_System_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_system_fep_bridge_heartbeat("emotional_sy_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_System_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_System_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void emotional_system_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_emotional_system_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int emotional_system_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    emotional_system_fep_bridge_heartbeat_instance(NULL, "emotional_system_fep_bridge_training_begin", 0.0f);
    return 0;
}

int emotional_system_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_fep_bridge_training_end: NULL argument");
        return -1;
    }
    emotional_system_fep_bridge_heartbeat_instance(NULL, "emotional_system_fep_bridge_training_end", 1.0f);
    return 0;
}

int emotional_system_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_system_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    emotional_system_fep_bridge_heartbeat_instance(NULL, "emotional_system_fep_bridge_training_step", progress);
    return 0;
}
