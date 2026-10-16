/**
 * @file nimcp_vta_snn_bridge.c
 * @brief Implementation of VTA-SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/vta/nimcp_vta_snn_bridge.h"
#include "core/brain/regions/vta/nimcp_vta_adapter.h"
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
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(vta_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "VTA_SNN_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct nimcp_vta_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_vta_snn_config_t config;
    nimcp_vta_adapter_t vta_adapter;
    void* snn;

    nimcp_vta_snn_bridge_state_t state;
    nimcp_vta_snn_modulation_t current_modulation;

    float* input_spikes;
    float* output_spikes;
    uint32_t spike_buffer_size;

    nimcp_vta_snn_stats_t stats;
    uint64_t current_time_us;
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static uint64_t get_timestamp_us(void) {
    static _Atomic uint64_t counter = 0;
    return ++counter;
}

static void compute_modulation(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) return;

    float da_level = bridge->state.da.da_level;
    float motivation = bridge->state.da.motivation;
    float rpe = bridge->state.da.current_rpe;

    bridge->current_modulation.motivation = motivation;
    bridge->current_modulation.vigor = bridge->state.da.vigor;
    bridge->current_modulation.rpe_signal = rpe;

    /* Effort willingness based on DA and motivation */
    bridge->current_modulation.effort_willingness =
        nimcp_clampf(0.3f + 0.7f * motivation * (da_level / 100.0f), 0.0f, 1.0f);

    /* Reward sensitivity */
    bridge->current_modulation.reward_sensitivity =
        0.5f + (da_level / 100.0f);

    /* Goal achievement check */
    bridge->current_modulation.goal_achieved =
        (bridge->state.da.wanting < 0.1f && motivation < 0.2f);
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_vta_snn_config_t nimcp_vta_snn_config_default(void) {
    nimcp_vta_snn_config_t config = {
        .population_size = VTA_SNN_POPULATION_SIZE,
        .input_dim = VTA_SNN_INPUT_DIM,
        .output_dim = 64,

        .encoding = VTA_SNN_ENCODE_RPE,
        .encoding_gain = 1.0f,
        .tonic_baseline_hz = 4.0f,
        .burst_rate_hz = 30.0f,
        .pause_suppression = 0.9f,
        .rpe_encoding_scale = 10.0f,

        .decoding = VTA_SNN_DECODE_AVERAGE,
        .decoding_threshold = 0.1f,
        .temporal_smoothing = 0.1f,

        .enable_rpe_computation = true,
        .rpe_learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .discount_factor = NIMCP_REWARD_DISCOUNT_DEFAULT,

        .enable_motivation_output = true,
        .motivation_gain = 1.0f,
        .effort_cost_scale = 0.5f,

        .dt_ms = VTA_SNN_DEFAULT_DT,
        .simulation_window_ms = VTA_SNN_ENCODING_WINDOW,

        .enable_bio_async = false,
        .enable_plasticity_bridge = false
    };
    return config;
}

nimcp_vta_snn_bridge_t* nimcp_vta_snn_create(const nimcp_vta_snn_config_t* config) {
    nimcp_vta_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    bridge->config = config ? *config : nimcp_vta_snn_config_default();

    bridge->spike_buffer_size = bridge->config.population_size;
    bridge->input_spikes = nimcp_calloc(bridge->spike_buffer_size, sizeof(float));
    bridge->output_spikes = nimcp_calloc(bridge->spike_buffer_size, sizeof(float));

    if (!bridge->input_spikes || !bridge->output_spikes) {
        nimcp_vta_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_vta_snn_create: required parameter is NULL (bridge->input_spikes, bridge->output_spikes)");
        return NULL;
    }

    bridge->state.state = VTA_SNN_STATE_IDLE;
    bridge->state.da.da_type = VTA_SNN_DA_TONIC;
    bridge->state.da.motivation = 0.5f;
    bridge->state.da.vigor = 0.5f;

    bridge->current_modulation.motivation = 0.5f;
    bridge->current_modulation.effort_willingness = 0.5f;
    bridge->current_modulation.reward_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->current_modulation.vigor = 0.5f;

    NIMCP_LOGGING_INFO("Created %s bridge", "vta_snn");
    return bridge;
}

void nimcp_vta_snn_destroy(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "vta_snn");
    nimcp_free(bridge->input_spikes);
    nimcp_free(bridge->output_spikes);
    nimcp_free(bridge);
}

int nimcp_vta_snn_reset(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_reset: bridge is NULL");
        return -1;
    }

    memset(&bridge->state, 0, sizeof(bridge->state));
    bridge->state.state = VTA_SNN_STATE_IDLE;
    bridge->state.da.da_type = VTA_SNN_DA_TONIC;
    bridge->state.da.motivation = 0.5f;

    memset(bridge->input_spikes, 0, bridge->spike_buffer_size * sizeof(float));
    memset(bridge->output_spikes, 0, bridge->spike_buffer_size * sizeof(float));
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_vta_snn_connect_vta(
    nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
) {
    if (!bridge || !vta_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_connect_vta: required parameter is NULL (bridge, vta_adapter)");
        return -1;
    }
    bridge->vta_adapter = vta_adapter;
    return 0;
}

int nimcp_vta_snn_connect_snn(
    nimcp_vta_snn_bridge_t* bridge,
    struct nimcp_snn_network* snn
) {
    if (!bridge || !snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_connect_snn: required parameter is NULL (bridge, snn)");
        return -1;
    }
    bridge->snn = snn;
    return 0;
}

/*=============================================================================
 * Encoding Functions (VTA --> SNN)
 *===========================================================================*/

int nimcp_vta_snn_encode_da_state(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_encode_da_state: bridge is NULL");
        return -1;
    }

    bridge->state.state = VTA_SNN_STATE_ENCODING;
    int spikes = 0;

    float da_level = bridge->state.da.da_level;
    float rate_factor = da_level / 100.0f;

    for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
        float rate = bridge->config.tonic_baseline_hz +
            rate_factor * (bridge->config.burst_rate_hz - bridge->config.tonic_baseline_hz);
        float spike_prob = rate * bridge->config.dt_ms / 1000.0f;
        bridge->input_spikes[i] = (nimcp_tl_rand() / (float)RAND_MAX) < spike_prob ? 1.0f : 0.0f;
        if (bridge->input_spikes[i] > 0) spikes++;
    }

    bridge->stats.total_spikes_generated += spikes;
    bridge->state.state = VTA_SNN_STATE_IDLE;
    return spikes;
}

int nimcp_vta_snn_encode_reward(
    nimcp_vta_snn_bridge_t* bridge,
    float reward,
    float expected_reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_encode_reward: bridge is NULL");
        return -1;
    }

    float rpe = reward - expected_reward;
    bridge->state.da.current_rpe = rpe;

    if (rpe > 0) {
        bridge->state.da.da_type = VTA_SNN_DA_PHASIC_POSITIVE;
        bridge->stats.positive_rpe_events++;
        return nimcp_vta_snn_encode_burst(bridge, rpe);
    } else if (rpe < 0) {
        bridge->state.da.da_type = VTA_SNN_DA_PHASIC_NEGATIVE;
        bridge->stats.negative_rpe_events++;
        return nimcp_vta_snn_encode_pause(bridge, -rpe);
    }

    return 0;
}

int nimcp_vta_snn_encode_burst(
    nimcp_vta_snn_bridge_t* bridge,
    float intensity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_encode_burst: bridge is NULL");
        return -1;
    }

    int spikes = 0;
    float burst_rate = intensity * bridge->config.burst_rate_hz;

    for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
        float spike_prob = burst_rate * bridge->config.dt_ms / 1000.0f;
        bridge->input_spikes[i] = (nimcp_tl_rand() / (float)RAND_MAX) < spike_prob ? 1.0f : 0.0f;
        if (bridge->input_spikes[i] > 0) spikes++;
    }

    bridge->stats.total_spikes_generated += spikes;
    return spikes;
}

int nimcp_vta_snn_encode_pause(
    nimcp_vta_snn_bridge_t* bridge,
    float suppression
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_encode_pause: bridge is NULL");
        return -1;
    }

    /* Pause encoding - suppress activity */
    float suppression_factor = 1.0f - (suppression * bridge->config.pause_suppression);
    suppression_factor = nimcp_clampf(suppression_factor, 0.0f, 1.0f);

    for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
        bridge->input_spikes[i] *= suppression_factor;
    }

    return 0;
}

int nimcp_vta_snn_encode_motivation(
    nimcp_vta_snn_bridge_t* bridge,
    float motivation,
    float wanting
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_encode_motivation: bridge is NULL");
        return -1;
    }

    bridge->state.da.motivation = nimcp_clampf(motivation, 0.0f, 1.0f);
    bridge->state.da.wanting = nimcp_clampf(wanting, 0.0f, 1.0f);
    bridge->state.da.vigor = motivation * wanting;

    compute_modulation(bridge);
    return nimcp_vta_snn_encode_da_state(bridge);
}

/*=============================================================================
 * Simulation Functions
 *===========================================================================*/

int nimcp_vta_snn_simulate(nimcp_vta_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_simulate: bridge is NULL");
        return -1;
    }

    bridge->state.state = VTA_SNN_STATE_SIMULATING;

    float time = 0.0f;
    while (time < duration_ms) {
        nimcp_vta_snn_step(bridge);
        time += bridge->config.dt_ms;
    }

    bridge->state.state = VTA_SNN_STATE_IDLE;
    return 0;
}

int nimcp_vta_snn_step(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_step: bridge is NULL");
        return -1;
    }

    bridge->stats.total_updates++;
    bridge->current_time_us = get_timestamp_us();

    /* Compute average firing rate */
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
        total += bridge->input_spikes[i];
    }
    bridge->state.avg_firing_rate = total / bridge->spike_buffer_size;

    /* Update reward prediction with TD learning */
    if (bridge->config.enable_rpe_computation) {
        bridge->state.reward_prediction =
            bridge->state.reward_prediction * bridge->config.discount_factor +
            bridge->state.da.current_rpe * bridge->config.rpe_learning_rate;
    }

    compute_modulation(bridge);

    /* Update statistics */
    bridge->stats.avg_da_level =
        bridge->stats.avg_da_level * 0.99f + bridge->state.da.da_level * 0.01f;
    bridge->stats.avg_rpe =
        bridge->stats.avg_rpe * 0.99f + fabsf(bridge->state.da.current_rpe) * 0.01f;

    return 0;
}

/*=============================================================================
 * Decoding Functions (SNN --> VTA)
 *===========================================================================*/

int nimcp_vta_snn_get_modulation(
    nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_snn_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }
    *modulation = bridge->current_modulation;
    return 0;
}

float nimcp_vta_snn_get_computed_rpe(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.da.current_rpe;
}

float nimcp_vta_snn_get_reward_prediction(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->state.reward_prediction;
}

bool nimcp_vta_snn_goal_achieved(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_goal_achieved: bridge is NULL");
        return false;
    }
    return bridge->current_modulation.goal_achieved;
}

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

int nimcp_vta_snn_get_state(
    const nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int nimcp_vta_snn_get_stats(
    const nimcp_vta_snn_bridge_t* bridge,
    nimcp_vta_snn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void nimcp_vta_snn_reset_stats(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

int nimcp_vta_snn_connect_bio_async(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_connect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->state.bio_async_connected = true;
    return 0;
}

int nimcp_vta_snn_disconnect_bio_async(nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->state.bio_async_connected = false;
    return 0;
}

bool nimcp_vta_snn_is_bio_async_connected(const nimcp_vta_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->state.bio_async_connected;
}

/*=============================================================================
 * External Modulation
 *===========================================================================*/

int nimcp_vta_snn_set_reward(
    nimcp_vta_snn_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_set_reward: bridge is NULL");
        return -1;
    }

    bridge->stats.reward_events++;
    bridge->stats.total_reward += reward;

    return nimcp_vta_snn_encode_reward(bridge, reward, bridge->state.reward_prediction);
}

int nimcp_vta_snn_set_goal(
    nimcp_vta_snn_bridge_t* bridge,
    uint32_t goal_id,
    float value,
    float distance
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_vta_snn_set_goal: bridge is NULL");
        return -1;
    }

    /* Update motivation based on goal value and distance */
    float proximity = 1.0f - nimcp_clampf(distance, 0.0f, 1.0f);
    bridge->state.da.motivation = value * (0.5f + 0.5f * proximity);
    bridge->state.da.wanting = value;

    compute_modulation(bridge);
    return 0;
}
