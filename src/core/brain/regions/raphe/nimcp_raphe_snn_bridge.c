/**
 * @file nimcp_raphe_snn_bridge.c
 * @brief Implementation of Raphe-SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/raphe/nimcp_raphe_snn_bridge.h"
#include "core/brain/regions/raphe/nimcp_raphe_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

BRIDGE_BOILERPLATE_MESH_ONLY(raphe_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "RAPHE_SNN_BRIDGE"


struct nimcp_raphe_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_raphe_snn_config_t config;
    nimcp_raphe_adapter_t raphe_adapter;
    void* snn;
    nimcp_raphe_snn_bridge_state_t state;
    nimcp_raphe_snn_modulation_t current_modulation;
    float* input_spikes;
    float* output_spikes;
    uint32_t spike_buffer_size;
    nimcp_raphe_snn_stats_t stats;
    uint64_t current_time_us;
};

nimcp_raphe_snn_config_t nimcp_raphe_snn_config_default(void) {
    return (nimcp_raphe_snn_config_t){
        .population_size = RAPHE_SNN_POPULATION_SIZE,
        .input_dim = RAPHE_SNN_INPUT_DIM,
        .output_dim = 32,
        .encoding = RAPHE_SNN_ENCODE_RATE,
        .encoding_gain = 1.0f,
        .tonic_baseline_hz = 2.0f,
        .mood_encoding_scale = 1.0f,
        .decoding = RAPHE_SNN_DECODE_AVERAGE,
        .decoding_threshold = 0.1f,
        .temporal_smoothing = 0.1f,
        .enable_impulse_output = true,
        .inhibition_gain = 1.0f,
        .patience_scale = 1.0f,
        .enable_mood_output = true,
        .mood_smoothing = 0.1f,
        .mood_bias_scale = 0.5f,
        .dt_ms = RAPHE_SNN_DEFAULT_DT,
        .simulation_window_ms = 50.0f,
        .enable_bio_async = false,
        .enable_plasticity_bridge = false
    };
}

nimcp_raphe_snn_bridge_t* nimcp_raphe_snn_create(const nimcp_raphe_snn_config_t* config) {
    nimcp_raphe_snn_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "b is NULL");

        return NULL;

    }
    b->config = config ? *config : nimcp_raphe_snn_config_default();
    b->spike_buffer_size = b->config.population_size;
    b->input_spikes = nimcp_calloc(b->spike_buffer_size, sizeof(float));
    b->output_spikes = nimcp_calloc(b->spike_buffer_size, sizeof(float));
    if (!b->input_spikes || !b->output_spikes) { nimcp_raphe_snn_destroy(b); return NULL; }
    b->state.state = RAPHE_SNN_STATE_IDLE;
    b->state.ht.impulse_control = 0.5f;
    b->state.ht.patience = 0.5f;
    b->current_modulation.inhibition_strength = 0.5f;
    b->current_modulation.patience_level = 0.5f;
    return b;
}

void nimcp_raphe_snn_destroy(nimcp_raphe_snn_bridge_t* b) {
    if (!b) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "raphe_snn");
    nimcp_free(b->input_spikes);
    nimcp_free(b->output_spikes);
    nimcp_free(b);
}

int nimcp_raphe_snn_reset(nimcp_raphe_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_reset: b is NULL");
        return -1;
    }
    memset(&b->state, 0, sizeof(b->state));
    b->state.state = RAPHE_SNN_STATE_IDLE;
    memset(b->input_spikes, 0, b->spike_buffer_size * sizeof(float));
    memset(b->output_spikes, 0, b->spike_buffer_size * sizeof(float));
    memset(&b->stats, 0, sizeof(b->stats));
    return 0;
}

int nimcp_raphe_snn_connect_raphe(nimcp_raphe_snn_bridge_t* b, nimcp_raphe_adapter_t a) {
    if (!b || !a) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_connect_raphe: required parameter is NULL (b, a)");
        return -1;
    }
    b->raphe_adapter = a;
    return 0;
}

int nimcp_raphe_snn_connect_snn(nimcp_raphe_snn_bridge_t* b, struct nimcp_snn_network* s) {
    if (!b || !s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_connect_snn: required parameter is NULL (b, s)");
        return -1;
    }
    b->snn = s;
    return 0;
}

int nimcp_raphe_snn_encode_ht_state(nimcp_raphe_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_encode_ht_state: b is NULL");
        return -1;
    }
    b->state.state = RAPHE_SNN_STATE_ENCODING;
    int spikes = 0;
    float ht = b->state.ht.serotonin_level;
    float rate = b->config.tonic_baseline_hz * (1.0f + ht / 100.0f);
    for (uint32_t i = 0; i < b->spike_buffer_size; i++) {
        float prob = rate * b->config.dt_ms / 1000.0f;
        b->input_spikes[i] = (nimcp_tl_rand() / (float)RAND_MAX) < prob ? 1.0f : 0.0f;
        if (b->input_spikes[i] > 0) spikes++;
    }
    b->stats.total_spikes_generated += spikes;
    b->state.state = RAPHE_SNN_STATE_IDLE;
    return spikes;
}

int nimcp_raphe_snn_encode_mood(nimcp_raphe_snn_bridge_t* b, nimcp_raphe_snn_mood_t m, float intensity) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_encode_mood: b is NULL");
        return -1;
    }
    b->state.ht.mood = m;
    b->state.ht.mood_valence = (m == RAPHE_SNN_MOOD_POSITIVE) ? intensity :
                               (m == RAPHE_SNN_MOOD_NEGATIVE) ? -intensity : 0.0f;
    b->stats.mood_transitions++;
    return nimcp_raphe_snn_encode_ht_state(b);
}

int nimcp_raphe_snn_encode_impulse_control(nimcp_raphe_snn_bridge_t* b, float level) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_encode_impulse_control: b is NULL");
        return -1;
    }
    b->state.ht.impulse_control = nimcp_clampf(level, 0.0f, 1.0f);
    b->current_modulation.inhibition_strength = level;
    return 0;
}

int nimcp_raphe_snn_encode_temporal_horizon(nimcp_raphe_snn_bridge_t* b, float h) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_encode_temporal_horizon: b is NULL");
        return -1;
    }
    b->state.ht.temporal_horizon = h;
    b->current_modulation.temporal_discount = 1.0f / (1.0f + h);
    return 0;
}

int nimcp_raphe_snn_simulate(nimcp_raphe_snn_bridge_t* b, float duration_ms) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_simulate: b is NULL");
        return -1;
    }
    b->state.state = RAPHE_SNN_STATE_SIMULATING;
    for (float t = 0; t < duration_ms; t += b->config.dt_ms) nimcp_raphe_snn_step(b);
    b->state.state = RAPHE_SNN_STATE_IDLE;
    return 0;
}

int nimcp_raphe_snn_step(nimcp_raphe_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_step: b is NULL");
        return -1;
    }
    b->stats.total_updates++;
    float total = 0.0f;
    for (uint32_t i = 0; i < b->spike_buffer_size; i++) total += b->input_spikes[i];
    b->state.avg_firing_rate = total / b->spike_buffer_size;
    b->current_modulation.patience_level = b->state.ht.patience;
    b->current_modulation.mood_bias = b->state.ht.mood_valence * b->config.mood_bias_scale;
    b->current_modulation.risk_aversion = b->state.ht.serotonin_level / 100.0f;
    b->stats.avg_serotonin_level = b->stats.avg_serotonin_level * 0.99f + b->state.ht.serotonin_level * 0.01f;
    return 0;
}

int nimcp_raphe_snn_get_modulation(nimcp_raphe_snn_bridge_t* b, nimcp_raphe_snn_modulation_t* m) {
    if (!b || !m) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_get_modulation: required parameter is NULL (b, m)");
        return -1;
    }
    *m = b->current_modulation;
    return 0;
}

float nimcp_raphe_snn_get_inhibition(nimcp_raphe_snn_bridge_t* b) {
    return b ? b->current_modulation.inhibition_strength : 0.0f;
}

float nimcp_raphe_snn_get_patience(nimcp_raphe_snn_bridge_t* b) {
    return b ? b->current_modulation.patience_level : 0.0f;
}

float nimcp_raphe_snn_get_mood_bias(nimcp_raphe_snn_bridge_t* b) {
    return b ? b->current_modulation.mood_bias : 0.0f;
}

int nimcp_raphe_snn_get_state(const nimcp_raphe_snn_bridge_t* b, nimcp_raphe_snn_bridge_state_t* s) {
    if (!b || !s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_get_state: required parameter is NULL (b, s)");
        return -1;
    }
    *s = b->state;
    return 0;
}

int nimcp_raphe_snn_get_stats(const nimcp_raphe_snn_bridge_t* b, nimcp_raphe_snn_stats_t* s) {
    if (!b || !s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_get_stats: required parameter is NULL (b, s)");
        return -1;
    }
    *s = b->stats;
    return 0;
}

void nimcp_raphe_snn_reset_stats(nimcp_raphe_snn_bridge_t* b) {
    if (b) memset(&b->stats, 0, sizeof(b->stats));
}

int nimcp_raphe_snn_connect_bio_async(nimcp_raphe_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_connect_bio_async: b is NULL");
        return -1;
    }
    b->state.bio_async_connected = true;
    return 0;
}

int nimcp_raphe_snn_disconnect_bio_async(nimcp_raphe_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_disconnect_bio_async: b is NULL");
        return -1;
    }
    b->state.bio_async_connected = false;
    return 0;
}

bool nimcp_raphe_snn_is_bio_async_connected(const nimcp_raphe_snn_bridge_t* b) {
    return b ? b->state.bio_async_connected : false;
}

int nimcp_raphe_snn_set_mood(nimcp_raphe_snn_bridge_t* b, nimcp_raphe_snn_mood_t m, float i) {
    return nimcp_raphe_snn_encode_mood(b, m, i);
}

int nimcp_raphe_snn_impulse_trigger(nimcp_raphe_snn_bridge_t* b, float u) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_raphe_snn_impulse_trigger: b is NULL");
        return -1;
    }
    if (b->state.ht.impulse_control < u) b->stats.impulse_inhibitions++;
    return 0;
}
