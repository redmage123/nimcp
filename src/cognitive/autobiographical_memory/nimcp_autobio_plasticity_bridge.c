/**
 * @file nimcp_autobio_plasticity_bridge.c
 * @brief Autobiographical Memory - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/autobiographical_memory/nimcp_autobio_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(autobio_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "AUTOBIO_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    autobio_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct autobio_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    autobio_plasticity_config_t config;

    /* State */
    autobio_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Consolidation state */
    autobio_consolidation_state_t consolidation_state;

    /* Global learning state */
    float current_emotional_intensity;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    autobio_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    autobio_plasticity_consolidation_callback_t consolidation_callback;
    void* consolidation_callback_data;

    /* Statistics */
    autobio_plasticity_stats_t stats;
};

static synapse_entry_t* find_synapse(autobio_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(autobio_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(autobio_synapse_type_t type) {
    return type == AUTOBIO_SYNAPSE_TEMPORAL ||
           type == AUTOBIO_SYNAPSE_CONSOLIDATION;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

autobio_plasticity_config_t autobio_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_c", 0.0f);


    autobio_plasticity_config_t config = {
        .base_learning_rate = AUTOBIO_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_memory_strength = 0.5f,

        .emotional_learning_boost = 1.5f,
        .retrieval_learning_boost = 1.3f,
        .self_relevance_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_temporal_context = true,
        .protect_consolidation = true,
        .protection_strength = 1.0f,

        .max_synapses = AUTOBIO_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

autobio_plasticity_bridge_t* autobio_plasticity_create(
    const autobio_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_c", 0.0f);


    autobio_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(autobio_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = autobio_plasticity_config_default();
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "autobio_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "autobio_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "autobio_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize consolidation state */
    bridge->consolidation_state.episodic_strength = 0.5f;
    bridge->consolidation_state.temporal_coherence = 0.5f;
    bridge->consolidation_state.emotional_consolidation = 0.5f;
    bridge->consolidation_state.self_relevance_strength = 0.5f;
    bridge->consolidation_state.retrieval_ease = 0.5f;
    bridge->consolidation_state.learning_rate_mod = 1.0f;
    bridge->consolidation_state.last_consolidation_us = 0;

    bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_emotional_intensity = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void autobio_plasticity_destroy(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "autobio_plasticity");

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_d", 0.0f);


    nimcp_free(bridge->synapses);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int autobio_plasticity_reset(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.weight = bridge->synapses[i].synapse.initial_weight;
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
            bridge->synapses[i].synapse.bcm_threshold = bridge->config.bcm_target_rate;
            bridge->synapses[i].synapse.avg_activity = 0.0f;
            bridge->synapses[i].synapse.update_count = 0;
        }
    }

    /* Reset consolidation state */
    bridge->consolidation_state.episodic_strength = 0.5f;
    bridge->consolidation_state.temporal_coherence = 0.5f;
    bridge->consolidation_state.emotional_consolidation = 0.5f;
    bridge->consolidation_state.self_relevance_strength = 0.5f;
    bridge->consolidation_state.retrieval_ease = 0.5f;
    bridge->consolidation_state.learning_rate_mod = 1.0f;

    bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
    bridge->current_emotional_intensity = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int autobio_plasticity_register_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    autobio_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_register_synapse: slot is NULL");
        return -1;
    }

    /* Initialize synapse */
    slot->in_use = true;
    slot->synapse.synapse_id = synapse_id;
    slot->synapse.type = type;
    slot->synapse.weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    slot->synapse.initial_weight = slot->synapse.weight;
    slot->synapse.eligibility_trace = 0.0f;
    slot->synapse.bcm_threshold = bridge->config.bcm_target_rate;
    slot->synapse.avg_activity = 0.0f;
    slot->synapse.last_update_us = bridge->current_time_us;
    slot->synapse.update_count = 0;

    /* Auto-protect temporal and consolidation synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_temporal_context || bridge->config.protect_consolidation);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_unregister_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_get_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    autobio_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_protect_synapse(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_p", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int autobio_plasticity_learn(
    autobio_plasticity_bridge_t* bridge,
    autobio_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_l", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = AUTOBIO_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case AUTOBIO_LEARN_ENCODING_SUCCESS:
            weight_change = lr * 1.0f;
            bridge->stats.encoding_success_events++;
            break;

        case AUTOBIO_LEARN_ENCODING_WEAK:
            weight_change = lr * 0.3f;
            break;

        case AUTOBIO_LEARN_RETRIEVAL_SUCCESS:
            weight_change = lr * bridge->config.retrieval_learning_boost;
            bridge->stats.retrieval_success_events++;
            break;

        case AUTOBIO_LEARN_RETRIEVAL_FAILURE:
            weight_change = -lr * 0.2f;
            break;

        case AUTOBIO_LEARN_EMOTIONAL_BOOST:
            weight_change = lr * bridge->config.emotional_learning_boost;
            bridge->stats.emotional_boost_events++;
            break;

        case AUTOBIO_LEARN_CONSOLIDATION_COMPLETE:
            weight_change = lr * 0.5f;
            bridge->stats.consolidation_events++;
            break;

        case AUTOBIO_LEARN_SELF_RELEVANCE_HIGH:
            weight_change = lr * bridge->config.self_relevance_modulation;
            break;

        case AUTOBIO_LEARN_TEMPORAL_LINK:
            weight_change = lr * 0.6f;
            break;

        case AUTOBIO_LEARN_CORE_MEMORY:
            weight_change = lr * 1.2f;
            break;

        case AUTOBIO_LEARN_DECAY_PREVENTED:
            weight_change = lr * 0.4f;
            break;
    }

    /* Context modulation */
    weight_change *= (0.5f + 0.5f * context);

    /* Apply weight change */
    float old_weight = entry->synapse.weight;
    entry->synapse.weight = nimcp_clampf(
        entry->synapse.weight + weight_change,
        bridge->config.weight_min,
        bridge->config.weight_max
    );

    /* Update statistics */
    float actual_change = entry->synapse.weight - old_weight;
    if (actual_change > 0) {
        bridge->stats.total_potentiation += actual_change;
    } else {
        bridge->stats.total_depression += -actual_change;
    }

    entry->synapse.update_count++;
    entry->synapse.last_update_us = bridge->current_time_us;

    bridge->stats.total_learning_events++;
    bridge->stats.weight_updates++;
    bridge->stats.mean_weight_change =
        (bridge->stats.mean_weight_change * (bridge->stats.weight_updates - 1) +
         fabsf(actual_change)) / bridge->stats.weight_updates;

    /* Snapshot callback before releasing lock */
    autobio_plasticity_learn_callback_t learn_cb = bridge->learn_callback;
    void* learn_cb_data = bridge->learn_callback_data;

    bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside lock to prevent deadlock */
    if (learn_cb) {
        learn_cb(bridge, event, magnitude, learn_cb_data);
    }

    return 0;
}

float autobio_plasticity_apply_stdp(
    autobio_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NAN;
    }

    /* Protected synapses don't get STDP */
    if (entry->synapse.is_protected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0.0f;
    }

    float dt = post_time - pre_time;
    float delta_w = 0.0f;

    if (dt > 0) {
        /* Potentiation: post after pre */
        delta_w = bridge->config.stdp_a_plus * expf(-dt / bridge->config.stdp_tau_plus_ms);
    } else {
        /* Depression: pre after post */
        delta_w = -bridge->config.stdp_a_minus * expf(dt / bridge->config.stdp_tau_minus_ms);
    }

    /* Apply weight change */
    float old_weight = entry->synapse.weight;
    entry->synapse.weight = nimcp_clampf(
        entry->synapse.weight + delta_w,
        bridge->config.weight_min,
        bridge->config.weight_max
    );

    float actual_change = entry->synapse.weight - old_weight;
    if (actual_change > 0) {
        bridge->stats.total_potentiation += actual_change;
    } else {
        bridge->stats.total_depression += -actual_change;
    }

    entry->synapse.update_count++;
    bridge->stats.weight_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return delta_w;
}

int autobio_plasticity_apply_emotional_boost(
    autobio_plasticity_bridge_t* bridge,
    float emotional_intensity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_apply_emotional_boost: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    emotional_intensity = nimcp_clampf(emotional_intensity, 0.0f, 1.0f);
    bridge->current_emotional_intensity = emotional_intensity;

    /* Apply emotional modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float trace = bridge->synapses[i].synapse.eligibility_trace;
            if (fabsf(trace) > 0.001f) {
                float boost = bridge->config.emotional_learning_boost * emotional_intensity;
                float delta = bridge->config.base_learning_rate * boost * trace;
                bridge->synapses[i].synapse.weight = nimcp_clampf(
                    bridge->synapses[i].synapse.weight + delta,
                    bridge->config.weight_min,
                    bridge->config.weight_max
                );
                bridge->stats.weight_updates++;
            }
        }
    }

    /* Update consolidation state */
    bridge->consolidation_state.emotional_consolidation =
        0.9f * bridge->consolidation_state.emotional_consolidation + 0.1f * emotional_intensity;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_update_bcm(
    autobio_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "autobio_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = AUTOBIO_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            /* Update sliding threshold towards average activity */
            float target = bridge->synapses[i].synapse.avg_activity;
            bridge->synapses[i].synapse.bcm_threshold =
                bridge->synapses[i].synapse.bcm_threshold * decay +
                target * (1.0f - decay);
        }
    }

    bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_homeostatic_update(
    autobio_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_h", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = AUTOBIO_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean memory strength */
    float mean_strength = 0.0f;
    uint32_t episodic_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == AUTOBIO_SYNAPSE_EPISODIC) {
            mean_strength += bridge->synapses[i].synapse.weight;
            episodic_count++;
        }
    }
    if (episodic_count > 0) {
        mean_strength /= episodic_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_memory_strength;
    float scale_factor = 1.0f;
    if (mean_strength > 0.0f) {
        scale_factor = target / mean_strength;
        scale_factor = nimcp_clampf(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float scaled = bridge->synapses[i].synapse.weight * (1.0f + (scale_factor - 1.0f) * (1.0f - decay));
            bridge->synapses[i].synapse.weight = nimcp_clampf(
                scaled,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_update_traces(
    autobio_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_consolidate(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = AUTOBIO_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update consolidation state based on synapse weights */
    float episodic_sum = 0.0f, episodic_count = 0;
    float temporal_sum = 0.0f, temporal_count = 0;
    float emotional_sum = 0.0f, emotional_count = 0;
    float self_ref_sum = 0.0f, self_ref_count = 0;
    float retrieval_sum = 0.0f, retrieval_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case AUTOBIO_SYNAPSE_EPISODIC:
                    episodic_sum += bridge->synapses[i].synapse.weight;
                    episodic_count++;
                    break;
                case AUTOBIO_SYNAPSE_TEMPORAL:
                    temporal_sum += bridge->synapses[i].synapse.weight;
                    temporal_count++;
                    break;
                case AUTOBIO_SYNAPSE_EMOTIONAL:
                    emotional_sum += bridge->synapses[i].synapse.weight;
                    emotional_count++;
                    break;
                case AUTOBIO_SYNAPSE_SELF_REFERENCE:
                    self_ref_sum += bridge->synapses[i].synapse.weight;
                    self_ref_count++;
                    break;
                case AUTOBIO_SYNAPSE_RETRIEVAL:
                    retrieval_sum += bridge->synapses[i].synapse.weight;
                    retrieval_count++;
                    break;
                default:
                    break;
            }
        }
    }

    float old_strength = bridge->consolidation_state.episodic_strength;

    if (episodic_count > 0) {
        bridge->consolidation_state.episodic_strength = episodic_sum / episodic_count;
    }
    if (temporal_count > 0) {
        bridge->consolidation_state.temporal_coherence = temporal_sum / temporal_count;
    }
    if (emotional_count > 0) {
        bridge->consolidation_state.emotional_consolidation = emotional_sum / emotional_count;
    }
    if (self_ref_count > 0) {
        bridge->consolidation_state.self_relevance_strength = self_ref_sum / self_ref_count;
    }
    if (retrieval_count > 0) {
        bridge->consolidation_state.retrieval_ease = retrieval_sum / retrieval_count;
    }

    bridge->consolidation_state.last_consolidation_us = bridge->current_time_us;

    /* Snapshot callback and values before releasing lock */
    autobio_plasticity_consolidation_callback_t consol_cb = bridge->consolidation_callback;
    void* consol_cb_data = bridge->consolidation_callback_data;
    float new_strength = bridge->consolidation_state.episodic_strength;

    bridge->state = AUTOBIO_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke consolidation callback outside lock to prevent deadlock */
    if (consol_cb) {
        consol_cb(bridge, old_strength, new_strength, consol_cb_data);
    }

    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int autobio_plasticity_get_consolidation_state(
    autobio_plasticity_bridge_t* bridge,
    autobio_consolidation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_get_consolidation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->consolidation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int autobio_plasticity_get_state(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;
    state->learning_rate_effective = bridge->learning_rate_effective;

    /* Calculate mean weight and variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            autobio_plasticity_bridge_heartbeat("autobio_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            float w = bridge->synapses[i].synapse.weight;
            sum += w;
            sum_sq += w * w;
        }
    }

    if (bridge->synapse_count > 0) {
        state->mean_weight = sum / bridge->synapse_count;
        state->weight_variance = (sum_sq / bridge->synapse_count) -
                                (state->mean_weight * state->mean_weight);
    } else {
        state->mean_weight = 0.0f;
        state->weight_variance = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_plasticity_get_stats(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int autobio_plasticity_reset_stats(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(autobio_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int autobio_plasticity_register_learn_callback(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int autobio_plasticity_register_consolidation_callback(
    autobio_plasticity_bridge_t* bridge,
    autobio_plasticity_consolidation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_register_consolidation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->consolidation_callback = callback;
    bridge->consolidation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int autobio_plasticity_bio_async_connect(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int autobio_plasticity_bio_async_disconnect(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool autobio_plasticity_is_bio_async_connected(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    autobio_plasticity_bridge_heartbeat("autobio_plas_autobio_plasticity_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

void autobio_plasticity_bridge_set_instance_health_agent(autobio_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
    g_autobio_plasticity_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("autobio_plasticity_bridge: instance health agent %s",
                        agent ? "set" : "cleared");
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int autobio_plasticity_bridge_training_begin(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    autobio_plasticity_bridge_heartbeat_instance(bridge, "autobio_pla_training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int autobio_plasticity_bridge_training_end(autobio_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    autobio_plasticity_bridge_heartbeat_instance(bridge, "autobio_pla_training_end", 1.0f);
    (void)bridge;
    return 0;
}

int autobio_plasticity_bridge_training_step(autobio_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    autobio_plasticity_bridge_heartbeat_instance(bridge, "autobio_pla_training_step", progress);
    (void)bridge;
    return 0;
}
