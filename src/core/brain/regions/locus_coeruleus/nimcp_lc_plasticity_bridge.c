/**
 * @file nimcp_lc_plasticity_bridge.c
 * @brief Implementation of LC-Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_plasticity_bridge.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lc_plasticity_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "LC_PLASTICITY_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct nimcp_lc_plasticity_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    nimcp_lc_plasticity_config_t config;

    /* Connected systems */
    nimcp_lc_adapter_t lc_adapter;
    void* plasticity_coordinator;

    /* Synapse storage */
    nimcp_lc_plasticity_synapse_t* synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;

    /* State */
    nimcp_lc_plasticity_bridge_state_t state;
    nimcp_lc_plasticity_modulation_t current_modulation;

    /* Callback */
    nimcp_lc_weight_change_cb weight_callback;
    void* callback_user_data;

    /* Statistics */
    nimcp_lc_plasticity_stats_t stats;

    /* Timestamps */
    uint64_t current_time_us;
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static uint64_t get_timestamp_us(void) {
    static _Atomic uint64_t counter = 0;
    return ++counter;
}

static nimcp_lc_plasticity_synapse_t* find_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        if (bridge->synapses[i].synapse_id == synapse_id) {
            return &bridge->synapses[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_synapse: validation failed");
    return NULL;
}

static void compute_lr_modulation(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    float ne_level = bridge->state.ne.ne_level;
    float ne_normalized = nimcp_clampf(ne_level / 100.0f, 0.0f, 1.0f);

    /* Linear interpolation for LR multiplier */
    float lr_range = bridge->config.ne_lr_multiplier_max -
                     bridge->config.ne_lr_multiplier_min;
    bridge->current_modulation.lr_multiplier =
        bridge->config.ne_lr_multiplier_min + (lr_range * ne_normalized);

    /* LTP boost from high NE */
    bridge->current_modulation.ltp_boost =
        1.0f + (bridge->config.ne_ltp_boost * ne_normalized);

    /* LTD suppression from high NE */
    bridge->current_modulation.ltd_suppression =
        1.0f - (bridge->config.ne_ltd_suppression * ne_normalized);

    /* Eligibility gate */
    if (bridge->state.ne.phasic_burst_active) {
        bridge->current_modulation.eligibility_gate = 1.0f;
    } else {
        bridge->current_modulation.eligibility_gate =
            ne_normalized > bridge->config.ne_gating_threshold ? 0.5f : 0.1f;
    }

    /* Consolidation rate based on arousal */
    bridge->current_modulation.consolidation_rate =
        bridge->config.consolidation_rate *
        (1.0f + bridge->config.arousal_consolidation_boost * bridge->state.ne.arousal);

    /* Trigger consolidation at high arousal */
    bridge->current_modulation.trigger_consolidation =
        bridge->state.ne.arousal > bridge->config.consolidation_threshold;

    bridge->state.ne.lr_multiplier = bridge->current_modulation.lr_multiplier;
    bridge->state.ne.eligibility_gate = bridge->current_modulation.eligibility_gate;
}

static void apply_stdp(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_synapse_t* synapse,
    float dt_ms
) {
    if (!bridge || !synapse) return;

    /* Get timing difference */
    int64_t delta_t = (int64_t)synapse->last_post_spike_us -
                      (int64_t)synapse->last_pre_spike_us;
    float delta_t_ms = delta_t / 1000.0f;

    float dw = 0.0f;

    if (delta_t_ms > 0 && delta_t_ms < bridge->config.stdp_ltp_window_ms) {
        /* LTP: post after pre */
        float tau = bridge->config.stdp_ltp_window_ms / 3.0f;
        dw = bridge->current_modulation.ltp_boost *
             expf(-delta_t_ms / tau) *
             bridge->current_modulation.lr_multiplier;
        bridge->stats.ltp_events++;
    } else if (delta_t_ms < 0 && delta_t_ms > -bridge->config.stdp_ltd_window_ms) {
        /* LTD: pre after post */
        float tau = bridge->config.stdp_ltd_window_ms / 3.0f;
        dw = -bridge->current_modulation.ltd_suppression *
             expf(delta_t_ms / tau) *
             bridge->current_modulation.lr_multiplier;
        bridge->stats.ltd_events++;
    }

    /* Apply weight change */
    if (dw != 0.0f) {
        float old_weight = synapse->weight;
        synapse->weight = nimcp_clampf(synapse->weight + dw,
                               bridge->config.weight_min,
                               bridge->config.weight_max);

        bridge->stats.avg_weight_change =
            bridge->stats.avg_weight_change * 0.99f + fabsf(dw) * 0.01f;

        /* Notify via callback */
        if (bridge->weight_callback) {
            nimcp_lc_learn_event_t event = dw > 0 ?
                LC_LEARN_ATTENTION : LC_LEARN_NOVELTY;
            bridge->weight_callback(synapse->synapse_id, synapse->type,
                old_weight, synapse->weight, event, bridge->callback_user_data);
        }
    }
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_lc_plasticity_config_t nimcp_lc_plasticity_config_default(void) {
    nimcp_lc_plasticity_config_t config = {
        .enable_ne_gating = true,
        .ne_lr_multiplier_min = 0.5f,
        .ne_lr_multiplier_max = 2.0f,
        .ne_gating_threshold = 0.3f,

        .stdp_ltp_window_ms = LC_PLASTICITY_STDP_WINDOW,
        .stdp_ltd_window_ms = LC_PLASTICITY_STDP_WINDOW,
        .ne_ltp_boost = 0.5f,
        .ne_ltd_suppression = 0.3f,
        .arousal_stdp_gain = 0.5f,

        .enable_eligibility_gating = true,
        .eligibility_decay_base = 0.01f,
        .ne_eligibility_extension = 2.0f,
        .phasic_conversion_boost = 3.0f,

        .enable_bcm_modulation = true,
        .ne_bcm_threshold_shift = 0.2f,
        .bcm_adaptation_rate = 0.001f,

        .enable_consolidation_gating = true,
        .consolidation_threshold = 0.7f,
        .consolidation_rate = 0.01f,
        .arousal_consolidation_boost = 0.5f,

        .enable_homeostatic = true,
        .target_activity = 0.5f,
        .homeostatic_tau_ms = 10000.0f,

        .weight_min = 0.0f,
        .weight_max = 1.0f,
        .initial_weight = NIMCP_SYNAPSE_WEIGHT_INIT,

        .enable_bio_async = false
    };
    return config;
}

nimcp_lc_plasticity_bridge_t* nimcp_lc_plasticity_create(
    const nimcp_lc_plasticity_config_t* config
) {
    nimcp_lc_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_lc_plasticity_config_default();
    }

    /* Allocate synapse storage */
    bridge->synapse_capacity = LC_PLASTICITY_MAX_SYNAPSES;
    bridge->synapses = nimcp_calloc(bridge->synapse_capacity, sizeof(nimcp_lc_plasticity_synapse_t));
    if (!bridge->synapses) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_lc_plasticity_create: bridge->synapses is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->state.state = LC_PLASTICITY_STATE_IDLE;
    bridge->state.ne.lr_multiplier = 1.0f;

    /* Initialize modulation */
    bridge->current_modulation.lr_multiplier = 1.0f;
    bridge->current_modulation.ltp_boost = 1.0f;
    bridge->current_modulation.ltd_suppression = 1.0f;
    bridge->current_modulation.eligibility_gate = 0.5f;

    return bridge;
}

void nimcp_lc_plasticity_destroy(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lc_plasticity");
    nimcp_free(bridge->synapses);
    nimcp_free(bridge);
}

int nimcp_lc_plasticity_reset(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_reset: bridge is NULL");
        return -1;
    }

    /* Reset synapses */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        bridge->synapses[i].weight = bridge->synapses[i].initial_weight;
        bridge->synapses[i].eligibility_trace = 0.0f;
        bridge->synapses[i].trace_gated = false;
        bridge->synapses[i].consolidation_level = 0.0f;
        bridge->synapses[i].consolidated = false;
    }

    /* Reset state */
    memset(&bridge->state, 0, sizeof(bridge->state));
    bridge->state.state = LC_PLASTICITY_STATE_IDLE;
    bridge->state.ne.lr_multiplier = 1.0f;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_lc_plasticity_connect_lc(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
) {
    if (!bridge || !lc_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_connect_lc: required parameter is NULL (bridge, lc_adapter)");
        return -1;
    }
    bridge->lc_adapter = lc_adapter;
    return 0;
}

int nimcp_lc_plasticity_connect_coordinator(
    nimcp_lc_plasticity_bridge_t* bridge,
    struct nimcp_plasticity_coordinator* coordinator
) {
    if (!bridge || !coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_connect_coordinator: required parameter is NULL (bridge, coordinator)");
        return -1;
    }
    bridge->plasticity_coordinator = coordinator;
    return 0;
}

/*=============================================================================
 * Synapse Management
 *===========================================================================*/

int nimcp_lc_plasticity_register_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_lc_synapse_type_t type,
    float initial_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_register_synapse: bridge is NULL");
        return -1;
    }
    if (bridge->synapse_count >= bridge->synapse_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_lc_plasticity_register_synapse: capacity exceeded");
        return -1;
    }

    nimcp_lc_plasticity_synapse_t* synapse = &bridge->synapses[bridge->synapse_count++];
    synapse->synapse_id = synapse_id;
    synapse->type = type;
    synapse->weight = initial_weight;
    synapse->initial_weight = initial_weight;

    bridge->state.registered_synapses = bridge->synapse_count;
    return 0;
}

int nimcp_lc_plasticity_unregister_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_unregister_synapse: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        if (bridge->synapses[i].synapse_id == synapse_id) {
            /* Move last synapse to this position */
            if (i < bridge->synapse_count - 1) {
                bridge->synapses[i] = bridge->synapses[bridge->synapse_count - 1];
            }
            bridge->synapse_count--;
            bridge->state.registered_synapses = bridge->synapse_count;
            return 0;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_lc_plasticity_unregister_synapse: validation failed");
    return -1;
}

int nimcp_lc_plasticity_get_synapse(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_lc_plasticity_synapse_t* synapse
) {
    if (!bridge || !synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_get_synapse: required parameter is NULL (bridge, synapse)");
        return -1;
    }

    nimcp_lc_plasticity_synapse_t* found = find_synapse(bridge, synapse_id);
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_get_synapse: found is NULL");
        return -1;
    }

    *synapse = *found;
    return 0;
}

/*=============================================================================
 * Event Recording
 *===========================================================================*/

int nimcp_lc_plasticity_pre_spike(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_pre_spike: bridge is NULL");
        return -1;
    }

    nimcp_lc_plasticity_synapse_t* synapse = find_synapse(bridge, synapse_id);
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_pre_spike: synapse is NULL");
        return -1;
    }

    synapse->last_pre_spike_us = timestamp_us;
    bridge->stats.total_pre_spikes++;

    /* Update eligibility trace */
    synapse->eligibility_trace = 1.0f;

    return 0;
}

int nimcp_lc_plasticity_post_spike(
    nimcp_lc_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_post_spike: bridge is NULL");
        return -1;
    }

    nimcp_lc_plasticity_synapse_t* synapse = find_synapse(bridge, synapse_id);
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_post_spike: synapse is NULL");
        return -1;
    }

    synapse->last_post_spike_us = timestamp_us;
    bridge->stats.total_post_spikes++;

    /* Apply STDP if timing conditions met */
    apply_stdp(bridge, synapse, 0.0f);

    return 0;
}

int nimcp_lc_plasticity_ne_burst(
    nimcp_lc_plasticity_bridge_t* bridge,
    float intensity,
    uint64_t timestamp_us
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_ne_burst: bridge is NULL");
        return -1;
    }

    bridge->state.ne.phasic_burst_active = true;
    bridge->state.ne.last_burst_us = timestamp_us;

    /* Convert eligibility traces on burst */
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        if (bridge->synapses[i].eligibility_trace > 0.1f) {
            float conversion = bridge->synapses[i].eligibility_trace *
                              intensity *
                              bridge->config.phasic_conversion_boost;
            bridge->synapses[i].weight = nimcp_clampf(
                bridge->synapses[i].weight + conversion * 0.1f,
                bridge->config.weight_min,
                bridge->config.weight_max
            );
            bridge->synapses[i].trace_gated = true;
            bridge->stats.gated_conversions++;
        }
    }

    return 0;
}

int nimcp_lc_plasticity_set_ne_level(
    nimcp_lc_plasticity_bridge_t* bridge,
    float ne_level,
    float arousal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_set_ne_level: bridge is NULL");
        return -1;
    }

    bridge->state.ne.ne_level = nimcp_clampf(ne_level, 0.0f, 100.0f);
    bridge->state.ne.arousal = nimcp_clampf(arousal, 0.0f, 1.0f);

    compute_lr_modulation(bridge);

    return 0;
}

/*=============================================================================
 * Update Functions
 *===========================================================================*/

int nimcp_lc_plasticity_update(
    nimcp_lc_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_update: bridge is NULL");
        return -1;
    }

    bridge->state.state = LC_PLASTICITY_STATE_UPDATING;
    bridge->stats.total_updates++;
    bridge->current_time_us = get_timestamp_us();

    /* Decay eligibility traces */
    float decay = expf(-dt_ms * bridge->config.eligibility_decay_base);
    if (bridge->state.ne.ne_level > 50.0f) {
        decay = powf(decay, 1.0f / bridge->config.ne_eligibility_extension);
    }

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        bridge->synapses[i].eligibility_trace *= decay;

        /* BCM threshold adaptation */
        if (bridge->config.enable_bcm_modulation) {
            bridge->synapses[i].avg_activity =
                bridge->synapses[i].avg_activity * 0.999f +
                (bridge->synapses[i].eligibility_trace > 0.5f ? 1.0f : 0.0f) * 0.001f;
            bridge->synapses[i].bcm_threshold =
                bridge->synapses[i].bcm_threshold * (1.0f - bridge->config.bcm_adaptation_rate) +
                bridge->synapses[i].avg_activity * bridge->config.bcm_adaptation_rate;
        }

        /* Consolidation progress */
        if (!bridge->synapses[i].consolidated &&
            bridge->current_modulation.trigger_consolidation) {
            bridge->synapses[i].consolidation_level +=
                bridge->current_modulation.consolidation_rate * dt_ms / 1000.0f;
            if (bridge->synapses[i].consolidation_level >= 1.0f) {
                bridge->synapses[i].consolidated = true;
                bridge->stats.consolidation_events++;
            }
        }
    }

    /* Update NE modulation */
    compute_lr_modulation(bridge);

    /* Track average NE during learning */
    bridge->stats.avg_ne_during_learning =
        bridge->stats.avg_ne_during_learning * 0.99f +
        bridge->state.ne.ne_level * 0.01f;

    /* Clear phasic burst after some time */
    if (bridge->state.ne.phasic_burst_active &&
        (bridge->current_time_us - bridge->state.ne.last_burst_us) > 100000) {
        bridge->state.ne.phasic_burst_active = false;
    }

    bridge->state.state = LC_PLASTICITY_STATE_IDLE;
    return 0;
}

int nimcp_lc_plasticity_consolidate(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_consolidate: bridge is NULL");
        return -1;
    }

    bridge->state.state = LC_PLASTICITY_STATE_CONSOLIDATING;

    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        if (!bridge->synapses[i].consolidated &&
            bridge->synapses[i].consolidation_level > 0.5f) {
            bridge->synapses[i].consolidated = true;
            bridge->stats.consolidation_events++;
        }
    }

    bridge->state.state = LC_PLASTICITY_STATE_IDLE;
    return 0;
}

/*=============================================================================
 * Query Functions
 *===========================================================================*/

int nimcp_lc_plasticity_get_modulation(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    *modulation = bridge->current_modulation;
    return 0;
}

float nimcp_lc_plasticity_get_learning_progress(
    nimcp_lc_plasticity_bridge_t* bridge
) {
    if (!bridge || bridge->synapse_count == 0) return 0.0f;

    float total_change = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        total_change += fabsf(bridge->synapses[i].weight -
                             bridge->synapses[i].initial_weight);
    }
    return total_change / bridge->synapse_count;
}

float nimcp_lc_plasticity_get_novelty_signal(
    nimcp_lc_plasticity_bridge_t* bridge
) {
    if (!bridge) return 0.0f;

    /* Novelty based on unexpected weight changes */
    return bridge->stats.avg_weight_change;
}

float nimcp_lc_plasticity_get_consolidation_progress(
    nimcp_lc_plasticity_bridge_t* bridge
) {
    if (!bridge || bridge->synapse_count == 0) return 0.0f;

    float total_consolidated = 0.0f;
    for (uint32_t i = 0; i < bridge->synapse_count; i++) {
        total_consolidated += bridge->synapses[i].consolidation_level;
    }
    return total_consolidated / bridge->synapse_count;
}

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

int nimcp_lc_plasticity_get_state(
    const nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int nimcp_lc_plasticity_get_stats(
    const nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_plasticity_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void nimcp_lc_plasticity_reset_stats(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

/*=============================================================================
 * Callbacks
 *===========================================================================*/

int nimcp_lc_plasticity_set_weight_callback(
    nimcp_lc_plasticity_bridge_t* bridge,
    nimcp_lc_weight_change_cb callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_set_weight_callback: bridge is NULL");
        return -1;
    }
    bridge->weight_callback = callback;
    bridge->callback_user_data = user_data;
    return 0;
}

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

int nimcp_lc_plasticity_connect_bio_async(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_connect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->state.bio_async_connected = true;
    return 0;
}

int nimcp_lc_plasticity_disconnect_bio_async(nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_plasticity_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->state.bio_async_connected = false;
    return 0;
}

bool nimcp_lc_plasticity_is_bio_async_connected(const nimcp_lc_plasticity_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.bio_async_connected;
}
