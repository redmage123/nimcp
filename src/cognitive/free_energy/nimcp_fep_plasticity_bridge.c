/**
 * @file nimcp_fep_plasticity_bridge.c
 * @brief Free Energy Principle - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/free_energy/nimcp_fep_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(fep_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "FEP_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    fep_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct fep_plasticity_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
    fep_plasticity_config_t config;

    /* State */
    fep_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Inference state */
    fep_inference_state_t inference_state;

    /* Global learning state */
    float current_pred_error;
    float learning_rate_effective;
    float bcm_global_threshold;
    float prev_free_energy;

    /* Callbacks */
    fep_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    fep_plasticity_inference_callback_t inference_callback;
    void* inference_callback_data;

    /* Statistics */
    fep_plasticity_stats_t stats;
};

static synapse_entry_t* find_synapse(fep_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(fep_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(fep_synapse_type_t type) {
    return type == FEP_SYNAPSE_PREDICTION ||
           type == FEP_SYNAPSE_ACTIVE_INFERENCE;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

fep_plasticity_config_t fep_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_confi", 0.0f);


    fep_plasticity_config_t config = {
        .base_learning_rate = FEP_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_free_energy = 0.3f,

        .pred_error_learning_boost = 1.5f,
        .precision_learning_scale = 1.2f,
        .belief_update_modulation = 1.0f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_prediction_weights = true,
        .protect_active_inference = true,
        .protection_strength = 1.0f,

        .max_synapses = FEP_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

fep_plasticity_bridge_t* fep_plasticity_create(
    const fep_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_creat", 0.0f);


    fep_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(fep_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = fep_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "fep_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "fep_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize inference state */
    bridge->inference_state.prediction_accuracy = 0.5f;
    bridge->inference_state.precision_calibration = 0.5f;
    bridge->inference_state.belief_stability = 0.5f;
    bridge->inference_state.free_energy_trend = 0.0f;
    bridge->inference_state.model_complexity = 0.5f;
    bridge->inference_state.learning_rate_mod = 1.0f;
    bridge->inference_state.last_learning_us = 0;

    bridge->state = FEP_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_pred_error = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;
    bridge->prev_free_energy = 0.5f;

    return bridge;
}

void fep_plasticity_destroy(fep_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "fep_plasticity");

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_destr", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int fep_plasticity_reset(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
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

    /* Reset inference state */
    bridge->inference_state.prediction_accuracy = 0.5f;
    bridge->inference_state.precision_calibration = 0.5f;
    bridge->inference_state.belief_stability = 0.5f;
    bridge->inference_state.free_energy_trend = 0.0f;
    bridge->inference_state.learning_rate_mod = 1.0f;

    bridge->state = FEP_PLASTICITY_STATE_IDLE;
    bridge->current_pred_error = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->prev_free_energy = 0.5f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int fep_plasticity_register_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    fep_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_register_synapse: slot is NULL");
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

    /* Auto-protect prediction and active inference synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_prediction_weights || bridge->config.protect_active_inference);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_plasticity_unregister_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_unreg", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_plasticity_get_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    fep_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_plasticity_protect_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_prote", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int fep_plasticity_learn(
    fep_plasticity_bridge_t* bridge,
    fep_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float precision
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_learn", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = FEP_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = FEP_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = FEP_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    /* Precision-weighted learning rate */
    float lr = bridge->learning_rate_effective * magnitude;
    lr *= (bridge->config.precision_learning_scale * precision);
    float weight_change = 0.0f;

    switch (event) {
        case FEP_LEARN_PREDICTION_CONFIRMED:
            weight_change = lr * 0.5f;
            bridge->stats.prediction_confirmed_events++;
            bridge->inference_state.prediction_accuracy += 0.01f;
            break;

        case FEP_LEARN_PREDICTION_ERROR:
            weight_change = -lr * bridge->config.pred_error_learning_boost;
            bridge->stats.prediction_error_events++;
            bridge->inference_state.prediction_accuracy -= 0.02f;
            break;

        case FEP_LEARN_PRECISION_UPDATE:
            weight_change = lr * 0.3f;
            bridge->inference_state.precision_calibration += 0.01f;
            break;

        case FEP_LEARN_BELIEF_UPDATE:
            weight_change = lr * bridge->config.belief_update_modulation;
            break;

        case FEP_LEARN_FREE_ENERGY_REDUCED:
            weight_change = lr * 0.8f;
            bridge->stats.free_energy_reduced_events++;
            bridge->inference_state.free_energy_trend -= 0.01f;
            break;

        case FEP_LEARN_FREE_ENERGY_INCREASED:
            weight_change = -lr * 0.4f;
            bridge->inference_state.free_energy_trend += 0.02f;
            break;

        case FEP_LEARN_ACTIVE_INFERENCE_SUCCESS:
            weight_change = lr * 1.0f;
            bridge->stats.active_inference_events++;
            break;

        case FEP_LEARN_ACTIVE_INFERENCE_FAILURE:
            weight_change = -lr * 0.3f;
            break;

        case FEP_LEARN_MODEL_COMPLEXITY_REDUCED:
            weight_change = lr * 0.5f;
            bridge->inference_state.model_complexity -= 0.01f;
            break;

        case FEP_LEARN_VARIATIONAL_BOUND_TIGHT:
            weight_change = lr * 0.4f;
            break;
    }

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

    /* Clamp inference state values */
    bridge->inference_state.prediction_accuracy =
        nimcp_clampf(bridge->inference_state.prediction_accuracy, 0.0f, 1.0f);
    bridge->inference_state.precision_calibration =
        nimcp_clampf(bridge->inference_state.precision_calibration, 0.0f, 1.0f);
    bridge->inference_state.model_complexity =
        nimcp_clampf(bridge->inference_state.model_complexity, 0.0f, 1.0f);

    /* Copy callback ptr and data under lock, invoke outside lock to prevent deadlock */
    fep_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    bridge->state = FEP_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside lock */
    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float fep_plasticity_apply_stdp(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_apply", 0.0f);


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

int fep_plasticity_apply_pred_error(
    fep_plasticity_bridge_t* bridge,
    float pred_error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_apply_pred_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_apply", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pred_error = nimcp_clampf(pred_error, -1.0f, 1.0f);
    bridge->current_pred_error = pred_error;

    /* Apply prediction error modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float trace = bridge->synapses[i].synapse.eligibility_trace;
            if (fabsf(trace) > 0.001f) {
                float delta = bridge->config.base_learning_rate *
                             bridge->config.pred_error_learning_boost *
                             pred_error * trace;
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

int fep_plasticity_update_bcm(
    fep_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = FEP_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
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

    bridge->state = FEP_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_plasticity_homeostatic_update(
    fep_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_homeo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = FEP_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean free energy proxy from prediction synapses */
    float mean_prediction = 0.0f;
    uint32_t prediction_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == FEP_SYNAPSE_PREDICTION) {
            mean_prediction += bridge->synapses[i].synapse.weight;
            prediction_count++;
        }
    }
    if (prediction_count > 0) {
        mean_prediction /= prediction_count;
    }

    /* Scale non-protected synapses toward target free energy */
    float target = bridge->config.target_free_energy;
    float scale_factor = 1.0f;
    if (mean_prediction > 0.0f) {
        scale_factor = target / mean_prediction;
        scale_factor = nimcp_clampf(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
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

    bridge->state = FEP_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_plasticity_update_traces(
    fep_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_updat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_plasticity_consolidate(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_conso", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = FEP_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update inference state based on synapse weights */
    float prediction_sum = 0.0f, prediction_count = 0;
    float precision_sum = 0.0f, precision_count = 0;
    float belief_sum = 0.0f, belief_count = 0;
    float error_sum = 0.0f, error_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case FEP_SYNAPSE_PREDICTION:
                    prediction_sum += bridge->synapses[i].synapse.weight;
                    prediction_count++;
                    break;
                case FEP_SYNAPSE_PRECISION:
                    precision_sum += bridge->synapses[i].synapse.weight;
                    precision_count++;
                    break;
                case FEP_SYNAPSE_BELIEF:
                    belief_sum += bridge->synapses[i].synapse.weight;
                    belief_count++;
                    break;
                case FEP_SYNAPSE_ERROR:
                    error_sum += bridge->synapses[i].synapse.weight;
                    error_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (prediction_count > 0) {
        bridge->inference_state.prediction_accuracy = prediction_sum / prediction_count;
    }
    if (precision_count > 0) {
        bridge->inference_state.precision_calibration = precision_sum / precision_count;
    }
    if (belief_count > 0) {
        bridge->inference_state.belief_stability = belief_sum / belief_count;
    }

    /* Calculate free energy trend from error synapses */
    float current_free_energy = 0.5f;
    if (error_count > 0) {
        current_free_energy = error_sum / error_count;
    }

    bridge->inference_state.free_energy_trend = current_free_energy - bridge->prev_free_energy;

    /* Copy callback ptr and data under lock, invoke outside lock to prevent deadlock */
    fep_plasticity_inference_callback_t inf_cb = NULL;
    void* inf_cb_data = NULL;
    float prev_fe = bridge->prev_free_energy;
    if (bridge->inference_callback && fabsf(bridge->inference_state.free_energy_trend) > 0.05f) {
        inf_cb = bridge->inference_callback;
        inf_cb_data = bridge->inference_callback_data;
    }

    bridge->prev_free_energy = current_free_energy;
    bridge->inference_state.last_learning_us = bridge->current_time_us;
    bridge->state = FEP_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke inference callback outside lock */
    if (inf_cb) {
        inf_cb(bridge, prev_fe, current_free_energy, inf_cb_data);
    }

    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int fep_plasticity_get_inference_state(
    fep_plasticity_bridge_t* bridge,
    fep_inference_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_get_inference_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_get_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->inference_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_plasticity_get_state(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_get_s", 0.0f);


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
            fep_plasticity_bridge_heartbeat("fep_plastici_loop",
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

int fep_plasticity_get_stats(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_plasticity_reset_stats(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(fep_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int fep_plasticity_register_learn_callback(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_plasticity_register_inference_callback(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_inference_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_register_inference_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->inference_callback = callback;
    bridge->inference_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int fep_plasticity_bio_async_connect(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_bio_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_plasticity_bio_async_disconnect(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_bio_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool fep_plasticity_is_bio_async_connected(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_plasticity_bridge_heartbeat("fep_plastici_fep_plasticity_is_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fep_plasticity_bridge_set_instance_health_agent(fep_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "fep_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fep_plasticity_bridge_training_begin(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    fep_plasticity_bridge_heartbeat_instance(bridge->health_agent, "fep_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int fep_plasticity_bridge_training_end(fep_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    fep_plasticity_bridge_heartbeat_instance(bridge->health_agent, "fep_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int fep_plasticity_bridge_training_step(fep_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fep_plasticity_bridge_heartbeat_instance(bridge->health_agent, "fep_plasticity_bridge_training_step", progress);
    return 0;
}
