/**
 * @file nimcp_rcog_snn_bridge.c
 * @brief Recursive Cognition - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/recursive/nimcp_rcog_snn_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rcog_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from rcog_snn_bridge module (instance-level) */
static inline void rcog_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_rcog_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_rcog_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_rcog_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

static nimcp_health_agent_t* g_rcog_snn_bridge_instance_health_agent = NULL;

void rcog_snn_bridge_set_instance_health_agent(
    rcog_snn_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    (void)bridge;
    g_rcog_snn_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_snn_bridge_training_begin(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    rcog_snn_bridge_heartbeat_instance(
        g_rcog_snn_bridge_instance_health_agent, "rcog_snn_training_begin", 0.0f);
    return 0;
}

int rcog_snn_bridge_training_step(rcog_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_snn_bridge_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_snn_bridge_heartbeat_instance(
        g_rcog_snn_bridge_instance_health_agent, "rcog_snn_training_step", clamped);
    return 0;
}

int rcog_snn_bridge_training_end(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_snn_bridge_training_end: NULL argument");
        return -1;
    }
    rcog_snn_bridge_heartbeat_instance(
        g_rcog_snn_bridge_instance_health_agent, "rcog_snn_training_end", 1.0f);
    return 0;
}

#define LOG_MODULE "RCOG_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct rcog_snn_bridge {
    bridge_base_t base;
    rcog_snn_config_t config;
    snn_network_t* snn;

    /* State */
    rcog_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    rcog_dim_state_t dim_states[RCOG_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* cognitive_buffer;

    /* Cognitive state */
    rcog_cognitive_state_t last_cognitive_state;
    float depth_signal;
    float meta_cognitive_signal;
    float self_reference_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    rcog_snn_depth_callback_t depth_callback;
    void* depth_callback_data;
    rcog_snn_state_callback_t state_callback;
    void* state_callback_data;
    rcog_snn_self_ref_callback_t self_ref_callback;
    void* self_ref_callback_data;

    /* Statistics */
    rcog_snn_stats_t stats;

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
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

rcog_snn_config_t rcog_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_config_defa", 0.0f);


    rcog_snn_config_t config = {
        .num_dimensions = RCOG_DIM_COUNT,
        .neurons_per_dim = RCOG_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = RCOG_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = RCOG_SNN_ENCODE_HIERARCHICAL,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = RCOG_SNN_DECODE_INTEGRATION,
        .depth_threshold = RCOG_SNN_DEPTH_THRESH,
        .meta_cognitive_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_self_reference_detection = true,
        .self_reference_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_depth_tracking = true,
        .depth_tracking_gain = 1.5f,
        .enable_meta_cognitive = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

rcog_snn_bridge_t* rcog_snn_create(const rcog_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_create", 0.0f);


    rcog_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(rcog_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = rcog_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > RCOG_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "rcog_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "rcog_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    /* Output: depth, meta_cognitive, self_reference, hierarchy, complexity, progress */
    uint32_t output_dim = 6;

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->cognitive_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->cognitive_buffer || !bridge->prev_state) {
        rcog_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize cognitive state to neutral */
    bridge->last_cognitive_state.recursion_depth = 0.0f;
    bridge->last_cognitive_state.meta_cognitive_level = 0.5f;
    bridge->last_cognitive_state.self_reference_intensity = 0.0f;
    bridge->last_cognitive_state.hierarchical_position = 0.5f;
    bridge->last_cognitive_state.problem_complexity = 0.5f;
    bridge->last_cognitive_state.deep_recursion_detected = false;
    bridge->last_cognitive_state.self_reference_loop = false;
    bridge->last_cognitive_state.decomposition_progress = 0.0f;
    bridge->last_cognitive_state.aggregation_confidence = 0.5f;
    bridge->last_cognitive_state.refinement_progress = 0.0f;

    bridge->state = RCOG_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->depth_signal = 0.0f;
    bridge->meta_cognitive_signal = 0.0f;
    bridge->self_reference_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "rcog_snn");
    return bridge;
}

void rcog_snn_destroy(rcog_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rcog_snn");

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_destroy", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->cognitive_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int rcog_snn_reset(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset cognitive state */
    memset(&bridge->last_cognitive_state, 0, sizeof(rcog_cognitive_state_t));
    bridge->last_cognitive_state.meta_cognitive_level = 0.5f;
    bridge->last_cognitive_state.hierarchical_position = 0.5f;
    bridge->last_cognitive_state.aggregation_confidence = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->cognitive_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = RCOG_SNN_STATE_IDLE;
    bridge->depth_signal = 0.0f;
    bridge->meta_cognitive_signal = 0.0f;
    bridge->self_reference_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int rcog_snn_encode_state(
    rcog_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_encode_stat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = RCOG_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Hierarchical population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float value = nimcp_clampf(dimensions[d], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Update dimension state */
        bridge->dim_states[d].activation = value;
        bridge->dim_states[d].mean_rate_hz = rate;

        /* Hierarchical encoding: different neurons for different value ranges */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_dim > 256) {
                rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                                 (float)(n + 1) / (float)neurons_per_dim);
            }

            float preferred = (neurons_per_dim <= 1) ? 0.5f : (float)n / (float)(neurons_per_dim - 1);
            float diff = value - preferred;
            /* Hierarchical tuning: sharper for recursion depth, broader for others */
            float sigma = (d == RCOG_DIM_RECURSION_DEPTH) ? 0.05f : 0.1f;
            float tuning = expf(-diff * diff / (2.0f * sigma * sigma));
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
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
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

int rcog_snn_encode_depth(
    rcog_snn_bridge_t* bridge,
    float depth,
    uint32_t max_depth
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_encode_depth: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_encode_dept", 0.0f);


    if (max_depth == 0) max_depth = 1;

    nimcp_mutex_lock(bridge->base.mutex);

    float normalized_depth = nimcp_clampf(depth / (float)max_depth, 0.0f, 1.0f);
    float dims[RCOG_DIM_COUNT] = {0};
    dims[RCOG_DIM_RECURSION_DEPTH] = normalized_depth;
    /* Higher depth correlates with working memory load */
    dims[RCOG_DIM_WORKING_MEMORY_LOAD] = normalized_depth * 0.8f;
    /* Meta-cognitive awareness increases with depth */
    dims[RCOG_DIM_META_COGNITIVE_LEVEL] = nimcp_clampf(normalized_depth * 1.2f, 0.0f, 1.0f);

    bridge->depth_signal = normalized_depth;

    nimcp_mutex_unlock(bridge->base.mutex);

    return rcog_snn_encode_state(bridge, dims, 3);
}

int rcog_snn_encode_meta_cognitive(
    rcog_snn_bridge_t* bridge,
    float awareness,
    float confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_encode_meta_cognitive: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_encode_meta", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[RCOG_DIM_COUNT] = {0};
    dims[RCOG_DIM_META_COGNITIVE_LEVEL] = nimcp_clampf(awareness, 0.0f, 1.0f);
    dims[RCOG_DIM_AGGREGATION_CONFIDENCE] = nimcp_clampf(confidence, 0.0f, 1.0f);
    dims[RCOG_DIM_ATTENTION_FOCUS] = (awareness + confidence) / 2.0f;

    bridge->meta_cognitive_signal = awareness;

    nimcp_mutex_unlock(bridge->base.mutex);

    return rcog_snn_encode_state(bridge, dims, 3);
}

int rcog_snn_encode_self_reference(
    rcog_snn_bridge_t* bridge,
    float intensity,
    uint32_t loop_depth
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_encode_self_reference: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_encode_self", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[RCOG_DIM_COUNT] = {0};
    dims[RCOG_DIM_SELF_REFERENCE] = nimcp_clampf(intensity, 0.0f, 1.0f);
    /* Loop depth affects meta-cognitive level */
    float loop_factor = nimcp_clampf((float)loop_depth / 5.0f, 0.0f, 1.0f);
    dims[RCOG_DIM_META_COGNITIVE_LEVEL] = nimcp_clampf(intensity * 0.5f + loop_factor * 0.5f, 0.0f, 1.0f);

    bridge->self_reference_signal = intensity;

    /* Trigger callback if self-reference detected */
    if (intensity > bridge->config.depth_threshold && bridge->self_ref_callback) {
        bridge->self_ref_callback(bridge, intensity, loop_depth,
                                  bridge->self_ref_callback_data);
        bridge->stats.self_reference_events++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return rcog_snn_encode_state(bridge, dims, 2);
}

int rcog_snn_encode_hierarchy(
    rcog_snn_bridge_t* bridge,
    uint32_t level,
    uint32_t total_levels
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_encode_hierarchy: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_encode_hier", 0.0f);


    if (total_levels == 0) total_levels = 1;

    nimcp_mutex_lock(bridge->base.mutex);

    float normalized_level = nimcp_clampf((float)level / (float)total_levels, 0.0f, 1.0f);
    float dims[RCOG_DIM_COUNT] = {0};
    dims[RCOG_DIM_HIERARCHICAL_POSITION] = normalized_level;
    /* Decomposition progress correlates with level */
    dims[RCOG_DIM_DECOMPOSITION_PROGRESS] = normalized_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return rcog_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int rcog_snn_simulate(rcog_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = RCOG_SNN_STATE_SIMULATING;

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
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
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
                rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
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

    /* Decode outputs into cognitive state */
    bridge->last_cognitive_state.recursion_depth = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_cognitive_state.meta_cognitive_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_cognitive_state.self_reference_intensity = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_cognitive_state.hierarchical_position = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_cognitive_state.problem_complexity = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_cognitive_state.decomposition_progress = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check for deep recursion */
    if (bridge->last_cognitive_state.recursion_depth > bridge->config.depth_threshold) {
        bridge->last_cognitive_state.deep_recursion_detected = true;
        bridge->stats.deep_recursion_events++;

        if (bridge->depth_callback) {
            bridge->depth_callback(bridge, bridge->last_cognitive_state.recursion_depth,
                                   bridge->current_time_us, bridge->depth_callback_data);
        }
    } else {
        bridge->last_cognitive_state.deep_recursion_detected = false;
    }

    /* Check for self-reference loop */
    if (bridge->last_cognitive_state.self_reference_intensity > bridge->config.depth_threshold) {
        bridge->last_cognitive_state.self_reference_loop = true;
    } else {
        bridge->last_cognitive_state.self_reference_loop = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = RCOG_SNN_STATE_IDLE;

    /* Invoke state callback */
    if (bridge->state_callback) {
        bridge->state_callback(bridge, &bridge->last_cognitive_state, bridge->state_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_snn_step(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_step", 0.0f);


    return rcog_snn_simulate(bridge, bridge->config.dt_ms);
}

int rcog_snn_forward(
    rcog_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_forward", 0.0f);


    int spike_count = rcog_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_snn_forward: validation failed");
        return -1;
    }

    if (rcog_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int rcog_snn_get_cognitive_state(
    rcog_snn_bridge_t* bridge,
    rcog_cognitive_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_get_cognitive_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_cogniti", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->last_cognitive_state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_snn_get_activations(
    rcog_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_activat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool rcog_snn_check_deep_recursion(
    rcog_snn_bridge_t* bridge,
    float* depth_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_check_deep_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_cognitive_state.recursion_depth;
    if (depth_level) {
        *depth_level = level;
    }
    bool detected = level > bridge->config.depth_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool rcog_snn_check_self_reference(
    rcog_snn_bridge_t* bridge,
    float* intensity
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_check_self_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_cognitive_state.self_reference_intensity;
    if (intensity) {
        *intensity = level;
    }
    bool detected = level > bridge->config.depth_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool rcog_snn_check_state_change(
    rcog_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_check_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
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

int rcog_snn_get_dim_state(
    rcog_snn_bridge_t* bridge,
    uint32_t dim,
    rcog_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_dim_sta", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_snn_get_state(
    rcog_snn_bridge_t* bridge,
    rcog_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_depth = bridge->last_cognitive_state.recursion_depth;
    state->meta_cognitive_signal = bridge->meta_cognitive_signal;
    state->self_reference_signal = bridge->self_reference_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
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

int rcog_snn_get_stats(rcog_snn_bridge_t* bridge, rcog_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_snn_reset_stats(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(rcog_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float rcog_snn_get_depth(rcog_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_depth", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float depth = bridge->last_cognitive_state.recursion_depth;
    nimcp_mutex_unlock(bridge->base.mutex);

    return depth;
}

float rcog_snn_get_total_activity(rcog_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_get_total_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            rcog_snn_bridge_heartbeat("rcog_snn_bri_loop",
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

int rcog_snn_register_depth_callback(
    rcog_snn_bridge_t* bridge,
    rcog_snn_depth_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_register_depth_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_register_de", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->depth_callback = callback;
    bridge->depth_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_snn_register_state_callback(
    rcog_snn_bridge_t* bridge,
    rcog_snn_state_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_register_state_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_register_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state_callback = callback;
    bridge->state_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_snn_register_self_ref_callback(
    rcog_snn_bridge_t* bridge,
    rcog_snn_self_ref_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_register_self_ref_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_register_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->self_ref_callback = callback;
    bridge->self_ref_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int rcog_snn_bio_async_connect(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_bio_async_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_snn_bio_async_disconnect(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_bio_async_d", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool rcog_snn_is_bio_async_connected(rcog_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_snn_bridge_heartbeat("rcog_snn_bri_rcog_snn_is_bio_asyn", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}
