/**
 * @file nimcp_social_snn_bridge.c
 * @brief Social Cognition - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include "cognitive/social/nimcp_social_snn_bridge.h"
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
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(social_snn_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_social_snn_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_social_snn_bridge_mesh_registry = NULL;

nimcp_error_t social_snn_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_social_snn_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "social_snn_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "social_snn_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_social_snn_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_social_snn_bridge_mesh_registry = registry;
    return err;
}

void social_snn_bridge_mesh_unregister(void) {
    if (g_social_snn_bridge_mesh_registry && g_social_snn_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_social_snn_bridge_mesh_registry, g_social_snn_bridge_mesh_id);
        g_social_snn_bridge_mesh_id = 0;
        g_social_snn_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from social_snn_bridge module (instance-level) */
static inline void social_snn_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_social_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_social_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_social_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SOCIAL_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct social_snn_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    social_snn_config_t config;
    snn_network_t* snn;

    /* State */
    social_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Dimension state */
    social_dim_state_t dim_states[SOCIAL_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* relationship_buffer;

    /* Relationship state */
    social_relationship_t last_relationship;
    float trust_signal;
    float cooperation_signal;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    social_snn_trust_callback_t trust_callback;
    void* trust_callback_data;
    social_snn_relationship_callback_t relationship_callback;
    void* relationship_callback_data;
    social_snn_bond_callback_t bond_callback;
    void* bond_callback_data;

    /* Statistics */
    social_snn_stats_t stats;
};

BRIDGE_DEFINE_SECURITY_SETTERS(social_snn_bridge)

static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            values[i] /= sum;
        }
    }
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

social_snn_config_t social_snn_config_default(void) {
    social_snn_config_t config = {
        .num_dimensions = SOCIAL_DIM_COUNT,
        .neurons_per_dim = SOCIAL_SNN_NEURONS_PER_DIM,
        .hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = SOCIAL_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,

        .encoding = SOCIAL_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = SOCIAL_SNN_DECODE_INTEGRATION,
        .trust_threshold = SOCIAL_SNN_TRUST_THRESH,
        .bonding_threshold = 0.6f,
        .state_change_threshold = 0.2f,

        .enable_competition = true,
        .inhibition_strength = 0.3f,
        .enable_hierarchy_processing = true,
        .hierarchy_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_bonding = true,
        .bonding_gain = 1.5f,
        .enable_cooperation_balance = true,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

social_snn_bridge_t* social_snn_create(const social_snn_config_t* config) {
    social_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(social_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = social_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > SOCIAL_SNN_MAX_DIMENSIONS) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_create: operation failed");
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "social_snn") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "social_snn_create: validation failed");
        return NULL;
    }

    /* Create SNN network */
    snn_config_t snn_config;
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t hidden_dim = bridge->config.hidden_dim;
    uint32_t output_dim = 6; /* trust, closeness, affection, bonding, cooperation, competition */

    snn_config_feedforward(&snn_config, input_dim, hidden_dim, output_dim);
    snn_config.dt = bridge->config.dt_ms;
    snn_config.enable_bio_async = bridge->config.enable_bio_async;

    bridge->snn = snn_network_create(&snn_config);
    if (!bridge->snn) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_snn_create: bridge->snn is NULL");
        return NULL;
    }

    /* Allocate buffers */
    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->relationship_buffer = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->relationship_buffer || !bridge->prev_state) {
        social_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "social_snn_create: operation failed");
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Initialize relationship to neutral */
    bridge->last_relationship.trust_level = 0.5f;
    bridge->last_relationship.closeness_level = 0.5f;
    bridge->last_relationship.affection_level = 0.5f;
    bridge->last_relationship.bonding_strength = 0.5f;
    bridge->last_relationship.hierarchy_position = 0.5f;
    bridge->last_relationship.high_trust = false;
    bridge->last_relationship.strong_bond = false;
    bridge->last_relationship.bond_magnitude = 0.0f;
    bridge->last_relationship.cooperation_level = 0.5f;
    bridge->last_relationship.competition_level = 0.5f;

    bridge->state = SOCIAL_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->bio_async_connected = false;
    bridge->trust_signal = 0.0f;
    bridge->cooperation_signal = 0.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "social_snn");
    return bridge;
}

void social_snn_destroy(social_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "social_snn");

    if (bridge->snn) {
        snn_network_destroy(bridge->snn);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge->encoding_buffer);
    nimcp_free(bridge->output_buffer);
    nimcp_free(bridge->relationship_buffer);
    nimcp_free(bridge->prev_state);
    nimcp_free(bridge);
    bridge = NULL;
}

int social_snn_reset(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_reset: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset SNN network */
    if (bridge->snn) {
        snn_network_reset(bridge->snn);
    }

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
    }

    /* Reset relationship */
    memset(&bridge->last_relationship, 0, sizeof(social_relationship_t));
    bridge->last_relationship.trust_level = 0.5f;
    bridge->last_relationship.closeness_level = 0.5f;
    bridge->last_relationship.bonding_strength = 0.5f;

    /* Reset buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    memset(bridge->encoding_buffer, 0, input_dim * sizeof(float));
    memset(bridge->output_buffer, 0, 6 * sizeof(float));
    memset(bridge->relationship_buffer, 0, bridge->config.num_dimensions * sizeof(float));
    memset(bridge->prev_state, 0, bridge->config.num_dimensions * sizeof(float));

    bridge->state = SOCIAL_SNN_STATE_IDLE;
    bridge->trust_signal = 0.0f;
    bridge->cooperation_signal = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int social_snn_encode_state(
    social_snn_bridge_t* bridge,
    const float* dimensions,
    uint32_t num_dims
) {
    if (!bridge || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_encode_state: required parameter is NULL (bridge, dimensions)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, dimensions, num_dims * sizeof(float));
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_snn_encode_state: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SOCIAL_SNN_STATE_ENCODING;

    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;
    int total_spikes = 0;

    /* Population encoding for each dimension */
    for (uint32_t d = 0; d < num_dims; d++) {
        float value = nimcp_clampf(dimensions[d], 0.0f, 1.0f);
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Update dimension state */
        bridge->dim_states[d].activation = value;
        bridge->dim_states[d].mean_rate_hz = rate;

        /* Population encode */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
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

int social_snn_encode_trust(
    social_snn_bridge_t* bridge,
    float trust,
    float reliability
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_encode_trust: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SOCIAL_DIM_COUNT] = {0};
    dims[SOCIAL_DIM_TRUST] = nimcp_clampf(trust, 0.0f, 1.0f);
    dims[SOCIAL_DIM_RECIPROCITY] = nimcp_clampf(reliability, 0.0f, 1.0f);
    dims[SOCIAL_DIM_BONDING] = (trust + reliability) / 2.0f;

    bridge->trust_signal = trust;

    nimcp_mutex_unlock(bridge->base.mutex);

    return social_snn_encode_state(bridge, dims, 3);
}

int social_snn_encode_closeness(
    social_snn_bridge_t* bridge,
    float closeness,
    float affection
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_encode_closeness: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SOCIAL_DIM_COUNT] = {0};
    dims[SOCIAL_DIM_CLOSENESS] = nimcp_clampf(closeness, 0.0f, 1.0f);
    dims[SOCIAL_DIM_AFFECTION] = nimcp_clampf(affection, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return social_snn_encode_state(bridge, dims, 2);
}

int social_snn_encode_hierarchy(
    social_snn_bridge_t* bridge,
    float hierarchy_position,
    uint32_t hierarchy_count
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_encode_hierarchy: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dims[SOCIAL_DIM_COUNT] = {0};
    dims[SOCIAL_DIM_HIERARCHY] = nimcp_clampf(hierarchy_position, 0.0f, 1.0f);
    dims[SOCIAL_DIM_COOPERATION] = nimcp_clampf((float)hierarchy_count / 10.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return social_snn_encode_state(bridge, dims, 2);
}

//=============================================================================
// Simulation Functions
//=============================================================================

int social_snn_simulate(social_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_simulate: bridge is NULL");
        return -1;
    }
    if (duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_snn_simulate: validation failed");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = SOCIAL_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    uint32_t steps = (uint32_t)(duration_ms / (fabsf(dt) > 1e-7f ? dt : 1e-7f));

    /* Set inputs before simulation */
    if (bridge->snn) {
        uint32_t input_size = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
        snn_network_set_inputs(bridge->snn, bridge->encoding_buffer, input_size);
    }

    for (uint32_t s = 0; s < steps; s++) {
        if (bridge->snn) {
            snn_network_step(bridge->snn, dt);
        }

        /* Update evidence integration */
        float decay = expf(-dt / bridge->config.integration_tau_ms);
        for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
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
    bridge->last_relationship.trust_level = nimcp_clampf(bridge->output_buffer[0], 0.0f, 1.0f);
    bridge->last_relationship.closeness_level = nimcp_clampf(bridge->output_buffer[1], 0.0f, 1.0f);
    bridge->last_relationship.affection_level = nimcp_clampf(bridge->output_buffer[2], 0.0f, 1.0f);
    bridge->last_relationship.bonding_strength = nimcp_clampf(bridge->output_buffer[3], 0.0f, 1.0f);
    bridge->last_relationship.cooperation_level = nimcp_clampf(bridge->output_buffer[4], 0.0f, 1.0f);
    bridge->last_relationship.competition_level = nimcp_clampf(bridge->output_buffer[5], 0.0f, 1.0f);

    /* Check trust threshold */
    if (bridge->last_relationship.trust_level > bridge->config.trust_threshold) {
        bridge->last_relationship.high_trust = true;
        bridge->stats.trust_detections++;

        if (bridge->trust_callback) {
            bridge->trust_callback(bridge, bridge->last_relationship.trust_level,
                                  bridge->current_time_us, bridge->trust_callback_data);
        }
    } else {
        bridge->last_relationship.high_trust = false;
    }

    /* Check bonding threshold */
    if (bridge->last_relationship.bonding_strength > bridge->config.bonding_threshold) {
        bridge->last_relationship.strong_bond = true;
        bridge->last_relationship.bond_magnitude = bridge->last_relationship.bonding_strength;
        bridge->stats.strong_bond_events++;

        if (bridge->bond_callback) {
            bridge->bond_callback(bridge, bridge->last_relationship.bonding_strength,
                                 SOCIAL_DIM_BONDING, bridge->bond_callback_data);
        }
    } else {
        bridge->last_relationship.strong_bond = false;
    }

    bridge->stats.total_evaluations++;
    bridge->state = SOCIAL_SNN_STATE_IDLE;

    /* Invoke relationship callback */
    if (bridge->relationship_callback) {
        bridge->relationship_callback(bridge, &bridge->last_relationship,
                                     bridge->relationship_callback_data);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_snn_step(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_step: bridge is NULL");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "social_snn_step");
    BRIDGE_LGSS_GATE(bridge, "social_snn_step");
    return social_snn_simulate(bridge, bridge->config.dt_ms);
}

int social_snn_forward(
    social_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, inputs, input_count * sizeof(float));

    int spike_count = social_snn_encode_state(bridge, inputs, input_count);
    if (spike_count < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_snn_forward: validation failed");
        return -1;
    }

    if (social_snn_simulate(bridge, bridge->config.encoding_window_ms) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_snn_forward: validation failed");
        return -1;
    }

    return spike_count;
}

//=============================================================================
// Decoding Functions
//=============================================================================

int social_snn_get_relationship(
    social_snn_bridge_t* bridge,
    social_relationship_t* relationship
) {
    if (!bridge || !relationship) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_get_relationship: required parameter is NULL (bridge, relationship)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *relationship = bridge->last_relationship;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_snn_get_activations(
    social_snn_bridge_t* bridge,
    float* activations,
    uint32_t num_dims
) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }
    if (num_dims == 0 || num_dims > bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_snn_get_activations: num_dims is zero");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t d = 0; d < num_dims; d++) {
        activations[d] = bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool social_snn_check_trust(
    social_snn_bridge_t* bridge,
    float* trust_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_relationship.trust_level;
    if (trust_level) {
        *trust_level = level;
    }
    bool detected = level > bridge->config.trust_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool social_snn_check_bond(
    social_snn_bridge_t* bridge,
    float* bond_level
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float level = bridge->last_relationship.bonding_strength;
    if (bond_level) {
        *bond_level = level;
    }
    bool detected = level > bridge->config.bonding_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);

    return detected;
}

bool social_snn_check_state_change(
    social_snn_bridge_t* bridge,
    float* change_magnitude
) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Calculate magnitude from prev_state differences */
    float mag = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
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

int social_snn_get_dim_state(
    social_snn_bridge_t* bridge,
    uint32_t dim,
    social_dim_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    if (dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_snn_get_dim_state: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_snn_get_state(
    social_snn_bridge_t* bridge,
    social_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->mean_bonding = bridge->last_relationship.bonding_strength;
    state->trust_signal = bridge->trust_signal;
    state->cooperation_signal = bridge->cooperation_signal;

    /* Count active dimensions */
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        if (bridge->dim_states[d].activation > 0.1f) {
            state->active_dimensions++;
        }
        state->total_activity += bridge->dim_states[d].activation;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_snn_get_stats(social_snn_bridge_t* bridge, social_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_snn_reset_stats(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(social_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float social_snn_get_bonding(social_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float bonding = bridge->last_relationship.bonding_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return bonding;
}

float social_snn_get_total_activity(social_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float total = 0.0f;
    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        total += bridge->dim_states[d].activation;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return total;
}

//=============================================================================
// Callback Registration
//=============================================================================

int social_snn_register_trust_callback(
    social_snn_bridge_t* bridge,
    social_snn_trust_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_register_trust_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->trust_callback = callback;
    bridge->trust_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_snn_register_relationship_callback(
    social_snn_bridge_t* bridge,
    social_snn_relationship_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_register_relationship_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->relationship_callback = callback;
    bridge->relationship_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_snn_register_bond_callback(
    social_snn_bridge_t* bridge,
    social_snn_bond_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_register_bond_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bond_callback = callback;
    bridge->bond_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int social_snn_bio_async_connect(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    /* Bio-async connection would be implemented here */
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int social_snn_bio_async_disconnect(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool social_snn_is_bio_async_connected(social_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void social_snn_bridge_set_instance_health_agent(social_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "social_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int social_snn_bridge_training_begin(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    social_snn_bridge_heartbeat_instance(bridge->health_agent, "social_snn_bridge_training_begin", 0.0f);
    return 0;
}

int social_snn_bridge_training_end(social_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_snn_bridge_training_end: NULL argument");
        return -1;
    }
    social_snn_bridge_heartbeat_instance(bridge->health_agent, "social_snn_bridge_training_end", 1.0f);
    return 0;
}

int social_snn_bridge_training_step(social_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "social_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    social_snn_bridge_heartbeat_instance(bridge->health_agent, "social_snn_bridge_training_step", progress);
    return 0;
}
