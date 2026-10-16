/**
 * @file nimcp_parietal_snn_bridge.c
 * @brief Parietal - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/parietal/nimcp_parietal_snn_bridge.h"
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

#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(parietal_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from parietal_snn_bridge module (instance-level) */
static inline void parietal_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_parietal_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_parietal_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_parietal_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PARIETAL_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct parietal_snn_bridge {
    bridge_base_t base;
    parietal_snn_config_t config;
    snn_network_t* snn;

    /* State */
    parietal_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    parietal_dim_state_t dim_states[PARIETAL_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* spatial_buffer;

    /* Spatial output state */
    parietal_spatial_output_t last_spatial;
    float attention_signal;
    float numerical_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    parietal_snn_attention_callback_t attention_callback;
    void* attention_callback_data;
    parietal_snn_spatial_callback_t spatial_callback;
    void* spatial_callback_data;
    parietal_snn_precision_callback_t precision_callback;
    void* precision_callback_data;

    /* Statistics */
    parietal_snn_stats_t stats;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(parietal_snn_bridge)

//=============================================================================
// Helper Functions
//=============================================================================

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
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                parietal_snn_bridge_heartbeat("parietal_snn_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

parietal_snn_config_t parietal_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_config_", 0.0f);


    parietal_snn_config_t config = {
        .num_dimensions = PARIETAL_DIM_COUNT,
        .neurons_per_dim = PARIETAL_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = PARIETAL_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = PARIETAL_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = PARIETAL_SNN_DECODE_INTEGRATION,
        .attention_threshold = PARIETAL_SNN_ATTENTION_THRESH,
        .magnitude_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_attention_detection = true,
        .attention_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_spatial_processing = true,
        .spatial_gain = 1.5f,
        .enable_numerical_processing = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

parietal_snn_bridge_t* parietal_snn_create(const parietal_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_create", 0.0f);


    parietal_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(parietal_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = parietal_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > PARIETAL_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "parietal_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "parietal_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* attention, magnitude, multisensory, body_schema, visuospatial, rotation */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->spatial_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->spatial_buffer || !bridge->prev_state) {
        parietal_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize spatial output to neutral */
    bridge->last_spatial.attention_level = 0.5f;
    bridge->last_spatial.numerical_magnitude = 0.5f;
    bridge->last_spatial.multisensory_integration = 0.5f;
    bridge->last_spatial.body_schema_strength = 0.5f;
    bridge->last_spatial.visuospatial_activity = 0.5f;
    bridge->last_spatial.attention_detected = false;
    bridge->last_spatial.high_precision = false;
    bridge->last_spatial.precision_magnitude = 0.0f;
    bridge->last_spatial.coordinate_transform_level = 0.5f;
    bridge->last_spatial.mental_rotation_activity = 0.5f;

    bridge->state = PARIETAL_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->attention_signal = 0.0f;
    bridge->numerical_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "parietal_snn");
    return bridge;
}

void parietal_snn_destroy(parietal_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "parietal_snn");

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_destroy", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->spatial_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int parietal_snn_reset(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset spatial output */
    memset(&bridge->last_spatial, 0, sizeof(parietal_spatial_output_t));
    bridge->last_spatial.attention_level = 0.5f;
    bridge->last_spatial.numerical_magnitude = 0.5f;
    bridge->last_spatial.visuospatial_activity = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->spatial_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = PARIETAL_SNN_STATE_IDLE;
    bridge->attention_signal = 0.0f;
    bridge->numerical_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int parietal_snn_encode_state(
    parietal_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, dimensions, sizeof(*dimensions));
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_encode_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PARIETAL_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float value = nimcp_myelin_clamp(dimensions[d], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Update dimension state */
        bridge->dim_states[d].activation = value;
        bridge->dim_states[d].mean_rate_hz = rate;

        /* Population encode */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_dim > 256) {
                parietal_snn_bridge_heartbeat("parietal_snn_loop",
                                 (float)(n + 1) / (float)neurons_per_dim);
            }

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
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
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

int parietal_snn_encode_attention(
    parietal_snn_bridge_t* bridge,
    float attention,
    float focus
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_encode_attention: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_encode_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[PARIETAL_DIM_COUNT] = {0};
    dims[PARIETAL_DIM_SPATIAL_ATTENTION] = nimcp_myelin_clamp(attention, 0.0f, 1.0f);
    dims[PARIETAL_DIM_VISUOSPATIAL] = nimcp_myelin_clamp(focus, 0.0f, 1.0f);
    dims[PARIETAL_DIM_COORDINATE_TRANSFORM] = (attention + focus) / 2.0f;

    bridge->attention_signal = attention;

    nimcp_mutex_unlock(bridge->base.mutex);

    return parietal_snn_encode_state(bridge, dims, 3);
}

int parietal_snn_encode_magnitude(
    parietal_snn_bridge_t* bridge,
    float magnitude,
    uint32_t precision
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_encode_magnitude: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_encode_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[PARIETAL_DIM_COUNT] = {0};
    dims[PARIETAL_DIM_NUMERICAL_MAGNITUDE] = nimcp_myelin_clamp(magnitude, 0.0f, 1.0f);
    dims[PARIETAL_DIM_PRECISION] = nimcp_myelin_clamp((float)precision / 10.0f, 0.0f, 1.0f);

    bridge->numerical_signal = magnitude;

    nimcp_mutex_unlock(bridge->base.mutex);

    return parietal_snn_encode_state(bridge, dims, 2);
}

int parietal_snn_encode_multisensory(
    parietal_snn_bridge_t* bridge,
    float integration,
    uint32_t modality_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_encode_multisensory: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_encode_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[PARIETAL_DIM_COUNT] = {0};
    dims[PARIETAL_DIM_MULTISENSORY] = nimcp_myelin_clamp(integration, 0.0f, 1.0f);
    dims[PARIETAL_DIM_INTEGRATION] = nimcp_myelin_clamp((float)modality_count / 5.0f, 0.0f, 1.0f);
    dims[PARIETAL_DIM_BODY_SCHEMA] = nimcp_myelin_clamp(integration * 0.8f, 0.0f, 1.0f);

    if (integration > bridge->config.attention_threshold) {
        bridge->last_spatial.high_precision = true;
        bridge->last_spatial.precision_magnitude = integration;
        bridge->stats.high_precision_events++;

        if (bridge->precision_callback) {
            bridge->precision_callback(bridge, integration, modality_count,
                                       bridge->precision_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return parietal_snn_encode_state(bridge, dims, 3);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int parietal_snn_simulate(parietal_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_simulat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = PARIETAL_SNN_STATE_SIMULATING;

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
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
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
                parietal_snn_bridge_heartbeat("parietal_snn_loop",
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
    bridge->last_spatial.attention_level = nimcp_myelin_clamp(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_spatial.numerical_magnitude = nimcp_myelin_clamp(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_spatial.multisensory_integration = nimcp_myelin_clamp(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_spatial.body_schema_strength = nimcp_myelin_clamp(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_spatial.visuospatial_activity = nimcp_myelin_clamp(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_spatial.mental_rotation_activity = nimcp_myelin_clamp(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check attention threshold */
    if (bridge->last_spatial.attention_level > bridge->config.attention_threshold) {
        bridge->last_spatial.attention_detected = true;
        bridge->stats.attention_detections++;

        if (bridge->attention_callback) {
            bridge->attention_callback(bridge, bridge->last_spatial.attention_level,
                                       bridge->current_time_us, bridge->attention_callback_data);
        }
    } else {
        bridge->last_spatial.attention_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = PARIETAL_SNN_STATE_IDLE;

    /* Invoke spatial callback */
    if (bridge->spatial_callback) {
        bridge->spatial_callback(bridge, &bridge->last_spatial, bridge->spatial_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int parietal_snn_step(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_step", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "parietal_snn_step");
    BRIDGE_LGSS_GATE(bridge, "parietal_snn_step");

    return parietal_snn_simulate(bridge, bridge->config.dt_ms);
}

int parietal_snn_forward(
    parietal_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_forward", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, inputs, sizeof(*inputs));

    int spike_count = parietal_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_snn_forward: validation failed");
        return -1;
    }

    if (parietal_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int parietal_snn_get_spatial_output(
    parietal_snn_bridge_t* bridge,
    parietal_spatial_output_t* spatial
) {
    if (!bridge || !spatial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_get_spatial_output: required parameter is NULL (bridge, spatial)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_spa", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, spatial, sizeof(*spatial));

    nimcp_mutex_lock(bridge->base.mutex);
    *spatial = bridge->last_spatial;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_snn_get_activations(
    parietal_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, activations, sizeof(*activations));
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_act", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool parietal_snn_check_attention(
    parietal_snn_bridge_t* bridge,
    float* attention_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_check_a", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, attention_level, sizeof(*attention_level));

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_spatial.attention_level;
    if (attention_level) {
        *attention_level = level;
    }
    bool detected = level > bridge->config.attention_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool parietal_snn_check_precision(
    parietal_snn_bridge_t* bridge,
    float* precision_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_check_p", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, precision_level, sizeof(*precision_level));

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_spatial.precision_magnitude;
    if (precision_level) {
        *precision_level = level;
    }
    bool detected = level > bridge->config.attention_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool parietal_snn_check_state_change(
    parietal_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_check_s", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, change_magnitude, sizeof(*change_magnitude));

    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
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

int parietal_snn_get_dim_state(
    parietal_snn_bridge_t* bridge,
    uint32_t dim,
    parietal_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_dim", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_snn_get_state(
    parietal_snn_bridge_t* bridge,
    parietal_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_sta", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_spatial = bridge->last_spatial.visuospatial_activity;
    state->attention_signal = bridge->attention_signal;
    state->numerical_signal = bridge->numerical_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
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

int parietal_snn_get_stats(parietal_snn_bridge_t* bridge, parietal_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_sta", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_snn_reset_stats(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_reset_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(parietal_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float parietal_snn_get_spatial_activity(parietal_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_spa", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float spatial = bridge->last_spatial.visuospatial_activity;
    nimcp_mutex_unlock(bridge->base.mutex);

    return spatial;
}

float parietal_snn_get_total_activity(parietal_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_get_tot", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            parietal_snn_bridge_heartbeat("parietal_snn_loop",
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

int parietal_snn_register_attention_callback(
    parietal_snn_bridge_t* bridge,
    parietal_snn_attention_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_register_attention_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_registe", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_callback = callback;
    bridge->attention_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_snn_register_spatial_callback(
    parietal_snn_bridge_t* bridge,
    parietal_snn_spatial_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_register_spatial_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_registe", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->spatial_callback = callback;
    bridge->spatial_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_snn_register_precision_callback(
    parietal_snn_bridge_t* bridge,
    parietal_snn_precision_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_register_precision_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_registe", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->precision_callback = callback;
    bridge->precision_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int parietal_snn_bio_async_connect(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_bio_asy", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int parietal_snn_bio_async_disconnect(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_bio_asy", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool parietal_snn_is_bio_async_connected(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    parietal_snn_bridge_heartbeat("parietal_snn_parietal_snn_is_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void parietal_snn_bridge_set_instance_health_agent(
    parietal_snn_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int parietal_snn_bridge_training_begin(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    parietal_snn_bridge_heartbeat_instance(bridge->health_agent, "parietal_snn_bridge_training_begin", 0.0f);
    return 0;
}

int parietal_snn_bridge_training_end(parietal_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_snn_bridge_training_end: NULL argument");
        return -1;
    }
    parietal_snn_bridge_heartbeat_instance(bridge->health_agent, "parietal_snn_bridge_training_end", 1.0f);
    return 0;
}

int parietal_snn_bridge_training_step(parietal_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "parietal_snn_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "parietal_snn_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "parietal_snn_bridge_training_step");
    parietal_snn_bridge_heartbeat_instance(bridge->health_agent, "parietal_snn_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}
