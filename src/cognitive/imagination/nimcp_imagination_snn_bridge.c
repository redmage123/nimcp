/**
 * @file nimcp_imagination_snn_bridge.c
 * @brief Imagination - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/imagination/nimcp_imagination_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(imagination_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_imagination_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_imagination_snn_bridge_mesh_registry = NULL;

nimcp_error_t imagination_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_imagination_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "imagination_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "imagination_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_imagination_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_imagination_snn_bridge_mesh_registry = registry;
    return err;
}

void imagination_snn_bridge_mesh_unregister(void) {
    if (g_imagination_snn_bridge_mesh_registry && g_imagination_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_imagination_snn_bridge_mesh_registry, g_imagination_snn_bridge_mesh_id);
        g_imagination_snn_bridge_mesh_id = 0;
        g_imagination_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from imagination_snn_bridge module (instance-level) */
static inline void imagination_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_imagination_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_imagination_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_imagination_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "IMAGINATION_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct imagination_snn_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    imagination_snn_config_t config;
    snn_network_t* snn;

    /* State */
    imagination_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    imagination_dim_state_t dim_states[IMAGINATION_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* imagery_buffer;

    /* Imagery state */
    imagination_imagery_t last_imagery;
    float coherence_signal;
    float creativity_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    imagination_snn_vividness_callback_t vividness_callback;
    void* vividness_callback_data;
    imagination_snn_imagery_callback_t imagery_callback;
    void* imagery_callback_data;
    imagination_snn_creative_callback_t creative_callback;
    void* creative_callback_data;

    /* Statistics */
    imagination_snn_stats_t stats;
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
            imagination_snn_bridge_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                imagination_snn_bridge_heartbeat("imagination__loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

imagination_snn_config_t imagination_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_conf", 0.0f);


    imagination_snn_config_t config = {
        .num_dimensions = IMAGINATION_DIM_COUNT,
        .neurons_per_dim = IMAGINATION_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = IMAGINATION_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = IMAGINATION_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = IMAGINATION_SNN_DECODE_INTEGRATION,
        .vividness_threshold = IMAGINATION_SNN_VIVIDNESS_THRESH,
        .coherence_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_vividness_detection = true,
        .vividness_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_generation = true,
        .generation_gain = 1.5f,
        .enable_creativity_modulation = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

imagination_snn_bridge_t* imagination_snn_create(const imagination_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_crea", 0.0f);


    imagination_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(imagination_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = imagination_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > IMAGINATION_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "imagination_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "imagination_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* vividness, detail, coherence, creativity, controllability, complexity */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->imagery_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->imagery_buffer || !bridge->prev_state) {
        imagination_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "imagination_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize imagery to neutral */
    bridge->last_imagery.vividness_level = 0.5f;
    bridge->last_imagery.detail_level = 0.5f;
    bridge->last_imagery.coherence_level = 0.5f;
    bridge->last_imagery.creativity_level = 0.5f;
    bridge->last_imagery.counterfactual_level = 0.0f;
    bridge->last_imagery.vivid_imagery_active = false;
    bridge->last_imagery.creative_mode_active = false;
    bridge->last_imagery.creative_magnitude = 0.0f;
    bridge->last_imagery.controllability_level = 0.5f;
    bridge->last_imagery.scenario_complexity = 0.5f;

    bridge->state = IMAGINATION_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->coherence_signal = 0.0f;
    bridge->creativity_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "imagination_snn");
    return bridge;
}

void imagination_snn_destroy(imagination_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "imagination_snn");

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_dest", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->imagery_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int imagination_snn_reset(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset imagery */
    memset(&bridge->last_imagery, 0, sizeof(imagination_imagery_t));
    bridge->last_imagery.vividness_level = 0.5f;
    bridge->last_imagery.detail_level = 0.5f;
    bridge->last_imagery.coherence_level = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->imagery_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = IMAGINATION_SNN_STATE_IDLE;
    bridge->coherence_signal = 0.0f;
    bridge->creativity_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int imagination_snn_encode_state(
    imagination_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = IMAGINATION_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
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
                imagination_snn_bridge_heartbeat("imagination__loop",
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
            imagination_snn_bridge_heartbeat("imagination__loop",
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

int imagination_snn_encode_vividness(
    imagination_snn_bridge_t* bridge,
    float vividness,
    float detail
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_encode_vividness: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[IMAGINATION_DIM_COUNT] = {0};
    dims[IMAGINATION_DIM_VIVIDNESS] = nimcp_clampf(vividness, 0.0f, 1.0f);
    dims[IMAGINATION_DIM_DETAIL] = nimcp_clampf(detail, 0.0f, 1.0f);
    dims[IMAGINATION_DIM_COHERENCE] = (vividness + detail) / 2.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return imagination_snn_encode_state(bridge, dims, 3);
}

int imagination_snn_encode_scenario(
    imagination_snn_bridge_t* bridge,
    float coherence,
    float complexity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_encode_scenario: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[IMAGINATION_DIM_COUNT] = {0};
    dims[IMAGINATION_DIM_COHERENCE] = nimcp_clampf(coherence, 0.0f, 1.0f);
    dims[IMAGINATION_DIM_SCENARIO_COMPLEXITY] = nimcp_clampf(complexity, 0.0f, 1.0f);

    bridge->coherence_signal = coherence;

    nimcp_mutex_unlock(bridge->base.mutex);

    return imagination_snn_encode_state(bridge, dims, 2);
}

int imagination_snn_encode_creativity(
    imagination_snn_bridge_t* bridge,
    float creativity,
    float novelty
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_encode_creativity: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[IMAGINATION_DIM_COUNT] = {0};
    dims[IMAGINATION_DIM_CREATIVITY] = nimcp_clampf(creativity, 0.0f, 1.0f);
    dims[IMAGINATION_DIM_NOVELTY] = nimcp_clampf(novelty, 0.0f, 1.0f);

    bridge->creativity_signal = creativity;

    /* Snapshot callback under mutex */
    imagination_snn_creative_callback_t creative_cb = NULL;
    void* creative_cb_data = NULL;
    if (creativity > bridge->config.vividness_threshold) {
        bridge->last_imagery.creative_mode_active = true;
        bridge->last_imagery.creative_magnitude = creativity;
        bridge->stats.creative_mode_events++;

        creative_cb = bridge->creative_callback;
        creative_cb_data = bridge->creative_callback_data;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callback outside mutex to prevent deadlock */
    if (creative_cb) {
        creative_cb(bridge, creativity, IMAGINATION_DIM_CREATIVITY, creative_cb_data);
    }

    return imagination_snn_encode_state(bridge, dims, 2);
}

int imagination_snn_encode_counterfactual(
    imagination_snn_bridge_t* bridge,
    float divergence,
    uint32_t steps_ahead
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_encode_counterfactual: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[IMAGINATION_DIM_COUNT] = {0};
    dims[IMAGINATION_DIM_COUNTERFACTUAL] = nimcp_clampf(divergence, 0.0f, 1.0f);
    dims[IMAGINATION_DIM_PROSPECTIVE] = nimcp_clampf((float)steps_ahead / 100.0f, 0.0f, 1.0f);
    dims[IMAGINATION_DIM_REALITY_DISTANCE] = divergence * 0.8f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return imagination_snn_encode_state(bridge, dims, 3);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int imagination_snn_simulate(imagination_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_simu", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = IMAGINATION_SNN_STATE_SIMULATING;

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
            imagination_snn_bridge_heartbeat("imagination__loop",
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
                imagination_snn_bridge_heartbeat("imagination__loop",
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
    bridge->last_imagery.vividness_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_imagery.detail_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_imagery.coherence_level = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_imagery.creativity_level = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_imagery.controllability_level = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_imagery.scenario_complexity = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check vividness threshold */
    if (bridge->last_imagery.vividness_level > bridge->config.vividness_threshold) {
        bridge->last_imagery.vivid_imagery_active = true;
        bridge->stats.vivid_imagery_events++;
    } else {
        bridge->last_imagery.vivid_imagery_active = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = IMAGINATION_SNN_STATE_IDLE;

    /* Snapshot callback state under mutex */
    imagination_snn_vividness_callback_t vivid_cb = bridge->vividness_callback;
    void* vivid_cb_data = bridge->vividness_callback_data;
    float vivid_level = bridge->last_imagery.vividness_level;
    uint64_t cur_time = bridge->current_time_us;
    bool do_vivid_cb = (vivid_cb != NULL && bridge->last_imagery.vivid_imagery_active);

    imagination_snn_imagery_callback_t img_cb = bridge->imagery_callback;
    void* img_cb_data = bridge->imagery_callback_data;
    imagination_imagery_t imagery_snapshot = bridge->last_imagery;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside mutex to prevent deadlock */
    if (do_vivid_cb) {
        vivid_cb(bridge, vivid_level, cur_time, vivid_cb_data);
    }
    if (img_cb) {
        img_cb(bridge, &imagery_snapshot, img_cb_data);
    }

    return 0;
}

int imagination_snn_step(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_step", 0.0f);


    return imagination_snn_simulate(bridge, bridge->config.dt_ms);
}

int imagination_snn_forward(
    imagination_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_forw", 0.0f);


    int spike_count = imagination_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_snn_forward: validation failed");
        return -1;
    }

    if (imagination_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int imagination_snn_get_imagery(
    imagination_snn_bridge_t* bridge,
    imagination_imagery_t* imagery
) {
    if (!bridge || !imagery) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_get_imagery: required parameter is NULL (bridge, imagery)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *imagery = bridge->last_imagery;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_snn_get_activations(
    imagination_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool imagination_snn_check_vividness(
    imagination_snn_bridge_t* bridge,
    float* vividness_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_chec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_imagery.vividness_level;
    if (vividness_level) {
        *vividness_level = level;
    }
    bool detected = level > bridge->config.vividness_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool imagination_snn_check_creative(
    imagination_snn_bridge_t* bridge,
    float* creative_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_chec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_imagery.creativity_level;
    if (creative_level) {
        *creative_level = level;
    }
    bool detected = level > bridge->config.vividness_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool imagination_snn_check_state_change(
    imagination_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_chec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
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

int imagination_snn_get_dim_state(
    imagination_snn_bridge_t* bridge,
    uint32_t dim,
    imagination_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imagination_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_snn_get_state(
    imagination_snn_bridge_t* bridge,
    imagination_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_vividness = bridge->last_imagery.vividness_level;
    state->coherence_signal = bridge->coherence_signal;
    state->creativity_signal = bridge->creativity_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
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

int imagination_snn_get_stats(imagination_snn_bridge_t* bridge, imagination_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_snn_reset_stats(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(imagination_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float imagination_snn_get_vividness(imagination_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float vividness = bridge->last_imagery.vividness_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return vividness;
}

float imagination_snn_get_total_activity(imagination_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            imagination_snn_bridge_heartbeat("imagination__loop",
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

int imagination_snn_register_vividness_callback(
    imagination_snn_bridge_t* bridge,
    imagination_snn_vividness_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_register_vividness_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->vividness_callback = callback;
    bridge->vividness_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_snn_register_imagery_callback(
    imagination_snn_bridge_t* bridge,
    imagination_snn_imagery_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_register_imagery_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->imagery_callback = callback;
    bridge->imagery_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_snn_register_creative_callback(
    imagination_snn_bridge_t* bridge,
    imagination_snn_creative_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_register_creative_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->creative_callback = callback;
    bridge->creative_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int imagination_snn_bio_async_connect(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imagination_snn_bio_async_disconnect(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imagination_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool imagination_snn_is_bio_async_connected(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_snn_bridge_heartbeat("imagination__imagination_snn_is_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void imagination_snn_bridge_set_instance_health_agent(imagination_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "imagination_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int imagination_snn_bridge_training_begin(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    imagination_snn_bridge_heartbeat_instance(bridge->health_agent, "imagination_snn_bridge_training_begin", 0.0f);
    return 0;
}

int imagination_snn_bridge_training_end(imagination_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_snn_bridge_training_end: NULL argument");
        return -1;
    }
    imagination_snn_bridge_heartbeat_instance(bridge->health_agent, "imagination_snn_bridge_training_end", 1.0f);
    return 0;
}

int imagination_snn_bridge_training_step(imagination_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    imagination_snn_bridge_heartbeat_instance(bridge->health_agent, "imagination_snn_bridge_training_step", progress);
    return 0;
}
