/**
 * @file nimcp_gw_snn_bridge.c
 * @brief Global Workspace - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/global_workspace/nimcp_gw_snn_bridge.h"
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
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gw_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_gw_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_gw_snn_bridge_mesh_registry = NULL;

nimcp_error_t gw_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_gw_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "gw_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "gw_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_gw_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_gw_snn_bridge_mesh_registry = registry;
    return err;
}

void gw_snn_bridge_mesh_unregister(void) {
    if (g_gw_snn_bridge_mesh_registry && g_gw_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_gw_snn_bridge_mesh_registry, g_gw_snn_bridge_mesh_id);
        g_gw_snn_bridge_mesh_id = 0;
        g_gw_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from gw_snn_bridge module (instance-level) */
static inline void gw_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_gw_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_gw_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_gw_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "GW_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct gw_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    gw_snn_config_t config;
    snn_network_t* snn;

    /* State */
    gw_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    gw_dim_state_t dim_states[GW_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* access_buffer;

    /* Conscious access state */
    gw_conscious_access_t last_access;
    float ignition_signal;
    float broadcast_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    gw_snn_ignition_callback_t ignition_callback;
    void* ignition_callback_data;
    gw_snn_conscious_callback_t conscious_callback;
    void* conscious_callback_data;
    gw_snn_broadcast_callback_t broadcast_callback;
    void* broadcast_callback_data;

    /* Statistics */
    gw_snn_stats_t stats;
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
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

gw_snn_config_t gw_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_config_defaul", 0.0f);


    gw_snn_config_t config = {
        .num_dimensions = GW_DIM_COUNT,
        .neurons_per_dim = GW_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = GW_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = GW_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = GW_SNN_DECODE_INTEGRATION,
        .ignition_threshold = GW_SNN_IGNITION_THRESH,
        .broadcast_threshold = 0.5f,
        .consciousness_threshold = 0.6f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_binding = true,
        .binding_threshold = 0.4f,

        .enable_access_consciousness = true,
        .access_gain = 1.5f,
        .enable_broadcast_detection = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

gw_snn_bridge_t* gw_snn_create(const gw_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_create", 0.0f);


    gw_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(gw_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = gw_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > GW_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_create: operation failed");
        return NULL;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "gw_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "gw_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* broadcast, ignition, competition, integration, binding, consciousness */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gw_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->access_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->access_buffer || !bridge->prev_state) {
        gw_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gw_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize access to neutral */
    bridge->last_access.broadcast_strength = 0.0f;
    bridge->last_access.ignition_level = 0.0f;
    bridge->last_access.competition_result = 0.0f;
    bridge->last_access.integration_score = 0.5f;
    bridge->last_access.access_consciousness = 0.5f;
    bridge->last_access.ignition_detected = false;
    bridge->last_access.broadcast_active = false;
    bridge->last_access.binding_strength = 0.0f;
    bridge->last_access.coalition_strength = 0.0f;
    bridge->last_access.attention_winner = 0.0f;

    bridge->state = GW_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->ignition_signal = 0.0f;
    bridge->broadcast_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "gw_snn");
    return bridge;
}

void gw_snn_destroy(gw_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "gw_snn");

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_destroy", 0.0f);


    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->access_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int gw_snn_reset(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset access */
    memset(&bridge->last_access, 0, sizeof(gw_conscious_access_t));
    bridge->last_access.integration_score = 0.5f;
    bridge->last_access.access_consciousness = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->access_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = GW_SNN_STATE_IDLE;
    bridge->ignition_signal = 0.0f;
    bridge->broadcast_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int gw_snn_encode_state(
    gw_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_snn_encode_state: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_encode_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GW_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
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
                gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
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

    /* Detect broadcast change */
    float change_magnitude = 0.0f;
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float diff = dimensions[d] - bridge->prev_state[d];
        change_magnitude += diff * diff;
        bridge->prev_state[d] = dimensions[d];
    }
    change_magnitude = (num_dims > 0) ? sqrtf(change_magnitude / num_dims) : 0.0f;

    /* Check for ignition cascade */
    if (dimensions[GW_DIM_IGNITION] > bridge->config.ignition_threshold) {
        bridge->last_access.ignition_detected = true;
        bridge->stats.ignition_events++;
    } else {
        bridge->last_access.ignition_detected = false;
    }

    bridge->stats.total_spikes += total_spikes;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int gw_snn_encode_broadcast(
    gw_snn_bridge_t* bridge,
    float broadcast_strength,
    uint32_t source_module
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_encode_broadcast: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_encode_broadc", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[GW_DIM_COUNT] = {0};
    dims[GW_DIM_BROADCAST] = nimcp_clampf(broadcast_strength, 0.0f, 1.0f);
    dims[GW_DIM_CONSCIOUS_CONTENT] = broadcast_strength * 0.8f;

    bridge->broadcast_signal = broadcast_strength;

    if (broadcast_strength > bridge->config.broadcast_threshold) {
        bridge->last_access.broadcast_active = true;
        bridge->stats.broadcast_events++;

        if (bridge->broadcast_callback) {
            bridge->broadcast_callback(bridge, broadcast_strength, source_module,
                                       bridge->broadcast_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return gw_snn_encode_state(bridge, dims, 2);
}

int gw_snn_encode_competition(
    gw_snn_bridge_t* bridge,
    float competition_strength,
    uint32_t num_competitors
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_encode_competition: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_encode_compet", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[GW_DIM_COUNT] = {0};
    dims[GW_DIM_COMPETITION] = nimcp_clampf(competition_strength, 0.0f, 1.0f);
    dims[GW_DIM_COALITION_STRENGTH] = nimcp_clampf((float)num_competitors / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return gw_snn_encode_state(bridge, dims, 2);
}

int gw_snn_encode_ignition(
    gw_snn_bridge_t* bridge,
    float ignition_strength,
    uint32_t cascade_depth
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_encode_ignition: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_encode_igniti", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float dims[GW_DIM_COUNT] = {0};
    dims[GW_DIM_IGNITION] = nimcp_clampf(ignition_strength, 0.0f, 1.0f);
    dims[GW_DIM_ACCESS_CONSCIOUSNESS] = nimcp_clampf(ignition_strength * 0.9f, 0.0f, 1.0f);

    bridge->ignition_signal = ignition_strength;

    if (ignition_strength > bridge->config.ignition_threshold) {
        bridge->last_access.ignition_detected = true;
        bridge->stats.ignition_events++;

        if (bridge->ignition_callback) {
            bridge->ignition_callback(bridge, ignition_strength,
                                      bridge->current_time_us, bridge->ignition_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return gw_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int gw_snn_simulate(gw_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_snn_simulate: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GW_SNN_STATE_SIMULATING;

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
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
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
                gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
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
    bridge->last_access.broadcast_strength = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_access.ignition_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_access.competition_result = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_access.integration_score = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_access.binding_strength = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_access.access_consciousness = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check ignition threshold */
    if (bridge->last_access.ignition_level > bridge->config.ignition_threshold) {
        bridge->last_access.ignition_detected = true;
        bridge->stats.ignition_events++;

        if (bridge->ignition_callback) {
            bridge->ignition_callback(bridge, bridge->last_access.ignition_level,
                                      bridge->current_time_us, bridge->ignition_callback_data);
        }
    }

    /* Check broadcast threshold */
    if (bridge->last_access.broadcast_strength > bridge->config.broadcast_threshold) {
        bridge->last_access.broadcast_active = true;
        bridge->stats.broadcast_events++;
    }

    /* Check binding threshold */
    if (bridge->last_access.binding_strength > bridge->config.binding_threshold) {
        bridge->stats.binding_events++;
    }

    bridge->stats.total_evaluations++;
    bridge->state = GW_SNN_STATE_IDLE;

    /* Invoke conscious access callback */
    if (bridge->conscious_callback) {
        bridge->conscious_callback(bridge, &bridge->last_access, bridge->conscious_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gw_snn_step(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_step: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_step", 0.0f);


    return gw_snn_simulate(bridge, bridge->config.dt_ms);
}

int gw_snn_forward(
    gw_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_forward", 0.0f);


    int spike_count = gw_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_snn_forward: validation failed");
        return -1;
    }

    if (gw_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int gw_snn_get_conscious_access(
    gw_snn_bridge_t* bridge,
    gw_conscious_access_t* access
) {
    if (!bridge || !access) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_get_conscious_access: required parameter is NULL (bridge, access)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_conscious", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *access = bridge->last_access;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_snn_get_activations(
    gw_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_snn_get_activations: num_dims is zero");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_activatio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool gw_snn_check_ignition(
    gw_snn_bridge_t* bridge,
    float* ignition_level
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_check_ignitio", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_access.ignition_level;
    if (ignition_level) {
        *ignition_level = level;
    }
    bool detected = level > bridge->config.ignition_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool gw_snn_check_broadcast(
    gw_snn_bridge_t* bridge,
    float* broadcast_strength
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_check_broadca", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float strength = bridge->last_access.broadcast_strength;
    if (broadcast_strength) {
        *broadcast_strength = strength;
    }
    bool active = strength > bridge->config.broadcast_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return active;
}

bool gw_snn_check_binding(
    gw_snn_bridge_t* bridge,
    float* binding_strength
) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_check_binding", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float strength = bridge->last_access.binding_strength;
    if (binding_strength) {
        *binding_strength = strength;
    }
    bool detected = strength > bridge->config.binding_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

//=============================================================================
// State Query Functions
//=============================================================================

int gw_snn_get_dim_state(
    gw_snn_bridge_t* bridge,
    uint32_t dim,
    gw_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gw_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_dim_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_snn_get_state(
    gw_snn_bridge_t* bridge,
    gw_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_state", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_broadcast = bridge->last_access.broadcast_strength;
    state->ignition_signal = bridge->ignition_signal;
    state->consciousness_level = bridge->last_access.access_consciousness;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
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

int gw_snn_get_stats(gw_snn_bridge_t* bridge, gw_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_snn_reset_stats(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(gw_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float gw_snn_get_broadcast_strength(gw_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_broadcast", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float strength = bridge->last_access.broadcast_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return strength;
}

float gw_snn_get_total_activity(gw_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_get_total_act", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            gw_snn_bridge_heartbeat("gw_snn_bridg_loop",
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

int gw_snn_register_ignition_callback(
    gw_snn_bridge_t* bridge,
    gw_snn_ignition_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_register_ignition_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_register_igni", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->ignition_callback = callback;
    bridge->ignition_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_snn_register_conscious_callback(
    gw_snn_bridge_t* bridge,
    gw_snn_conscious_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_register_conscious_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_register_cons", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->conscious_callback = callback;
    bridge->conscious_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_snn_register_broadcast_callback(
    gw_snn_bridge_t* bridge,
    gw_snn_broadcast_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_register_broadcast_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_register_broa", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->broadcast_callback = callback;
    bridge->broadcast_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int gw_snn_bio_async_connect(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_bio_async_con", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int gw_snn_bio_async_disconnect(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_bio_async_dis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool gw_snn_is_bio_async_connected(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gw_snn_bridge_heartbeat("gw_snn_bridg_gw_snn_is_bio_async_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gw_snn_bridge_set_instance_health_agent(gw_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "gw_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gw_snn_bridge_training_begin(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    gw_snn_bridge_heartbeat_instance(bridge->health_agent, "gw_snn_bridge_training_begin", 0.0f);
    return 0;
}

int gw_snn_bridge_training_end(gw_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_snn_bridge_training_end: NULL argument");
        return -1;
    }
    gw_snn_bridge_heartbeat_instance(bridge->health_agent, "gw_snn_bridge_training_end", 1.0f);
    return 0;
}

int gw_snn_bridge_training_step(gw_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gw_snn_bridge_heartbeat_instance(bridge->health_agent, "gw_snn_bridge_training_step", progress);
    return 0;
}
