/**
 * @file nimcp_curiosity_snn_bridge.c
 * @brief Curiosity - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/curiosity/nimcp_curiosity_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(curiosity_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_curiosity_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_curiosity_snn_bridge_mesh_registry = NULL;

nimcp_error_t curiosity_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_curiosity_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "curiosity_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "curiosity_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_curiosity_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_curiosity_snn_bridge_mesh_registry = registry;
    return err;
}

void curiosity_snn_bridge_mesh_unregister(void) {
    if (g_curiosity_snn_bridge_mesh_registry && g_curiosity_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_curiosity_snn_bridge_mesh_registry, g_curiosity_snn_bridge_mesh_id);
        g_curiosity_snn_bridge_mesh_id = 0;
        g_curiosity_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from curiosity_snn_bridge module (instance-level) */
static inline void curiosity_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_curiosity_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_curiosity_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_curiosity_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "CURIOSITY_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct curiosity_snn_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    curiosity_snn_config_t config;
    snn_network_t* snn;

    /* State */
    curiosity_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    curiosity_dim_state_t dim_states[CURIOSITY_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* drive_buffer;

    /* Drive state */
    curiosity_drive_t last_drive;
    float novelty_signal;
    float information_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    curiosity_snn_novelty_callback_t novelty_callback;
    void* novelty_callback_data;
    curiosity_snn_drive_callback_t drive_callback;
    void* drive_callback_data;
    curiosity_snn_interest_callback_t interest_callback;
    void* interest_callback_data;

    /* Statistics */
    curiosity_snn_stats_t stats;
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
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

curiosity_snn_config_t curiosity_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_config", 0.0f);


    curiosity_snn_config_t config = {
        .num_dimensions = CURIOSITY_DIM_COUNT,
        .neurons_per_dim = CURIOSITY_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = CURIOSITY_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = CURIOSITY_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = CURIOSITY_SNN_DECODE_INTEGRATION,
        .novelty_threshold = CURIOSITY_SNN_NOVELTY_THRESH,
        .exploration_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_novelty_detection = true,
        .novelty_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_exploration = true,
        .exploration_gain = 1.5f,
        .enable_learning_progress = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

curiosity_snn_bridge_t* curiosity_snn_create(const curiosity_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_create", 0.0f);


    curiosity_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(curiosity_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_snn_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = curiosity_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > CURIOSITY_SNN_MAX_DIMENSIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_create: invalid num_dimensions");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "curiosity_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_snn_create: bridge_base_init failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* novelty, surprise, info_gain, exploration, interest, seeking */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_snn_create: failed to create SNN");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->drive_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->drive_buffer || !bridge->prev_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "curiosity_snn_create: failed to allocate buffers");
        curiosity_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize drive to neutral */
    bridge->last_drive.novelty_level = 0.5f;
    bridge->last_drive.surprise_level = 0.5f;
    bridge->last_drive.information_gain = 0.5f;
    bridge->last_drive.exploration_drive = 0.5f;
    bridge->last_drive.knowledge_gap = 0.5f;
    bridge->last_drive.novelty_detected = false;
    bridge->last_drive.high_interest = false;
    bridge->last_drive.interest_magnitude = 0.0f;
    bridge->last_drive.seeking_level = 0.5f;
    bridge->last_drive.learning_progress = 0.5f;

    bridge->state = CURIOSITY_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->novelty_signal = 0.0f;
    bridge->information_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "curiosity_snn");
    return bridge;
}

void curiosity_snn_destroy(curiosity_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "curiosity_snn");

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_destro", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->drive_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int curiosity_snn_reset(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset drive */
    memset(&bridge->last_drive, 0, sizeof(curiosity_drive_t));
    bridge->last_drive.novelty_level = 0.5f;
    bridge->last_drive.surprise_level = 0.5f;
    bridge->last_drive.exploration_drive = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->drive_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = CURIOSITY_SNN_STATE_IDLE;
    bridge->novelty_signal = 0.0f;
    bridge->information_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int curiosity_snn_encode_state(
    curiosity_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = CURIOSITY_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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
                curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - bridge->prev_state[d];
        change_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    change_magnitude = (num_dims > 0) ? sqrtf(change_magnitude / num_dims) : 0.0f;

    if (change_magnitude > bridge->config.state_change_threshold) {
        bridge->stats.state_changes++;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int curiosity_snn_encode_novelty(
    curiosity_snn_bridge_t* bridge,
    float novelty,
    float surprise
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_encode_novelty: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[CURIOSITY_DIM_NOVELTY] = nimcp_clampf(novelty, 0.0f, 1.0f);
    dims[CURIOSITY_DIM_SURPRISE] = nimcp_clampf(surprise, 0.0f, 1.0f);
    dims[CURIOSITY_DIM_EXPLORATION] = (novelty + surprise) / 2.0f;

    bridge->novelty_signal = novelty;

    nimcp_mutex_unlock(bridge->base.mutex);

    return curiosity_snn_encode_state(bridge, dims, 3);
}

int curiosity_snn_encode_knowledge_gap(
    curiosity_snn_bridge_t* bridge,
    float gap_size,
    uint32_t gap_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_encode_knowledge_gap: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[CURIOSITY_DIM_KNOWLEDGE_GAP] = nimcp_clampf(gap_size, 0.0f, 1.0f);
    dims[CURIOSITY_DIM_SEEKING] = nimcp_clampf((float)gap_count / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return curiosity_snn_encode_state(bridge, dims, 2);
}

int curiosity_snn_encode_info_gain(
    curiosity_snn_bridge_t* bridge,
    float info_gain,
    uint32_t info_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_encode_info_gain: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[CURIOSITY_DIM_INFORMATION_GAIN] = nimcp_clampf(info_gain, 0.0f, 1.0f);
    dims[CURIOSITY_DIM_INTEREST] = nimcp_clampf(info_gain * 0.8f, 0.0f, 1.0f);

    bridge->information_signal = info_gain;

    if (info_gain > bridge->config.novelty_threshold) {
        bridge->last_drive.high_interest = true;
        bridge->last_drive.interest_magnitude = info_gain;
        bridge->stats.high_interest_events++;

        if (bridge->interest_callback) {
            bridge->interest_callback(bridge, info_gain, info_type,
                                     bridge->interest_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return curiosity_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int curiosity_snn_simulate(curiosity_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_simula", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = CURIOSITY_SNN_STATE_SIMULATING;

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
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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
                curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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
    bridge->last_drive.novelty_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_drive.surprise_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_drive.information_gain = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_drive.exploration_drive = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_drive.interest_magnitude = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_drive.seeking_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check novelty threshold */
    if (bridge->last_drive.novelty_level > bridge->config.novelty_threshold) {
        bridge->last_drive.novelty_detected = true;
        bridge->stats.novelty_detections++;

        if (bridge->novelty_callback) {
            bridge->novelty_callback(bridge, bridge->last_drive.novelty_level,
                                    bridge->current_time_us, bridge->novelty_callback_data);
        }
    } else {
        bridge->last_drive.novelty_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = CURIOSITY_SNN_STATE_IDLE;

    /* Invoke drive callback */
    if (bridge->drive_callback) {
        bridge->drive_callback(bridge, &bridge->last_drive, bridge->drive_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_snn_step(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_step", 0.0f);


    return curiosity_snn_simulate(bridge, bridge->config.dt_ms);
}

int curiosity_snn_forward(
    curiosity_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_forwar", 0.0f);


    int spike_count = curiosity_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_forward: validation failed");
        return -1;
    }

    if (curiosity_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int curiosity_snn_get_drive(
    curiosity_snn_bridge_t* bridge,
    curiosity_drive_t* drive
) {
    if (!bridge || !drive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_get_drive: required parameter is NULL (bridge, drive)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_dr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *drive = bridge->last_drive;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int curiosity_snn_get_activations(
    curiosity_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool curiosity_snn_check_novelty(
    curiosity_snn_bridge_t* bridge,
    float* novelty_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_drive.novelty_level;
    if (novelty_level) {
        *novelty_level = level;
    }
    bool detected = level > bridge->config.novelty_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool curiosity_snn_check_interest(
    curiosity_snn_bridge_t* bridge,
    float* interest_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_drive.interest_magnitude;
    if (interest_level) {
        *interest_level = level;
    }
    bool detected = level > bridge->config.novelty_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool curiosity_snn_check_state_change(
    curiosity_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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

int curiosity_snn_get_dim_state(
    curiosity_snn_bridge_t* bridge,
    uint32_t dim,
    curiosity_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "curiosity_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int curiosity_snn_get_state(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_exploration = bridge->last_drive.exploration_drive;
    state->novelty_signal = bridge->novelty_signal;
    state->information_signal = bridge->information_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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

int curiosity_snn_get_stats(curiosity_snn_bridge_t* bridge, curiosity_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int curiosity_snn_reset_stats(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(curiosity_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float curiosity_snn_get_exploration(curiosity_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_ex", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float exploration = bridge->last_drive.exploration_drive;
    nimcp_mutex_unlock(bridge->base.mutex);

    return exploration;
}

float curiosity_snn_get_total_activity(curiosity_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_get_to", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            curiosity_snn_bridge_heartbeat("curiosity_sn_loop",
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

int curiosity_snn_register_novelty_callback(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_novelty_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_register_novelty_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->novelty_callback = callback;
    bridge->novelty_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int curiosity_snn_register_drive_callback(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_drive_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_register_drive_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->drive_callback = callback;
    bridge->drive_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int curiosity_snn_register_interest_callback(
    curiosity_snn_bridge_t* bridge,
    curiosity_snn_interest_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_register_interest_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->interest_callback = callback;
    bridge->interest_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int curiosity_snn_bio_async_connect(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int curiosity_snn_bio_async_disconnect(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool curiosity_snn_is_bio_async_connected(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    curiosity_snn_bridge_heartbeat("curiosity_sn_curiosity_snn_is_bio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_snn_bridge_set_instance_health_agent(curiosity_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "curiosity_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_snn_bridge_training_begin(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    curiosity_snn_bridge_heartbeat_instance(bridge->health_agent, "curiosity_snn_bridge_training_begin", 0.0f);
    return 0;
}

int curiosity_snn_bridge_training_end(curiosity_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_snn_bridge_training_end: NULL argument");
        return -1;
    }
    curiosity_snn_bridge_heartbeat_instance(bridge->health_agent, "curiosity_snn_bridge_training_end", 1.0f);
    return 0;
}

int curiosity_snn_bridge_training_step(curiosity_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_snn_bridge_heartbeat_instance(bridge->health_agent, "curiosity_snn_bridge_training_step", progress);
    return 0;
}
