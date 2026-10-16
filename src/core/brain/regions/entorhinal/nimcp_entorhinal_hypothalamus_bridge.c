/**
 * @file nimcp_entorhinal_hypothalamus_bridge.c
 * @brief Implementation of Entorhinal-Hypothalamus Bidirectional Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/entorhinal/nimcp_entorhinal_hypothalamus_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(entorhinal_hypothalamus_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "ENTORHINAL_HYPOTHALAMUS_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define DEFAULT_MOTIVATION_ENCODING_WEIGHT      0.3f
#define DEFAULT_REWARD_PLASTICITY_WEIGHT        0.5f
#define DEFAULT_CIRCADIAN_CONSOLIDATION_WEIGHT  0.4f
#define DEFAULT_STRESS_MEMORY_WEIGHT            0.2f
#define DEFAULT_MAX_VALUE_BINDINGS              1024
#define DEFAULT_VALUE_MAP_RESOLUTION            0.5f
#define DEFAULT_VALUE_DECAY_RATE                0.001f
#define DEFAULT_VALUE_LEARNING_RATE             NIMCP_LEARNING_RATE_COARSE
#define DEFAULT_MOTIVATION_UPDATE_RATE_HZ       10.0f
#define DEFAULT_VALUE_MAP_UPDATE_RATE_HZ        5.0f
#define DEFAULT_HIGH_MOTIVATION_THRESHOLD       0.7f
#define DEFAULT_STRESS_IMPAIRMENT_THRESHOLD     0.8f

static float inverted_u_stress_modulation(float stress_level) {
    /* Yerkes-Dodson law: moderate stress enhances memory, high stress impairs */
    /* Baseline ~1.0 at zero stress, peak ~1.2 at moderate stress ~0.4, drops at high stress */
    if (stress_level < 0.4f) {
        return 1.0f + stress_level * 0.5f;  /* Rising part: 1.0 -> 1.2 */
    } else {
        return 1.2f - (stress_level - 0.4f) * 2.0f;  /* Falling part: 1.2 -> 0.0 */
    }
}

static uint32_t position_to_value_map_index(
    const spatial_value_map_t* map,
    const float* position,
    uint32_t dim)
{
    if (!map || !position || dim < 2) return UINT32_MAX;

    /* Simple 2D hash for now */
    int ix = (int)(position[0] / map->spatial_resolution);
    int iy = (int)(position[1] / map->spatial_resolution);

    /* Linear search for matching binding */
    for (uint32_t i = 0; i < map->num_bindings; i++) {
        int bx = (int)(map->bindings[i].position[0] / map->spatial_resolution);
        int by = (int)(map->bindings[i].position[1] / map->spatial_resolution);
        if (bx == ix && by == iy) {
            return i;
        }
    }

    return UINT32_MAX;  /* Not found */
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

entorhinal_hypothalamus_config_t entorhinal_hypothalamus_default_config(void) {
    entorhinal_hypothalamus_config_t config = {0};

    config.enable_motivation_modulation = true;
    config.enable_reward_learning = true;
    config.enable_circadian_modulation = true;
    config.enable_stress_modulation = true;
    config.enable_value_mapping = true;

    config.motivation_encoding_weight = DEFAULT_MOTIVATION_ENCODING_WEIGHT;
    config.reward_plasticity_weight = DEFAULT_REWARD_PLASTICITY_WEIGHT;
    config.circadian_consolidation_weight = DEFAULT_CIRCADIAN_CONSOLIDATION_WEIGHT;
    config.stress_memory_weight = DEFAULT_STRESS_MEMORY_WEIGHT;

    config.max_value_bindings = DEFAULT_MAX_VALUE_BINDINGS;
    config.value_map_resolution = DEFAULT_VALUE_MAP_RESOLUTION;
    config.value_decay_rate = DEFAULT_VALUE_DECAY_RATE;
    config.value_learning_rate = DEFAULT_VALUE_LEARNING_RATE;

    config.motivation_update_rate_hz = DEFAULT_MOTIVATION_UPDATE_RATE_HZ;
    config.value_map_update_rate_hz = DEFAULT_VALUE_MAP_UPDATE_RATE_HZ;

    config.high_motivation_threshold = DEFAULT_HIGH_MOTIVATION_THRESHOLD;
    config.stress_impairment_threshold = DEFAULT_STRESS_IMPAIRMENT_THRESHOLD;
    config.consolidation_circadian_peak = NIMCP_PI_F;  /* Peak during sleep (night) */

    return config;
}

entorhinal_hypothalamus_bridge_state_t* entorhinal_hypothalamus_bridge_create(
    const entorhinal_hypothalamus_config_t* config)
{
    entorhinal_hypothalamus_bridge_state_t* bridge =
        (entorhinal_hypothalamus_bridge_state_t*)nimcp_calloc(1,
            sizeof(entorhinal_hypothalamus_bridge_state_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_hypothalamus_bridge_create: allocation failed");

        return NULL;

    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = entorhinal_hypothalamus_default_config();
    }

    /* Initialize value map if enabled */
    if (bridge->config.enable_value_mapping) {
        bridge->value_map = (spatial_value_map_t*)nimcp_calloc(1, sizeof(spatial_value_map_t));
        if (bridge->value_map) {
            bridge->value_map->bindings = (spatial_value_binding_t*)nimcp_calloc(
                bridge->config.max_value_bindings, sizeof(spatial_value_binding_t));
            if (!bridge->value_map->bindings) {
                nimcp_free(bridge->value_map);
                bridge->value_map = NULL;
            } else {
                bridge->value_map->max_bindings = bridge->config.max_value_bindings;
                bridge->value_map->spatial_resolution = bridge->config.value_map_resolution;
                bridge->value_map->decay_rate = bridge->config.value_decay_rate;
                bridge->value_map->learning_rate = bridge->config.value_learning_rate;
            }
        }
    }

    /* Initialize default modulation values */
    bridge->encoding_modulation = 1.0f;
    bridge->retrieval_modulation = 1.0f;
    bridge->plasticity_modulation = 1.0f;
    bridge->consolidation_modulation = 1.0f;

    /* Initialize motivation to baseline */
    bridge->motivation.arousal_level = 0.5f;
    bridge->motivation.exploration_drive = 0.3f;

    return bridge;
}

void entorhinal_hypothalamus_bridge_destroy(
    entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "entorhinal_hypothalamus");

    if (bridge->value_map) {
        nimcp_free(bridge->value_map->bindings);
        nimcp_free(bridge->value_map);
    }

    nimcp_free(bridge);
}

int entorhinal_hypothalamus_bridge_connect(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    nimcp_entorhinal_t* entorhinal,
    hypothalamus_adapter_t* hypothalamus)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_bridge_connect: bridge is NULL");
        return -1;
    }

    bridge->entorhinal = entorhinal;
    bridge->hypothalamus = hypothalamus;
    bridge->connected = true;

    return 0;
}

int entorhinal_hypothalamus_bridge_disconnect(
    entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_bridge_disconnect: bridge is NULL");
        return -1;
    }

    bridge->entorhinal = NULL;
    bridge->hypothalamus = NULL;
    bridge->connected = false;

    return 0;
}

int entorhinal_hypothalamus_bridge_reset(
    entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Reset motivation state */
    memset(&bridge->motivation, 0, sizeof(hypothalamic_motivational_state_t));
    bridge->motivation.arousal_level = 0.5f;

    /* Reset modulation values */
    bridge->encoding_modulation = 1.0f;
    bridge->retrieval_modulation = 1.0f;
    bridge->plasticity_modulation = 1.0f;
    bridge->consolidation_modulation = 1.0f;

    /* Clear value map */
    if (bridge->value_map) {
        bridge->value_map->num_bindings = 0;
    }

    /* Clear nucleus activity */
    memset(bridge->nucleus_activity, 0, sizeof(bridge->nucleus_activity));

    /* Reset statistics */
    bridge->updates_processed = 0;
    bridge->mean_motivation_signal = 0.0f;
    bridge->mean_reward_prediction = 0.0f;
    bridge->value_map_updates = 0;
    bridge->mean_encoding_boost = 0.0f;

    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW IMPLEMENTATION
 *===========================================================================*/

int entorhinal_hypothalamus_bridge_update(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float dt)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_bridge_update: bridge is NULL");
        return -1;
    }

    /* Compute encoding modulation based on motivation */
    if (bridge->config.enable_motivation_modulation) {
        float motivation_boost = 0.0f;

        /* Higher drives increase encoding strength */
        motivation_boost += bridge->motivation.hunger_drive * 0.2f;
        motivation_boost += bridge->motivation.thirst_drive * 0.2f;
        motivation_boost += bridge->motivation.safety_drive * 0.3f;
        motivation_boost += bridge->motivation.exploration_drive * 0.2f;

        /* Arousal affects overall encoding */
        float arousal_factor = 0.5f + bridge->motivation.arousal_level * 0.5f;

        bridge->encoding_modulation = (1.0f + motivation_boost) * arousal_factor;
        bridge->encoding_modulation = nimcp_clamp01(bridge->encoding_modulation * 0.5f + 0.5f);
    }

    /* Compute stress modulation (inverted U) */
    if (bridge->config.enable_stress_modulation) {
        float stress_mod = inverted_u_stress_modulation(bridge->motivation.stress_level);
        bridge->encoding_modulation *= stress_mod;
        bridge->plasticity_modulation = stress_mod;
    }

    /* Compute circadian modulation for consolidation */
    if (bridge->config.enable_circadian_modulation) {
        float phase_diff = fabsf(bridge->motivation.circadian_phase -
            bridge->config.consolidation_circadian_peak);
        if (phase_diff > 3.14159f) phase_diff = NIMCP_TWO_PI_F - phase_diff;

        /* Gaussian around peak */
        float consolidation_factor = expf(-phase_diff * phase_diff * 0.5f);
        bridge->consolidation_modulation = 0.3f + 0.7f * consolidation_factor;
    }

    /* Reward-based plasticity modulation */
    if (bridge->config.enable_reward_learning) {
        float reward_factor = 1.0f + bridge->motivation.reward_prediction_error *
            bridge->config.reward_plasticity_weight;
        bridge->plasticity_modulation *= nimcp_clamp01(reward_factor);
    }

    /* Decay value map */
    if (bridge->value_map) {
        entorhinal_hypothalamus_decay_value_map(bridge, dt);
    }

    /* Update nucleus activity (simple model) */
    bridge->nucleus_activity[HYPOTHAL_NUCLEUS_LATERAL] =
        bridge->motivation.hunger_drive * 0.5f + bridge->motivation.reward_prediction * 0.5f;
    bridge->nucleus_activity[HYPOTHAL_NUCLEUS_VENTROMEDIAL] =
        1.0f - bridge->motivation.hunger_drive;
    bridge->nucleus_activity[HYPOTHAL_NUCLEUS_SUPRACHIASMATIC] =
        sinf(bridge->motivation.circadian_phase) * 0.5f + 0.5f;
    bridge->nucleus_activity[HYPOTHAL_NUCLEUS_PARAVENTRICULAR] =
        bridge->motivation.stress_level;
    bridge->nucleus_activity[HYPOTHAL_NUCLEUS_POSTERIOR] =
        bridge->motivation.arousal_level;
    bridge->nucleus_activity[HYPOTHAL_NUCLEUS_MAMMILLARY] =
        bridge->encoding_modulation;

    /* Update statistics */
    bridge->updates_processed++;
    float motivation_signal = (bridge->motivation.hunger_drive +
        bridge->motivation.thirst_drive + bridge->motivation.exploration_drive) / 3.0f;
    bridge->mean_motivation_signal = bridge->mean_motivation_signal * 0.99f +
        motivation_signal * 0.01f;
    bridge->mean_encoding_boost = bridge->mean_encoding_boost * 0.99f +
        bridge->encoding_modulation * 0.01f;

    return 0;
}

int entorhinal_hypothalamus_receive_motivation(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    const hypothalamic_motivational_state_t* motivation)
{
    if (!bridge || !motivation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_receive_motivation: required parameter is NULL (bridge, motivation)");
        return -1;
    }

    bridge->motivation = *motivation;
    bridge->motivation.memory_encoding_boost = bridge->encoding_modulation;

    return 0;
}

int entorhinal_hypothalamus_send_spatial_context(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim,
    float memory_salience)
{
    if (!bridge || !position || dim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_send_spatial_context: required parameter is NULL (bridge, position)");
        return -1;
    }

    /* Would send to hypothalamus adapter here */
    /* For now, just update local state */

    return 0;
}

int entorhinal_hypothalamus_process_reward(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float reward,
    const float* position, uint32_t dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_process_reward: bridge is NULL");
        return -1;
    }

    /* Update reward prediction error */
    bridge->motivation.reward_prediction_error = reward - bridge->motivation.reward_prediction;

    /* Update reward prediction (simple exponential moving average) */
    bridge->motivation.reward_prediction = bridge->motivation.reward_prediction * 0.9f +
        reward * 0.1f;
    bridge->mean_reward_prediction = bridge->mean_reward_prediction * 0.99f +
        bridge->motivation.reward_prediction * 0.01f;

    /* Update value map if position provided */
    if (position && dim >= 2) {
        entorhinal_hypothalamus_update_spatial_value(bridge, position, dim, reward);
    }

    return 0;
}

/*=============================================================================
 * MODULATION API IMPLEMENTATION
 *===========================================================================*/

float entorhinal_hypothalamus_get_encoding_modulation(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->encoding_modulation;
}

float entorhinal_hypothalamus_get_retrieval_modulation(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->retrieval_modulation;
}

float entorhinal_hypothalamus_get_plasticity_modulation(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) return 1.0f;
    return bridge->plasticity_modulation;
}

float entorhinal_hypothalamus_get_consolidation_gate(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->consolidation_modulation;
}

int entorhinal_hypothalamus_modulate_encoding(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float* encoding_strength)
{
    if (!bridge || !encoding_strength) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_modulate_encoding: required parameter is NULL (bridge, encoding_strength)");
        return -1;
    }

    *encoding_strength *= bridge->encoding_modulation;

    return 0;
}

/*=============================================================================
 * VALUE MAP API IMPLEMENTATION
 *===========================================================================*/

float entorhinal_hypothalamus_get_spatial_value(
    const entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim)
{
    if (!bridge || !bridge->value_map || !position || dim < 2) return 0.0f;

    uint32_t idx = position_to_value_map_index(bridge->value_map, position, dim);
    if (idx == UINT32_MAX) return 0.0f;

    return bridge->value_map->bindings[idx].value;
}

int entorhinal_hypothalamus_update_spatial_value(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim,
    float reward)
{
    if (!bridge || !bridge->value_map || !position || dim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_update_spatial_value: required parameter is NULL (bridge, bridge->value_map, position)");
        return -1;
    }

    uint32_t idx = position_to_value_map_index(bridge->value_map, position, dim);

    if (idx == UINT32_MAX) {
        /* Create new binding */
        if (bridge->value_map->num_bindings >= bridge->value_map->max_bindings) {
            /* Map is full, could implement replacement policy here */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "entorhinal_hypothalamus_update_spatial_value: capacity exceeded");
            return -1;
        }

        idx = bridge->value_map->num_bindings++;
        bridge->value_map->bindings[idx].position[0] = position[0];
        bridge->value_map->bindings[idx].position[1] = position[1];
        if (dim > 2) bridge->value_map->bindings[idx].position[2] = position[2];
        bridge->value_map->bindings[idx].value = 0.0f;
        bridge->value_map->bindings[idx].visit_count = 0;
        bridge->value_map->bindings[idx].avg_reward = 0.0f;
    }

    /* Update value with learning rate */
    spatial_value_binding_t* binding = &bridge->value_map->bindings[idx];
    binding->value = binding->value * (1.0f - bridge->value_map->learning_rate) +
        reward * bridge->value_map->learning_rate;
    binding->last_reward = reward;
    binding->visit_count++;
    binding->avg_reward = binding->avg_reward * ((float)(binding->visit_count - 1) /
        binding->visit_count) + reward / binding->visit_count;

    bridge->value_map_updates++;

    return 0;
}

int entorhinal_hypothalamus_get_value_gradient(
    const entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim,
    float* gradient_out)
{
    if (!bridge || !bridge->value_map || !position || !gradient_out || dim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_get_value_gradient: required parameter is NULL (bridge, bridge->value_map, position, gradient_out)");
        return -1;
    }

    /* Compute numerical gradient */
    float delta = bridge->value_map->spatial_resolution;

    float pos_plus_x[3] = {position[0] + delta, position[1], dim > 2 ? position[2] : 0.0f};
    float pos_minus_x[3] = {position[0] - delta, position[1], dim > 2 ? position[2] : 0.0f};
    float pos_plus_y[3] = {position[0], position[1] + delta, dim > 2 ? position[2] : 0.0f};
    float pos_minus_y[3] = {position[0], position[1] - delta, dim > 2 ? position[2] : 0.0f};

    float v_plus_x = entorhinal_hypothalamus_get_spatial_value(bridge, pos_plus_x, dim);
    float v_minus_x = entorhinal_hypothalamus_get_spatial_value(bridge, pos_minus_x, dim);
    float v_plus_y = entorhinal_hypothalamus_get_spatial_value(bridge, pos_plus_y, dim);
    float v_minus_y = entorhinal_hypothalamus_get_spatial_value(bridge, pos_minus_y, dim);

    gradient_out[0] = (v_plus_x - v_minus_x) / (2.0f * delta);
    gradient_out[1] = (v_plus_y - v_minus_y) / (2.0f * delta);

    return 0;
}

int entorhinal_hypothalamus_decay_value_map(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float dt)
{
    if (!bridge || !bridge->value_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_decay_value_map: required parameter is NULL (bridge, bridge->value_map)");
        return -1;
    }

    float decay_factor = expf(-bridge->value_map->decay_rate * dt);

    for (uint32_t i = 0; i < bridge->value_map->num_bindings; i++) {
        bridge->value_map->bindings[i].value *= decay_factor;
    }

    return 0;
}

/*=============================================================================
 * CIRCADIAN API IMPLEMENTATION
 *===========================================================================*/

float entorhinal_hypothalamus_get_circadian_consolidation(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) return 0.5f;
    return bridge->consolidation_modulation;
}

bool entorhinal_hypothalamus_in_consolidation_window(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_in_consolidation_window: bridge is NULL");
        return false;
    }
    return bridge->consolidation_modulation > 0.7f;
}

/*=============================================================================
 * DIAGNOSTICS API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_hypothalamus_bridge_get_stats(
    const entorhinal_hypothalamus_bridge_state_t* bridge,
    uint64_t* updates_processed,
    float* mean_motivation,
    float* mean_encoding_boost)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_bridge_get_stats: bridge is NULL");
        return -1;
    }

    if (updates_processed) *updates_processed = bridge->updates_processed;
    if (mean_motivation) *mean_motivation = bridge->mean_motivation_signal;
    if (mean_encoding_boost) *mean_encoding_boost = bridge->mean_encoding_boost;

    return 0;
}

int entorhinal_hypothalamus_bridge_log_diagnostics(
    const entorhinal_hypothalamus_bridge_state_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_hypothalamus_bridge_log_diagnostics: bridge is NULL");
        return -1;
    }

    /* Would log to nimcp_logger here */
    /* For now, just return success */

    return 0;
}
