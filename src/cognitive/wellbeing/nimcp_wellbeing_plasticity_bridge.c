/**
 * @file nimcp_wellbeing_plasticity_bridge.c
 * @brief Wellbeing - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/wellbeing/nimcp_wellbeing_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(wellbeing_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "WELLBEING_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct wellbeing_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    wellbeing_plasticity_config_t config;

    /* State */
    wellbeing_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapses */
    wellbeing_plasticity_synapse_t* synapses;
    uint32_t synapse_count;

    /* Foundation state */
    wellbeing_foundation_state_t foundation;

    /* Global modulation */
    float reward_accumulator;
    float homeostatic_factor;
    float bcm_global_threshold;

    /* Callbacks */
    wellbeing_plasticity_learn_callback_t learn_callback;
    void* learn_callback_data;
    wellbeing_plasticity_foundation_callback_t foundation_callback;
    void* foundation_callback_data;

    /* Statistics */
    wellbeing_plasticity_stats_t stats;
};

static wellbeing_plasticity_synapse_t* find_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static bool is_protected_type(wellbeing_synapse_type_t type) {
    return (type == WELLBEING_SYNAPSE_RESILIENCE);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

wellbeing_plasticity_config_t wellbeing_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    wellbeing_plasticity_config_t config = {
        .base_learning_rate = WELLBEING_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 0.5f,

        .homeostatic_tau_ms = 5000.0f,
        .target_wellbeing = 0.6f,

        .positive_learning_boost = 1.5f,
        .negative_learning_rate = 0.8f,
        .social_modulation = 1.2f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_resilience = true,
        .protect_flourishing = true,
        .protection_strength = 0.95f,

        .max_synapses = WELLBEING_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

wellbeing_plasticity_bridge_t* wellbeing_plasticity_create(
    const wellbeing_plasticity_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    wellbeing_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(wellbeing_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = wellbeing_plasticity_config_default();
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "wellbeing_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "wellbeing_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse array */
    bridge->synapses = nimcp_calloc(bridge->config.max_synapses,
                                    sizeof(wellbeing_plasticity_synapse_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wellbeing_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize foundation state */
    bridge->foundation.hedonic_sensitivity = 0.5f;
    bridge->foundation.eudaimonic_strength = 0.5f;
    bridge->foundation.vitality_capacity = 0.5f;
    bridge->foundation.resilience_level = 0.5f;
    bridge->foundation.social_connection_strength = 0.5f;
    bridge->foundation.learning_rate_mod = 1.0f;
    bridge->foundation.last_learning_us = 0;

    bridge->state = WELLBEING_PLASTICITY_STATE_IDLE;
    bridge->synapse_count = 0;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;

    bridge->reward_accumulator = 0.0f;
    bridge->homeostatic_factor = 1.0f;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    return bridge;
}

void wellbeing_plasticity_destroy(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "wellbeing_plasticity");

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
    bridge = NULL;
}

int wellbeing_plasticity_reset(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset synapses to initial weights but keep registrations */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].bcm_threshold = bridge->config.bcm_target_rate;
        bridge->synapses[i].avg_activity = 0.0f;
        bridge->synapses[i].update_count = 0;
    }

    /* Reset foundation state */
    bridge->foundation.hedonic_sensitivity = 0.5f;
    bridge->foundation.eudaimonic_strength = 0.5f;
    bridge->foundation.vitality_capacity = 0.5f;
    bridge->foundation.resilience_level = 0.5f;
    bridge->foundation.social_connection_strength = 0.5f;
    bridge->foundation.learning_rate_mod = 1.0f;

    bridge->reward_accumulator = 0.0f;
    bridge->homeostatic_factor = 1.0f;
    bridge->bcm_global_threshold = bridge->config.bcm_target_rate;

    bridge->state = WELLBEING_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int wellbeing_plasticity_register_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    wellbeing_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check for duplicate */
    if (find_synapse(bridge, synapse_id) != NULL) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Check capacity */
    if (bridge->synapse_count >= bridge->config.max_synapses) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "wellbeing_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    /* Initialize synapse */
    wellbeing_plasticity_synapse_t* syn = &bridge->synapses[bridge->synapse_count];
    syn->synapse_id = synapse_id;
    syn->type = type;
    syn->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    syn->initial_weight = syn->weight;
    syn->eligibility_trace = 0.0f;
    syn->bcm_threshold = bridge->config.bcm_target_rate;
    syn->avg_activity = 0.0f;
    syn->last_update_us = bridge->current_time_us;
    syn->update_count = 0;

    /* Auto-protect resilience synapses if configured */
    syn->is_protected = false;
    if (bridge->config.protect_resilience && type == WELLBEING_SYNAPSE_RESILIENCE) {
        syn->is_protected = true;
    }

    bridge->synapse_count++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_plasticity_unregister_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int found_idx = -1;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    /* Shift remaining synapses */
    for (uint32_t i = found_idx; i < bridge->synapse_count - 1; i++) {
        bridge->synapses[i] = bridge->synapses[i + 1];
    }
    bridge->synapse_count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_get_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    wellbeing_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    wellbeing_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    *synapse = *syn;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_protect_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    wellbeing_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    syn->is_protected = protect;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Functions
//=============================================================================

int wellbeing_plasticity_learn(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = WELLBEING_PLASTICITY_STATE_LEARNING;

    wellbeing_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        bridge->state = WELLBEING_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1;  /* Synapse not found — normal condition */
    }

    /* Check protection */
    if (syn->is_protected) {
        bridge->stats.protected_updates_blocked++;
        bridge->state = WELLBEING_PLASTICITY_STATE_IDLE;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    float lr = bridge->config.base_learning_rate * bridge->foundation.learning_rate_mod;
    float delta = 0.0f;
    magnitude = nimcp_clampf(magnitude, 0.0f, 1.0f);
    context = nimcp_clampf(context, 0.0f, 1.0f);

    switch (event) {
        case WELLBEING_LEARN_POSITIVE_EXPERIENCE:
            delta = lr * magnitude * bridge->config.positive_learning_boost;
            bridge->stats.positive_experience_events++;
            /* Boost hedonic sensitivity */
            bridge->foundation.hedonic_sensitivity =
                nimcp_clampf(bridge->foundation.hedonic_sensitivity + delta * 0.1f, 0.0f, 1.0f);
            break;

        case WELLBEING_LEARN_NEGATIVE_EXPERIENCE:
            delta = -lr * magnitude * bridge->config.negative_learning_rate;
            bridge->stats.negative_experience_events++;
            break;

        case WELLBEING_LEARN_STRESS_RECOVERED:
            delta = lr * magnitude * 1.2f;
            bridge->stats.stress_recovery_events++;
            /* Strengthen resilience */
            bridge->foundation.resilience_level =
                nimcp_clampf(bridge->foundation.resilience_level + magnitude * 0.05f, 0.0f, 1.0f);
            break;

        case WELLBEING_LEARN_STRESS_ACCUMULATED:
            delta = -lr * magnitude * 0.8f;
            /* Slightly reduce vitality */
            bridge->foundation.vitality_capacity =
                nimcp_clampf(bridge->foundation.vitality_capacity - magnitude * 0.02f, 0.0f, 1.0f);
            break;

        case WELLBEING_LEARN_SOCIAL_SUPPORT:
            delta = lr * magnitude * bridge->config.social_modulation;
            bridge->stats.social_support_events++;
            /* Strengthen social connection */
            bridge->foundation.social_connection_strength =
                nimcp_clampf(bridge->foundation.social_connection_strength + magnitude * 0.1f, 0.0f, 1.0f);
            break;

        case WELLBEING_LEARN_MEANING_FOUND:
            delta = lr * magnitude * 1.3f;
            /* Strengthen eudaimonic foundation */
            bridge->foundation.eudaimonic_strength =
                nimcp_clampf(bridge->foundation.eudaimonic_strength + magnitude * 0.1f, 0.0f, 1.0f);
            break;

        case WELLBEING_LEARN_GOAL_ACHIEVED:
            delta = lr * magnitude * 1.4f;
            bridge->stats.positive_experience_events++;
            break;

        case WELLBEING_LEARN_BALANCE_IMPROVED:
            delta = lr * magnitude;
            break;

        default:
            break;
    }

    /* Apply context modulation */
    delta *= (0.5f + context * 0.5f);

    /* Apply eligibility trace */
    delta *= (1.0f + syn->eligibility_trace);

    /* Apply homeostatic factor */
    delta *= bridge->homeostatic_factor;

    /* Update weight */
    float old_weight = syn->weight;
    syn->weight = nimcp_clampf(syn->weight + delta, bridge->config.weight_min, bridge->config.weight_max);
    float actual_delta = syn->weight - old_weight;

    /* Update eligibility trace */
    syn->eligibility_trace = nimcp_clampf(syn->eligibility_trace + fabsf(actual_delta), 0.0f, 1.0f);

    /* Track statistics */
    syn->update_count++;
    syn->last_update_us = bridge->current_time_us;
    bridge->stats.total_learning_events++;
    bridge->stats.weight_updates++;
    bridge->stats.mean_weight_change =
        (bridge->stats.mean_weight_change * (bridge->stats.weight_updates - 1) + fabsf(actual_delta)) /
        bridge->stats.weight_updates;

    if (actual_delta > 0) {
        bridge->stats.total_potentiation += actual_delta;
    } else {
        bridge->stats.total_depression += fabsf(actual_delta);
    }

    bridge->foundation.last_learning_us = bridge->current_time_us;
    bridge->state = WELLBEING_PLASTICITY_STATE_IDLE;

    /* Invoke callback outside mutex to prevent deadlock */
    wellbeing_plasticity_learn_callback_t cb = bridge->learn_callback;
    void* cb_data = bridge->learn_callback_data;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (cb) {
        cb(bridge, event, magnitude, cb_data);
    }

    return 0;
}

float wellbeing_plasticity_apply_stdp(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
) {
    if (!bridge) return NAN;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    wellbeing_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NAN;
    }

    /* Protected synapses don't change */
    if (syn->is_protected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0.0f;
    }

    float dt = post_time - pre_time;
    float delta = 0.0f;

    if (dt > 0) {
        /* Post after pre: potentiation */
        delta = bridge->config.stdp_a_plus * expf(-dt / bridge->config.stdp_tau_plus_ms);
    } else {
        /* Pre after post: depression */
        delta = -bridge->config.stdp_a_minus * expf(dt / bridge->config.stdp_tau_minus_ms);
    }

    /* Apply learning rate and homeostatic factor */
    delta *= bridge->config.base_learning_rate * bridge->homeostatic_factor;

    /* Update weight */
    float old_weight = syn->weight;
    syn->weight = nimcp_clampf(syn->weight + delta, bridge->config.weight_min, bridge->config.weight_max);
    float actual_delta = syn->weight - old_weight;

    syn->update_count++;
    syn->last_update_us = bridge->current_time_us;
    bridge->stats.weight_updates++;

    if (actual_delta > 0) {
        bridge->stats.total_potentiation += actual_delta;
    } else {
        bridge->stats.total_depression += fabsf(actual_delta);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return actual_delta;
}

int wellbeing_plasticity_apply_reward(
    wellbeing_plasticity_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_clampf(reward, -1.0f, 1.0f);
    bridge->reward_accumulator = nimcp_clampf(bridge->reward_accumulator + reward, -1.0f, 1.0f);

    /* Apply reward to all non-protected synapses */
    float reward_factor = 1.0f + reward * 0.1f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (!bridge->synapses[i].is_protected) {
            /* Modulate eligibility traces */
            bridge->synapses[i].eligibility_trace *= reward_factor;
            bridge->synapses[i].eligibility_trace =
                nimcp_clampf(bridge->synapses[i].eligibility_trace, 0.0f, 1.0f);
        }
    }

    /* Positive rewards boost learning rate temporarily */
    if (reward > 0) {
        bridge->foundation.learning_rate_mod =
            nimcp_clampf(bridge->foundation.learning_rate_mod + reward * 0.05f, 0.5f, 2.0f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_update_bcm(
    wellbeing_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_update_bcm: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_plasticity_update_bcm: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* BCM threshold sliding based on average activity */
    float decay = expf(-dt_ms / bridge->config.bcm_tau_ms);

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        wellbeing_plasticity_synapse_t* syn = &bridge->synapses[i];

        /* Update average activity */
        syn->avg_activity = syn->avg_activity * decay + syn->weight * (1.0f - decay);

        /* Slide BCM threshold towards average */
        syn->bcm_threshold = syn->bcm_threshold * decay +
                            syn->avg_activity * syn->avg_activity * (1.0f - decay);
    }

    /* Update global BCM threshold */
    bridge->bcm_global_threshold = bridge->bcm_global_threshold * decay +
                                   bridge->config.bcm_target_rate * (1.0f - decay);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_homeostatic_update(
    wellbeing_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_plasticity_homeostatic_update: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate mean wellbeing across weights */
    float mean_weight = 0.0f;
    uint32_t unprotected_count = 0;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (!bridge->synapses[i].is_protected) {
            mean_weight += bridge->synapses[i].weight;
            unprotected_count++;
        }
    }

    if (unprotected_count > 0) {
        mean_weight /= unprotected_count;
    }

    /* Calculate homeostatic factor */
    float error = bridge->config.target_wellbeing - mean_weight;
    float correction = error * dt_ms / bridge->config.homeostatic_tau_ms;

    bridge->homeostatic_factor = nimcp_clampf(1.0f + correction, 0.5f, 2.0f);

    /* Apply slow homeostatic scaling to weights */
    float scale = 1.0f + correction * 0.01f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (!bridge->synapses[i].is_protected) {
            bridge->synapses[i].weight = nimcp_clampf(
                bridge->synapses[i].weight * scale,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_update_traces(
    wellbeing_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_update_traces: bridge is NULL");
        return -1;
    }
    if (dt_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_plasticity_update_traces: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay eligibility traces */
    float decay = expf(-dt_ms / 100.0f);  /* 100ms time constant */

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].eligibility_trace *= decay;
    }

    /* Decay reward accumulator */
    bridge->reward_accumulator *= decay;

    /* Slowly return learning rate mod towards baseline */
    bridge->foundation.learning_rate_mod =
        bridge->foundation.learning_rate_mod * 0.99f + 1.0f * 0.01f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_consolidate(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = WELLBEING_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidation: strengthen strong synapses, weaken weak ones */
    float threshold = 0.5f;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        wellbeing_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (syn->is_protected) continue;

        if (syn->weight > threshold) {
            /* Strengthen strong synapses slightly */
            syn->weight = nimcp_clampf(syn->weight * 1.01f, bridge->config.weight_min, bridge->config.weight_max);
        } else if (syn->weight < threshold * 0.5f && syn->update_count > 0) {
            /* Weaken rarely-used weak synapses */
            syn->weight = nimcp_clampf(syn->weight * 0.99f, bridge->config.weight_min, bridge->config.weight_max);
        }

        /* Clear eligibility traces during consolidation */
        syn->eligibility_trace *= 0.5f;
    }

    bridge->state = WELLBEING_PLASTICITY_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Wellbeing Protection Functions
//=============================================================================

int wellbeing_plasticity_protect_resilience(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_protect_resilience: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int protected_count = 0;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].type == WELLBEING_SYNAPSE_RESILIENCE) {
            bridge->synapses[i].is_protected = true;
            protected_count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return protected_count;
}

int wellbeing_plasticity_protect_flourishing(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_protect_flourishing: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int protected_count = 0;
    /* Protect synapses that represent core flourishing capacity */
    /* In practice, this would protect specific synapse patterns */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        /* Protect high-weight synapses as they represent established patterns */
        if (bridge->synapses[i].weight > 0.8f &&
            bridge->synapses[i].type != WELLBEING_SYNAPSE_RESILIENCE) {
            bridge->synapses[i].is_protected = true;
            protected_count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return protected_count;
}

//=============================================================================
// State Query Functions
//=============================================================================

int wellbeing_plasticity_get_foundation_state(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_foundation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_get_foundation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->foundation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_plasticity_get_state(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;
    state->learning_rate_effective = bridge->config.base_learning_rate *
                                     bridge->foundation.learning_rate_mod *
                                     bridge->homeostatic_factor;

    /* Calculate mean weight and variance */
    float mean = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        mean += bridge->synapses[i].weight;
    }
    if (bridge->synapse_count > 0) {
        mean /= bridge->synapse_count;
    }
    state->mean_weight = mean;

    float variance = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        float diff = bridge->synapses[i].weight - mean;
        variance += diff * diff;
    }
    if (bridge->synapse_count > 0) {
        variance /= bridge->synapse_count;
    }
    state->weight_variance = variance;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_plasticity_get_stats(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_plasticity_reset_stats(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(wellbeing_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int wellbeing_plasticity_register_learn_callback(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_learn_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_register_learn_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->learn_callback = callback;
    bridge->learn_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_plasticity_register_foundation_callback(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_foundation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_register_foundation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->foundation_callback = callback;
    bridge->foundation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int wellbeing_plasticity_bio_async_connect(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_plasticity_bio_async_disconnect(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool wellbeing_plasticity_is_bio_async_connected(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_plasticity_bridge_heartbeat("wellbeing_pl_wellbeing_plasticity", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void wellbeing_plasticity_bridge_set_instance_health_agent(wellbeing_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "wellbeing_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_plasticity_bridge_training_begin(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    wellbeing_plasticity_bridge_heartbeat_instance(bridge->health_agent, "wellbeing_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int wellbeing_plasticity_bridge_training_end(wellbeing_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    wellbeing_plasticity_bridge_heartbeat_instance(bridge->health_agent, "wellbeing_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int wellbeing_plasticity_bridge_training_step(wellbeing_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_plasticity_bridge_heartbeat_instance(bridge->health_agent, "wellbeing_plasticity_bridge_training_step", progress);
    return 0;
}
