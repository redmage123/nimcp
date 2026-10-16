/**
 * @file nimcp_wellbeing_snn_bridge.c
 * @brief Wellbeing - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/wellbeing/nimcp_wellbeing_snn_bridge.h"
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

BRIDGE_BOILERPLATE(wellbeing_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "WELLBEING_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct wellbeing_snn_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    wellbeing_snn_config_t config;
    snn_network_t* snn;

    /* State */
    wellbeing_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    wellbeing_dim_state_t dim_states[WELLBEING_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* assessment_buffer;

    /* Assessment state */
    wellbeing_assessment_t last_assessment;
    float stress_signal;
    float balance_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    wellbeing_snn_stress_callback_t stress_callback;
    void* stress_callback_data;
    wellbeing_snn_assessment_callback_t assessment_callback;
    void* assessment_callback_data;
    wellbeing_snn_balance_callback_t balance_callback;
    void* balance_callback_data;

    /* Statistics */
    wellbeing_snn_stats_t stats;
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
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

wellbeing_snn_config_t wellbeing_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_config", 0.0f);


    wellbeing_snn_config_t config = {
        .num_dimensions = WELLBEING_DIM_COUNT,
        .neurons_per_dim = WELLBEING_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = WELLBEING_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = WELLBEING_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = WELLBEING_SNN_DECODE_INTEGRATION,
        .flourishing_threshold = 0.7f,
        .stress_threshold = WELLBEING_SNN_STRESS_THRESHOLD,
        .balance_threshold = 0.6f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_stress_detection = true,
        .vitality_weight = 1.0f,

        .enable_homeostasis = true,
        .homeostatic_gain = 1.5f,
        .enable_resilience_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

wellbeing_snn_bridge_t* wellbeing_snn_create(const wellbeing_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_create", 0.0f);


    wellbeing_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(wellbeing_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = wellbeing_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > WELLBEING_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_create: operation failed");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, 0, "wellbeing_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "wellbeing_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 8; /* hedonic, eudaimonic, vitality, resilience, social, stress, balance, flourishing */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wellbeing_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->assessment_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->assessment_buffer || !bridge->prev_state) {
        wellbeing_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wellbeing_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize assessment to neutral */
    bridge->last_assessment.hedonic_tone = 0.5f;
    bridge->last_assessment.eudaimonic_level = 0.5f;
    bridge->last_assessment.vitality_level = 0.5f;
    bridge->last_assessment.resilience_score = 0.5f;
    bridge->last_assessment.social_connection = 0.5f;
    bridge->last_assessment.autonomy_level = 0.5f;
    bridge->last_assessment.competence_level = 0.5f;
    bridge->last_assessment.stress_level = 0.3f;
    bridge->last_assessment.stress_detected = false;
    bridge->last_assessment.balance_achieved = false;
    bridge->last_assessment.flourishing_score = 0.5f;
    bridge->last_assessment.integration_score = 0.5f;

    bridge->state = WELLBEING_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->stress_signal = 0.0f;
    bridge->balance_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "wellbeing_snn");
    return bridge;
}

void wellbeing_snn_destroy(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "wellbeing_snn");

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_destro", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->assessment_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int wellbeing_snn_reset(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset assessment */
    memset(&bridge->last_assessment, 0, sizeof(wellbeing_assessment_t));
    bridge->last_assessment.hedonic_tone = 0.5f;
    bridge->last_assessment.eudaimonic_level = 0.5f;
    bridge->last_assessment.vitality_level = 0.5f;
    bridge->last_assessment.resilience_score = 0.5f;
    bridge->last_assessment.flourishing_score = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 8 * sizeof(float));
    memset(bridge->assessment_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = WELLBEING_SNN_STATE_IDLE;
    bridge->stress_signal = 0.0f;
    bridge->balance_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int wellbeing_snn_encode_state(
    wellbeing_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = WELLBEING_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
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
                wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
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

    /* Calculate balance signal (variance of dimensions) */
    float mean = 0.0f;
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        mean += dimensions[d];
    }
    mean /= num_dims;

    float variance = 0.0f;
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - mean;
        variance += diff * diff;
    }
    variance /= num_dims;

    /* Low variance = good balance */
    bridge->balance_signal = 1.0f - sqrtf(variance);

    /* Store previous state */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        bridge->prev_state[d] = dimensions[d];
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int wellbeing_snn_encode_hedonic(
    wellbeing_snn_bridge_t* bridge,
    float pleasure,
    float pain
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_encode_hedonic: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[WELLBEING_DIM_COUNT] = {0};
    dims[WELLBEING_DIM_HEDONIC] = nimcp_clampf(pleasure - pain * 0.5f, 0.0f, 1.0f);
    dims[WELLBEING_DIM_STRESS] = nimcp_clampf(pain, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return wellbeing_snn_encode_state(bridge, dims, 2);
}

int wellbeing_snn_encode_eudaimonic(
    wellbeing_snn_bridge_t* bridge,
    float meaning,
    float purpose,
    float growth
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_encode_eudaimonic: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[WELLBEING_DIM_COUNT] = {0};
    dims[WELLBEING_DIM_EUDAIMONIC] = nimcp_clampf((meaning + purpose + growth) / 3.0f, 0.0f, 1.0f);
    dims[WELLBEING_DIM_AUTONOMY] = nimcp_clampf(purpose * 0.8f, 0.0f, 1.0f);
    dims[WELLBEING_DIM_COMPETENCE] = nimcp_clampf(growth * 0.9f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return wellbeing_snn_encode_state(bridge, dims, 3);
}

int wellbeing_snn_encode_stress(
    wellbeing_snn_bridge_t* bridge,
    float stress_level,
    bool chronic
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_encode_stress: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[WELLBEING_DIM_COUNT] = {0};
    float adjusted_stress = nimcp_clampf(stress_level, 0.0f, 1.0f);
    if (chronic) {
        adjusted_stress *= 1.3f;
        adjusted_stress = nimcp_clampf(adjusted_stress, 0.0f, 1.0f);
    }

    dims[WELLBEING_DIM_STRESS] = adjusted_stress;
    dims[WELLBEING_DIM_VITALITY] = 1.0f - adjusted_stress * 0.5f;
    dims[WELLBEING_DIM_RESILIENCE] = nimcp_clampf(1.0f - adjusted_stress * 0.3f, 0.0f, 1.0f);

    bridge->stress_signal = adjusted_stress;

    if (adjusted_stress > bridge->config.stress_threshold) {
        bridge->last_assessment.stress_detected = true;
        bridge->stats.stress_detections++;

        if (bridge->stress_callback) {
            bridge->stress_callback(bridge, adjusted_stress,
                                   bridge->current_time_us, bridge->stress_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return wellbeing_snn_encode_state(bridge, dims, 3);
}

int wellbeing_snn_encode_social(
    wellbeing_snn_bridge_t* bridge,
    float belongingness,
    float support,
    float loneliness
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_encode_social: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_encode", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[WELLBEING_DIM_COUNT] = {0};
    float social = (belongingness + support) / 2.0f - loneliness * 0.5f;
    dims[WELLBEING_DIM_SOCIAL_CONNECTION] = nimcp_clampf(social, 0.0f, 1.0f);
    dims[WELLBEING_DIM_HEDONIC] = nimcp_clampf(belongingness * 0.3f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return wellbeing_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int wellbeing_snn_simulate(wellbeing_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_simula", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = WELLBEING_SNN_STATE_SIMULATING;

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
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
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
                wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
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
        snn_network_get_outputs(bridge->snn, bridge->output_buffer, 8);
    }

    /* Decode outputs */
    bridge->last_assessment.hedonic_tone = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_assessment.eudaimonic_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_assessment.vitality_level = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_assessment.resilience_score = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_assessment.social_connection = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_assessment.stress_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Calculate flourishing as weighted combination */
    float flourishing = (bridge->last_assessment.hedonic_tone * 0.2f +
                        bridge->last_assessment.eudaimonic_level * 0.25f +
                        bridge->last_assessment.vitality_level * 0.15f +
                        bridge->last_assessment.resilience_score * 0.2f +
                        bridge->last_assessment.social_connection * 0.2f) *
                        (1.0f - bridge->last_assessment.stress_level * 0.3f);
    bridge->last_assessment.flourishing_score = nimcp_clampf(flourishing, 0.0f, 1.0f);

    /* Check balance */
    bridge->last_assessment.balance_achieved = bridge->balance_signal > bridge->config.balance_threshold;
    if (bridge->last_assessment.balance_achieved) {
        bridge->stats.balance_achievements++;
    }

    /* Check flourishing threshold */
    if (bridge->last_assessment.flourishing_score > bridge->config.flourishing_threshold) {
        bridge->stats.flourishing_detections++;
    }

    /* Check stress threshold */
    if (bridge->last_assessment.stress_level > bridge->config.stress_threshold) {
        bridge->last_assessment.stress_detected = true;
        bridge->stats.stress_detections++;

        if (bridge->stress_callback) {
            bridge->stress_callback(bridge, bridge->last_assessment.stress_level,
                                   bridge->current_time_us, bridge->stress_callback_data);
        }
    } else {
        bridge->last_assessment.stress_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = WELLBEING_SNN_STATE_IDLE;

    /* Invoke assessment callback */
    if (bridge->assessment_callback) {
        bridge->assessment_callback(bridge, &bridge->last_assessment, bridge->assessment_callback_data);
    }

    /* Invoke balance callback if changed */
    if (bridge->balance_callback) {
        bridge->balance_callback(bridge, bridge->balance_signal,
                                bridge->last_assessment.balance_achieved, bridge->balance_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_snn_step(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_step", 0.0f);


    return wellbeing_snn_simulate(bridge, bridge->config.dt_ms);
}

int wellbeing_snn_forward(
    wellbeing_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_forwar", 0.0f);


    int spike_count = wellbeing_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_snn_forward: validation failed");
        return -1;
    }

    if (wellbeing_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int wellbeing_snn_get_assessment(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_assessment_t* assessment
) {
    if (!bridge || !assessment) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_get_assessment: required parameter is NULL (bridge, assessment)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *assessment = bridge->last_assessment;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_snn_get_activations(
    wellbeing_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool wellbeing_snn_check_stress(
    wellbeing_snn_bridge_t* bridge,
    float* stress_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_assessment.stress_level;
    if (stress_level) {
        *stress_level = level;
    }
    bool detected = level > bridge->config.stress_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool wellbeing_snn_check_flourishing(
    wellbeing_snn_bridge_t* bridge,
    float* flourishing_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_assessment.flourishing_score;
    if (flourishing_level) {
        *flourishing_level = level;
    }
    bool detected = level > bridge->config.flourishing_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool wellbeing_snn_check_balance(
    wellbeing_snn_bridge_t* bridge,
    float* balance_score
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_check_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float score = bridge->balance_signal;
    if (balance_score) {
        *balance_score = score;
    }
    bool achieved = bridge->last_assessment.balance_achieved;
    nimcp_mutex_unlock(bridge->base.mutex);

    return achieved;
}

//=============================================================================
// State Query Functions
//=============================================================================

int wellbeing_snn_get_dim_state(
    wellbeing_snn_bridge_t* bridge,
    uint32_t dim,
    wellbeing_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "wellbeing_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_snn_get_state(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_wellbeing = bridge->last_assessment.flourishing_score;
    state->stress_signal = bridge->stress_signal;
    state->balance_signal = bridge->balance_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
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

int wellbeing_snn_get_stats(wellbeing_snn_bridge_t* bridge, wellbeing_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_snn_reset_stats(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(wellbeing_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float wellbeing_snn_get_flourishing(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_fl", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float flourishing = bridge->last_assessment.flourishing_score;
    nimcp_mutex_unlock(bridge->base.mutex);

    return flourishing;
}

float wellbeing_snn_get_total_activity(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_get_to", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            wellbeing_snn_bridge_heartbeat("wellbeing_sn_loop",
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

int wellbeing_snn_register_stress_callback(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_stress_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_register_stress_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stress_callback = callback;
    bridge->stress_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_snn_register_assessment_callback(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_assessment_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_register_assessment_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->assessment_callback = callback;
    bridge->assessment_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_snn_register_balance_callback(
    wellbeing_snn_bridge_t* bridge,
    wellbeing_snn_balance_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_register_balance_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->balance_callback = callback;
    bridge->balance_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int wellbeing_snn_bio_async_connect(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int wellbeing_snn_bio_async_disconnect(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wellbeing_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool wellbeing_snn_is_bio_async_connected(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    wellbeing_snn_bridge_heartbeat("wellbeing_sn_wellbeing_snn_is_bio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}


void wellbeing_snn_bridge_set_instance_health_agent(wellbeing_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "wellbeing_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_snn_bridge_training_begin(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    wellbeing_snn_bridge_heartbeat_instance(bridge->health_agent, "wellbeing_snn_bridge_training_begin", 0.0f);
    return 0;
}

int wellbeing_snn_bridge_training_end(wellbeing_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_snn_bridge_training_end: NULL argument");
        return -1;
    }
    wellbeing_snn_bridge_heartbeat_instance(bridge->health_agent, "wellbeing_snn_bridge_training_end", 1.0f);
    return 0;
}

int wellbeing_snn_bridge_training_step(wellbeing_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_snn_bridge_heartbeat_instance(bridge->health_agent, "wellbeing_snn_bridge_training_step", progress);
    return 0;
}
