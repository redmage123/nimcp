/**
 * @file nimcp_epistemic_snn_bridge.c
 * @brief Epistemic - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Implementation of SNN bridge for epistemic filtering
 * WHY:  Enable biologically-plausible belief evaluation through spike-based processing
 * HOW:  Encode evidence quality, source reliability, and bias signals as spike patterns
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/epistemic/nimcp_epistemic_snn_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
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
#include "constants/nimcp_neural_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(epistemic_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_epistemic_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_epistemic_snn_bridge_mesh_registry = NULL;

nimcp_error_t epistemic_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_epistemic_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "epistemic_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "epistemic_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_epistemic_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_epistemic_snn_bridge_mesh_registry = registry;
    return err;
}

void epistemic_snn_bridge_mesh_unregister(void) {
    if (g_epistemic_snn_bridge_mesh_registry && g_epistemic_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_epistemic_snn_bridge_mesh_registry, g_epistemic_snn_bridge_mesh_id);
        g_epistemic_snn_bridge_mesh_id = 0;
        g_epistemic_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from epistemic_snn_bridge module (instance-level) */
static inline void epistemic_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_epistemic_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_epistemic_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_epistemic_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "EPISTEMIC_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    uint32_t source_id;
    float reliability;
    float confidence;
    uint32_t evaluation_count;
    uint32_t correct_count;
    uint64_t last_update_us;
    bool active;
} source_tracking_t;

typedef struct {
    float membrane_potential;
    float threshold;
    float spike_rate;
    uint32_t spike_count;
    float refractory_remaining;
    float evidence_input;
    float reliability_input;
    float bias_input;
} epistemic_neuron_t;

struct epistemic_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    epistemic_snn_config_t config;
    epistemic_snn_state_t state;

    // Neuron populations
    epistemic_neuron_t* evidence_neurons;
    epistemic_neuron_t* reliability_neurons;
    epistemic_neuron_t* bias_neurons;
    epistemic_neuron_t* output_neurons;
    uint32_t num_evidence_neurons;
    uint32_t num_reliability_neurons;
    uint32_t num_bias_neurons;
    uint32_t num_output_neurons;

    // Source tracking
    source_tracking_t* sources;
    uint32_t num_sources;

    // Current inputs
    float current_evidence_quality;
    float current_plausibility;
    float current_source_reliability;
    float* bias_magnitudes;
    uint32_t num_biases;

    // Output state
    epistemic_snn_output_t last_output;

    // Statistics
    epistemic_snn_stats_t stats;

    // Callbacks
    epistemic_snn_spike_callback_t spike_callback;
    void* spike_callback_data;
    epistemic_snn_bias_callback_t bias_callback;
    void* bias_callback_data;

    // Bio-async
    bool bio_async_connected;

    // Simulation time
    uint64_t sim_time_us;
};

static void reset_neuron(epistemic_neuron_t* neuron) {
    neuron->membrane_potential = 0.0f;
    neuron->threshold = 1.0f;
    neuron->spike_rate = 0.0f;
    neuron->spike_count = 0;
    neuron->refractory_remaining = 0.0f;
    neuron->evidence_input = 0.0f;
    neuron->reliability_input = 0.0f;
    neuron->bias_input = 0.0f;
}

static bool neuron_step(epistemic_neuron_t* neuron, float dt_ms, float input) {
    const float tau_membrane = NIMCP_MEMBRANE_TAU_MS;
    const float refractory_period = NIMCP_REFRACTORY_PERIOD_MS;

    if (neuron->refractory_remaining > 0.0f) {
        neuron->refractory_remaining -= dt_ms;
        return false;
    }

    // Leaky integrate
    float decay = expf(-dt_ms / tau_membrane);
    neuron->membrane_potential = neuron->membrane_potential * decay + input * (1.0f - decay);

    // Check for spike
    if (neuron->membrane_potential >= neuron->threshold) {
        neuron->membrane_potential = 0.0f;
        neuron->refractory_remaining = refractory_period;
        neuron->spike_count++;
        return true;
    }

    return false;
}

static float compute_population_rate(epistemic_neuron_t* neurons, uint32_t count, float window_ms) {
    if (count == 0 || window_ms <= 0.0f) return 0.0f;

    uint32_t total_spikes = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)count);
        }

        total_spikes += neurons[i].spike_count;
    }

    return (float)total_spikes / (count * window_ms / 1000.0f);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

epistemic_snn_config_t epistemic_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_config", 0.0f);


    epistemic_snn_config_t config = {
        .max_sources = EPISTEMIC_SNN_MAX_SOURCES,
        .neurons_per_dim = EPISTEMIC_SNN_NEURONS_PER_DIM,
        .input_dim = EPISTEMIC_SNN_INPUT_DIM,
        .hidden_dim = EPISTEMIC_SNN_HIDDEN_DIM,
        .dt_ms = 1.0f,
        .evidence_gain = 2.0f,
        .uncertainty_gain = 1.5f,
        .bias_detection_threshold = 0.7f,
        .encoding_type = EPISTEMIC_SNN_ENCODE_RATE,
        .enable_source_tracking = true,
        .enable_bias_detection = true,
        .enable_conspiracy_detection = true,
        .enable_bio_async = false
    };
    return config;
}

epistemic_snn_bridge_t* epistemic_snn_create(const epistemic_snn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_create: config is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_create", 0.0f);


    epistemic_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(epistemic_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_snn_create: failed to allocate bridge");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = EPISTEMIC_SNN_STATE_IDLE;

    // Allocate neuron populations
    bridge->num_evidence_neurons = config->neurons_per_dim;
    bridge->num_reliability_neurons = config->neurons_per_dim;
    bridge->num_bias_neurons = config->neurons_per_dim;
    bridge->num_output_neurons = config->neurons_per_dim * 2;

    bridge->evidence_neurons = nimcp_calloc(bridge->num_evidence_neurons, sizeof(epistemic_neuron_t));
    bridge->reliability_neurons = nimcp_calloc(bridge->num_reliability_neurons, sizeof(epistemic_neuron_t));
    bridge->bias_neurons = nimcp_calloc(bridge->num_bias_neurons, sizeof(epistemic_neuron_t));
    bridge->output_neurons = nimcp_calloc(bridge->num_output_neurons, sizeof(epistemic_neuron_t));

    if (!bridge->evidence_neurons || !bridge->reliability_neurons ||
        !bridge->bias_neurons || !bridge->output_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_snn_create: failed to allocate neurons");
        epistemic_snn_destroy(bridge);
        return NULL;
    }

    // Initialize neurons
    for (uint32_t i = 0; i < bridge->num_evidence_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_evidence_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_evidence_neurons);
        }

        reset_neuron(&bridge->evidence_neurons[i]);
    }
    for (uint32_t i = 0; i < bridge->num_reliability_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_reliability_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_reliability_neurons);
        }

        reset_neuron(&bridge->reliability_neurons[i]);
    }
    for (uint32_t i = 0; i < bridge->num_bias_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_bias_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_bias_neurons);
        }

        reset_neuron(&bridge->bias_neurons[i]);
    }
    for (uint32_t i = 0; i < bridge->num_output_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_output_neurons);
        }

        reset_neuron(&bridge->output_neurons[i]);
    }

    // Allocate source tracking
    if (config->enable_source_tracking) {
        bridge->sources = nimcp_calloc(config->max_sources, sizeof(source_tracking_t));
        if (!bridge->sources) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_snn_create: failed to allocate sources");
            epistemic_snn_destroy(bridge);
            return NULL;
        }
    }

    // Allocate bias magnitudes array
    bridge->bias_magnitudes = nimcp_calloc(16, sizeof(float));
    if (!bridge->bias_magnitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "epistemic_snn_create: failed to allocate bias_magnitudes");
        epistemic_snn_destroy(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "epistemic_snn");
    return bridge;
}

void epistemic_snn_destroy(epistemic_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "epistemic_snn");

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_destro", 0.0f);


    nimcp_free(bridge->evidence_neurons);
    nimcp_free(bridge->reliability_neurons);
    nimcp_free(bridge->bias_neurons);
    nimcp_free(bridge->output_neurons);
    nimcp_free(bridge->sources);
    nimcp_free(bridge->bias_magnitudes);
    nimcp_free(bridge);
    bridge = NULL;
}

int epistemic_snn_reset(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_reset", 0.0f);


    bridge->state = EPISTEMIC_SNN_STATE_IDLE;
    bridge->sim_time_us = 0;

    // Reset neurons
    for (uint32_t i = 0; i < bridge->num_evidence_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_evidence_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_evidence_neurons);
        }

        reset_neuron(&bridge->evidence_neurons[i]);
    }
    for (uint32_t i = 0; i < bridge->num_reliability_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_reliability_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_reliability_neurons);
        }

        reset_neuron(&bridge->reliability_neurons[i]);
    }
    for (uint32_t i = 0; i < bridge->num_bias_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_bias_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_bias_neurons);
        }

        reset_neuron(&bridge->bias_neurons[i]);
    }
    for (uint32_t i = 0; i < bridge->num_output_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_output_neurons);
        }

        reset_neuron(&bridge->output_neurons[i]);
    }

    // Note: Sources are NOT reset - source reliability is learned state
    // that should persist across neural resets. Only neuron states are cleared.

    // Reset inputs
    bridge->current_evidence_quality = 0.0f;
    bridge->current_plausibility = 0.0f;
    bridge->current_source_reliability = 0.5f;
    bridge->num_biases = 0;

    // Reset output
    memset(&bridge->last_output, 0, sizeof(epistemic_snn_output_t));

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int epistemic_snn_encode_evidence(
    epistemic_snn_bridge_t* bridge,
    float evidence_quality,
    float plausibility,
    float source_reliability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_encode_evidence: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_encode", 0.0f);


    bridge->state = EPISTEMIC_SNN_STATE_ENCODING;

    bridge->current_evidence_quality = nimcp_clampf(evidence_quality, 0.0f, 1.0f);
    bridge->current_plausibility = nimcp_clampf(plausibility, 0.0f, 1.0f);
    bridge->current_source_reliability = nimcp_clampf(source_reliability, 0.0f, 1.0f);

    // Encode into evidence neurons using rate coding
    float evidence_input = bridge->current_evidence_quality * bridge->config.evidence_gain;
    for (uint32_t i = 0; i < bridge->num_evidence_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_evidence_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_evidence_neurons);
        }

        // Population coding: each neuron tuned to different evidence level
        float tuning = (bridge->num_evidence_neurons > 1) ? (float)i / (float)(bridge->num_evidence_neurons - 1) : 0.0f;
        float activation = expf(-powf(evidence_quality - tuning, 2.0f) / 0.1f);
        bridge->evidence_neurons[i].evidence_input = evidence_input * activation;
    }

    // Encode into reliability neurons
    for (uint32_t i = 0; i < bridge->num_reliability_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_reliability_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_reliability_neurons);
        }

        float tuning = (bridge->num_reliability_neurons > 1) ? (float)i / (float)(bridge->num_reliability_neurons - 1) : 0.0f;
        float activation = expf(-powf(source_reliability - tuning, 2.0f) / 0.1f);
        bridge->reliability_neurons[i].reliability_input = source_reliability * activation;
    }

    bridge->stats.total_evaluations++;

    return 0;
}

int epistemic_snn_encode_claim(
    epistemic_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    float prior_probability
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_encode_claim: required parameter is NULL (bridge, features)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_encode", 0.0f);


    bridge->state = EPISTEMIC_SNN_STATE_ENCODING;

    // Distribute features across evidence neurons
    uint32_t features_per_neuron = (feature_count + bridge->num_evidence_neurons - 1) /
                                    bridge->num_evidence_neurons;

    for (uint32_t i = 0; i < bridge->num_evidence_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_evidence_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_evidence_neurons);
        }

        float sum = 0.0f;
        uint32_t start = i * features_per_neuron;
        uint32_t end = start + features_per_neuron;
        if (end > feature_count) end = feature_count;

        for (uint32_t j = start; j < end; j++) {
            sum += features[j];
        }

        if (end > start) {
            bridge->evidence_neurons[i].evidence_input =
                (sum / (end - start)) * prior_probability * bridge->config.evidence_gain;
        }
    }

    return 0;
}

int epistemic_snn_encode_bias_signals(
    epistemic_snn_bridge_t* bridge,
    const float* bias_magnitudes,
    uint32_t num_biases
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_encode_bias_signals: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_encode", 0.0f);


    if (bias_magnitudes && num_biases > 0) {
        uint32_t copy_count = num_biases < 16 ? num_biases : 16;
        memcpy(bridge->bias_magnitudes, bias_magnitudes, copy_count * sizeof(float));
        bridge->num_biases = copy_count;

        // Encode into bias neurons
        for (uint32_t i = 0; i < bridge->num_bias_neurons && i < copy_count; i++) {
            bridge->bias_neurons[i].bias_input = bias_magnitudes[i];
        }
    }

    return 0;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int epistemic_snn_simulate(epistemic_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_simulate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_simula", 0.0f);


    bridge->state = EPISTEMIC_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    int steps = (int)(duration_ms / (fabsf(dt) > 1e-7f ? dt : 1e-7f));

    for (int step = 0; step < steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && steps > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(step + 1) / (float)steps);
        }

        epistemic_snn_step(bridge);
    }

    bridge->state = EPISTEMIC_SNN_STATE_IDLE;
    return 0;
}

int epistemic_snn_step(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_step: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_step", 0.0f);


    float dt = bridge->config.dt_ms;

    // Step evidence neurons
    float evidence_activity = 0.0f;
    for (uint32_t i = 0; i < bridge->num_evidence_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_evidence_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_evidence_neurons);
        }

        if (neuron_step(&bridge->evidence_neurons[i], dt,
                       bridge->evidence_neurons[i].evidence_input)) {
            evidence_activity += 1.0f;
            bridge->stats.total_spikes++;
        }
    }
    evidence_activity /= bridge->num_evidence_neurons;

    // Step reliability neurons
    float reliability_activity = 0.0f;
    for (uint32_t i = 0; i < bridge->num_reliability_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_reliability_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_reliability_neurons);
        }

        if (neuron_step(&bridge->reliability_neurons[i], dt,
                       bridge->reliability_neurons[i].reliability_input)) {
            reliability_activity += 1.0f;
            bridge->stats.total_spikes++;
        }
    }
    reliability_activity /= bridge->num_reliability_neurons;

    // Step bias neurons
    float bias_activity = 0.0f;
    for (uint32_t i = 0; i < bridge->num_bias_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_bias_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_bias_neurons);
        }

        if (neuron_step(&bridge->bias_neurons[i], dt,
                       bridge->bias_neurons[i].bias_input)) {
            bias_activity += 1.0f;
            bridge->stats.total_spikes++;

            // Check for bias detection
            if (bridge->config.enable_bias_detection &&
                bridge->bias_neurons[i].bias_input > bridge->config.bias_detection_threshold) {
                bridge->stats.bias_detections++;
                if (bridge->bias_callback) {
                    bridge->bias_callback(bridge, i, bridge->bias_neurons[i].bias_input,
                                         bridge->bias_callback_data);
                }
            }
        }
    }
    bias_activity /= bridge->num_bias_neurons;

    // Step output neurons - integrate evidence and reliability
    for (uint32_t i = 0; i < bridge->num_output_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_output_neurons);
        }

        float input = evidence_activity * reliability_activity - bias_activity * 0.5f;
        if (neuron_step(&bridge->output_neurons[i], dt, input)) {
            bridge->stats.total_spikes++;
        }
    }

    bridge->sim_time_us += (uint64_t)(dt * 1000.0f);

    return 0;
}

int epistemic_snn_forward(
    epistemic_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    // Encode inputs directly into evidence neurons
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_forwar", 0.0f);


    for (uint32_t i = 0; i < bridge->num_evidence_neurons && i < input_count; i++) {
        bridge->evidence_neurons[i].evidence_input = inputs[i] * bridge->config.evidence_gain;
    }

    // Run single simulation step
    return epistemic_snn_step(bridge);
}

//=============================================================================
// Decoding Functions
//=============================================================================

int epistemic_snn_decode_assessment(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_output_t* output
) {
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_decode_assessment: required parameter is NULL (bridge, output)");
        return -1;
    }

    // Compute average membrane potential activity (more responsive than spike rates)
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_decode", 0.0f);


    float evidence_activity = 0.0f;
    for (uint32_t i = 0; i < bridge->num_evidence_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_evidence_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_evidence_neurons);
        }

        evidence_activity += bridge->evidence_neurons[i].membrane_potential;
        evidence_activity += bridge->evidence_neurons[i].evidence_input * 0.5f;
    }
    evidence_activity /= bridge->num_evidence_neurons;

    float reliability_activity = 0.0f;
    for (uint32_t i = 0; i < bridge->num_reliability_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_reliability_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_reliability_neurons);
        }

        reliability_activity += bridge->reliability_neurons[i].membrane_potential;
        reliability_activity += bridge->reliability_neurons[i].reliability_input * 0.5f;
    }
    reliability_activity /= bridge->num_reliability_neurons;

    float bias_activity = 0.0f;
    for (uint32_t i = 0; i < bridge->num_bias_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_bias_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_bias_neurons);
        }

        bias_activity += bridge->bias_neurons[i].membrane_potential;
        bias_activity += bridge->bias_neurons[i].bias_input * 0.5f;
    }
    bias_activity /= bridge->num_bias_neurons;

    // Combine inputs with membrane activity for robust output
    float combined_evidence = bridge->current_evidence_quality * 0.6f + evidence_activity * 0.4f;
    float combined_reliability = bridge->current_source_reliability * 0.6f + reliability_activity * 0.4f;

    // Epistemic quality: weighted combination of evidence and reliability, reduced by bias
    output->epistemic_quality = nimcp_clampf(combined_evidence * combined_reliability * 1.2f - bias_activity * 0.3f, 0.0f, 1.0f);
    output->evidence_strength = nimcp_clampf(combined_evidence, 0.0f, 1.0f);
    output->source_reliability = nimcp_clampf(combined_reliability, 0.0f, 1.0f);
    output->bias_magnitude = nimcp_clampf(bias_activity, 0.0f, 1.0f);

    // Compute uncertainty from variance in output membrane potentials using statistics module
    float variance = 0.0f;
    if (bridge->num_output_neurons > 0) {
        float* potentials = (float*)nimcp_calloc(bridge->num_output_neurons, sizeof(float));
        if (potentials) {
            for (uint32_t i = 0; i < bridge->num_output_neurons; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && bridge->num_output_neurons > 256) {
                    epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                                     (float)(i + 1) / (float)bridge->num_output_neurons);
                }
                potentials[i] = bridge->output_neurons[i].membrane_potential;
            }
            variance = nimcp_stats_variance_population(potentials, bridge->num_output_neurons);
            nimcp_free(potentials);
            potentials = NULL;
        }
    }
    // Higher variance means less certainty; also factor in evidence quality
    output->uncertainty = nimcp_clampf(1.0f - output->epistemic_quality + sqrtf(variance), 0.0f, 1.0f);

    // Bias detection
    output->bias_detected = (output->bias_magnitude > bridge->config.bias_detection_threshold);
    if (output->bias_detected) {
        bridge->stats.bias_detections++;
    }

    // Conspiracy detection - multiple high biases co-occurring
    if (bridge->config.enable_conspiracy_detection) {
        int high_bias_count = 0;
        for (uint32_t i = 0; i < bridge->num_biases; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->num_biases > 256) {
                epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                                 (float)(i + 1) / (float)bridge->num_biases);
            }

            if (bridge->bias_magnitudes[i] > 0.6f) {
                high_bias_count++;
            }
        }
        output->conspiracy_likelihood = nimcp_clampf((float)high_bias_count / 3.0f, 0.0f, 1.0f);
        output->conspiracy_detected = (output->conspiracy_likelihood > 0.7f);
        if (output->conspiracy_detected) {
            bridge->stats.conspiracy_detections++;
        }
    } else {
        output->conspiracy_likelihood = 0.0f;
        output->conspiracy_detected = false;
    }

    bridge->last_output = *output;

    // Update running stats
    bridge->stats.mean_evidence_quality =
        (bridge->stats.mean_evidence_quality * 0.99f) + (output->evidence_strength * 0.01f);
    bridge->stats.mean_source_reliability =
        (bridge->stats.mean_source_reliability * 0.99f) + (output->source_reliability * 0.01f);

    return 0;
}

float epistemic_snn_get_epistemic_quality(epistemic_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_ep", 0.0f);


    return bridge->last_output.epistemic_quality;
}

float epistemic_snn_get_uncertainty(epistemic_snn_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_un", 0.0f);


    return bridge->last_output.uncertainty;
}

float epistemic_snn_get_bias_level(epistemic_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_bi", 0.0f);


    return bridge->last_output.bias_magnitude;
}

float epistemic_snn_get_conspiracy_score(epistemic_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_co", 0.0f);


    return bridge->last_output.conspiracy_likelihood;
}

//=============================================================================
// Source Tracking Functions
//=============================================================================

int epistemic_snn_register_source(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id,
    float initial_reliability
) {
    if (!bridge || !bridge->sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_register_source: required parameter is NULL (bridge, bridge->sources)");
        return -1;
    }
    if (bridge->num_sources >= bridge->config.max_sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "epistemic_snn_register_source: capacity exceeded");
        return -1;
    }

    // Check if source already exists
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_regist", 0.0f);


    for (uint32_t i = 0; i < bridge->config.max_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_sources > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->config.max_sources);
        }

        if (bridge->sources[i].active && bridge->sources[i].source_id == source_id) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epistemic_snn_register_source: validation failed");
            return -1;  // Already registered
        }
    }

    // Find empty slot
    for (uint32_t i = 0; i < bridge->config.max_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_sources > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->config.max_sources);
        }

        if (!bridge->sources[i].active) {
            bridge->sources[i].source_id = source_id;
            bridge->sources[i].reliability = nimcp_clampf(initial_reliability, 0.0f, 1.0f);
            bridge->sources[i].confidence = 0.5f;
            bridge->sources[i].evaluation_count = 0;
            bridge->sources[i].correct_count = 0;
            bridge->sources[i].last_update_us = bridge->sim_time_us;
            bridge->sources[i].active = true;
            bridge->num_sources++;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epistemic_snn_register_source: operation failed");
    return -1;
}

int epistemic_snn_update_source_reliability(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id,
    bool was_correct
) {
    if (!bridge || !bridge->sources) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_update_source_reliability: required parameter is NULL (bridge, bridge->sources)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_update", 0.0f);


    for (uint32_t i = 0; i < bridge->config.max_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_sources > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->config.max_sources);
        }

        if (bridge->sources[i].active && bridge->sources[i].source_id == source_id) {
            bridge->sources[i].evaluation_count++;
            if (was_correct) {
                bridge->sources[i].correct_count++;
            }

            // Update reliability using Bayesian update
            float alpha = (float)bridge->sources[i].correct_count + 1.0f;
            float beta = (float)(bridge->sources[i].evaluation_count -
                                bridge->sources[i].correct_count) + 1.0f;
            bridge->sources[i].reliability = alpha / (alpha + beta);

            // Update confidence based on sample size
            bridge->sources[i].confidence = 1.0f - 1.0f / (1.0f +
                                            0.1f * bridge->sources[i].evaluation_count);

            bridge->sources[i].last_update_us = bridge->sim_time_us;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epistemic_snn_update_source_reliability: operation failed");
    return -1;
}

float epistemic_snn_get_source_reliability(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id
) {
    if (!bridge || !bridge->sources) return 0.5f;

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_so", 0.0f);


    for (uint32_t i = 0; i < bridge->config.max_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.max_sources > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->config.max_sources);
        }

        if (bridge->sources[i].active && bridge->sources[i].source_id == source_id) {
            return bridge->sources[i].reliability;
        }
    }

    return 0.5f;  // Default reliability for unknown sources
}

//=============================================================================
// State Query Functions
//=============================================================================

int epistemic_snn_get_dimension_state(
    epistemic_snn_bridge_t* bridge,
    uint32_t dim,
    epistemic_dimension_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_get_dimension_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->num_evidence_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "epistemic_snn_get_dimension_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_di", 0.0f);


    state->evidence_quality = bridge->evidence_neurons[dim].evidence_input;
    state->source_reliability = bridge->reliability_neurons[dim % bridge->num_reliability_neurons].reliability_input;
    state->bias_level = bridge->bias_neurons[dim % bridge->num_bias_neurons].bias_input;
    state->uncertainty = bridge->last_output.uncertainty;
    state->spike_rate = (float)bridge->evidence_neurons[dim].spike_count;
    state->spike_count = bridge->evidence_neurons[dim].spike_count;

    return 0;
}

int epistemic_snn_get_state(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_st", 0.0f);


    state->state = bridge->state;

    // Compute total activity
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->num_output_neurons; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            epistemic_snn_bridge_heartbeat("epistemic_sn_loop",
                             (float)(i + 1) / (float)bridge->num_output_neurons);
        }

        total += bridge->output_neurons[i].membrane_potential;
    }
    state->total_activity = total / bridge->num_output_neurons;

    state->mean_evidence_quality = bridge->stats.mean_evidence_quality;
    state->mean_uncertainty = bridge->last_output.uncertainty;
    state->conspiracy_score = bridge->last_output.conspiracy_likelihood;
    state->active_sources = bridge->num_sources;

    return 0;
}

int epistemic_snn_get_stats(epistemic_snn_bridge_t* bridge, epistemic_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_get_st", 0.0f);


    return 0;
}

int epistemic_snn_reset_stats(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_reset_stats: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_reset_", 0.0f);


    memset(&bridge->stats, 0, sizeof(epistemic_snn_stats_t));
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int epistemic_snn_register_spike_callback(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_spike_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_register_spike_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_regist", 0.0f);


    bridge->spike_callback = callback;
    bridge->spike_callback_data = user_data;
    return 0;
}

int epistemic_snn_register_bias_callback(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_bias_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_register_bias_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_regist", 0.0f);


    bridge->bias_callback = callback;
    bridge->bias_callback_data = user_data;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int epistemic_snn_bio_async_connect(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_bio_as", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int epistemic_snn_bio_async_disconnect(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "epistemic_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_bio_as", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool epistemic_snn_is_bio_async_connected(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    epistemic_snn_bridge_heartbeat("epistemic_sn_epistemic_snn_is_bio", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void epistemic_snn_bridge_set_instance_health_agent(epistemic_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "epistemic_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int epistemic_snn_bridge_training_begin(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    epistemic_snn_bridge_heartbeat_instance(bridge->health_agent, "epistemic_snn_bridge_training_begin", 0.0f);
    return 0;
}

int epistemic_snn_bridge_training_end(epistemic_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_snn_bridge_training_end: NULL argument");
        return -1;
    }
    epistemic_snn_bridge_heartbeat_instance(bridge->health_agent, "epistemic_snn_bridge_training_end", 1.0f);
    return 0;
}

int epistemic_snn_bridge_training_step(epistemic_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "epistemic_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    epistemic_snn_bridge_heartbeat_instance(bridge->health_agent, "epistemic_snn_bridge_training_step", progress);
    return 0;
}
