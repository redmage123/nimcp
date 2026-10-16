/**
 * @file nimcp_snn_tom_bridge.c
 * @brief SNN-Theory of Mind integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_tom_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_tom_bridge)

#define BIO_MODULE_SNN_TOM_BRIDGE 0x0629

void snn_tom_config_default(snn_tom_config_t* config) {
    if (!config) return;
    config->mentalizing_threshold = 0.55f;
    config->perspective_shift_rate = 0.75f;
    config->integration_time_window_ms = 300.0f;
    config->enable_attribution_tracking = true;
    config->attribution_confidence_min = 0.6f;
    config->tom_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
    config->n_mental_state_dims = 6;
    config->max_encoding_rate = 100.0f;
    config->decoding_window_ms = 100.0f;
}

snn_tom_bridge_t* snn_tom_bridge_create(
    const snn_tom_config_t* config,
    snn_network_t* snn,
    tom_context_t tom
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_bridge_create: snn is NULL");
        return NULL;
    }

    snn_tom_bridge_t* bridge = nimcp_malloc(sizeof(snn_tom_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_tom_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_tom_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_tom") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->tom = tom;
    bridge->config = *config;

    if (config->tom_population_id > 0) {
        bridge->tom_pop = snn_network_get_population(snn, config->tom_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-tom bridge");
    return bridge;
}

void snn_tom_bridge_destroy(snn_tom_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        snn_tom_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-tom bridge");
}

int snn_tom_bridge_connect_bio_async(snn_tom_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_TOM_BRIDGE,
        .module_name = "snn_tom_bridge",
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

int snn_tom_bridge_disconnect_bio_async(snn_tom_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_tom_bridge_is_bio_async_connected(const snn_tom_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int snn_tom_bridge_update(snn_tom_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    if (bridge->tom_pop) {
        float spike_rate = snn_population_get_firing_rate(bridge->tom_pop);
        bridge->state.mentalizing_activity = snn_tom_compute_mentalizing(bridge, spike_rate);
        bridge->state.avg_tom_rate = (bridge->state.avg_tom_rate * 0.9f + spike_rate * 0.1f);
        if (!isfinite(bridge->state.avg_tom_rate)) bridge->state.avg_tom_rate = spike_rate;

        if (bridge->state.mentalizing_activity >= bridge->config.mentalizing_threshold) {
            bridge->state.mentalizing_active = true;
            bridge->state.attribution_count++;
        }

        if (bridge->config.enable_attribution_tracking) {
            float shift_amount = bridge->config.perspective_shift_rate * dt / 1000.0f;
            bridge->state.last_perspective_shift += shift_amount;
            bridge->state.perspective_accuracy = fminf(1.0f,
                bridge->state.mentalizing_activity * bridge->config.perspective_shift_rate);
        }
    }

    return 0;
}

float snn_tom_compute_mentalizing(snn_tom_bridge_t* bridge, float spike_rate) {
    if (!bridge) return 0.0f;

    float normalized = spike_rate / bridge->config.max_encoding_rate;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized;
}

int snn_tom_update_perspective(snn_tom_bridge_t* bridge, const float* spike_train, uint32_t length, float* accuracy) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_update_perspective: bridge is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (!spike_train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_update_perspective: spike_train is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_tom_update_perspective: length is 0");
        return SNN_ERROR_INVALID_CONFIG;
    }

    float autocorr = 0.0f;
    uint32_t lag = (length > 10) ? 10 : length / 2;

    for (uint32_t i = 0; i < length - lag; i++) {
        autocorr += spike_train[i] * spike_train[i + lag];
    }
    autocorr /= (length - lag);

    float mean_sq = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        mean_sq += spike_train[i] * spike_train[i];
    }
    mean_sq /= length;

    float accuracy_val = (mean_sq > 0.0f) ? fminf(1.0f, autocorr / mean_sq) : 0.0f;
    if (accuracy_val < 0.0f) accuracy_val = 0.0f;

    if (accuracy) *accuracy = accuracy_val;
    bridge->state.perspective_accuracy = accuracy_val;

    return 0;
}

bool snn_tom_check_mentalizing_active(const snn_tom_bridge_t* bridge) {
    return bridge ? bridge->state.mentalizing_active : false;
}

int snn_tom_bridge_get_state(const snn_tom_bridge_t* bridge, snn_tom_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_tom_get_mentalizing_activity(const snn_tom_bridge_t* bridge) {
    return bridge ? bridge->state.mentalizing_activity : 0.0f;
}

float snn_tom_get_perspective_accuracy(const snn_tom_bridge_t* bridge) {
    return bridge ? bridge->state.perspective_accuracy : 0.0f;
}

int snn_tom_get_stats(const snn_tom_bridge_t* bridge, uint32_t* attribution_count, uint32_t* mentalizing_detections, float* avg_activity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_tom_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (attribution_count) *attribution_count = bridge->state.attribution_count;
    if (mentalizing_detections) *mentalizing_detections = bridge->state.mentalizing_active ? 1 : 0;
    if (avg_activity) *avg_activity = bridge->state.mentalizing_activity;
    return 0;
}

void snn_tom_reset_stats(snn_tom_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.attribution_count = 0;
    bridge->state.mentalizing_active = false;
    bridge->state.mentalizing_activity = 0.0f;
    bridge->state.perspective_accuracy = 0.0f;
    bridge->state.avg_tom_rate = 0.0f;
}
