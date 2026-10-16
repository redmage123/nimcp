/**
 * @file nimcp_snn_grief_bridge.c
 * @brief SNN-Grief integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_grief_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_grief_bridge)

#define BIO_MODULE_SNN_GRIEF_BRIDGE 0x0623

void snn_grief_config_default(snn_grief_config_t* config) {
    if (!config) return;

    config->grief_intensity_threshold = 0.3f;
    config->recovery_rate = 0.01f;
    config->rumination_suppression = 0.7f;
    config->slow_wave_min_freq = 0.5f;
    config->slow_wave_max_freq = 2.0f;
    config->baseline_activity_level = 0.3f;
    config->enable_recovery_tracking = true;
    config->enable_rumination_detection = true;
    config->grief_pop_id = 0;
    config->recovery_pop_id = 0;
    config->update_interval_ms = 100.0f;
    config->enable_bio_async = false;
}

snn_grief_bridge_t* snn_grief_bridge_create(
    const snn_grief_config_t* config,
    snn_network_t* snn,
    grief_system_t* grief_system
) {
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_grief_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_bridge_create: config/snn is NULL");
        return NULL;
    }

    snn_grief_bridge_t* bridge = nimcp_malloc(sizeof(snn_grief_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-grief bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_grief_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_grief_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_grief") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->grief_system = grief_system;
    bridge->config = *config;

    if (config->grief_pop_id > 0) {
        bridge->grief_pop = snn_network_get_population(snn, config->grief_pop_id);
    }
    if (config->recovery_pop_id > 0) {
        bridge->recovery_pop = snn_network_get_population(snn, config->recovery_pop_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-grief bridge");
    return bridge;
}

void snn_grief_bridge_destroy(snn_grief_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        snn_grief_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-grief bridge");
}

int snn_grief_bridge_connect_bio_async(snn_grief_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_GRIEF_BRIDGE,
        .module_name = "snn_grief_bridge",
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

int snn_grief_bridge_disconnect_bio_async(snn_grief_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_grief_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_grief_bridge_is_bio_async_connected(const snn_grief_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_grief_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_grief_bridge_update(snn_grief_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    float grief_intensity = 0.0f, recovery_progress = 0.0f;
    snn_grief_decode_from_spikes(bridge, &grief_intensity, &recovery_progress);

    bridge->state.grief_intensity = grief_intensity;
    bridge->state.recovery_progress = recovery_progress;

    if (grief_intensity > bridge->config.grief_intensity_threshold) {
        bridge->state.grief_duration_ms += (uint64_t)bridge->config.update_interval_ms;
    }

    if (bridge->config.enable_rumination_detection) {
        snn_grief_detect_rumination(bridge, &bridge->state.rumination_level);
    }

    snn_grief_detect_slow_wave(bridge, &bridge->state.slow_wave);
    snn_grief_modulate_populations(bridge);

    bridge->state.sync_count++;
    bridge->state.avg_grief_intensity = (bridge->state.avg_grief_intensity * 0.95f + grief_intensity * 0.05f);
    if (!isfinite(bridge->state.avg_grief_intensity)) bridge->state.avg_grief_intensity = grief_intensity;
    bridge->state.avg_recovery_progress = (bridge->state.avg_recovery_progress * 0.95f + recovery_progress * 0.05f);
    if (!isfinite(bridge->state.avg_recovery_progress)) bridge->state.avg_recovery_progress = recovery_progress;

    return 0;
}

int snn_grief_decode_from_spikes(
    snn_grief_bridge_t* bridge,
    float* grief_intensity_out,
    float* recovery_progress_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_decode_from_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float grief_rate = 0.0f, recovery_rate = 0.0f;

    if (bridge->grief_pop) {
        grief_rate = snn_population_get_firing_rate(bridge->grief_pop);
    }
    if (bridge->recovery_pop) {
        recovery_rate = snn_population_get_firing_rate(bridge->recovery_pop);
    }

    float grief_intensity = grief_rate / 50.0f;
    if (grief_intensity > 1.0f) grief_intensity = 1.0f;

    float recovery_progress = recovery_rate / 30.0f;
    if (recovery_progress > 1.0f) recovery_progress = 1.0f;

    if (grief_intensity_out) *grief_intensity_out = grief_intensity;
    if (recovery_progress_out) *recovery_progress_out = recovery_progress;

    return 0;
}

int snn_grief_detect_slow_wave(
    snn_grief_bridge_t* bridge,
    snn_slow_wave_state_t* slow_wave_state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_detect_slow_wave: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!slow_wave_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_detect_slow_wave: slow_wave_state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float center_freq = (bridge->config.slow_wave_min_freq + bridge->config.slow_wave_max_freq) / 2.0f;
    slow_wave_state->frequency = center_freq;
    slow_wave_state->amplitude = bridge->state.grief_intensity * 0.8f;
    slow_wave_state->phase = fmodf(bridge->last_update_time * 0.001f * NIMCP_TWO_PI_F * center_freq, NIMCP_TWO_PI_F);
    slow_wave_state->power = bridge->state.grief_intensity * bridge->state.grief_intensity;
    slow_wave_state->is_active = (bridge->state.grief_intensity > bridge->config.grief_intensity_threshold);

    return 0;
}

int snn_grief_detect_rumination(
    snn_grief_bridge_t* bridge,
    float* rumination_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_detect_rumination: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float rumination = bridge->state.grief_intensity * (1.0f - bridge->state.recovery_progress);
    if (rumination_level) *rumination_level = rumination;

    return 0;
}

int snn_grief_modulate_populations(snn_grief_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_modulate_populations: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float suppression = 1.0f - (bridge->state.grief_intensity *
                                (1.0f - bridge->config.baseline_activity_level));

    if (bridge->config.enable_rumination_detection) {
        suppression *= (1.0f - bridge->state.rumination_level * bridge->config.rumination_suppression);
    }

    bridge->state.activity_suppression = suppression;

    return 0;
}

int snn_grief_encode_to_spikes(
    snn_grief_bridge_t* bridge,
    float grief_intensity,
    float recovery_progress
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_encode_to_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

int snn_grief_bridge_get_state(
    const snn_grief_bridge_t* bridge,
    snn_grief_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_grief_get_intensity(const snn_grief_bridge_t* bridge) {
    return bridge ? bridge->state.grief_intensity : 0.0f;
}

float snn_grief_get_recovery_progress(const snn_grief_bridge_t* bridge) {
    return bridge ? bridge->state.recovery_progress : 0.0f;
}

bool snn_grief_is_slow_wave_active(const snn_grief_bridge_t* bridge) {
    return bridge ? bridge->state.slow_wave.is_active : false;
}

int snn_grief_get_stats(
    const snn_grief_bridge_t* bridge,
    uint32_t* sync_count,
    float* avg_grief_intensity,
    float* avg_recovery_progress
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_grief_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (sync_count) *sync_count = bridge->state.sync_count;
    if (avg_grief_intensity) *avg_grief_intensity = bridge->state.avg_grief_intensity;
    if (avg_recovery_progress) *avg_recovery_progress = bridge->state.avg_recovery_progress;
    return 0;
}

void snn_grief_reset_stats(snn_grief_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.sync_count = 0;
    bridge->state.avg_grief_intensity = 0.0f;
    bridge->state.avg_recovery_progress = 0.0f;
}
