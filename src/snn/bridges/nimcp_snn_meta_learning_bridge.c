/**
 * @file nimcp_snn_meta_learning_bridge.c
 * @brief SNN-Meta Learning integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_meta_learning_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_meta_learning_bridge)

#define BIO_MODULE_SNN_META_LEARNING_BRIDGE 0x062A

void snn_meta_learning_config_default(snn_meta_learning_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_config_default: null config pointer");
        return;
    }
    config->meta_learning_rate = NIMCP_DEFAULT_LEARNING_RATE;
    config->adaptation_threshold = 0.65f;
    config->integration_time_window_ms = 500.0f;
    config->enable_efficiency_tracking = true;
    config->efficiency_decay_rate = 0.98f;
    config->meta_learning_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
    config->n_adaptation_dims = 5;
    config->max_encoding_rate = 100.0f;
    config->decoding_window_ms = 100.0f;
}

snn_meta_learning_bridge_t* snn_meta_learning_bridge_create(
    const snn_meta_learning_config_t* config,
    snn_network_t* snn,
    meta_learning_context_t meta_learning
) {
    if (!config || !snn) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_meta_learning_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_create: config/snn is NULL");
        return NULL;
    }

    snn_meta_learning_bridge_t* bridge = nimcp_malloc(sizeof(snn_meta_learning_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-meta_learning bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_meta_learning_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_meta_learning_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_meta_learning") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->meta_learning = meta_learning;
    bridge->config = *config;

    if (config->meta_learning_population_id > 0) {
        bridge->meta_learning_pop = snn_network_get_population(snn, config->meta_learning_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-meta_learning bridge");
    return bridge;
}

void snn_meta_learning_bridge_destroy(snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_destroy: null bridge pointer");
        return;
    }
    if (bridge->base.bio_async_enabled) {
        snn_meta_learning_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-meta_learning bridge");
}

int snn_meta_learning_bridge_connect_bio_async(snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_META_LEARNING_BRIDGE,
        .module_name = "snn_meta_learning_bridge",
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

int snn_meta_learning_bridge_disconnect_bio_async(snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_meta_learning_bridge_is_bio_async_connected(const snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_meta_learning_bridge_update(snn_meta_learning_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    if (bridge->meta_learning_pop) {
        float spike_rate = snn_population_get_firing_rate(bridge->meta_learning_pop);
        bridge->state.adaptation_level = snn_meta_learning_compute_adaptation(bridge, spike_rate);
        bridge->state.avg_meta_rate = (bridge->state.avg_meta_rate * 0.9f + spike_rate * 0.1f);
        if (!isfinite(bridge->state.avg_meta_rate)) bridge->state.avg_meta_rate = spike_rate;

        if (bridge->state.adaptation_level >= bridge->config.adaptation_threshold) {
            bridge->state.adaptation_active = true;
            bridge->state.meta_updates_count++;
        }

        if (bridge->config.enable_efficiency_tracking) {
            bridge->state.learning_efficiency *= bridge->config.efficiency_decay_rate;
            bridge->state.learning_efficiency += (1.0f - bridge->config.efficiency_decay_rate) *
                                                 bridge->state.adaptation_level *
                                                 bridge->config.meta_learning_rate;
            bridge->state.last_adaptation_time = bridge->last_update_time;
        }
    }

    return 0;
}

float snn_meta_learning_compute_adaptation(snn_meta_learning_bridge_t* bridge, float spike_rate) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_compute_adaptation: null bridge pointer");
        return 0.0f;
    }

    float normalized = spike_rate / bridge->config.max_encoding_rate;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized * bridge->config.meta_learning_rate * 10.0f;
}

int snn_meta_learning_update_efficiency(snn_meta_learning_bridge_t* bridge, const float* spike_train, uint32_t length, float* efficiency) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_update_efficiency: bridge is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (!spike_train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_update_efficiency: spike_train is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_meta_learning_update_efficiency: length is 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    float mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) mean += spike_train[i];
    mean /= length;

    float entropy = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        if (spike_train[i] > 0.0f) {
            float p = spike_train[i] / (mean * length);
            entropy -= p * logf(p + 1e-10f);
        }
    }

    float efficiency_val = (entropy > 0.0f) ? (1.0f / (1.0f + entropy)) : 0.0f;
    if (efficiency) *efficiency = efficiency_val;

    bridge->state.learning_efficiency = efficiency_val;
    bridge->state.last_adaptation_time = bridge->last_update_time;

    return 0;
}

bool snn_meta_learning_check_adaptation_active(const snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.adaptation_active;
}

int snn_meta_learning_bridge_get_state(const snn_meta_learning_bridge_t* bridge, snn_meta_learning_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_meta_learning_get_adaptation_level(const snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_get_adaptation_level: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.adaptation_level;
}

float snn_meta_learning_get_efficiency(const snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_get_efficiency: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.learning_efficiency;
}

int snn_meta_learning_get_stats(const snn_meta_learning_bridge_t* bridge, uint32_t* update_count, uint32_t* adaptation_detections, float* avg_adaptation) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (update_count) *update_count = bridge->state.meta_updates_count;
    if (adaptation_detections) *adaptation_detections = bridge->state.adaptation_active ? 1 : 0;
    if (avg_adaptation) *avg_adaptation = bridge->state.adaptation_level;
    return 0;
}

void snn_meta_learning_reset_stats(snn_meta_learning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_meta_learning_reset_stats: null bridge pointer");
        return;
    }
    bridge->state.meta_updates_count = 0;
    bridge->state.adaptation_active = false;
    bridge->state.adaptation_level = 0.0f;
    bridge->state.learning_efficiency = 0.0f;
    bridge->state.avg_meta_rate = 0.0f;
}
