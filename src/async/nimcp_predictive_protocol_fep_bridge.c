/**
 * @file nimcp_predictive_protocol_fep_bridge.c
 * @brief Implementation of FEP bridge for predictive protocol
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include "async/nimcp_predictive_protocol_fep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_protocol_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int predictive_protocol_fep_default_config(predictive_protocol_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config != NULL, NIMCP_ERROR_NULL_POINTER,
                       "predictive_protocol_fep_default_config: NULL config");

    config->prediction_confidence_threshold = 0.7f;
    config->pattern_surprise_threshold = 2.0f;
    config->fep_hierarchy_levels = 3;
    config->learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config->enable_pattern_learning = true;
    config->pattern_history_size = 100;
    config->enable_fep_guided_prefetch = true;
    config->prefetch_confidence_boost = 0.2f;
    config->max_prefetch_depth = 3;
    config->enable_cache_feedback = true;
    config->cache_feedback_gain = 1.0f;

    return 0;
}

predictive_protocol_fep_bridge_t* predictive_protocol_fep_create(
    const predictive_protocol_fep_config_t* config,
    fep_system_t* fep_system,
    predictive_protocol_t protocol
) {
    if (!config || !fep_system || !protocol) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_create: NULL parameter");
        NIMCP_LOGGING_ERROR("predictive_protocol_fep_create: NULL parameter");
        return NULL;
    }

    predictive_protocol_fep_bridge_t* bridge =
        (predictive_protocol_fep_bridge_t*)nimcp_malloc(
            sizeof(predictive_protocol_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_protocol_fep_create: Failed to allocate");
        NIMCP_LOGGING_ERROR("predictive_protocol_fep_create: Failed to allocate");
        return NULL;
    }

    memset(bridge, 0, sizeof(predictive_protocol_fep_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(predictive_protocol_fep_config_t));

    /* Connect modules */
    bridge->fep_system = fep_system;
    bridge->protocol = protocol;

    /* Initialize state */
    bridge->state.fep_active = true;
    bridge->state.protocol_connected = true;

    /* Initialize effects */
    bridge->fep_effects.prefetch_confidence = config->prediction_confidence_threshold;
    bridge->fep_effects.exploration_factor = 0.1f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "predictive_protocol_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "predictive_protocol_fep_create: Failed to create mutex");
        NIMCP_LOGGING_ERROR("predictive_protocol_fep_create: Failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created predictive protocol FEP bridge");

    return bridge;
}

void predictive_protocol_fep_destroy(predictive_protocol_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        predictive_protocol_fep_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Integration Implementation
 * ============================================================================ */

int predictive_protocol_fep_update_effects(predictive_protocol_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER,
                       "predictive_protocol_fep_update_effects: NULL bridge or fep_system");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current free energy */
    float free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->stats.avg_free_energy =
        NIMCP_EMA_WEIGHT_SLOW * bridge->stats.avg_free_energy + NIMCP_EMA_WEIGHT_FAST * free_energy;

    /* Low free energy = confident predictions = prefetch */
    bridge->fep_effects.should_prefetch =
        bridge->config.enable_fep_guided_prefetch &&
        (free_energy < 5.0f);

    /* Prefetch confidence from FEP certainty */
    bridge->fep_effects.prefetch_confidence = expf(-free_energy / 5.0f);
    bridge->fep_effects.prefetch_confidence += bridge->config.prefetch_confidence_boost;
    bridge->fep_effects.prefetch_confidence =
        fminf(1.0f, bridge->fep_effects.prefetch_confidence);

    /* Predict cache hit rate based on FEP performance */
    bridge->fep_effects.cache_hit_rate_prediction =
        bridge->fep_effects.prefetch_confidence * 0.8f;

    /* Prefetch urgency based on prediction confidence */
    bridge->fep_effects.prefetch_urgency = bridge->fep_effects.prefetch_confidence;

    /* Adjust exploration based on prediction accuracy */
    if (bridge->protocol_effects.prediction_accuracy > 0.8f) {
        bridge->fep_effects.exploration_factor = 0.05f; /* Exploit */
    } else {
        bridge->fep_effects.exploration_factor = 0.2f;  /* Explore */
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_protocol_fep_observe_prefetch(
    predictive_protocol_fep_bridge_t* bridge,
    bio_message_type_t msg_type,
    bool cache_hit,
    float latency_saved_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_observe_prefetch: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update cache observations */
    if (cache_hit) {
        bridge->protocol_effects.cache_hits++;
        bridge->stats.prefetches_guided_by_fep++;
    } else {
        bridge->protocol_effects.cache_misses++;
    }

    /* Compute hit rate */
    uint64_t total = bridge->protocol_effects.cache_hits +
                     bridge->protocol_effects.cache_misses;
    if (total > 0) {
        bridge->protocol_effects.hit_rate =
            (float)bridge->protocol_effects.cache_hits / (float)total;
    }

    /* Compute hit rate prediction error */
    float predicted_hit_rate = bridge->fep_effects.cache_hit_rate_prediction;
    bridge->protocol_effects.hit_rate_prediction_error =
        bridge->protocol_effects.hit_rate - predicted_hit_rate;

    /* Compute prefetch surprise */
    if (!cache_hit && bridge->fep_effects.should_prefetch) {
        bridge->protocol_effects.unexpected_cache_miss = true;
        bridge->protocol_effects.prefetch_surprise += 1.0f;
    } else {
        bridge->protocol_effects.unexpected_cache_miss = false;
    }

    /* Update statistics */
    bridge->stats.avg_prefetch_hit_rate =
        0.95f * bridge->stats.avg_prefetch_hit_rate +
        0.05f * bridge->protocol_effects.hit_rate;

    if (cache_hit) {
        bridge->stats.accurate_prefetches++;
    }

    /* Update prediction accuracy */
    if (bridge->state.total_predictions > 0) {
        bridge->protocol_effects.prediction_accuracy =
            (float)bridge->state.successful_predictions /
            (float)bridge->state.total_predictions;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_protocol_fep_predict_pattern(
    predictive_protocol_fep_bridge_t* bridge,
    const bio_message_header_t* current_msg,
    bio_message_type_t* predicted_msg,
    float* confidence
) {
    if (!bridge || !current_msg || !predicted_msg || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_predict_pattern: NULL parameter");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Use FEP to predict next message type */
    /* For simplicity, use same type as current (would use FEP hierarchy) */
    *predicted_msg = current_msg->type;
    *confidence = bridge->fep_effects.prefetch_confidence;

    /* Update state */
    bridge->state.active_predictions++;
    bridge->state.total_predictions++;
    bridge->fep_effects.predicted_msg_type = *predicted_msg;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_protocol_fep_learn_pattern(
    predictive_protocol_fep_bridge_t* bridge,
    uint32_t level,
    uint32_t* pattern_id
) {
    if (!bridge || !pattern_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_learn_pattern: NULL bridge or pattern_id");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Extract FEP belief as message pattern */
    /* In full implementation, would extract from FEP hierarchy */

    *pattern_id = bridge->state.learned_patterns;
    bridge->state.learned_patterns++;
    bridge->state.active_patterns++;

    NIMCP_LOGGING_DEBUG("Learned pattern %u from FEP level %u",
                        *pattern_id, level);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int predictive_protocol_fep_connect_bio_async(predictive_protocol_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_connect_bio_async: NULL bridge");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected predictive protocol FEP bridge");

    return 0;
}

int predictive_protocol_fep_disconnect_bio_async(predictive_protocol_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_disconnect_bio_async: NULL bridge");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected predictive protocol FEP bridge");

    return 0;
}

bool predictive_protocol_fep_is_bio_async_connected(
    const predictive_protocol_fep_bridge_t* bridge
) {
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int predictive_protocol_fep_get_effects(
    const predictive_protocol_fep_bridge_t* bridge,
    predictive_protocol_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_get_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->fep_effects, sizeof(predictive_protocol_fep_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_protocol_fep_get_protocol_effects(
    const predictive_protocol_fep_bridge_t* bridge,
    fep_predictive_protocol_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_get_protocol_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->protocol_effects, sizeof(fep_predictive_protocol_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_protocol_fep_get_stats(
    const predictive_protocol_fep_bridge_t* bridge,
    predictive_protocol_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_get_stats: NULL bridge or stats");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(predictive_protocol_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int predictive_protocol_fep_reset_stats(predictive_protocol_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_protocol_fep_reset_stats: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(predictive_protocol_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Predictive_Protocol_FEP_Bridge module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Predictive_Protocol_FEP_Bridge entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int predictive_protocol_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Predictive_Protocol_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Predictive_Protocol_FEP_Bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Predictive_Protocol_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Predictive_Protocol_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
