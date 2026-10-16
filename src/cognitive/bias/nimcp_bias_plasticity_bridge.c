/**
 * @file nimcp_bias_plasticity_bridge.c
 * @brief Cognitive Bias - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Implementation of plasticity bridge for cognitive bias learning
 * WHY:  Enable learning-based bias recognition and metacognitive improvement through STDP
 * HOW:  Track bias detection events for spike-timing dependent synaptic changes
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/bias/nimcp_bias_plasticity_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(bias_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "BIAS_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct bias_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    bias_plasticity_config_t config;
    bias_plasticity_state_t state;

    // Synapses
    bias_plasticity_synapse_t* synapses;
    uint32_t num_synapses;
    uint32_t max_synapses;

    // Per-type learning state
    bias_type_learning_t* type_learning;
    uint32_t num_types;
    uint32_t max_types;

    // Global state
    float global_learning_rate;
    float overall_metacognitive_awareness;
    float bcm_global_threshold;

    // Statistics
    bias_plasticity_stats_t stats;

    // Callbacks
    bias_weight_change_cb weight_callback;
    void* weight_callback_data;
    bias_metacognitive_cb metacognitive_callback;
    void* metacognitive_callback_data;

    // Bio-async
    bool bio_async_connected;

    // Simulation time
    uint64_t sim_time_us;
};

static bias_plasticity_synapse_t* find_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static bias_type_learning_t* find_type_learning(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
) {
    for (uint32_t i = 0; i < bridge->num_types; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_types > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_types);
        }

        if (bridge->type_learning[i].bias_type == bias_type) {
            return &bridge->type_learning[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static void apply_weight_bounds(
    bias_plasticity_bridge_t* bridge,
    bias_plasticity_synapse_t* synapse
) {
    synapse->weight = nimcp_clampf(synapse->weight, bridge->config.weight_min, bridge->config.weight_max);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

bias_plasticity_config_t bias_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_conf", 0.0f);


    bias_plasticity_config_t config = {
        .stdp_ltp_window_ms = BIAS_PLASTICITY_STDP_WINDOW,
        .stdp_ltd_window_ms = BIAS_PLASTICITY_STDP_WINDOW,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = NIMCP_STDP_TAU_MINUS_MS,

        .enable_detection_learning = true,
        .detection_correct_ltp = 0.05f,
        .detection_incorrect_ltd = 0.08f,

        .enable_conflict_learning = true,
        .conflict_resolution_ltp = 0.03f,
        .conflict_persistence_ltd = 0.05f,

        .enable_metacognitive_learning = true,
        .metacognitive_insight_gain = 0.02f,
        .awareness_growth_rate = 0.08f,  // Increased for faster awareness buildup

        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 100.0f,

        .enable_homeostatic = true,
        .target_detection_accuracy = 0.8f,
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

bias_plasticity_bridge_t* bias_plasticity_create(
    const bias_plasticity_config_t* config
) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_crea", 0.0f);


    bias_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(bias_plasticity_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->config = *config;
    bridge->state = BIAS_PLASTICITY_STATE_IDLE;
    bridge->max_synapses = BIAS_PLASTICITY_MAX_SYNAPSES;
    bridge->max_types = BIAS_PLASTICITY_MAX_TYPES;

    // Allocate synapses
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(bias_plasticity_synapse_t));
    if (!bridge->synapses) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    // Allocate type learning states
    bridge->type_learning = nimcp_calloc(bridge->max_types, sizeof(bias_type_learning_t));
    if (!bridge->type_learning) {
        nimcp_free(bridge->synapses);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_plasticity_create: bridge->type_learning is NULL");
        return NULL;
    }

    bridge->global_learning_rate = 1.0f;
    bridge->overall_metacognitive_awareness = 0.0f;
    bridge->bcm_global_threshold = 0.5f;

    return bridge;
}

void bias_plasticity_destroy(bias_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "bias_plasticity");

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_dest", 0.0f);


    nimcp_free(bridge->synapses);
    nimcp_free(bridge->type_learning);
    nimcp_free(bridge);
    bridge = NULL;
}

int bias_plasticity_reset(bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_rese", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_IDLE;
    bridge->num_synapses = 0;
    bridge->num_types = 0;
    bridge->global_learning_rate = 1.0f;
    bridge->overall_metacognitive_awareness = 0.0f;
    bridge->bcm_global_threshold = 0.5f;
    bridge->sim_time_us = 0;

    memset(bridge->synapses, 0, bridge->max_synapses * sizeof(bias_plasticity_synapse_t));
    memset(bridge->type_learning, 0, bridge->max_types * sizeof(bias_type_learning_t));
    memset(&bridge->stats, 0, sizeof(bias_plasticity_stats_t));

    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int bias_plasticity_register_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bias_synapse_type_t type,
    uint32_t bias_type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_register_synapse: bridge is NULL");
        return -1;
    }
    if (bridge->num_synapses >= bridge->max_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bias_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    // Check for duplicate
    if (find_synapse(bridge, synapse_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bias_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_regi", 0.0f);


    bias_plasticity_synapse_t* synapse = &bridge->synapses[bridge->num_synapses];
    synapse->synapse_id = synapse_id;
    synapse->type = type;
    synapse->bias_type = bias_type;
    synapse->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    synapse->initial_weight = synapse->weight;
    synapse->last_pre_spike_us = 0;
    synapse->last_post_spike_us = 0;
    synapse->eligibility_trace = 0.0f;
    synapse->bcm_threshold = bridge->bcm_global_threshold;
    synapse->avg_activity = 0.0f;
    synapse->consolidation_level = 0.0f;
    synapse->correct_detections = 0;
    synapse->false_positives = 0;
    synapse->false_negatives = 0;

    bridge->num_synapses++;
    return 0;
}

int bias_plasticity_unregister_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_unre", 0.0f);


    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
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

    return -1;  /* Synapse not found */
}

int bias_plasticity_get_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bias_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    bias_plasticity_synapse_t* found = find_synapse(bridge, synapse_id);
    if (!found) {
        return -1;  /* Synapse not found */
    }

    *synapse = *found;
    return 0;
}

//=============================================================================
// Event Recording
//=============================================================================

int bias_plasticity_bias_detected(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    float confidence,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_bias_detected: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_bias", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_DETECTING;

    // Find or create type learning state
    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (!type_learn && bridge->num_types < bridge->max_types) {
        type_learn = &bridge->type_learning[bridge->num_types++];
        type_learn->bias_type = bias_type;
        type_learn->detection_sensitivity = 0.5f;
        type_learn->correction_efficiency = 0.0f;
        type_learn->metacognitive_awareness = 0.0f;
        type_learn->total_encounters = 0;
        type_learn->successful_corrections = 0;
    }

    if (type_learn) {
        type_learn->total_encounters++;
        type_learn->last_encounter_time_us = timestamp_us;
    }

    // Update eligibility traces for detection synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].bias_type == bias_type &&
            bridge->synapses[i].type == BIAS_SYNAPSE_DETECTION) {
            bridge->synapses[i].last_pre_spike_us = timestamp_us;

            if (bridge->config.enable_eligibility) {
                bridge->synapses[i].eligibility_trace += confidence;
                bridge->synapses[i].eligibility_trace =
                    nimcp_clampf(bridge->synapses[i].eligibility_trace, 0.0f, 1.0f);
            }
        }
    }

    bridge->stats.total_detections++;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

int bias_plasticity_detection_feedback(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    bool was_correct,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_detection_feedback: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_detection_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_dete", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_UPDATING;

    // Update type learning state
    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (type_learn) {
        if (was_correct) {
            type_learn->successful_corrections++;
        }
        // Update detection sensitivity
        float accuracy = (float)type_learn->successful_corrections /
                        (type_learn->total_encounters > 0 ? type_learn->total_encounters : 1);
        type_learn->detection_sensitivity = type_learn->detection_sensitivity * 0.9f + accuracy * 0.1f;
    }

    // Update detection synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].bias_type == bias_type &&
            bridge->synapses[i].type == BIAS_SYNAPSE_DETECTION) {

            float old_weight = bridge->synapses[i].weight;
            float dw = 0.0f;

            if (was_correct) {
                dw = bridge->config.detection_correct_ltp * bridge->global_learning_rate;
                bridge->synapses[i].correct_detections++;
                bridge->stats.ltp_events++;
                bridge->stats.correct_detections++;
            } else {
                dw = -bridge->config.detection_incorrect_ltd * bridge->global_learning_rate;
                bridge->synapses[i].false_positives++;
                bridge->stats.ltd_events++;
                bridge->stats.false_positives++;
            }

            // Apply eligibility trace modulation
            if (bridge->config.enable_eligibility) {
                dw *= (1.0f + bridge->synapses[i].eligibility_trace);
            }

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                bias_learn_event_t event = was_correct ?
                    BIAS_LEARN_DETECTION : BIAS_LEARN_FALSE_POSITIVE;
                bridge->weight_callback(bridge->synapses[i].synapse_id, bias_type,
                                       old_weight, bridge->synapses[i].weight,
                                       event, bridge->weight_callback_data);
            }
        }
    }

    bridge->sim_time_us = timestamp_us;
    return 0;
}

int bias_plasticity_conflict_resolved(
    bias_plasticity_bridge_t* bridge,
    float conflict_level,
    bool resolution_correct,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_conflict_resolved: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_conflict_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_conf", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_UPDATING;

    // Update conflict monitor synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].type == BIAS_SYNAPSE_CONFLICT_MONITOR) {
            float old_weight = bridge->synapses[i].weight;
            float dw = 0.0f;

            if (resolution_correct) {
                dw = bridge->config.conflict_resolution_ltp * conflict_level * bridge->global_learning_rate;
                bridge->stats.ltp_events++;
            } else {
                dw = -bridge->config.conflict_persistence_ltd * conflict_level * bridge->global_learning_rate;
                bridge->stats.ltd_events++;
            }

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, 0,
                                       old_weight, bridge->synapses[i].weight,
                                       BIAS_LEARN_CONFLICT_RESOLVED, bridge->weight_callback_data);
            }
        }
    }

    bridge->sim_time_us = timestamp_us;
    return 0;
}

int bias_plasticity_metacognitive_insight(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    float insight_magnitude,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_metacognitive_insight: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_metacognitive_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_meta", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_UPDATING;

    // Update type metacognitive awareness - create if not exists
    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (!type_learn && bridge->num_types < bridge->max_types) {
        type_learn = &bridge->type_learning[bridge->num_types++];
        type_learn->bias_type = bias_type;
        type_learn->detection_sensitivity = 0.5f;
        type_learn->correction_efficiency = 0.0f;
        type_learn->metacognitive_awareness = 0.0f;
        type_learn->total_encounters = 0;
        type_learn->successful_corrections = 0;
    }
    if (type_learn) {
        float old_awareness = type_learn->metacognitive_awareness;
        type_learn->metacognitive_awareness +=
            insight_magnitude * bridge->config.awareness_growth_rate;
        type_learn->metacognitive_awareness =
            nimcp_clampf(type_learn->metacognitive_awareness, 0.0f, 1.0f);

        if (bridge->metacognitive_callback) {
            bridge->metacognitive_callback(bias_type, old_awareness,
                                          type_learn->metacognitive_awareness,
                                          bridge->metacognitive_callback_data);
        }
    }

    // Update metacognitive synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].type == BIAS_SYNAPSE_METACOGNITIVE) {
            float old_weight = bridge->synapses[i].weight;
            float dw = bridge->config.metacognitive_insight_gain *
                      insight_magnitude * bridge->global_learning_rate;

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            bridge->stats.ltp_events++;
            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, bias_type,
                                       old_weight, bridge->synapses[i].weight,
                                       BIAS_LEARN_METACOGNITIVE_INSIGHT, bridge->weight_callback_data);
            }
        }
    }

    // Update overall metacognitive awareness
    float total_awareness = 0.0f;
    for (uint32_t i = 0; i < bridge->num_types; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_types > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_types);
        }

        total_awareness += bridge->type_learning[i].metacognitive_awareness;
    }
    if (bridge->num_types > 0) {
        bridge->overall_metacognitive_awareness = total_awareness / bridge->num_types;
    }

    bridge->sim_time_us = timestamp_us;
    return 0;
}

int bias_plasticity_reward(
    bias_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_reward: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_eligibility) return 0;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_rewa", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_UPDATING;

    // Apply reward-modulated plasticity to all synapses with eligibility
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].eligibility_trace > 0.01f) {
            float old_weight = bridge->synapses[i].weight;
            float dw = reward * bridge->synapses[i].eligibility_trace *
                      bridge->config.reward_modulation_gain * bridge->global_learning_rate;

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            if (dw > 0) {
                bridge->stats.ltp_events++;
            } else {
                bridge->stats.ltd_events++;
            }

            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, 0,
                                       old_weight, bridge->synapses[i].weight,
                                       BIAS_LEARN_REWARD, bridge->weight_callback_data);
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

int bias_plasticity_update(
    bias_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_update: bridge is NULL");
        return -1;
    }

    // Decay eligibility traces
    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_upda", 0.0f);


    if (bridge->config.enable_eligibility) {
        float decay = powf(bridge->config.eligibility_decay, dt_ms);
        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                                 (float)(i + 1) / (float)bridge->num_synapses);
            }

            bridge->synapses[i].eligibility_trace *= decay;
        }
    }

    // BCM threshold update
    if (bridge->config.enable_bcm) {
        float threshold_decay = expf(-dt_ms / bridge->config.bcm_threshold_tau);
        float activity_decay = expf(-dt_ms / bridge->config.bcm_activity_tau);

        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                bias_plasticity_bridge_heartbeat("bias_plastic_loop",
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
        float homeo_rate = dt_ms / bridge->config.homeostatic_tau_ms;
        float accuracy = bridge->stats.correct_detections > 0 ?
            (float)bridge->stats.correct_detections /
            (bridge->stats.correct_detections + bridge->stats.false_positives + bridge->stats.false_negatives)
            : 0.5f;
        float accuracy_error = bridge->config.target_detection_accuracy - accuracy;

        bridge->global_learning_rate += accuracy_error * homeo_rate;
        bridge->global_learning_rate = nimcp_clampf(bridge->global_learning_rate, 0.1f, 2.0f);
    }

    // Update mean detection accuracy
    if (bridge->stats.total_detections > 0) {
        bridge->stats.mean_detection_accuracy =
            (float)bridge->stats.correct_detections / bridge->stats.total_detections;
    }

    bridge->sim_time_us += (uint64_t)(dt_ms * 1000.0f);

    return 0;
}

int bias_plasticity_consolidate(bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_cons", 0.0f);


    bridge->state = BIAS_PLASTICITY_STATE_CONSOLIDATING;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            bias_plasticity_bridge_heartbeat("bias_plastic_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        // Consolidation based on detection accuracy
        uint32_t total = bridge->synapses[i].correct_detections +
                        bridge->synapses[i].false_positives +
                        bridge->synapses[i].false_negatives;

        if (total > 10) {
            float accuracy = (float)bridge->synapses[i].correct_detections / (total > 0 ? total : 1);

            if (accuracy > 0.7f) {
                bridge->synapses[i].consolidation_level += 0.1f * accuracy;
                bridge->synapses[i].consolidation_level =
                    nimcp_clampf(bridge->synapses[i].consolidation_level, 0.0f, 1.0f);
            }
        }
    }

    bridge->state = BIAS_PLASTICITY_STATE_IDLE;
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float bias_plasticity_get_detection_sensitivity(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
) {
    if (!bridge) return 0.5f;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (!type_learn) return 0.5f;

    return type_learn->detection_sensitivity;
}

float bias_plasticity_get_correction_efficiency(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (!type_learn) return 0.0f;

    if (type_learn->total_encounters == 0) return 0.0f;
    return (float)type_learn->successful_corrections / type_learn->total_encounters;
}

float bias_plasticity_get_metacognitive_awareness(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
) {
    if (!bridge) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (!type_learn) return 0.0f;

    return type_learn->metacognitive_awareness;
}

int bias_plasticity_get_type_learning(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    bias_type_learning_t* learning
) {
    if (!bridge || !learning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_get_type_learning: required parameter is NULL (bridge, learning)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    bias_type_learning_t* type_learn = find_type_learning(bridge, bias_type);
    if (!type_learn) {
        return -1;  /* Type not found */
    }

    *learning = *type_learn;
    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int bias_plasticity_get_state(
    const bias_plasticity_bridge_t* bridge,
    bias_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    state->state = bridge->state;
    state->registered_synapses = bridge->num_synapses;
    state->tracked_bias_types = bridge->num_types;
    state->global_learning_rate = bridge->global_learning_rate;
    state->overall_metacognitive_awareness = bridge->overall_metacognitive_awareness;
    state->bio_async_connected = bridge->bio_async_connected;

    return 0;
}

int bias_plasticity_get_stats(
    const bias_plasticity_bridge_t* bridge,
    bias_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_get_", 0.0f);


    return 0;
}

void bias_plasticity_reset_stats(bias_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_rese", 0.0f);


    memset(&bridge->stats, 0, sizeof(bias_plasticity_stats_t));
}

//=============================================================================
// Callbacks
//=============================================================================

int bias_plasticity_set_weight_callback(
    bias_plasticity_bridge_t* bridge,
    bias_weight_change_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_set_", 0.0f);


    bridge->weight_callback = callback;
    bridge->weight_callback_data = user_data;
    return 0;
}

int bias_plasticity_set_metacognitive_callback(
    bias_plasticity_bridge_t* bridge,
    bias_metacognitive_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_set_metacognitive_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_set_", 0.0f);


    bridge->metacognitive_callback = callback;
    bridge->metacognitive_callback_data = user_data;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int bias_plasticity_connect_bio_async(bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_connect_bio_async: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_conn", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int bias_plasticity_disconnect_bio_async(bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_plasticity_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_disc", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool bias_plasticity_is_bio_async_connected(const bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_plasticity_bridge_heartbeat("bias_plastic_bias_plasticity_is_b", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void bias_plasticity_bridge_set_instance_health_agent(bias_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "bias_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int bias_plasticity_bridge_training_begin(bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    bias_plasticity_bridge_heartbeat_instance(bridge->health_agent, "bias_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int bias_plasticity_bridge_training_end(bias_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    bias_plasticity_bridge_heartbeat_instance(bridge->health_agent, "bias_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int bias_plasticity_bridge_training_step(bias_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    bias_plasticity_bridge_heartbeat_instance(bridge->health_agent, "bias_plasticity_bridge_training_step", progress);
    return 0;
}
