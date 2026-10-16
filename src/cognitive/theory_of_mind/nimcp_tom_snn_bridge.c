/**
 * @file nimcp_tom_snn_bridge.c
 * @brief Theory of Mind - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tom_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_tom_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_tom_snn_bridge_mesh_registry = NULL;

nimcp_error_t tom_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_tom_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "tom_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "tom_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_tom_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_tom_snn_bridge_mesh_registry = registry;
    return err;
}

void tom_snn_bridge_mesh_unregister(void) {
    if (g_tom_snn_bridge_mesh_registry && g_tom_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_tom_snn_bridge_mesh_registry, g_tom_snn_bridge_mesh_id);
        g_tom_snn_bridge_mesh_id = 0;
        g_tom_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from tom_snn_bridge module (instance-level) */
static inline void tom_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_tom_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_tom_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_tom_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "TOM_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct tom_snn_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    tom_snn_config_t config;
    snn_network_t* snn;

    /* State */
    tom_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    tom_dim_state_t dim_states[TOM_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* inference_buffer;

    /* Inference state */
    tom_inference_t last_inference;
    float deception_signal;
    float empathy_signal;
    float perspective_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    tom_snn_deception_callback_t deception_callback;
    void* deception_callback_data;
    tom_snn_inference_callback_t inference_callback;
    void* inference_callback_data;
    tom_snn_perspective_callback_t perspective_callback;
    void* perspective_callback_data;

    /* Statistics */
    tom_snn_stats_t stats;
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
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

tom_snn_config_t tom_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_config_defau", 0.0f);


    tom_snn_config_t config = {
        .num_dimensions = TOM_DIM_COUNT,
        .neurons_per_dim = TOM_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = TOM_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = TOM_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = TOM_SNN_DECODE_INTEGRATION,
        .inference_threshold = 0.6f,
        .deception_threshold = TOM_SNN_DECEPTION_THRESHOLD,
        .confidence_threshold = 0.6f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_deception_detection = true,
        .deception_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_mental_simulation = true,
        .simulation_gain = 1.0f,
        .enable_perspective_taking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

tom_snn_bridge_t* tom_snn_create(const tom_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_create", 0.0f);


    tom_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(tom_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = tom_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > TOM_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_create: operation failed");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "tom_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize bridge base in tom_snn_create");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 10; /* belief, desire, intention, perspective, empathy, social, deception, shared_attn, empathic_acc, simulation */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to create SNN network in tom_snn_create");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->inference_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->inference_buffer || !bridge->prev_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate buffers in tom_snn_create");
        tom_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize inference to neutral */
    bridge->last_inference.belief_state = 0.5f;
    bridge->last_inference.desire_state = 0.5f;
    bridge->last_inference.intention_clarity = 0.5f;
    bridge->last_inference.perspective_alignment = 0.5f;
    bridge->last_inference.empathic_accuracy = 0.5f;
    bridge->last_inference.social_context_match = 0.5f;
    bridge->last_inference.deception_detected = false;
    bridge->last_inference.deception_confidence = 0.0f;
    bridge->last_inference.shared_attention_strength = 0.5f;
    bridge->last_inference.mental_simulation_depth = 0.5f;
    bridge->last_inference.confidence = 0.5f;

    bridge->state = TOM_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->deception_signal = 0.0f;
    bridge->empathy_signal = 0.0f;
    bridge->perspective_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "tom_snn");
    return bridge;
}

void tom_snn_destroy(tom_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "tom_snn");

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_destroy", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->inference_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int tom_snn_reset(tom_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset inference */
    memset(&bridge->last_inference, 0, sizeof(tom_inference_t));
    bridge->last_inference.belief_state = 0.5f;
    bridge->last_inference.desire_state = 0.5f;
    bridge->last_inference.intention_clarity = 0.5f;
    bridge->last_inference.perspective_alignment = 0.5f;
    bridge->last_inference.confidence = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 10 * sizeof(float));
    memset(bridge->inference_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = TOM_SNN_STATE_IDLE;
    bridge->deception_signal = 0.0f;
    bridge->empathy_signal = 0.0f;
    bridge->perspective_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int tom_snn_encode_context(
    tom_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_encode_context: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_snn_encode_context: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_encode_conte", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = TOM_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
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
                tom_snn_bridge_heartbeat("tom_snn_brid_loop",
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

    /* Detect perspective change */
    float perspective_change = 0.0f;
    if (TOM_DIM_PERSPECTIVE < num_dims) {
        perspective_change = fabsf(dimensions[TOM_DIM_PERSPECTIVE] -
                                   bridge->prev_state[TOM_DIM_PERSPECTIVE]);
    }

    /* Store previous state */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        bridge->prev_state[d] = dimensions[d];
    }

    if (perspective_change > 0.3f) {
        bridge->stats.perspective_switches++;
        if (bridge->perspective_callback) {
            bridge->perspective_callback(bridge, dimensions[TOM_DIM_PERSPECTIVE],
                                         TOM_DIM_PERSPECTIVE, bridge->perspective_callback_data);
        }
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int tom_snn_encode_belief(
    tom_snn_bridge_t* bridge,
    float self_belief,
    float other_belief
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_encode_belief: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_encode_belie", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[TOM_DIM_COUNT] = {0};
    dims[TOM_DIM_BELIEF_STATE] = nimcp_clampf(other_belief, 0.0f, 1.0f);

    /* Detect false belief (discrepancy between self and other belief) */
    float belief_discrepancy = fabsf(self_belief - other_belief);
    dims[TOM_DIM_DECEPTION_DETECTION] = belief_discrepancy;

    if (belief_discrepancy > bridge->config.deception_threshold) {
        bridge->deception_signal = belief_discrepancy;
        bridge->stats.belief_updates++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return tom_snn_encode_context(bridge, dims, 2);
}

int tom_snn_encode_intention(
    tom_snn_bridge_t* bridge,
    float intention_strength,
    float goal_alignment,
    float action_predictability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_encode_intention: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_encode_inten", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[TOM_DIM_COUNT] = {0};
    dims[TOM_DIM_INTENTION] = nimcp_clampf(intention_strength, 0.0f, 1.0f);
    dims[TOM_DIM_DESIRE_STATE] = nimcp_clampf(goal_alignment, 0.0f, 1.0f);
    dims[TOM_DIM_SOCIAL_CONTEXT] = nimcp_clampf(action_predictability, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return tom_snn_encode_context(bridge, dims, 3);
}

int tom_snn_encode_empathy(
    tom_snn_bridge_t* bridge,
    float emotional_resonance,
    float cognitive_empathy
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_encode_empathy: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_encode_empat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[TOM_DIM_COUNT] = {0};
    dims[TOM_DIM_EMOTION_INFERENCE] = nimcp_clampf(emotional_resonance, 0.0f, 1.0f);
    dims[TOM_DIM_EMPATHIC_ACCURACY] = nimcp_clampf(cognitive_empathy, 0.0f, 1.0f);
    dims[TOM_DIM_MENTAL_SIMULATION] = nimcp_clampf((emotional_resonance + cognitive_empathy) / 2.0f, 0.0f, 1.0f);

    bridge->empathy_signal = (emotional_resonance + cognitive_empathy) / 2.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return tom_snn_encode_context(bridge, dims, 3);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int tom_snn_simulate(tom_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = TOM_SNN_STATE_SIMULATING;

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
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
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
                tom_snn_bridge_heartbeat("tom_snn_brid_loop",
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
        snn_network_get_outputs(bridge->snn, bridge->output_buffer, 10);
    }

    /* Decode outputs */
    bridge->last_inference.belief_state = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_inference.desire_state = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_inference.intention_clarity = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_inference.perspective_alignment = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_inference.empathic_accuracy = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_inference.social_context_match = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);
    bridge->last_inference.deception_confidence = nimcp_clampf(bridge->output_buffer[6], 0.0f, 1.0f);
    bridge->last_inference.shared_attention_strength = nimcp_clampf(bridge->output_buffer[7], 0.0f, 1.0f);
    bridge->last_inference.empathic_accuracy = nimcp_clampf(bridge->output_buffer[8], 0.0f, 1.0f);
    bridge->last_inference.mental_simulation_depth = nimcp_clampf(bridge->output_buffer[9], 0.0f, 1.0f);

    /* Compute overall confidence */
    float sum = 0.0f;
    for (int i = 0; i < 6; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 6 > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                             (float)(i + 1) / (float)6);
        }

        sum += bridge->output_buffer[i];
    }
    bridge->last_inference.confidence = nimcp_clampf(sum / 6.0f, 0.0f, 1.0f);

    /* Check deception threshold */
    if (bridge->last_inference.deception_confidence > bridge->config.deception_threshold) {
        bridge->last_inference.deception_detected = true;
        bridge->stats.deception_detections++;

        if (bridge->deception_callback) {
            bridge->deception_callback(bridge, bridge->last_inference.deception_confidence,
                                       bridge->current_time_us, bridge->deception_callback_data);
        }
    } else {
        bridge->last_inference.deception_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = TOM_SNN_STATE_IDLE;

    /* Invoke inference callback */
    if (bridge->inference_callback) {
        bridge->inference_callback(bridge, &bridge->last_inference, bridge->inference_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_snn_step(tom_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_step", 0.0f);


    return tom_snn_simulate(bridge, bridge->config.dt_ms);
}

int tom_snn_forward(
    tom_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_forward", 0.0f);


    int spike_count = tom_snn_encode_context(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_snn_forward: validation failed");
        return -1;
    }

    if (tom_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int tom_snn_get_inference(
    tom_snn_bridge_t* bridge,
    tom_inference_t* inference
) {
    if (!bridge || !inference) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_get_inference: required parameter is NULL (bridge, inference)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_inferenc", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *inference = bridge->last_inference;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_snn_get_activations(
    tom_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_activati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool tom_snn_check_deception(
    tom_snn_bridge_t* bridge,
    float* deception_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_check_decept", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_inference.deception_confidence;
    if (deception_level) {
        *deception_level = level;
    }
    bool detected = level > bridge->config.deception_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool tom_snn_check_perspective_shift(
    tom_snn_bridge_t* bridge,
    float* perspective_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_check_perspe", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_inference.perspective_alignment;
    if (perspective_level) {
        *perspective_level = level;
    }
    bool detected = bridge->perspective_signal > 0.3f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool tom_snn_check_empathy(
    tom_snn_bridge_t* bridge,
    float* resonance_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_check_empath", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->empathy_signal;
    if (resonance_level) {
        *resonance_level = level;
    }
    bool detected = level > 0.5f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

//=============================================================================
// State Query Functions
//=============================================================================

int tom_snn_get_dim_state(
    tom_snn_bridge_t* bridge,
    uint32_t dim,
    tom_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_dim_stat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_snn_get_state(
    tom_snn_bridge_t* bridge,
    tom_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_confidence = bridge->last_inference.confidence;
    state->deception_signal = bridge->deception_signal;
    state->empathy_signal = bridge->empathy_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
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

int tom_snn_get_stats(tom_snn_bridge_t* bridge, tom_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_snn_reset_stats(tom_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(tom_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float tom_snn_get_confidence(tom_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_confiden", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float confidence = bridge->last_inference.confidence;
    nimcp_mutex_unlock(bridge->base.mutex);

    return confidence;
}

float tom_snn_get_total_activity(tom_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_get_total_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            tom_snn_bridge_heartbeat("tom_snn_brid_loop",
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

int tom_snn_register_deception_callback(
    tom_snn_bridge_t* bridge,
    tom_snn_deception_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_register_deception_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_register_dec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->deception_callback = callback;
    bridge->deception_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_snn_register_inference_callback(
    tom_snn_bridge_t* bridge,
    tom_snn_inference_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_register_inference_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_register_inf", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->inference_callback = callback;
    bridge->inference_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_snn_register_perspective_callback(
    tom_snn_bridge_t* bridge,
    tom_snn_perspective_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_snn_register_perspective_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_register_per", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->perspective_callback = callback;
    bridge->perspective_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int tom_snn_bio_async_connect(tom_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!bridge->config.enable_bio_async) return -1;

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_bio_async_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_snn_bio_async_disconnect(tom_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_bio_async_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool tom_snn_is_bio_async_connected(tom_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    tom_snn_bridge_heartbeat("tom_snn_brid_tom_snn_is_bio_async", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void tom_snn_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_tom_snn_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */
int tom_snn_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    tom_snn_bridge_heartbeat("tom_snn_bridge_training_begin", 0.0f);
    return 0;
}

int tom_snn_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_snn_bridge_training_end: NULL argument");
        return -1;
    }
    tom_snn_bridge_heartbeat("tom_snn_bridge_training_end", 1.0f);
    return 0;
}

int tom_snn_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_snn_bridge_training_step: NULL argument");
        return -1;
    }
    tom_snn_bridge_heartbeat("tom_snn_bridge_training_step", progress);
    return 0;
}
