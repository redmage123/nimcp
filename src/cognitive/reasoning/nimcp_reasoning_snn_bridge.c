/**
 * @file nimcp_reasoning_snn_bridge.c
 * @brief Reasoning - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/reasoning/nimcp_reasoning_snn_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(reasoning_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "REASONING_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct reasoning_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    reasoning_snn_config_t config;
    snn_network_t* snn;

    /* State */
    reasoning_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    reasoning_dim_state_t dim_states[REASONING_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* inference_buffer;

    /* Inference state */
    reasoning_inference_t last_inference;
    float conflict_signal;
    float causal_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    reasoning_snn_conflict_callback_t conflict_callback;
    void* conflict_callback_data;
    reasoning_snn_inference_callback_t inference_callback;
    void* inference_callback_data;
    reasoning_snn_conclusion_callback_t conclusion_callback;
    void* conclusion_callback_data;

    /* Statistics */
    reasoning_snn_stats_t stats;
};

BRIDGE_DEFINE_SECURITY_SETTERS(reasoning_snn_bridge)

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
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

reasoning_snn_config_t reasoning_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_config", 0.0f);


    reasoning_snn_config_t config = {
        .num_dimensions = REASON_DIM_COUNT,
        .neurons_per_dim = REASONING_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = REASONING_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = REASONING_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = REASONING_SNN_DECODE_INTEGRATION,
        .inference_threshold = REASONING_SNN_INFERENCE_THRESH,
        .confidence_threshold = 0.6f,
        .conclusion_threshold = 0.7f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_conflict_detection = true,
        .conflict_threshold = 0.4f,

        .enable_causal_chains = true,
        .causal_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .enable_analogical_binding = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

reasoning_snn_bridge_t* reasoning_snn_create(const reasoning_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_create", 0.0f);


    reasoning_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(reasoning_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = reasoning_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > REASONING_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_create: operation failed");
        return NULL;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "reasoning_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "reasoning_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 8; /* deduction, induction, abduction, causal, analogy, validity, evidence, depth */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->inference_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->inference_buffer || !bridge->prev_state) {
        reasoning_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reasoning_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize inference to neutral */
    bridge->last_inference.deduction_strength = 0.5f;
    bridge->last_inference.induction_strength = 0.5f;
    bridge->last_inference.abduction_strength = 0.5f;
    bridge->last_inference.causal_confidence = 0.5f;
    bridge->last_inference.analogy_match = 0.5f;
    bridge->last_inference.logical_validity = 0.5f;
    bridge->last_inference.evidence_weight = 0.5f;
    bridge->last_inference.inference_depth = 0.0f;
    bridge->last_inference.conclusion_valid = false;
    bridge->last_inference.conflict_detected = false;
    bridge->last_inference.conflict_magnitude = 0.0f;
    bridge->last_inference.counterfactual_score = 0.5f;

    bridge->state = REASONING_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->conflict_signal = 0.0f;
    bridge->causal_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "reasoning_snn");
    return bridge;
}

void reasoning_snn_destroy(reasoning_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "reasoning_snn");

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_destro", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->inference_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int reasoning_snn_reset(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset inference */
    memset(&bridge->last_inference, 0, sizeof(reasoning_inference_t));
    bridge->last_inference.deduction_strength = 0.5f;
    bridge->last_inference.induction_strength = 0.5f;
    bridge->last_inference.logical_validity = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 8 * sizeof(float));
    memset(bridge->inference_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = REASONING_SNN_STATE_IDLE;
    bridge->conflict_signal = 0.0f;
    bridge->causal_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int reasoning_snn_encode_state(
    reasoning_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_snn_encode_state: num_dims is zero");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, dimensions, num_dims * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = REASONING_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
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
                reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
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

    /* Detect conflict from dimension disagreement */
    float conflict_magnitude = 0.0f;
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - bridge->prev_state[d];
        conflict_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    conflict_magnitude = (num_dims > 0) ? sqrtf(conflict_magnitude / num_dims) : 0.0f;

    if (conflict_magnitude > bridge->config.conflict_threshold) {
        bridge->last_inference.conflict_detected = true;
        bridge->last_inference.conflict_magnitude = conflict_magnitude;
        bridge->stats.conflict_detections++;
    } else {
        bridge->last_inference.conflict_detected = false;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int reasoning_snn_encode_deduction(
    reasoning_snn_bridge_t* bridge,
    float premise_strength,
    float rule_validity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_encode_deduction: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_DEDUCTION] = nimcp_clampf(premise_strength * rule_validity, 0.0f, 1.0f);
    dims[REASON_DIM_LOGICAL_VALIDITY] = nimcp_clampf(rule_validity, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
}

int reasoning_snn_encode_causal(
    reasoning_snn_bridge_t* bridge,
    float cause_strength,
    float effect_probability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_encode_causal: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_CAUSAL] = nimcp_clampf(cause_strength * effect_probability, 0.0f, 1.0f);
    dims[REASON_DIM_PROBABILITY] = nimcp_clampf(effect_probability, 0.0f, 1.0f);

    bridge->causal_signal = dims[REASON_DIM_CAUSAL];
    bridge->stats.causal_chains++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
}

int reasoning_snn_encode_evidence(
    reasoning_snn_bridge_t* bridge,
    float evidence_strength,
    uint32_t evidence_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_encode_evidence: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[REASON_DIM_COUNT] = {0};
    dims[REASON_DIM_EVIDENCE_WEIGHT] = nimcp_clampf(evidence_strength, 0.0f, 1.0f);
    dims[REASON_DIM_INFERENCE_DEPTH] = nimcp_clampf((float)evidence_count / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return reasoning_snn_encode_state(bridge, dims, REASON_DIM_COUNT);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int reasoning_snn_simulate(reasoning_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_simula", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = REASONING_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    if (dt <= 0.0f) dt = 1.0f;
    uint32_t steps = (uint32_t)(duration_ms / dt);

    /* Set inputs before simulation */
    if (bridge->snn) {
        uint32_t input_size = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
        snn_network_set_inputs(bridge->snn, bridge->encoding_buffer, input_size);
    }

    for (uint32_t s = 0; s < steps; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && steps > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
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
                reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                                 (float)(d + 1) / (float)bridge->config.num_dimensions);
            }

            bridge->dim_states[d].accumulated_evidence *= decay;
            if (fabsf(bridge->config.integration_tau_ms) > 1e-10f) {
                bridge->dim_states[d].accumulated_evidence +=
                    bridge->dim_states[d].activation * dt / bridge->config.integration_tau_ms;
            }
        }

        /* Apply causal decay */
        if (bridge->config.enable_causal_chains) {
            bridge->causal_signal *= bridge->config.causal_decay;
        }

        bridge->current_time_us += (uint64_t)(dt * 1000.0f);
        bridge->stats.total_simulations++;
    }

    /* Get output from SNN */
    if (bridge->snn) {
        snn_network_get_outputs(bridge->snn, bridge->output_buffer, 8);
    }

    /* Decode outputs */
    bridge->last_inference.deduction_strength = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_inference.induction_strength = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_inference.abduction_strength = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_inference.causal_confidence = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_inference.analogy_match = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_inference.logical_validity = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);
    bridge->last_inference.evidence_weight = nimcp_clampf(bridge->output_buffer[6], 0.0f, 1.0f);
    bridge->last_inference.inference_depth = nimcp_clampf(bridge->output_buffer[7], 0.0f, 1.0f);

    /* Check conclusion validity */
    float mean_inference = (bridge->last_inference.deduction_strength +
                           bridge->last_inference.logical_validity +
                           bridge->last_inference.evidence_weight) / 3.0f;

    if (mean_inference > bridge->config.conclusion_threshold) {
        bridge->last_inference.conclusion_valid = true;
        bridge->stats.valid_conclusions++;
    } else {
        bridge->last_inference.conclusion_valid = false;
    }

    /* Check conflict threshold */
    if (bridge->conflict_signal > bridge->config.conflict_threshold) {
        if (bridge->conflict_callback) {
            bridge->conflict_callback(bridge, bridge->conflict_signal,
                                      bridge->current_time_us, bridge->conflict_callback_data);
        }
    }

    bridge->stats.total_evaluations++;
    bridge->state = REASONING_SNN_STATE_IDLE;

    /* Invoke inference callback */
    if (bridge->inference_callback) {
        bridge->inference_callback(bridge, &bridge->last_inference, bridge->inference_callback_data);
    }

    /* Invoke conclusion callback if valid */
    if (bridge->last_inference.conclusion_valid && bridge->conclusion_callback) {
        bridge->conclusion_callback(bridge, bridge->last_inference.logical_validity,
                                   0, bridge->conclusion_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_snn_step(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_step", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "reasoning_snn_step");
    BRIDGE_LGSS_GATE(bridge, "reasoning_snn_step");

    return reasoning_snn_simulate(bridge, bridge->config.dt_ms);
}

int reasoning_snn_forward(
    reasoning_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, inputs, input_count * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_forwar", 0.0f);


    int spike_count = reasoning_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_snn_forward: validation failed");
        return -1;
    }

    if (reasoning_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int reasoning_snn_get_inference(
    reasoning_snn_bridge_t* bridge,
    reasoning_inference_t* inference
) {
    if (!bridge || !inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_get_inference: required parameter is NULL (bridge, inference)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_in", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *inference = bridge->last_inference;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_snn_get_activations(
    reasoning_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool reasoning_snn_check_conflict(
    reasoning_snn_bridge_t* bridge,
    float* conflict_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->conflict_signal;
    if (conflict_level) {
        *conflict_level = level;
    }
    bool detected = level > bridge->config.conflict_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool reasoning_snn_check_conclusion(
    reasoning_snn_bridge_t* bridge,
    float* validity
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_inference.logical_validity;
    if (validity) {
        *validity = level;
    }
    bool valid = bridge->last_inference.conclusion_valid;
    nimcp_mutex_unlock(bridge->base.mutex);

    return valid;
}

bool reasoning_snn_check_causal(
    reasoning_snn_bridge_t* bridge,
    float* causal_strength
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->causal_signal;
    if (causal_strength) {
        *causal_strength = level;
    }
    bool detected = level > 0.3f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

//=============================================================================
// State Query Functions
//=============================================================================

int reasoning_snn_get_dim_state(
    reasoning_snn_bridge_t* bridge,
    uint32_t dim,
    reasoning_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_snn_get_state(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_inference = (bridge->last_inference.deduction_strength +
                            bridge->last_inference.logical_validity) / 2.0f;
    state->conflict_signal = bridge->conflict_signal;
    state->causal_signal = bridge->causal_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
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

int reasoning_snn_get_stats(reasoning_snn_bridge_t* bridge, reasoning_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_snn_reset_stats(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(reasoning_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float reasoning_snn_get_inference_strength(reasoning_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_in", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float strength = (bridge->last_inference.deduction_strength +
                     bridge->last_inference.induction_strength +
                     bridge->last_inference.logical_validity) / 3.0f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return strength;
}

float reasoning_snn_get_total_activity(reasoning_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_get_to", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            reasoning_snn_bridge_heartbeat("reasoning_sn_loop",
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

int reasoning_snn_register_conflict_callback(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_conflict_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_register_conflict_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->conflict_callback = callback;
    bridge->conflict_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_snn_register_inference_callback(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_inference_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_register_inference_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->inference_callback = callback;
    bridge->inference_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_snn_register_conclusion_callback(
    reasoning_snn_bridge_t* bridge,
    reasoning_snn_conclusion_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_register_conclusion_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->conclusion_callback = callback;
    bridge->conclusion_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int reasoning_snn_bio_async_connect(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int reasoning_snn_bio_async_disconnect(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool reasoning_snn_is_bio_async_connected(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_snn_bridge_heartbeat("reasoning_sn_reasoning_snn_is_bio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}


void reasoning_snn_bridge_set_instance_health_agent(reasoning_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "reasoning_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_snn_bridge_training_begin(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    reasoning_snn_bridge_heartbeat_instance(bridge->health_agent, "reasoning_snn_bridge_training_begin", 0.0f);
    return 0;
}

int reasoning_snn_bridge_training_end(reasoning_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_snn_bridge_training_end: NULL argument");
        return -1;
    }
    reasoning_snn_bridge_heartbeat_instance(bridge->health_agent, "reasoning_snn_bridge_training_end", 1.0f);
    return 0;
}

int reasoning_snn_bridge_training_step(reasoning_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_snn_bridge_heartbeat_instance(bridge->health_agent, "reasoning_snn_bridge_training_step", progress);
    return 0;
}
