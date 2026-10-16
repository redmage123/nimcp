/**
 * @file nimcp_snn_love_bridge.c
 * @brief SNN-Love integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_love_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_love_bridge)

#define BIO_MODULE_SNN_LOVE_BRIDGE 0x0625

void snn_love_config_default(snn_love_config_t* config) {
    if (!config) return;

    config->attachment_threshold = 0.5f;
    config->bonding_strength_gain = 1.2f;
    config->sustained_activity_min_ms = 500.0f;
    config->oxytocin_baseline = 0.3f;
    config->oxytocin_bonding_peak = 1.0f;
    config->synchrony_threshold = 0.6f;
    config->coupling_strength = 0.8f;
    config->enable_bonding_tracking = true;
    config->enable_synchrony_detection = true;
    config->attachment_pop_id = 0;
    config->bonding_pop_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_love_bridge_t* snn_love_bridge_create(
    const snn_love_config_t* config,
    snn_network_t* snn,
    love_system_t* love_system
) {
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_love_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_bridge_create: config/snn is NULL");
        return NULL;
    }

    snn_love_bridge_t* bridge = nimcp_malloc(sizeof(snn_love_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-love bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_love_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_love_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_love") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->love_system = love_system;
    bridge->config = *config;

    if (config->attachment_pop_id > 0) {
        bridge->attachment_pop = snn_network_get_population(snn, config->attachment_pop_id);
    }
    if (config->bonding_pop_id > 0) {
        bridge->bonding_pop = snn_network_get_population(snn, config->bonding_pop_id);
    }

    bridge->state.oxytocin_level = config->oxytocin_baseline;

    NIMCP_LOGGING_INFO("Created SNN-love bridge");
    return bridge;
}

void snn_love_bridge_destroy(snn_love_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        snn_love_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-love bridge");
}

int snn_love_bridge_connect_bio_async(snn_love_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_LOVE_BRIDGE,
        .module_name = "snn_love_bridge",
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

int snn_love_bridge_disconnect_bio_async(snn_love_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_love_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_love_bridge_is_bio_async_connected(const snn_love_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_love_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_love_bridge_update(snn_love_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    float attachment_level = 0.0f, bonding_strength = 0.0f;
    snn_love_decode_from_spikes(bridge, &attachment_level, &bonding_strength);

    bridge->state.attachment_level = attachment_level;
    bridge->state.bonding_strength = bonding_strength;

    snn_love_detect_sustained_activity(bridge, &bridge->state.sustained);

    if (bridge->config.enable_synchrony_detection) {
        snn_love_detect_synchrony(bridge, &bridge->state.population_synchrony);
    }

    if (bridge->config.enable_bonding_tracking &&
        attachment_level > bridge->config.attachment_threshold &&
        bridge->state.sustained.is_sustained) {
        bridge->state.bonding_events_count++;
    }

    snn_love_modulate_populations(bridge);

    bridge->state.sync_count++;
    bridge->state.avg_attachment_level = (bridge->state.avg_attachment_level * 0.95f +
                                          attachment_level * 0.05f);
    if (!isfinite(bridge->state.avg_attachment_level)) bridge->state.avg_attachment_level = attachment_level;
    bridge->state.avg_synchrony = (bridge->state.avg_synchrony * 0.95f +
                                   bridge->state.population_synchrony * 0.05f);
    if (!isfinite(bridge->state.avg_synchrony)) bridge->state.avg_synchrony = bridge->state.population_synchrony;

    return 0;
}

int snn_love_decode_from_spikes(
    snn_love_bridge_t* bridge,
    float* attachment_level_out,
    float* bonding_strength_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_decode_from_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float attachment_rate = 0.0f, bonding_rate = 0.0f;

    if (bridge->attachment_pop) {
        attachment_rate = snn_population_get_firing_rate(bridge->attachment_pop);
    }
    if (bridge->bonding_pop) {
        bonding_rate = snn_population_get_firing_rate(bridge->bonding_pop);
    }

    float attachment_level = attachment_rate / 60.0f;
    if (attachment_level > 1.0f) attachment_level = 1.0f;

    float bonding_strength = (bonding_rate / 40.0f) * bridge->config.bonding_strength_gain;
    if (bonding_strength > 1.0f) bonding_strength = 1.0f;

    if (attachment_level_out) *attachment_level_out = attachment_level;
    if (bonding_strength_out) *bonding_strength_out = bonding_strength;

    return 0;
}

int snn_love_detect_sustained_activity(
    snn_love_bridge_t* bridge,
    snn_sustained_activity_state_t* sustained_state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_detect_sustained_activity: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!sustained_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_detect_sustained_activity: sustained_state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float attachment_rate = bridge->attachment_pop ?
                           snn_population_get_firing_rate(bridge->attachment_pop) : 0.0f;

    if (attachment_rate > bridge->config.attachment_threshold * 60.0f) {
        if (!sustained_state->is_sustained) {
            bridge->sustained_start_time = 0.0f;
        }
        bridge->sustained_start_time += bridge->config.update_interval_ms;

        sustained_state->is_sustained =
            (bridge->sustained_start_time >= bridge->config.sustained_activity_min_ms);
        sustained_state->duration_ms = bridge->sustained_start_time;
        sustained_state->amplitude = attachment_rate / 60.0f;
        if (sustained_state->amplitude > 1.0f) sustained_state->amplitude = 1.0f;

        sustained_state->stability = fminf(bridge->sustained_start_time /
                                           (bridge->config.sustained_activity_min_ms * 2.0f), 1.0f);
    } else {
        sustained_state->is_sustained = false;
        sustained_state->duration_ms = 0.0f;
        sustained_state->amplitude = 0.0f;
        sustained_state->stability = 0.0f;
        bridge->sustained_start_time = 0.0f;
    }

    return 0;
}

int snn_love_detect_synchrony(
    snn_love_bridge_t* bridge,
    float* synchrony_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_detect_synchrony: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float attachment_rate = bridge->attachment_pop ?
                           snn_population_get_firing_rate(bridge->attachment_pop) : 0.0f;
    float bonding_rate = bridge->bonding_pop ?
                        snn_population_get_firing_rate(bridge->bonding_pop) : 0.0f;

    float rate_diff = fabsf(attachment_rate - bonding_rate);
    float avg_rate = (attachment_rate + bonding_rate) / 2.0f + 1e-6f;

    float synchrony = 1.0f - (rate_diff / avg_rate);
    if (synchrony < 0.0f) synchrony = 0.0f;
    if (synchrony > 1.0f) synchrony = 1.0f;

    if (synchrony_out) *synchrony_out = synchrony;

    return 0;
}

int snn_love_modulate_populations(snn_love_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_modulate_populations: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    if (bridge->state.sustained.is_sustained) {
        bridge->state.oxytocin_level = bridge->config.oxytocin_baseline +
                                       (bridge->config.oxytocin_bonding_peak - bridge->config.oxytocin_baseline) *
                                       bridge->state.sustained.stability;
    } else {
        bridge->state.oxytocin_level = bridge->config.oxytocin_baseline +
                                       bridge->state.attachment_level * 0.2f;
    }

    bridge->state.coupling_factor = bridge->config.coupling_strength *
                                    bridge->state.population_synchrony;

    return 0;
}

int snn_love_encode_to_spikes(
    snn_love_bridge_t* bridge,
    float attachment_level,
    float bonding_strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_encode_to_spikes: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

int snn_love_bridge_get_state(
    const snn_love_bridge_t* bridge,
    snn_love_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_love_get_attachment_level(const snn_love_bridge_t* bridge) {
    return bridge ? bridge->state.attachment_level : 0.0f;
}

float snn_love_get_bonding_strength(const snn_love_bridge_t* bridge) {
    return bridge ? bridge->state.bonding_strength : 0.0f;
}

bool snn_love_is_sustained(const snn_love_bridge_t* bridge) {
    return bridge ? bridge->state.sustained.is_sustained : false;
}

int snn_love_get_stats(
    const snn_love_bridge_t* bridge,
    uint32_t* sync_count,
    uint32_t* bonding_events_count,
    float* avg_attachment_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_love_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (sync_count) *sync_count = bridge->state.sync_count;
    if (bonding_events_count) *bonding_events_count = bridge->state.bonding_events_count;
    if (avg_attachment_level) *avg_attachment_level = bridge->state.avg_attachment_level;
    return 0;
}

void snn_love_reset_stats(snn_love_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.sync_count = 0;
    bridge->state.bonding_events_count = 0;
    bridge->state.avg_attachment_level = 0.0f;
    bridge->state.avg_synchrony = 0.0f;
}
