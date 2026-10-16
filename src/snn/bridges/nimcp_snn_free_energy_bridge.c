/**
 * @file nimcp_snn_free_energy_bridge.c
 * @brief SNN-Free Energy integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_free_energy_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_free_energy_bridge)

#define BIO_MODULE_SNN_FREE_ENERGY_BRIDGE 0x062C

void snn_free_energy_config_default(snn_free_energy_config_t* config) {
    if (!config) return;
    config->prediction_error_gain = 1.5f;
    config->surprise_threshold = 2.0f;
    config->precision_weighting = 1.0f;
    config->prediction_population_id = 0;
    config->error_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_free_energy_bridge_t* snn_free_energy_bridge_create(const snn_free_energy_config_t* config, snn_network_t* snn, free_energy_system_t* free_energy_system) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_free_energy_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_free_energy_bridge_create: snn is NULL");
        return NULL;
    }

    snn_free_energy_bridge_t* bridge = nimcp_malloc(sizeof(snn_free_energy_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_free_energy_bridge_t),
                          "snn_free_energy_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_free_energy_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_free_energy") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->free_energy_system = free_energy_system;
    bridge->config = *config;

    if (config->prediction_population_id > 0) {
        bridge->prediction_pop = snn_network_get_population(snn, config->prediction_population_id);
    }
    if (config->error_population_id > 0) {
        bridge->error_pop = snn_network_get_population(snn, config->error_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-free-energy bridge");
    return bridge;
}

void snn_free_energy_bridge_destroy(snn_free_energy_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        snn_free_energy_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->prediction_buffer) nimcp_free(bridge->prediction_buffer);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-free-energy bridge");
}

int snn_free_energy_bridge_connect_bio_async(snn_free_energy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_FREE_ENERGY_BRIDGE,
        .module_name = "snn_free_energy_bridge",
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

int snn_free_energy_bridge_disconnect_bio_async(snn_free_energy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_free_energy_bridge_is_bio_async_connected(const snn_free_energy_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_free_energy_bridge_update(snn_free_energy_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) return 0;
    bridge->last_update_time = 0.0f;

    float prediction_rate = 0.0f;
    float error_rate = 0.0f;

    if (bridge->prediction_pop) {
        prediction_rate = snn_population_get_firing_rate(bridge->prediction_pop);
    }

    if (bridge->error_pop) {
        error_rate = snn_population_get_firing_rate(bridge->error_pop);
        bridge->state.prediction_error = error_rate * bridge->config.prediction_error_gain;
        if (bridge->state.prediction_error > 0.0f) {
            bridge->state.prediction_error_count++;
        }
    }

    bridge->state.free_energy_estimate = snn_free_energy_compute_free_energy(bridge, prediction_rate, error_rate);
    bridge->state.accumulated_free_energy += bridge->state.free_energy_estimate;

    bridge->state.surprise_level = bridge->state.free_energy_estimate * bridge->config.precision_weighting;
    bridge->state.is_surprised = (bridge->state.surprise_level >= bridge->config.surprise_threshold);
    if (bridge->state.is_surprised) {
        bridge->state.surprise_events++;
    }

    return 0;
}

int snn_free_energy_bridge_encode_prediction_error(snn_free_energy_bridge_t* bridge, float error) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_encode_prediction_error: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->error_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "snn_free_energy_bridge_encode_prediction_error: error population not configured");
        return SNN_ERROR_INVALID_STATE;
    }

    float spike_rate = error * bridge->config.prediction_error_gain;
    return 0;
}

int snn_free_energy_bridge_update_precision(snn_free_energy_bridge_t* bridge, float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_update_precision: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (precision < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "snn_free_energy_bridge_update_precision: precision must be non-negative");
        return SNN_ERROR_INVALID_CONFIG;
    }

    bridge->config.precision_weighting = precision;
    return 0;
}

float snn_free_energy_compute_free_energy(const snn_free_energy_bridge_t* bridge, float prediction_rate, float error_rate) {
    if (!bridge) return 0.0f;

    float weighted_error = error_rate * bridge->config.prediction_error_gain;
    float free_energy = weighted_error + logf(1.0f + prediction_rate);

    return free_energy;
}

int snn_free_energy_bridge_get_state(const snn_free_energy_bridge_t* bridge, snn_free_energy_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_free_energy_get_estimate(const snn_free_energy_bridge_t* bridge) {
    return bridge ? bridge->state.free_energy_estimate : 0.0f;
}

float snn_free_energy_get_prediction_error(const snn_free_energy_bridge_t* bridge) {
    return bridge ? bridge->state.prediction_error : 0.0f;
}

float snn_free_energy_get_surprise_level(const snn_free_energy_bridge_t* bridge) {
    return bridge ? bridge->state.surprise_level : 0.0f;
}

bool snn_free_energy_is_surprised(const snn_free_energy_bridge_t* bridge) {
    return bridge ? bridge->state.is_surprised : false;
}

int snn_free_energy_get_stats(const snn_free_energy_bridge_t* bridge, uint32_t* error_count, uint32_t* surprise_events, float* avg_free_energy) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_free_energy_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (error_count) *error_count = bridge->state.prediction_error_count;
    if (surprise_events) *surprise_events = bridge->state.surprise_events;
    if (avg_free_energy) {
        if (bridge->state.prediction_error_count > 0) {
            *avg_free_energy = bridge->state.accumulated_free_energy / (float)bridge->state.prediction_error_count;
        } else {
            *avg_free_energy = 0.0f;
        }
    }
    return 0;
}

void snn_free_energy_reset_stats(snn_free_energy_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.prediction_error_count = 0;
    bridge->state.surprise_events = 0;
    bridge->state.accumulated_free_energy = 0.0f;
}
