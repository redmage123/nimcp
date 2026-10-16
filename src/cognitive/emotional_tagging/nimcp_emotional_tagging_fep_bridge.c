/**
 * @file nimcp_emotional_tagging_fep_bridge.c
 * @brief Free Energy Principle - Emotional Tagging Integration Bridge Implementation
 */

#include "cognitive/emotional_tagging/nimcp_emotional_tagging_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "emotional_tagging_fep_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotional_tagging_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int emotional_tagging_fep_default_config(emotional_tagging_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* FEP → Emotional Tagging */
    config->pe_valence_gain = EMOTIONAL_TAGGING_FEP_PE_VALENCE_SCALING;
    config->precision_arousal_gain = EMOTIONAL_TAGGING_FEP_PRECISION_AROUSAL_SCALING;
    config->surprise_intensity_gain = EMOTIONAL_TAGGING_FEP_SURPRISE_INTENSITY_SCALING;
    config->enable_pe_valence_generation = true;
    config->enable_precision_arousal = true;
    config->enable_surprise_intensity = true;

    /* Emotional Tagging → FEP */
    config->arousal_precision_modulation = 1.0f;
    config->valence_value_modulation = 1.0f;
    config->intensity_encoding_boost = 1.5f;
    config->enable_arousal_precision = true;
    config->enable_valence_value = true;
    config->enable_intensity_encoding = true;

    /* Sensitivity */
    config->fe_sensitivity = 1.0f;
    config->emotion_sensitivity = 1.0f;

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

emotional_tagging_fep_bridge_t* emotional_tagging_fep_create(
    const emotional_tagging_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    emotional_tagging_fep_bridge_t* bridge = nimcp_malloc(sizeof(emotional_tagging_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate emotional tagging FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(emotional_tagging_fep_bridge_t));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        emotional_tagging_fep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "emotional_tagging_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_tagging_fep_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created emotional tagging FEP bridge");
    return bridge;
}

void emotional_tagging_fep_destroy(emotional_tagging_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    if (bridge->base.bio_async_enabled) {
        emotional_tagging_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed emotional tagging FEP bridge");
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int emotional_tagging_fep_connect_fep(
    emotional_tagging_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to emotional tagging bridge");
    return 0;
}

int emotional_tagging_fep_connect_tag(
    emotional_tagging_fep_bridge_t* bridge,
    emotional_tag_t* tag
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && tag, NIMCP_ERROR_NULL_POINTER, "bridge or tag is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_tag = tag;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected emotional tag to FEP bridge");
    return 0;
}

int emotional_tagging_fep_disconnect(emotional_tagging_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->current_tag = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected all systems from emotional tagging FEP bridge");
    return 0;
}

/* ============================================================================
 * FEP → Emotional Tagging Direction
 * ============================================================================ */

int emotional_tagging_fep_generate_pe_valence(
    emotional_tagging_fep_bridge_t* bridge,
    float pe_magnitude
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_valence_generation) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Positive PE = better than expected = positive valence
     * Negative PE = worse than expected = negative valence
     */
    float valence = pe_magnitude * bridge->config.pe_valence_gain;

    /* Clamp to [-1, +1] */
    if (valence > 1.0f) valence = 1.0f;
    if (valence < -1.0f) valence = -1.0f;

    bridge->fep_effects.prediction_error_valence = valence;
    bridge->state.current_valence = valence;
    bridge->state.current_prediction_error = pe_magnitude;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Generated PE valence: %f from PE: %f", valence, pe_magnitude);
    return 0;
}

int emotional_tagging_fep_generate_precision_arousal(
    emotional_tagging_fep_bridge_t* bridge,
    float precision
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_precision_arousal) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* High precision (certainty) or high uncertainty = high arousal
     * Use absolute value to capture both extremes
     */
    float arousal = fabsf(precision) * bridge->config.precision_arousal_gain;

    /* Clamp to [0, 1] */
    if (arousal > 1.0f) arousal = 1.0f;
    if (arousal < 0.0f) arousal = 0.0f;

    bridge->fep_effects.precision_arousal = arousal;
    bridge->state.current_arousal = arousal;
    bridge->state.current_precision = precision;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Generated precision arousal: %f from precision: %f", arousal, precision);
    return 0;
}

int emotional_tagging_fep_generate_surprise_intensity(
    emotional_tagging_fep_bridge_t* bridge,
    float surprise
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_surprise_intensity) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Surprise (free energy) drives emotional intensity */
    float intensity = surprise * bridge->config.surprise_intensity_gain;

    /* Clamp to [0, 1] */
    if (intensity > 1.0f) intensity = 1.0f;
    if (intensity < 0.0f) intensity = 0.0f;

    bridge->fep_effects.surprise_intensity = intensity;
    bridge->state.current_intensity = intensity;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Generated surprise intensity: %f from surprise: %f", intensity, surprise);
    return 0;
}

int emotional_tagging_fep_generate_tag(
    emotional_tagging_fep_bridge_t* bridge,
    uint64_t timestamp_ms
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Create emotional tag from current FEP-derived values */
    emotional_tag_t tag = emotional_tag_create(
        bridge->fep_effects.prediction_error_valence,
        bridge->fep_effects.precision_arousal,
        timestamp_ms
    );

    /* Override intensity with surprise-derived value */
    tag.intensity = bridge->fep_effects.surprise_intensity;

    bridge->fep_effects.generated_tag = tag;
    bridge->fep_effects.tag_generated = true;
    bridge->state.emotion_active = true;
    bridge->state.last_emotion_time = timestamp_ms;

    /* Update stats */
    bridge->stats.emotion_generation_events++;
    bridge->stats.avg_valence =
        (bridge->stats.avg_valence * 0.9f) + (tag.valence * 0.1f);
    bridge->stats.avg_arousal =
        (bridge->stats.avg_arousal * 0.9f) + (tag.arousal * 0.1f);
    bridge->stats.avg_intensity =
        (bridge->stats.avg_intensity * 0.9f) + (tag.intensity * 0.1f);

    /* If connected to a tag, update it */
    if (bridge->current_tag) {
        *bridge->current_tag = tag;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Generated emotional tag: valence=%f, arousal=%f, intensity=%f",
                      tag.valence, tag.arousal, tag.intensity);
    return 0;
}

/* ============================================================================
 * Emotional Tagging → FEP Direction
 * ============================================================================ */

int emotional_tagging_fep_modulate_precision(
    emotional_tagging_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_arousal_precision) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* High arousal increases precision weighting */
    float precision_modifier = 1.0f +
        (bridge->state.current_arousal * bridge->config.arousal_precision_modulation);

    bridge->emotion_effects.precision_modifier = precision_modifier;

    /* Update stats */
    bridge->stats.precision_modulation_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Modulated precision by arousal: %f", precision_modifier);
    return 0;
}

int emotional_tagging_fep_modulate_value(
    emotional_tagging_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_valence_value) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Positive valence increases expected value */
    float value_modifier = 1.0f +
        (bridge->state.current_valence * bridge->config.valence_value_modulation);

    bridge->emotion_effects.value_modifier = value_modifier;

    /* Update stats */
    bridge->stats.value_modulation_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Modulated value by valence: %f", value_modifier);
    return 0;
}

int emotional_tagging_fep_boost_encoding(
    emotional_tagging_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_intensity_encoding) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* High intensity boosts memory encoding */
    float encoding_boost = 1.0f +
        (bridge->state.current_intensity * bridge->config.intensity_encoding_boost);

    bridge->emotion_effects.encoding_boost = encoding_boost;

    /* Compute overall emotional salience */
    float salience = encoding_boost * bridge->emotion_effects.precision_modifier;
    bridge->emotion_effects.emotional_salience = salience;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Boosted encoding by intensity: %f", encoding_boost);
    return 0;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int emotional_tagging_fep_update(
    emotional_tagging_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* FEP → Emotional Tagging (if FEP connected) */
    if (bridge->fep_system) {
        /* These would be called externally with FEP values */
    }

    /* Emotional Tagging → FEP */
    emotional_tagging_fep_modulate_precision(bridge);
    emotional_tagging_fep_modulate_value(bridge);
    emotional_tagging_fep_boost_encoding(bridge);

    /* Update average stats */
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->stats.avg_prediction_error =
        (bridge->stats.avg_prediction_error * 0.9f) +
        (bridge->state.current_prediction_error * 0.1f);

    bridge->stats.avg_precision =
        (bridge->stats.avg_precision * 0.9f) +
        (bridge->state.current_precision * 0.1f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int emotional_tagging_fep_get_state(
    const emotional_tagging_fep_bridge_t* bridge,
    emotional_tagging_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int emotional_tagging_fep_get_stats(
    const emotional_tagging_fep_bridge_t* bridge,
    emotional_tagging_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int emotional_tagging_fep_connect_bio_async(
    emotional_tagging_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EMOTIONAL_TAGGING_BRIDGE,
        .module_name = "emotional_tagging_fep_bridge",
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

int emotional_tagging_fep_disconnect_bio_async(
    emotional_tagging_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


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

bool emotional_tagging_fep_is_bio_async_connected(
    const emotional_tagging_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_emotional_tagging_fe", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_tagging_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_fep_bridge_heartbeat("emotional_ta_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_Tagging_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_tagging_fep_bridge_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_Tagging_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_Tagging_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void emotional_tagging_fep_bridge_set_instance_health_agent(emotional_tagging_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int emotional_tagging_fep_bridge_training_begin(emotional_tagging_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    emotional_tagging_fep_bridge_heartbeat_instance(bridge->health_agent, "etag_fep_training_begin", 0.0f);
    return 0;
}

int emotional_tagging_fep_bridge_training_end(emotional_tagging_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_fep_bridge_training_end: NULL argument");
        return -1;
    }
    emotional_tagging_fep_bridge_heartbeat_instance(bridge->health_agent, "etag_fep_training_end", 1.0f);
    return 0;
}

int emotional_tagging_fep_bridge_training_step(emotional_tagging_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_fep_bridge_training_step: NULL argument");
        return -1;
    }
    emotional_tagging_fep_bridge_heartbeat_instance(bridge->health_agent, "etag_fep_training_step", progress);
    return 0;
}
