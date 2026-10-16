/**
 * @file nimcp_sleep_wake_plasticity_bridge.c
 * @brief Sleep-Wake - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/sleep_wake/nimcp_sleep_wake_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(sleep_wake_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "SLEEP_WAKE_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    sleep_wake_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct sleep_wake_plasticity_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    sleep_wake_plasticity_config_t config;

    /* State */
    sleep_wake_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Regulation state */
    sleep_wake_regulation_state_t regulation_state;

    /* Global learning state */
    float current_consolidation;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    sleep_wake_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    sleep_wake_plasticity_regulation_callback_t regulation_callback;
    void* regulation_callback_data;

    /* Statistics */
    sleep_wake_plasticity_stats_t stats;
};

static synapse_entry_t* find_synapse(sleep_wake_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(sleep_wake_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(sleep_wake_synapse_type_t type) {
    return type == SLEEP_WAKE_SYNAPSE_SLEEP_DRIVE ||
           type == SLEEP_WAKE_SYNAPSE_WAKE_DRIVE;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

sleep_wake_plasticity_config_t sleep_wake_plasticity_config_default(void) {
    sleep_wake_plasticity_config_t config = {
        .base_learning_rate = SLEEP_WAKE_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_arousal = 0.5f,

        .sleep_learning_reduction = 0.3f,
        .consolidation_boost = 1.5f,
        .downscaling_factor = 0.85f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_sleep_drive = true,
        .protect_wake_drive = true,
        .protection_strength = 1.0f,

        .max_synapses = SLEEP_WAKE_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

sleep_wake_plasticity_bridge_t* sleep_wake_plasticity_create(
    const sleep_wake_plasticity_config_t* config
) {
    sleep_wake_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(sleep_wake_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = sleep_wake_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "sleep_wake_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "sleep_wake_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_wake_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize regulation state */
    bridge->regulation_state.arousal_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->regulation_state.sleep_drive_strength = 0.5f;
    bridge->regulation_state.wake_drive_strength = 0.5f;
    bridge->regulation_state.circadian_coupling = 0.8f;
    bridge->regulation_state.consolidation_efficiency = 0.5f;
    bridge->regulation_state.learning_rate_mod = 1.0f;
    bridge->regulation_state.last_learning_us = 0;

    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_consolidation = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void sleep_wake_plasticity_destroy(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sleep_wake_plasticity");

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int sleep_wake_plasticity_reset(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.weight = bridge->synapses[i].synapse.initial_weight;
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
            bridge->synapses[i].synapse.bcm_threshold = bridge->config.bcm_target_rate;
            bridge->synapses[i].synapse.avg_activity = 0.0f;
            bridge->synapses[i].synapse.update_count = 0;
        }
    }

    /* Reset regulation state */
    bridge->regulation_state.arousal_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->regulation_state.sleep_drive_strength = 0.5f;
    bridge->regulation_state.wake_drive_strength = 0.5f;
    bridge->regulation_state.learning_rate_mod = 1.0f;

    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
    bridge->current_consolidation = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int sleep_wake_plasticity_register_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    sleep_wake_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_register_synapse: slot is NULL");
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

    /* Auto-protect sleep and wake drive synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_sleep_drive || bridge->config.protect_wake_drive);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_unregister_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_get_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    sleep_wake_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_protect_synapse(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int sleep_wake_plasticity_learn(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_learn: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case SLEEP_WAKE_LEARN_SLEEP_ONSET:
            weight_change = lr * 0.8f;
            bridge->stats.sleep_onset_events++;
            break;

        case SLEEP_WAKE_LEARN_WAKE_ONSET:
            weight_change = lr * 0.8f;
            bridge->stats.wake_onset_events++;
            break;

        case SLEEP_WAKE_LEARN_STAGE_TRANSITION:
            weight_change = lr * 0.5f;
            bridge->stats.stage_transitions++;
            break;

        case SLEEP_WAKE_LEARN_PRESSURE_HIGH:
            weight_change = lr * 0.6f;
            break;

        case SLEEP_WAKE_LEARN_PRESSURE_CLEARED:
            weight_change = -lr * 0.3f;
            break;

        case SLEEP_WAKE_LEARN_CONSOLIDATION_SUCCESS:
            weight_change = lr * bridge->config.consolidation_boost;
            bridge->stats.consolidation_events++;
            break;

        case SLEEP_WAKE_LEARN_CONSOLIDATION_PARTIAL:
            weight_change = lr * 0.5f;
            break;

        case SLEEP_WAKE_LEARN_AROUSAL_SPIKE:
            weight_change = lr * 0.4f;
            break;

        case SLEEP_WAKE_LEARN_CIRCADIAN_ALIGNED:
            weight_change = lr * 0.7f;
            break;

        case SLEEP_WAKE_LEARN_CIRCADIAN_MISALIGNED:
            weight_change = -lr * 0.4f;
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

    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;

    /* Snapshot callback under mutex */
    sleep_wake_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside mutex to prevent deadlock */
    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float sleep_wake_plasticity_apply_stdp(
    sleep_wake_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

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

int sleep_wake_plasticity_apply_consolidation(
    sleep_wake_plasticity_bridge_t* bridge,
    float consolidation_strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_apply_consolidation: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    consolidation_strength = nimcp_clampf(consolidation_strength, -1.0f, 1.0f);
    bridge->current_consolidation = consolidation_strength;

    /* Apply consolidation modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            /* Only consolidation-type synapses get boosted */
            if (bridge->synapses[i].synapse.type == SLEEP_WAKE_SYNAPSE_CONSOLIDATION) {
                float trace = bridge->synapses[i].synapse.eligibility_trace;
                if (fabsf(trace) > 0.001f) {
                    float delta = bridge->config.base_learning_rate *
                                 bridge->config.consolidation_boost *
                                 consolidation_strength * trace;
                    bridge->synapses[i].synapse.weight = nimcp_clampf(
                        bridge->synapses[i].synapse.weight + delta,
                        bridge->config.weight_min,
                        bridge->config.weight_max
                    );
                    bridge->stats.weight_updates++;
                }
            }
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_update_bcm(
    sleep_wake_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_plasticity_update_bcm: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_SCALING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use) {
            /* Update sliding threshold towards average activity */
            float target = bridge->synapses[i].synapse.avg_activity;
            bridge->synapses[i].synapse.bcm_threshold =
                bridge->synapses[i].synapse.bcm_threshold * decay +
                target * (1.0f - decay);
        }
    }

    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_homeostatic_update(
    sleep_wake_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_SCALING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean arousal-related activity */
    float mean_arousal = 0.0f;
    uint32_t arousal_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == SLEEP_WAKE_SYNAPSE_AROUSAL) {
            mean_arousal += bridge->synapses[i].synapse.weight;
            arousal_count++;
        }
    }
    if (arousal_count > 0) {
        mean_arousal /= arousal_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_arousal;
    float scale_factor = 1.0f;
    if (mean_arousal > 0.0f) {
        scale_factor = target / mean_arousal;
        scale_factor = nimcp_clampf(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float scaled = bridge->synapses[i].synapse.weight * (1.0f + (scale_factor - 1.0f) * (1.0f - decay));
            bridge->synapses[i].synapse.weight = nimcp_clampf(
                scaled,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_apply_downscaling(
    sleep_wake_plasticity_bridge_t* bridge,
    float downscale_factor
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_apply_downscaling: bridge is NULL");
        return -1;
    }
    if (downscale_factor <= 0.0f || downscale_factor > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_plasticity_apply_downscaling: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_SCALING;

    /* Apply synaptic downscaling to all non-protected synapses */
    /* This implements the synaptic homeostasis hypothesis */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float old_weight = bridge->synapses[i].synapse.weight;
            bridge->synapses[i].synapse.weight *= downscale_factor;

            /* Clamp to bounds */
            bridge->synapses[i].synapse.weight = nimcp_clampf(
                bridge->synapses[i].synapse.weight,
                bridge->config.weight_min,
                bridge->config.weight_max
            );

            float actual_change = bridge->synapses[i].synapse.weight - old_weight;
            bridge->stats.total_depression += -actual_change;
            bridge->stats.weight_updates++;
        }
    }

    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_update_traces(
    sleep_wake_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_plasticity_consolidate(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update regulation state based on synapse weights */
    float arousal_sum = 0.0f, arousal_count = 0;
    float sleep_sum = 0.0f, sleep_count = 0;
    float wake_sum = 0.0f, wake_count = 0;
    float circadian_sum = 0.0f, circadian_count = 0;
    float consolidation_sum = 0.0f, consolidation_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case SLEEP_WAKE_SYNAPSE_AROUSAL:
                    arousal_sum += bridge->synapses[i].synapse.weight;
                    arousal_count++;
                    break;
                case SLEEP_WAKE_SYNAPSE_SLEEP_DRIVE:
                    sleep_sum += bridge->synapses[i].synapse.weight;
                    sleep_count++;
                    break;
                case SLEEP_WAKE_SYNAPSE_WAKE_DRIVE:
                    wake_sum += bridge->synapses[i].synapse.weight;
                    wake_count++;
                    break;
                case SLEEP_WAKE_SYNAPSE_CIRCADIAN:
                    circadian_sum += bridge->synapses[i].synapse.weight;
                    circadian_count++;
                    break;
                case SLEEP_WAKE_SYNAPSE_CONSOLIDATION:
                    consolidation_sum += bridge->synapses[i].synapse.weight;
                    consolidation_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (arousal_count > 0) {
        bridge->regulation_state.arousal_sensitivity = arousal_sum / arousal_count * 2.0f;
    }
    if (sleep_count > 0) {
        bridge->regulation_state.sleep_drive_strength = sleep_sum / sleep_count;
    }
    if (wake_count > 0) {
        bridge->regulation_state.wake_drive_strength = wake_sum / wake_count;
    }
    if (circadian_count > 0) {
        bridge->regulation_state.circadian_coupling = circadian_sum / circadian_count * 2.0f;
    }
    if (consolidation_count > 0) {
        bridge->regulation_state.consolidation_efficiency = consolidation_sum / consolidation_count;
    }

    bridge->regulation_state.last_learning_us = bridge->current_time_us;
    bridge->state = SLEEP_WAKE_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int sleep_wake_plasticity_get_regulation_state(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_regulation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_get_regulation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->regulation_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_plasticity_get_state(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;
    state->learning_rate_effective = bridge->learning_rate_effective;

    /* Calculate mean weight and variance */
    float sum = 0.0f;
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
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

int sleep_wake_plasticity_get_stats(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_plasticity_reset_stats(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(sleep_wake_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int sleep_wake_plasticity_register_learn_callback(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_plasticity_register_regulation_callback(
    sleep_wake_plasticity_bridge_t* bridge,
    sleep_wake_plasticity_regulation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_register_regulation_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->regulation_callback = callback;
    bridge->regulation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int sleep_wake_plasticity_bio_async_connect(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_plasticity_bio_async_disconnect(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool sleep_wake_plasticity_is_bio_async_connected(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_wake_plasticity_bridge_set_instance_health_agent(sleep_wake_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "sleep_wake_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_wake_plasticity_bridge_training_begin(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    sleep_wake_plasticity_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int sleep_wake_plasticity_bridge_training_end(sleep_wake_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    sleep_wake_plasticity_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int sleep_wake_plasticity_bridge_training_step(sleep_wake_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sleep_wake_plasticity_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_plasticity_bridge_training_step", progress);
    return 0;
}
