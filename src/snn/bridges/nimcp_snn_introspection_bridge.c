/**
 * @file nimcp_snn_introspection_bridge.c
 * @brief SNN-Introspection integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_introspection_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_introspection_bridge)

#define BIO_MODULE_SNN_INTROSPECTION_BRIDGE 0x0627

void snn_introspection_config_default(snn_introspection_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_config_default: null config pointer");
        return;
    }
    config->phi_rate_min = 20.0f;
    config->phi_rate_max = 120.0f;
    config->consciousness_threshold = 0.5f;
    config->integration_time_window_ms = 200.0f;
    config->enable_pattern_detection = true;
    config->pattern_match_threshold = 0.7f;
    config->introspection_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
    config->n_metacog_dims = 4;
    config->max_encoding_rate = 100.0f;
    config->decoding_window_ms = 100.0f;
}

snn_introspection_bridge_t* snn_introspection_bridge_create(
    const snn_introspection_config_t* config,
    snn_network_t* snn,
    introspection_context_t introspection
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_create: snn is NULL");
        return NULL;
    }

    snn_introspection_bridge_t* bridge = nimcp_malloc(sizeof(snn_introspection_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_introspection_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_introspection_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_introspection") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->introspection = introspection;
    bridge->config = *config;

    if (config->introspection_population_id > 0) {
        bridge->introspection_pop = snn_network_get_population(snn, config->introspection_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-introspection bridge");
    return bridge;
}

void snn_introspection_bridge_destroy(snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_destroy: null bridge pointer");
        return;
    }
    if (bridge->base.bio_async_enabled) {
        snn_introspection_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-introspection bridge");
}

int snn_introspection_bridge_connect_bio_async(snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_INTROSPECTION_BRIDGE,
        .module_name = "snn_introspection_bridge",
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

int snn_introspection_bridge_disconnect_bio_async(snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_introspection_bridge_is_bio_async_connected(const snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_introspection_bridge_update(snn_introspection_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time = 0.0f;

    if (bridge->introspection_pop) {
        float spike_rate = snn_population_get_firing_rate(bridge->introspection_pop);
        bridge->state.phi_estimate = snn_introspection_estimate_phi(bridge, spike_rate);
        bridge->state.avg_metacog_rate = (bridge->state.avg_metacog_rate * 0.9f + spike_rate * 0.1f);
        if (!isfinite(bridge->state.avg_metacog_rate)) bridge->state.avg_metacog_rate = spike_rate;

        if (bridge->state.phi_estimate >= bridge->config.consciousness_threshold) {
            bridge->state.consciousness_detected = true;
        }
    }

    return 0;
}

float snn_introspection_estimate_phi(snn_introspection_bridge_t* bridge, float spike_rate) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_estimate_phi: null bridge pointer");
        return 0.0f;
    }

    float min_rate = bridge->config.phi_rate_min;
    float max_rate = bridge->config.phi_rate_max;
    float normalized = (spike_rate - min_rate) / (max_rate - min_rate);

    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized;
}

int snn_introspection_detect_patterns(snn_introspection_bridge_t* bridge, const float* spike_train, uint32_t length, float* coherence) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_detect_patterns: bridge is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (!spike_train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_detect_patterns: spike_train is NULL");
        return SNN_ERROR_INVALID_CONFIG;
    }
    if (length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_introspection_detect_patterns: length is 0");
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

    float coherence_val = (variance > 0.0f) ? (1.0f / (1.0f + variance)) : 0.0f;
    if (coherence) *coherence = coherence_val;

    bridge->state.pattern_coherence = coherence_val;
    if (coherence_val >= bridge->config.pattern_match_threshold) {
        bridge->state.pattern_matches++;
    }

    return 0;
}

bool snn_introspection_check_consciousness(const snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.consciousness_detected;
}

int snn_introspection_bridge_get_state(const snn_introspection_bridge_t* bridge, snn_introspection_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_introspection_get_phi(const snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_get_phi: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.phi_estimate;
}

float snn_introspection_get_uncertainty(const snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_get_uncertainty: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.uncertainty_level;
}

int snn_introspection_get_stats(const snn_introspection_bridge_t* bridge, uint32_t* pattern_matches, uint32_t* consciousness_detections, float* avg_phi) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (pattern_matches) *pattern_matches = bridge->state.pattern_matches;
    if (consciousness_detections) *consciousness_detections = bridge->state.consciousness_detected ? 1 : 0;
    if (avg_phi) *avg_phi = bridge->state.phi_estimate;
    return 0;
}

void snn_introspection_reset_stats(snn_introspection_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_introspection_reset_stats: null bridge pointer");
        return;
    }
    bridge->state.pattern_matches = 0;
    bridge->state.consciousness_detected = false;
    bridge->state.phi_estimate = 0.0f;
    bridge->state.avg_metacog_rate = 0.0f;
}
