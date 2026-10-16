/**
 * @file nimcp_snn_wellbeing_bridge.c
 * @brief SNN-Wellbeing integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_wellbeing_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_wellbeing_bridge)

#define BIO_MODULE_SNN_WELLBEING_BRIDGE 0x062E

void snn_wellbeing_config_default(snn_wellbeing_config_t* config) {
    if (!config) return;
    config->homeostasis_setpoint = 0.5f;
    config->allostatic_load_threshold = 0.7f;
    config->recovery_rate = 0.05f;
    config->wellbeing_population_id = 0;
    config->regulation_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_wellbeing_bridge_t* snn_wellbeing_bridge_create(const snn_wellbeing_config_t* config, snn_network_t* snn, wellbeing_system_t* wellbeing_system) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_create: snn is NULL");
        return NULL;
    }

    snn_wellbeing_bridge_t* bridge = nimcp_malloc(sizeof(snn_wellbeing_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_wellbeing_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_wellbeing_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_wellbeing") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->wellbeing_system = wellbeing_system;
    bridge->config = *config;

    if (config->wellbeing_population_id > 0) {
        bridge->wellbeing_pop = snn_network_get_population(snn, config->wellbeing_population_id);
    }
    if (config->regulation_population_id > 0) {
        bridge->regulation_pop = snn_network_get_population(snn, config->regulation_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-wellbeing bridge");
    return bridge;
}

void snn_wellbeing_bridge_destroy(snn_wellbeing_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        snn_wellbeing_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->wellbeing_buffer) nimcp_free(bridge->wellbeing_buffer);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-wellbeing bridge");
}

int snn_wellbeing_bridge_connect_bio_async(snn_wellbeing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_connect_bio_async: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_WELLBEING_BRIDGE,
        .module_name = "snn_wellbeing_bridge",
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

int snn_wellbeing_bridge_disconnect_bio_async(snn_wellbeing_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_wellbeing_bridge_is_bio_async_connected(const snn_wellbeing_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int snn_wellbeing_bridge_update(snn_wellbeing_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_update: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    bridge->last_update_time += dt;
    if (bridge->last_update_time < bridge->config.update_interval_ms) return 0;
    bridge->last_update_time = 0.0f;

    float population_rate = 0.0f;
    if (bridge->wellbeing_pop) {
        population_rate = snn_population_get_firing_rate(bridge->wellbeing_pop);
    }

    float regulation_rate = 0.0f;
    if (bridge->regulation_pop) {
        regulation_rate = snn_population_get_firing_rate(bridge->regulation_pop);
        bridge->state.is_regulating = (regulation_rate > 0.0f);
        if (bridge->state.is_regulating) {
            bridge->state.regulation_events++;
        }
    }

    bridge->state.allostatic_load = snn_wellbeing_compute_allostatic_load(bridge, regulation_rate, bridge->state.allostatic_load, dt);
    bridge->state.wellbeing_index = snn_wellbeing_compute_wellbeing_index(bridge, population_rate, bridge->state.allostatic_load);

    bridge->state.is_overloaded = (bridge->state.allostatic_load >= bridge->config.allostatic_load_threshold);
    if (bridge->state.is_overloaded) {
        bridge->state.overload_events++;
        bridge->state.accumulated_load += bridge->state.allostatic_load;
    }

    snn_wellbeing_bridge_apply_recovery(bridge, dt);

    return 0;
}

int snn_wellbeing_bridge_encode_wellbeing(snn_wellbeing_bridge_t* bridge, float wellbeing) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_encode_wellbeing: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->wellbeing_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_wellbeing_bridge_encode_wellbeing: wellbeing_pop is NULL");
        return SNN_ERROR_INVALID_STATE;
    }

    return 0;
}

int snn_wellbeing_bridge_trigger_regulation(snn_wellbeing_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_trigger_regulation: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!bridge->regulation_pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_wellbeing_bridge_trigger_regulation: regulation_pop is NULL");
        return SNN_ERROR_INVALID_STATE;
    }

    bridge->state.regulation_events++;
    return 0;
}

int snn_wellbeing_bridge_apply_recovery(snn_wellbeing_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_apply_recovery: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }

    float recovery = bridge->config.recovery_rate * dt / 1000.0f;
    bridge->state.allostatic_load -= recovery;
    if (bridge->state.allostatic_load < 0.0f) {
        bridge->state.allostatic_load = 0.0f;
    }

    return 0;
}

float snn_wellbeing_compute_wellbeing_index(const snn_wellbeing_bridge_t* bridge, float population_rate, float allostatic_load) {
    if (!bridge) return 0.0f;

    float normalized_rate = population_rate / 100.0f;
    float deviation = fabsf(normalized_rate - bridge->config.homeostasis_setpoint);
    float wellbeing = 1.0f - deviation - allostatic_load;

    if (wellbeing < 0.0f) wellbeing = 0.0f;
    if (wellbeing > 1.0f) wellbeing = 1.0f;

    return wellbeing;
}

float snn_wellbeing_compute_allostatic_load(const snn_wellbeing_bridge_t* bridge, float regulation_rate, float current_load, float dt) {
    if (!bridge) return 0.0f;

    float load_increment = regulation_rate * 0.01f * dt / 1000.0f;
    float new_load = current_load + load_increment;

    if (new_load > 1.0f) new_load = 1.0f;

    return new_load;
}

int snn_wellbeing_bridge_get_state(const snn_wellbeing_bridge_t* bridge, snn_wellbeing_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_get_state: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_bridge_get_state: state is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    *state = bridge->state;
    return 0;
}

float snn_wellbeing_get_index(const snn_wellbeing_bridge_t* bridge) {
    return bridge ? bridge->state.wellbeing_index : 0.0f;
}

float snn_wellbeing_get_allostatic_load(const snn_wellbeing_bridge_t* bridge) {
    return bridge ? bridge->state.allostatic_load : 0.0f;
}

uint32_t snn_wellbeing_get_regulation_events(const snn_wellbeing_bridge_t* bridge) {
    return bridge ? bridge->state.regulation_events : 0;
}

bool snn_wellbeing_is_regulating(const snn_wellbeing_bridge_t* bridge) {
    return bridge ? bridge->state.is_regulating : false;
}

bool snn_wellbeing_is_overloaded(const snn_wellbeing_bridge_t* bridge) {
    return bridge ? bridge->state.is_overloaded : false;
}

int snn_wellbeing_get_stats(const snn_wellbeing_bridge_t* bridge, uint32_t* regulation_events, uint32_t* overload_events, float* avg_load) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_wellbeing_get_stats: bridge is NULL");
        return SNN_ERROR_NULL_POINTER;
    }
    if (regulation_events) *regulation_events = bridge->state.regulation_events;
    if (overload_events) *overload_events = bridge->state.overload_events;
    if (avg_load) {
        if (bridge->state.overload_events > 0) {
            *avg_load = bridge->state.accumulated_load / (float)bridge->state.overload_events;
        } else {
            *avg_load = 0.0f;
        }
    }
    return 0;
}

void snn_wellbeing_reset_stats(snn_wellbeing_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.regulation_events = 0;
    bridge->state.overload_events = 0;
    bridge->state.accumulated_load = 0.0f;
}
