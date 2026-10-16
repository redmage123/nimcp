/**
 * @file nimcp_habenula_snn_bridge.c
 * @brief Implementation of Habenula-SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/habenula/nimcp_habenula_snn_bridge.h"
#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(habenula_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HABENULA_SNN_BRIDGE"


struct nimcp_habenula_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_habenula_snn_config_t config;
    nimcp_habenula_adapter_t habenula_adapter;
    void* snn;
    nimcp_habenula_snn_bridge_state_t state;
    nimcp_habenula_snn_modulation_t current_modulation;
    float* input_spikes;
    float* output_spikes;
    uint32_t spike_buffer_size;
    nimcp_habenula_snn_stats_t stats;
    uint64_t current_time_us;
};

nimcp_habenula_snn_config_t nimcp_habenula_snn_config_default(void) {
    return (nimcp_habenula_snn_config_t){
        .population_size = HABENULA_SNN_POPULATION_SIZE,
        .input_dim = HABENULA_SNN_INPUT_DIM,
        .output_dim = 32,
        .encoding = HABENULA_SNN_ENCODE_NEGATIVE_RPE,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .aversive_rate_boost = 3.0f,
        .relief_suppression = 0.8f,
        .decoding = HABENULA_SNN_DECODE_AVOIDANCE,
        .decoding_threshold = 0.3f,
        .temporal_smoothing = 0.1f,
        .enable_avoidance_output = true,
        .avoidance_gain = 1.0f,
        .inhibition_strength = 0.7f,
        .enable_error_output = true,
        .error_amplification = 1.5f,
        .dt_ms = HABENULA_SNN_DEFAULT_DT,
        .simulation_window_ms = 50.0f,
        .enable_bio_async = false,
        .enable_plasticity_bridge = false
    };
}

nimcp_habenula_snn_bridge_t* nimcp_habenula_snn_create(const nimcp_habenula_snn_config_t* config) {
    nimcp_habenula_snn_bridge_t* b = nimcp_calloc(1, sizeof(*b));
    if (!b) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "b is NULL");

        return NULL;

    }
    b->config = config ? *config : nimcp_habenula_snn_config_default();
    b->spike_buffer_size = b->config.population_size;
    b->input_spikes = nimcp_calloc(b->spike_buffer_size, sizeof(float));
    b->output_spikes = nimcp_calloc(b->spike_buffer_size, sizeof(float));
    if (!b->input_spikes || !b->output_spikes) { nimcp_habenula_snn_destroy(b); return NULL; }
    b->state.state = HABENULA_SNN_STATE_IDLE;
    return b;
}

void nimcp_habenula_snn_destroy(nimcp_habenula_snn_bridge_t* b) {
    if (!b) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "habenula_snn");
    nimcp_free(b->input_spikes);
    nimcp_free(b->output_spikes);
    nimcp_free(b);
}

int nimcp_habenula_snn_reset(nimcp_habenula_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_reset: b is NULL");
        return -1;
    }
    memset(&b->state, 0, sizeof(b->state));
    b->state.state = HABENULA_SNN_STATE_IDLE;
    memset(b->input_spikes, 0, b->spike_buffer_size * sizeof(float));
    memset(b->output_spikes, 0, b->spike_buffer_size * sizeof(float));
    memset(&b->stats, 0, sizeof(b->stats));
    return 0;
}

int nimcp_habenula_snn_connect_habenula(nimcp_habenula_snn_bridge_t* b, nimcp_habenula_adapter_t a) {
    if (!b || !a) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_connect_habenula: required parameter is NULL (b, a)");
        return -1;
    }
    b->habenula_adapter = a;
    return 0;
}

int nimcp_habenula_snn_connect_snn(nimcp_habenula_snn_bridge_t* b, struct nimcp_snn_network* s) {
    if (!b || !s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_connect_snn: required parameter is NULL (b, s)");
        return -1;
    }
    b->snn = s;
    return 0;
}

int nimcp_habenula_snn_encode_state(nimcp_habenula_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_encode_state: b is NULL");
        return -1;
    }
    b->state.state = HABENULA_SNN_STATE_ENCODING;
    int spikes = 0;
    float aversive = b->state.habenula.aversive_level;
    float rate = b->config.baseline_rate_hz * (1.0f + aversive * b->config.aversive_rate_boost);
    for (uint32_t i = 0; i < b->spike_buffer_size; i++) {
        float prob = rate * b->config.dt_ms / 1000.0f;
        b->input_spikes[i] = (nimcp_tl_rand() / (float)RAND_MAX) < prob ? 1.0f : 0.0f;
        if (b->input_spikes[i] > 0) spikes++;
    }
    b->stats.total_spikes_generated += spikes;
    b->state.state = HABENULA_SNN_STATE_IDLE;
    return spikes;
}

int nimcp_habenula_snn_encode_aversive(nimcp_habenula_snn_bridge_t* b, nimcp_habenula_snn_aversive_t event, float intensity) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_encode_aversive: b is NULL");
        return -1;
    }
    b->state.habenula.event_type = event;
    b->state.habenula.aversive_level = nimcp_clampf(intensity, 0.0f, 1.0f);
    b->state.habenula.last_aversive_us = b->current_time_us;
    b->stats.aversive_events++;
    return nimcp_habenula_snn_encode_state(b);
}

int nimcp_habenula_snn_encode_negative_rpe(nimcp_habenula_snn_bridge_t* b, float neg_rpe) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_encode_negative_rpe: b is NULL");
        return -1;
    }
    b->state.habenula.negative_rpe = nimcp_clampf(neg_rpe, 0.0f, 1.0f);
    b->current_modulation.error_signal = neg_rpe * b->config.error_amplification;
    return nimcp_habenula_snn_encode_state(b);
}

int nimcp_habenula_snn_encode_disappointment(nimcp_habenula_snn_bridge_t* b, float expected, float actual) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_encode_disappointment: b is NULL");
        return -1;
    }
    float disappointment = nimcp_clampf(expected - actual, 0.0f, 1.0f);
    b->state.habenula.disappointment = disappointment;
    return nimcp_habenula_snn_encode_aversive(b, HABENULA_SNN_AVERSIVE_DISAPPOINTMENT, disappointment);
}

int nimcp_habenula_snn_encode_relief(nimcp_habenula_snn_bridge_t* b, float relief) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_encode_relief: b is NULL");
        return -1;
    }
    b->state.habenula.relief = nimcp_clampf(relief, 0.0f, 1.0f);
    b->state.habenula.aversive_level *= (1.0f - relief * b->config.relief_suppression);
    b->stats.relief_events++;
    return 0;
}

int nimcp_habenula_snn_simulate(nimcp_habenula_snn_bridge_t* b, float duration_ms) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_simulate: b is NULL");
        return -1;
    }
    b->state.state = HABENULA_SNN_STATE_SIMULATING;
    for (float t = 0; t < duration_ms; t += b->config.dt_ms) nimcp_habenula_snn_step(b);
    b->state.state = HABENULA_SNN_STATE_IDLE;
    return 0;
}

int nimcp_habenula_snn_step(nimcp_habenula_snn_bridge_t* b) {
    if (!b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_habenula_snn_step: b is NULL");
        return -1;
    }
    b->stats.total_updates++;
    b->current_time_us++;
    float total = 0.0f;
    for (uint32_t i = 0; i < b->spike_buffer_size; i++) total += b->input_spikes[i];
    b->state.avg_firing_rate = total / b->spike_buffer_size;
    b->current_modulation.avoidance_signal = b->state.habenula.aversive_level * b->config.avoidance_gain;
    b->current_modulation.vta_inhibition = b->state.habenula.aversive_level * b->config.inhibition_strength;
    b->current_modulation.raphe_inhibition = b->state.habenula.aversive_level * b->config.inhibition_strength * 0.5f;
    b->current_modulation.frustration = b->state.habenula.disappointment;
    b->current_modulation.trigger_avoidance = b->current_modulation.avoidance_signal > 0.5f;
    b->state.vta_inhibition = b->current_modulation.vta_inhibition;
    b->state.raphe_inhibition = b->current_modulation.raphe_inhibition;
    b->stats.avg_aversive_level = b->stats.avg_aversive_level * 0.99f + b->state.habenula.aversive_level * 0.01f;
    b->stats.avg_negative_rpe = b->stats.avg_negative_rpe * 0.99f + b->state.habenula.negative_rpe * 0.01f;
    return 0;
}

int nimcp_habenula_snn_get_modulation(nimcp_habenula_snn_bridge_t* b, nimcp_habenula_snn_modulation_t* m) { if (!b||!m) return -1; *m = b->current_modulation; return 0; }
float nimcp_habenula_snn_get_avoidance(nimcp_habenula_snn_bridge_t* b) { return b ? b->current_modulation.avoidance_signal : 0.0f; }
float nimcp_habenula_snn_get_vta_inhibition(nimcp_habenula_snn_bridge_t* b) { return b ? b->current_modulation.vta_inhibition : 0.0f; }
float nimcp_habenula_snn_get_raphe_inhibition(nimcp_habenula_snn_bridge_t* b) { return b ? b->current_modulation.raphe_inhibition : 0.0f; }
bool nimcp_habenula_snn_should_avoid(nimcp_habenula_snn_bridge_t* b) { return b ? b->current_modulation.trigger_avoidance : false; }

int nimcp_habenula_snn_get_state(const nimcp_habenula_snn_bridge_t* b, nimcp_habenula_snn_bridge_state_t* s) { if (!b||!s) return -1; *s = b->state; return 0; }
int nimcp_habenula_snn_get_stats(const nimcp_habenula_snn_bridge_t* b, nimcp_habenula_snn_stats_t* s) { if (!b||!s) return -1; *s = b->stats; return 0; }
void nimcp_habenula_snn_reset_stats(nimcp_habenula_snn_bridge_t* b) { if (b) memset(&b->stats, 0, sizeof(b->stats)); }
int nimcp_habenula_snn_connect_bio_async(nimcp_habenula_snn_bridge_t* b) { if (!b) return -1; b->state.bio_async_connected = true; return 0; }
int nimcp_habenula_snn_disconnect_bio_async(nimcp_habenula_snn_bridge_t* b) { if (!b) return -1; b->state.bio_async_connected = false; return 0; }
bool nimcp_habenula_snn_is_bio_async_connected(const nimcp_habenula_snn_bridge_t* b) { return b ? b->state.bio_async_connected : false; }
int nimcp_habenula_snn_set_punishment(nimcp_habenula_snn_bridge_t* b, float p) { return nimcp_habenula_snn_encode_aversive(b, HABENULA_SNN_AVERSIVE_PUNISHMENT, p); }
int nimcp_habenula_snn_set_omission(nimcp_habenula_snn_bridge_t* b, float e) { return nimcp_habenula_snn_encode_aversive(b, HABENULA_SNN_AVERSIVE_OMISSION, e); }
