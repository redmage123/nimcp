/**
 * @file nimcp_ethics_plasticity_bridge.c
 * @brief Ethics Engine - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/ethics/nimcp_ethics_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(ethics_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "ETHICS_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct ethics_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    ethics_plasticity_config_t config;

    /* State */
    ethics_plasticity_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Synapses */
    ethics_plasticity_synapse_t* synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;

    /* Principle state */
    ethics_principle_state_t principles;

    /* Global modulation */
    float global_learning_rate;
    float dopamine_level;

    /* Callbacks */
    ethics_weight_change_cb weight_callback;
    void* weight_callback_data;
    ethics_principle_update_cb principle_callback;
    void* principle_callback_data;

    /* Statistics */
    ethics_plasticity_stats_t stats;
};

static ethics_plasticity_synapse_t* find_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static float compute_stdp_update(
    ethics_plasticity_bridge_t* bridge,
    int64_t dt_us)
{
    float dt_ms = (float)dt_us / 1000.0f;

    if (dt_ms > 0) {
        /* Post after pre: potentiation */
        return bridge->config.stdp_a_plus *
               expf(-dt_ms / bridge->config.stdp_tau_plus_ms);
    } else {
        /* Pre after post: depression */
        return -bridge->config.stdp_a_minus *
               expf(dt_ms / bridge->config.stdp_tau_minus_ms);
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

ethics_plasticity_config_t ethics_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_co", 0.0f);


    ethics_plasticity_config_t config = {
        .base_learning_rate = ETHICS_PLASTICITY_DEFAULT_LR,
        .stdp_tau_plus_ms = 20.0f,
        .stdp_tau_minus_ms = 20.0f,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,

        .bcm_tau_ms = 1000.0f,
        .bcm_target_rate = 0.5f,

        .homeostatic_tau_ms = 10000.0f,
        .target_sensitivity = 0.5f,

        .reward_learning_boost = 2.0f,
        .punishment_learning_boost = 1.5f,
        .dopamine_modulation = 1.0f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,

        .protect_first_law = true,
        .protect_golden_rule = true,
        .protection_strength = 0.9f,

        .max_synapses = ETHICS_PLASTICITY_MAX_SYNAPSES,

        .enable_bio_async = false
    };
    return config;
}

ethics_plasticity_bridge_t* ethics_plasticity_create(
    const ethics_plasticity_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_cr", 0.0f);


    ethics_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(ethics_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = ethics_plasticity_config_default();
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "ethics_plasticity") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "ethics_plasticity_create: validation failed");
        return NULL;
    }

    /* Allocate synapse array */
    bridge->synapse_capacity = bridge->config.max_synapses;
    bridge->synapses = nimcp_calloc(bridge->synapse_capacity,
                                    sizeof(ethics_plasticity_synapse_t));
    if (!bridge->synapses) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ethics_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize principle state */
    bridge->principles.harm_sensitivity = 0.5f;
    bridge->principles.fairness_sensitivity = 0.5f;
    bridge->principles.empathy_strength = 0.5f;
    bridge->principles.golden_rule_strength = 0.8f;
    bridge->principles.first_law_strength = 1.0f;
    bridge->principles.learning_rate_mod = 1.0f;
    bridge->principles.last_learning_us = 0;

    bridge->global_learning_rate = 1.0f;
    bridge->dopamine_level = 0.5f;
    bridge->state = ETHICS_PLASTICITY_STATE_IDLE;
    bridge->current_time_us = nimcp_time_get_us();
    bridge->bio_async_connected = false;

    return bridge;
}

void ethics_plasticity_destroy(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "ethics_plasticity");

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_de", 0.0f);


    if (bridge->synapses) nimcp_free(bridge->synapses);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int ethics_plasticity_reset(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset synapses to initial weights */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].bcm_threshold = 0.5f;
        bridge->synapses[i].avg_activity = 0.0f;
    }

    /* Reset principle state */
    bridge->principles.harm_sensitivity = 0.5f;
    bridge->principles.fairness_sensitivity = 0.5f;
    bridge->principles.empathy_strength = 0.5f;
    bridge->principles.golden_rule_strength = 0.8f;
    bridge->principles.first_law_strength = 1.0f;
    bridge->principles.learning_rate_mod = 1.0f;

    bridge->global_learning_rate = 1.0f;
    bridge->dopamine_level = 0.5f;
    bridge->state = ETHICS_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int ethics_plasticity_register_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    ethics_synapse_type_t type,
    float initial_weight)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_register_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already exists */
    if (find_synapse(bridge, synapse_id)) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Check capacity */
    if (bridge->synapse_count >= bridge->synapse_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ethics_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    /* Add synapse */
    ethics_plasticity_synapse_t* synapse = &bridge->synapses[bridge->synapse_count];
    synapse->synapse_id = synapse_id;
    synapse->type = type;
    synapse->weight = nimcp_clampf(initial_weight,
                              bridge->config.weight_min,
                              bridge->config.weight_max);
    synapse->initial_weight = synapse->weight;
    synapse->eligibility_trace = 0.0f;
    synapse->bcm_threshold = 0.5f;
    synapse->avg_activity = 0.0f;
    synapse->last_update_us = nimcp_time_get_us();
    synapse->update_count = 0;

    /* Auto-protect First Law and Golden Rule synapses */
    synapse->is_protected = false;
    if (bridge->config.protect_first_law && type == ETHICS_SYNAPSE_FIRST_LAW) {
        synapse->is_protected = true;
    }
    if (bridge->config.protect_golden_rule && type == ETHICS_SYNAPSE_GOLDEN_RULE) {
        synapse->is_protected = true;
    }

    bridge->synapse_count++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_unregister_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_un", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            /* Move last synapse to this position */
            if (i < bridge->synapse_count - 1) {
                bridge->synapses[i] = bridge->synapses[bridge->synapse_count - 1];
            }
            bridge->synapse_count--;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

int ethics_plasticity_get_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    ethics_plasticity_synapse_t* synapse)
{
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    ethics_plasticity_synapse_t* found = find_synapse(bridge, synapse_id);
    if (found) {
        *synapse = *found;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

//=============================================================================
// Learning Functions
//=============================================================================

int ethics_plasticity_learn(
    ethics_plasticity_bridge_t* bridge,
    ethics_learn_event_t event_type,
    float context_activation,
    float outcome_value,
    uint64_t timestamp)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_learn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_le", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ETHICS_PLASTICITY_STATE_LEARNING;

    if (timestamp == 0) {
        timestamp = nimcp_time_get_us();
    }

    context_activation = nimcp_clampf(context_activation, 0.0f, 1.0f);
    outcome_value = nimcp_clampf(outcome_value, -1.0f, 1.0f);

    /* Compute learning rate with modulation */
    float lr = bridge->config.base_learning_rate *
               bridge->global_learning_rate *
               bridge->principles.learning_rate_mod;

    /* Apply reward/punishment boost */
    if (outcome_value > 0) {
        lr *= bridge->config.reward_learning_boost;
        bridge->stats.positive_outcomes++;
    } else if (outcome_value < 0) {
        lr *= bridge->config.punishment_learning_boost;
        bridge->stats.negative_outcomes++;
    }

    /* Dopamine modulation */
    lr *= (0.5f + bridge->dopamine_level * bridge->config.dopamine_modulation);

    /* Update synapses based on event type */
    ethics_synapse_type_t target_type;
    switch (event_type) {
        case ETHICS_LEARN_HARM_AVOIDED:
        case ETHICS_LEARN_HARM_CAUSED:
            target_type = ETHICS_SYNAPSE_HARM_DETECTION;
            bridge->stats.harm_avoidance_events++;
            break;
        case ETHICS_LEARN_GOLDEN_RULE_APPLIED:
            target_type = ETHICS_SYNAPSE_GOLDEN_RULE;
            break;
        case ETHICS_LEARN_FIRST_LAW_ACTIVATED:
            target_type = ETHICS_SYNAPSE_FIRST_LAW;
            bridge->stats.first_law_activations++;
            break;
        case ETHICS_LEARN_CONFLICT_RESOLVED:
            target_type = ETHICS_SYNAPSE_CONFLICT;
            break;
        default:
            target_type = ETHICS_SYNAPSE_OUTCOME;
            break;
    }

    /* Update matching synapses */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        ethics_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (syn->type == target_type || target_type == ETHICS_SYNAPSE_OUTCOME) {
            /* Check protection */
            if (syn->is_protected) {
                float protected_lr = lr * (1.0f - bridge->config.protection_strength);
                if (protected_lr < 0.001f) {
                    bridge->stats.protected_updates_blocked++;
                    continue;
                }
                lr = protected_lr;
            }

            float old_weight = syn->weight;

            /* Compute weight update */
            float delta = lr * context_activation * outcome_value;

            /* Apply update */
            syn->weight += delta;
            syn->weight = nimcp_clampf(syn->weight,
                                  bridge->config.weight_min,
                                  bridge->config.weight_max);

            /* Update eligibility trace */
            syn->eligibility_trace = context_activation;

            syn->last_update_us = timestamp;
            syn->update_count++;

            bridge->stats.weight_updates++;
            bridge->stats.mean_weight_change =
                0.9f * bridge->stats.mean_weight_change +
                0.1f * fabsf(syn->weight - old_weight);

            if (delta > 0) {
                bridge->stats.total_potentiation += delta;
            } else {
                bridge->stats.total_depression += fabsf(delta);
            }

            /* Fire callback */
            if (bridge->weight_callback) {
                nimcp_mutex_unlock(bridge->base.mutex);
                bridge->weight_callback(syn->synapse_id, syn->type,
                                       old_weight, syn->weight,
                                       event_type, bridge->weight_callback_data);
                nimcp_mutex_lock(bridge->base.mutex);
            }
        }
    }

    /* Update principle state based on event */
    switch (event_type) {
        case ETHICS_LEARN_HARM_AVOIDED:
            bridge->principles.harm_sensitivity += lr * 0.1f;
            break;
        case ETHICS_LEARN_HARM_CAUSED:
            bridge->principles.harm_sensitivity += lr * 0.2f;
            break;
        case ETHICS_LEARN_GOLDEN_RULE_APPLIED:
            bridge->principles.golden_rule_strength += lr * outcome_value * 0.1f;
            break;
        case ETHICS_LEARN_FIRST_LAW_ACTIVATED:
            bridge->principles.first_law_strength =
                fmaxf(bridge->principles.first_law_strength, 0.9f);
            break;
        default:
            break;
    }

    bridge->principles.last_learning_us = timestamp;
    bridge->stats.total_learning_events++;

    bridge->state = ETHICS_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float ethics_plasticity_apply_stdp(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t pre_spike_time,
    uint64_t post_spike_time)
{
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    ethics_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (!syn) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0.0f;
    }

    /* Check protection */
    if (syn->is_protected) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.protected_updates_blocked++;
        return 0.0f;
    }

    int64_t dt = (int64_t)post_spike_time - (int64_t)pre_spike_time;
    float delta = compute_stdp_update(bridge, dt);

    float old_weight = syn->weight;
    syn->weight += delta;
    syn->weight = nimcp_clampf(syn->weight,
                          bridge->config.weight_min,
                          bridge->config.weight_max);

    syn->last_update_us = nimcp_time_get_us();
    syn->update_count++;
    bridge->stats.weight_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return syn->weight - old_weight;
}

int ethics_plasticity_apply_reward(
    ethics_plasticity_bridge_t* bridge,
    float reward)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_apply_reward: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ap", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    reward = nimcp_clampf(reward, -1.0f, 1.0f);

    /* Update dopamine level */
    bridge->dopamine_level = 0.8f * bridge->dopamine_level + 0.2f * (reward + 1.0f) / 2.0f;

    /* Apply reward-modulated update to all synapses with eligibility */
    float lr = bridge->config.base_learning_rate * bridge->global_learning_rate;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        ethics_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (syn->eligibility_trace > 0.01f && !syn->is_protected) {
            float delta = lr * syn->eligibility_trace * reward;
            syn->weight += delta;
            syn->weight = nimcp_clampf(syn->weight,
                                  bridge->config.weight_min,
                                  bridge->config.weight_max);
            bridge->stats.weight_updates++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_update_traces(
    ethics_plasticity_bridge_t* bridge,
    float dt_ms)
{
    if (!bridge || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_plasticity_update_traces: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float decay = expf(-dt_ms / bridge->config.stdp_tau_plus_ms);

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        bridge->synapses[i].eligibility_trace *= decay;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_update_bcm(
    ethics_plasticity_bridge_t* bridge,
    float dt_ms)
{
    if (!bridge || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ethics_plasticity_update_bcm: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float alpha = dt_ms / bridge->config.bcm_tau_ms;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        ethics_plasticity_synapse_t* syn = &bridge->synapses[i];

        /* Update BCM threshold towards average activity */
        syn->bcm_threshold += alpha * (syn->avg_activity - syn->bcm_threshold);
        syn->bcm_threshold = nimcp_clampf(syn->bcm_threshold, 0.1f, 0.9f);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_homeostatic_update(
    ethics_plasticity_bridge_t* bridge,
    float target_activity)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_homeostatic_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ho", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    target_activity = nimcp_clampf(target_activity, 0.0f, 1.0f);

    /* Compute current average weight */
    float mean_weight = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        mean_weight += bridge->synapses[i].weight;
    }
    if (bridge->synapse_count > 0) {
        mean_weight /= (float)bridge->synapse_count;
    }

    /* Scale weights to achieve target */
    float scale = bridge->config.target_sensitivity / (mean_weight + 0.001f);
    scale = nimcp_clampf(scale, 0.9f, 1.1f); /* Limit scaling */

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (!bridge->synapses[i].is_protected) {
            bridge->synapses[i].weight *= scale;
            bridge->synapses[i].weight = nimcp_clampf(
                bridge->synapses[i].weight,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_consolidate(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = ETHICS_PLASTICITY_STATE_CONSOLIDATING;

    /* Consolidation: strengthen strong weights, weaken weak ones */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        ethics_plasticity_synapse_t* syn = &bridge->synapses[i];

        if (!syn->is_protected) {
            float mid = (bridge->config.weight_max + bridge->config.weight_min) / 2.0f;

            if (syn->weight > mid) {
                /* Strengthen */
                syn->weight += 0.01f * (bridge->config.weight_max - syn->weight);
            } else {
                /* Weaken */
                syn->weight -= 0.01f * (syn->weight - bridge->config.weight_min);
            }

            syn->weight = nimcp_clampf(syn->weight,
                                  bridge->config.weight_min,
                                  bridge->config.weight_max);
        }

        /* Clear eligibility traces */
        syn->eligibility_trace = 0.0f;
    }

    bridge->state = ETHICS_PLASTICITY_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Principle Protection
//=============================================================================

int ethics_plasticity_protect_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_protect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    ethics_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (syn) {
        syn->is_protected = true;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

int ethics_plasticity_unprotect_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_unprotect_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_un", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    ethics_plasticity_synapse_t* syn = find_synapse(bridge, synapse_id);
    if (syn) {
        syn->is_protected = false;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return -1;  /* Synapse not found — normal condition */
}

int ethics_plasticity_protect_first_law(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_protect_first_law: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int count = 0;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].type == ETHICS_SYNAPSE_FIRST_LAW) {
            bridge->synapses[i].is_protected = true;
            count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return count;
}

int ethics_plasticity_protect_golden_rule(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_protect_golden_rule: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int count = 0;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
            ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                             (float)(i + 1) / (float)bridge->synapse_count);
        }

        if (bridge->synapses[i].type == ETHICS_SYNAPSE_GOLDEN_RULE) {
            bridge->synapses[i].is_protected = true;
            count++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return count;
}

//=============================================================================
// State Query Functions
//=============================================================================

int ethics_plasticity_get_principle_state(
    ethics_plasticity_bridge_t* bridge,
    ethics_principle_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_get_principle_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->principles;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_get_state(
    ethics_plasticity_bridge_t* bridge,
    ethics_plasticity_bridge_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_synapses = bridge->synapse_count;
    state->learning_rate_effective = bridge->config.base_learning_rate *
                                     bridge->global_learning_rate *
                                     bridge->principles.learning_rate_mod;

    /* Compute weight statistics */
    state->mean_weight = 0.0f;
    state->weight_variance = 0.0f;

    if (bridge->synapse_count > 0) {
        for (uint32_t i = 0; i < bridge->synapse_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
                ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                                 (float)(i + 1) / (float)bridge->synapse_count);
            }

            state->mean_weight += bridge->synapses[i].weight;
        }
        state->mean_weight /= (float)bridge->synapse_count;

        for (uint32_t i = 0; i < bridge->synapse_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->synapse_count > 256) {
                ethics_plasticity_bridge_heartbeat("ethics_plast_loop",
                                 (float)(i + 1) / (float)bridge->synapse_count);
            }

            float diff = bridge->synapses[i].weight - state->mean_weight;
            state->weight_variance += diff * diff;
        }
        state->weight_variance /= (float)bridge->synapse_count;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_get_stats(
    ethics_plasticity_bridge_t* bridge,
    ethics_plasticity_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_reset_stats(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(ethics_plasticity_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int ethics_plasticity_set_weight_callback(
    ethics_plasticity_bridge_t* bridge,
    ethics_weight_change_cb callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->weight_callback = callback;
    bridge->weight_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_set_principle_callback(
    ethics_plasticity_bridge_t* bridge,
    ethics_principle_update_cb callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_set_principle_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->principle_callback = callback;
    bridge->principle_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int ethics_plasticity_bio_async_connect(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_LOGGING_WARN("ethics_plasticity_bio_async_connect: bio_async not enabled in config");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int ethics_plasticity_bio_async_disconnect(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_plasticity_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool ethics_plasticity_is_bio_async_connected(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    ethics_plasticity_bridge_heartbeat("ethics_plast_ethics_plasticity_is", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_plasticity_bridge_set_instance_health_agent(ethics_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "ethics_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_plasticity_bridge_training_begin(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    ethics_plasticity_bridge_heartbeat_instance(bridge->health_agent, "ethics_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int ethics_plasticity_bridge_training_end(ethics_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    ethics_plasticity_bridge_heartbeat_instance(bridge->health_agent, "ethics_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int ethics_plasticity_bridge_training_step(ethics_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_plasticity_bridge_heartbeat_instance(bridge->health_agent, "ethics_plasticity_bridge_training_step", progress);
    return 0;
}
