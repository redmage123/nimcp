/**
 * @file nimcp_jepa_snn_bridge.c
 * @brief JEPA - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/jepa/nimcp_jepa_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(jepa_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_jepa_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_jepa_snn_bridge_mesh_registry = NULL;

nimcp_error_t jepa_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_jepa_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "jepa_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "jepa_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_jepa_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_jepa_snn_bridge_mesh_registry = registry;
    return err;
}

void jepa_snn_bridge_mesh_unregister(void) {
    if (g_jepa_snn_bridge_mesh_registry && g_jepa_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_jepa_snn_bridge_mesh_registry, g_jepa_snn_bridge_mesh_id);
        g_jepa_snn_bridge_mesh_id = 0;
        g_jepa_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from jepa_snn_bridge module (instance + global) */
static inline void jepa_snn_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_jepa_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_jepa_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_jepa_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "JEPA_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct jepa_snn_bridge {
    bridge_base_t base;  /* MUST be first member */
    jepa_snn_config_t config;
    snn_network_t* snn;

    /* State */
    jepa_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    jepa_dim_state_t dim_states[JEPA_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* prediction_buffer;

    /* Prediction state */
    jepa_prediction_output_t last_prediction;
    float error_signal;
    float context_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    jepa_snn_error_callback_t error_callback;
    void* error_callback_data;
    jepa_snn_prediction_callback_t prediction_callback;
    void* prediction_callback_data;
    jepa_snn_confidence_callback_t confidence_callback;
    void* confidence_callback_data;

    /* Statistics */
    jepa_snn_stats_t stats;

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
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

jepa_snn_config_t jepa_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_config_defa", 0.0f);


    jepa_snn_config_t config = {
        .num_dimensions = JEPA_DIM_COUNT,
        .neurons_per_dim = JEPA_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = JEPA_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = JEPA_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = JEPA_SNN_DECODE_INTEGRATION,
        .prediction_error_threshold = JEPA_SNN_PRED_ERROR_THRESH,
        .confidence_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_error_detection = true,
        .error_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_prediction = true,
        .prediction_gain = 1.5f,
        .enable_context_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

jepa_snn_bridge_t* jepa_snn_create(const jepa_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_create", 0.0f);


    jepa_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(jepa_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = jepa_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > JEPA_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_snn_create: operation failed");
        return NULL;
    }

    /* Initialize base (includes mutex creation) */
    if (bridge_base_init(&bridge->base, 0, "jepa_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "jepa_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* context, target, error, confidence, multimodal, self_supervised */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->prediction_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->prediction_buffer || !bridge->prev_state) {
        jepa_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize prediction to neutral */
    bridge->last_prediction.context_level = 0.5f;
    bridge->last_prediction.target_level = 0.5f;
    bridge->last_prediction.prediction_error = 0.5f;
    bridge->last_prediction.prediction_confidence = 0.5f;
    bridge->last_prediction.multimodal_integration = 0.5f;
    bridge->last_prediction.error_detected = false;
    bridge->last_prediction.high_confidence = false;
    bridge->last_prediction.confidence_magnitude = 0.0f;
    bridge->last_prediction.self_supervised_signal = 0.5f;
    bridge->last_prediction.temporal_context = 0.5f;

    bridge->state = JEPA_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->error_signal = 0.0f;
    bridge->context_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "jepa_snn");
    return bridge;
}

void jepa_snn_destroy(jepa_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "jepa_snn");

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_destroy", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->prediction_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int jepa_snn_reset(jepa_snn_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset prediction */
    memset(&bridge->last_prediction, 0, sizeof(jepa_prediction_output_t));
    bridge->last_prediction.context_level = 0.5f;
    bridge->last_prediction.target_level = 0.5f;
    bridge->last_prediction.prediction_confidence = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->prediction_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = JEPA_SNN_STATE_IDLE;
    bridge->error_signal = 0.0f;
    bridge->context_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int jepa_snn_encode_state(
    jepa_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_encode_stat", 0.0f);


    NIMCP_CHECK_THROW(bridge && dimensions, -1, "bridge or dimensions is NULL");
    NIMCP_CHECK_THROW(num_dims > 0 && num_dims <= bridge->config.num_dimensions, -1, "invalid num_dims");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = JEPA_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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
                jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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

int jepa_snn_encode_latent(
    jepa_snn_bridge_t* bridge,
    float context,
    float target
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_encode_late", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[JEPA_DIM_COUNT] = {0};
    dims[JEPA_DIM_LATENT_CONTEXT] = nimcp_clampf(context, 0.0f, 1.0f);
    dims[JEPA_DIM_LATENT_TARGET] = nimcp_clampf(target, 0.0f, 1.0f);
    dims[JEPA_DIM_CONTEXT_EMBEDDING] = (context + target) / 2.0f;

    bridge->context_signal = context;

    nimcp_mutex_unlock(bridge->base.mutex);

    return jepa_snn_encode_state(bridge, dims, 3);
}

int jepa_snn_encode_prediction_error(
    jepa_snn_bridge_t* bridge,
    float error_magnitude,
    uint32_t error_dimension
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_encode_pred", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[JEPA_DIM_COUNT] = {0};
    dims[JEPA_DIM_PREDICTION_ERROR] = nimcp_clampf(error_magnitude, 0.0f, 1.0f);
    dims[JEPA_DIM_SELF_SUPERVISED] = nimcp_clampf((float)error_dimension / 10.0f, 0.0f, 1.0f);

    bridge->error_signal = error_magnitude;

    nimcp_mutex_unlock(bridge->base.mutex);

    return jepa_snn_encode_state(bridge, dims, 2);
}

int jepa_snn_encode_context(
    jepa_snn_bridge_t* bridge,
    float context_strength,
    uint32_t context_type
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_encode_cont", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[JEPA_DIM_COUNT] = {0};
    dims[JEPA_DIM_CONTEXT_EMBEDDING] = nimcp_clampf(context_strength, 0.0f, 1.0f);
    dims[JEPA_DIM_TEMPORAL_CONTEXT] = nimcp_clampf(context_strength * 0.8f, 0.0f, 1.0f);

    bridge->context_signal = context_strength;

    /* Snapshot callback under mutex */
    jepa_snn_confidence_callback_t conf_cb = NULL;
    void* conf_cb_data = NULL;
    if (context_strength > bridge->config.confidence_threshold) {
        bridge->last_prediction.high_confidence = true;
        bridge->last_prediction.confidence_magnitude = context_strength;
        bridge->stats.high_confidence_events++;

        conf_cb = bridge->confidence_callback;
        conf_cb_data = bridge->confidence_callback_data;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside mutex to prevent deadlock */
    if (conf_cb) {
        conf_cb(bridge, context_strength, context_type, conf_cb_data);
    }

    return jepa_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int jepa_snn_simulate(jepa_snn_bridge_t* bridge, float duration_ms) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_simulate", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    NIMCP_CHECK_THROW(duration_ms > 0.0f, -1, "duration_ms must be positive");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = JEPA_SNN_STATE_SIMULATING;

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
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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
                jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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
    bridge->last_prediction.context_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_prediction.target_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_prediction.prediction_error = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_prediction.prediction_confidence = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_prediction.multimodal_integration = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_prediction.self_supervised_signal = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check prediction error threshold */
    if (bridge->last_prediction.prediction_error > bridge->config.prediction_error_threshold) {
        bridge->last_prediction.error_detected = true;
        bridge->stats.error_detections++;
    } else {
        bridge->last_prediction.error_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = JEPA_SNN_STATE_IDLE;

    /* Snapshot callback state under mutex */
    jepa_snn_error_callback_t err_cb = bridge->error_callback;
    void* err_cb_data = bridge->error_callback_data;
    float pred_error = bridge->last_prediction.prediction_error;
    uint64_t cur_time = bridge->current_time_us;
    bool do_err_cb = (err_cb != NULL && bridge->last_prediction.error_detected);

    jepa_snn_prediction_callback_t pred_cb = bridge->prediction_callback;
    void* pred_cb_data = bridge->prediction_callback_data;
    jepa_prediction_output_t pred_snapshot = bridge->last_prediction;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside mutex to prevent deadlock */
    if (do_err_cb) {
        err_cb(bridge, pred_error, cur_time, err_cb_data);
    }
    if (pred_cb) {
        pred_cb(bridge, &pred_snapshot, pred_cb_data);
    }

    return 0;
}

int jepa_snn_step(jepa_snn_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_step", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    return jepa_snn_simulate(bridge, bridge->config.dt_ms);
}

int jepa_snn_forward(
    jepa_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_forward", 0.0f);


    NIMCP_CHECK_THROW(bridge && inputs, -1, "bridge or inputs is NULL");

    int spike_count = jepa_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) return -1;

    if (jepa_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int jepa_snn_get_prediction(
    jepa_snn_bridge_t* bridge,
    jepa_prediction_output_t* output
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_predict", 0.0f);


    NIMCP_CHECK_THROW(bridge && output, -1, "bridge or output is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *output = bridge->last_prediction;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_snn_get_activations(
    jepa_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_activat", 0.0f);


    NIMCP_CHECK_THROW(bridge && activations, -1, "bridge or activations is NULL");
    NIMCP_CHECK_THROW(num_dims > 0 && num_dims <= bridge->config.num_dimensions, -1, "invalid num_dims");

    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool jepa_snn_check_prediction_error(
    jepa_snn_bridge_t* bridge,
    float* error_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_check_predi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_prediction.prediction_error;
    if (error_level) {
        *error_level = level;
    }
    bool detected = level > bridge->config.prediction_error_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool jepa_snn_check_confidence(
    jepa_snn_bridge_t* bridge,
    float* confidence_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_check_confi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_prediction.confidence_magnitude;
    if (confidence_level) {
        *confidence_level = level;
    }
    bool detected = level > bridge->config.confidence_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool jepa_snn_check_state_change(
    jepa_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_check_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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

int jepa_snn_get_dim_state(
    jepa_snn_bridge_t* bridge,
    uint32_t dim,
    jepa_dim_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_dim_sta", 0.0f);


    NIMCP_CHECK_THROW(bridge && state, -1, "bridge or state is NULL");
    NIMCP_CHECK_THROW(dim < bridge->config.num_dimensions, -1, "dim out of range");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_snn_get_state(
    jepa_snn_bridge_t* bridge,
    jepa_snn_bridge_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_state", 0.0f);


    NIMCP_CHECK_THROW(bridge && state, -1, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_prediction = bridge->last_prediction.prediction_confidence;
    state->error_signal = bridge->error_signal;
    state->context_signal = bridge->context_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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

int jepa_snn_get_stats(jepa_snn_bridge_t* bridge, jepa_snn_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge && stats, -1, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_snn_reset_stats(jepa_snn_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(jepa_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float jepa_snn_get_prediction_confidence(jepa_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_snn_get_prediction_confidence: bridge is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_predict", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float confidence = bridge->last_prediction.prediction_confidence;
    nimcp_mutex_unlock(bridge->base.mutex);

    return confidence;
}

float jepa_snn_get_total_activity(jepa_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_snn_get_total_activity: bridge is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_get_total_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            jepa_snn_bridge_heartbeat("jepa_snn_bri_loop",
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

int jepa_snn_register_error_callback(
    jepa_snn_bridge_t* bridge,
    jepa_snn_error_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_register_er", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->error_callback = callback;
    bridge->error_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_snn_register_prediction_callback(
    jepa_snn_bridge_t* bridge,
    jepa_snn_prediction_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_register_pr", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->prediction_callback = callback;
    bridge->prediction_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_snn_register_confidence_callback(
    jepa_snn_bridge_t* bridge,
    jepa_snn_confidence_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_register_co", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->confidence_callback = callback;
    bridge->confidence_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int jepa_snn_bio_async_connect(jepa_snn_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_bio_async_c", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->config.enable_bio_async, -1, "bio_async not enabled");

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_snn_bio_async_disconnect(jepa_snn_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_bio_async_d", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool jepa_snn_is_bio_async_connected(jepa_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_snn_bridge_heartbeat("jepa_snn_bri_jepa_snn_is_bio_asyn", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_snn_bridge_set_instance_health_agent(jepa_snn_bridge_t* bridge,
                                                nimcp_health_agent_t* agent) {
    if (!bridge) return;
    bridge->health_agent = agent;
}

int jepa_snn_bridge_training_begin(jepa_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    jepa_snn_bridge_heartbeat_instance(bridge, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int jepa_snn_bridge_training_end(jepa_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_snn_bridge_training_end: NULL argument");
        return -1;
    }
    jepa_snn_bridge_heartbeat_instance(bridge, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

int jepa_snn_bridge_training_step(jepa_snn_bridge_t* bridge, uint32_t step) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_snn_bridge_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_snn_bridge_heartbeat_instance(bridge, "training_step", progress);
    (void)bridge;
    return 0;
}
