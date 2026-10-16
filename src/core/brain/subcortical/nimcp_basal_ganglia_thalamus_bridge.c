/**
 * @file nimcp_basal_ganglia_thalamus_bridge.c
 * @brief Basal ganglia-thalamus motor relay bridge implementation
 */

#include "core/brain/subcortical/nimcp_basal_ganglia_thalamus_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(basal_ganglia_thalamus_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


#define LOG_MODULE "BASAL_GANGLIA_THALAMUS_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void bgt_bridge_default_config(bgt_bridge_config_t* config) {
    if (!config) return;

    config->num_channels = 16;
    config->relay_gain = BGT_DEFAULT_GAIN;
    config->disinhibition_threshold = BGT_DEFAULT_THRESHOLD;
    config->attention_weight = 0.3f;
    config->urgency_weight = 0.2f;
    config->trn_sensitivity = 0.5f;
    config->output_type = BGT_OUTPUT_DISCRETE;
    config->enable_attention_gating = true;
    config->enable_urgency_boost = true;
    config->enable_burst_detection = false;
}

bgt_bridge_t* bgt_bridge_create(const bgt_bridge_config_t* config) {
    bgt_bridge_t* bridge = nimcp_calloc(1, sizeof(bgt_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgt_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bgt_bridge_default_config(&bridge->config);
    }

    uint32_t nc = bridge->config.num_channels;

    /* Allocate channel mapping */
    bridge->channels = nimcp_calloc(nc, sizeof(bgt_channel_map_t));
    if (!bridge->channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgt_bridge_create: failed to allocate channels");
        goto fail;
    }
    bridge->num_channels = nc;

    /* Allocate buffers */
    bridge->bg_output_buffer = nimcp_calloc(nc, sizeof(float));
    bridge->thal_input_buffer = nimcp_calloc(nc, sizeof(float));
    bridge->motor_output_buffer = nimcp_calloc(nc, sizeof(float));
    if (!bridge->bg_output_buffer || !bridge->thal_input_buffer ||
        !bridge->motor_output_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgt_bridge_create: failed to allocate buffers");
        goto fail;
    }

    /* Initialize state */
    bridge->current_mode = BGT_MODE_NORMAL;
    bridge->current_attention = BGT_DEFAULT_ATTENTION;
    bridge->current_urgency = 0.0f;
    bridge->trn_inhibition = 0.0f;

    /* Create default mapping */
    bgt_bridge_create_default_mapping(bridge);

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "basal_ganglia_thalamus") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "bgt_bridge_create: failed to initialize bridge base");
        goto fail;
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "basal_ganglia_thalamus");
    return bridge;

fail:
    bgt_bridge_destroy(bridge);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgt_bridge_create: operation failed");
    return NULL;
}

void bgt_bridge_destroy(bgt_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "basal_ganglia_thalamus");

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    if (bridge->channels) nimcp_free(bridge->channels);
    if (bridge->bg_output_buffer) nimcp_free(bridge->bg_output_buffer);
    if (bridge->thal_input_buffer) nimcp_free(bridge->thal_input_buffer);
    if (bridge->motor_output_buffer) nimcp_free(bridge->motor_output_buffer);

    nimcp_free(bridge);
}

int bgt_bridge_reset(bgt_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_reset: bridge is NULL");
        return -1;
    }

    bridge->current_mode = BGT_MODE_NORMAL;
    bridge->current_attention = BGT_DEFAULT_ATTENTION;
    bridge->current_urgency = 0.0f;
    bridge->trn_inhibition = 0.0f;

    memset(bridge->bg_output_buffer, 0, bridge->num_channels * sizeof(float));
    memset(bridge->thal_input_buffer, 0, bridge->num_channels * sizeof(float));
    memset(bridge->motor_output_buffer, 0, bridge->num_channels * sizeof(float));

    memset(&bridge->stats, 0, sizeof(bgt_bridge_stats_t));

    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int bgt_bridge_connect_bg(bgt_bridge_t* bridge, basal_ganglia_t* bg) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_connect_bg: bridge is NULL");
        return -1;
    }
    bridge->bg = bg;
    return 0;
}

int bgt_bridge_connect_thalamus(bgt_bridge_t* bridge, thalamus_t* thalamus) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_connect_thalamus: bridge is NULL");
        return -1;
    }
    bridge->thalamus = thalamus;
    return 0;
}

bool bgt_bridge_is_connected(const bgt_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bg != NULL && bridge->thalamus != NULL;
}

/* ============================================================================
 * Channel Mapping Functions
 * ============================================================================ */

int bgt_bridge_set_channel_map(
    bgt_bridge_t* bridge,
    uint32_t bg_action,
    uint32_t thal_channel,
    float weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_set_channel_map: bridge is NULL");
        return -1;
    }
    if (bg_action >= bridge->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgt_bridge_set_channel_map: bg_action out of range");
        return -1;
    }

    bridge->channels[bg_action].bg_action_id = bg_action;
    bridge->channels[bg_action].thal_channel = thal_channel;
    bridge->channels[bg_action].weight = weight;
    bridge->channels[bg_action].is_active = true;

    return 0;
}

int bgt_bridge_create_default_mapping(bgt_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_create_default_mapping: bridge is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_channels; i++) {
        bridge->channels[i].bg_action_id = i;
        bridge->channels[i].thal_channel = i;
        bridge->channels[i].weight = 1.0f;
        bridge->channels[i].is_active = true;
    }

    return 0;
}

float bgt_bridge_get_channel_weight(
    const bgt_bridge_t* bridge,
    uint32_t bg_action
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_get_channel_weight: bridge is NULL");
        return -1.0f;
    }
    if (bg_action >= bridge->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgt_bridge_get_channel_weight: bg_action out of range");
        return -1.0f;
    }
    return bridge->channels[bg_action].weight;
}

/* ============================================================================
 * Relay Functions
 * ============================================================================ */

int bgt_bridge_relay(bgt_bridge_t* bridge, bgt_relay_result_t* result) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_relay: bridge is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_relay: result is NULL");
        return -1;
    }

    /* Get BG output if connected */
    if (bridge->bg) {
        basal_ganglia_get_thalamic_output(bridge->bg, bridge->bg_output_buffer);
    }

    return bgt_bridge_relay_explicit(
        bridge,
        bridge->bg_output_buffer,
        bridge->num_channels,
        result
    );
}

int bgt_bridge_relay_explicit(
    bgt_bridge_t* bridge,
    const float* bg_output,
    uint32_t num_actions,
    bgt_relay_result_t* result
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_relay_explicit: bridge is NULL");
        return -1;
    }
    if (!bg_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_relay_explicit: bg_output is NULL");
        return -1;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_relay_explicit: result is NULL");
        return -1;
    }

    uint32_t nc = bridge->num_channels < num_actions ?
                  bridge->num_channels : num_actions;

    /* Check for suppression mode */
    if (bridge->current_mode == BGT_MODE_SUPPRESSED) {
        memset(bridge->motor_output_buffer, 0, bridge->num_channels * sizeof(float));
        result->motor_output = bridge->motor_output_buffer;
        result->output_size = bridge->num_channels;
        result->selected_action = 0;
        result->selection_confidence = 0.0f;
        result->mode = BGT_MODE_SUPPRESSED;
        bridge->stats.suppressed_relays++;
        return 0;
    }

    /* Copy BG output to thalamic input */
    memcpy(bridge->thal_input_buffer, bg_output, nc * sizeof(float));

    /* Apply attention modulation */
    if (bridge->config.enable_attention_gating) {
        float att_factor = 0.5f + 0.5f * bridge->current_attention;
        for (uint32_t i = 0; i < nc; i++) {
            bridge->thal_input_buffer[i] *= att_factor;
        }
    }

    /* Apply urgency boost */
    if (bridge->config.enable_urgency_boost && bridge->current_urgency > 0.0f) {
        float urgency_boost = 1.0f + bridge->current_urgency * BGT_URGENCY_BOOST_MAX;
        for (uint32_t i = 0; i < nc; i++) {
            bridge->thal_input_buffer[i] *= urgency_boost;
        }
    }

    /* Apply TRN inhibition */
    float trn_factor = 1.0f - bridge->trn_inhibition * bridge->config.trn_sensitivity;
    trn_factor = nimcp_clampf(trn_factor, 0.0f, 1.0f);
    for (uint32_t i = 0; i < nc; i++) {
        bridge->thal_input_buffer[i] *= trn_factor;
    }

    /* Route through thalamus if connected */
    if (bridge->thalamus) {
        thalamus_relay_motor(
            bridge->thalamus,
            bridge->thal_input_buffer,
            nc,
            bridge->motor_output_buffer,
            bridge->num_channels
        );
    } else {
        /* Direct passthrough with gain */
        for (uint32_t i = 0; i < nc; i++) {
            bridge->motor_output_buffer[i] =
                bridge->thal_input_buffer[i] * bridge->config.relay_gain;
        }
    }

    /* Apply threshold for discrete output */
    float max_output = 0.0f;
    uint32_t max_action = 0;
    for (uint32_t i = 0; i < bridge->num_channels; i++) {
        if (bridge->motor_output_buffer[i] > max_output) {
            max_output = bridge->motor_output_buffer[i];
            max_action = i;
        }
    }

    /* Fill result */
    result->motor_output = bridge->motor_output_buffer;
    result->output_size = bridge->num_channels;
    result->selected_action = max_action;
    result->selection_confidence = max_output;
    result->relay_latency_ms = 5.0f;  /* Simulated relay latency */
    result->mode = bridge->current_mode;

    /* Update statistics */
    bridge->stats.total_relays++;
    if (max_output > bridge->config.disinhibition_threshold) {
        bridge->stats.successful_relays++;
    }
    bridge->stats.avg_relay_gain =
        (bridge->stats.avg_relay_gain * (bridge->stats.total_relays - 1) +
         bridge->config.relay_gain) / bridge->stats.total_relays;
    bridge->stats.avg_attention =
        (bridge->stats.avg_attention * (bridge->stats.total_relays - 1) +
         bridge->current_attention) / bridge->stats.total_relays;

    return 0;
}

float bgt_bridge_get_action_output(
    const bgt_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_get_action_output: bridge is NULL");
        return -1.0f;
    }
    if (action_id >= bridge->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgt_bridge_get_action_output: action_id out of range");
        return -1.0f;
    }
    return bridge->motor_output_buffer[action_id];
}

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

int bgt_bridge_set_attention(bgt_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_set_attention: bridge is NULL");
        return -1;
    }
    bridge->current_attention = nimcp_clampf(attention, 0.0f, 1.0f);
    return 0;
}

int bgt_bridge_set_urgency(bgt_bridge_t* bridge, float urgency) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_set_urgency: bridge is NULL");
        return -1;
    }
    bridge->current_urgency = nimcp_clampf(urgency, 0.0f, 1.0f);
    return 0;
}

int bgt_bridge_set_trn_inhibition(bgt_bridge_t* bridge, float inhibition) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_set_trn_inhibition: bridge is NULL");
        return -1;
    }
    bridge->trn_inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);
    return 0;
}

int bgt_bridge_set_mode(bgt_bridge_t* bridge, bgt_relay_mode_t mode) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_set_mode: bridge is NULL");
        return -1;
    }
    bridge->current_mode = mode;
    return 0;
}

float bgt_bridge_get_attention(const bgt_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->current_attention;
}

bgt_relay_mode_t bgt_bridge_get_mode(const bgt_bridge_t* bridge) {
    if (!bridge) return BGT_MODE_NORMAL;
    return bridge->current_mode;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int bgt_bridge_get_stats(
    const bgt_bridge_t* bridge,
    bgt_bridge_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgt_bridge_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void bgt_bridge_reset_stats(bgt_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bgt_bridge_stats_t));
}

const char* bgt_relay_mode_name(bgt_relay_mode_t mode) {
    switch (mode) {
        case BGT_MODE_NORMAL: return "normal";
        case BGT_MODE_URGENT: return "urgent";
        case BGT_MODE_SUPPRESSED: return "suppressed";
        case BGT_MODE_BURST: return "burst";
        default: return "unknown";
    }
}

const char* bgt_output_type_name(bgt_output_type_t type) {
    switch (type) {
        case BGT_OUTPUT_DISCRETE: return "discrete";
        case BGT_OUTPUT_CONTINUOUS: return "continuous";
        case BGT_OUTPUT_VELOCITY: return "velocity";
        default: return "unknown";
    }
}
