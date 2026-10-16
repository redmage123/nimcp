/**
 * @file nimcp_salience_plasticity_bridge.c
 * @brief Salience - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Implementation of plasticity bridge for salience-based attention
 * WHY:  Enable learning-based attention improvement through STDP
 * HOW:  Track attention events for spike-timing dependent synaptic changes
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/salience/nimcp_salience_plasticity_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(salience_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SALIENCE_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct salience_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    salience_plasticity_config_t config;
    salience_plasticity_state_t state;

    // Synapses
    salience_plasticity_synapse_t* synapses;
    uint32_t num_synapses;
    uint32_t max_synapses;

    // Per-feature learning state
    salience_feature_learning_t* features;
    uint32_t num_features;
    uint32_t max_features;

    // Global state
    float global_learning_rate;
    float current_attention_level;
    float bcm_global_threshold;

    // Statistics
    salience_plasticity_stats_t stats;

    // Callbacks
    salience_weight_change_cb weight_callback;
    void* weight_callback_data;
    salience_habituation_cb habituation_callback;
    void* habituation_callback_data;

    // Bio-async
    bool bio_async_connected;

    // Simulation time
    uint64_t sim_time_us;
};

static salience_plasticity_synapse_t* find_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static salience_feature_learning_t* find_feature(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
) {
    for (uint32_t i = 0; i < bridge->num_features; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_features > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_features);
        }

        if (bridge->features[i].feature_index == feature_index) {
            return &bridge->features[i];
        }
    }
    return NULL;
}

static void apply_weight_bounds(
    salience_plasticity_bridge_t* bridge,
    salience_plasticity_synapse_t* synapse
) {
    synapse->weight = nimcp_clampf(synapse->weight, bridge->config.weight_min, bridge->config.weight_max);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

salience_plasticity_config_t salience_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_plasticity_config_t config = {
        .stdp_ltp_window_ms = SALIENCE_PLASTICITY_STDP_WINDOW,
        .stdp_ltd_window_ms = SALIENCE_PLASTICITY_STDP_WINDOW,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = NIMCP_STDP_TAU_MINUS_MS,

        .enable_habituation = true,
        .habituation_rate = 0.05f,  // Increased for faster habituation buildup
        .dishabituation_boost = 0.3f,

        .enable_value_learning = true,
        .value_ltp_gain = 0.1f,  // Increased for faster value learning
        .value_decay_rate = 0.001f,

        .enable_novelty_seeking = true,
        .novelty_bonus = 0.1f,
        .exploration_drive = 0.2f,

        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 100.0f,

        .enable_homeostatic = true,
        .target_attention_level = 0.5f,
        .homeostatic_tau_ms = 10000.0f,

        .enable_eligibility = true,
        .eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .reward_modulation_gain = 2.0f,

        .weight_min = 0.0f,
        .weight_max = 2.0f,
        .initial_weight = NIMCP_SYNAPSE_WEIGHT_INIT,

        .enable_bio_async = false
    };
    return config;
}

salience_plasticity_bridge_t* salience_plasticity_create(
    const salience_plasticity_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(salience_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->config = *config;
    bridge->state = SALIENCE_PLASTICITY_STATE_IDLE;
    bridge->max_synapses = SALIENCE_PLASTICITY_MAX_SYNAPSES;
    bridge->max_features = SALIENCE_PLASTICITY_MAX_FEATURES;

    // Allocate synapses
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(salience_plasticity_synapse_t));
    if (!bridge->synapses) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    // Allocate feature learning states
    bridge->features = nimcp_calloc(bridge->max_features, sizeof(salience_feature_learning_t));
    if (!bridge->features) {
        nimcp_free(bridge->synapses);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_plasticity_create: bridge->features is NULL");
        return NULL;
    }

    bridge->global_learning_rate = 1.0f;
    bridge->current_attention_level = 0.5f;
    bridge->bcm_global_threshold = 0.5f;

    return bridge;
}

void salience_plasticity_destroy(salience_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "salience_plasticity");

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    nimcp_free(bridge->synapses);
    nimcp_free(bridge->features);
    nimcp_free(bridge);
    bridge = NULL;
}

int salience_plasticity_reset(salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->state = SALIENCE_PLASTICITY_STATE_IDLE;
    bridge->num_synapses = 0;
    bridge->num_features = 0;
    bridge->global_learning_rate = 1.0f;
    bridge->current_attention_level = 0.5f;
    bridge->bcm_global_threshold = 0.5f;
    bridge->sim_time_us = 0;

    memset(bridge->synapses, 0, bridge->max_synapses * sizeof(salience_plasticity_synapse_t));
    memset(bridge->features, 0, bridge->max_features * sizeof(salience_feature_learning_t));
    memset(&bridge->stats, 0, sizeof(salience_plasticity_stats_t));

    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int salience_plasticity_register_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    salience_synapse_type_t type,
    uint32_t feature_index,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_register_synapse: bridge is NULL");
        return -1;
    }
    if (bridge->num_synapses >= bridge->max_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "salience_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    if (find_synapse(bridge, synapse_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "salience_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_plasticity_synapse_t* synapse = &bridge->synapses[bridge->num_synapses];
    synapse->synapse_id = synapse_id;
    synapse->type = type;
    synapse->feature_index = feature_index;
    synapse->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    synapse->initial_weight = synapse->weight;
    synapse->eligibility_trace = 0.0f;
    synapse->bcm_threshold = bridge->bcm_global_threshold;
    synapse->avg_activity = 0.0f;
    synapse->habituation_level = 0.0f;
    synapse->value_estimate = 0.0f;

    bridge->num_synapses++;
    return 0;
}

int salience_plasticity_unregister_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            if (i < bridge->num_synapses - 1) {
                bridge->synapses[i] = bridge->synapses[bridge->num_synapses - 1];
            }
            bridge->num_synapses--;
            return 0;
        }
    }

    return -1;  /* Synapse not found — normal condition */
}

int salience_plasticity_get_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    salience_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_plasticity_synapse_t* found = find_synapse(bridge, synapse_id);
    if (!found) {
        return -1;  /* Synapse not found — normal condition */
    }

    *synapse = *found;
    return 0;
}

//=============================================================================
// Event Recording
//=============================================================================

int salience_plasticity_attention_event(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    float attention_strength,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_attention_event: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->state = SALIENCE_PLASTICITY_STATE_ATTENDING;
    bridge->current_attention_level = nimcp_clampf(attention_strength, 0.0f, 1.0f);

    // Find or create feature learning state
    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (!feature && bridge->num_features < bridge->max_features) {
        feature = &bridge->features[bridge->num_features++];
        feature->feature_index = feature_index;
        feature->learned_salience = 0.5f;
        feature->habituation_level = 0.0f;
        feature->value_estimate = 0.0f;
    }

    if (feature) {
        feature->exposure_count++;
        feature->last_exposure_time_us = timestamp_us;

        // Habituation
        if (bridge->config.enable_habituation) {
            float old_hab = feature->habituation_level;
            feature->habituation_level += bridge->config.habituation_rate;
            feature->habituation_level = nimcp_clampf(feature->habituation_level, 0.0f, 1.0f);

            if (bridge->habituation_callback) {
                bridge->habituation_callback(feature_index, old_hab, feature->habituation_level,
                                            bridge->habituation_callback_data);
            }
            bridge->stats.habituation_events++;
        }
    }

    // Update eligibility traces
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].feature_index == feature_index) {
            bridge->synapses[i].last_pre_spike_us = timestamp_us;
            if (bridge->config.enable_eligibility) {
                bridge->synapses[i].eligibility_trace += attention_strength;
                bridge->synapses[i].eligibility_trace =
                    nimcp_clampf(bridge->synapses[i].eligibility_trace, 0.0f, 1.0f);
            }
        }
    }

    bridge->stats.total_attention_events++;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

int salience_plasticity_attention_feedback(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    bool was_correct,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_attention_feedback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->state = SALIENCE_PLASTICITY_STATE_UPDATING;

    // Update feature learning state
    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (feature) {
        if (was_correct) {
            feature->learned_salience += 0.05f;
            feature->learned_salience = nimcp_clampf(feature->learned_salience, 0.0f, 1.0f);
        } else {
            feature->learned_salience -= 0.02f;
            feature->learned_salience = nimcp_clampf(feature->learned_salience, 0.0f, 1.0f);
        }
    }

    // Update synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].feature_index == feature_index) {
            float old_weight = bridge->synapses[i].weight;
            float dw = 0.0f;

            if (was_correct) {
                dw = bridge->config.stdp_a_plus * bridge->global_learning_rate;
                bridge->synapses[i].attention_count++;
                bridge->stats.ltp_events++;
                bridge->stats.correct_attention++;
            } else {
                dw = -bridge->config.stdp_a_minus * bridge->global_learning_rate;
                bridge->stats.ltd_events++;
                bridge->stats.incorrect_attention++;
            }

            if (bridge->config.enable_eligibility) {
                dw *= (1.0f + bridge->synapses[i].eligibility_trace);
            }

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                salience_learn_event_t event = was_correct ?
                    SALIENCE_LEARN_ATTENTION_CORRECT : SALIENCE_LEARN_ATTENTION_INCORRECT;
                bridge->weight_callback(bridge->synapses[i].synapse_id, feature_index,
                                       old_weight, bridge->synapses[i].weight,
                                       event, bridge->weight_callback_data);
            }
        }
    }

    bridge->sim_time_us = timestamp_us;
    return 0;
}

int salience_plasticity_feature_exposure(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    float intensity,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_feature_exposure: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (!feature && bridge->num_features < bridge->max_features) {
        feature = &bridge->features[bridge->num_features++];
        feature->feature_index = feature_index;
        feature->learned_salience = 0.5f;
        feature->habituation_level = 0.0f;
        feature->value_estimate = 0.0f;
    }

    if (feature) {
        feature->exposure_count++;
        feature->last_exposure_time_us = timestamp_us;

        // Accumulate habituation with each exposure
        if (bridge->config.enable_habituation) {
            float old_hab = feature->habituation_level;
            feature->habituation_level += bridge->config.habituation_rate * intensity;
            feature->habituation_level = nimcp_clampf(feature->habituation_level, 0.0f, 1.0f);

            if (bridge->habituation_callback && feature->habituation_level != old_hab) {
                bridge->habituation_callback(feature_index, old_hab, feature->habituation_level,
                                            bridge->habituation_callback_data);
            }
            bridge->stats.habituation_events++;
        }

        // Check for dishabituation (strong stimulus after significant habituation)
        if (bridge->config.enable_habituation && feature->habituation_level > 0.5f && intensity > 0.8f) {
            float old_hab = feature->habituation_level;
            feature->habituation_level *= (1.0f - bridge->config.dishabituation_boost);
            feature->habituation_level = nimcp_clampf(feature->habituation_level, 0.0f, 1.0f);

            if (bridge->habituation_callback) {
                bridge->habituation_callback(feature_index, old_hab, feature->habituation_level,
                                            bridge->habituation_callback_data);
            }
            bridge->stats.dishabituation_events++;

            // Dishabituation strengthens synapses
            for (uint32_t i = 0; i < bridge->num_synapses; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                    salience_plasticity_bridge_heartbeat("salience_pla_loop",
                                     (float)(i + 1) / (float)bridge->num_synapses);
                }

                if (bridge->synapses[i].feature_index == feature_index &&
                    bridge->synapses[i].type == SALIENCE_SYNAPSE_HABITUATION) {
                    float old_weight = bridge->synapses[i].weight;
                    float dw = bridge->config.dishabituation_boost * bridge->global_learning_rate;
                    bridge->synapses[i].weight += dw;
                    apply_weight_bounds(bridge, &bridge->synapses[i]);

                    if (bridge->weight_callback) {
                        bridge->weight_callback(bridge->synapses[i].synapse_id, feature_index,
                                               old_weight, bridge->synapses[i].weight,
                                               SALIENCE_LEARN_DISHABITUATION, bridge->weight_callback_data);
                    }
                }
            }
        }
    }

    bridge->sim_time_us = timestamp_us;
    return 0;
}

int salience_plasticity_novelty_response(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    float novelty_level,
    bool rewarded,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_novelty_response: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_novelty_seeking) return 0;

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->state = SALIENCE_PLASTICITY_STATE_UPDATING;

    // Update novelty synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].type == SALIENCE_SYNAPSE_NOVELTY) {
            float old_weight = bridge->synapses[i].weight;
            float dw = 0.0f;

            if (rewarded) {
                dw = bridge->config.novelty_bonus * novelty_level * bridge->global_learning_rate;
                bridge->stats.ltp_events++;
            } else {
                dw = -bridge->config.novelty_bonus * 0.5f * novelty_level * bridge->global_learning_rate;
                bridge->stats.ltd_events++;
            }

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, feature_index,
                                       old_weight, bridge->synapses[i].weight,
                                       SALIENCE_LEARN_NOVELTY_REWARDED, bridge->weight_callback_data);
            }
        }
    }

    bridge->sim_time_us = timestamp_us;
    return 0;
}

int salience_plasticity_reward(
    salience_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_reward: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_eligibility) return 0;

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->state = SALIENCE_PLASTICITY_STATE_UPDATING;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].eligibility_trace > 0.01f) {
            float old_weight = bridge->synapses[i].weight;
            float dw = reward * bridge->synapses[i].eligibility_trace *
                      bridge->config.reward_modulation_gain * bridge->global_learning_rate;

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);
            bridge->synapses[i].reward_count++;

            if (dw > 0) {
                bridge->stats.ltp_events++;
            } else {
                bridge->stats.ltd_events++;
            }

            // Update value estimate on synapse
            if (bridge->config.enable_value_learning) {
                bridge->synapses[i].value_estimate += reward * bridge->config.value_ltp_gain;
                bridge->synapses[i].value_estimate =
                    nimcp_clampf(bridge->synapses[i].value_estimate, -1.0f, 1.0f);

                // Also update feature value estimate
                salience_feature_learning_t* feature = find_feature(bridge, bridge->synapses[i].feature_index);
                if (feature) {
                    feature->value_estimate += reward * bridge->config.value_ltp_gain;
                    feature->value_estimate = nimcp_clampf(feature->value_estimate, -1.0f, 1.0f);
                }
            }

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, 0,
                                       old_weight, bridge->synapses[i].weight,
                                       SALIENCE_LEARN_REWARD, bridge->weight_callback_data);
            }
        }
    }

    bridge->stats.total_reward += reward;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int salience_plasticity_update(
    salience_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_update: bridge is NULL");
        return -1;
    }

    // Decay eligibility traces
    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    if (bridge->config.enable_eligibility) {
        float decay = powf(bridge->config.eligibility_decay, dt_ms);
        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                salience_plasticity_bridge_heartbeat("salience_pla_loop",
                                 (float)(i + 1) / (float)bridge->num_synapses);
            }

            bridge->synapses[i].eligibility_trace *= decay;
        }
    }

    // Decay habituation slowly
    if (bridge->config.enable_habituation) {
        float hab_decay = expf(-dt_ms / 10000.0f);  // 10 second time constant
        for (uint32_t i = 0; i < bridge->num_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_features > 256) {
                salience_plasticity_bridge_heartbeat("salience_pla_loop",
                                 (float)(i + 1) / (float)bridge->num_features);
            }

            bridge->features[i].habituation_level *= hab_decay;
        }

        float total_hab = 0.0f;
        for (uint32_t i = 0; i < bridge->num_features; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_features > 256) {
                salience_plasticity_bridge_heartbeat("salience_pla_loop",
                                 (float)(i + 1) / (float)bridge->num_features);
            }

            total_hab += bridge->features[i].habituation_level;
        }
        if (bridge->num_features > 0) {
            bridge->stats.mean_habituation = total_hab / bridge->num_features;
        }
    }

    // Decay value estimates
    if (bridge->config.enable_value_learning) {
        float value_decay = expf(-dt_ms * bridge->config.value_decay_rate);
        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                salience_plasticity_bridge_heartbeat("salience_pla_loop",
                                 (float)(i + 1) / (float)bridge->num_synapses);
            }

            bridge->synapses[i].value_estimate *= value_decay;
        }
    }

    // BCM threshold update
    if (bridge->config.enable_bcm) {
        float threshold_decay = (fabsf(bridge->config.bcm_threshold_tau) > 1e-10f) ?
            expf(-dt_ms / bridge->config.bcm_threshold_tau) : 0.0f;
        float activity_decay = (fabsf(bridge->config.bcm_activity_tau) > 1e-10f) ?
            expf(-dt_ms / bridge->config.bcm_activity_tau) : 0.0f;

        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                salience_plasticity_bridge_heartbeat("salience_pla_loop",
                                 (float)(i + 1) / (float)bridge->num_synapses);
            }

            bridge->synapses[i].avg_activity =
                bridge->synapses[i].avg_activity * activity_decay +
                bridge->synapses[i].weight * (1.0f - activity_decay);

            float target = bridge->synapses[i].avg_activity * bridge->synapses[i].avg_activity;
            bridge->synapses[i].bcm_threshold =
                bridge->synapses[i].bcm_threshold * threshold_decay +
                target * (1.0f - threshold_decay);
        }
    }

    // Homeostatic regulation
    if (bridge->config.enable_homeostatic) {
        float homeo_rate = (fabsf(bridge->config.homeostatic_tau_ms) > 1e-10f) ?
            (dt_ms / bridge->config.homeostatic_tau_ms) : 0.0f;
        float attention_error = bridge->config.target_attention_level - bridge->current_attention_level;

        bridge->global_learning_rate += attention_error * homeo_rate;
        bridge->global_learning_rate = nimcp_clampf(bridge->global_learning_rate, 0.1f, 2.0f);
    }

    bridge->sim_time_us += (uint64_t)(dt_ms * 1000.0f);

    return 0;
}

int salience_plasticity_consolidate(salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->state = SALIENCE_PLASTICITY_STATE_CONSOLIDATING;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            salience_plasticity_bridge_heartbeat("salience_pla_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        // Consolidation based on consistent attention and rewards
        if (bridge->synapses[i].attention_count > 10 && bridge->synapses[i].reward_count > 5) {
            float success_rate = (float)bridge->synapses[i].reward_count /
                                bridge->synapses[i].attention_count;

            if (success_rate > 0.5f) {
                // Move weight toward the middle (more stable)
                float target = (bridge->config.weight_min + bridge->config.weight_max) / 2.0f;
                bridge->synapses[i].weight =
                    bridge->synapses[i].weight * 0.9f + target * 0.1f * success_rate;
            }
        }
    }

    bridge->state = SALIENCE_PLASTICITY_STATE_IDLE;
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float salience_plasticity_get_learned_salience(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
) {
    if (!bridge) return 0.5f;

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (!feature) return 0.5f;

    return feature->learned_salience;
}

float salience_plasticity_get_habituation(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (!feature) return 0.0f;

    return feature->habituation_level;
}

float salience_plasticity_get_value_estimate(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (!feature) return 0.0f;

    return feature->value_estimate;
}

int salience_plasticity_get_feature_learning(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    salience_feature_learning_t* learning
) {
    if (!bridge || !learning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_get_feature_learning: required parameter is NULL (bridge, learning)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    salience_feature_learning_t* feature = find_feature(bridge, feature_index);
    if (!feature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_get_feature_learning: feature is NULL");
        return -1;
    }

    *learning = *feature;
    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int salience_plasticity_get_state(
    const salience_plasticity_bridge_t* bridge,
    salience_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    state->state = bridge->state;
    state->registered_synapses = bridge->num_synapses;
    state->tracked_features = bridge->num_features;
    state->global_learning_rate = bridge->global_learning_rate;
    state->current_attention_level = bridge->current_attention_level;
    state->bio_async_connected = bridge->bio_async_connected;

    return 0;
}

int salience_plasticity_get_stats(
    const salience_plasticity_bridge_t* bridge,
    salience_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    return 0;
}

void salience_plasticity_reset_stats(salience_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    memset(&bridge->stats, 0, sizeof(salience_plasticity_stats_t));
}

//=============================================================================
// Callbacks
//=============================================================================

int salience_plasticity_set_weight_callback(
    salience_plasticity_bridge_t* bridge,
    salience_weight_change_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->weight_callback = callback;
    bridge->weight_callback_data = user_data;
    return 0;
}

int salience_plasticity_set_habituation_callback(
    salience_plasticity_bridge_t* bridge,
    salience_habituation_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_set_habituation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->habituation_callback = callback;
    bridge->habituation_callback_data = user_data;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int salience_plasticity_connect_bio_async(salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_connect_bio_async: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int salience_plasticity_disconnect_bio_async(salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_plasticity_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool salience_plasticity_is_bio_async_connected(const salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_plasticity_bridge_heartbeat("salience_pla_salience_plasticity_", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_plasticity_bridge_set_instance_health_agent(salience_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "salience_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_plasticity_bridge_training_begin(salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    salience_plasticity_bridge_heartbeat_instance(bridge->health_agent, "salience_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int salience_plasticity_bridge_training_end(salience_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    salience_plasticity_bridge_heartbeat_instance(bridge->health_agent, "salience_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int salience_plasticity_bridge_training_step(salience_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_plasticity_bridge_heartbeat_instance(bridge->health_agent, "salience_plasticity_bridge_training_step", progress);
    return 0;
}
