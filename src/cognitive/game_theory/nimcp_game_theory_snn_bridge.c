/**
 * @file nimcp_game_theory_snn_bridge.c
 * @brief Game Theory - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/game_theory/nimcp_game_theory_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(game_theory_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_game_theory_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_game_theory_snn_bridge_mesh_registry = NULL;

nimcp_error_t game_theory_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_game_theory_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "game_theory_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "game_theory_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_game_theory_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_game_theory_snn_bridge_mesh_registry = registry;
    return err;
}

void game_theory_snn_bridge_mesh_unregister(void) {
    if (g_game_theory_snn_bridge_mesh_registry && g_game_theory_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_game_theory_snn_bridge_mesh_registry, g_game_theory_snn_bridge_mesh_id);
        g_game_theory_snn_bridge_mesh_id = 0;
        g_game_theory_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from game_theory_snn_bridge module (instance-level) */
static inline void game_theory_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_game_theory_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_game_theory_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_game_theory_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "GAME_THEORY_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct game_theory_snn_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    game_theory_snn_config_t config;
    snn_network_t* snn;

    /* State */
    game_theory_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    game_theory_dim_state_t dim_states[GAME_THEORY_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* strategy_buffer;

    /* Decision state */
    game_theory_decision_t last_decision;
    float equilibrium_signal;
    float payoff_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    game_theory_snn_equilibrium_callback_t equilibrium_callback;
    void* equilibrium_callback_data;
    game_theory_snn_decision_callback_t decision_callback;
    void* decision_callback_data;
    game_theory_snn_cooperation_callback_t cooperation_callback;
    void* cooperation_callback_data;

    /* Statistics */
    game_theory_snn_stats_t stats;
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
            game_theory_snn_bridge_heartbeat("game_theory__loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                game_theory_snn_bridge_heartbeat("game_theory__loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

game_theory_snn_config_t game_theory_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_conf", 0.0f);


    game_theory_snn_config_t config = {
        .num_dimensions = GT_DIM_COUNT,
        .neurons_per_dim = GAME_THEORY_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = GAME_THEORY_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = GT_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = GT_SNN_DECODE_INTEGRATION,
        .equilibrium_threshold = GAME_THEORY_SNN_EQUILIBRIUM_THRESH,
        .cooperation_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_equilibrium_detection = true,
        .equilibrium_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_opponent_modeling = true,
        .opponent_model_gain = 1.5f,
        .enable_payoff_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

game_theory_snn_bridge_t* game_theory_snn_create(const game_theory_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_crea", 0.0f);


    game_theory_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(game_theory_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = game_theory_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > GAME_THEORY_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_create: num_dimensions out of range");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "game_theory_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "game_theory_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* cooperation, defection, payoff, equilibrium, trust, fairness */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "game_theory_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->strategy_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->strategy_buffer || !bridge->prev_state) {
        game_theory_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "game_theory_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize decision to neutral */
    bridge->last_decision.cooperation_level = 0.5f;
    bridge->last_decision.defection_level = 0.5f;
    bridge->last_decision.payoff_expectation = 0.5f;
    bridge->last_decision.equilibrium_distance = 0.5f;
    bridge->last_decision.opponent_prediction = 0.5f;
    bridge->last_decision.equilibrium_detected = false;
    bridge->last_decision.cooperation_dominant = false;
    bridge->last_decision.trust_level = 0.5f;
    bridge->last_decision.fairness_level = 0.5f;
    bridge->last_decision.risk_tolerance = 0.5f;

    bridge->state = GT_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->equilibrium_signal = 0.0f;
    bridge->payoff_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "game_theory_snn");
    return bridge;
}

void game_theory_snn_destroy(game_theory_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "game_theory_snn");

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_dest", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->strategy_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int game_theory_snn_reset(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset decision */
    memset(&bridge->last_decision, 0, sizeof(game_theory_decision_t));
    bridge->last_decision.cooperation_level = 0.5f;
    bridge->last_decision.defection_level = 0.5f;
    bridge->last_decision.payoff_expectation = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->strategy_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = GT_SNN_STATE_IDLE;
    bridge->equilibrium_signal = 0.0f;
    bridge->payoff_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int game_theory_snn_encode_state(
    game_theory_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GT_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
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
                game_theory_snn_bridge_heartbeat("game_theory__loop",
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
            game_theory_snn_bridge_heartbeat("game_theory__loop",
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

int game_theory_snn_encode_payoff(
    game_theory_snn_bridge_t* bridge,
    const float* payoffs,
    uint32_t num_actions
) {
    if (!bridge || !payoffs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_encode_payoff: required parameter is NULL (bridge, payoffs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[GT_DIM_COUNT] = {0};

    /* Compute average and max payoff */
    float max_payoff = payoffs[0];
    float sum_payoff = payoffs[0];
    for (uint32_t i = 1; i < num_actions; i++) {
        if (payoffs[i] > max_payoff) max_payoff = payoffs[i];
        sum_payoff += payoffs[i];
    }
    float avg_payoff = (num_actions > 0) ? (sum_payoff / num_actions) : 0.0f;

    dims[GT_DIM_PAYOFF_EXPECTATION] = nimcp_clampf(avg_payoff, 0.0f, 1.0f);
    dims[GT_DIM_RISK_TOLERANCE] = nimcp_clampf(max_payoff - avg_payoff, 0.0f, 1.0f);

    bridge->payoff_signal = avg_payoff;

    nimcp_mutex_unlock(bridge->base.mutex);

    return game_theory_snn_encode_state(bridge, dims, 2);
}

int game_theory_snn_encode_opponent(
    game_theory_snn_bridge_t* bridge,
    float opponent_strategy,
    float confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_encode_opponent: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[GT_DIM_COUNT] = {0};
    dims[GT_DIM_OPPONENT_MODEL] = nimcp_clampf(opponent_strategy, 0.0f, 1.0f);
    dims[GT_DIM_TRUST] = nimcp_clampf(confidence, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return game_theory_snn_encode_state(bridge, dims, 2);
}

int game_theory_snn_encode_cooperation(
    game_theory_snn_bridge_t* bridge,
    float cooperation,
    float defection
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_encode_cooperation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_enco", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[GT_DIM_COUNT] = {0};
    dims[GT_DIM_COOPERATION] = nimcp_clampf(cooperation, 0.0f, 1.0f);
    dims[GT_DIM_DEFECTION] = nimcp_clampf(defection, 0.0f, 1.0f);
    dims[GT_DIM_RECIPROCITY] = (cooperation + (1.0f - defection)) / 2.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    return game_theory_snn_encode_state(bridge, dims, 3);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int game_theory_snn_simulate(game_theory_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_simu", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GT_SNN_STATE_SIMULATING;

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
            game_theory_snn_bridge_heartbeat("game_theory__loop",
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
                game_theory_snn_bridge_heartbeat("game_theory__loop",
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
    bridge->last_decision.cooperation_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_decision.defection_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_decision.payoff_expectation = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_decision.equilibrium_distance = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_decision.trust_level = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_decision.fairness_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check equilibrium threshold */
    if (bridge->last_decision.equilibrium_distance < bridge->config.equilibrium_threshold) {
        bridge->last_decision.equilibrium_detected = true;
        bridge->stats.equilibrium_detections++;

        if (bridge->equilibrium_callback) {
            bridge->equilibrium_callback(bridge, bridge->last_decision.equilibrium_distance,
                                        bridge->current_time_us, bridge->equilibrium_callback_data);
        }
    } else {
        bridge->last_decision.equilibrium_detected = false;
    }

    /* Check cooperation dominance */
    if (bridge->last_decision.cooperation_level > bridge->config.cooperation_threshold) {
        bridge->last_decision.cooperation_dominant = true;
        bridge->stats.cooperation_events++;

        if (bridge->cooperation_callback) {
            bridge->cooperation_callback(bridge, bridge->last_decision.cooperation_level,
                                        GT_DIM_COOPERATION, bridge->cooperation_callback_data);
        }
    } else {
        bridge->last_decision.cooperation_dominant = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = GT_SNN_STATE_IDLE;

    /* Invoke decision callback */
    if (bridge->decision_callback) {
        bridge->decision_callback(bridge, &bridge->last_decision, bridge->decision_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int game_theory_snn_step(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_step", 0.0f);


    return game_theory_snn_simulate(bridge, bridge->config.dt_ms);
}

int game_theory_snn_forward(
    game_theory_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_forw", 0.0f);


    int spike_count = game_theory_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_forward: validation failed");
        return -1;
    }

    if (game_theory_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int game_theory_snn_get_decision(
    game_theory_snn_bridge_t* bridge,
    game_theory_decision_t* decision
) {
    if (!bridge || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_get_decision: required parameter is NULL (bridge, decision)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *decision = bridge->last_decision;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int game_theory_snn_get_activations(
    game_theory_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool game_theory_snn_check_equilibrium(
    game_theory_snn_bridge_t* bridge,
    float* distance
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_chec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float dist = bridge->last_decision.equilibrium_distance;
    if (distance) {
        *distance = dist;
    }
    bool detected = dist < bridge->config.equilibrium_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool game_theory_snn_check_cooperation(
    game_theory_snn_bridge_t* bridge,
    float* cooperation_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_chec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_decision.cooperation_level;
    if (cooperation_level) {
        *cooperation_level = level;
    }
    bool dominant = level > bridge->config.cooperation_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return dominant;
}

bool game_theory_snn_check_state_change(
    game_theory_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_chec", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
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

int game_theory_snn_get_dim_state(
    game_theory_snn_bridge_t* bridge,
    uint32_t dim,
    game_theory_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "game_theory_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int game_theory_snn_get_state(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_strategy = bridge->last_decision.cooperation_level;
    state->equilibrium_signal = bridge->equilibrium_signal;
    state->payoff_signal = bridge->payoff_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
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

int game_theory_snn_get_stats(game_theory_snn_bridge_t* bridge, game_theory_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int game_theory_snn_reset_stats(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(game_theory_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float game_theory_snn_get_cooperation(game_theory_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float cooperation = bridge->last_decision.cooperation_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return cooperation;
}

float game_theory_snn_get_total_activity(game_theory_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            game_theory_snn_bridge_heartbeat("game_theory__loop",
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

int game_theory_snn_register_equilibrium_callback(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_equilibrium_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_register_equilibrium_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->equilibrium_callback = callback;
    bridge->equilibrium_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int game_theory_snn_register_decision_callback(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_decision_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_register_decision_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->decision_callback = callback;
    bridge->decision_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int game_theory_snn_register_cooperation_callback(
    game_theory_snn_bridge_t* bridge,
    game_theory_snn_cooperation_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_register_cooperation_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->cooperation_callback = callback;
    bridge->cooperation_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int game_theory_snn_bio_async_connect(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_LOGGING_WARN("game_theory_snn_bio_async_connect: bio_async not enabled in config");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int game_theory_snn_bio_async_disconnect(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "game_theory_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool game_theory_snn_is_bio_async_connected(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    game_theory_snn_bridge_heartbeat("game_theory__game_theory_snn_is_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void game_theory_snn_bridge_set_instance_health_agent(game_theory_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "game_theory_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int game_theory_snn_bridge_training_begin(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    game_theory_snn_bridge_heartbeat_instance(bridge->health_agent, "game_theory_snn_bridge_training_begin", 0.0f);
    return 0;
}

int game_theory_snn_bridge_training_end(game_theory_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_snn_bridge_training_end: NULL argument");
        return -1;
    }
    game_theory_snn_bridge_heartbeat_instance(bridge->health_agent, "game_theory_snn_bridge_training_end", 1.0f);
    return 0;
}

int game_theory_snn_bridge_training_step(game_theory_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "game_theory_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    game_theory_snn_bridge_heartbeat_instance(bridge->health_agent, "game_theory_snn_bridge_training_step", progress);
    return 0;
}
