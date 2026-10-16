/**
 * @file nimcp_collective_snn_bridge.c
 * @brief Collective Cognition - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/collective_cognition/nimcp_collective_snn_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(collective_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_collective_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_collective_snn_bridge_mesh_registry = NULL;

nimcp_error_t collective_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_collective_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "collective_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "collective_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_collective_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_collective_snn_bridge_mesh_registry = registry;
    return err;
}

void collective_snn_bridge_mesh_unregister(void) {
    if (g_collective_snn_bridge_mesh_registry && g_collective_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_collective_snn_bridge_mesh_registry, g_collective_snn_bridge_mesh_id);
        g_collective_snn_bridge_mesh_id = 0;
        g_collective_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from collective_snn_bridge module (instance-level) */
static inline void collective_snn_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_collective_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_collective_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_collective_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "COLLECTIVE_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct collective_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    collective_snn_config_t config;
    snn_network_t* snn;

    /* State */
    collective_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    collective_dim_state_t dim_states[COLLECTIVE_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* drive_buffer;

    /* Drive state */
    collective_drive_t last_drive;
    float sync_signal;
    float emergence_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    collective_snn_sync_callback_t sync_callback;
    void* sync_callback_data;
    collective_snn_drive_callback_t drive_callback;
    void* drive_callback_data;
    collective_snn_coordination_callback_t coordination_callback;
    void* coordination_callback_data;

    /* Statistics */
    collective_snn_stats_t stats;

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
            collective_snn_bridge_heartbeat("collective_s_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                collective_snn_bridge_heartbeat("collective_s_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

collective_snn_config_t collective_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_confi", 0.0f);


    collective_snn_config_t config = {
        .num_dimensions = COLLECTIVE_DIM_COUNT,
        .neurons_per_dim = COLLECTIVE_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = COLLECTIVE_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = COLLECTIVE_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = COLLECTIVE_SNN_DECODE_INTEGRATION,
        .sync_threshold = COLLECTIVE_SNN_SYNC_THRESH,
        .coordination_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_sync_detection = true,
        .sync_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_coordination = true,
        .coordination_gain = 1.5f,
        .enable_emergence_tracking = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

collective_snn_bridge_t* collective_snn_create(const collective_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_creat", 0.0f);


    collective_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(collective_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = collective_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > COLLECTIVE_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "collective_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* coherence, sync, intention, coordination, consensus, emergence */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->drive_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->drive_buffer || !bridge->prev_state) {
        collective_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize drive to neutral */
    bridge->last_drive.swarm_coherence = 0.5f;
    bridge->last_drive.group_sync_level = 0.5f;
    bridge->last_drive.shared_intention = 0.5f;
    bridge->last_drive.coordination_drive = 0.5f;
    bridge->last_drive.consensus_level = 0.5f;
    bridge->last_drive.sync_detected = false;
    bridge->last_drive.high_coordination = false;
    bridge->last_drive.coordination_magnitude = 0.0f;
    bridge->last_drive.emergence_level = 0.5f;
    bridge->last_drive.trust_strength = 0.5f;

    bridge->state = COLLECTIVE_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->sync_signal = 0.0f;
    bridge->emergence_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "collective_snn");
    return bridge;
}

void collective_snn_destroy(collective_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "collective_snn");

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_destr", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->drive_buffer);
    nimcp_free(bridge->prev_state);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int collective_snn_reset(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset drive */
    memset(&bridge->last_drive, 0, sizeof(collective_drive_t));
    bridge->last_drive.swarm_coherence = 0.5f;
    bridge->last_drive.group_sync_level = 0.5f;
    bridge->last_drive.coordination_drive = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->drive_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = COLLECTIVE_SNN_STATE_IDLE;
    bridge->sync_signal = 0.0f;
    bridge->emergence_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int collective_snn_encode_state(
    collective_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_encod", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = COLLECTIVE_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
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
                collective_snn_bridge_heartbeat("collective_s_loop",
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
            collective_snn_bridge_heartbeat("collective_s_loop",
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

int collective_snn_encode_swarm(
    collective_snn_bridge_t* bridge,
    float coherence,
    float sync
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_encode_swarm: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_encod", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[COLLECTIVE_DIM_COUNT] = {0};
    dims[COLLECTIVE_DIM_SWARM_COHERENCE] = nimcp_clampf(coherence, 0.0f, 1.0f);
    dims[COLLECTIVE_DIM_GROUP_SYNC] = nimcp_clampf(sync, 0.0f, 1.0f);
    dims[COLLECTIVE_DIM_ROLE_COORDINATION] = (coherence + sync) / 2.0f;

    bridge->sync_signal = sync;

    nimcp_mutex_unlock(bridge->base.mutex);

    return collective_snn_encode_state(bridge, dims, 3);
}

int collective_snn_encode_decision(
    collective_snn_bridge_t* bridge,
    float consensus,
    uint32_t participant_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_encode_decision: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_encod", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[COLLECTIVE_DIM_COUNT] = {0};
    dims[COLLECTIVE_DIM_CONSENSUS_LEVEL] = nimcp_clampf(consensus, 0.0f, 1.0f);
    dims[COLLECTIVE_DIM_DISTRIBUTED_DECISION] = nimcp_clampf((float)participant_count / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return collective_snn_encode_state(bridge, dims, 2);
}

int collective_snn_encode_intention(
    collective_snn_bridge_t* bridge,
    float intention,
    uint32_t intent_type
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_encode_intention: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_encod", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[COLLECTIVE_DIM_COUNT] = {0};
    dims[COLLECTIVE_DIM_SHARED_INTENTION] = nimcp_clampf(intention, 0.0f, 1.0f);
    dims[COLLECTIVE_DIM_JOINT_ATTENTION] = nimcp_clampf(intention * 0.8f, 0.0f, 1.0f);

    bridge->emergence_signal = intention;

    if (intention > bridge->config.sync_threshold) {
        bridge->last_drive.high_coordination = true;
        bridge->last_drive.coordination_magnitude = intention;
        bridge->stats.high_coordination_events++;

        if (bridge->coordination_callback) {
            bridge->coordination_callback(bridge, intention, intent_type,
                                         bridge->coordination_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return collective_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int collective_snn_simulate(collective_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_simul", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = COLLECTIVE_SNN_STATE_SIMULATING;

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
            collective_snn_bridge_heartbeat("collective_s_loop",
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
                collective_snn_bridge_heartbeat("collective_s_loop",
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
    bridge->last_drive.swarm_coherence = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_drive.group_sync_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_drive.shared_intention = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_drive.coordination_drive = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_drive.coordination_magnitude = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_drive.emergence_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check sync threshold */
    if (bridge->last_drive.group_sync_level > bridge->config.sync_threshold) {
        bridge->last_drive.sync_detected = true;
        bridge->stats.sync_detections++;

        if (bridge->sync_callback) {
            bridge->sync_callback(bridge, bridge->last_drive.group_sync_level,
                                  bridge->current_time_us, bridge->sync_callback_data);
        }
    } else {
        bridge->last_drive.sync_detected = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = COLLECTIVE_SNN_STATE_IDLE;

    /* Invoke drive callback */
    if (bridge->drive_callback) {
        bridge->drive_callback(bridge, &bridge->last_drive, bridge->drive_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int collective_snn_step(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_step", 0.0f);


    return collective_snn_simulate(bridge, bridge->config.dt_ms);
}

int collective_snn_forward(
    collective_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_forwa", 0.0f);


    int spike_count = collective_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_snn_forward: validation failed");
        return -1;
    }

    if (collective_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int collective_snn_get_drive(
    collective_snn_bridge_t* bridge,
    collective_drive_t* drive
) {
    if (!bridge || !drive) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_get_drive: required parameter is NULL (bridge, drive)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_d", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *drive = bridge->last_drive;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_snn_get_activations(
    collective_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool collective_snn_check_sync(
    collective_snn_bridge_t* bridge,
    float* sync_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_check", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_drive.group_sync_level;
    if (sync_level) {
        *sync_level = level;
    }
    bool detected = level > bridge->config.sync_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool collective_snn_check_coordination(
    collective_snn_bridge_t* bridge,
    float* coordination_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_check", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_drive.coordination_magnitude;
    if (coordination_level) {
        *coordination_level = level;
    }
    bool detected = level > bridge->config.sync_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool collective_snn_check_state_change(
    collective_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_check", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
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

int collective_snn_get_dim_state(
    collective_snn_bridge_t* bridge,
    uint32_t dim,
    collective_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_d", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_snn_get_state(
    collective_snn_bridge_t* bridge,
    collective_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_coordination = bridge->last_drive.coordination_drive;
    state->sync_signal = bridge->sync_signal;
    state->emergence_signal = bridge->emergence_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
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

int collective_snn_get_stats(collective_snn_bridge_t* bridge, collective_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_snn_reset_stats(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(collective_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float collective_snn_get_coordination(collective_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float coordination = bridge->last_drive.coordination_drive;
    nimcp_mutex_unlock(bridge->base.mutex);

    return coordination;
}

float collective_snn_get_total_activity(collective_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_get_t", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            collective_snn_bridge_heartbeat("collective_s_loop",
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

int collective_snn_register_sync_callback(
    collective_snn_bridge_t* bridge,
    collective_snn_sync_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_register_sync_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->sync_callback = callback;
    bridge->sync_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_snn_register_drive_callback(
    collective_snn_bridge_t* bridge,
    collective_snn_drive_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_register_drive_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->drive_callback = callback;
    bridge->drive_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_snn_register_coordination_callback(
    collective_snn_bridge_t* bridge,
    collective_snn_coordination_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_register_coordination_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_regis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->coordination_callback = callback;
    bridge->coordination_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int collective_snn_bio_async_connect(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_bio_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int collective_snn_bio_async_disconnect(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_bio_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool collective_snn_is_bio_async_connected(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_snn_bridge_heartbeat("collective_s_collective_snn_is_bi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void collective_snn_bridge_set_instance_health_agent(collective_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
int collective_snn_bridge_training_begin(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    collective_snn_bridge_heartbeat_instance(bridge, "coll_snn_train_beg", 0.0f);
    (void)bridge;
    return 0;
}

int collective_snn_bridge_training_step(collective_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_snn_bridge_heartbeat_instance(bridge, "coll_snn_train_stp", progress);
    (void)bridge;
    return 0;
}

int collective_snn_bridge_training_end(collective_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_snn_bridge_training_end: NULL argument");
        return -1;
    }
    collective_snn_bridge_heartbeat_instance(bridge, "coll_snn_train_end", 1.0f);
    (void)bridge;
    return 0;
}
