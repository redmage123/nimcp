/**
 * @file nimcp_consolidation_snn_bridge.c
 * @brief Consolidation - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/consolidation/nimcp_consolidation_snn_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(consolidation_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "CONSOLIDATION_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct consolidation_snn_bridge {
    bridge_base_t base;  /* MUST be first member for bridge_base pattern */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    consolidation_snn_config_t config;
    snn_network_t* snn;

    /* State */
    consolidation_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    consolidation_dim_state_t dim_states[CONSOLIDATION_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* consolidation_buffer;

    /* Memory state */
    consolidation_memory_state_t last_state;
    float replay_signal;
    float stabilization_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    consolidation_snn_replay_callback_t replay_callback;
    void* replay_callback_data;
    consolidation_snn_state_callback_t state_callback;
    void* state_callback_data;
    consolidation_snn_stabilization_callback_t stabilization_callback;
    void* stabilization_callback_data;

    /* Statistics */
    consolidation_snn_stats_t stats;
};

static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                consolidation_snn_bridge_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

consolidation_snn_config_t consolidation_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_co", 0.0f);


    consolidation_snn_config_t config = {
        .num_dimensions = CONSOLIDATION_DIM_COUNT,
        .neurons_per_dim = CONSOLIDATION_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = CONSOLIDATION_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = CONSOLIDATION_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = CONSOLIDATION_SNN_DECODE_INTEGRATION,
        .stabilization_threshold = CONSOLIDATION_SNN_STABILIZATION_THRESH,
        .replay_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_replay_detection = true,
        .replay_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_ltp_tracking = true,
        .ltp_gain = 1.5f,
        .enable_schema_integration = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

consolidation_snn_bridge_t* consolidation_snn_create(const consolidation_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_cr", 0.0f);


    consolidation_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(consolidation_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = consolidation_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > CONSOLIDATION_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "consolidation_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "consolidation_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* replay, stabilization, ltp, schema, ripple, transfer */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "consolidation_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->consolidation_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->consolidation_buffer || !bridge->prev_state) {
        consolidation_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "consolidation_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize memory state to neutral */
    bridge->last_state.replay_strength = 0.5f;
    bridge->last_state.stabilization_level = 0.5f;
    bridge->last_state.ltp_state = 0.5f;
    bridge->last_state.schema_integration = 0.5f;
    bridge->last_state.sleep_phase = 0.5f;
    bridge->last_state.replay_detected = false;
    bridge->last_state.consolidation_active = false;
    bridge->last_state.consolidation_strength = 0.0f;
    bridge->last_state.ripple_activity = 0.5f;
    bridge->last_state.transfer_progress = 0.5f;

    bridge->state = CONSOLIDATION_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->replay_signal = 0.0f;
    bridge->stabilization_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "consolidation_snn");
    return bridge;
}

void consolidation_snn_destroy(consolidation_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "consolidation_snn");

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_de", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->consolidation_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int consolidation_snn_reset(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset memory state */
    memset(&bridge->last_state, 0, sizeof(consolidation_memory_state_t));
    bridge->last_state.replay_strength = 0.5f;
    bridge->last_state.stabilization_level = 0.5f;
    bridge->last_state.ltp_state = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->consolidation_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = CONSOLIDATION_SNN_STATE_IDLE;
    bridge->replay_signal = 0.0f;
    bridge->stabilization_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int consolidation_snn_encode_state(
    consolidation_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = CONSOLIDATION_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float value = nimcp_clampf(dimensions[d], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Update dimension state */
        bridge->dim_states[d].activation = value;
        bridge->dim_states[d].mean_rate_hz = rate;

        /* Population encode */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_dim > 256) {
                consolidation_snn_bridge_heartbeat("consolidatio_loop",
                                 (float)(n + 1) / (float)neurons_per_dim);
            }

            float preferred = (neurons_per_dim > 1) ? ((float)n / (float)(neurons_per_dim - 1)) : 0.0f;
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
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - bridge->prev_state[d];
        change_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    change_magnitude = sqrtf(change_magnitude / num_dims);

    if (change_magnitude > bridge->config.state_change_threshold) {
        bridge->stats.state_changes++;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int consolidation_snn_encode_replay(
    consolidation_snn_bridge_t* bridge,
    float replay_strength,
    uint32_t sequence_idx
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_encode_replay: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[CONSOLIDATION_DIM_COUNT] = {0};
    dims[CONSOLIDATION_DIM_REPLAY_STRENGTH] = nimcp_clampf(replay_strength, 0.0f, 1.0f);
    dims[CONSOLIDATION_DIM_RIPPLE_ACTIVITY] = nimcp_clampf(replay_strength * 0.8f, 0.0f, 1.0f);
    dims[CONSOLIDATION_DIM_STABILIZATION] = (replay_strength + 0.5f) / 2.0f;

    bridge->replay_signal = replay_strength;

    nimcp_mutex_unlock(bridge->base.mutex);

    return consolidation_snn_encode_state(bridge, dims, 3);
}

int consolidation_snn_encode_ltp(
    consolidation_snn_bridge_t* bridge,
    float ltp_level,
    uint32_t synapse_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_encode_ltp: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[CONSOLIDATION_DIM_COUNT] = {0};
    dims[CONSOLIDATION_DIM_LTP_STATE] = nimcp_clampf(ltp_level, 0.0f, 1.0f);
    dims[CONSOLIDATION_DIM_STABILIZATION] = nimcp_clampf(ltp_level * 0.9f, 0.0f, 1.0f);
    dims[CONSOLIDATION_DIM_TRANSFER_PROGRESS] = nimcp_clampf((float)synapse_count / 1000.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return consolidation_snn_encode_state(bridge, dims, 3);
}

int consolidation_snn_encode_schema(
    consolidation_snn_bridge_t* bridge,
    float integration_level,
    uint32_t schema_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_encode_schema: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[CONSOLIDATION_DIM_COUNT] = {0};
    dims[CONSOLIDATION_DIM_SCHEMA_INTEGRATION] = nimcp_clampf(integration_level, 0.0f, 1.0f);
    dims[CONSOLIDATION_DIM_TRANSFER_PROGRESS] = nimcp_clampf(integration_level * 0.8f, 0.0f, 1.0f);

    bridge->stabilization_signal = integration_level;

    /* Snapshot callback under mutex */
    consolidation_snn_stabilization_callback_t stab_cb = NULL;
    void* stab_cb_data = NULL;
    if (integration_level > bridge->config.stabilization_threshold) {
        bridge->last_state.consolidation_active = true;
        bridge->last_state.consolidation_strength = integration_level;
        bridge->stats.consolidation_events++;

        stab_cb = bridge->stabilization_callback;
        stab_cb_data = bridge->stabilization_callback_data;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside mutex to prevent deadlock */
    if (stab_cb) {
        stab_cb(bridge, integration_level, schema_type, stab_cb_data);
    }

    return consolidation_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int consolidation_snn_simulate(consolidation_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_si", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = CONSOLIDATION_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    uint32_t steps = (uint32_t)(duration_ms / (fabsf(dt) > 1e-7f ? dt : 1e-7f));

    /* Set inputs before simulation */
    if (bridge->snn) {
        uint32_t input_size = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
        snn_network_set_inputs(bridge->snn, bridge->encoding_buffer, input_size);
    }

    for (uint32_t s = 0; s < steps; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && steps > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(s + 1) / (float)steps);
        }

        if (bridge->snn) {
            snn_network_step(bridge->snn, dt);
        }

        /* Update evidence integration */
        float decay = expf(-dt / bridge->config.integration_tau_ms);
        for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
                consolidation_snn_bridge_heartbeat("consolidatio_loop",
                                 (float)(d + 1) / (float)bridge->config.num_dimensions);
            }

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
    bridge->last_state.replay_strength = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_state.stabilization_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_state.ltp_state = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_state.schema_integration = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_state.ripple_activity = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_state.transfer_progress = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check replay threshold */
    if (bridge->last_state.replay_strength > bridge->config.replay_threshold) {
        bridge->last_state.replay_detected = true;
        bridge->stats.replay_detections++;
    } else {
        bridge->last_state.replay_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = CONSOLIDATION_SNN_STATE_IDLE;

    /* Snapshot callback state under mutex */
    consolidation_snn_replay_callback_t replay_cb = bridge->replay_callback;
    void* replay_cb_data = bridge->replay_callback_data;
    float replay_str = bridge->last_state.replay_strength;
    uint64_t cur_time = bridge->current_time_us;
    bool do_replay_cb = (replay_cb != NULL && bridge->last_state.replay_detected);

    consolidation_snn_state_callback_t state_cb = bridge->state_callback;
    void* state_cb_data = bridge->state_callback_data;
    consolidation_memory_state_t state_snapshot = bridge->last_state;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside mutex to prevent deadlock */
    if (do_replay_cb) {
        replay_cb(bridge, replay_str, cur_time, replay_cb_data);
    }
    if (state_cb) {
        state_cb(bridge, &state_snapshot, state_cb_data);
    }

    return 0;
}

int consolidation_snn_step(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_st", 0.0f);


    return consolidation_snn_simulate(bridge, bridge->config.dt_ms);
}

int consolidation_snn_forward(
    consolidation_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_fo", 0.0f);


    int spike_count = consolidation_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_snn_forward: validation failed");
        return -1;
    }

    if (consolidation_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int consolidation_snn_get_memory_state(
    consolidation_snn_bridge_t* bridge,
    consolidation_memory_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_get_memory_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->last_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int consolidation_snn_get_activations(
    consolidation_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool consolidation_snn_check_replay(
    consolidation_snn_bridge_t* bridge,
    float* replay_strength
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_state.replay_strength;
    if (replay_strength) {
        *replay_strength = level;
    }
    bool detected = level > bridge->config.replay_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool consolidation_snn_check_stabilization(
    consolidation_snn_bridge_t* bridge,
    float* stabilization_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_state.stabilization_level;
    if (stabilization_level) {
        *stabilization_level = level;
    }
    bool detected = level > bridge->config.stabilization_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool consolidation_snn_check_state_change(
    consolidation_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(d + 1) / (float)bridge->config.num_dimensions);
        }

        float diff = bridge->dim_states[d].activation - bridge->prev_state[d];
        mag += diff * diff;
    }
    mag = sqrtf(mag / bridge->config.num_dimensions);

    if (change_magnitude) {
        *change_magnitude = mag;
    }
    bool changed = mag > bridge->config.state_change_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return changed;
}

//=============================================================================
// State Query Functions
//=============================================================================

int consolidation_snn_get_dim_state(
    consolidation_snn_bridge_t* bridge,
    uint32_t dim,
    consolidation_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "consolidation_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int consolidation_snn_get_state(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_consolidation = bridge->last_state.stabilization_level;
    state->replay_signal = bridge->replay_signal;
    state->stabilization_signal = bridge->stabilization_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(d + 1) / (float)bridge->config.num_dimensions);
        }

        if (bridge->dim_states[d].activation > 0.1f) {
            state->active_dimensions++;
        }
        state->total_activity += bridge->dim_states[d].activation;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int consolidation_snn_get_stats(consolidation_snn_bridge_t* bridge, consolidation_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int consolidation_snn_reset_stats(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(consolidation_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float consolidation_snn_get_consolidation_level(consolidation_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float consolidation = bridge->last_state.stabilization_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return consolidation;
}

float consolidation_snn_get_total_activity(consolidation_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            consolidation_snn_bridge_heartbeat("consolidatio_loop",
                             (float)(d + 1) / (float)bridge->config.num_dimensions);
        }

        total += bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return total;
}

//=============================================================================
// Callback Registration
//=============================================================================

int consolidation_snn_register_replay_callback(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_replay_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_register_replay_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->replay_callback = callback;
    bridge->replay_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int consolidation_snn_register_state_callback(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_state_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_register_state_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state_callback = callback;
    bridge->state_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int consolidation_snn_register_stabilization_callback(
    consolidation_snn_bridge_t* bridge,
    consolidation_snn_stabilization_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_register_stabilization_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stabilization_callback = callback;
    bridge->stabilization_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int consolidation_snn_bio_async_connect(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int consolidation_snn_bio_async_disconnect(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool consolidation_snn_is_bio_async_connected(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    consolidation_snn_bridge_heartbeat("consolidatio_consolidation_snn_is", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

void consolidation_snn_bridge_set_instance_health_agent(consolidation_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "consolidation_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
    g_consolidation_snn_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("consolidation_snn_bridge: instance health agent %s", agent ? "set" : "cleared");
}

int consolidation_snn_bridge_training_begin(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    consolidation_snn_bridge_heartbeat_instance(bridge, "consol_snn_training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int consolidation_snn_bridge_training_end(consolidation_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_snn_bridge_training_end: NULL argument");
        return -1;
    }
    consolidation_snn_bridge_heartbeat_instance(bridge, "consol_snn_training_end", 1.0f);
    (void)bridge;
    return 0;
}

int consolidation_snn_bridge_training_step(consolidation_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    consolidation_snn_bridge_heartbeat_instance(bridge, "consol_snn_training_step", progress);
    (void)bridge;
    return 0;
}
