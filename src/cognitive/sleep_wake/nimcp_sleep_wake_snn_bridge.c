/**
 * @file nimcp_sleep_wake_snn_bridge.c
 * @brief Sleep-Wake - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/sleep_wake/nimcp_sleep_wake_snn_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/synapse_types/nimcp_synapse_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sleep_wake_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_sleep_wake_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_sleep_wake_snn_bridge_mesh_registry = NULL;

nimcp_error_t sleep_wake_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_sleep_wake_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "sleep_wake_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "sleep_wake_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_sleep_wake_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_sleep_wake_snn_bridge_mesh_registry = registry;
    return err;
}

void sleep_wake_snn_bridge_mesh_unregister(void) {
    if (g_sleep_wake_snn_bridge_mesh_registry && g_sleep_wake_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_sleep_wake_snn_bridge_mesh_registry, g_sleep_wake_snn_bridge_mesh_id);
        g_sleep_wake_snn_bridge_mesh_id = 0;
        g_sleep_wake_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from sleep_wake_snn_bridge module (instance-level) */
static inline void sleep_wake_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_sleep_wake_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_sleep_wake_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_sleep_wake_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SLEEP_WAKE_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct sleep_wake_snn_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    sleep_wake_snn_config_t config;
    snn_network_t* snn;

    /* State */
    sleep_wake_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    sleep_wake_dim_state_t dim_states[SLEEP_WAKE_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* arousal_buffer;

    /* Arousal state */
    sleep_wake_arousal_t last_arousal;
    float sleep_pressure_signal;
    float circadian_signal;

    /* Previous state for change detection */
    float* prev_state;
    uint32_t prev_stage;

    /* Callbacks */
    sleep_wake_snn_sleep_callback_t sleep_callback;
    void* sleep_callback_data;
    sleep_wake_snn_arousal_callback_t arousal_callback;
    void* arousal_callback_data;
    sleep_wake_snn_stage_callback_t stage_callback;
    void* stage_callback_data;

    /* Statistics */
    sleep_wake_snn_stats_t stats;
};

static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

sleep_wake_snn_config_t sleep_wake_snn_config_default(void) {
    sleep_wake_snn_config_t config = {
        .num_dimensions = SLEEP_WAKE_DIM_COUNT,
        .neurons_per_dim = SLEEP_WAKE_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = SLEEP_WAKE_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = SLEEP_WAKE_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = SLEEP_WAKE_SNN_DECODE_INTEGRATION,
        .arousal_threshold = SLEEP_WAKE_SNN_AROUSAL_THRESH,
        .sleep_threshold = 0.4f,
        .stage_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_oscillation_detection = true,
        .oscillation_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_stage_detection = true,
        .stage_detection_gain = 1.5f,
        .enable_circadian_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

sleep_wake_snn_bridge_t* sleep_wake_snn_create(const sleep_wake_snn_config_t* config) {
    sleep_wake_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(sleep_wake_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = sleep_wake_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > SLEEP_WAKE_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "sleep_wake_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "sleep_wake_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* sleep_pressure, arousal, circadian, wake_drive, sleep_drive, consolidation */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_wake_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->arousal_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->arousal_buffer || !bridge->prev_state) {
        sleep_wake_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sleep_wake_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize arousal to awake/alert state */
    bridge->last_arousal.sleep_pressure = 0.0f;
    bridge->last_arousal.arousal_level = 0.7f;
    bridge->last_arousal.circadian_phase = 0.5f;
    bridge->last_arousal.wake_drive = 0.7f;
    bridge->last_arousal.sleep_drive = 0.3f;
    bridge->last_arousal.high_arousal = false;
    bridge->last_arousal.sleep_onset = false;
    bridge->last_arousal.stage_confidence = 1.0f;
    bridge->last_arousal.consolidation_signal = 0.0f;
    bridge->last_arousal.detected_stage = 0; /* Awake */

    bridge->state = SLEEP_WAKE_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->sleep_pressure_signal = 0.0f;
    bridge->circadian_signal = 0.5f;
    bridge->prev_stage = 0;

    NIMCP_LOGGING_INFO("Created %s bridge", "sleep_wake_snn");
    return bridge;
}

void sleep_wake_snn_destroy(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sleep_wake_snn");

    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->arousal_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int sleep_wake_snn_reset(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset arousal to awake state */
    memset(&bridge->last_arousal, 0, sizeof(sleep_wake_arousal_t));
    bridge->last_arousal.arousal_level = 0.7f;
    bridge->last_arousal.wake_drive = 0.7f;
    bridge->last_arousal.circadian_phase = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->arousal_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = SLEEP_WAKE_SNN_STATE_IDLE;
    bridge->sleep_pressure_signal = 0.0f;
    bridge->circadian_signal = 0.5f;
    bridge->prev_stage = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int sleep_wake_snn_encode_state(
    sleep_wake_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_encode_state: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        float value = nimcp_clampf(dimensions[d], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Update dimension state */
        bridge->dim_states[d].activation = value;
        bridge->dim_states[d].mean_rate_hz = rate;

        /* Population encode */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
            float preferred = (neurons_per_dim <= 1) ? 0.5f : (float)n / (float)(neurons_per_dim - 1);
            float diff = value - preferred;
            float tuning = expf(-diff * diff / 0.1f);
            uint32_t idx = d * neurons_per_dim + n;
            bridge->encoding_buffer[idx] = tuning * bridge->config.encoding_gain;
            if (bridge->encoding_buffer[idx] > 0.5f) {
                total_spikes++;
                bridge->dim_states[d].spike_count++;
            }
        }
    }

    /* Detect state change */
    float change_magnitude = 0.0f;
    for (uint32_t d = 0; d < num_dims; d++) {
        float diff = dimensions[d] - bridge->prev_state[d];
        change_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    change_magnitude = sqrtf(change_magnitude / num_dims);

    if (change_magnitude > bridge->config.stage_change_threshold) {
        bridge->stats.stage_transitions++;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int sleep_wake_snn_encode_pressure(
    sleep_wake_snn_bridge_t* bridge,
    float pressure,
    float circadian_phase
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_encode_pressure: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SLEEP_WAKE_DIM_COUNT] = {0};
    dims[SLEEP_WAKE_DIM_SLEEP_PRESSURE] = nimcp_clampf(pressure, 0.0f, 1.0f);
    dims[SLEEP_WAKE_DIM_CIRCADIAN_PHASE] = nimcp_clampf(circadian_phase, 0.0f, 1.0f);

    /* Sleep drive increases with pressure */
    dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = pressure * 0.8f;
    /* Wake drive inversely related to pressure */
    dims[SLEEP_WAKE_DIM_WAKE_PROMOTION] = (1.0f - pressure) * 0.8f;

    bridge->sleep_pressure_signal = pressure;
    bridge->circadian_signal = circadian_phase;

    nimcp_mutex_unlock(bridge->base.mutex);

    return sleep_wake_snn_encode_state(bridge, dims, 4);
}

int sleep_wake_snn_encode_arousal(
    sleep_wake_snn_bridge_t* bridge,
    float arousal,
    float wake_drive
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_encode_arousal: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SLEEP_WAKE_DIM_COUNT] = {0};
    dims[SLEEP_WAKE_DIM_AROUSAL] = nimcp_clampf(arousal, 0.0f, 1.0f);
    dims[SLEEP_WAKE_DIM_WAKE_PROMOTION] = nimcp_clampf(wake_drive, 0.0f, 1.0f);
    dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = nimcp_clampf(1.0f - arousal, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return sleep_wake_snn_encode_state(bridge, dims, 3);
}

int sleep_wake_snn_encode_stage(
    sleep_wake_snn_bridge_t* bridge,
    uint32_t stage,
    float stage_depth
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_encode_stage: bridge is NULL");
        return -1;
    }
    if (stage > 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_encode_stage: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SLEEP_WAKE_DIM_COUNT] = {0};
    float depth = nimcp_clampf(stage_depth, 0.0f, 1.0f);

    /* Encode stage-specific activity */
    switch (stage) {
        case 0: /* Awake */
            dims[SLEEP_WAKE_DIM_AROUSAL] = 0.7f + 0.3f * depth;
            dims[SLEEP_WAKE_DIM_WAKE_PROMOTION] = 0.8f;
            dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = 0.2f;
            break;
        case 1: /* N1 - Light sleep */
            dims[SLEEP_WAKE_DIM_N1_ACTIVITY] = depth;
            dims[SLEEP_WAKE_DIM_AROUSAL] = 0.4f - 0.1f * depth;
            dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = 0.5f;
            break;
        case 2: /* N2 - Spindle sleep */
            dims[SLEEP_WAKE_DIM_N2_ACTIVITY] = depth;
            dims[SLEEP_WAKE_DIM_AROUSAL] = 0.3f - 0.1f * depth;
            dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = 0.7f;
            dims[SLEEP_WAKE_DIM_CONSOLIDATION] = 0.3f;
            break;
        case 3: /* N3 - Slow wave/deep sleep */
            dims[SLEEP_WAKE_DIM_N3_ACTIVITY] = depth;
            dims[SLEEP_WAKE_DIM_AROUSAL] = 0.1f;
            dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = 0.9f;
            dims[SLEEP_WAKE_DIM_CONSOLIDATION] = 0.8f + 0.2f * depth;
            break;
        case 4: /* REM */
            dims[SLEEP_WAKE_DIM_REM_ACTIVITY] = depth;
            dims[SLEEP_WAKE_DIM_AROUSAL] = 0.4f + 0.2f * depth; /* Brain active */
            dims[SLEEP_WAKE_DIM_SLEEP_PROMOTION] = 0.6f;
            dims[SLEEP_WAKE_DIM_CONSOLIDATION] = 0.5f;
            break;
    }

    /* Check for stage change */
    if (stage != bridge->prev_stage) {
        bridge->stats.stage_transitions++;

        if (bridge->stage_callback) {
            bridge->stage_callback(bridge, stage, bridge->prev_stage,
                                  bridge->stage_callback_data);
        }
        bridge->prev_stage = stage;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return sleep_wake_snn_encode_state(bridge, dims, SLEEP_WAKE_DIM_COUNT);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int sleep_wake_snn_simulate(sleep_wake_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_simulate: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SLEEP_WAKE_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    uint32_t steps = (uint32_t)(duration_ms / (fabsf(dt) > 1e-7f ? dt : 1e-7f));

    /* Set inputs before simulation */
    if (bridge->snn) {
        uint32_t input_size = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
        snn_network_set_inputs(bridge->snn, bridge->encoding_buffer, input_size);
    }

    for (uint32_t s = 0; s < steps; s++) {
        if (bridge->snn) {
            snn_network_step(bridge->snn, dt);
        }

        /* Update evidence integration */
        float decay = expf(-dt / bridge->config.integration_tau_ms);
        for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
            bridge->dim_states[d].accumulated_evidence *= decay;
            bridge->dim_states[d].accumulated_evidence +=
                bridge->dim_states[d].activation * dt / bridge->config.integration_tau_ms;
        }

        bridge->current_time_us += (uint64_t)(dt * 1000.0f);
        bridge->stats.total_simulations++;
    }

    /* Get output from SNN */
    if (bridge->snn) {
        snn_network_get_outputs(bridge->snn, bridge->output_buffer, 6);
    }

    /* Decode outputs */
    bridge->last_arousal.sleep_pressure = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_arousal.arousal_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_arousal.circadian_phase = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_arousal.wake_drive = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_arousal.sleep_drive = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_arousal.consolidation_signal = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check high arousal threshold */
    if (bridge->last_arousal.arousal_level > bridge->config.arousal_threshold) {
        bridge->last_arousal.high_arousal = true;
        bridge->stats.wake_detections++;
    } else {
        bridge->last_arousal.high_arousal = false;
    }

    /* Check sleep onset threshold */
    if (bridge->last_arousal.sleep_drive > bridge->config.sleep_threshold &&
        bridge->last_arousal.arousal_level < bridge->config.arousal_threshold) {
        bridge->last_arousal.sleep_onset = true;
        bridge->stats.sleep_onset_detections++;

        if (bridge->sleep_callback) {
            bridge->sleep_callback(bridge, bridge->last_arousal.sleep_pressure,
                                  bridge->current_time_us, bridge->sleep_callback_data);
        }
    } else {
        bridge->last_arousal.sleep_onset = false;
    }

    /* Detect sleep stage from activity patterns */
    float stage_activations[5] = {0};
    stage_activations[0] = bridge->dim_states[SLEEP_WAKE_DIM_AROUSAL].activation;
    stage_activations[1] = bridge->dim_states[SLEEP_WAKE_DIM_N1_ACTIVITY].activation;
    stage_activations[2] = bridge->dim_states[SLEEP_WAKE_DIM_N2_ACTIVITY].activation;
    stage_activations[3] = bridge->dim_states[SLEEP_WAKE_DIM_N3_ACTIVITY].activation;
    stage_activations[4] = bridge->dim_states[SLEEP_WAKE_DIM_REM_ACTIVITY].activation;

    uint32_t max_stage = 0;
    float max_activation = stage_activations[0];
    for (uint32_t i = 1; i < 5; i++) {
        if (stage_activations[i] > max_activation) {
            max_activation = stage_activations[i];
            max_stage = i;
        }
    }
    bridge->last_arousal.detected_stage = max_stage;
    bridge->last_arousal.stage_confidence = max_activation;

    bridge->stats.total_evaluations++;
    bridge->state = SLEEP_WAKE_SNN_STATE_IDLE;

    /* Snapshot callback state under mutex */
    sleep_wake_snn_arousal_callback_t arousal_cb = bridge->arousal_callback;
    void* arousal_cb_data = bridge->arousal_callback_data;
    sleep_wake_arousal_t arousal_snapshot = bridge->last_arousal;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside mutex to prevent deadlock */
    if (arousal_cb) {
        arousal_cb(bridge, &arousal_snapshot, arousal_cb_data);
    }

    return 0;
}

int sleep_wake_snn_step(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_step: bridge is NULL");
        return -1;
    }
    return sleep_wake_snn_simulate(bridge, bridge->config.dt_ms);
}

int sleep_wake_snn_forward(
    sleep_wake_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    int spike_count = sleep_wake_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_forward: validation failed");
        return -1;
    }

    if (sleep_wake_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int sleep_wake_snn_get_arousal(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_arousal_t* arousal
) {
    if (!bridge || !arousal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_get_arousal: required parameter is NULL (bridge, arousal)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *arousal = bridge->last_arousal;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_snn_get_activations(
    sleep_wake_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_get_activations: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool sleep_wake_snn_check_sleep_onset(
    sleep_wake_snn_bridge_t* bridge,
    float* sleep_pressure
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float pressure = bridge->last_arousal.sleep_pressure;
    if (sleep_pressure) {
        *sleep_pressure = pressure;
    }
    bool onset = bridge->last_arousal.sleep_onset;
    nimcp_mutex_unlock(bridge->base.mutex);

    return onset;
}

bool sleep_wake_snn_check_high_arousal(
    sleep_wake_snn_bridge_t* bridge,
    float* arousal_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_arousal.arousal_level;
    if (arousal_level) {
        *arousal_level = level;
    }
    bool high = level > bridge->config.arousal_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return high;
}

bool sleep_wake_snn_check_stage_change(
    sleep_wake_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        float diff = bridge->dim_states[d].activation - bridge->prev_state[d];
        mag += diff * diff;
    }
    mag = sqrtf(mag / bridge->config.num_dimensions);

    if (change_magnitude) {
        *change_magnitude = mag;
    }
    bool changed = mag > bridge->config.stage_change_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return changed;
}

//=============================================================================
// State Query Functions
//=============================================================================

int sleep_wake_snn_get_dim_state(
    sleep_wake_snn_bridge_t* bridge,
    uint32_t dim,
    sleep_wake_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sleep_wake_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_snn_get_state(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_arousal = bridge->last_arousal.arousal_level;
    state->sleep_pressure_signal = bridge->sleep_pressure_signal;
    state->circadian_signal = bridge->circadian_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        if (bridge->dim_states[d].activation > 0.1f) {
            state->active_dimensions++;
        }
        state->total_activity += bridge->dim_states[d].activation;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_snn_get_stats(sleep_wake_snn_bridge_t* bridge, sleep_wake_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_snn_reset_stats(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(sleep_wake_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float sleep_wake_snn_get_arousal_level(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float arousal = bridge->last_arousal.arousal_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return arousal;
}

float sleep_wake_snn_get_total_activity(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        total += bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return total;
}

//=============================================================================
// Callback Registration
//=============================================================================

int sleep_wake_snn_register_sleep_callback(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_sleep_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_register_sleep_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->sleep_callback = callback;
    bridge->sleep_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_snn_register_arousal_callback(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_arousal_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_register_arousal_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->arousal_callback = callback;
    bridge->arousal_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_snn_register_stage_callback(
    sleep_wake_snn_bridge_t* bridge,
    sleep_wake_snn_stage_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_register_stage_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stage_callback = callback;
    bridge->stage_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int sleep_wake_snn_bio_async_connect(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sleep_wake_snn_bio_async_disconnect(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool sleep_wake_snn_is_bio_async_connected(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_wake_snn_bridge_set_instance_health_agent(sleep_wake_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "sleep_wake_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_wake_snn_bridge_training_begin(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    sleep_wake_snn_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_snn_bridge_training_begin", 0.0f);
    return 0;
}

int sleep_wake_snn_bridge_training_end(sleep_wake_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_snn_bridge_training_end: NULL argument");
        return -1;
    }
    sleep_wake_snn_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_snn_bridge_training_end", 1.0f);
    return 0;
}

int sleep_wake_snn_bridge_training_step(sleep_wake_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sleep_wake_snn_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_snn_bridge_training_step", progress);
    return 0;
}
