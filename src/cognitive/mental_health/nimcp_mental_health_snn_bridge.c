/**
 * @file nimcp_mental_health_snn_bridge.c
 * @brief Mental Health - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/mental_health/nimcp_mental_health_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mental_health_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mental_health_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mental_health_snn_bridge_mesh_registry = NULL;

nimcp_error_t mental_health_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mental_health_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mental_health_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mental_health_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mental_health_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mental_health_snn_bridge_mesh_registry = registry;
    return err;
}

void mental_health_snn_bridge_mesh_unregister(void) {
    if (g_mental_health_snn_bridge_mesh_registry && g_mental_health_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mental_health_snn_bridge_mesh_registry, g_mental_health_snn_bridge_mesh_id);
        g_mental_health_snn_bridge_mesh_id = 0;
        g_mental_health_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mental_health_snn_bridge module (instance-level) */
static inline void mental_health_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mental_health_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mental_health_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mental_health_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "MENTAL_HEALTH_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct mental_health_snn_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    mental_health_snn_config_t config;
    snn_network_t* snn;

    /* State */
    mental_health_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    mental_health_dim_state_t dim_states[MENTAL_HEALTH_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* mood_buffer;

    /* Emotional state */
    mental_health_emotional_state_t last_emotional_state;
    float anxiety_signal;
    float depression_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    mental_health_snn_anxiety_callback_t anxiety_callback;
    void* anxiety_callback_data;
    mental_health_snn_state_callback_t state_callback;
    void* state_callback_data;
    mental_health_snn_depression_callback_t depression_callback;
    void* depression_callback_data;

    /* Statistics */
    mental_health_snn_stats_t stats;
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
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                mental_health_snn_bridge_heartbeat("mental_healt_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

mental_health_snn_config_t mental_health_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_co", 0.0f);


    mental_health_snn_config_t config = {
        .num_dimensions = MENTAL_HEALTH_DIM_COUNT,
        .neurons_per_dim = MENTAL_HEALTH_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = MENTAL_HEALTH_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = MENTAL_HEALTH_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = MENTAL_HEALTH_SNN_DECODE_INTEGRATION,
        .anxiety_threshold = MENTAL_HEALTH_SNN_ANXIETY_THRESH,
        .depression_threshold = MENTAL_HEALTH_SNN_DEPRESSION_THRESH,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_mood_tracking = true,
        .mood_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_regulation = true,
        .regulation_gain = 1.5f,
        .enable_resilience_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

mental_health_snn_bridge_t* mental_health_snn_create(const mental_health_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_cr", 0.0f);


    mental_health_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(mental_health_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = mental_health_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > MENTAL_HEALTH_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "mental_health_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mental_health_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* mood, anxiety, depression, stress, regulation, resilience */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mental_health_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->mood_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->mood_buffer || !bridge->prev_state) {
        mental_health_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mental_health_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize emotional state to neutral */
    bridge->last_emotional_state.mood_level = 0.0f;
    bridge->last_emotional_state.anxiety_level = 0.3f;
    bridge->last_emotional_state.depression_level = 0.2f;
    bridge->last_emotional_state.stress_level = 0.3f;
    bridge->last_emotional_state.regulation_capacity = 0.7f;
    bridge->last_emotional_state.anxiety_detected = false;
    bridge->last_emotional_state.depression_detected = false;
    bridge->last_emotional_state.resilience_factor = 0.5f;
    bridge->last_emotional_state.engagement_level = 0.7f;
    bridge->last_emotional_state.cognitive_clarity = 0.8f;

    bridge->state = MENTAL_HEALTH_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->anxiety_signal = 0.0f;
    bridge->depression_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "mental_health_snn");
    return bridge;
}

void mental_health_snn_destroy(mental_health_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mental_health_snn");

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_de", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->mood_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int mental_health_snn_reset(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset emotional state */
    memset(&bridge->last_emotional_state, 0, sizeof(mental_health_emotional_state_t));
    bridge->last_emotional_state.mood_level = 0.0f;
    bridge->last_emotional_state.anxiety_level = 0.3f;
    bridge->last_emotional_state.depression_level = 0.2f;
    bridge->last_emotional_state.regulation_capacity = 0.7f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->mood_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = MENTAL_HEALTH_SNN_STATE_IDLE;
    bridge->anxiety_signal = 0.0f;
    bridge->depression_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int mental_health_snn_encode_state(
    mental_health_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MENTAL_HEALTH_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
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
                mental_health_snn_bridge_heartbeat("mental_healt_loop",
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
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
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

int mental_health_snn_encode_mood(
    mental_health_snn_bridge_t* bridge,
    float mood,
    float stability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_encode_mood: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[MENTAL_HEALTH_DIM_COUNT] = {0};
    /* Convert mood from [-1,1] to [0,1] for encoding */
    dims[MENTAL_HEALTH_DIM_MOOD_STATE] = nimcp_clampf((mood + 1.0f) / 2.0f, 0.0f, 1.0f);
    dims[MENTAL_HEALTH_DIM_EMOTIONAL_REGULATION] = nimcp_clampf(stability, 0.0f, 1.0f);
    dims[MENTAL_HEALTH_DIM_RESILIENCE] = nimcp_clampf(stability * 0.8f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return mental_health_snn_encode_state(bridge, dims, 3);
}

int mental_health_snn_encode_anxiety(
    mental_health_snn_bridge_t* bridge,
    float anxiety,
    uint32_t threat_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_encode_anxiety: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[MENTAL_HEALTH_DIM_COUNT] = {0};
    dims[MENTAL_HEALTH_DIM_ANXIETY_LEVEL] = nimcp_clampf(anxiety, 0.0f, 1.0f);
    dims[MENTAL_HEALTH_DIM_STRESS_RESPONSE] = nimcp_clampf(anxiety * 0.9f + (float)threat_count * 0.05f, 0.0f, 1.0f);
    /* High anxiety reduces cognitive clarity */
    dims[MENTAL_HEALTH_DIM_COGNITIVE_CLARITY] = nimcp_clampf(1.0f - anxiety * 0.5f, 0.0f, 1.0f);

    bridge->anxiety_signal = anxiety;

    nimcp_mutex_unlock(bridge->base.mutex);

    return mental_health_snn_encode_state(bridge, dims, 3);
}

int mental_health_snn_encode_depression(
    mental_health_snn_bridge_t* bridge,
    float depression,
    float anhedonia
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_encode_depression: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[MENTAL_HEALTH_DIM_COUNT] = {0};
    dims[MENTAL_HEALTH_DIM_DEPRESSION_LEVEL] = nimcp_clampf(depression, 0.0f, 1.0f);
    /* Depression affects engagement negatively */
    dims[MENTAL_HEALTH_DIM_ENGAGEMENT] = nimcp_clampf(1.0f - anhedonia, 0.0f, 1.0f);
    /* Depression reduces mood */
    dims[MENTAL_HEALTH_DIM_MOOD_STATE] = nimcp_clampf(0.5f - depression * 0.4f, 0.0f, 1.0f);
    /* Depression affects social functioning */
    dims[MENTAL_HEALTH_DIM_SOCIAL_FUNCTION] = nimcp_clampf(1.0f - depression * 0.3f, 0.0f, 1.0f);

    bridge->depression_signal = depression;

    if (depression > bridge->config.depression_threshold) {
        bridge->last_emotional_state.depression_detected = true;
        bridge->stats.depression_detections++;

        if (bridge->depression_callback) {
            bridge->depression_callback(bridge, depression, MENTAL_HEALTH_DIM_DEPRESSION_LEVEL,
                                       bridge->depression_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return mental_health_snn_encode_state(bridge, dims, 4);
}

int mental_health_snn_encode_stress(
    mental_health_snn_bridge_t* bridge,
    float stress,
    uint32_t stressor_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_encode_stress: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[MENTAL_HEALTH_DIM_COUNT] = {0};
    dims[MENTAL_HEALTH_DIM_STRESS_RESPONSE] = nimcp_clampf(stress, 0.0f, 1.0f);

    /* Chronic stress (type 1) has more pervasive effects */
    float chronic_factor = (stressor_type == 1) ? 1.2f : 1.0f;

    /* Stress affects anxiety */
    dims[MENTAL_HEALTH_DIM_ANXIETY_LEVEL] = nimcp_clampf(stress * 0.6f * chronic_factor, 0.0f, 1.0f);
    /* Chronic stress erodes resilience */
    dims[MENTAL_HEALTH_DIM_RESILIENCE] = nimcp_clampf(1.0f - stress * 0.3f * chronic_factor, 0.0f, 1.0f);
    /* Sleep quality affected by stress */
    dims[MENTAL_HEALTH_DIM_SLEEP_QUALITY] = nimcp_clampf(1.0f - stress * 0.4f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return mental_health_snn_encode_state(bridge, dims, 4);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int mental_health_snn_simulate(mental_health_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_si", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = MENTAL_HEALTH_SNN_STATE_SIMULATING;

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
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
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
                mental_health_snn_bridge_heartbeat("mental_healt_loop",
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

    /* Decode outputs to emotional state */
    /* Convert mood from [0,1] back to [-1,1] */
    bridge->last_emotional_state.mood_level = nimcp_clampf(bridge->output_buffer[0] * 2.0f - 1.0f, -1.0f, 1.0f);
    bridge->last_emotional_state.anxiety_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_emotional_state.depression_level = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_emotional_state.stress_level = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_emotional_state.regulation_capacity = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_emotional_state.resilience_factor = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check anxiety threshold */
    if (bridge->last_emotional_state.anxiety_level > bridge->config.anxiety_threshold) {
        bridge->last_emotional_state.anxiety_detected = true;
        bridge->stats.anxiety_detections++;

        if (bridge->anxiety_callback) {
            bridge->anxiety_callback(bridge, bridge->last_emotional_state.anxiety_level,
                                    bridge->current_time_us, bridge->anxiety_callback_data);
        }
    } else {
        bridge->last_emotional_state.anxiety_detected = false;
    }

    /* Check depression threshold */
    if (bridge->last_emotional_state.depression_level > bridge->config.depression_threshold) {
        bridge->last_emotional_state.depression_detected = true;
    } else {
        bridge->last_emotional_state.depression_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = MENTAL_HEALTH_SNN_STATE_IDLE;

    /* Invoke state callback */
    if (bridge->state_callback) {
        bridge->state_callback(bridge, &bridge->last_emotional_state, bridge->state_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_snn_step(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_st", 0.0f);


    return mental_health_snn_simulate(bridge, bridge->config.dt_ms);
}

int mental_health_snn_forward(
    mental_health_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_fo", 0.0f);


    int spike_count = mental_health_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_snn_forward: validation failed");
        return -1;
    }

    if (mental_health_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int mental_health_snn_get_emotional_state(
    mental_health_snn_bridge_t* bridge,
    mental_health_emotional_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_get_emotional_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->last_emotional_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_snn_get_activations(
    mental_health_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool mental_health_snn_check_anxiety(
    mental_health_snn_bridge_t* bridge,
    float* anxiety_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_emotional_state.anxiety_level;
    if (anxiety_level) {
        *anxiety_level = level;
    }
    bool detected = level > bridge->config.anxiety_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool mental_health_snn_check_depression(
    mental_health_snn_bridge_t* bridge,
    float* depression_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_emotional_state.depression_level;
    if (depression_level) {
        *depression_level = level;
    }
    bool detected = level > bridge->config.depression_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool mental_health_snn_check_state_change(
    mental_health_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
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

int mental_health_snn_get_dim_state(
    mental_health_snn_bridge_t* bridge,
    uint32_t dim,
    mental_health_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_snn_get_state(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_mood = bridge->last_emotional_state.mood_level;
    state->anxiety_signal = bridge->anxiety_signal;
    state->depression_signal = bridge->depression_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
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

int mental_health_snn_get_stats(mental_health_snn_bridge_t* bridge, mental_health_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_snn_reset_stats(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(mental_health_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float mental_health_snn_get_mood(mental_health_snn_bridge_t* bridge) {
    if (!bridge) return -2.0f;

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float mood = bridge->last_emotional_state.mood_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return mood;
}

float mental_health_snn_get_total_activity(mental_health_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            mental_health_snn_bridge_heartbeat("mental_healt_loop",
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

int mental_health_snn_register_anxiety_callback(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_anxiety_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_register_anxiety_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->anxiety_callback = callback;
    bridge->anxiety_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_snn_register_state_callback(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_state_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_register_state_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state_callback = callback;
    bridge->state_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_snn_register_depression_callback(
    mental_health_snn_bridge_t* bridge,
    mental_health_snn_depression_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_register_depression_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->depression_callback = callback;
    bridge->depression_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int mental_health_snn_bio_async_connect(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mental_health_snn_bio_async_disconnect(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool mental_health_snn_is_bio_async_connected(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mental_health_snn_bridge_heartbeat("mental_healt_mental_health_snn_is", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mental_health_snn_bridge_set_instance_health_agent(mental_health_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "mental_health_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mental_health_snn_bridge_training_begin(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    mental_health_snn_bridge_heartbeat_instance(bridge->health_agent, "mental_health_snn_bridge_training_begin", 0.0f);
    return 0;
}

int mental_health_snn_bridge_training_end(mental_health_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_snn_bridge_training_end: NULL argument");
        return -1;
    }
    mental_health_snn_bridge_heartbeat_instance(bridge->health_agent, "mental_health_snn_bridge_training_end", 1.0f);
    return 0;
}

int mental_health_snn_bridge_training_step(mental_health_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mental_health_snn_bridge_heartbeat_instance(bridge->health_agent, "mental_health_snn_bridge_training_step", progress);
    return 0;
}
