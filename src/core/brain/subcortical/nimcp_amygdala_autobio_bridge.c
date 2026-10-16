/**
 * @file nimcp_amygdala_autobio_bridge.c
 * @brief Amygdala-Autobiographical Memory Integration Bridge Implementation
 *
 * WHAT: Bidirectional coupling between emotional processing and autobiographical memory
 * WHY:  Emotional events are better remembered; recalling emotional memories reactivates emotions
 * HOW:  Amygdala tags memories with emotional salience; memory recall triggers amygdala
 */

#include "core/brain/subcortical/nimcp_amygdala_autobio_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(amygdala_autobio_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int amygdala_autobio_default_config(amygdala_autobio_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->enable_emotional_tagging = true;
    config->enable_flashbulb_memories = true;
    config->enable_fear_consolidation = true;
    config->enable_recall_reactivation = true;
    config->enable_positive_regulation = true;
    config->enable_negative_bias = true;

    config->salience_sensitivity = 1.0f;
    config->reactivation_sensitivity = 1.0f;

    config->flashbulb_fear_threshold = FLASHBULB_FEAR_THRESHOLD;
    config->trauma_reactivation_threshold = TRAUMA_REACTIVATION_THRESHOLD;

    return 0;
}

amygdala_autobio_bridge_t* amygdala_autobio_create(
    const amygdala_autobio_config_t* config
) {
    amygdala_autobio_bridge_t* bridge = nimcp_malloc(sizeof(amygdala_autobio_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate amygdala-autobio bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(amygdala_autobio_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        amygdala_autobio_default_config(&bridge->config);
    }

    /* Initialize bridge base (allocates mutex, sets module name) */
    if (bridge_base_init(&bridge->base, 0, "amygdala_autobio") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created amygdala-autobio bridge");
    return bridge;
}

void amygdala_autobio_destroy(amygdala_autobio_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Cleanup bridge base (disconnects bio-async, destroys+frees mutex) */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed amygdala-autobio bridge");
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int amygdala_autobio_connect_amygdala(
    amygdala_autobio_bridge_t* bridge,
    amygdala_t* amygdala
) {
    NIMCP_CHECK_THROW(bridge && amygdala, NIMCP_ERROR_NULL_POINTER, "bridge or amygdala is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->amygdala = amygdala;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = (bridge->base.system_a_connected && bridge->base.system_b_connected);
    bridge->connected = bridge->base.bridge_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected amygdala to bridge");
    return 0;
}

int amygdala_autobio_connect_memory(
    amygdala_autobio_bridge_t* bridge,
    autobiographical_memory_t autobio
) {
    NIMCP_CHECK_THROW(bridge && autobio, NIMCP_ERROR_NULL_POINTER, "bridge or autobio is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->autobio_memory = autobio;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = (bridge->base.system_a_connected && bridge->base.system_b_connected);
    bridge->connected = bridge->base.bridge_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected autobiographical memory to bridge");
    return 0;
}

/* ============================================================================
 * Amygdala → Memory Functions (Emotional Tagging)
 * ============================================================================ */

int amygdala_autobio_update(amygdala_autobio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->connected, NIMCP_ERROR_INVALID_STATE, "bridge is not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Query amygdala state */
    bridge->tagging_state.fear_level = amygdala_get_fear_level(bridge->amygdala);
    bridge->tagging_state.anxiety_level = amygdala_get_anxiety_level(bridge->amygdala);
    bridge->tagging_state.threat_level = amygdala_get_threat_level(bridge->amygdala);

    /* Compute arousal as combination of fear and anxiety */
    bridge->tagging_state.arousal_level =
        bridge->tagging_state.fear_level * 0.7f +
        bridge->tagging_state.anxiety_level * 0.3f;

    if (bridge->config.enable_emotional_tagging) {
        /* Compute emotional salience boost */
        float base_salience = AMYGDALA_SALIENCE_BOOST_BASE;
        float arousal_boost = bridge->tagging_state.arousal_level * AMYGDALA_SALIENCE_BOOST_SCALE;
        bridge->tagging_state.emotional_salience =
            (base_salience + arousal_boost) * bridge->config.salience_sensitivity;

        /* Apply negative bias if enabled */
        if (bridge->config.enable_negative_bias) {
            bridge->tagging_state.negative_bias = AMYGDALA_NEGATIVE_BIAS;
        } else {
            bridge->tagging_state.negative_bias = 0.0f;
        }
    }

    if (bridge->config.enable_flashbulb_memories) {
        /* Check for flashbulb memory conditions */
        bridge->tagging_state.flashbulb_mode =
            (bridge->tagging_state.fear_level >= bridge->config.flashbulb_fear_threshold) &&
            (bridge->tagging_state.arousal_level >= FLASHBULB_AROUSAL_THRESHOLD);
    }

    if (bridge->config.enable_fear_consolidation) {
        /* Compute consolidation boost */
        float fear_factor = bridge->tagging_state.fear_level;
        bridge->tagging_state.consolidation_boost =
            FEAR_CONSOLIDATION_BASE +
            (FEAR_CONSOLIDATION_MAX - FEAR_CONSOLIDATION_BASE) * fear_factor;
    }

    bridge->total_updates++;
    bridge->base.total_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_autobio_tag_memory(
    amygdala_autobio_bridge_t* bridge,
    uint64_t memory_id,
    float emotional_intensity
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->connected, NIMCP_ERROR_INVALID_STATE, "bridge is not connected");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Retrieve memory */
    autobiographical_memory_entry_t memory;
    if (!autobio_retrieve(bridge->autobio_memory, memory_id, &memory)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Compute total salience boost */
    float salience_boost = bridge->tagging_state.emotional_salience;

    /* Extra boost for negative memories */
    if (memory.valence < VALENCE_NEUTRAL && bridge->config.enable_negative_bias) {
        salience_boost += bridge->tagging_state.negative_bias;
    }

    /* Flashbulb multiplier */
    if (bridge->tagging_state.flashbulb_mode) {
        salience_boost *= FLASHBULB_SALIENCE_MULTIPLIER;
        bridge->tagging_state.flashbulb_memories++;
        bridge->flashbulb_count++;
    }

    /* Apply salience boost to importance (clamped to [0, 1]) */
    float new_importance = memory.importance + salience_boost * emotional_intensity;
    if (new_importance > 1.0f) {
        new_importance = 1.0f;
    }

    /* Update importance in the autobiographical memory system */
    autobio_update_importance(bridge->autobio_memory, memory_id, new_importance);

    /* Note: memory_strength and identity_defining updates would require
     * additional autobio_update_* functions to modify in-place */

    bridge->tagging_state.tagged_memories++;
    bridge->memories_tagged++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float amygdala_autobio_get_salience_boost(const amygdala_autobio_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }

    return 1.0f + bridge->tagging_state.emotional_salience;
}

bool amygdala_autobio_is_flashbulb_mode(const amygdala_autobio_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->tagging_state.flashbulb_mode;
}

float amygdala_autobio_get_consolidation_boost(const amygdala_autobio_bridge_t* bridge) {
    if (!bridge) {
        return 0.0f;
    }

    return bridge->tagging_state.consolidation_boost;
}

/* ============================================================================
 * Memory → Amygdala Functions (Recall Reactivation)
 * ============================================================================ */

int amygdala_autobio_on_recall(
    amygdala_autobio_bridge_t* bridge,
    uint64_t memory_id
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->connected && bridge->config.enable_recall_reactivation,
                      NIMCP_ERROR_INVALID_STATE, "bridge not connected or recall reactivation disabled");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Retrieve memory */
    autobiographical_memory_entry_t memory;
    if (!autobio_retrieve(bridge->autobio_memory, memory_id, &memory)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Update reactivation state */
    bridge->reactivation_state.memory_id = memory_id;
    bridge->reactivation_state.memory_type = memory.type;
    bridge->reactivation_state.valence = memory.valence;
    bridge->reactivation_state.emotional_intensity = memory.emotional_intensity;
    bridge->reactivation_state.importance = memory.importance;

    /* Reset reactivation levels */
    bridge->reactivation_state.fear_reactivation = 0.0f;
    bridge->reactivation_state.anxiety_reactivation = 0.0f;
    bridge->reactivation_state.positive_regulation = 0.0f;

    /* Process based on memory type and valence */
    if (memory.type == AUTOBIO_CRISIS || memory.type == AUTOBIO_FAILURE) {
        /* Trauma/crisis memory reactivation */
        if (memory.importance >= bridge->config.trauma_reactivation_threshold) {
            bridge->reactivation_state.full_reactivation = true;
            bridge->reactivation_state.fear_reactivation =
                TRAUMA_FEAR_REACTIVATION * memory.emotional_intensity;
            bridge->reactivation_state.anxiety_reactivation =
                CRISIS_ANXIETY_REACTIVATION * memory.emotional_intensity;

            bridge->reactivation_state.trauma_recalls++;
            bridge->trauma_reactivations++;
        } else {
            bridge->reactivation_state.full_reactivation = false;
            bridge->reactivation_state.anxiety_reactivation =
                CRISIS_ANXIETY_REACTIVATION * memory.emotional_intensity * 0.5f;
        }
    } else if (memory.valence < VALENCE_NEUTRAL) {
        /* Negative memory (not trauma) */
        if (memory.importance >= NEGATIVE_MEMORY_IMPACT_THRESHOLD) {
            float valence_factor = (float)(-memory.valence) / 2.0f;  /* Scale -1/-2 to 0.5/1.0 */
            bridge->reactivation_state.anxiety_reactivation =
                NEGATIVE_VALENCE_SCALING * valence_factor * memory.emotional_intensity;
        }
    } else if (memory.valence > VALENCE_NEUTRAL) {
        /* Positive memory regulation */
        if (bridge->config.enable_positive_regulation &&
            memory.importance >= POSITIVE_MEMORY_REGULATION_THRESHOLD) {

            float valence_factor = (float)memory.valence / 2.0f;  /* Scale 1/2 to 0.5/1.0 */
            bridge->reactivation_state.positive_regulation =
                valence_factor * memory.emotional_intensity;
            bridge->reactivation_state.anxiety_reduction =
                bridge->reactivation_state.positive_regulation * 0.3f;

            bridge->reactivation_state.positive_recalls++;
            bridge->positive_regulations++;
        }
    }

    /* Apply reactivation sensitivity */
    bridge->reactivation_state.fear_reactivation *= bridge->config.reactivation_sensitivity;
    bridge->reactivation_state.anxiety_reactivation *= bridge->config.reactivation_sensitivity;

    /* Compute emotional re-experience intensity */
    bridge->reactivation_state.emotional_reexperience =
        memory.importance * memory.emotional_intensity;

    /* Apply to amygdala */
    if (bridge->reactivation_state.fear_reactivation > 0.0f) {
        float current_fear = amygdala_get_fear_level(bridge->amygdala);
        float new_fear = current_fear + bridge->reactivation_state.fear_reactivation;
        if (new_fear > 1.0f) new_fear = 1.0f;
        amygdala_set_fear_level(bridge->amygdala, new_fear);
    }

    if (bridge->reactivation_state.anxiety_reactivation > 0.0f) {
        float current_anxiety = amygdala_get_anxiety_level(bridge->amygdala);
        float new_anxiety = current_anxiety + bridge->reactivation_state.anxiety_reactivation;
        if (new_anxiety > 1.0f) new_anxiety = 1.0f;
        amygdala_set_anxiety(bridge->amygdala, new_anxiety);
    }

    if (bridge->reactivation_state.anxiety_reduction > 0.0f) {
        float current_anxiety = amygdala_get_anxiety_level(bridge->amygdala);
        float new_anxiety = current_anxiety - bridge->reactivation_state.anxiety_reduction;
        if (new_anxiety < 0.0f) new_anxiety = 0.0f;
        amygdala_set_anxiety(bridge->amygdala, new_anxiety);
    }

    bridge->memories_recalled++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int amygdala_autobio_reactivate_trauma(
    amygdala_autobio_bridge_t* bridge,
    const autobiographical_memory_entry_t* trauma_memory
) {
    NIMCP_CHECK_THROW(bridge && trauma_memory, NIMCP_ERROR_NULL_POINTER, "bridge or trauma_memory is NULL");
    NIMCP_CHECK_THROW(bridge->connected, NIMCP_ERROR_INVALID_STATE, "bridge is not connected");

    /* Call on_recall with the trauma memory ID */
    return amygdala_autobio_on_recall(bridge, trauma_memory->memory_id);
}

int amygdala_autobio_regulate_from_positive(
    amygdala_autobio_bridge_t* bridge,
    const autobiographical_memory_entry_t* positive_memory
) {
    NIMCP_CHECK_THROW(bridge && positive_memory, NIMCP_ERROR_NULL_POINTER, "bridge or positive_memory is NULL");
    NIMCP_CHECK_THROW(bridge->connected && bridge->config.enable_positive_regulation,
                      NIMCP_ERROR_INVALID_STATE, "bridge not connected or positive regulation disabled");

    /* Verify memory is positive */
    NIMCP_CHECK_THROW(positive_memory->valence > VALENCE_NEUTRAL, NIMCP_ERROR_INVALID_PARAM, "memory must have positive valence");

    /* Call on_recall with the positive memory ID */
    return amygdala_autobio_on_recall(bridge, positive_memory->memory_id);
}

int amygdala_autobio_get_reactivation(
    const amygdala_autobio_bridge_t* bridge,
    float* fear_reactivation,
    float* anxiety_reactivation
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (fear_reactivation) {
        *fear_reactivation = bridge->reactivation_state.fear_reactivation;
    }

    if (anxiety_reactivation) {
        *anxiety_reactivation = bridge->reactivation_state.anxiety_reactivation;
    }

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int amygdala_autobio_get_tagging_state(
    const amygdala_autobio_bridge_t* bridge,
    emotional_tagging_state_t* state
) {
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    *state = bridge->tagging_state;
    return 0;
}

int amygdala_autobio_get_reactivation_state(
    const amygdala_autobio_bridge_t* bridge,
    recall_reactivation_state_t* state
) {
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    *state = bridge->reactivation_state;
    return 0;
}

int amygdala_autobio_get_statistics(
    const amygdala_autobio_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* memories_tagged,
    uint32_t* trauma_reactivations
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (total_updates) {
        *total_updates = bridge->total_updates;
    }

    if (memories_tagged) {
        *memories_tagged = bridge->memories_tagged;
    }

    if (trauma_reactivations) {
        *trauma_reactivations = bridge->trauma_reactivations;
    }

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int amygdala_autobio_connect_bio_async(amygdala_autobio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_AMYGDALA_AUTOBIO,
        .module_name = "amygdala_autobio_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected amygdala-autobio bridge to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

int amygdala_autobio_disconnect_bio_async(amygdala_autobio_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected amygdala-autobio bridge from bio-async router");

    return 0;
}

bool amygdala_autobio_is_bio_async_connected(const amygdala_autobio_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
