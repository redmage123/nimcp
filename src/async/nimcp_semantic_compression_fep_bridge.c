/**
 * @file nimcp_semantic_compression_fep_bridge.c
 * @brief Implementation of FEP bridge for semantic compression
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include "async/nimcp_semantic_compression_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(semantic_compression_fep_bridge)

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int semantic_compression_fep_default_config(semantic_compression_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_default_config: NULL config");
        return -1;
    }

    config->prediction_threshold = 0.7f;
    config->semantic_surprise_threshold = 2.0f;
    config->primitive_learning_iterations = 10;
    config->enable_predictive_compression = true;
    config->enable_semantic_preservation = true;
    config->quality_vs_compression_tradeoff = 0.7f; /* Favor quality */
    config->learning_rate = NIMCP_LEARNING_RATE_COARSE;
    config->enable_primitive_learning = true;
    config->max_primitives = 256;
    config->enable_error_feedback = true;
    config->error_feedback_gain = 1.0f;

    return 0;
}

semantic_compression_fep_bridge_t* semantic_compression_fep_create(
    const semantic_compression_fep_config_t* config,
    fep_system_t* fep_system,
    nimcp_semantic_compressor_t* compressor
) {
    if (!config || !fep_system || !compressor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_create: NULL parameter");
        NIMCP_LOGGING_ERROR("semantic_compression_fep_create: NULL parameter");
        return NULL;
    }

    semantic_compression_fep_bridge_t* bridge =
        (semantic_compression_fep_bridge_t*)nimcp_malloc(
            sizeof(semantic_compression_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "semantic_compression_fep_create: Failed to allocate");
        NIMCP_LOGGING_ERROR("semantic_compression_fep_create: Failed to allocate");
        return NULL;
    }

    memset(bridge, 0, sizeof(semantic_compression_fep_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(semantic_compression_fep_config_t));

    /* Connect modules */
    bridge->fep_system = fep_system;
    bridge->compressor = compressor;

    /* Initialize state */
    bridge->state.fep_active = true;
    bridge->state.compressor_connected = true;

    /* Initialize effects */
    bridge->fep_effects.compression_confidence = config->prediction_threshold;
    bridge->fep_effects.preserve_semantic_structure = config->enable_semantic_preservation;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "semantic_compression_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "semantic_compression_fep_create: Failed to create mutex");
        NIMCP_LOGGING_ERROR("semantic_compression_fep_create: Failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created semantic compression FEP bridge");

    return bridge;
}

void semantic_compression_fep_destroy(semantic_compression_fep_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        semantic_compression_fep_disconnect_bio_async(bridge);
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

int semantic_compression_fep_update_effects(semantic_compression_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_update_effects: NULL bridge or fep_system");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current free energy */
    float free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->stats.avg_free_energy =
        NIMCP_EMA_WEIGHT_SLOW * bridge->stats.avg_free_energy + NIMCP_EMA_WEIGHT_FAST * free_energy;

    /* Low free energy = high predictability = high compression potential */
    bridge->fep_effects.predicted_compressibility = expf(-free_energy / 5.0f);

    /* Compression confidence from FEP certainty */
    bridge->fep_effects.compression_confidence =
        1.0f / (1.0f + free_energy / 10.0f);

    /* Quality modulation based on prediction certainty */
    float certainty = 1.0f - free_energy / 20.0f;
    certainty = fmaxf(0.0f, fminf(1.0f, certainty));
    bridge->fep_effects.quality_modulation =
        0.5f + 0.5f * certainty; /* Range [0.5, 1.0] */

    /* Compression aggressiveness - low when uncertain */
    bridge->fep_effects.compression_aggressiveness =
        bridge->config.quality_vs_compression_tradeoff * certainty;

    /* Acceptable semantic loss based on FEP tolerance */
    bridge->fep_effects.acceptable_semantic_loss =
        0.1f * (1.0f - certainty); /* Higher uncertainty = more loss acceptable */

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int semantic_compression_fep_observe_compression(
    semantic_compression_fep_bridge_t* bridge,
    float ratio,
    float semantic_loss
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_observe_compression: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update compression observations */
    bridge->compression_effects.achieved_compression_ratio = ratio;
    bridge->compression_effects.semantic_loss_measured = semantic_loss;

    /* Compute compression prediction error */
    float predicted_ratio = bridge->fep_effects.predicted_compressibility * 10.0f;
    bridge->compression_effects.compression_prediction_error =
        ratio - predicted_ratio;

    /* Compute quality prediction error */
    float predicted_loss = bridge->fep_effects.acceptable_semantic_loss;
    bridge->compression_effects.quality_prediction_error =
        semantic_loss - predicted_loss;

    /* Compute compression surprise */
    float error_magnitude = fabsf(bridge->compression_effects.compression_prediction_error);
    bridge->compression_effects.compression_surprise = error_magnitude / 5.0f;

    /* Check for high semantic loss */
    bridge->compression_effects.high_semantic_loss_event =
        semantic_loss > bridge->config.semantic_surprise_threshold;

    /* Update statistics */
    bridge->state.total_compressions++;
    if (ratio > 1.0f && semantic_loss < 0.5f) {
        bridge->state.successful_compressions++;
    }

    bridge->stats.avg_compression_ratio =
        0.95f * bridge->stats.avg_compression_ratio + 0.05f * ratio;
    bridge->stats.avg_semantic_loss =
        0.95f * bridge->stats.avg_semantic_loss + 0.05f * semantic_loss;
    bridge->stats.avg_compression_surprise =
        0.95f * bridge->stats.avg_compression_surprise +
        0.05f * bridge->compression_effects.compression_surprise;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int semantic_compression_fep_predict_compressibility(
    semantic_compression_fep_bridge_t* bridge,
    const float* signal,
    size_t len,
    float* predicted_ratio,
    float* confidence
) {
    if (!bridge || !signal || !predicted_ratio || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_predict_compressibility: NULL parameter");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Use FEP to estimate signal predictability */
    /* High predictability = high compression ratio */
    *predicted_ratio = bridge->fep_effects.predicted_compressibility * 10.0f;
    *confidence = bridge->fep_effects.compression_confidence;

    /* Clamp ratio to reasonable bounds */
    *predicted_ratio = fmaxf(1.0f, fminf(*predicted_ratio, 20.0f));

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int semantic_compression_fep_learn_primitive(
    semantic_compression_fep_bridge_t* bridge,
    uint32_t level,
    uint32_t* primitive_id
) {
    if (!bridge || !primitive_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_learn_primitive: NULL bridge or primitive_id");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Extract FEP belief as semantic primitive */
    /* In a full implementation, would extract belief mean vector */
    /* and add as compression primitive */

    /* For now, just increment counter */
    *primitive_id = bridge->state.primitives_learned;
    bridge->state.primitives_learned++;
    bridge->state.current_primitives++;

    NIMCP_LOGGING_DEBUG("Learned primitive %u from FEP level %u",
                        *primitive_id, level);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

int semantic_compression_fep_connect_bio_async(semantic_compression_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_connect_bio_async: NULL bridge");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0; /* Already connected */
    }

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected semantic compression FEP bridge");

    return 0;
}

int semantic_compression_fep_disconnect_bio_async(semantic_compression_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_disconnect_bio_async: NULL bridge");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected semantic compression FEP bridge");

    return 0;
}

bool semantic_compression_fep_is_bio_async_connected(
    const semantic_compression_fep_bridge_t* bridge
) {
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int semantic_compression_fep_get_effects(
    const semantic_compression_fep_bridge_t* bridge,
    semantic_compression_fep_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_get_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->fep_effects, sizeof(semantic_compression_fep_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int semantic_compression_fep_get_compression_effects(
    const semantic_compression_fep_bridge_t* bridge,
    fep_semantic_compression_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_get_compression_effects: NULL bridge or effects");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->compression_effects, sizeof(fep_semantic_compression_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int semantic_compression_fep_get_stats(
    const semantic_compression_fep_bridge_t* bridge,
    semantic_compression_fep_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_get_stats: NULL bridge or stats");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(semantic_compression_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int semantic_compression_fep_reset_stats(semantic_compression_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "semantic_compression_fep_reset_stats: NULL bridge");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(semantic_compression_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Semantic_Compression_FEP_Bridge module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Semantic_Compression_FEP_Bridge entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int semantic_compression_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Semantic_Compression_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Semantic_Compression_FEP_Bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Semantic_Compression_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Semantic_Compression_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
