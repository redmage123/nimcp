/**
 * @file nimcp_snn_self_model_bridge.c
 * @brief SNN-Self Model integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_self_model_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_self_model_bridge)

#define BIO_MODULE_SNN_SELF_MODEL_BRIDGE 0x0628

void snn_self_model_config_default(snn_self_model_config_t* config) {
    if (!config) return;
    config->self_reference_threshold = 0.6f;
    config->identity_stability_gain = 0.8f;
    config->integration_time_window_ms = 250.0f;
    config->enable_coherence_tracking = true;
    config->coherence_decay_rate = 0.95f;
    config->self_model_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
    config->n_self_dims = 8;
    config->max_encoding_rate = 100.0f;
    config->decoding_window_ms = 100.0f;
}

snn_self_model_bridge_t* snn_self_model_bridge_create(
    const snn_self_model_config_t* config,
    snn_network_t* snn,
    self_model_context_t self_model
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_bridge_create: snn is NULL");
        return NULL;
    }

    snn_self_model_bridge_t* bridge = nimcp_malloc(sizeof(snn_self_model_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_self_model_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_self_model_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_self_model") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->self_model = self_model;
    bridge->config = *config;

    if (config->self_model_population_id > 0) {
        bridge->self_model_pop = snn_network_get_population(snn, config->self_model_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-self_model bridge");
    return bridge;
}

void snn_self_model_bridge_destroy(snn_self_model_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        snn_self_model_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-self_model bridge");
}

int snn_self_model_bridge_connect_bio_async(snn_self_model_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_SELF_MODEL_BRIDGE,
        .module_name = "snn_self_model_bridge",
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

int snn_self_model_bridge_disconnect_bio_async(snn_self_model_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_self_model_bridge_is_bio_async_connected(const snn_self_model_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int snn_self_model_bridge_update(snn_self_model_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    if (bridge->self_model_pop) {
        float spike_rate = snn_population_get_firing_rate(bridge->self_model_pop);
        bridge->state.self_coherence = snn_self_model_compute_coherence(bridge, spike_rate);
        bridge->state.avg_self_rate = (bridge->state.avg_self_rate * 0.9f + spike_rate * 0.1f);
        if (!isfinite(bridge->state.avg_self_rate)) bridge->state.avg_self_rate = spike_rate;

        if (bridge->state.self_coherence >= bridge->config.self_reference_threshold) {
            bridge->state.coherent_self_detected = true;
            bridge->state.self_reference_count++;
        }

        if (bridge->config.enable_coherence_tracking) {
            bridge->state.identity_stability *= bridge->config.coherence_decay_rate;
            bridge->state.identity_stability += (1.0f - bridge->config.coherence_decay_rate) *
                                                bridge->state.self_coherence *
                                                bridge->config.identity_stability_gain;
        }
    }

    return 0;
}

float snn_self_model_compute_coherence(snn_self_model_bridge_t* bridge, float spike_rate) {
    if (!bridge) return 0.0f;

    float normalized = spike_rate / bridge->config.max_encoding_rate;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized;
}

int snn_self_model_update_stability(snn_self_model_bridge_t* bridge, const float* spike_train, uint32_t length, float* stability) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_update_stability: bridge is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (!spike_train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_update_stability: spike_train is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_self_model_update_stability: length is zero");
        return SNN_ERROR_INVALID_CONFIG;
    }

    float mean = 0.0f;
    for (uint32_t i = 0; i < length; i++) mean += spike_train[i];
    mean /= length;

    float variance = 0.0f;
    for (uint32_t i = 0; i < length; i++) {
        float diff = spike_train[i] - mean;
        variance += diff * diff;
    }
    variance /= length;

    float stability_val = (variance > 0.0f) ? expf(-variance) : 1.0f;
    if (stability) *stability = stability_val;

    bridge->state.identity_stability = stability_val;
    bridge->state.last_coherence_time = bridge->last_update_time;

    return 0;
}

bool snn_self_model_check_coherent_self(const snn_self_model_bridge_t* bridge) {
    return bridge ? bridge->state.coherent_self_detected : false;
}

int snn_self_model_bridge_get_state(const snn_self_model_bridge_t* bridge, snn_self_model_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_self_model_get_coherence(const snn_self_model_bridge_t* bridge) {
    return bridge ? bridge->state.self_coherence : 0.0f;
}

float snn_self_model_get_stability(const snn_self_model_bridge_t* bridge) {
    return bridge ? bridge->state.identity_stability : 0.0f;
}

int snn_self_model_get_stats(const snn_self_model_bridge_t* bridge, uint32_t* reference_count, uint32_t* coherent_detections, float* avg_coherence) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_self_model_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (reference_count) *reference_count = bridge->state.self_reference_count;
    if (coherent_detections) *coherent_detections = bridge->state.coherent_self_detected ? 1 : 0;
    if (avg_coherence) *avg_coherence = bridge->state.self_coherence;
    return 0;
}

void snn_self_model_reset_stats(snn_self_model_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.self_reference_count = 0;
    bridge->state.coherent_self_detected = false;
    bridge->state.self_coherence = 0.0f;
    bridge->state.identity_stability = 0.0f;
    bridge->state.avg_self_rate = 0.0f;
}
