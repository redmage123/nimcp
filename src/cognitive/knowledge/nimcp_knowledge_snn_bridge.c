/**
 * @file nimcp_knowledge_snn_bridge.c
 * @brief Knowledge - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/knowledge/nimcp_knowledge_snn_bridge.h"
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
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(knowledge_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "KNOWLEDGE_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct knowledge_snn_bridge {
    bridge_base_t base;
    knowledge_snn_config_t config;
    snn_network_t* snn;

    /* State */
    knowledge_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    knowledge_dim_state_t dim_states[KNOWLEDGE_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* retrieval_buffer;

    /* Retrieval state */
    knowledge_retrieval_t last_retrieval;
    float semantic_signal;
    float retrieval_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    knowledge_snn_activation_callback_t activation_callback;
    void* activation_callback_data;
    knowledge_snn_retrieval_callback_t retrieval_callback;
    void* retrieval_callback_data;
    knowledge_snn_association_callback_t association_callback;
    void* association_callback_data;

    /* Statistics */
    knowledge_snn_stats_t stats;

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
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

knowledge_snn_config_t knowledge_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_config", 0.0f);


    knowledge_snn_config_t config = {
        .num_dimensions = KNOWLEDGE_DIM_COUNT,
        .neurons_per_dim = KNOWLEDGE_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = KNOWLEDGE_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = KNOWLEDGE_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = KNOWLEDGE_SNN_DECODE_INTEGRATION,
        .activation_threshold = KNOWLEDGE_SNN_ACTIVATION_THRESH,
        .retrieval_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_spreading_activation = true,
        .spreading_decay = NIMCP_EMA_DECAY_FAST,

        .enable_retrieval = true,
        .retrieval_gain = 1.5f,
        .enable_association = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

knowledge_snn_bridge_t* knowledge_snn_create(const knowledge_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_create", 0.0f);


    knowledge_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(knowledge_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_snn_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = knowledge_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > KNOWLEDGE_SNN_MAX_DIMENSIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_create: invalid num_dimensions");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "knowledge_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_snn_create: bridge_base_init failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* semantic, activation, retrieval, association, categorical, integration */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_snn_create: failed to create SNN network");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->retrieval_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->retrieval_buffer || !bridge->prev_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_snn_create: failed to allocate buffers");
        knowledge_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize retrieval to neutral */
    bridge->last_retrieval.semantic_level = 0.5f;
    bridge->last_retrieval.activation_level = 0.5f;
    bridge->last_retrieval.retrieval_strength = 0.5f;
    bridge->last_retrieval.association_strength = 0.5f;
    bridge->last_retrieval.categorical_coherence = 0.5f;
    bridge->last_retrieval.concept_activated = false;
    bridge->last_retrieval.retrieval_success = false;
    bridge->last_retrieval.retrieval_magnitude = 0.0f;
    bridge->last_retrieval.integration_level = 0.5f;
    bridge->last_retrieval.confidence_level = 0.5f;

    bridge->state = KNOWLEDGE_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->semantic_signal = 0.0f;
    bridge->retrieval_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "knowledge_snn");
    return bridge;
}

void knowledge_snn_destroy(knowledge_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "knowledge_snn");

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_destro", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->retrieval_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int knowledge_snn_reset(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset retrieval */
    memset(&bridge->last_retrieval, 0, sizeof(knowledge_retrieval_t));
    bridge->last_retrieval.semantic_level = 0.5f;
    bridge->last_retrieval.activation_level = 0.5f;
    bridge->last_retrieval.retrieval_strength = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->retrieval_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = KNOWLEDGE_SNN_STATE_IDLE;
    bridge->semantic_signal = 0.0f;
    bridge->retrieval_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int knowledge_snn_encode_state(
    knowledge_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = KNOWLEDGE_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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
                knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
                                 (float)(n + 1) / (float)neurons_per_dim);
            }

            float preferred = (neurons_per_dim > 1) ? ((float)n / (float)(neurons_per_dim - 1)) : 0.0f;
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
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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

int knowledge_snn_encode_semantic(
    knowledge_snn_bridge_t* bridge,
    float semantic,
    float activation
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_encode_semantic: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[KNOWLEDGE_DIM_COUNT] = {0};
    dims[KNOWLEDGE_DIM_SEMANTIC] = nimcp_clampf(semantic, 0.0f, 1.0f);
    dims[KNOWLEDGE_DIM_ACTIVATION] = nimcp_clampf(activation, 0.0f, 1.0f);
    dims[KNOWLEDGE_DIM_RETRIEVAL] = (semantic + activation) / 2.0f;

    bridge->semantic_signal = semantic;

    nimcp_mutex_unlock(bridge->base.mutex);

    return knowledge_snn_encode_state(bridge, dims, 3);
}

int knowledge_snn_encode_retrieval(
    knowledge_snn_bridge_t* bridge,
    float retrieval_strength,
    uint32_t concept_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_encode_retrieval: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[KNOWLEDGE_DIM_COUNT] = {0};
    dims[KNOWLEDGE_DIM_RETRIEVAL] = nimcp_clampf(retrieval_strength, 0.0f, 1.0f);
    dims[KNOWLEDGE_DIM_CATEGORICAL] = nimcp_clampf((float)concept_count / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return knowledge_snn_encode_state(bridge, dims, 2);
}

int knowledge_snn_encode_association(
    knowledge_snn_bridge_t* bridge,
    float association,
    uint32_t association_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_encode_association: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[KNOWLEDGE_DIM_COUNT] = {0};
    dims[KNOWLEDGE_DIM_ASSOCIATION] = nimcp_clampf(association, 0.0f, 1.0f);
    dims[KNOWLEDGE_DIM_INTEGRATION] = nimcp_clampf(association * 0.8f, 0.0f, 1.0f);

    bridge->retrieval_signal = association;

    if (association > bridge->config.activation_threshold) {
        bridge->last_retrieval.retrieval_success = true;
        bridge->last_retrieval.retrieval_magnitude = association;
        bridge->stats.retrieval_successes++;

        if (bridge->association_callback) {
            bridge->association_callback(bridge, association, association_type,
                                        bridge->association_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return knowledge_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int knowledge_snn_simulate(knowledge_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_simula", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = KNOWLEDGE_SNN_STATE_SIMULATING;

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
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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
                knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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
    bridge->last_retrieval.semantic_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_retrieval.activation_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_retrieval.retrieval_strength = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_retrieval.association_strength = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_retrieval.categorical_coherence = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_retrieval.integration_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check activation threshold */
    if (bridge->last_retrieval.activation_level > bridge->config.activation_threshold) {
        bridge->last_retrieval.concept_activated = true;
        bridge->stats.concept_activations++;

        if (bridge->activation_callback) {
            bridge->activation_callback(bridge, bridge->last_retrieval.activation_level,
                                       bridge->current_time_us, bridge->activation_callback_data);
        }
    } else {
        bridge->last_retrieval.concept_activated = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = KNOWLEDGE_SNN_STATE_IDLE;

    /* Invoke retrieval callback */
    if (bridge->retrieval_callback) {
        bridge->retrieval_callback(bridge, &bridge->last_retrieval, bridge->retrieval_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_snn_step(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_step", 0.0f);


    return knowledge_snn_simulate(bridge, bridge->config.dt_ms);
}

int knowledge_snn_forward(
    knowledge_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_forwar", 0.0f);


    int spike_count = knowledge_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_forward: validation failed");
        return -1;
    }

    if (knowledge_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int knowledge_snn_get_retrieval(
    knowledge_snn_bridge_t* bridge,
    knowledge_retrieval_t* retrieval
) {
    if (!bridge || !retrieval) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_get_retrieval: required parameter is NULL (bridge, retrieval)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *retrieval = bridge->last_retrieval;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_snn_get_activations(
    knowledge_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool knowledge_snn_check_activation(
    knowledge_snn_bridge_t* bridge,
    float* activation_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_retrieval.activation_level;
    if (activation_level) {
        *activation_level = level;
    }
    bool activated = level > bridge->config.activation_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return activated;
}

bool knowledge_snn_check_retrieval(
    knowledge_snn_bridge_t* bridge,
    float* retrieval_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_retrieval.retrieval_strength;
    if (retrieval_level) {
        *retrieval_level = level;
    }
    bool success = level > bridge->config.retrieval_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return success;
}

bool knowledge_snn_check_state_change(
    knowledge_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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

int knowledge_snn_get_dim_state(
    knowledge_snn_bridge_t* bridge,
    uint32_t dim,
    knowledge_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_snn_get_state(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_activation = bridge->last_retrieval.activation_level;
    state->semantic_signal = bridge->semantic_signal;
    state->retrieval_signal = bridge->retrieval_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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

int knowledge_snn_get_stats(knowledge_snn_bridge_t* bridge, knowledge_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_snn_reset_stats(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(knowledge_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float knowledge_snn_get_activation(knowledge_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float activation = bridge->last_retrieval.activation_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return activation;
}

float knowledge_snn_get_total_activity(knowledge_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_get_to", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            knowledge_snn_bridge_heartbeat("knowledge_sn_loop",
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

int knowledge_snn_register_activation_callback(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_activation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_register_activation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->activation_callback = callback;
    bridge->activation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_snn_register_retrieval_callback(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_retrieval_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_register_retrieval_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->retrieval_callback = callback;
    bridge->retrieval_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_snn_register_association_callback(
    knowledge_snn_bridge_t* bridge,
    knowledge_snn_association_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_register_association_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->association_callback = callback;
    bridge->association_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int knowledge_snn_bio_async_connect(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int knowledge_snn_bio_async_disconnect(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool knowledge_snn_is_bio_async_connected(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_snn_bridge_heartbeat("knowledge_sn_knowledge_snn_is_bio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_snn_bridge_set_instance_health_agent(knowledge_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "knowledge_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_snn_bridge_training_begin(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    knowledge_snn_bridge_heartbeat_instance(bridge->health_agent, "knowledge_snn_bridge_training_begin", 0.0f);
    return 0;
}

int knowledge_snn_bridge_training_end(knowledge_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_snn_bridge_training_end: NULL argument");
        return -1;
    }
    knowledge_snn_bridge_heartbeat_instance(bridge->health_agent, "knowledge_snn_bridge_training_end", 1.0f);
    return 0;
}

int knowledge_snn_bridge_training_step(knowledge_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_snn_bridge_heartbeat_instance(bridge->health_agent, "knowledge_snn_bridge_training_step", progress);
    return 0;
}
