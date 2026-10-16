/**
 * @file nimcp_empathy_plasticity_bridge.c
 * @brief Empathy - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/empathetic_response/nimcp_empathy_plasticity_bridge.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
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

BRIDGE_BOILERPLATE(empathy_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "EMPATHY_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct synapse_entry {
    empathy_plasticity_synapse_t synapse;
    bool in_use;
} synapse_entry_t;

struct empathy_plasticity_bridge {
    bridge_base_t base;
    empathy_plasticity_config_t config;

    /* State */
    empathy_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapse storage */
    synapse_entry_t* synapses;
    uint32_t synapse_count;
    uint32_t max_synapses;

    /* Empathic capacity state */
    empathy_capacity_state_t capacity_state;

    /* Global learning state */
    float current_reward;
    float learning_rate_effective;
    float bcm_global_threshold;

    /* Callbacks */
    empathy_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    empathy_plasticity_capacity_callback_t capacity_callback;
    void* capacity_callback_data;

    /* Statistics */
    empathy_plasticity_stats_t stats;

    nimcp_health_agent_t* health_agent;
};

//=============================================================================
// Helper Functions
//=============================================================================

static synapse_entry_t* find_synapse(empathy_plasticity_bridge_t* bridge, uint32_t synapse_id) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static synapse_entry_t* find_free_slot(empathy_plasticity_bridge_t* bridge) {
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (!bridge->synapses[i].in_use) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* All slots occupied is normal */
}

static bool is_protected_type(empathy_synapse_type_t type) {
    return type == EMPATHY_SYNAPSE_PERSPECTIVE ||
           type == EMPATHY_SYNAPSE_REGULATION;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

empathy_plasticity_config_t empathy_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_c", 0.0f);


    empathy_plasticity_config_t config = {
        .base_learning_rate = EMPATHY_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 5.0f,

        .homeostatic_tau_ms = 10000.0f,
        .target_empathy = 0.5f,

        .compassion_learning_boost = 1.5f,
        .validation_learning_boost = 1.3f,
        .mirroring_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_perspective_taking = true,
        .protect_emotion_regulation = true,
        .protection_strength = 1.0f,

        .max_synapses = EMPATHY_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

empathy_plasticity_bridge_t* empathy_plasticity_create(
    const empathy_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_c", 0.0f);


    empathy_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(empathy_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = empathy_plasticity_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "empathy_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "empathy_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse storage */
    bridge->max_synapses = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(synapse_entry_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "empathy_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize empathic capacity state */
    bridge->capacity_state.mirroring_accuracy = 1.0f;
    bridge->capacity_state.perspective_calibration = 0.5f;
    bridge->capacity_state.compassion_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->capacity_state.validation_effectiveness = 0.5f;
    bridge->capacity_state.regulation_strength = 0.5f;
    bridge->capacity_state.learning_rate_mod = 1.0f;
    bridge->capacity_state.last_learning_us = 0;

    bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->synapse_count = 0;

    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void empathy_plasticity_destroy(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "empathy_plasticity");

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_d", 0.0f);


    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int empathy_plasticity_reset(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset all synapses to initial weights */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
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

    /* Reset empathic capacity state */
    bridge->capacity_state.mirroring_accuracy = 1.0f;
    bridge->capacity_state.perspective_calibration = 0.5f;
    bridge->capacity_state.compassion_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->capacity_state.learning_rate_mod = 1.0f;

    bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
    bridge->current_reward = 0.0f;
    bridge->learning_rate_effective = bridge->config.base_learning_rate;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int empathy_plasticity_register_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    empathy_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "empathy_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Find free slot */
    synapse_entry_t* slot = find_free_slot(bridge);
    if (!slot) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_register_synapse: slot is NULL");
        return -1;
    }

    /* Initialize synapse */
    slot->in_use = true;
    slot->synapse.synapse_id = synapse_id;
    slot->synapse.type = type;
    slot->synapse.weight = nimcp_myelin_clamp(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    slot->synapse.initial_weight = slot->synapse.weight;
    slot->synapse.eligibility_trace = 0.0f;
    slot->synapse.bcm_threshold = bridge->config.bcm_target_rate;
    slot->synapse.avg_activity = 0.0f;
    slot->synapse.last_update_us = bridge->current_time_us;
    slot->synapse.update_count = 0;

    /* Auto-protect perspective and regulation synapses */
    slot->synapse.is_protected = is_protected_type(type) &&
        (bridge->config.protect_perspective_taking || bridge->config.protect_emotion_regulation);

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int empathy_plasticity_unregister_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_unregister_synapse: entry is NULL");
        return -1;
    }

    entry->in_use = false;
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int empathy_plasticity_get_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    empathy_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_get_synapse: entry is NULL");
        return -1;
    }

    *synapse = entry->synapse;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int empathy_plasticity_protect_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_p", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_protect_synapse: entry is NULL");
        return -1;
    }

    entry->synapse.is_protected = protect;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int empathy_plasticity_learn(
    empathy_plasticity_bridge_t* bridge,
    empathy_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_l", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = EMPATHY_PLASTICITY_STATE_LEARNING;

    synapse_entry_t* entry = find_synapse(bridge, synapse_id);
    if (!entry) {
        bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_learn: entry is NULL");
        return -1;
    }

    /* Check protection */
    if (entry->synapse.is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0; /* Not an error, just blocked */
    }

    float lr = bridge->learning_rate_effective * magnitude;
    float weight_change = 0.0f;

    switch (event) {
        case EMPATHY_LEARN_MIRRORING_ACCURATE:
            weight_change = lr * bridge->config.mirroring_modulation;
            bridge->stats.mirroring_accurate_events++;
            break;

        case EMPATHY_LEARN_MIRRORING_ERROR:
            weight_change = -lr * 0.5f;
            bridge->stats.mirroring_error_events++;
            break;

        case EMPATHY_LEARN_PERSPECTIVE_SUCCESS:
            weight_change = lr * 1.0f;
            break;

        case EMPATHY_LEARN_PERSPECTIVE_FAILURE:
            weight_change = -lr * 0.3f;
            break;

        case EMPATHY_LEARN_COMPASSION_EFFECTIVE:
            weight_change = lr * bridge->config.compassion_learning_boost;
            bridge->stats.compassion_effective_events++;
            break;

        case EMPATHY_LEARN_COMPASSION_INEFFECTIVE:
            weight_change = -lr * 0.2f;
            break;

        case EMPATHY_LEARN_VALIDATION_ACCEPTED:
            weight_change = lr * bridge->config.validation_learning_boost;
            bridge->stats.validation_accepted_events++;
            break;

        case EMPATHY_LEARN_VALIDATION_REJECTED:
            weight_change = -lr * 0.3f;
            break;

        case EMPATHY_LEARN_DEESCALATION_SUCCESS:
            weight_change = lr * 1.2f;
            break;

        case EMPATHY_LEARN_BOUNDARY_RESPECTED:
            weight_change = lr * 0.8f;
            break;
    }

    /* Context modulation */
    weight_change *= (0.5f + 0.5f * context);

    /* Apply weight change */
    float old_weight = entry->synapse.weight;
    entry->synapse.weight = nimcp_myelin_clamp(
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

    /* Invoke callback */
    if (bridge->learn_callback) {
        bridge->learn_callback(bridge, event, magnitude, bridge->learn_callback_data);
    }

    bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float empathy_plasticity_apply_stdp(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_a", 0.0f);


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
    entry->synapse.weight = nimcp_myelin_clamp(
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

int empathy_plasticity_apply_reward(
    empathy_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_myelin_clamp(reward, -1.0f, 1.0f);
    bridge->current_reward = reward;

    /* Apply reward modulation to all eligible synapses */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float trace = bridge->synapses[i].synapse.eligibility_trace;
            if (fabsf(trace) > 0.001f) {
                float delta = bridge->config.base_learning_rate * reward * trace;
                bridge->synapses[i].synapse.weight = nimcp_myelin_clamp(
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

int empathy_plasticity_update_bcm(
    empathy_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "empathy_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = EMPATHY_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
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

    bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int empathy_plasticity_homeostatic_update(
    empathy_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_h", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = EMPATHY_PLASTICITY_STATE_UPDATING;

    float decay = expf(-dt_ms / bridge->config.homeostatic_tau_ms);

    /* Calculate mean empathy-related activity */
    float mean_empathy = 0.0f;
    uint32_t empathy_count = 0;
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use &&
            bridge->synapses[i].synapse.type == EMPATHY_SYNAPSE_COMPASSION) {
            mean_empathy += bridge->synapses[i].synapse.weight;
            empathy_count++;
        }
    }
    if (empathy_count > 0) {
        mean_empathy /= empathy_count;
    }

    /* Scale non-protected synapses toward target */
    float target = bridge->config.target_empathy;
    float scale_factor = 1.0f;
    if (mean_empathy > 0.0f) {
        scale_factor = target / mean_empathy;
        scale_factor = nimcp_myelin_clamp(scale_factor, 0.9f, 1.1f);
    }

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use && !bridge->synapses[i].synapse.is_protected) {
            float scaled = bridge->synapses[i].synapse.weight * (1.0f + (scale_factor - 1.0f) * (1.0f - decay));
            bridge->synapses[i].synapse.weight = nimcp_myelin_clamp(
                scaled,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int empathy_plasticity_update_traces(
    empathy_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_u", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace *= decay;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int empathy_plasticity_consolidate(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = EMPATHY_PLASTICITY_STATE_CONSOLIDATING;

    /* Clear eligibility traces */
    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            bridge->synapses[i].synapse.eligibility_trace = 0.0f;
        }
    }

    /* Update empathic capacity state based on synapse weights */
    float mirroring_sum = 0.0f, mirroring_count = 0;
    float compassion_sum = 0.0f, compassion_count = 0;
    float validation_sum = 0.0f, validation_count = 0;
    float regulation_sum = 0.0f, regulation_count = 0;

    for (uint32_t i = 0; i < bridge->max_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_synapses > 256) {
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
                             (float)(i + 1) / (float)bridge->max_synapses);
        }

        if (bridge->synapses[i].in_use) {
            switch (bridge->synapses[i].synapse.type) {
                case EMPATHY_SYNAPSE_MIRRORING:
                    mirroring_sum += bridge->synapses[i].synapse.weight;
                    mirroring_count++;
                    break;
                case EMPATHY_SYNAPSE_COMPASSION:
                    compassion_sum += bridge->synapses[i].synapse.weight;
                    compassion_count++;
                    break;
                case EMPATHY_SYNAPSE_VALIDATION:
                    validation_sum += bridge->synapses[i].synapse.weight;
                    validation_count++;
                    break;
                case EMPATHY_SYNAPSE_REGULATION:
                    regulation_sum += bridge->synapses[i].synapse.weight;
                    regulation_count++;
                    break;
                default:
                    break;
            }
        }
    }

    if (mirroring_count > 0) {
        bridge->capacity_state.mirroring_accuracy = mirroring_sum / mirroring_count * 2.0f;
    }
    if (compassion_count > 0) {
        bridge->capacity_state.compassion_sensitivity = compassion_sum / compassion_count * 2.0f;
    }
    if (validation_count > 0) {
        bridge->capacity_state.validation_effectiveness = validation_sum / validation_count;
    }
    if (regulation_count > 0) {
        bridge->capacity_state.regulation_strength = regulation_sum / regulation_count;
    }

    bridge->capacity_state.last_learning_us = bridge->current_time_us;
    bridge->state = EMPATHY_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

int empathy_plasticity_get_capacity_state(
    empathy_plasticity_bridge_t* bridge,
    empathy_capacity_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_get_capacity_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->capacity_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int empathy_plasticity_get_state(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_g", 0.0f);


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
            empathy_plasticity_bridge_heartbeat("empathy_plas_loop",
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

int empathy_plasticity_get_stats(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int empathy_plasticity_reset_stats(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(empathy_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int empathy_plasticity_register_learn_callback(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int empathy_plasticity_register_capacity_callback(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_capacity_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_register_capacity_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->capacity_callback = callback;
    bridge->capacity_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int empathy_plasticity_bio_async_connect(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int empathy_plasticity_bio_async_disconnect(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "empathy_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool empathy_plasticity_is_bio_async_connected(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    empathy_plasticity_bridge_heartbeat("empathy_plas_empathy_plasticity_i", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

void empathy_plasticity_bridge_set_instance_health_agent(empathy_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "empathy_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

int empathy_plasticity_bridge_training_begin(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "empathy_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    empathy_plasticity_bridge_heartbeat_instance(bridge->health_agent, "empathy_plasticity_training_begin", 0.0f);
    return 0;
}

int empathy_plasticity_bridge_training_end(empathy_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "empathy_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    empathy_plasticity_bridge_heartbeat_instance(bridge->health_agent, "empathy_plasticity_training_end", 1.0f);
    return 0;
}

int empathy_plasticity_bridge_training_step(empathy_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "empathy_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    empathy_plasticity_bridge_heartbeat_instance(bridge->health_agent, "empathy_plasticity_training_step", progress);
    return 0;
}
