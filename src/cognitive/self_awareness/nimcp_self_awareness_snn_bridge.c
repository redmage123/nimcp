/**
 * @file nimcp_self_awareness_snn_bridge.c
 * @brief Self-Awareness - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/self_awareness/nimcp_self_awareness_snn_bridge.h"
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
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_awareness_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_awareness_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_awareness_snn_bridge_mesh_registry = NULL;

nimcp_error_t self_awareness_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_self_awareness_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_awareness_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_awareness_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_awareness_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_awareness_snn_bridge_mesh_registry = registry;
    return err;
}

void self_awareness_snn_bridge_mesh_unregister(void) {
    if (g_self_awareness_snn_bridge_mesh_registry && g_self_awareness_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_awareness_snn_bridge_mesh_registry, g_self_awareness_snn_bridge_mesh_id);
        g_self_awareness_snn_bridge_mesh_id = 0;
        g_self_awareness_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_awareness_snn_bridge module (instance-level) */
static inline void self_awareness_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_awareness_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_awareness_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_awareness_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "SELF_AWARENESS_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct self_awareness_snn_bridge {
    bridge_base_t base;
    self_awareness_snn_config_t config;
    snn_network_t* snn;

    /* State */
    self_awareness_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    self_awareness_dim_state_t dim_states[SELF_AWARENESS_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* awareness_buffer;

    /* Awareness state */
    self_awareness_state_t last_awareness_state;
    float recognition_signal;
    float agency_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    self_awareness_snn_recognition_callback_t recognition_callback;
    void* recognition_callback_data;
    self_awareness_snn_state_callback_t state_callback;
    void* state_callback_data;
    self_awareness_snn_agency_callback_t agency_callback;
    void* agency_callback_data;

    /* Statistics */
    self_awareness_snn_stats_t stats;

    /* Phase 8: Instance health agent (B24 upgrade) */
    nimcp_health_agent_t* health_agent;
};

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

self_awareness_snn_config_t self_awareness_snn_config_default(void) {
    self_awareness_snn_config_t config = {
        .num_dimensions = SELF_DIM_COUNT,
        .neurons_per_dim = SELF_AWARENESS_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = SELF_AWARENESS_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = SELF_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = SELF_SNN_DECODE_INTEGRATION,
        .recognition_threshold = SELF_AWARENESS_SNN_RECOGNITION_THRESH,
        .agency_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_self_recognition = true,
        .recognition_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_body_ownership = true,
        .body_ownership_gain = 1.5f,
        .enable_interoception = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

self_awareness_snn_bridge_t* self_awareness_snn_create(const self_awareness_snn_config_t* config) {
    self_awareness_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(self_awareness_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = self_awareness_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > SELF_AWARENESS_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "self_awareness_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "self_awareness_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* recognition, body_ownership, agency, metacog, reflection, continuity */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_awareness_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->awareness_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->awareness_buffer || !bridge->prev_state) {
        self_awareness_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_awareness_snn_create: operation failed");
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

    /* Initialize awareness state to neutral */
    bridge->last_awareness_state.self_recognition = 0.5f;
    bridge->last_awareness_state.body_ownership = 0.5f;
    bridge->last_awareness_state.agency_sense = 0.5f;
    bridge->last_awareness_state.metacognitive_level = 0.5f;
    bridge->last_awareness_state.self_reflection = 0.5f;
    bridge->last_awareness_state.self_recognized = false;
    bridge->last_awareness_state.agency_detected = false;
    bridge->last_awareness_state.recognition_magnitude = 0.0f;
    bridge->last_awareness_state.temporal_continuity = 0.5f;
    bridge->last_awareness_state.self_boundary_clarity = 0.5f;

    bridge->state = SELF_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->recognition_signal = 0.0f;
    bridge->agency_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "self_awareness_snn");
    return bridge;
}

void self_awareness_snn_destroy(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "self_awareness_snn");

    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->awareness_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int self_awareness_snn_reset(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_reset: bridge is NULL");
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

    /* Reset awareness state */
    memset(&bridge->last_awareness_state, 0, sizeof(self_awareness_state_t));
    bridge->last_awareness_state.self_recognition = 0.5f;
    bridge->last_awareness_state.body_ownership = 0.5f;
    bridge->last_awareness_state.agency_sense = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->awareness_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = SELF_SNN_STATE_IDLE;
    bridge->recognition_signal = 0.0f;
    bridge->agency_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int self_awareness_snn_encode_state(
    self_awareness_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_snn_encode_state: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SELF_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        float value = nimcp_myelin_clamp(dimensions[d], 0.0f, 1.0f);
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

int self_awareness_snn_encode_self_recognition(
    self_awareness_snn_bridge_t* bridge,
    float recognition,
    float body_ownership
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_encode_self_recognition: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_SELF_RECOGNITION] = nimcp_myelin_clamp(recognition, 0.0f, 1.0f);
    dims[SELF_DIM_BODY_OWNERSHIP] = nimcp_myelin_clamp(body_ownership, 0.0f, 1.0f);
    dims[SELF_DIM_SELF_BOUNDARY] = (recognition + body_ownership) / 2.0f;

    bridge->recognition_signal = recognition;

    nimcp_mutex_unlock(bridge->base.mutex);

    return self_awareness_snn_encode_state(bridge, dims, 3);
}

int self_awareness_snn_encode_agency(
    self_awareness_snn_bridge_t* bridge,
    float agency_level,
    uint32_t agency_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_encode_agency: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_AGENCY_SENSE] = nimcp_myelin_clamp(agency_level, 0.0f, 1.0f);
    /* Agency type modulates temporal continuity */
    dims[SELF_DIM_TEMPORAL_CONTINUITY] = (agency_type == 0) ? agency_level : agency_level * 0.5f;

    bridge->agency_signal = agency_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return self_awareness_snn_encode_state(bridge, dims, 2);
}

int self_awareness_snn_encode_metacognitive(
    self_awareness_snn_bridge_t* bridge,
    float metacog_level,
    float reflection_depth
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_encode_metacognitive: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SELF_DIM_COUNT] = {0};
    dims[SELF_DIM_METACOGNITIVE] = nimcp_myelin_clamp(metacog_level, 0.0f, 1.0f);
    dims[SELF_DIM_SELF_REFLECTION] = nimcp_myelin_clamp(reflection_depth, 0.0f, 1.0f);

    if (metacog_level > bridge->config.recognition_threshold) {
        bridge->last_awareness_state.agency_detected = true;
        bridge->last_awareness_state.recognition_magnitude = metacog_level;
        bridge->stats.agency_events++;

        if (bridge->agency_callback) {
            bridge->agency_callback(bridge, metacog_level, SELF_DIM_METACOGNITIVE,
                                   bridge->agency_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return self_awareness_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int self_awareness_snn_simulate(self_awareness_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_snn_simulate: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SELF_SNN_STATE_SIMULATING;

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
    bridge->last_awareness_state.self_recognition = nimcp_myelin_clamp(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_awareness_state.body_ownership = nimcp_myelin_clamp(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_awareness_state.agency_sense = nimcp_myelin_clamp(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_awareness_state.metacognitive_level = nimcp_myelin_clamp(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_awareness_state.self_reflection = nimcp_myelin_clamp(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_awareness_state.temporal_continuity = nimcp_myelin_clamp(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check recognition threshold */
    if (bridge->last_awareness_state.self_recognition > bridge->config.recognition_threshold) {
        bridge->last_awareness_state.self_recognized = true;
        bridge->stats.recognition_events++;

        if (bridge->recognition_callback) {
            bridge->recognition_callback(bridge, bridge->last_awareness_state.self_recognition,
                                        bridge->current_time_us, bridge->recognition_callback_data);
        }
    } else {
        bridge->last_awareness_state.self_recognized = false;
    }

    /* Check agency threshold */
    if (bridge->last_awareness_state.agency_sense > bridge->config.agency_threshold) {
        bridge->last_awareness_state.agency_detected = true;
        bridge->stats.agency_events++;
    } else {
        bridge->last_awareness_state.agency_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = SELF_SNN_STATE_IDLE;

    /* Invoke state callback */
    if (bridge->state_callback) {
        bridge->state_callback(bridge, &bridge->last_awareness_state, bridge->state_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_awareness_snn_step(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_step: bridge is NULL");
        return -1;
    }
    return self_awareness_snn_simulate(bridge, bridge->config.dt_ms);
}

int self_awareness_snn_forward(
    self_awareness_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    int spike_count = self_awareness_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_snn_forward: validation failed");
        return -1;
    }

    if (self_awareness_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int self_awareness_snn_get_awareness_state(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_state_t* awareness_state
) {
    if (!bridge || !awareness_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_get_awareness_state: required parameter is NULL (bridge, awareness_state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *awareness_state = bridge->last_awareness_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_awareness_snn_get_activations(
    self_awareness_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_snn_get_activations: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool self_awareness_snn_check_recognition(
    self_awareness_snn_bridge_t* bridge,
    float* recognition_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_awareness_state.self_recognition;
    if (recognition_level) {
        *recognition_level = level;
    }
    bool detected = level > bridge->config.recognition_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool self_awareness_snn_check_agency(
    self_awareness_snn_bridge_t* bridge,
    float* agency_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_awareness_state.agency_sense;
    if (agency_level) {
        *agency_level = level;
    }
    bool detected = level > bridge->config.agency_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool self_awareness_snn_check_state_change(
    self_awareness_snn_bridge_t* bridge,
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

int self_awareness_snn_get_dim_state(
    self_awareness_snn_bridge_t* bridge,
    uint32_t dim,
    self_awareness_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_awareness_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_awareness_snn_get_state(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_self_awareness = bridge->last_awareness_state.self_recognition;
    state->recognition_signal = bridge->recognition_signal;
    state->agency_signal = bridge->agency_signal;

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

int self_awareness_snn_get_stats(self_awareness_snn_bridge_t* bridge, self_awareness_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_awareness_snn_reset_stats(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(self_awareness_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float self_awareness_snn_get_awareness_level(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float awareness = bridge->last_awareness_state.self_recognition;
    nimcp_mutex_unlock(bridge->base.mutex);

    return awareness;
}

float self_awareness_snn_get_total_activity(self_awareness_snn_bridge_t* bridge) {
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

int self_awareness_snn_register_recognition_callback(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_recognition_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_register_recognition_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->recognition_callback = callback;
    bridge->recognition_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_awareness_snn_register_state_callback(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_state_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_register_state_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state_callback = callback;
    bridge->state_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_awareness_snn_register_agency_callback(
    self_awareness_snn_bridge_t* bridge,
    self_awareness_snn_agency_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_register_agency_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->agency_callback = callback;
    bridge->agency_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int self_awareness_snn_bio_async_connect(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_awareness_snn_bio_async_disconnect(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_awareness_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool self_awareness_snn_is_bio_async_connected(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

//=============================================================================
// Instance Health Agent Setter (B24 Upgrade)
//=============================================================================

void self_awareness_snn_bridge_set_instance_health_agent(
    self_awareness_snn_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B24 Upgrade)
//=============================================================================

int self_awareness_snn_bridge_training_begin(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    self_awareness_snn_bridge_heartbeat_instance(bridge->health_agent, "self_awareness_snn_bridge_training_begin", 0.0f);
    return 0;
}

int self_awareness_snn_bridge_training_end(self_awareness_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_snn_bridge_training_end: NULL argument");
        return -1;
    }
    self_awareness_snn_bridge_heartbeat_instance(bridge->health_agent, "self_awareness_snn_bridge_training_end", 1.0f);
    return 0;
}

int self_awareness_snn_bridge_training_step(self_awareness_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_awareness_snn_bridge_training_step: NULL argument");
        return -1;
    }
    self_awareness_snn_bridge_heartbeat_instance(bridge->health_agent, "self_awareness_snn_bridge_training_step", progress);
    return 0;
}
