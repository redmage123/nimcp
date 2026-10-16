/**
 * @file nimcp_snn_curiosity_bridge.c
 * @brief SNN-Curiosity integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_curiosity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_curiosity_bridge)

#define BIO_MODULE_SNN_CURIOSITY_BRIDGE 0x062D

void snn_curiosity_config_default(snn_curiosity_config_t* config) {
    if (!config) return;
    config->novelty_threshold = 0.5f;
    config->exploration_drive_gain = 2.0f;
    config->habituation_rate = 0.1f;
    config->novelty_population_id = 0;
    config->exploration_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_curiosity_bridge_t* snn_curiosity_bridge_create(const snn_curiosity_config_t* config, snn_network_t* snn, curiosity_system_t* curiosity_system) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_curiosity_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_curiosity_bridge_create: snn is NULL");
        return NULL;
    }

    snn_curiosity_bridge_t* bridge = nimcp_malloc(sizeof(snn_curiosity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_curiosity_bridge_t),
                          "snn_curiosity_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_curiosity_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_curiosity") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->curiosity_system = curiosity_system;
    bridge->config = *config;

    if (config->novelty_population_id > 0) {
        bridge->novelty_pop = snn_network_get_population(snn, config->novelty_population_id);
    }
    if (config->exploration_population_id > 0) {
        bridge->exploration_pop = snn_network_get_population(snn, config->exploration_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-curiosity bridge");
    return bridge;
}

void snn_curiosity_bridge_destroy(snn_curiosity_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        snn_curiosity_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->novelty_buffer) nimcp_free(bridge->novelty_buffer);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-curiosity bridge");
}

int snn_curiosity_bridge_connect_bio_async(snn_curiosity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_CURIOSITY_BRIDGE,
        .module_name = "snn_curiosity_bridge",
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

int snn_curiosity_bridge_disconnect_bio_async(snn_curiosity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_curiosity_bridge_is_bio_async_connected(const snn_curiosity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_curiosity_bridge_update(snn_curiosity_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) return 0;
    bridge->last_update_time = 0.0f;

    snn_curiosity_bridge_apply_habituation(bridge, dt);

    float novelty_rate = 0.0f;
    if (bridge->novelty_pop) {
        novelty_rate = snn_population_get_firing_rate(bridge->novelty_pop);
        bridge->state.novelty_response = novelty_rate;
        bridge->state.is_novel = (novelty_rate >= bridge->config.novelty_threshold);
        if (bridge->state.is_novel) {
            bridge->state.novelty_events++;
            bridge->state.accumulated_novelty += novelty_rate;
        }
    }

    bridge->state.curiosity_level = snn_curiosity_compute_curiosity_level(bridge, novelty_rate, bridge->habituation_accumulator);

    if (bridge->exploration_pop) {
        float exploration_rate = snn_population_get_firing_rate(bridge->exploration_pop);
        bridge->state.is_exploring = (exploration_rate > 0.0f && bridge->state.curiosity_level > 0.0f);
        if (bridge->state.is_exploring) {
            bridge->state.exploration_count++;
        }
    }

    return 0;
}

int snn_curiosity_bridge_encode_novelty(snn_curiosity_bridge_t* bridge, float novelty) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_encode_novelty: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->novelty_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "snn_curiosity_bridge_encode_novelty: novelty population not configured");
        return SNN_ERROR_INVALID_STATE;
    }

    return 0;
}

int snn_curiosity_bridge_trigger_exploration(snn_curiosity_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_trigger_exploration: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->exploration_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
                             "snn_curiosity_bridge_trigger_exploration: exploration population not configured");
        return SNN_ERROR_INVALID_STATE;
    }

    bridge->state.exploration_count++;
    return 0;
}

int snn_curiosity_bridge_apply_habituation(snn_curiosity_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_apply_habituation: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float decay_factor = expf(-bridge->config.habituation_rate * dt / 1000.0f);
    bridge->habituation_accumulator *= decay_factor;

    if (bridge->state.novelty_response > 0.0f) {
        bridge->habituation_accumulator += bridge->state.novelty_response * (1.0f - decay_factor);
    }

    return 0;
}

float snn_curiosity_compute_curiosity_level(const snn_curiosity_bridge_t* bridge, float novelty_rate, float habituation) {
    if (!bridge) return 0.0f;

    float habituated_novelty = novelty_rate * (1.0f - habituation);
    float curiosity = habituated_novelty * bridge->config.exploration_drive_gain;

    if (curiosity < 0.0f) curiosity = 0.0f;
    if (curiosity > 1.0f) curiosity = 1.0f;

    return curiosity;
}

int snn_curiosity_bridge_get_state(const snn_curiosity_bridge_t* bridge, snn_curiosity_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_curiosity_get_level(const snn_curiosity_bridge_t* bridge) {
    return bridge ? bridge->state.curiosity_level : 0.0f;
}

float snn_curiosity_get_novelty_response(const snn_curiosity_bridge_t* bridge) {
    return bridge ? bridge->state.novelty_response : 0.0f;
}

uint32_t snn_curiosity_get_exploration_count(const snn_curiosity_bridge_t* bridge) {
    return bridge ? bridge->state.exploration_count : 0;
}

bool snn_curiosity_is_exploring(const snn_curiosity_bridge_t* bridge) {
    return bridge ? bridge->state.is_exploring : false;
}

bool snn_curiosity_is_novel(const snn_curiosity_bridge_t* bridge) {
    return bridge ? bridge->state.is_novel : false;
}

int snn_curiosity_get_stats(const snn_curiosity_bridge_t* bridge, uint32_t* novelty_events, uint32_t* exploration_count, float* avg_novelty) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_curiosity_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (novelty_events) *novelty_events = bridge->state.novelty_events;
    if (exploration_count) *exploration_count = bridge->state.exploration_count;
    if (avg_novelty) {
        if (bridge->state.novelty_events > 0) {
            *avg_novelty = bridge->state.accumulated_novelty / (float)bridge->state.novelty_events;
        } else {
            *avg_novelty = 0.0f;
        }
    }
    return 0;
}

void snn_curiosity_reset_stats(snn_curiosity_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.novelty_events = 0;
    bridge->state.exploration_count = 0;
    bridge->state.accumulated_novelty = 0.0f;
    bridge->habituation_accumulator = 0.0f;
}
