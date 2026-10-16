/**
 * @file nimcp_snn_global_workspace_bridge.c
 * @brief SNN-Global Workspace integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_global_workspace_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_global_workspace_bridge)

#define BIO_MODULE_SNN_GLOBAL_WORKSPACE_BRIDGE 0x062B

void snn_global_workspace_config_default(snn_global_workspace_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_config_default: null config pointer");
        return;
    }
    config->competition_rate_threshold = 30.0f;
    config->broadcast_encoding_gain = 2.0f;
    config->ignition_rate_threshold = 50.0f;
    config->workspace_population_id = 0;
    config->competition_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
    config->module_id = 0;
}

snn_global_workspace_bridge_t* snn_global_workspace_bridge_create(const snn_global_workspace_config_t* config, snn_network_t* snn, global_workspace_t* workspace) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_bridge_create: snn is NULL");
        return NULL;
    }

    snn_global_workspace_bridge_t* bridge = nimcp_malloc(sizeof(snn_global_workspace_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_global_workspace_bridge_t),
                          "snn_global_workspace_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_global_workspace_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_global_workspace") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->workspace = workspace;
    bridge->config = *config;

    if (config->workspace_population_id > 0) {
        bridge->workspace_pop = snn_network_get_population(snn, config->workspace_population_id);
    }
    if (config->competition_population_id > 0) {
        bridge->competition_pop = snn_network_get_population(snn, config->competition_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-global-workspace bridge");
    return bridge;
}

void snn_global_workspace_bridge_destroy(snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_bridge_destroy: null bridge pointer");
        return;
    }
    if (bridge->base.bio_async_enabled) {
        snn_global_workspace_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->broadcast_buffer) nimcp_free(bridge->broadcast_buffer);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-global-workspace bridge");
}

int snn_global_workspace_bridge_connect_bio_async(snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_GLOBAL_WORKSPACE_BRIDGE,
        .module_name = "snn_global_workspace_bridge",
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

int snn_global_workspace_bridge_disconnect_bio_async(snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_disconnect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_global_workspace_bridge_is_bio_async_connected(const snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_global_workspace_bridge_update(snn_global_workspace_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) return 0;
    bridge->last_update_time = 0.0f;

    if (bridge->competition_pop) {
        float spike_rate = snn_population_get_firing_rate(bridge->competition_pop);
        bridge->state.competition_strength = snn_global_workspace_compute_competition_strength(bridge, spike_rate);
        bridge->state.is_competing = (spike_rate >= bridge->config.competition_rate_threshold);
    }

    if (bridge->workspace_pop) {
        float rate = snn_population_get_firing_rate(bridge->workspace_pop);
        bridge->state.current_broadcast_rate = rate;
        bridge->state.is_broadcasting = (rate >= bridge->config.ignition_rate_threshold);
        if (bridge->state.is_broadcasting) bridge->state.broadcast_count++;
    }

    return 0;
}

int snn_global_workspace_bridge_process_broadcast(snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_process_broadcast: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

int snn_global_workspace_bridge_submit_competition(snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_submit_competition: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    return 0;
}

float snn_global_workspace_compute_competition_strength(const snn_global_workspace_bridge_t* bridge, float spike_rate) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_compute_competition_strength: null bridge pointer");
        return 0.0f;
    }
    float threshold = bridge->config.competition_rate_threshold;
    float strength = (spike_rate - threshold) / threshold;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    return strength;
}

int snn_global_workspace_bridge_get_state(const snn_global_workspace_bridge_t* bridge, snn_global_workspace_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_global_workspace_get_broadcast_rate(const snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_get_broadcast_rate: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.current_broadcast_rate;
}

float snn_global_workspace_get_competition_strength(const snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_get_competition_strength: null bridge pointer");
        return 0.0f;
    }
    return bridge->state.competition_strength;
}

bool snn_global_workspace_is_broadcasting(const snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.is_broadcasting;
}

int snn_global_workspace_get_stats(const snn_global_workspace_bridge_t* bridge, uint32_t* broadcast_count, uint32_t* wins, float* avg_strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_global_workspace_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (broadcast_count) *broadcast_count = bridge->state.broadcast_count;
    if (wins) *wins = bridge->state.competition_wins;
    if (avg_strength) *avg_strength = bridge->state.competition_strength;
    return 0;
}

void snn_global_workspace_reset_stats(snn_global_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_global_workspace_reset_stats: null bridge pointer");
        return;
    }
    bridge->state.broadcast_count = 0;
    bridge->state.competition_wins = 0;
}
