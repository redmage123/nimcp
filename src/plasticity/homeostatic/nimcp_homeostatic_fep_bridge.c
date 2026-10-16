/**
 * @file nimcp_homeostatic_fep_bridge.c
 * @brief FEP-Homeostatic Integration Bridge Implementation
 */

#include "plasticity/homeostatic/nimcp_homeostatic_fep_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "security/nimcp_bbb_helpers.h"

#define LOG_MODULE_HOMEOSTATIC_FEP "HOMEOSTATIC_FEP_BRIDGE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(homeostatic_fep_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(homeostatic_fep_bridge)

int homeostatic_fep_bridge_default_config(homeostatic_fep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "Homeostatic-FEP config is NULL");
    config->precision_target_gain = HOMEOSTATIC_FEP_TARGET_RATE_SCALING;
    config->free_energy_scaling_factor = 1.0f;
    config->stability_threshold = 0.01f;
    config->enable_precision_normalization = true;
    config->enable_fe_modulation = true;
    config->enable_stability_tracking = true;
    return 0;
}

homeostatic_fep_bridge_t* homeostatic_fep_bridge_create(const homeostatic_fep_config_t* config) {
    homeostatic_fep_bridge_t* bridge = (homeostatic_fep_bridge_t*)nimcp_malloc(sizeof(homeostatic_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_fep_bridge_create: bridge allocation failed");
        return NULL;
    }
    memset(bridge, 0, sizeof(homeostatic_fep_bridge_t));
    if (config) bridge->config = *config;
    else homeostatic_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "homeostatic_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_fep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("Homeostatic-FEP bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_fep_bridge_create: mutex creation failed");
        return NULL;
    }
    bridge->effects.total_normalization_factor = 1.0f;
    bridge->effects.precision_target_rate = 1.0f;  /* Initialize to 1.0 for proper get_effective_target_rate */
    NIMCP_LOGGING_INFO("Homeostatic-FEP bridge created");
    return bridge;
}

void homeostatic_fep_bridge_destroy(homeostatic_fep_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) homeostatic_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int homeostatic_fep_bridge_connect_fep(homeostatic_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_connect_fep: bridge is NULL");
        return -1;
    }
    /* Allow NULL fep to disconnect/reset FEP connection */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int homeostatic_fep_bridge_connect_homeostatic(homeostatic_fep_bridge_t* bridge, homeostatic_controller_t homeostatic) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_connect_homeostatic: bridge is NULL");
        return -1;
    }
    /* Allow NULL homeostatic to disconnect/reset connection */
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->homeostatic_system = homeostatic;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int homeostatic_fep_bridge_disconnect(homeostatic_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->homeostatic_system = NULL;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float homeostatic_fep_apply_precision_normalization(homeostatic_fep_bridge_t* bridge, float precision) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_apply_precision_normalization: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->config.enable_precision_normalization) return 1.0f;
    float normalized = fminf(fmaxf(precision, HOMEOSTATIC_FEP_PRECISION_MIN), HOMEOSTATIC_FEP_PRECISION_MAX);
    /* Store for get_effective_target_rate to use */
    bridge->effects.precision_value = precision;
    bridge->effects.precision_target_rate = normalized;
    return normalized;
}

float homeostatic_fep_apply_fe_modulation(homeostatic_fep_bridge_t* bridge, float free_energy) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_apply_fe_modulation: bridge is NULL");
        return 1.0f;
    }
    if (!bridge->config.enable_fe_modulation) return 1.0f;
    return 1.0f + free_energy * bridge->config.free_energy_scaling_factor;
}

float homeostatic_fep_get_effective_target_rate(const homeostatic_fep_bridge_t* bridge, float base_rate) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_get_effective_target_rate: bridge is NULL");
        return base_rate;
    }
    return base_rate * bridge->effects.precision_target_rate;
}

int homeostatic_fep_report_scaling(homeostatic_fep_bridge_t* bridge, float scaling_factor) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_report_scaling: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.avg_scaling_factor = (bridge->stats.avg_scaling_factor * bridge->stats.total_updates + scaling_factor) /
                                        (bridge->stats.total_updates + 1);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int homeostatic_fep_bridge_update(homeostatic_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_update: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->fep_system) {
        float precision_norm = homeostatic_fep_apply_precision_normalization(bridge, bridge->effects.precision_value);
        float fe_mod = homeostatic_fep_apply_fe_modulation(bridge, bridge->effects.free_energy_value);
        bridge->effects.precision_target_rate = precision_norm;
        bridge->effects.scaling_time_modulation = fe_mod;
        bridge->effects.total_normalization_factor = precision_norm * fe_mod;
    }
    bridge->stats.total_updates++;
    bridge->state.last_update_time = delta_ms;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int homeostatic_fep_bridge_get_state(const homeostatic_fep_bridge_t* bridge, homeostatic_fep_state_t* state) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_get_state: state is NULL");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

int homeostatic_fep_bridge_get_stats(const homeostatic_fep_bridge_t* bridge, homeostatic_fep_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int homeostatic_fep_bridge_connect_bio_async(homeostatic_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_HOMEOSTATIC_BRIDGE,
        .module_name = "homeostatic_fep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }
    return 0;
}

int homeostatic_fep_bridge_disconnect_bio_async(homeostatic_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_fep_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;  /* Already disconnected - success */
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool homeostatic_fep_bridge_is_bio_async_connected(const homeostatic_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
