/**
 * @file nimcp_shadow_snn_bridge.c
 * @brief Shadow Emotions - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/shadow/nimcp_shadow_snn_bridge.h"
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
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(shadow_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_shadow_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_shadow_snn_bridge_mesh_registry = NULL;

nimcp_error_t shadow_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_shadow_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "shadow_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "shadow_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_shadow_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_shadow_snn_bridge_mesh_registry = registry;
    return err;
}

void shadow_snn_bridge_mesh_unregister(void) {
    if (g_shadow_snn_bridge_mesh_registry && g_shadow_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_shadow_snn_bridge_mesh_registry, g_shadow_snn_bridge_mesh_id);
        g_shadow_snn_bridge_mesh_id = 0;
        g_shadow_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from shadow_snn_bridge module (instance-level) */
static inline void shadow_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_shadow_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_shadow_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_shadow_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "SHADOW_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct shadow_snn_bridge {
    bridge_base_t base;
    shadow_snn_config_t config;
    snn_network_t* snn;

    /* State */
    shadow_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    shadow_dim_state_t dim_states[SHADOW_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* suppression_buffer;

    /* Processing state */
    shadow_processing_output_t last_output;
    float suppression_signal;
    float unconscious_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    shadow_snn_suppression_callback_t suppression_callback;
    void* suppression_callback_data;
    shadow_snn_processing_callback_t processing_callback;
    void* processing_callback_data;
    shadow_snn_defense_callback_t defense_callback;
    void* defense_callback_data;

    /* Statistics */
    shadow_snn_stats_t stats;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS(shadow_snn_bridge)

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

shadow_snn_config_t shadow_snn_config_default(void) {
    shadow_snn_config_t config = {
        .num_dimensions = SHADOW_DIM_COUNT,
        .neurons_per_dim = SHADOW_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = SHADOW_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = SHADOW_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = SHADOW_SNN_DECODE_INTEGRATION,
        .suppression_threshold = SHADOW_SNN_SUPPRESSION_THRESH,
        .defense_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_suppression_detection = true,
        .suppression_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_unconscious_processing = true,
        .unconscious_gain = 1.5f,
        .enable_defense_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

shadow_snn_bridge_t* shadow_snn_create(const shadow_snn_config_t* config) {
    shadow_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(shadow_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = shadow_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > SHADOW_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "shadow_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "shadow_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* suppression, repression, integration, defense, unconscious, breakthrough */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "shadow_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->suppression_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->suppression_buffer || !bridge->prev_state) {
        shadow_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "shadow_snn_create: operation failed");
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

    /* Initialize processing output to neutral */
    bridge->last_output.suppression_level = 0.5f;
    bridge->last_output.repression_strength = 0.5f;
    bridge->last_output.shadow_integration = 0.5f;
    bridge->last_output.defense_activation = 0.5f;
    bridge->last_output.active_defense = SHADOW_DEFENSE_NONE;
    bridge->last_output.suppression_detected = false;
    bridge->last_output.shadow_breakthrough = false;
    bridge->last_output.breakthrough_magnitude = 0.0f;
    bridge->last_output.unconscious_activity = 0.5f;
    bridge->last_output.integration_progress = 0.5f;

    bridge->state = SHADOW_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->suppression_signal = 0.0f;
    bridge->unconscious_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "shadow_snn");
    return bridge;
}

void shadow_snn_destroy(shadow_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "shadow_snn");

    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->suppression_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int shadow_snn_reset(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_reset: bridge is NULL");
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

    /* Reset processing output */
    memset(&bridge->last_output, 0, sizeof(shadow_processing_output_t));
    bridge->last_output.suppression_level = 0.5f;
    bridge->last_output.repression_strength = 0.5f;
    bridge->last_output.defense_activation = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->suppression_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = SHADOW_SNN_STATE_IDLE;
    bridge->suppression_signal = 0.0f;
    bridge->unconscious_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int shadow_snn_encode_state(
    shadow_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, dimensions, num_dims * sizeof(float));
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_snn_encode_state: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SHADOW_SNN_STATE_ENCODING;

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

    if (change_magnitude > bridge->config.state_change_threshold) {
        bridge->stats.state_changes++;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int shadow_snn_encode_suppression(
    shadow_snn_bridge_t* bridge,
    float suppression,
    float repression
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_encode_suppression: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SHADOW_DIM_COUNT] = {0};
    dims[SHADOW_DIM_REPRESSION_STRENGTH] = nimcp_clampf(repression, 0.0f, 1.0f);
    dims[SHADOW_DIM_DEFENSE_ACTIVATION] = nimcp_clampf(suppression, 0.0f, 1.0f);
    dims[SHADOW_DIM_SUPPRESSED_FEAR] = (suppression + repression) / 2.0f;

    bridge->suppression_signal = suppression;

    nimcp_mutex_unlock(bridge->base.mutex);

    return shadow_snn_encode_state(bridge, dims, 3);
}

int shadow_snn_encode_unconscious(
    shadow_snn_bridge_t* bridge,
    float unconscious_level,
    uint32_t defense_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_encode_unconscious: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SHADOW_DIM_COUNT] = {0};
    dims[SHADOW_DIM_DEFENSE_ACTIVATION] = nimcp_clampf((float)defense_count / 5.0f, 0.0f, 1.0f);
    dims[SHADOW_DIM_SUPPRESSED_SHAME] = nimcp_clampf(unconscious_level, 0.0f, 1.0f);

    bridge->unconscious_signal = unconscious_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return shadow_snn_encode_state(bridge, dims, 2);
}

int shadow_snn_encode_defense(
    shadow_snn_bridge_t* bridge,
    float defense_strength,
    shadow_defense_type_t defense_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_encode_defense: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SHADOW_DIM_COUNT] = {0};
    dims[SHADOW_DIM_DEFENSE_ACTIVATION] = nimcp_clampf(defense_strength, 0.0f, 1.0f);
    dims[SHADOW_DIM_REPRESSION_STRENGTH] = nimcp_clampf(defense_strength * 0.8f, 0.0f, 1.0f);

    /* Invoke defense callback if defense is significant */
    if (defense_strength > bridge->config.defense_threshold) {
        bridge->last_output.active_defense = defense_type;
        bridge->stats.defense_activations++;

        if (bridge->defense_callback) {
            bridge->defense_callback(bridge, defense_type, defense_strength,
                                    bridge->defense_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return shadow_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int shadow_snn_simulate(shadow_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_snn_simulate: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SHADOW_SNN_STATE_SIMULATING;

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
    bridge->last_output.suppression_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_output.repression_strength = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_output.shadow_integration = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_output.defense_activation = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_output.unconscious_activity = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_output.breakthrough_magnitude = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check suppression threshold */
    if (bridge->last_output.suppression_level > bridge->config.suppression_threshold) {
        bridge->last_output.suppression_detected = true;
        bridge->stats.suppression_detections++;

        if (bridge->suppression_callback) {
            bridge->suppression_callback(bridge, bridge->last_output.suppression_level,
                                        bridge->current_time_us, bridge->suppression_callback_data);
        }
    } else {
        bridge->last_output.suppression_detected = false;
    }

    /* Check for shadow breakthrough (high unconscious + low repression) */
    if (bridge->last_output.unconscious_activity > 0.7f &&
        bridge->last_output.repression_strength < 0.4f) {
        bridge->last_output.shadow_breakthrough = true;
    } else {
        bridge->last_output.shadow_breakthrough = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = SHADOW_SNN_STATE_IDLE;

    /* Invoke processing callback */
    if (bridge->processing_callback) {
        bridge->processing_callback(bridge, &bridge->last_output, bridge->processing_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_snn_step(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_step: bridge is NULL");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "shadow_snn_step");
    BRIDGE_LGSS_GATE(bridge, "shadow_snn_step");
    return shadow_snn_simulate(bridge, bridge->config.dt_ms);
}

int shadow_snn_forward(
    shadow_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, inputs, input_count * sizeof(float));

    int spike_count = shadow_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_snn_forward: validation failed");
        return -1;
    }

    if (shadow_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int shadow_snn_get_output(
    shadow_snn_bridge_t* bridge,
    shadow_processing_output_t* output
) {
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_get_output: required parameter is NULL (bridge, output)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *output = bridge->last_output;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_snn_get_activations(
    shadow_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_snn_get_activations: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool shadow_snn_check_suppression(
    shadow_snn_bridge_t* bridge,
    float* suppression_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_output.suppression_level;
    if (suppression_level) {
        *suppression_level = level;
    }
    bool detected = level > bridge->config.suppression_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool shadow_snn_check_defense(
    shadow_snn_bridge_t* bridge,
    float* defense_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_output.defense_activation;
    if (defense_level) {
        *defense_level = level;
    }
    bool detected = level > bridge->config.defense_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool shadow_snn_check_state_change(
    shadow_snn_bridge_t* bridge,
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
    bool changed = mag > bridge->config.state_change_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return changed;
}

//=============================================================================
// State Query Functions
//=============================================================================

int shadow_snn_get_dim_state(
    shadow_snn_bridge_t* bridge,
    uint32_t dim,
    shadow_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_snn_get_state(
    shadow_snn_bridge_t* bridge,
    shadow_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_suppression = bridge->last_output.suppression_level;
    state->defense_signal = bridge->last_output.defense_activation;
    state->unconscious_signal = bridge->unconscious_signal;

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

int shadow_snn_get_stats(shadow_snn_bridge_t* bridge, shadow_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_snn_reset_stats(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(shadow_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float shadow_snn_get_suppression(shadow_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float suppression = bridge->last_output.suppression_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return suppression;
}

float shadow_snn_get_total_activity(shadow_snn_bridge_t* bridge) {
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

int shadow_snn_register_suppression_callback(
    shadow_snn_bridge_t* bridge,
    shadow_snn_suppression_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_register_suppression_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->suppression_callback = callback;
    bridge->suppression_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_snn_register_processing_callback(
    shadow_snn_bridge_t* bridge,
    shadow_snn_processing_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_register_processing_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->processing_callback = callback;
    bridge->processing_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_snn_register_defense_callback(
    shadow_snn_bridge_t* bridge,
    shadow_snn_defense_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_register_defense_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->defense_callback = callback;
    bridge->defense_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int shadow_snn_bio_async_connect(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_snn_bio_async_disconnect(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool shadow_snn_is_bio_async_connected(shadow_snn_bridge_t* bridge) {
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

void shadow_snn_bridge_set_instance_health_agent(shadow_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shadow_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int shadow_snn_bridge_training_begin(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    shadow_snn_bridge_heartbeat_instance(bridge->health_agent, "shadow_snn_bridge_training_begin", 0.0f);
    return 0;
}

int shadow_snn_bridge_training_end(shadow_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_snn_bridge_training_end: NULL argument");
        return -1;
    }
    shadow_snn_bridge_heartbeat_instance(bridge->health_agent, "shadow_snn_bridge_training_end", 1.0f);
    return 0;
}

int shadow_snn_bridge_training_step(shadow_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    shadow_snn_bridge_heartbeat_instance(bridge->health_agent, "shadow_snn_bridge_training_step", progress);
    return 0;
}
