/**
 * @file nimcp_snn_shadow_bridge.c
 * @brief SNN-Shadow integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_shadow_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_shadow_bridge)

#define BIO_MODULE_SNN_SHADOW_BRIDGE 0x0626

void snn_shadow_config_default(snn_shadow_config_t* config) {
    if (!config) return;

    config->shadow_activation_threshold = 0.2f;
    config->integration_rate = 0.05f;
    config->background_frequency_min = 0.1f;
    config->background_frequency_max = 4.0f;
    config->background_amplitude_max = 0.4f;
    config->resting_state_baseline = 0.3f;
    config->coherence_threshold = 0.7f;
    config->enable_integration_tracking = true;
    config->enable_dmn_simulation = true;
    config->shadow_pop_id = 0;
    config->dmn_pop_id = 0;
    config->update_interval_ms = 100.0f;
    config->enable_bio_async = false;
}

snn_shadow_bridge_t* snn_shadow_bridge_create(
    const snn_shadow_config_t* config,
    snn_network_t* snn,
    shadow_system_t* shadow_system
) {
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_shadow_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_bridge_create: config/snn is NULL");
        return NULL;
    }

    snn_shadow_bridge_t* bridge = nimcp_malloc(sizeof(snn_shadow_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-shadow bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_shadow_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_shadow_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_shadow") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->shadow_system = shadow_system;
    bridge->config = *config;

    if (config->shadow_pop_id > 0) {
        bridge->shadow_pop = snn_network_get_population(snn, config->shadow_pop_id);
    }
    if (config->dmn_pop_id > 0) {
        bridge->dmn_pop = snn_network_get_population(snn, config->dmn_pop_id);
    }

    bridge->state.resting_state_level = config->resting_state_baseline;

    NIMCP_LOGGING_INFO("Created SNN-shadow bridge");
    return bridge;
}

void snn_shadow_bridge_destroy(snn_shadow_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        snn_shadow_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-shadow bridge");
}

int snn_shadow_bridge_connect_bio_async(snn_shadow_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_SHADOW_BRIDGE,
        .module_name = "snn_shadow_bridge",
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

int snn_shadow_bridge_disconnect_bio_async(snn_shadow_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_shadow_bridge_is_bio_async_connected(const snn_shadow_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int snn_shadow_bridge_update(snn_shadow_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    float shadow_activity = 0.0f, dmn_activity = 0.0f;
    snn_shadow_decode_from_spikes(bridge, &shadow_activity, &dmn_activity);

    bridge->state.shadow_activity_level = shadow_activity;
    bridge->state.dmn_activity = dmn_activity;

    snn_shadow_detect_background_pattern(bridge, &bridge->state.background);

    if (bridge->config.enable_integration_tracking) {
        bool integration_detected = false;
        snn_shadow_detect_integration_event(bridge, &integration_detected);
        if (integration_detected) {
            bridge->state.integration_events++;
        }
    }

    bridge->state.integration_progress += bridge->config.integration_rate *
                                          bridge->state.background.coherence;
    if (bridge->state.integration_progress > 1.0f) {
        bridge->state.integration_progress = 1.0f;
    }

    snn_shadow_modulate_populations(bridge);

    bridge->state.sync_count++;
    bridge->state.avg_shadow_activity = (bridge->state.avg_shadow_activity * 0.95f +
                                         shadow_activity * 0.05f);
    if (!isfinite(bridge->state.avg_shadow_activity)) bridge->state.avg_shadow_activity = shadow_activity;
    bridge->state.avg_dmn_activity = (bridge->state.avg_dmn_activity * 0.95f +
                                      dmn_activity * 0.05f);
    if (!isfinite(bridge->state.avg_dmn_activity)) bridge->state.avg_dmn_activity = dmn_activity;

    return 0;
}

int snn_shadow_decode_from_spikes(
    snn_shadow_bridge_t* bridge,
    float* shadow_activity_out,
    float* dmn_activity_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_decode_from_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float shadow_rate = 0.0f, dmn_rate = 0.0f;

    if (bridge->shadow_pop) {
        shadow_rate = snn_population_get_firing_rate(bridge->shadow_pop);
    }
    if (bridge->dmn_pop) {
        dmn_rate = snn_population_get_firing_rate(bridge->dmn_pop);
    }

    float shadow_activity = shadow_rate / 20.0f;
    if (shadow_activity > 1.0f) shadow_activity = 1.0f;

    float dmn_activity = dmn_rate / 25.0f;
    if (dmn_activity > 1.0f) dmn_activity = 1.0f;

    if (shadow_activity_out) *shadow_activity_out = shadow_activity;
    if (dmn_activity_out) *dmn_activity_out = dmn_activity;

    return 0;
}

int snn_shadow_detect_background_pattern(
    snn_shadow_bridge_t* bridge,
    snn_background_pattern_state_t* background_state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_detect_background_pattern: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!background_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_detect_background_pattern: background_state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float center_freq = (bridge->config.background_frequency_min +
                        bridge->config.background_frequency_max) / 2.0f;

    background_state->is_active =
        (bridge->state.shadow_activity_level > bridge->config.shadow_activation_threshold);

    if (background_state->is_active) {
        background_state->frequency = center_freq;
        background_state->amplitude = bridge->state.shadow_activity_level *
                                      bridge->config.background_amplitude_max;
        background_state->phase = fmodf(bridge->last_update_time * 0.001f * NIMCP_TWO_PI_F *
                                       center_freq, NIMCP_TWO_PI_F);
        background_state->coherence = bridge->state.shadow_activity_level *
                                      bridge->state.dmn_activity;
    } else {
        background_state->frequency = 0.0f;
        background_state->amplitude = 0.0f;
        background_state->phase = 0.0f;
        background_state->coherence = 0.0f;
    }

    return 0;
}

int snn_shadow_detect_integration_event(
    snn_shadow_bridge_t* bridge,
    bool* integration_detected
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_detect_integration_event: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bool detected = (bridge->state.background.coherence > bridge->config.coherence_threshold &&
                    bridge->state.background.is_active);

    if (integration_detected) *integration_detected = detected;

    return 0;
}

int snn_shadow_modulate_populations(snn_shadow_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_modulate_populations: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->state.resting_state_level = bridge->config.resting_state_baseline +
                                        bridge->state.shadow_activity_level * 0.3f;

    bridge->state.shadow_conscious_coupling = bridge->state.background.coherence *
                                              bridge->state.integration_progress;

    return 0;
}

int snn_shadow_encode_to_spikes(
    snn_shadow_bridge_t* bridge,
    float shadow_activity,
    float integration_progress
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_encode_to_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

int snn_shadow_bridge_get_state(
    const snn_shadow_bridge_t* bridge,
    snn_shadow_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_shadow_get_activity_level(const snn_shadow_bridge_t* bridge) {
    return bridge ? bridge->state.shadow_activity_level : 0.0f;
}

float snn_shadow_get_dmn_activity(const snn_shadow_bridge_t* bridge) {
    return bridge ? bridge->state.dmn_activity : 0.0f;
}

bool snn_shadow_is_background_active(const snn_shadow_bridge_t* bridge) {
    return bridge ? bridge->state.background.is_active : false;
}

int snn_shadow_get_stats(
    const snn_shadow_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* integration_events,
    float* avg_shadow_activity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_shadow_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (sync_count) *sync_count = bridge->state.sync_count;
    if (integration_events) *integration_events = bridge->state.integration_events;
    if (avg_shadow_activity) *avg_shadow_activity = bridge->state.avg_shadow_activity;
    return 0;
}

void snn_shadow_reset_stats(snn_shadow_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.sync_count = 0;
    bridge->state.integration_events = 0;
    bridge->state.avg_shadow_activity = 0.0f;
    bridge->state.avg_dmn_activity = 0.0f;
}
