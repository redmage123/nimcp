/**
 * @file nimcp_lc_snn_bridge.c
 * @brief Implementation of LC-SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/locus_coeruleus/nimcp_lc_snn_bridge.h"
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
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lc_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "LC_SNN_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct nimcp_lc_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    nimcp_lc_snn_config_t config;

    /* Connected systems */
    nimcp_lc_adapter_t lc_adapter;
    void* snn;  /* Generic pointer for SNN network */

    /* State */
    nimcp_lc_snn_bridge_state_t state;
    nimcp_lc_snn_modulation_t current_modulation;

    /* Spike buffers */
    float* input_spikes;
    float* output_spikes;
    uint32_t spike_buffer_size;

    /* Statistics */
    nimcp_lc_snn_stats_t stats;

    /* Timestamps */
    uint64_t current_time_us;
    uint64_t last_update_us;
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static uint64_t get_timestamp_us(void) {
    /* Platform-independent timestamp - could be replaced with actual timer */
    static _Atomic uint64_t counter = 0;
    return ++counter;
}

static void compute_modulation(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) return;

    float ne_level = bridge->state.ne.ne_level;
    float arousal = bridge->state.ne.arousal;

    /* Compute gain based on NE level */
    float gain_range = bridge->config.max_gain - bridge->config.min_gain;
    bridge->current_modulation.gain = bridge->config.min_gain +
        (gain_range * nimcp_clampf(ne_level / 100.0f, 0.0f, 1.0f));

    /* Attention boost from arousal */
    bridge->current_modulation.attention_boost = arousal;

    /* Noise suppression from NE */
    bridge->current_modulation.noise_suppression =
        nimcp_clampf(ne_level / 80.0f, 0.0f, 1.0f);

    /* Exploration drive (high tonic = explore, phasic = exploit) */
    if (bridge->state.ne.mode == LC_SNN_MODE_TONIC ||
        bridge->state.ne.mode == LC_SNN_MODE_EXPLORATORY) {
        bridge->current_modulation.exploration_drive = 0.7f;
    } else {
        bridge->current_modulation.exploration_drive = 0.3f;
    }

    /* Attention reset trigger */
    bridge->current_modulation.trigger_reset =
        (bridge->state.ne.novelty_response > 0.7f);
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

nimcp_lc_snn_config_t nimcp_lc_snn_config_default(void) {
    nimcp_lc_snn_config_t config = {
        .population_size = LC_SNN_POPULATION_SIZE,
        .input_dim = LC_SNN_INPUT_DIM,
        .output_dim = 32,

        .encoding = LC_SNN_ENCODE_RATE,
        .encoding_gain = 1.0f,
        .tonic_baseline_hz = 3.0f,
        .phasic_burst_hz = 20.0f,
        .burst_duration_ms = 50.0f,

        .decoding = LC_SNN_DECODE_AVERAGE,
        .decoding_threshold = 0.1f,
        .temporal_smoothing = 0.1f,

        .enable_gain_modulation = true,
        .min_gain = 0.5f,
        .max_gain = 2.0f,
        .gain_time_constant_ms = 100.0f,

        .dt_ms = LC_SNN_DEFAULT_DT,
        .simulation_window_ms = LC_SNN_ENCODING_WINDOW,

        .enable_bio_async = false,
        .enable_plasticity_bridge = false
    };
    return config;
}

nimcp_lc_snn_bridge_t* nimcp_lc_snn_create(const nimcp_lc_snn_config_t* config) {
    nimcp_lc_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(nimcp_lc_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_lc_snn_config_default();
    }

    /* Allocate spike buffers */
    bridge->spike_buffer_size = bridge->config.population_size;
    bridge->input_spikes = nimcp_calloc(bridge->spike_buffer_size, sizeof(float));
    bridge->output_spikes = nimcp_calloc(bridge->spike_buffer_size, sizeof(float));

    if (!bridge->input_spikes || !bridge->output_spikes) {
        nimcp_lc_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_lc_snn_create: required parameter is NULL (bridge->input_spikes, bridge->output_spikes)");
        return NULL;
    }

    /* Initialize state */
    bridge->state.state = LC_SNN_STATE_IDLE;
    bridge->state.ne.mode = LC_SNN_MODE_TONIC;
    bridge->state.current_gain = 1.0f;

    /* Initialize modulation */
    bridge->current_modulation.gain = 1.0f;
    bridge->current_modulation.attention_boost = 0.5f;
    bridge->current_modulation.noise_suppression = 0.5f;
    bridge->current_modulation.exploration_drive = 0.5f;
    bridge->current_modulation.trigger_reset = false;

    NIMCP_LOGGING_INFO("Created %s bridge", "lc_snn");
    return bridge;
}

void nimcp_lc_snn_destroy(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "lc_snn");

    nimcp_free(bridge->input_spikes);
    nimcp_free(bridge->output_spikes);
    nimcp_free(bridge);
}

int nimcp_lc_snn_reset(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_reset: bridge is NULL");
        return -1;
    }

    /* Reset state */
    memset(&bridge->state, 0, sizeof(bridge->state));
    bridge->state.state = LC_SNN_STATE_IDLE;
    bridge->state.ne.mode = LC_SNN_MODE_TONIC;
    bridge->state.current_gain = 1.0f;

    /* Reset buffers */
    memset(bridge->input_spikes, 0, bridge->spike_buffer_size * sizeof(float));
    memset(bridge->output_spikes, 0, bridge->spike_buffer_size * sizeof(float));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

/*=============================================================================
 * Connection API
 *===========================================================================*/

int nimcp_lc_snn_connect_lc(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_adapter_t lc_adapter
) {
    if (!bridge || !lc_adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_connect_lc: required parameter is NULL (bridge, lc_adapter)");
        return -1;
    }
    bridge->lc_adapter = lc_adapter;
    return 0;
}

int nimcp_lc_snn_connect_snn(
    nimcp_lc_snn_bridge_t* bridge,
    struct nimcp_snn_network* snn
) {
    if (!bridge || !snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_connect_snn: required parameter is NULL (bridge, snn)");
        return -1;
    }
    bridge->snn = snn;
    return 0;
}

/*=============================================================================
 * Encoding Functions (LC --> SNN)
 *===========================================================================*/

int nimcp_lc_snn_encode_ne_state(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_encode_ne_state: bridge is NULL");
        return -1;
    }

    bridge->state.state = LC_SNN_STATE_ENCODING;
    int spikes_generated = 0;

    float ne_level = bridge->state.ne.ne_level;
    float rate_factor = ne_level / 100.0f;

    /* Generate spikes based on encoding method */
    switch (bridge->config.encoding) {
        case LC_SNN_ENCODE_RATE:
            /* Rate coding: higher NE = higher firing rate */
            for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
                float rate = bridge->config.tonic_baseline_hz +
                    rate_factor * (bridge->config.phasic_burst_hz -
                                   bridge->config.tonic_baseline_hz);
                /* Probability of spike in this timestep */
                float spike_prob = rate * bridge->config.dt_ms / 1000.0f;
                bridge->input_spikes[i] = (nimcp_tl_rand() / (float)RAND_MAX) < spike_prob ? 1.0f : 0.0f;
                if (bridge->input_spikes[i] > 0) spikes_generated++;
            }
            break;

        case LC_SNN_ENCODE_BURST:
            /* Burst coding: synchronized volleys */
            if (bridge->state.ne.mode == LC_SNN_MODE_PHASIC) {
                for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
                    bridge->input_spikes[i] = 1.0f;
                    spikes_generated++;
                }
            }
            break;

        case LC_SNN_ENCODE_POPULATION:
            /* Population vector coding */
            for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
                float preferred_ne = (float)i / bridge->spike_buffer_size * 100.0f;
                float diff = ne_level - preferred_ne;
                float response = expf(-diff * diff / 400.0f);
                bridge->input_spikes[i] = response;
                if (response > 0.5f) spikes_generated++;
            }
            break;

        default:
            break;
    }

    bridge->stats.total_spikes_generated += spikes_generated;
    bridge->state.state = LC_SNN_STATE_IDLE;

    return spikes_generated;
}

int nimcp_lc_snn_encode_burst(
    nimcp_lc_snn_bridge_t* bridge,
    float intensity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_encode_burst: bridge is NULL");
        return -1;
    }

    bridge->state.ne.mode = LC_SNN_MODE_PHASIC;
    bridge->stats.total_bursts++;

    int spikes = 0;
    float burst_rate = intensity * bridge->config.phasic_burst_hz;

    for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
        float spike_prob = burst_rate * bridge->config.dt_ms / 1000.0f;
        bridge->input_spikes[i] = (nimcp_tl_rand() / (float)RAND_MAX) < spike_prob ? 1.0f : 0.0f;
        if (bridge->input_spikes[i] > 0) spikes++;
    }

    bridge->stats.total_spikes_generated += spikes;
    return spikes;
}

int nimcp_lc_snn_encode_novelty(
    nimcp_lc_snn_bridge_t* bridge,
    float novelty_score
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_encode_novelty: bridge is NULL");
        return -1;
    }

    bridge->state.ne.novelty_response = novelty_score;
    bridge->stats.novelty_events++;

    /* Novelty triggers phasic burst */
    if (novelty_score > 0.5f) {
        return nimcp_lc_snn_encode_burst(bridge, novelty_score);
    }

    return 0;
}

int nimcp_lc_snn_encode_arousal(
    nimcp_lc_snn_bridge_t* bridge,
    float arousal,
    float vigilance
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_encode_arousal: bridge is NULL");
        return -1;
    }

    bridge->state.ne.arousal = nimcp_clampf(arousal, 0.0f, 1.0f);
    bridge->state.ne.vigilance = nimcp_clampf(vigilance, 0.0f, 1.0f);

    /* Update gain modulation */
    compute_modulation(bridge);

    /* Encode current state */
    return nimcp_lc_snn_encode_ne_state(bridge);
}

/*=============================================================================
 * Simulation Functions
 *===========================================================================*/

int nimcp_lc_snn_simulate(nimcp_lc_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_simulate: bridge is NULL");
        return -1;
    }

    bridge->state.state = LC_SNN_STATE_SIMULATING;

    /* Simulate for duration */
    float time = 0.0f;
    while (time < duration_ms) {
        nimcp_lc_snn_step(bridge);
        time += bridge->config.dt_ms;
    }

    bridge->state.state = LC_SNN_STATE_IDLE;
    return 0;
}

int nimcp_lc_snn_step(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_step: bridge is NULL");
        return -1;
    }

    bridge->stats.total_updates++;
    bridge->current_time_us = get_timestamp_us();

    /* Update NE state from LC adapter if connected */
    if (bridge->lc_adapter) {
        nimcp_lc_adapter_state_t lc_state;
        if (nimcp_lc_adapter_get_state(bridge->lc_adapter, &lc_state) == 0) {
            /* Extract NE level from adapter - using generic activity metric */
            bridge->state.ne.ne_level = 50.0f; /* Default, would need adapter API */
        }
    }

    /* Compute firing rate statistics */
    float total_rate = 0.0f;
    for (uint32_t i = 0; i < bridge->spike_buffer_size; i++) {
        total_rate += bridge->input_spikes[i];
    }
    bridge->state.avg_firing_rate = total_rate / bridge->spike_buffer_size;

    /* Update modulation */
    compute_modulation(bridge);

    /* Track average NE level */
    bridge->stats.avg_ne_level =
        bridge->stats.avg_ne_level * 0.99f + bridge->state.ne.ne_level * 0.01f;

    bridge->last_update_us = bridge->current_time_us;
    return 0;
}

/*=============================================================================
 * Decoding Functions (SNN --> LC)
 *===========================================================================*/

int nimcp_lc_snn_get_modulation(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_modulation_t* modulation
) {
    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_get_modulation: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    *modulation = bridge->current_modulation;
    return 0;
}

int nimcp_lc_snn_get_ne_feedback(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_ne_state_t* ne_state
) {
    if (!bridge || !ne_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_get_ne_feedback: required parameter is NULL (bridge, ne_state)");
        return -1;
    }

    *ne_state = bridge->state.ne;
    return 0;
}

bool nimcp_lc_snn_should_reset_attention(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_should_reset_attention: bridge is NULL");
        return false;
    }
    return bridge->current_modulation.trigger_reset;
}

float nimcp_lc_snn_get_exploration_drive(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) return 0.5f;
    return bridge->current_modulation.exploration_drive;
}

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

int nimcp_lc_snn_get_state(
    const nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int nimcp_lc_snn_get_stats(
    const nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void nimcp_lc_snn_reset_stats(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

int nimcp_lc_snn_connect_bio_async(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_connect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->state.bio_async_connected = true;
    return 0;
}

int nimcp_lc_snn_disconnect_bio_async(nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    bridge->state.bio_async_connected = false;
    return 0;
}

bool nimcp_lc_snn_is_bio_async_connected(const nimcp_lc_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->state.bio_async_connected;
}

/*=============================================================================
 * External Modulation
 *===========================================================================*/

int nimcp_lc_snn_apply_external_gain(
    nimcp_lc_snn_bridge_t* bridge,
    float external_gain
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_apply_external_gain: bridge is NULL");
        return -1;
    }

    bridge->state.current_gain = nimcp_clampf(external_gain,
        bridge->config.min_gain, bridge->config.max_gain);
    bridge->current_modulation.gain = bridge->state.current_gain;

    return 0;
}

int nimcp_lc_snn_set_mode(
    nimcp_lc_snn_bridge_t* bridge,
    nimcp_lc_snn_mode_t mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_lc_snn_set_mode: bridge is NULL");
        return -1;
    }

    bridge->state.ne.mode = mode;

    /* Update exploration drive based on mode */
    switch (mode) {
        case LC_SNN_MODE_EXPLORATORY:
            bridge->current_modulation.exploration_drive = 0.8f;
            break;
        case LC_SNN_MODE_EXPLOITATIVE:
            bridge->current_modulation.exploration_drive = 0.2f;
            break;
        case LC_SNN_MODE_PHASIC:
            bridge->current_modulation.exploration_drive = 0.3f;
            break;
        default:
            bridge->current_modulation.exploration_drive = 0.5f;
            break;
    }

    return 0;
}
