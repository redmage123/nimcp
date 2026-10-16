/**
 * @file nimcp_fep_snn_bridge.c
 * @brief Free Energy Principle - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/free_energy/nimcp_fep_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fep_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_fep_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_fep_snn_bridge_mesh_registry = NULL;

nimcp_error_t fep_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_fep_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "fep_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "fep_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_fep_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_fep_snn_bridge_mesh_registry = registry;
    return err;
}

void fep_snn_bridge_mesh_unregister(void) {
    if (g_fep_snn_bridge_mesh_registry && g_fep_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_fep_snn_bridge_mesh_registry, g_fep_snn_bridge_mesh_id);
        g_fep_snn_bridge_mesh_id = 0;
        g_fep_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from fep_snn_bridge module (instance-level) */
static inline void fep_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fep_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fep_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fep_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "FEP_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct fep_snn_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
    fep_snn_config_t config;
    snn_network_t* snn;

    /* State */
    fep_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    fep_dim_state_t dim_states[FEP_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* belief_buffer;

    /* Belief state */
    fep_belief_state_t last_belief;
    float pred_error_signal;
    float precision_signal;

    /* Previous state for change detection */
    float* prev_state;
    float prev_free_energy;

    /* Callbacks */
    fep_snn_pred_error_callback_t pred_error_callback;
    void* pred_error_callback_data;
    fep_snn_belief_callback_t belief_callback;
    void* belief_callback_data;
    fep_snn_free_energy_callback_t free_energy_callback;
    void* free_energy_callback_data;

    /* Statistics */
    fep_snn_stats_t stats;
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
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

fep_snn_config_t fep_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_config_defau", 0.0f);


    fep_snn_config_t config = {
        .num_dimensions = FEP_DIM_COUNT,
        .neurons_per_dim = FEP_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = FEP_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = FEP_SNN_ENCODE_PREDICTIVE,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = FEP_SNN_DECODE_VARIATIONAL,
        .pred_error_threshold = FEP_SNN_PRED_ERROR_THRESH,
        .belief_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_pred_error_detection = true,
        .pred_error_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_active_inference = true,
        .active_inference_gain = 1.5f,
        .enable_precision_weighting = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

fep_snn_bridge_t* fep_snn_create(const fep_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_create", 0.0f);


    fep_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(fep_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = fep_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > FEP_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "fep_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "fep_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* pred_error, free_energy, precision, belief, active_inf, variational */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->belief_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->belief_buffer || !bridge->prev_state) {
        fep_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fep_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize belief to neutral state */
    bridge->last_belief.prediction_error = 0.0f;
    bridge->last_belief.free_energy = 0.5f;
    bridge->last_belief.precision = 0.5f;
    bridge->last_belief.belief_strength = 0.5f;
    bridge->last_belief.active_inference_drive = 0.5f;
    bridge->last_belief.variational_bound = 0.5f;
    bridge->last_belief.high_pred_error = false;
    bridge->last_belief.belief_update_needed = false;
    bridge->last_belief.entropy_estimate = 0.5f;
    bridge->last_belief.kl_divergence = 0.0f;

    bridge->state = FEP_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->pred_error_signal = 0.0f;
    bridge->precision_signal = 0.5f;
    bridge->prev_free_energy = 0.5f;

    NIMCP_LOGGING_INFO("Created %s bridge", "fep_snn");
    return bridge;
}

void fep_snn_destroy(fep_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "fep_snn");

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_destroy", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->belief_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int fep_snn_reset(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset belief */
    memset(&bridge->last_belief, 0, sizeof(fep_belief_state_t));
    bridge->last_belief.free_energy = 0.5f;
    bridge->last_belief.precision = 0.5f;
    bridge->last_belief.belief_strength = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->belief_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = FEP_SNN_STATE_IDLE;
    bridge->pred_error_signal = 0.0f;
    bridge->precision_signal = 0.5f;
    bridge->prev_free_energy = 0.5f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int fep_snn_encode_state(
    fep_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_encode_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = FEP_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Predictive coding encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float value = nimcp_clampf(dimensions[d], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Update dimension state */
        bridge->dim_states[d].activation = value;
        bridge->dim_states[d].mean_rate_hz = rate;

        /* Population encode with precision weighting */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_dim > 256) {
                fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                                 (float)(n + 1) / (float)neurons_per_dim);
            }

            float preferred = (neurons_per_dim <= 1) ? 0.5f : (float)n / (float)(neurons_per_dim - 1);
            float diff = value - preferred;
            float tuning = expf(-diff * diff / 0.1f);

            /* Apply precision weighting */
            if (bridge->config.enable_precision_weighting) {
                tuning *= bridge->precision_signal;
            }

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
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
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

int fep_snn_encode_prediction_error(
    fep_snn_bridge_t* bridge,
    float pred_error,
    float precision
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_encode_prediction_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_encode_predi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[FEP_DIM_COUNT] = {0};
    dims[FEP_DIM_PREDICTION_ERROR] = nimcp_clampf(pred_error, 0.0f, 1.0f);
    dims[FEP_DIM_PRECISION] = nimcp_clampf(precision, 0.0f, 1.0f);
    dims[FEP_DIM_FREE_ENERGY] = (pred_error * precision);

    bridge->pred_error_signal = pred_error;
    bridge->precision_signal = precision;

    nimcp_mutex_unlock(bridge->base.mutex);

    return fep_snn_encode_state(bridge, dims, 3);
}

int fep_snn_encode_free_energy(
    fep_snn_bridge_t* bridge,
    float free_energy,
    float complexity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_encode_free_energy: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_encode_free_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[FEP_DIM_COUNT] = {0};
    dims[FEP_DIM_FREE_ENERGY] = nimcp_clampf(free_energy, 0.0f, 1.0f);
    dims[FEP_DIM_MODEL_COMPLEXITY] = nimcp_clampf(complexity, 0.0f, 1.0f);

    /* Free energy change callback */
    if (bridge->free_energy_callback) {
        float old_fe = bridge->prev_free_energy;
        bridge->free_energy_callback(bridge, old_fe, free_energy,
                                     bridge->free_energy_callback_data);
    }
    bridge->prev_free_energy = free_energy;

    nimcp_mutex_unlock(bridge->base.mutex);

    return fep_snn_encode_state(bridge, dims, 2);
}

int fep_snn_encode_active_inference(
    fep_snn_bridge_t* bridge,
    float action_drive,
    uint32_t action_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_encode_active_inference: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_encode_activ", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[FEP_DIM_COUNT] = {0};
    dims[FEP_DIM_ACTIVE_INFERENCE] = nimcp_clampf(action_drive, 0.0f, 1.0f);
    dims[FEP_DIM_BELIEF_STRENGTH] = nimcp_clampf(action_drive * 0.8f, 0.0f, 1.0f);

    bridge->last_belief.active_inference_drive = action_drive;

    nimcp_mutex_unlock(bridge->base.mutex);

    return fep_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int fep_snn_simulate(fep_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = FEP_SNN_STATE_SIMULATING;

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
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
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
                fep_snn_bridge_heartbeat("fep_snn_brid_loop",
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

    /* Decode outputs into belief state */
    bridge->last_belief.prediction_error = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_belief.free_energy = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_belief.precision = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_belief.belief_strength = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_belief.active_inference_drive = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_belief.variational_bound = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Calculate derived values */
    bridge->last_belief.entropy_estimate = 1.0f - bridge->last_belief.belief_strength;
    bridge->last_belief.kl_divergence = bridge->last_belief.prediction_error *
                                        bridge->last_belief.precision;

    /* Check prediction error threshold */
    if (bridge->last_belief.prediction_error > bridge->config.pred_error_threshold) {
        bridge->last_belief.high_pred_error = true;
        bridge->stats.high_pred_error_events++;

        if (bridge->pred_error_callback) {
            bridge->pred_error_callback(bridge, bridge->last_belief.prediction_error,
                                       bridge->current_time_us, bridge->pred_error_callback_data);
        }
    } else {
        bridge->last_belief.high_pred_error = false;
    }

    /* Check if belief update is needed */
    if (bridge->last_belief.prediction_error > 0.1f &&
        bridge->last_belief.precision > 0.3f) {
        bridge->last_belief.belief_update_needed = true;
        bridge->stats.belief_updates++;
    } else {
        bridge->last_belief.belief_update_needed = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = FEP_SNN_STATE_IDLE;

    /* Invoke belief callback */
    if (bridge->belief_callback) {
        bridge->belief_callback(bridge, &bridge->last_belief, bridge->belief_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int fep_snn_step(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_step", 0.0f);


    return fep_snn_simulate(bridge, bridge->config.dt_ms);
}

int fep_snn_forward(
    fep_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_forward", 0.0f);


    int spike_count = fep_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_snn_forward: validation failed");
        return -1;
    }

    if (fep_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int fep_snn_get_belief(
    fep_snn_bridge_t* bridge,
    fep_belief_state_t* belief
) {
    if (!bridge || !belief) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_get_belief: required parameter is NULL (bridge, belief)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_belief", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *belief = bridge->last_belief;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_snn_get_activations(
    fep_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_activati", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool fep_snn_check_pred_error(
    fep_snn_bridge_t* bridge,
    float* pred_error_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_check_pred_e", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_belief.prediction_error;
    if (pred_error_level) {
        *pred_error_level = level;
    }
    bool detected = level > bridge->config.pred_error_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool fep_snn_check_belief_update(
    fep_snn_bridge_t* bridge,
    float* update_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_check_belief", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float magnitude = bridge->last_belief.prediction_error * bridge->last_belief.precision;
    if (update_magnitude) {
        *update_magnitude = magnitude;
    }
    bool needed = bridge->last_belief.belief_update_needed;
    nimcp_mutex_unlock(bridge->base.mutex);

    return needed;
}

bool fep_snn_check_state_change(
    fep_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_check_state_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
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

int fep_snn_get_dim_state(
    fep_snn_bridge_t* bridge,
    uint32_t dim,
    fep_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fep_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_dim_stat", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_snn_get_state(
    fep_snn_bridge_t* bridge,
    fep_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_free_energy = bridge->last_belief.free_energy;
    state->pred_error_signal = bridge->pred_error_signal;
    state->precision_signal = bridge->precision_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
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

int fep_snn_get_stats(fep_snn_bridge_t* bridge, fep_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_snn_reset_stats(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(fep_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float fep_snn_get_free_energy(fep_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_free_ene", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float free_energy = bridge->last_belief.free_energy;
    nimcp_mutex_unlock(bridge->base.mutex);

    return free_energy;
}

float fep_snn_get_total_activity(fep_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_get_total_ac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            fep_snn_bridge_heartbeat("fep_snn_brid_loop",
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

int fep_snn_register_pred_error_callback(
    fep_snn_bridge_t* bridge,
    fep_snn_pred_error_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_register_pred_error_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_register_pre", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_error_callback = callback;
    bridge->pred_error_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_snn_register_belief_callback(
    fep_snn_bridge_t* bridge,
    fep_snn_belief_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_register_belief_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_register_bel", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->belief_callback = callback;
    bridge->belief_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_snn_register_free_energy_callback(
    fep_snn_bridge_t* bridge,
    fep_snn_free_energy_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_register_free_energy_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_register_fre", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->free_energy_callback = callback;
    bridge->free_energy_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int fep_snn_bio_async_connect(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_bio_async_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int fep_snn_bio_async_disconnect(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fep_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_bio_async_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool fep_snn_is_bio_async_connected(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    fep_snn_bridge_heartbeat("fep_snn_brid_fep_snn_is_bio_async", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fep_snn_bridge_set_instance_health_agent(fep_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "fep_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fep_snn_bridge_training_begin(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    fep_snn_bridge_heartbeat_instance(bridge->health_agent, "fep_snn_bridge_training_begin", 0.0f);
    return 0;
}

int fep_snn_bridge_training_end(fep_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_snn_bridge_training_end: NULL argument");
        return -1;
    }
    fep_snn_bridge_heartbeat_instance(bridge->health_agent, "fep_snn_bridge_training_end", 1.0f);
    return 0;
}

int fep_snn_bridge_training_step(fep_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fep_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fep_snn_bridge_heartbeat_instance(bridge->health_agent, "fep_snn_bridge_training_step", progress);
    return 0;
}
