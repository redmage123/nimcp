/**
 * @file nimcp_epistemic_plasticity_bridge.c
 * @brief Epistemic - Plasticity Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Implementation of plasticity bridge for epistemic filtering
 * WHY:  Enable learning-based improvement of belief evaluation through STDP
 * HOW:  Track belief updates for spike-timing dependent synaptic changes
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/epistemic/nimcp_epistemic_plasticity_bridge.h"
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

BRIDGE_BOILERPLATE(epistemic_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "EPISTEMIC_PLASTICITY_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct epistemic_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    epistemic_plasticity_config_t config;
    epistemic_plasticity_state_t state;

    // Synapses
    epistemic_plasticity_synapse_t* synapses;
    uint32_t num_synapses;
    uint32_t max_synapses;

    // Source learning
    epistemic_source_learning_t* sources;
    uint32_t num_sources;
    uint32_t max_sources;

    // Global state
    float global_learning_rate;
    float current_epistemic_quality;
    float bcm_global_threshold;

    // Statistics
    epistemic_plasticity_stats_t stats;

    // Callbacks
    epistemic_weight_change_cb weight_callback;
    void* weight_callback_data;
    epistemic_source_update_cb source_callback;
    void* source_callback_data;

    // Bio-async
    bool bio_async_connected;

    // Simulation time
    uint64_t sim_time_us;
};

static float stdp_ltp(float dt_ms, float a_plus, float tau_plus) {
    if (dt_ms <= 0.0f) return 0.0f;
    return a_plus * expf(-dt_ms / tau_plus);
}

static float stdp_ltd(float dt_ms, float a_minus, float tau_minus) {
    if (dt_ms >= 0.0f) return 0.0f;
    return -a_minus * expf(dt_ms / tau_minus);
}

static epistemic_plasticity_synapse_t* find_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    return NULL;  /* Not found is normal */
}

static epistemic_source_learning_t* find_source(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id
) {
    for (uint32_t i = 0; i < bridge->num_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sources > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_sources);
        }

        if (bridge->sources[i].source_id == source_id) {
            return &bridge->sources[i];
        }
    }
    return NULL;
}

static void apply_weight_bounds(
    epistemic_plasticity_bridge_t* bridge,
    epistemic_plasticity_synapse_t* synapse
) {
    synapse->weight = nimcp_clampf(synapse->weight, bridge->config.weight_min, bridge->config.weight_max);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

epistemic_plasticity_config_t epistemic_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    epistemic_plasticity_config_t config = {
        .stdp_ltp_window_ms = EPISTEMIC_PLASTICITY_STDP_WINDOW,
        .stdp_ltd_window_ms = EPISTEMIC_PLASTICITY_STDP_WINDOW,
        .stdp_a_plus = NIMCP_STDP_A_PLUS,
        .stdp_a_minus = NIMCP_STDP_A_MINUS,
        .stdp_tau_plus = NIMCP_STDP_TAU_PLUS_MS,
        .stdp_tau_minus = NIMCP_STDP_TAU_MINUS_MS,

        .enable_source_learning = true,
        .source_correct_ltp = 0.05f,
        .source_incorrect_ltd = 0.08f,

        .enable_bias_learning = true,
        .bias_detection_ltp = 0.03f,
        .bias_correction_reward = 0.1f,

        .enable_evidence_weighting = true,
        .evidence_quality_gain = 1.5f,
        .evidence_recency_decay = 0.001f,

        .enable_bcm = true,
        .bcm_threshold_tau = 1000.0f,
        .bcm_activity_tau = 100.0f,

        .enable_homeostatic = true,
        .target_epistemic_quality = 0.7f,
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

epistemic_plasticity_bridge_t* epistemic_plasticity_create(
    const epistemic_plasticity_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_create: config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    epistemic_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(epistemic_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_plasticity_create: failed to allocate bridge");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = EPISTEMIC_PLASTICITY_STATE_IDLE;
    bridge->max_synapses = EPISTEMIC_PLASTICITY_MAX_SYNAPSES;
    bridge->max_sources = EPISTEMIC_PLASTICITY_MAX_SOURCES;

    // Allocate synapses
    bridge->synapses = nimcp_calloc(bridge->max_synapses, sizeof(epistemic_plasticity_synapse_t));
    if (!bridge->synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_plasticity_create: failed to allocate synapses");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    // Allocate sources
    bridge->sources = nimcp_calloc(bridge->max_sources, sizeof(epistemic_source_learning_t));
    if (!bridge->sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_plasticity_create: failed to allocate sources");
        nimcp_free(bridge->synapses);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->global_learning_rate = 1.0f;
    bridge->current_epistemic_quality = 0.5f;
    bridge->bcm_global_threshold = 0.5f;

    return bridge;
}

void epistemic_plasticity_destroy(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "epistemic_plasticity");

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    nimcp_free(bridge->synapses);
    nimcp_free(bridge->sources);
    nimcp_free(bridge);
    bridge = NULL;
}

int epistemic_plasticity_reset(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_IDLE;
    bridge->num_synapses = 0;
    bridge->num_sources = 0;
    bridge->global_learning_rate = 1.0f;
    bridge->current_epistemic_quality = 0.5f;
    bridge->bcm_global_threshold = 0.5f;
    bridge->sim_time_us = 0;

    memset(bridge->synapses, 0, bridge->max_synapses * sizeof(epistemic_plasticity_synapse_t));
    memset(bridge->sources, 0, bridge->max_sources * sizeof(epistemic_source_learning_t));
    memset(&bridge->stats, 0, sizeof(epistemic_plasticity_stats_t));

    return 0;
}

//=============================================================================
// Synapse Management
//=============================================================================

int epistemic_plasticity_register_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    epistemic_synapse_type_t type,
    uint32_t source_id,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_register_synapse: bridge is NULL");
        return -1;
    }
    if (bridge->num_synapses >= bridge->max_synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "epistemic_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    // Check for duplicate
    if (find_synapse(bridge, synapse_id)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epistemic_plasticity_register_synapse: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    epistemic_plasticity_synapse_t* synapse = &bridge->synapses[bridge->num_synapses];
    synapse->synapse_id = synapse_id;
    synapse->type = type;
    synapse->source_id = source_id;
    synapse->weight = nimcp_clampf(initial_weight, bridge->config.weight_min, bridge->config.weight_max);
    synapse->initial_weight = synapse->weight;
    synapse->last_pre_spike_us = 0;
    synapse->last_post_spike_us = 0;
    synapse->eligibility_trace = 0.0f;
    synapse->bcm_threshold = bridge->bcm_global_threshold;
    synapse->avg_activity = 0.0f;
    synapse->consolidation_level = 0.0f;
    synapse->correct_count = 0;
    synapse->incorrect_count = 0;

    bridge->num_synapses++;
    return 0;
}

int epistemic_plasticity_unregister_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].synapse_id == synapse_id) {
            // Move last synapse to this slot
            if (i < bridge->num_synapses - 1) {
                bridge->synapses[i] = bridge->synapses[bridge->num_synapses - 1];
            }
            bridge->num_synapses--;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epistemic_plasticity_unregister_synapse: validation failed");
    return -1;
}

int epistemic_plasticity_get_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    epistemic_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    epistemic_plasticity_synapse_t* found = find_synapse(bridge, synapse_id);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_get_synapse: found is NULL");
        return -1;
    }

    *synapse = *found;
    return 0;
}

//=============================================================================
// Event Recording
//=============================================================================

int epistemic_plasticity_evidence_update(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id,
    float evidence_quality,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_evidence_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_EVALUATING;
    bridge->current_epistemic_quality = nimcp_clampf(evidence_quality, 0.0f, 1.0f);

    // Update eligibility traces for source-related synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].source_id == source_id) {
            bridge->synapses[i].last_pre_spike_us = timestamp_us;

            if (bridge->config.enable_eligibility) {
                bridge->synapses[i].eligibility_trace +=
                    evidence_quality * bridge->config.evidence_quality_gain;
                bridge->synapses[i].eligibility_trace =
                    nimcp_clampf(bridge->synapses[i].eligibility_trace, 0.0f, 1.0f);
            }
        }
    }

    bridge->stats.total_evaluations++;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

int epistemic_plasticity_source_feedback(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id,
    bool was_correct,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_source_feedback: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_source_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_UPDATING;

    // Find or create source
    epistemic_source_learning_t* source = find_source(bridge, source_id);
    if (!source) {
        if (bridge->num_sources >= bridge->max_sources) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "epistemic_plasticity_source_feedback: capacity exceeded");
            return -1;
        }
        source = &bridge->sources[bridge->num_sources++];
        source->source_id = source_id;
        source->learned_reliability = 0.5f;
        source->confidence = 0.0f;
        source->total_evaluations = 0;
        source->correct_evaluations = 0;
    }

    float old_reliability = source->learned_reliability;

    source->total_evaluations++;
    if (was_correct) {
        source->correct_evaluations++;
    }

    // Bayesian reliability update
    float alpha = (float)source->correct_evaluations + 1.0f;
    float beta = (float)(source->total_evaluations - source->correct_evaluations) + 1.0f;
    source->learned_reliability = alpha / (alpha + beta);
    source->confidence = 1.0f - 1.0f / (1.0f + 0.1f * source->total_evaluations);
    source->last_evaluation_time_us = timestamp_us;

    // Update synapses for this source
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].source_id == source_id &&
            bridge->synapses[i].type == EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY) {

            float old_weight = bridge->synapses[i].weight;
            float dw = 0.0f;

            if (was_correct) {
                dw = bridge->config.source_correct_ltp * bridge->global_learning_rate;
                bridge->synapses[i].correct_count++;
                bridge->stats.ltp_events++;
            } else {
                dw = -bridge->config.source_incorrect_ltd * bridge->global_learning_rate;
                bridge->synapses[i].incorrect_count++;
                bridge->stats.ltd_events++;
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
                epistemic_learn_event_t event = was_correct ?
                    EPISTEMIC_LEARN_SOURCE_CORRECT : EPISTEMIC_LEARN_SOURCE_INCORRECT;
                bridge->weight_callback(bridge->synapses[i].synapse_id, source_id,
                                       old_weight, bridge->synapses[i].weight,
                                       event, bridge->weight_callback_data);
            }
        }
    }

    if (bridge->source_callback) {
        bridge->source_callback(source_id, old_reliability, source->learned_reliability,
                               bridge->source_callback_data);
    }

    bridge->stats.source_updates++;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

int epistemic_plasticity_bias_detected(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    float confidence,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_bias_detected: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_bias_learning) return 0;

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_UPDATING;

    // Strengthen bias detection synapses
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].type == EPISTEMIC_SYNAPSE_BIAS_DETECTION) {
            float old_weight = bridge->synapses[i].weight;
            float dw = bridge->config.bias_detection_ltp * confidence * bridge->global_learning_rate;

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            bridge->stats.ltp_events++;
            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, bias_type,
                                       old_weight, bridge->synapses[i].weight,
                                       EPISTEMIC_LEARN_BIAS_DETECTED, bridge->weight_callback_data);
            }
        }
    }

    bridge->stats.bias_detections++;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

int epistemic_plasticity_belief_revision(
    epistemic_plasticity_bridge_t* bridge,
    float prior,
    float posterior,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_belief_revision: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_UPDATING;

    float belief_change = fabsf(posterior - prior);

    // Update prior-update synapses based on revision magnitude
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].type == EPISTEMIC_SYNAPSE_PRIOR_UPDATE) {
            float old_weight = bridge->synapses[i].weight;
            float dw = 0.0f;

            // Large revisions strengthen adaptability
            if (belief_change > 0.3f) {
                dw = bridge->config.stdp_a_plus * belief_change * bridge->global_learning_rate;
                bridge->stats.ltp_events++;
            } else {
                // Small revisions stabilize current weights
                dw = -bridge->config.stdp_a_minus * (0.3f - belief_change) * bridge->global_learning_rate;
                bridge->stats.ltd_events++;
            }

            bridge->synapses[i].weight += dw;
            apply_weight_bounds(bridge, &bridge->synapses[i]);

            bridge->stats.avg_weight_change =
                bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

            if (bridge->weight_callback) {
                bridge->weight_callback(bridge->synapses[i].synapse_id, 0,
                                       old_weight, bridge->synapses[i].weight,
                                       EPISTEMIC_LEARN_BELIEF_REVISION, bridge->weight_callback_data);
            }
        }
    }

    bridge->stats.belief_revisions++;
    bridge->sim_time_us = timestamp_us;

    return 0;
}

int epistemic_plasticity_reward(
    epistemic_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_reward: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_eligibility) return 0;

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_UPDATING;

    // Apply reward-modulated plasticity to all synapses with eligibility
    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
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
                                       EPISTEMIC_LEARN_REWARD, bridge->weight_callback_data);
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

int epistemic_plasticity_update(
    epistemic_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_update: bridge is NULL");
        return -1;
    }

    // Decay eligibility traces
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    if (bridge->config.enable_eligibility) {
        float decay = powf(bridge->config.eligibility_decay, dt_ms);
        for (uint32_t i = 0; i < bridge->num_synapses; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
                epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
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
                epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                                 (float)(i + 1) / (float)bridge->num_synapses);
            }

            // Update average activity
            bridge->synapses[i].avg_activity =
                bridge->synapses[i].avg_activity * activity_decay +
                bridge->synapses[i].weight * (1.0f - activity_decay);

            // Update BCM threshold
            float target = bridge->synapses[i].avg_activity * bridge->synapses[i].avg_activity;
            bridge->synapses[i].bcm_threshold =
                bridge->synapses[i].bcm_threshold * threshold_decay +
                target * (1.0f - threshold_decay);
        }
    }

    // Homeostatic regulation
    if (bridge->config.enable_homeostatic) {
        float homeo_rate = dt_ms / bridge->config.homeostatic_tau_ms;
        float quality_error = bridge->config.target_epistemic_quality - bridge->current_epistemic_quality;

        bridge->global_learning_rate += quality_error * homeo_rate;
        bridge->global_learning_rate = nimcp_clampf(bridge->global_learning_rate, 0.1f, 2.0f);
    }

    // Update source reliability averages
    float total_reliability = 0.0f;
    for (uint32_t i = 0; i < bridge->num_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_sources > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_sources);
        }

        total_reliability += bridge->sources[i].learned_reliability;
    }
    if (bridge->num_sources > 0) {
        bridge->stats.avg_source_reliability = total_reliability / bridge->num_sources;
    }

    bridge->sim_time_us += (uint64_t)(dt_ms * 1000.0f);

    return 0;
}

int epistemic_plasticity_consolidate(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->state = EPISTEMIC_PLASTICITY_STATE_CONSOLIDATING;

    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        // Consolidation based on reliability of learning
        float reliability = 0.0f;
        uint32_t total = bridge->synapses[i].correct_count + bridge->synapses[i].incorrect_count;
        if (total > 0) {
            reliability = (float)bridge->synapses[i].correct_count / total;
        }

        // High reliability leads to consolidation
        if (reliability > 0.7f && total > 10) {
            bridge->synapses[i].consolidation_level += 0.1f * reliability;
            bridge->synapses[i].consolidation_level =
                nimcp_clampf(bridge->synapses[i].consolidation_level, 0.0f, 1.0f);

            // Consolidated synapses become more stable
            float stability_factor = bridge->synapses[i].consolidation_level;
            bridge->synapses[i].weight =
                bridge->synapses[i].weight * (1.0f - stability_factor * 0.1f) +
                bridge->synapses[i].initial_weight * stability_factor * 0.1f;
        }
    }

    bridge->state = EPISTEMIC_PLASTICITY_STATE_IDLE;
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float epistemic_plasticity_get_source_reliability(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id
) {
    if (!bridge) return 0.5f;

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    epistemic_source_learning_t* source = find_source(bridge, source_id);
    if (!source) return 0.5f;

    return source->learned_reliability;
}

float epistemic_plasticity_get_evidence_weight(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id
) {
    if (!bridge) return 0.5f;

    // Find evidence integration synapse for this source
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].source_id == source_id &&
            bridge->synapses[i].type == EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION) {
            return bridge->synapses[i].weight;
        }
    }

    return bridge->config.initial_weight;
}

float epistemic_plasticity_get_bias_sensitivity(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t bias_type
) {
    if (!bridge) return 0.5f;

    // Find bias detection synapse for this type
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    for (uint32_t i = 0; i < bridge->num_synapses; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_synapses > 256) {
            epistemic_plasticity_bridge_heartbeat("epistemic_pl_loop",
                             (float)(i + 1) / (float)bridge->num_synapses);
        }

        if (bridge->synapses[i].type == EPISTEMIC_SYNAPSE_BIAS_DETECTION &&
            bridge->synapses[i].source_id == bias_type) {
            return bridge->synapses[i].weight;
        }
    }

    return bridge->config.initial_weight;
}

int epistemic_plasticity_get_source_learning(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id,
    epistemic_source_learning_t* learning
) {
    if (!bridge || !learning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_get_source_learning: required parameter is NULL (bridge, learning)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    epistemic_source_learning_t* source = find_source(bridge, source_id);
    if (!source) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_get_source_learning: source is NULL");
        return -1;
    }

    *learning = *source;
    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int epistemic_plasticity_get_state(
    const epistemic_plasticity_bridge_t* bridge,
    epistemic_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    state->state = bridge->state;
    state->registered_synapses = bridge->num_synapses;
    state->tracked_sources = bridge->num_sources;
    state->global_learning_rate = bridge->global_learning_rate;
    state->current_epistemic_quality = bridge->current_epistemic_quality;
    state->bio_async_connected = bridge->bio_async_connected;

    return 0;
}

int epistemic_plasticity_get_stats(
    const epistemic_plasticity_bridge_t* bridge,
    epistemic_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    return 0;
}

void epistemic_plasticity_reset_stats(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    memset(&bridge->stats, 0, sizeof(epistemic_plasticity_stats_t));
}

//=============================================================================
// Callbacks
//=============================================================================

int epistemic_plasticity_set_weight_callback(
    epistemic_plasticity_bridge_t* bridge,
    epistemic_weight_change_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->weight_callback = callback;
    bridge->weight_callback_data = user_data;
    return 0;
}

int epistemic_plasticity_set_source_callback(
    epistemic_plasticity_bridge_t* bridge,
    epistemic_source_update_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_set_source_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->source_callback = callback;
    bridge->source_callback_data = user_data;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int epistemic_plasticity_connect_bio_async(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_connect_bio_async: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int epistemic_plasticity_disconnect_bio_async(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_plasticity_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool epistemic_plasticity_is_bio_async_connected(const epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    epistemic_plasticity_bridge_heartbeat("epistemic_pl_epistemic_plasticity", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void epistemic_plasticity_bridge_set_instance_health_agent(epistemic_plasticity_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "epistemic_plasticity_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int epistemic_plasticity_bridge_training_begin(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_plasticity_bridge_training_begin: NULL argument");
        return -1;
    }
    epistemic_plasticity_bridge_heartbeat_instance(bridge->health_agent, "epistemic_plasticity_bridge_training_begin", 0.0f);
    return 0;
}

int epistemic_plasticity_bridge_training_end(epistemic_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_plasticity_bridge_training_end: NULL argument");
        return -1;
    }
    epistemic_plasticity_bridge_heartbeat_instance(bridge->health_agent, "epistemic_plasticity_bridge_training_end", 1.0f);
    return 0;
}

int epistemic_plasticity_bridge_training_step(epistemic_plasticity_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_plasticity_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    epistemic_plasticity_bridge_heartbeat_instance(bridge->health_agent, "epistemic_plasticity_bridge_training_step", progress);
    return 0;
}
