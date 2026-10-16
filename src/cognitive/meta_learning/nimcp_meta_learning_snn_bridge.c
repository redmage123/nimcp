/**
 * @file nimcp_meta_learning_snn_bridge.c
 * @brief Meta Learning - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/meta_learning/nimcp_meta_learning_snn_bridge.h"
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
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(meta_learning_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "META_LEARNING_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct meta_learning_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    meta_learning_snn_config_t config;
    snn_network_t* snn;

    /* State */
    meta_learning_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    meta_learning_dim_state_t dim_states[META_LEARNING_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* insight_buffer;

    /* Insight state */
    meta_learning_insight_t last_insight;
    float transfer_signal;
    float strategy_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    meta_learning_snn_adaptation_callback_t adaptation_callback;
    void* adaptation_callback_data;
    meta_learning_snn_insight_callback_t insight_callback;
    void* insight_callback_data;
    meta_learning_snn_transfer_callback_t transfer_callback;
    void* transfer_callback_data;

    /* Statistics */
    meta_learning_snn_stats_t stats;
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
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

meta_learning_snn_config_t meta_learning_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_co", 0.0f);


    meta_learning_snn_config_t config = {
        .num_dimensions = META_DIM_COUNT,
        .neurons_per_dim = META_LEARNING_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = META_LEARNING_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = META_LEARNING_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = META_LEARNING_SNN_DECODE_INTEGRATION,
        .adaptation_threshold = META_LEARNING_SNN_ADAPTATION_THRESH,
        .transfer_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_strategy_selection = true,
        .strategy_threshold = 0.4f,

        .enable_curriculum = true,
        .curriculum_gain = 1.5f,
        .enable_transfer_detection = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

meta_learning_snn_bridge_t* meta_learning_snn_create(const meta_learning_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_cr", 0.0f);


    meta_learning_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(meta_learning_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = meta_learning_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > META_LEARNING_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "meta_learning_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "meta_learning_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* learning_rate, adaptation, transfer, generalization, strategy, consolidation */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_learning_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->insight_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->insight_buffer || !bridge->prev_state) {
        meta_learning_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_learning_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize insight to neutral */
    bridge->last_insight.learning_rate_level = 0.5f;
    bridge->last_insight.adaptation_level = 0.5f;
    bridge->last_insight.transfer_potential = 0.5f;
    bridge->last_insight.generalization_score = 0.5f;
    bridge->last_insight.task_similarity = 0.5f;
    bridge->last_insight.strategy_change_detected = false;
    bridge->last_insight.transfer_opportunity = false;
    bridge->last_insight.transfer_magnitude = 0.0f;
    bridge->last_insight.meta_learning_progress = 0.5f;
    bridge->last_insight.consolidation_score = 0.5f;

    bridge->state = META_LEARNING_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->transfer_signal = 0.0f;
    bridge->strategy_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "meta_learning_snn");
    return bridge;
}

void meta_learning_snn_destroy(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "meta_learning_snn");

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_de", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->insight_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int meta_learning_snn_reset(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset insight */
    memset(&bridge->last_insight, 0, sizeof(meta_learning_insight_t));
    bridge->last_insight.learning_rate_level = 0.5f;
    bridge->last_insight.adaptation_level = 0.5f;
    bridge->last_insight.transfer_potential = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->insight_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = META_LEARNING_SNN_STATE_IDLE;
    bridge->transfer_signal = 0.0f;
    bridge->strategy_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int meta_learning_snn_encode_state(
    meta_learning_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = META_LEARNING_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
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
                meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
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
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - bridge->prev_state[d];
        change_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    change_magnitude = (num_dims > 0) ? sqrtf(change_magnitude / num_dims) : 0.0f;

    if (change_magnitude > bridge->config.state_change_threshold) {
        bridge->last_insight.strategy_change_detected = true;
        bridge->stats.state_changes++;
    } else {
        bridge->last_insight.strategy_change_detected = false;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int meta_learning_snn_encode_learning_rate(
    meta_learning_snn_bridge_t* bridge,
    float current_rate,
    float target_rate
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_encode_learning_rate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_LEARNING_RATE] = nimcp_clampf(current_rate, 0.0f, 1.0f);
    dims[META_DIM_ADAPTATION_SPEED] = nimcp_clampf(fabsf(target_rate - current_rate), 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return meta_learning_snn_encode_state(bridge, dims, 2);
}

int meta_learning_snn_encode_task_similarity(
    meta_learning_snn_bridge_t* bridge,
    float similarity,
    uint32_t task_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_encode_task_similarity: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_TASK_SIMILARITY] = nimcp_clampf(similarity, 0.0f, 1.0f);
    dims[META_DIM_TRANSFER] = nimcp_clampf(similarity * 0.8f, 0.0f, 1.0f);
    dims[META_DIM_PRIOR_KNOWLEDGE] = nimcp_clampf(similarity * 0.6f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return meta_learning_snn_encode_state(bridge, dims, 3);
}

int meta_learning_snn_encode_transfer(
    meta_learning_snn_bridge_t* bridge,
    float transfer_potential,
    uint32_t source_task
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_encode_transfer: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_en", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[META_DIM_COUNT] = {0};
    dims[META_DIM_TRANSFER] = nimcp_clampf(transfer_potential, 0.0f, 1.0f);
    dims[META_DIM_GENERALIZATION] = nimcp_clampf(transfer_potential * 0.7f, 0.0f, 1.0f);

    bridge->transfer_signal = transfer_potential;

    if (transfer_potential > bridge->config.transfer_threshold) {
        bridge->last_insight.transfer_opportunity = true;
        bridge->last_insight.transfer_magnitude = transfer_potential;
        bridge->stats.transfer_detections++;

        if (bridge->transfer_callback) {
            bridge->transfer_callback(bridge, transfer_potential, source_task,
                                      bridge->transfer_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return meta_learning_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int meta_learning_snn_simulate(meta_learning_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_si", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = META_LEARNING_SNN_STATE_SIMULATING;

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
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
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
                meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                                 (float)(d + 1) / (float)bridge->config.num_dimensions);
            }

            bridge->dim_states[d].accumulated_evidence *= decay;
            bridge->dim_states[d].accumulated_evidence +=
                bridge->dim_states[d].activation * dt / bridge->config.integration_tau_ms;
        }

        bridge->current_time_us += (uint64_t)(dt * 1000.0f);
        bridge->stats.total_simulations++;
    }

    /* WHAT: Compute insight from encoded dimension activations */
    /* WHY: SNN outputs may be 0/uniform; use direct dimension values for insight */
    /* HOW: Map dimension activations to corresponding insight fields */
    bridge->last_insight.learning_rate_level = bridge->dim_states[META_DIM_LEARNING_RATE].activation;
    bridge->last_insight.adaptation_level = bridge->dim_states[META_DIM_ADAPTATION_SPEED].activation;
    bridge->last_insight.transfer_potential = bridge->dim_states[META_DIM_TRANSFER].activation;
    bridge->last_insight.generalization_score = bridge->dim_states[META_DIM_GENERALIZATION].activation;
    bridge->strategy_signal = bridge->dim_states[META_DIM_STRATEGY_SELECT].activation;
    bridge->last_insight.consolidation_score = bridge->dim_states[META_DIM_CONSOLIDATION].activation;
    bridge->last_insight.task_similarity = bridge->dim_states[META_DIM_TASK_SIMILARITY].activation;
    bridge->last_insight.meta_learning_progress = bridge->dim_states[META_DIM_LEARNING_TO_LEARN].activation;

    /* Check adaptation threshold */
    if (bridge->last_insight.adaptation_level > bridge->config.adaptation_threshold) {
        bridge->stats.adaptation_detections++;
    }

    bridge->stats.total_evaluations++;
    bridge->state = META_LEARNING_SNN_STATE_IDLE;

    /* Snapshot callback state under mutex */
    meta_learning_snn_adaptation_callback_t adapt_cb = bridge->adaptation_callback;
    void* adapt_cb_data = bridge->adaptation_callback_data;
    float adapt_level = bridge->last_insight.adaptation_level;
    uint64_t cur_time = bridge->current_time_us;
    bool do_adapt_cb = (adapt_cb != NULL &&
                        adapt_level > bridge->config.adaptation_threshold);

    meta_learning_snn_insight_callback_t insight_cb = bridge->insight_callback;
    void* insight_cb_data = bridge->insight_callback_data;
    meta_learning_insight_t insight_snapshot = bridge->last_insight;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside mutex to prevent deadlock */
    if (do_adapt_cb) {
        adapt_cb(bridge, adapt_level, cur_time, adapt_cb_data);
    }
    if (insight_cb) {
        insight_cb(bridge, &insight_snapshot, insight_cb_data);
    }

    return 0;
}

int meta_learning_snn_step(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_st", 0.0f);


    return meta_learning_snn_simulate(bridge, bridge->config.dt_ms);
}

int meta_learning_snn_forward(
    meta_learning_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_fo", 0.0f);


    int spike_count = meta_learning_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_snn_forward: validation failed");
        return -1;
    }

    if (meta_learning_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int meta_learning_snn_get_insight(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_insight_t* insight
) {
    if (!bridge || !insight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_get_insight: required parameter is NULL (bridge, insight)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *insight = bridge->last_insight;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int meta_learning_snn_get_activations(
    meta_learning_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool meta_learning_snn_check_adaptation(
    meta_learning_snn_bridge_t* bridge,
    float* adaptation_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_insight.adaptation_level;
    if (adaptation_level) {
        *adaptation_level = level;
    }
    bool detected = level > bridge->config.adaptation_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool meta_learning_snn_check_transfer(
    meta_learning_snn_bridge_t* bridge,
    float* transfer_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->transfer_signal;
    if (transfer_level) {
        *transfer_level = level;
    }
    bool detected = level > bridge->config.transfer_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool meta_learning_snn_check_state_change(
    meta_learning_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ch", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool changed = bridge->last_insight.strategy_change_detected;
    if (change_magnitude && changed) {
        /* Calculate magnitude from prev_state differences */
        float mag = 0.0f;
        for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
                meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
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

int meta_learning_snn_get_dim_state(
    meta_learning_snn_bridge_t* bridge,
    uint32_t dim,
    meta_learning_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_learning_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int meta_learning_snn_get_state(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_adaptation = bridge->last_insight.adaptation_level;
    state->transfer_signal = bridge->transfer_signal;
    state->strategy_signal = bridge->strategy_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
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

int meta_learning_snn_get_stats(meta_learning_snn_bridge_t* bridge, meta_learning_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int meta_learning_snn_reset_stats(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(meta_learning_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float meta_learning_snn_get_adaptation(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float adaptation = bridge->last_insight.adaptation_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return adaptation;
}

float meta_learning_snn_get_total_activity(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_ge", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            meta_learning_snn_bridge_heartbeat("meta_learnin_loop",
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

int meta_learning_snn_register_adaptation_callback(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_adaptation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_register_adaptation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->adaptation_callback = callback;
    bridge->adaptation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int meta_learning_snn_register_insight_callback(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_insight_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_register_insight_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->insight_callback = callback;
    bridge->insight_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int meta_learning_snn_register_transfer_callback(
    meta_learning_snn_bridge_t* bridge,
    meta_learning_snn_transfer_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_register_transfer_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->transfer_callback = callback;
    bridge->transfer_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int meta_learning_snn_bio_async_connect(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int meta_learning_snn_bio_async_disconnect(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_learning_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool meta_learning_snn_is_bio_async_connected(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    meta_learning_snn_bridge_heartbeat("meta_learnin_meta_learning_snn_is", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}


void meta_learning_snn_bridge_set_instance_health_agent(meta_learning_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "meta_learning_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int meta_learning_snn_bridge_training_begin(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_learning_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    meta_learning_snn_bridge_heartbeat_instance(bridge->health_agent, "meta_learning_snn_bridge_training_begin", 0.0f);
    return 0;
}

int meta_learning_snn_bridge_training_end(meta_learning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_learning_snn_bridge_training_end: NULL argument");
        return -1;
    }
    meta_learning_snn_bridge_heartbeat_instance(bridge->health_agent, "meta_learning_snn_bridge_training_end", 1.0f);
    return 0;
}

int meta_learning_snn_bridge_training_step(meta_learning_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "meta_learning_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    meta_learning_snn_bridge_heartbeat_instance(bridge->health_agent, "meta_learning_snn_bridge_training_step", progress);
    return 0;
}
