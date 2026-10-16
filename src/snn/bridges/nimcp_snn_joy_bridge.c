/**
 * @file nimcp_snn_joy_bridge.c
 * @brief SNN-Joy integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_joy_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_joy_bridge)

#define BIO_MODULE_SNN_JOY_BRIDGE 0x0624

void snn_joy_config_default(snn_joy_config_t* config) {
    if (!config) return;

    config->joy_burst_threshold = 50.0f;
    config->reward_prediction_gain = 1.0f;
    config->burst_frequency_min = 10.0f;
    config->burst_frequency_max = 80.0f;
    config->burst_duration_ms = 100.0f;
    config->dopamine_baseline = 0.2f;
    config->dopamine_burst_peak = 1.0f;
    config->enable_rpe_tracking = true;
    config->enable_burst_counting = true;
    config->joy_pop_id = 0;
    config->reward_pop_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_joy_bridge_t* snn_joy_bridge_create(
    const snn_joy_config_t* config,
    snn_network_t* snn,
    joy_system_t* joy_system
) {
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_joy_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_bridge_create: config/snn is NULL");
        return NULL;
    }

    snn_joy_bridge_t* bridge = nimcp_malloc(sizeof(snn_joy_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-joy bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_joy_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_joy_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_joy") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->joy_system = joy_system;
    bridge->config = *config;

    if (config->joy_pop_id > 0) {
        bridge->joy_pop = snn_network_get_population(snn, config->joy_pop_id);
    }
    if (config->reward_pop_id > 0) {
        bridge->reward_pop = snn_network_get_population(snn, config->reward_pop_id);
    }

    bridge->state.dopamine_level = config->dopamine_baseline;

    NIMCP_LOGGING_INFO("Created SNN-joy bridge");
    return bridge;
}

void snn_joy_bridge_destroy(snn_joy_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        snn_joy_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-joy bridge");
}

int snn_joy_bridge_connect_bio_async(snn_joy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_JOY_BRIDGE,
        .module_name = "snn_joy_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    return SNN_ERROR_OPERATION_FAILED;
}

int snn_joy_bridge_disconnect_bio_async(snn_joy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_joy_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_joy_bridge_is_bio_async_connected(const snn_joy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_joy_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_joy_bridge_update(snn_joy_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    float joy_level = 0.0f, rpe = 0.0f;
    snn_joy_decode_from_spikes(bridge, &joy_level, &rpe);

    bridge->state.joy_level = joy_level;
    bridge->state.reward_prediction_error = rpe;

    snn_joy_detect_burst(bridge, &bridge->state.burst);

    if (bridge->config.enable_burst_counting && bridge->state.burst.is_bursting) {
        bridge->state.burst_count++;
    }

    snn_joy_modulate_populations(bridge);

    bridge->state.sync_count++;
    bridge->state.avg_joy_level = (bridge->state.avg_joy_level * 0.95f + joy_level * 0.05f);
    if (!isfinite(bridge->state.avg_joy_level)) bridge->state.avg_joy_level = joy_level;
    bridge->state.avg_burst_frequency = (bridge->state.avg_burst_frequency * 0.95f +
                                         bridge->state.burst.frequency * 0.05f);
    if (!isfinite(bridge->state.avg_burst_frequency)) bridge->state.avg_burst_frequency = bridge->state.burst.frequency;

    return 0;
}

int snn_joy_decode_from_spikes(
    snn_joy_bridge_t* bridge,
    float* joy_level_out,
    float* rpe_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_decode_from_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float joy_rate = 0.0f, reward_rate = 0.0f;

    if (bridge->joy_pop) {
        joy_rate = snn_population_get_firing_rate(bridge->joy_pop);
    }
    if (bridge->reward_pop) {
        reward_rate = snn_population_get_firing_rate(bridge->reward_pop);
    }

    float joy_level = joy_rate / 100.0f;
    if (joy_level > 1.0f) joy_level = 1.0f;

    float rpe = 0.0f;
    if (bridge->config.enable_rpe_tracking) {
        float predicted = bridge->state.predicted_reward;
        float actual = reward_rate / 50.0f;
        if (actual > 1.0f) actual = 1.0f;
        rpe = (actual - predicted) * bridge->config.reward_prediction_gain;
        if (rpe < -1.0f) rpe = -1.0f;
        if (rpe > 1.0f) rpe = 1.0f;

        bridge->state.actual_reward = actual;
    }

    if (joy_level_out) *joy_level_out = joy_level;
    if (rpe_out) *rpe_out = rpe;

    return 0;
}

int snn_joy_detect_burst(
    snn_joy_bridge_t* bridge,
    snn_burst_state_t* burst_state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_detect_burst: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!burst_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_detect_burst: burst_state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float joy_rate = bridge->joy_pop ? snn_population_get_firing_rate(bridge->joy_pop) : 0.0f;

    burst_state->is_bursting = (joy_rate > bridge->config.joy_burst_threshold);

    if (burst_state->is_bursting) {
        float range = bridge->config.burst_frequency_max - bridge->config.burst_frequency_min;
        burst_state->frequency = bridge->config.burst_frequency_min +
                                 (joy_rate / 100.0f) * range;
        burst_state->amplitude = (joy_rate - bridge->config.joy_burst_threshold) /
                                 (100.0f - bridge->config.joy_burst_threshold);
        if (burst_state->amplitude > 1.0f) burst_state->amplitude = 1.0f;

        burst_state->duration_ms = bridge->config.burst_duration_ms;
        burst_state->spike_count = (uint32_t)(joy_rate * bridge->config.burst_duration_ms / 1000.0f);
    } else {
        burst_state->frequency = 0.0f;
        burst_state->amplitude = 0.0f;
        burst_state->duration_ms = 0.0f;
        burst_state->spike_count = 0;
    }

    return 0;
}

int snn_joy_compute_rpe(
    snn_joy_bridge_t* bridge,
    float predicted_reward,
    float actual_reward,
    float* rpe_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_compute_rpe: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->state.predicted_reward = predicted_reward;
    bridge->state.actual_reward = actual_reward;

    float rpe = (actual_reward - predicted_reward) * bridge->config.reward_prediction_gain;
    if (rpe < -1.0f) rpe = -1.0f;
    if (rpe > 1.0f) rpe = 1.0f;

    bridge->state.reward_prediction_error = rpe;

    if (rpe_out) *rpe_out = rpe;

    return 0;
}

int snn_joy_modulate_populations(snn_joy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_modulate_populations: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (bridge->state.burst.is_bursting) {
        bridge->state.dopamine_level = bridge->config.dopamine_baseline +
                                       (bridge->config.dopamine_burst_peak - bridge->config.dopamine_baseline) *
                                       bridge->state.burst.amplitude;
    } else {
        bridge->state.dopamine_level = bridge->config.dopamine_baseline +
                                       bridge->state.joy_level * 0.3f;
    }

    return 0;
}

int snn_joy_encode_to_spikes(
    snn_joy_bridge_t* bridge,
    float joy_level,
    float rpe
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_encode_to_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

int snn_joy_bridge_get_state(
    const snn_joy_bridge_t* bridge,
    snn_joy_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_joy_get_level(const snn_joy_bridge_t* bridge) {
    return bridge ? bridge->state.joy_level : 0.0f;
}

float snn_joy_get_rpe(const snn_joy_bridge_t* bridge) {
    return bridge ? bridge->state.reward_prediction_error : 0.0f;
}

bool snn_joy_is_bursting(const snn_joy_bridge_t* bridge) {
    return bridge ? bridge->state.burst.is_bursting : false;
}

int snn_joy_get_stats(
    const snn_joy_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* burst_count,
    float* avg_joy_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_joy_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (sync_count) *sync_count = bridge->state.sync_count;
    if (burst_count) *burst_count = bridge->state.burst_count;
    if (avg_joy_level) *avg_joy_level = bridge->state.avg_joy_level;
    return 0;
}

void snn_joy_reset_stats(snn_joy_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.sync_count = 0;
    bridge->state.burst_count = 0;
    bridge->state.avg_joy_level = 0.0f;
    bridge->state.avg_burst_frequency = 0.0f;
}
