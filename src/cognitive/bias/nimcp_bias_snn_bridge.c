/**
 * @file nimcp_bias_snn_bridge.c
 * @brief Cognitive Bias - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Implementation of SNN bridge for cognitive bias detection
 * WHY:  Enable biologically-plausible bias detection through spike-based processing
 * HOW:  Encode decision contexts as spike patterns, decode bias detection from population activity
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/bias/nimcp_bias_snn_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_neural_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bias_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_bias_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_bias_snn_bridge_mesh_registry = NULL;

nimcp_error_t bias_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_bias_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "bias_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "bias_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_bias_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_bias_snn_bridge_mesh_registry = registry;
    return err;
}

void bias_snn_bridge_mesh_unregister(void) {
    if (g_bias_snn_bridge_mesh_registry && g_bias_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_bias_snn_bridge_mesh_registry, g_bias_snn_bridge_mesh_id);
        g_bias_snn_bridge_mesh_id = 0;
        g_bias_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from bias_snn_bridge module (instance-level) */
static inline void bias_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_bias_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_bias_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_bias_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "BIAS_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    float membrane_potential;
    float threshold;
    float spike_rate;
    uint32_t spike_count;
    float refractory_remaining;
    float input_current;
    float adaptation;
} bias_neuron_t;

struct bias_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    bias_snn_config_t config;
    bias_snn_state_t state;

    // Neuron populations per bias type
    bias_neuron_t** bias_neurons;      // [type][neuron]
    uint32_t num_types;

    // Conflict detection neurons
    bias_neuron_t* conflict_neurons;
    uint32_t num_conflict_neurons;

    // Output neurons
    bias_neuron_t* output_neurons;
    uint32_t num_output_neurons;

    // Current input state
    float anchor_value;
    float recent_weight;
    float emotional_valence;
    float prior_belief;
    float prediction_error;

    // Per-type activation levels
    float* type_activations;
    float* type_confidences;

    // Output state
    bias_snn_output_t last_output;

    // Statistics
    bias_snn_stats_t stats;

    // Callbacks
    bias_snn_detection_callback_t detection_callback;
    void* detection_callback_data;
    bias_snn_conflict_callback_t conflict_callback;
    void* conflict_callback_data;

    // Bio-async
    bool bio_async_connected;

    // Simulation time
    uint64_t sim_time_us;
};

static void reset_neuron(bias_neuron_t* neuron) {
    neuron->membrane_potential = 0.0f;
    neuron->threshold = 1.0f;
    neuron->spike_rate = 0.0f;
    neuron->spike_count = 0;
    neuron->refractory_remaining = 0.0f;
    neuron->input_current = 0.0f;
    neuron->adaptation = 0.0f;
}

static bool neuron_step(bias_neuron_t* neuron, float dt_ms, float input) {
    const float tau_membrane = NIMCP_MEMBRANE_TAU_MS;
    const float tau_adaptation = 100.0f;
    const float refractory_period = NIMCP_REFRACTORY_PERIOD_MS;
    const float adaptation_strength = 0.1f;

    if (neuron->refractory_remaining > 0.0f) {
        neuron->refractory_remaining -= dt_ms;
        return false;
    }

    // Leaky integrate with adaptation
    float decay = expf(-dt_ms / tau_membrane);
    float effective_input = input - neuron->adaptation;
    neuron->membrane_potential = neuron->membrane_potential * decay + effective_input * (1.0f - decay);

    // Adaptation decay
    float adapt_decay = expf(-dt_ms / tau_adaptation);
    neuron->adaptation *= adapt_decay;

    // Check for spike
    if (neuron->membrane_potential >= neuron->threshold) {
        neuron->membrane_potential = 0.0f;
        neuron->refractory_remaining = refractory_period;
        neuron->spike_count++;
        neuron->adaptation += adaptation_strength;
        return true;
    }

    return false;
}

static const char* bias_type_names[] = {
    "confirmation",
    "availability",
    "anchoring",
    "recency",
    "optimism",
    "pessimism",
    "framing",
    "sunk_cost"
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

bias_snn_config_t bias_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_config_defa", 0.0f);


    bias_snn_config_t config = {
        .max_bias_types = BIAS_SNN_MAX_BIAS_TYPES,
        .neurons_per_type = BIAS_SNN_NEURONS_PER_TYPE,
        .input_dim = BIAS_SNN_INPUT_DIM,
        .hidden_dim = BIAS_SNN_HIDDEN_DIM,
        .dt_ms = 1.0f,
        .bias_detection_threshold = 0.6f,
        .conflict_threshold = NIMCP_CONFLICT_THRESHOLD,
        .baseline_activation = 0.1f,
        .encoding_type = BIAS_SNN_ENCODE_RATE,
        .enable_conflict_detection = true,
        .enable_metacognitive_monitoring = true,
        .enable_bio_async = false
    };
    return config;
}

bias_snn_bridge_t* bias_snn_create(const bias_snn_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_create", 0.0f);


    bias_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(bias_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->config = *config;
    bridge->state = BIAS_SNN_STATE_IDLE;
    bridge->num_types = BIAS_SNN_TYPE_COUNT;

    // Allocate neurons per bias type
    bridge->bias_neurons = nimcp_calloc(bridge->num_types, sizeof(bias_neuron_t*));
    if (!bridge->bias_neurons) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_snn_create: bridge->bias_neurons is NULL");
        return NULL;
    }

    for (uint32_t t = 0; t < bridge->num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->num_types > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)bridge->num_types);
        }

        bridge->bias_neurons[t] = nimcp_calloc(config->neurons_per_type, sizeof(bias_neuron_t));
        if (!bridge->bias_neurons[t]) {
            for (uint32_t i = 0; i < t; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && t > 256) {
                    bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                     (float)(i + 1) / (float)t);
                }

                nimcp_free(bridge->bias_neurons[i]);
            }
            nimcp_free(bridge->bias_neurons);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bias_snn_create: validation failed");
            return NULL;
        }
        for (uint32_t n = 0; n < config->neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && config->neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)config->neurons_per_type);
            }

            reset_neuron(&bridge->bias_neurons[t][n]);
        }
    }

    // Allocate conflict detection neurons
    bridge->num_conflict_neurons = config->neurons_per_type;
    bridge->conflict_neurons = nimcp_calloc(bridge->num_conflict_neurons, sizeof(bias_neuron_t));
    if (!bridge->conflict_neurons) {
        bias_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_snn_create: bridge->conflict_neurons is NULL");
        return NULL;
    }
    for (uint32_t n = 0; n < bridge->num_conflict_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_conflict_neurons > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->num_conflict_neurons);
        }

        reset_neuron(&bridge->conflict_neurons[n]);
    }

    // Allocate output neurons
    bridge->num_output_neurons = config->neurons_per_type * 2;
    bridge->output_neurons = nimcp_calloc(bridge->num_output_neurons, sizeof(bias_neuron_t));
    if (!bridge->output_neurons) {
        bias_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_snn_create: bridge->output_neurons is NULL");
        return NULL;
    }
    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        reset_neuron(&bridge->output_neurons[n]);
    }

    // Allocate activation arrays
    bridge->type_activations = nimcp_calloc(bridge->num_types, sizeof(float));
    bridge->type_confidences = nimcp_calloc(bridge->num_types, sizeof(float));
    if (!bridge->type_activations || !bridge->type_confidences) {
        bias_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bias_snn_create: required parameter is NULL (bridge->type_activations, bridge->type_confidences)");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "bias_snn");
    return bridge;
}

void bias_snn_destroy(bias_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "bias_snn");

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_destroy", 0.0f);


    if (bridge->bias_neurons) {
        for (uint32_t t = 0; t < bridge->num_types; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && bridge->num_types > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(t + 1) / (float)bridge->num_types);
            }

            nimcp_free(bridge->bias_neurons[t]);
        }
        nimcp_free(bridge->bias_neurons);
    }
    nimcp_free(bridge->conflict_neurons);
    nimcp_free(bridge->output_neurons);
    nimcp_free(bridge->type_activations);
    nimcp_free(bridge->type_confidences);
    nimcp_free(bridge);
    bridge = NULL;
}

int bias_snn_reset(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_reset", 0.0f);


    bridge->state = BIAS_SNN_STATE_IDLE;
    bridge->sim_time_us = 0;

    // Reset all neurons
    for (uint32_t t = 0; t < bridge->num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->num_types > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)bridge->num_types);
        }

        for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_type);
            }

            reset_neuron(&bridge->bias_neurons[t][n]);
        }
    }
    for (uint32_t n = 0; n < bridge->num_conflict_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_conflict_neurons > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->num_conflict_neurons);
        }

        reset_neuron(&bridge->conflict_neurons[n]);
    }
    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        reset_neuron(&bridge->output_neurons[n]);
    }

    // Reset activations
    memset(bridge->type_activations, 0, bridge->num_types * sizeof(float));
    memset(bridge->type_confidences, 0, bridge->num_types * sizeof(float));

    // Reset inputs
    bridge->anchor_value = 0.0f;
    bridge->recent_weight = 0.0f;
    bridge->emotional_valence = 0.0f;
    bridge->prior_belief = 0.5f;
    bridge->prediction_error = 0.0f;

    // Reset output
    memset(&bridge->last_output, 0, sizeof(bias_snn_output_t));

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int bias_snn_encode_evidence(
    bias_snn_bridge_t* bridge,
    const float* evidence,
    uint32_t evidence_count,
    float prior_belief
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_encode_evidence: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_encode_evid", 0.0f);


    bridge->state = BIAS_SNN_STATE_ENCODING;
    bridge->prior_belief = nimcp_clampf(prior_belief, 0.0f, 1.0f);

    // Compute evidence statistics for bias detection
    float evidence_mean = 0.0f;
    float evidence_var = 0.0f;

    if (evidence && evidence_count > 0) {
        for (uint32_t i = 0; i < evidence_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && evidence_count > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(i + 1) / (float)evidence_count);
            }

            evidence_mean += evidence[i];
        }
        evidence_mean /= evidence_count;

        for (uint32_t i = 0; i < evidence_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && evidence_count > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(i + 1) / (float)evidence_count);
            }

            float diff = evidence[i] - evidence_mean;
            evidence_var += diff * diff;
        }
        evidence_var /= evidence_count;
    }

    // Confirmation bias detection: evidence aligned with prior
    float confirmation_signal = fabsf(evidence_mean - prior_belief) < 0.2f ?
                                prior_belief * 1.5f : 0.0f;
    for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_type);
        }

        bridge->bias_neurons[BIAS_SNN_TYPE_CONFIRMATION][n].input_current = confirmation_signal;
    }

    bridge->stats.total_detections++;
    return 0;
}

int bias_snn_encode_decision_context(
    bias_snn_bridge_t* bridge,
    float anchor_value,
    float recent_evidence_weight,
    float emotional_valence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_encode_decision_context: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_encode_deci", 0.0f);


    bridge->state = BIAS_SNN_STATE_ENCODING;
    bridge->anchor_value = anchor_value;
    bridge->recent_weight = nimcp_clampf(recent_evidence_weight, 0.0f, 1.0f);
    bridge->emotional_valence = nimcp_clampf(emotional_valence, -1.0f, 1.0f);

    // Anchoring bias: strong anchor effect
    float anchoring_signal = fabsf(anchor_value) * 2.0f;
    for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_type);
        }

        bridge->bias_neurons[BIAS_SNN_TYPE_ANCHORING][n].input_current = anchoring_signal;
    }

    // Recency bias: over-weighting recent evidence
    float recency_signal = recent_evidence_weight > 0.7f ? recent_evidence_weight * 2.0f : 0.0f;
    for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_type);
        }

        bridge->bias_neurons[BIAS_SNN_TYPE_RECENCY][n].input_current = recency_signal;
    }

    // Optimism/Pessimism bias: based on emotional valence
    if (emotional_valence > 0.3f) {
        for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_type);
            }

            bridge->bias_neurons[BIAS_SNN_TYPE_OPTIMISM][n].input_current = emotional_valence * 2.0f;
        }
    } else if (emotional_valence < -0.3f) {
        for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_type);
            }

            bridge->bias_neurons[BIAS_SNN_TYPE_PESSIMISM][n].input_current = -emotional_valence * 2.0f;
        }
    }

    return 0;
}

int bias_snn_encode_prediction_error(
    bias_snn_bridge_t* bridge,
    float prediction_error,
    float prediction_confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_encode_prediction_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_encode_pred", 0.0f);


    bridge->prediction_error = prediction_error;

    // Large prediction errors with high confidence may indicate bias
    float pe_magnitude = fabsf(prediction_error);
    if (pe_magnitude > 0.5f && prediction_confidence > 0.7f) {
        // Availability bias: systematic PE in one direction
        float availability_signal = pe_magnitude * prediction_confidence;
        for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_type);
            }

            bridge->bias_neurons[BIAS_SNN_TYPE_AVAILABILITY][n].input_current = availability_signal;
        }
    }

    return 0;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int bias_snn_simulate(bias_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_simulate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_simulate", 0.0f);


    bridge->state = BIAS_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    int steps = (int)(duration_ms / (fabsf(dt) > 1e-7f ? dt : 1e-7f));

    for (int step = 0; step < steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && steps > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(step + 1) / (float)steps);
        }

        bias_snn_step(bridge);
    }

    bridge->state = BIAS_SNN_STATE_IDLE;
    return 0;
}

int bias_snn_step(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_step: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_step", 0.0f);


    float dt = bridge->config.dt_ms;

    // Step all bias type neurons and track activity
    for (uint32_t t = 0; t < bridge->num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->num_types > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)bridge->num_types);
        }

        float type_activity = 0.0f;
        float membrane_activity = 0.0f;
        for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_type);
            }

            float input = bridge->bias_neurons[t][n].input_current + bridge->config.baseline_activation;
            if (neuron_step(&bridge->bias_neurons[t][n], dt, input)) {
                type_activity += 1.0f;
                bridge->stats.total_spikes++;
            }
            // Also track membrane potential for continuous activation
            membrane_activity += bridge->bias_neurons[t][n].membrane_potential;
            membrane_activity += bridge->bias_neurons[t][n].input_current * 0.3f;
        }
        // Combine spike activity with membrane activity for robust detection
        float spike_rate = type_activity / bridge->config.neurons_per_type;
        membrane_activity /= bridge->config.neurons_per_type;
        bridge->type_activations[t] = nimcp_clampf(spike_rate * 2.0f + membrane_activity * 0.8f, 0.0f, 1.0f);
    }

    // Conflict detection - multiple bias types active simultaneously
    if (bridge->config.enable_conflict_detection) {
        float conflict_input = 0.0f;
        int active_count = 0;
        for (uint32_t t = 0; t < bridge->num_types; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && bridge->num_types > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(t + 1) / (float)bridge->num_types);
            }

            if (bridge->type_activations[t] > 0.3f) {
                active_count++;
                conflict_input += bridge->type_activations[t];
            }
        }
        if (active_count > 1) {
            conflict_input *= (float)active_count / bridge->num_types;
        }

        for (uint32_t n = 0; n < bridge->num_conflict_neurons; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->num_conflict_neurons > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->num_conflict_neurons);
            }

            if (neuron_step(&bridge->conflict_neurons[n], dt, conflict_input)) {
                bridge->stats.total_spikes++;
            }
        }
    }

    // Output neurons integrate all activity
    float total_input = 0.0f;
    for (uint32_t t = 0; t < bridge->num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->num_types > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)bridge->num_types);
        }

        total_input += bridge->type_activations[t];
    }
    total_input /= bridge->num_types;

    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        if (neuron_step(&bridge->output_neurons[n], dt, total_input)) {
            bridge->stats.total_spikes++;
        }
    }

    bridge->sim_time_us += (uint64_t)(dt * 1000.0f);
    return 0;
}

int bias_snn_forward(
    bias_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    // Distribute inputs to bias type neurons
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_forward", 0.0f);


    uint32_t inputs_per_type = input_count / bridge->num_types;
    for (uint32_t t = 0; t < bridge->num_types && t * inputs_per_type < input_count; t++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < inputs_per_type && (t * inputs_per_type + i) < input_count; i++) {
            sum += inputs[t * inputs_per_type + i];
        }
        sum /= inputs_per_type;

        for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
                bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_type);
            }

            bridge->bias_neurons[t][n].input_current = sum;
        }
    }

    return bias_snn_step(bridge);
}

//=============================================================================
// Detection Functions
//=============================================================================

int bias_snn_detect_biases(
    bias_snn_bridge_t* bridge,
    bias_snn_output_t* output
) {
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_detect_biases: required parameter is NULL (bridge, output)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_detect_bias", 0.0f);


    bridge->state = BIAS_SNN_STATE_DETECTING;

    // Copy per-type activations to output
    float max_activation = 0.0f;
    bias_snn_type_t dominant = BIAS_SNN_TYPE_CONFIRMATION;
    float total_activation = 0.0f;

    for (uint32_t t = 0; t < BIAS_SNN_TYPE_COUNT; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && BIAS_SNN_TYPE_COUNT > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)BIAS_SNN_TYPE_COUNT);
        }

        output->bias_magnitudes[t] = bridge->type_activations[t];
        total_activation += bridge->type_activations[t];

        if (bridge->type_activations[t] > max_activation) {
            max_activation = bridge->type_activations[t];
            dominant = (bias_snn_type_t)t;
        }
    }

    output->overall_bias_level = total_activation / (fabsf(BIAS_SNN_TYPE_COUNT) > 1e-7f ? BIAS_SNN_TYPE_COUNT : 1e-7f);
    output->dominant_bias = dominant;
    output->bias_detected = (max_activation > bridge->config.bias_detection_threshold);

    // Compute conflict level
    float conflict_activity = 0.0f;
    for (uint32_t n = 0; n < bridge->num_conflict_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_conflict_neurons > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->num_conflict_neurons);
        }

        conflict_activity += bridge->conflict_neurons[n].membrane_potential;
    }
    output->conflict_level = nimcp_clampf(conflict_activity / bridge->num_conflict_neurons, 0.0f, 1.0f);

    // Metacognitive awareness
    if (bridge->config.enable_metacognitive_monitoring) {
        output->metacognitive_awareness = output->conflict_level * 0.5f +
                                         (output->bias_detected ? 0.5f : 0.0f);
    } else {
        output->metacognitive_awareness = 0.0f;
    }

    // Detection confidence based on activation strength
    output->detection_confidence = max_activation > 0.0f ?
                                   nimcp_clampf(max_activation / 1.0f, 0.0f, 1.0f) : 0.0f;

    bridge->last_output = *output;

    // Update stats
    if (output->bias_detected) {
        switch (dominant) {
            case BIAS_SNN_TYPE_CONFIRMATION:
                bridge->stats.confirmation_detections++;
                break;
            case BIAS_SNN_TYPE_AVAILABILITY:
                bridge->stats.availability_detections++;
                break;
            case BIAS_SNN_TYPE_ANCHORING:
                bridge->stats.anchoring_detections++;
                break;
            default:
                break;
        }

        if (bridge->detection_callback) {
            bridge->detection_callback(bridge, dominant, max_activation,
                                      output->detection_confidence,
                                      bridge->detection_callback_data);
        }
    }

    if (output->conflict_level > bridge->config.conflict_threshold && bridge->conflict_callback) {
        bridge->conflict_callback(bridge, output->conflict_level, bridge->conflict_callback_data);
    }

    bridge->stats.mean_bias_level =
        bridge->stats.mean_bias_level * 0.99f + output->overall_bias_level * 0.01f;
    bridge->stats.mean_conflict =
        bridge->stats.mean_conflict * 0.99f + output->conflict_level * 0.01f;

    bridge->state = BIAS_SNN_STATE_IDLE;
    return 0;
}

float bias_snn_get_bias_level(bias_snn_bridge_t* bridge, bias_snn_type_t type) {
    if (!bridge || type >= BIAS_SNN_TYPE_COUNT) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_bias_le", 0.0f);


    return bridge->type_activations[type];
}

float bias_snn_get_overall_bias(bias_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_overall", 0.0f);


    return bridge->last_output.overall_bias_level;
}

float bias_snn_get_conflict_level(bias_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_conflic", 0.0f);


    return bridge->last_output.conflict_level;
}

bias_snn_type_t bias_snn_get_dominant_bias(bias_snn_bridge_t* bridge) {
    if (!bridge) return BIAS_SNN_TYPE_CONFIRMATION;
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_dominan", 0.0f);


    return bridge->last_output.dominant_bias;
}

//=============================================================================
// State Query Functions
//=============================================================================

int bias_snn_get_type_state(
    bias_snn_bridge_t* bridge,
    bias_snn_type_t type,
    bias_type_state_t* state
) {
    if (!bridge || !state || type >= BIAS_SNN_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_get_type_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_type_st", 0.0f);


    state->type = type;
    state->activation = bridge->type_activations[type];
    state->confidence = bridge->type_confidences[type];

    // Compute spike count and rate
    uint32_t total_spikes = 0;
    for (uint32_t n = 0; n < bridge->config.neurons_per_type; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_type > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_type);
        }

        total_spikes += bridge->bias_neurons[type][n].spike_count;
    }
    state->spike_count = total_spikes;
    state->spike_rate = (float)total_spikes / bridge->config.neurons_per_type;

    state->conflict_level = bridge->last_output.conflict_level;

    return 0;
}

int bias_snn_get_state(
    bias_snn_bridge_t* bridge,
    bias_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_state", 0.0f);


    state->state = bridge->state;

    // Compute total activity
    float total = 0.0f;
    for (uint32_t t = 0; t < bridge->num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->num_types > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)bridge->num_types);
        }

        total += bridge->type_activations[t];
    }
    state->total_activity = total;

    state->mean_bias_level = bridge->stats.mean_bias_level;
    state->max_bias_level = bridge->last_output.overall_bias_level;
    state->dominant_bias = bridge->last_output.dominant_bias;
    state->metacognitive_awareness = bridge->last_output.metacognitive_awareness;

    // Count active biases
    uint32_t active = 0;
    for (uint32_t t = 0; t < bridge->num_types; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && bridge->num_types > 256) {
            bias_snn_bridge_heartbeat("bias_snn_bri_loop",
                             (float)(t + 1) / (float)bridge->num_types);
        }

        if (bridge->type_activations[t] > 0.3f) active++;
    }
    state->active_biases = active;

    return 0;
}

int bias_snn_get_stats(bias_snn_bridge_t* bridge, bias_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_get_stats", 0.0f);


    return 0;
}

int bias_snn_reset_stats(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_reset_stats: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_reset_stats", 0.0f);


    memset(&bridge->stats, 0, sizeof(bias_snn_stats_t));
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int bias_snn_register_detection_callback(
    bias_snn_bridge_t* bridge,
    bias_snn_detection_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_register_detection_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_register_de", 0.0f);


    bridge->detection_callback = callback;
    bridge->detection_callback_data = user_data;
    return 0;
}

int bias_snn_register_conflict_callback(
    bias_snn_bridge_t* bridge,
    bias_snn_conflict_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_register_conflict_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_register_co", 0.0f);


    bridge->conflict_callback = callback;
    bridge->conflict_callback_data = user_data;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int bias_snn_bio_async_connect(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_bio_async_c", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int bias_snn_bio_async_disconnect(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_bio_async_d", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool bias_snn_is_bio_async_connected(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_snn_bridge_heartbeat("bias_snn_bri_bias_snn_is_bio_asyn", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* bias_snn_type_name(bias_snn_type_t type) {
    if (type >= BIAS_SNN_TYPE_COUNT) return "unknown";
    return bias_type_names[type];
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void bias_snn_bridge_set_instance_health_agent(bias_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "bias_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int bias_snn_bridge_training_begin(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    bias_snn_bridge_heartbeat_instance(bridge->health_agent, "bias_snn_bridge_training_begin", 0.0f);
    return 0;
}

int bias_snn_bridge_training_end(bias_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_snn_bridge_training_end: NULL argument");
        return -1;
    }
    bias_snn_bridge_heartbeat_instance(bridge->health_agent, "bias_snn_bridge_training_end", 1.0f);
    return 0;
}

int bias_snn_bridge_training_step(bias_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    bias_snn_bridge_heartbeat_instance(bridge->health_agent, "bias_snn_bridge_training_step", progress);
    return 0;
}
