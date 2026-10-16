/**
 * @file nimcp_introspection_snn_bridge.c
 * @brief Introspection - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/introspection/nimcp_introspection_snn_bridge.h"
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
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(introspection_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_introspection_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_introspection_snn_bridge_mesh_registry = NULL;

nimcp_error_t introspection_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_introspection_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "introspection_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "introspection_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_introspection_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_introspection_snn_bridge_mesh_registry = registry;
    return err;
}

void introspection_snn_bridge_mesh_unregister(void) {
    if (g_introspection_snn_bridge_mesh_registry && g_introspection_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_introspection_snn_bridge_mesh_registry, g_introspection_snn_bridge_mesh_id);
        g_introspection_snn_bridge_mesh_id = 0;
        g_introspection_snn_bridge_mesh_registry = NULL;
    }
}


static inline void introspection_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_introspection_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_introspection_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_introspection_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "INTROSPECTION_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct introspection_snn_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    introspection_snn_config_t config;
    snn_network_t* snn;

    /* State */
    introspection_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    introspection_dim_state_t dim_states[INTROSPECTION_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* insight_buffer;

    /* Insight state */
    introspection_insight_t last_insight;
    float uncertainty_signal;
    float error_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    introspection_snn_uncertainty_callback_t uncertainty_callback;
    void* uncertainty_callback_data;
    introspection_snn_insight_callback_t insight_callback;
    void* insight_callback_data;
    introspection_snn_error_callback_t error_callback;
    void* error_callback_data;

    /* Statistics */
    introspection_snn_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
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
            introspection_snn_bridge_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                introspection_snn_bridge_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

introspection_snn_config_t introspection_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_co", 0.0f);


    introspection_snn_config_t config = {
        .num_dimensions = INTROSPECTION_DIM_COUNT,
        .neurons_per_dim = INTROSPECTION_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = INTROSPECTION_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = INTROSPECTION_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = INTROSPECTION_SNN_DECODE_INTEGRATION,
        .uncertainty_threshold = INTROSPECTION_SNN_UNCERTAINTY_THRESH,
        .confidence_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_error_detection = true,
        .error_threshold = 0.4f,

        .enable_metacognition = true,
        .metacognition_gain = 1.5f,
        .enable_self_reference = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

introspection_snn_bridge_t* introspection_snn_create(const introspection_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_cr", 0.0f);


    introspection_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(introspection_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_snn_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = introspection_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > INTROSPECTION_SNN_MAX_DIMENSIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_create: invalid num_dimensions");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "introspection_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_snn_create: bridge_base_init failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* certainty, uncertainty, confidence, alertness, error, metacog */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_snn_create: failed to create SNN");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->insight_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->insight_buffer || !bridge->prev_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "introspection_snn_create: failed to allocate buffers");
        introspection_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            introspection_snn_bridge_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize insight to neutral */
    bridge->last_insight.certainty_level = 0.5f;
    bridge->last_insight.uncertainty_level = 0.5f;
    bridge->last_insight.confidence = 0.5f;
    bridge->last_insight.alertness = 0.5f;
    bridge->last_insight.attention_focus = 0.5f;
    bridge->last_insight.state_change_detected = false;
    bridge->last_insight.error_detected = false;
    bridge->last_insight.error_magnitude = 0.0f;
    bridge->last_insight.metacognition_level = 0.5f;
    bridge->last_insight.integration_score = 0.5f;

    bridge->state = INTROSPECTION_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->uncertainty_signal = 0.0f;
    bridge->error_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "introspection_snn");
    return bridge;
}

void introspection_snn_destroy(introspection_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "introspection_snn");

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_de", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->insight_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int introspection_snn_reset(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            introspection_snn_bridge_heartbeat("introspectio_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset insight */
    memset(&bridge->last_insight, 0, sizeof(introspection_insight_t));
    bridge->last_insight.certainty_level = 0.5f;
    bridge->last_insight.uncertainty_level = 0.5f;
    bridge->last_insight.confidence = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->insight_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = INTROSPECTION_SNN_STATE_IDLE;
    bridge->uncertainty_signal = 0.0f;
    bridge->error_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int introspection_snn_encode_state(
    introspection_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = INTROSPECTION_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            introspection_snn_bridge_heartbeat("introspectio_loop",
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
                introspection_snn_bridge_heartbeat("introspectio_loop",
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
            introspection_snn_bridge_heartbeat("introspectio_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - bridge->prev_state[d];
        change_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    change_magnitude = sqrtf(change_magnitude / num_dims);

    if (change_magnitude > bridge->config.state_change_threshold) {
        bridge->last_insight.state_change_detected = true;
        bridge->stats.state_changes++;
    } else {
        bridge->last_insight.state_change_detected = false;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int introspection_snn_encode_uncertainty(
    introspection_snn_bridge_t* bridge,
    float epistemic,
    float aleatoric
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_encode_uncertainty: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_CERTAINTY] = 1.0f - (epistemic + aleatoric) / 2.0f;
    dims[INTROSPECTION_DIM_UNCERTAINTY] = (epistemic + aleatoric) / 2.0f;
    dims[INTROSPECTION_DIM_CONFIDENCE] = 1.0f - epistemic;

    bridge->uncertainty_signal = (epistemic + aleatoric) / 2.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return introspection_snn_encode_state(bridge, dims, 3);
}

int introspection_snn_encode_pattern(
    introspection_snn_bridge_t* bridge,
    float pattern_strength,
    uint32_t pattern_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_encode_pattern: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_PATTERN_MATCH] = nimcp_clampf(pattern_strength, 0.0f, 1.0f);
    dims[INTROSPECTION_DIM_ATTENTION_FOCUS] = nimcp_clampf((float)pattern_count / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return introspection_snn_encode_state(bridge, dims, 2);
}

int introspection_snn_encode_error(
    introspection_snn_bridge_t* bridge,
    float error_magnitude,
    uint32_t error_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_encode_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[INTROSPECTION_DIM_COUNT] = {0};
    dims[INTROSPECTION_DIM_ERROR_SIGNAL] = nimcp_clampf(error_magnitude, 0.0f, 1.0f);
    dims[INTROSPECTION_DIM_CONFLICT] = nimcp_clampf(error_magnitude * 0.5f, 0.0f, 1.0f);

    bridge->error_signal = error_magnitude;

    if (error_magnitude > bridge->config.error_threshold) {
        bridge->last_insight.error_detected = true;
        bridge->last_insight.error_magnitude = error_magnitude;
        bridge->stats.error_detections++;

        if (bridge->error_callback) {
            bridge->error_callback(bridge, error_magnitude, error_type,
                                   bridge->error_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return introspection_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int introspection_snn_simulate(introspection_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_si", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = INTROSPECTION_SNN_STATE_SIMULATING;

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
            introspection_snn_bridge_heartbeat("introspectio_loop",
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
                introspection_snn_bridge_heartbeat("introspectio_loop",
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
    bridge->last_insight.certainty_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_insight.uncertainty_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_insight.confidence = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_insight.alertness = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_insight.metacognition_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check uncertainty threshold */
    if (bridge->last_insight.uncertainty_level > bridge->config.uncertainty_threshold) {
        bridge->stats.uncertainty_detections++;

        if (bridge->uncertainty_callback) {
            bridge->uncertainty_callback(bridge, bridge->last_insight.uncertainty_level,
                                        bridge->current_time_us, bridge->uncertainty_callback_data);
        }
    }

    bridge->stats.total_evaluations++;
    bridge->state = INTROSPECTION_SNN_STATE_IDLE;

    /* Invoke insight callback */
    if (bridge->insight_callback) {
        bridge->insight_callback(bridge, &bridge->last_insight, bridge->insight_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_snn_step(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_st", 0.0f);


    return introspection_snn_simulate(bridge, bridge->config.dt_ms);
}

int introspection_snn_forward(
    introspection_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_fo", 0.0f);


    int spike_count = introspection_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_forward: validation failed");
        return -1;
    }

    if (introspection_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int introspection_snn_get_insight(
    introspection_snn_bridge_t* bridge,
    introspection_insight_t* insight
) {
    if (!bridge || !insight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_get_insight: required parameter is NULL (bridge, insight)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *insight = bridge->last_insight;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int introspection_snn_get_activations(
    introspection_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            introspection_snn_bridge_heartbeat("introspectio_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool introspection_snn_check_uncertainty(
    introspection_snn_bridge_t* bridge,
    float* uncertainty_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_insight.uncertainty_level;
    if (uncertainty_level) {
        *uncertainty_level = level;
    }
    bool detected = level > bridge->config.uncertainty_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool introspection_snn_check_error(
    introspection_snn_bridge_t* bridge,
    float* error_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->error_signal;
    if (error_level) {
        *error_level = level;
    }
    bool detected = level > bridge->config.error_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool introspection_snn_check_state_change(
    introspection_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool changed = bridge->last_insight.state_change_detected;
    if (change_magnitude && changed) {
        /* Calculate magnitude from prev_state differences */
        float mag = 0.0f;
        for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
                introspection_snn_bridge_heartbeat("introspectio_loop",
                                 (float)(d + 1) / (float)bridge->config.num_dimensions);
            }

            float diff = bridge->dim_states[d].activation - bridge->prev_state[d];
            mag += diff * diff;
        }
        *change_magnitude = sqrtf(mag / bridge->config.num_dimensions);
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return changed;
}

//=============================================================================
// State Query Functions
//=============================================================================

int introspection_snn_get_dim_state(
    introspection_snn_bridge_t* bridge,
    uint32_t dim,
    introspection_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int introspection_snn_get_state(
    introspection_snn_bridge_t* bridge,
    introspection_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_confidence = bridge->last_insight.confidence;
    state->uncertainty_signal = bridge->uncertainty_signal;
    state->error_signal = bridge->error_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            introspection_snn_bridge_heartbeat("introspectio_loop",
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

int introspection_snn_get_stats(introspection_snn_bridge_t* bridge, introspection_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int introspection_snn_reset_stats(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(introspection_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float introspection_snn_get_confidence(introspection_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float confidence = bridge->last_insight.confidence;
    nimcp_mutex_unlock(bridge->base.mutex);

    return confidence;
}

float introspection_snn_get_total_activity(introspection_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            introspection_snn_bridge_heartbeat("introspectio_loop",
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

int introspection_snn_register_uncertainty_callback(
    introspection_snn_bridge_t* bridge,
    introspection_snn_uncertainty_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_register_uncertainty_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->uncertainty_callback = callback;
    bridge->uncertainty_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int introspection_snn_register_insight_callback(
    introspection_snn_bridge_t* bridge,
    introspection_snn_insight_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_register_insight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->insight_callback = callback;
    bridge->insight_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int introspection_snn_register_error_callback(
    introspection_snn_bridge_t* bridge,
    introspection_snn_error_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_register_error_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->error_callback = callback;
    bridge->error_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int introspection_snn_bio_async_connect(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int introspection_snn_bio_async_disconnect(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool introspection_snn_is_bio_async_connected(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_snn_bridge_heartbeat("introspectio_introspection_snn_is", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

void introspection_snn_bridge_set_instance_health_agent(
    introspection_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int introspection_snn_bridge_training_begin(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    introspection_snn_bridge_heartbeat_instance(bridge->health_agent,
        "intro_snn_training_begin", 0.0f);
    bridge->stats.total_evaluations = 0;
    bridge->stats.mean_confidence = 0.0f;
    bridge->uncertainty_signal = 0.5f;
    NIMCP_LOGGING_INFO("[INTRO_SNN] Training begin: counters reset, baseline state initialized");
    return 0;
}

int introspection_snn_bridge_training_step(introspection_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    introspection_snn_bridge_heartbeat_instance(bridge->health_agent,
        "intro_snn_training_step", progress);
    float lr = bridge->config.encoding_gain;
    float adaptation = lr * (1.0f - progress) * 0.1f;
    bridge->config.encoding_gain = lr + adaptation;
    if (bridge->config.encoding_gain > 1.0f) bridge->config.encoding_gain = 1.0f;
    if (bridge->config.encoding_gain < 0.001f) bridge->config.encoding_gain = 0.001f;
    bridge->uncertainty_signal = bridge->uncertainty_signal * 0.99f + progress * 0.01f;
    bridge->stats.total_evaluations++;
    return 0;
}

int introspection_snn_bridge_training_end(introspection_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_snn_bridge_training_end: NULL argument");
        return -1;
    }
    introspection_snn_bridge_heartbeat_instance(bridge->health_agent,
        "intro_snn_training_end", 1.0f);
    if (bridge->uncertainty_signal < 0.0f) bridge->uncertainty_signal = 0.0f;
    if (bridge->uncertainty_signal > 1.0f) bridge->uncertainty_signal = 1.0f;
    NIMCP_LOGGING_INFO("[INTRO_SNN] Training end: uncertainty=%.3f, evals=%u",
        bridge->uncertainty_signal, bridge->stats.total_evaluations);
    return 0;
}
