/**
 * @file nimcp_jepa_plasticity_bridge.c
 * @brief JEPA - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/jepa/nimcp_jepa_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(jepa_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "JEPA_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    jepa_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct jepa_plasticity_bridge {
    bridge_base_t base;  /* MUST be first member */
    jepa_plasticity_config_t config;

    /* State */
    jepa_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Prediction state */
    jepa_prediction_state_t prediction_state;

    /* Global learning state */
    float current_error;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    jepa_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    jepa_plasticity_prediction_callback_t prediction_callback;
    void* prediction_callback_data;

    /* Statistics */
    jepa_plasticity_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

static synapse_entry_t* find_synapse(jepa_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(jepa_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(jepa_synapse_type_t type, const jepa_plasticity_config_t* config) {
    if (type == JEPA_SYNAPSE_PREDICTION && config->protect_prediction_pathway) {
        return true;
    }
    if (type == JEPA_SYNAPSE_SELF_SUPERVISED && config->protect_self_supervised) {
        return true;
    }
    return false;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

jepa_plasticity_config_t jepa_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_conf", 0.0f);


    jepa_plasticity_config_t config = {
        .base_learning_rate = JEPA_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_prediction_accuracy = 0.8f,

        .prediction_accuracy_boost = 1.5f,
        .error_learning_boost = 1.3f,
        .context_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_prediction_pathway = true,
        .protect_self_supervised = true,
        .protection_strength = 1.0f,

        .max_synapses = JEPA_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

jepa_plasticity_bridge_t* jepa_plasticity_create(
    const jepa_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_crea", 0.0f);


    jepa_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(jepa_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = jepa_plasticity_config_default();
    }

    /* Initialize base (includes mutex creation) */
    if (bridge_base_init(&bridge->base, 0, "jepa_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "jepa_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize prediction state */
    bridge->prediction_state.prediction_accuracy = 0.5f;
    bridge->prediction_state.context_calibration = 0.5f;
    bridge->prediction_state.latent_quality = 0.5f;
    bridge->prediction_state.multimodal_alignment = 0.5f;
    bridge->prediction_state.self_supervised_strength = 0.5f;
    bridge->prediction_state.learning_rate_mod = 1.0f;
    bridge->prediction_state.last_learning_us = 0;

    bridge->state = JEPA_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_error = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void jepa_plasticity_destroy(jepa_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "jepa_plasticity");

    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_dest", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int jepa_plasticity_reset(jepa_plasticity_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_rese", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
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

    /* Reset prediction state */
    bridge->prediction_state.prediction_accuracy = 0.5f;
    bridge->prediction_state.context_calibration = 0.5f;
    bridge->prediction_state.latent_quality = 0.5f;
    bridge->prediction_state.learning_rate_mod = 1.0f;

    bridge->state = JEPA_PLASTICITY_STATE_IDLE;
    bridge->current_error = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int jepa_plasticity_register_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    jepa_synapse_type_t type,
    float initial_weight
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_regi", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_plasticity_register_synapse: slot is NULL");
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

    /* Auto-protect prediction and self-supervised synapses */
    slot->synapse.is_protected = is_protected_type(type, &bridge->config);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_unregister_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_unre", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_get_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    jepa_plasticity_synapse_t* synapse
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_get_", 0.0f);


    NIMCP_CHECK_THROW(bridge && synapse, -1, "bridge or synapse is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_protect_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_prot", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int jepa_plasticity_learn(
    jepa_plasticity_bridge_t* bridge,
    jepa_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_lear", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = JEPA_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = JEPA_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = JEPA_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case JEPA_LEARN_PREDICTION_ACCURATE:
            weight_change = lr * bridge->config.prediction_accuracy_boost;
            bridge->stats.prediction_accurate_events++;
            break;

        case JEPA_LEARN_PREDICTION_ERROR:
            weight_change = -lr * bridge->config.error_learning_boost;
            bridge->stats.prediction_error_events++;
            break;

        case JEPA_LEARN_CONTEXT_MATCHED:
            weight_change = lr * bridge->config.context_modulation;
            bridge->stats.context_matched_events++;
            break;

        case JEPA_LEARN_CONTEXT_MISMATCH:
            weight_change = -lr * 0.3f;
            break;

        case JEPA_LEARN_LATENT_CONVERGED:
            weight_change = lr * 1.2f;
            bridge->stats.latent_converged_events++;
            break;

        case JEPA_LEARN_LATENT_DIVERGED:
            weight_change = -lr * 0.5f;
            break;

        case JEPA_LEARN_MULTIMODAL_ALIGNED:
            weight_change = lr * 1.0f;
            break;

        case JEPA_LEARN_MASKING_SUCCESS:
            weight_change = lr * 1.1f;
            break;

        case JEPA_LEARN_MASKING_FAILURE:
            weight_change = -lr * 0.4f;
            break;

        case JEPA_LEARN_EMBEDDING_IMPROVED:
            weight_change = lr * 0.8f;
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

    bridge->state = JEPA_PLASTICITY_STATE_IDLE;

    /* Snapshot callback under mutex */
    jepa_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside mutex to prevent deadlock */
    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float jepa_plasticity_apply_stdp(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_plasticity_apply_stdp: bridge is NULL");
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_appl", 0.0f);


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

int jepa_plasticity_apply_precision(
    jepa_plasticity_bridge_t* bridge,
    float precision
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_appl", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    precision = nimcp_clampf(precision, 0.0f, 1.0f);

    /* Apply precision modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float trace = bridge->synapses[i].synapse.eligibility_trace;
            if (fabsf(trace) > 0.001f) {
                float delta = bridge->config.base_learning_rate * precision * trace;
                bridge->synapses[i].synapse.weight = nimcp_clampf(
                    bridge->synapses[i].synapse.weight + delta,
                    bridge->config.weight_min,
                    bridge->config.weight_max
                );
                bridge->stats.weight_updates++;
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_update_bcm(
    jepa_plasticity_bridge_t* bridge,
    float dt_ms
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_upda", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    NIMCP_CHECK_THROW(dt_ms > 0.0f, -1, "dt_ms must be positive");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = JEPA_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
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

    bridge->state = JEPA_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_homeostatic_update(
    jepa_plasticity_bridge_t* bridge,
    float dt_ms
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_home", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = JEPA_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean prediction accuracy */
    float mean_prediction = 0.0f;
    uint32_t prediction_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == JEPA_SYNAPSE_PREDICTION) {
            mean_prediction += bridge->synapses[i].synapse.weight;
            prediction_count++;
        }
    }
    if (prediction_count > 0) {
        mean_prediction /= prediction_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_prediction_accuracy;
    float scale_factor = 1.0f;
    if (mean_prediction > 0.0f) {
        scale_factor = target / mean_prediction;
        scale_factor = nimcp_clampf(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
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

    bridge->state = JEPA_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_update_traces(
    jepa_plasticity_bridge_t* bridge,
    float dt_ms
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_upda", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_plasticity_consolidate(jepa_plasticity_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_cons", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = JEPA_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update prediction state based on synapse weights */
    float prediction_sum = 0.0f, prediction_count = 0;
    float context_sum = 0.0f, context_count = 0;
    float latent_sum = 0.0f, latent_count = 0;
    float multimodal_sum = 0.0f, multimodal_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case JEPA_SYNAPSE_PREDICTION:
                    prediction_sum += bridge->synapses[i].synapse.weight;
                    prediction_count++;
                    break;
                case JEPA_SYNAPSE_CONTEXT:
                    context_sum += bridge->synapses[i].synapse.weight;
                    context_count++;
                    break;
                case JEPA_SYNAPSE_LATENT:
                    latent_sum += bridge->synapses[i].synapse.weight;
                    latent_count++;
                    break;
                case JEPA_SYNAPSE_MULTIMODAL:
                    multimodal_sum += bridge->synapses[i].synapse.weight;
                    multimodal_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (prediction_count > 0) {
        bridge->prediction_state.prediction_accuracy = prediction_sum / prediction_count * 2.0f;
    }
    if (context_count > 0) {
        bridge->prediction_state.context_calibration = context_sum / context_count;
    }
    if (latent_count > 0) {
        bridge->prediction_state.latent_quality = latent_sum / latent_count;
    }
    if (multimodal_count > 0) {
        bridge->prediction_state.multimodal_alignment = multimodal_sum / multimodal_count;
    }

    bridge->prediction_state.last_learning_us = bridge->current_time_us;
    bridge->state = JEPA_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int jepa_plasticity_get_prediction_state(
    jepa_plasticity_bridge_t* bridge,
    jepa_prediction_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_get_", 0.0f);


    NIMCP_CHECK_THROW(bridge && state, -1, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->prediction_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_plasticity_get_state(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_bridge_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_get_", 0.0f);


    NIMCP_CHECK_THROW(bridge && state, -1, "bridge or state is NULL");

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
            jepa_plasticity_bridge_heartbeat("jepa_plastic_loop",
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

int jepa_plasticity_get_stats(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_get_", 0.0f);


    NIMCP_CHECK_THROW(bridge && stats, -1, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_plasticity_reset_stats(jepa_plasticity_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_rese", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(jepa_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int jepa_plasticity_register_learn_callback(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_learn_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_regi", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_plasticity_register_prediction_callback(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_prediction_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_regi", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->prediction_callback = callback;
    bridge->prediction_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int jepa_plasticity_bio_async_connect(jepa_plasticity_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_bio_", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->config.enable_bio_async, -1, "bio_async not enabled");

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_plasticity_bio_async_disconnect(jepa_plasticity_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_bio_", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool jepa_plasticity_is_bio_async_connected(jepa_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_plasticity_bridge_heartbeat("jepa_plastic_jepa_plasticity_is_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_plasticity_bridge_set_instance_health_agent(jepa_plasticity_bridge_t* bridge,
                                                       nimcp_health_agent_t* agent) {
    if (!bridge) return;
    bridge->health_agent = agent;
}

int jepa_plasticity_bridge_training_begin(jepa_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    jepa_plasticity_bridge_heartbeat_instance(bridge, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int jepa_plasticity_bridge_training_end(jepa_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    jepa_plasticity_bridge_heartbeat_instance(bridge, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

int jepa_plasticity_bridge_training_step(jepa_plasticity_bridge_t* bridge, uint32_t step) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_plasticity_bridge_heartbeat_instance(bridge, "training_step", progress);
    (void)bridge;
    return 0;
}
